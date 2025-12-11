#pragma once

#include <filesystem>
#include <fstream>

#include "hash.hpp"
#include "logger/logger.hpp"
#include "net_common/net_connection.hpp"
#include "net_common/net_message.hpp"
#include "ppcommon.hpp"
#include "tsqueue/tsqueue.hpp"

namespace PingPong
{

class Session
{
  public:
    Session(Common::EPayloadType payload_type)
        : m_payload_type{payload_type}
    {
    }

    virtual ~Session() = default;

  protected:
    Common::EPayloadType m_payload_type;
};

class ClientSession : public Session
{
  public:
    using IncomingQueue = Net::TSQueue<Net::OwnedMessage<Common::EMessageType>>;

  public:
    ClientSession(Common::EPayloadType payload_type, IncomingQueue &messages_in)
        : Session(payload_type), m_messages_in{messages_in}
    {
    }

  public:
    virtual bool mainLoop() = 0;

  protected:
    IncomingQueue &m_messages_in;
};

class ClientReceiverSession : public ClientSession
{
  public:
    ClientReceiverSession(Common::EPayloadType payload_type, Net::TSQueue<Net::OwnedMessage<Common::EMessageType>> &messages_in, const std::filesystem::path &file, const std::function<void(Common::Message &&)> sendcb)
        : ClientSession(payload_type, messages_in), m_file{file}, m_sendcb{sendcb}
    {
    }

    ~ClientReceiverSession() override
    {
    }

    bool mainLoop() override
    {
        using namespace Common;

        DBG_LOG(__PRETTY_FUNCTION__);
        std::ofstream ofs(m_file, std::ios::out | std::ios::binary);

        if (!ofs.is_open())
        {
            std::cerr << "Error opening file: " << m_file << std::endl;
            Message failed_msg = encode<EMessageType::FailedReceive>(Empty{});
            m_sendcb(std::move(failed_msg));

            return false;
        }

        bool op_result = true;
        bool finish = false;

        while (ofs && !finish && op_result)
        {
            m_messages_in.wait();

            // Check for incoming messages from a server
            while (!m_messages_in.empty())
            {
                auto msg = m_messages_in.pop_front().msg;
                if (msg.header.id == EMessageType::Abort)
                {
                    std::cerr << "Abort command from the server\n";
                    op_result = false;
                    break;
                }
                else if (msg.header.id == EMessageType::Chunk)
                {
                    const auto offset = SHA256_DIGEST_LENGTH;
                    DBG_LOG("Incoming chunk of size ", msg.size() - offset);
                    {
                        Hash inc_hash;
                        msg >> inc_hash;

                        Hash hash = sha256_chunk(msg.body);

                        if (inc_hash != hash)
                        {
                            std::cerr << "Chunk control sums don't match. Aborting\n";
                            op_result = false;
                            break;
                        }
                    }
                    ofs.write(reinterpret_cast<const char *>(msg.body.data()), msg.size());
                }
                else if (msg.header.id == EMessageType::FinalChunk)
                {
                    DBG_LOG("End of transmission. Finishing.");
                    finish = true;
                    break;
                }
                else
                {
                    DBG_LOG("Skipped an unknown message from the server with header ", static_cast<uint32_t>(msg.header.id));
                }
            }
        }

        ofs.close();

        return op_result;
    }

  private:
    const std::filesystem::path m_file;
    std::function<void(Common::Message &&)> m_sendcb;
};

class ClientSenderSession : public ClientSession
{
  public:
    ClientSenderSession(Common::EPayloadType payload_type, Net::TSQueue<Net::OwnedMessage<Common::EMessageType>> &messages_in, const std::filesystem::path &file, const uint64_t chunksize, const std::function<bool(Common::Message &&)> sendcb)
        : ClientSession(payload_type, messages_in), m_file{file}, m_max_chunk_size{chunksize}, m_sendcb{sendcb}
    {
    }

    ~ClientSenderSession() override
    {
    }

