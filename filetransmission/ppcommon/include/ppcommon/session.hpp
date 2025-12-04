#pragma once

#include <filesystem>
#include <fstream>

#include "net_common/net_connection.hpp"
#include "net_common/net_message.hpp"
#include "ppcommon.hpp"
#include "tsqueue/tsqueue.hpp"

namespace PingPong
{

class Session
{
  public:
    using Message = Net::Message<Common::EMessageType>;

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
    ClientReceiverSession(Common::EPayloadType payload_type, Net::TSQueue<Net::OwnedMessage<Common::EMessageType>> &messages_in, const std::filesystem::path &file, const std::function<void(Message &&)> sendcb)
        : ClientSession(payload_type, messages_in), m_file{file}, m_sendcb{sendcb}
    {
    }

    ~ClientReceiverSession() override
    {
    }

    bool mainLoop() override
    {
        std::cout << __PRETTY_FUNCTION__ << '\n';
        std::ofstream ofs(m_file, std::ios::out | std::ios::binary);

        if (!ofs.is_open())
        {
            std::cerr << "Error opening file: " << m_file << std::endl;
            Message outmsg;
            outmsg.header.id = Common::EMessageType::Abort;
            m_sendcb(std::move(outmsg));

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
                if (msg.header.id == Common::EMessageType::Abort)
                {
                    std::cout << "Abort command from the server\n";
                    op_result = false;
                    break;
                }
                else if (msg.header.id == Common::EMessageType::Chunk)
                {
                    std::cout << "Incoming chunk of size " << msg.size() << '\n';
                    ofs.write(reinterpret_cast<const char *>(msg.body.data()), msg.size());
                }
                else if (msg.header.id == Common::EMessageType::FinalChunk)
                {
                    std::cout << "End of transmission. Finishing." << '\n';
                    finish = true;
                    break;
                }
                else
                {
                    std::cout << "Skipped an unknown message from the server with header " << static_cast<uint32_t>(msg.header.id) << '\n';
                }
            }
        }

        ofs.close();

        return op_result;
    }

  private:
    const std::filesystem::path m_file;
    std::function<void(Message &&)> m_sendcb;
};

class ClientSenderSession : public ClientSession
{
  public:
    ClientSenderSession(Common::EPayloadType payload_type, Net::TSQueue<Net::OwnedMessage<Common::EMessageType>> &messages_in, const std::filesystem::path &file, const uint64_t chunksize, const std::function<void(Message &&)> sendcb)
        : ClientSession(payload_type, messages_in), m_file{file}, m_max_chunk_size{chunksize}, m_sendcb{sendcb}
    {
    }

    ~ClientSenderSession() override
    {
    }

    bool mainLoop() override
    {
        std::cout << __PRETTY_FUNCTION__ << '\n';
        if (!std::filesystem::exists(m_file))
        {
            std::cout << "File " << m_file << " doesn't exist\n";
            return false;
        }

        std::ifstream ifs(m_file, std::ios::binary);

        Message msg;
        msg.body = std::vector<std::uint8_t>(m_max_chunk_size);

        bool op_result = true;

        while (ifs && op_result)
        {
            // Check for incoming messages from a server
            while (!m_messages_in.empty())
            {
                auto msg = m_messages_in.pop_front().msg;
                if (msg.header.id == Common::EMessageType::Abort)
                {
                    std::cout << "Abort command from the server\n";
                    op_result = false;
                    break;
                }
                else
                {
                    std::cout << "Skipped an unknown message from the server with header " << static_cast<uint32_t>(msg.header.id) << '\n';
                }
            }

            if (!op_result)
                break;

            ifs.read(reinterpret_cast<char *>(msg.body.data()), msg.body.size());
            const std::streamsize n = ifs.gcount();

            if (n > 0)
            {
                msg.header.id = Common::EMessageType::Chunk;
                msg.body.resize(static_cast<size_t>(n));
                msg.header.size = msg.body.size();

                std::cout << "Sending Chunk of size " << msg.size() << '\n';
            }
            else
            {
                break;
            }

            m_sendcb(std::move(msg));
        }

        std::cout << "Sending FinalChunk\n";
        msg.header.id = Common::EMessageType::FinalChunk;
        msg.header.size = 0;
        msg.body.clear();
        m_sendcb(std::move(msg));

        ifs.close();

        return op_result;
    }

  private:
    const std::filesystem::path m_file;
    const uint64_t m_max_chunk_size;
    std::function<void(Message &&)> m_sendcb;
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
    virtual bool onMessage(Message &&msg) = 0;
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

    bool onMessage(Message &&msg) override
    {
        std::cout << __PRETTY_FUNCTION__ << " msg type: " << (int)msg.header.id << '\n';

        if (msg.header.id == Common::EMessageType::Chunk || msg.header.id == Common::EMessageType::FinalChunk)
        {
            m_sink->send(std::move(msg));
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

    bool onMessage(Message &&msg) override
    {
        if (msg.header.id == Common::EMessageType::Chunk)
        {
            if (!m_ofs.is_open())
            {
                return false;
            }

            m_ofs.write(reinterpret_cast<const char *>(msg.body.data()), msg.body.size());

            if (m_ofs.fail())
            {
                return false;
            }
        }
        else
        {
            return false;
        }

        return true;
    }

  private:
    std::ofstream m_ofs;
};

} // namespace PingPong