    bool mainLoop() override
    {
        using namespace Common;

        DBG_LOG(__PRETTY_FUNCTION__);
        if (!std::filesystem::exists(m_file))
        {
            std::cerr << "File " << m_file << " doesn't exist\n";
            return false;
        }

        std::ifstream ifs(m_file, std::ios::binary);

        bool op_result = true;

        while (ifs && op_result)
        {
            // Check for incoming messages from a server
            while (!m_messages_in.empty())
            {
                auto incoming_msg = m_messages_in.pop_front().msg;
                if (incoming_msg.header.id == EMessageType::Abort)
                {
                    std::cerr << "Abort command from the server\n";
                    op_result = false;
                    break;
                }
                else
                {
                    DBG_LOG("Skipped an unknown message from the server with header ", static_cast<uint32_t>(incoming_msg.header.id));
                }
            }

            if (!op_result)
                break;

            const auto offset = SHA256_DIGEST_LENGTH;

            Message msg;
            msg.body = std::vector<std::uint8_t>(m_max_chunk_size);

            ifs.read(reinterpret_cast<char *>(msg.body.data()), msg.body.size());

            const std::streamsize n = ifs.gcount();

            if (n > 0)
            {
                msg.header.id = EMessageType::Chunk;
                msg.body.resize(static_cast<size_t>(n));
                msg.header.size = msg.body.size();

                Hash hash = sha256_chunk(msg.body);
                msg << hash;

                DBG_LOG("Sending Chunk of size ", msg.size() - offset);
            }
            else
            {
                break;
            }

            if (!m_sendcb(std::move(msg)))
            {
                DBG_LOG(__PRETTY_FUNCTION__, " failed to send message");
                op_result = false;
                break;
            }
        }

        DBG_LOG("Sending FinalChunk");
        Message final_msg = encode<EMessageType::FinalChunk>(Empty{});
        m_sendcb(std::move(final_msg));

        ifs.close();

        return op_result;
    }

  private:
    const std::filesystem::path m_file;
    const uint64_t m_max_chunk_size;
    std::function<bool(Common::Message &&)> m_sendcb;
};

class ServerSession : public Session
{
  public:
    using ConnectionPtr = std::shared_ptr<Net::Connection<Common::EMessageType>>;

  public:
    ServerSession(const Common::EPayloadType &payload_type)
        : Session(payload_type)
    {
    }

  public:
    virtual bool onMessage(Common::Message &&msg) = 0;
};

class ServerOneToOneRetranslatorSession : public ServerSession
{
  public:
    ServerOneToOneRetranslatorSession(const uint64_t file_size, const uint32_t max_chunk_size, ConnectionPtr sink)
        : ServerSession(Common::EPayloadType::File), file_size{file_size}, max_chunk_size{max_chunk_size}, m_sink(sink)
    {
    }

    ~ServerOneToOneRetranslatorSession()
    {
    }

    bool onMessage(Common::Message &&msg) override
    {
        using namespace Common;

        DBG_LOG(__PRETTY_FUNCTION__, " msg type: ", (int)msg.header.id);

        if (msg.header.id == EMessageType::Chunk || msg.header.id == EMessageType::FinalChunk || msg.header.id == EMessageType::Abort)
        {
            if (!m_sink->send(std::move(msg)))
            {
                return false;
            }
            return true;
        }

        return false;
    }

  private:
    uint64_t file_size;
    uint32_t max_chunk_size;
    ConnectionPtr m_sink;
};

class ServerSaveFileSession : public ServerSession
{
  public:
    ServerSaveFileSession(Common::EPayloadType payload_type, const std::filesystem::path &file)
        : ServerSession(payload_type), m_ofs(file, std::ios::binary)
    {
        if (!m_ofs.is_open())
        {
            throw std::runtime_error("Could not open file");
        }
    }

    ~ServerSaveFileSession()
    {
        m_ofs.close();
    }

    bool onMessage(Common::Message &&msg) override
    {
        // @TODO: Deprecated
        // using namespace Common;

        // if (msg.header.id == EMessageType::Chunk)
        //{
        //     if (!m_ofs.is_open())
        //     {
        //         return false;
        //     }

        //    m_ofs.write(reinterpret_cast<const char *>(msg.body.data()), msg.body.size());

        //    if (m_ofs.fail())
        //    {
        //        return false;
        //    }
        //}
        // else
        //{
        //    return false;
        //}

        return true;
    }

  private:
    std::ofstream m_ofs;
};

} // namespace PingPong
