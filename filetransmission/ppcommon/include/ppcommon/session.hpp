#pragma once

#include <filesystem>
#include <fstream>

#include "net_common/net_message.hpp"
#include "ppcommon.hpp"
#include "tsqueue/tsqueue.hpp"

namespace PingPong
{

class Session
{
  public:
    enum class EPayloadType
    {
        File
    };
    using Message = Net::Message<Common::EMessageType>;

  public:
    Session(EPayloadType payload_type)
        : m_payload_type{payload_type}
    {
    }

    virtual ~Session() = default;

  protected:
    EPayloadType m_payload_type;
};

class ClientSession : public Session
{
  public:
    using IncomingQueue = Net::TSQueue<Net::OwnedMessage<Common::EMessageType>>;

  public:
    ClientSession(EPayloadType payload_type, IncomingQueue &messages_in)
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
    bool mainLoop()
    {
        return true;
    }
};

class ClientSenderSession : public ClientSession
{
  public:
    ClientSenderSession(EPayloadType payload_type, Net::TSQueue<Net::OwnedMessage<Common::EMessageType>> &messages_in, const std::filesystem::path &file, const uint64_t chunksize, const std::function<void(const Message &)> sendcb)
        : ClientSession(payload_type, messages_in), m_file{file}, m_max_chunk_size{chunksize}, m_sendcb{sendcb}
    {
    }

    ~ClientSenderSession() override
    {
    }

    bool mainLoop() override
    {
        if (!std::filesystem::exists(m_file))
        {
            std::cout << "File " << m_file << " doesn't exist\n";
            return false;
        }

        std::ifstream ifs(m_file, std::ios::binary);

        Message msg;
        msg.body = std::vector<std::uint8_t>(m_max_chunk_size);

        bool op_result = true;

        while (ifs)
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
                    std::cout << "Skipped strange message from the server with header " << static_cast<uint32_t>(msg.header.id) << '\n';
                }
            }

            if (!op_result)
                break;

            ifs.read(reinterpret_cast<char *>(msg.body.data()), msg.body.size());
            const std::streamsize n = ifs.gcount();

            if (n <= 0)
            {
                msg.header.id = Common::EMessageType::FinalChunk;
                msg.header.size = 0;
                msg.body.clear();
                m_sendcb(msg);
                break;
            }
            else
            {
                msg.header.id = Common::EMessageType::Chunk;
                msg.body.resize(static_cast<size_t>(n));
                msg.header.size = msg.body.size();
            }

            m_sendcb(msg);
        }

        ifs.close();

        return op_result;
    }

  private:
    const std::filesystem::path m_file;
    const uint64_t m_max_chunk_size;
    std::function<void(const Message &)> m_sendcb;
};

class ServerSession : public Session
{
  public:
    ServerSession(EPayloadType payload_type)
        : Session(payload_type)
    {
    }

  public:
    virtual bool onMessage(const Message &msg) = 0;
};

class ServerReceiverSession : public ServerSession
{
  public:
    ServerReceiverSession(EPayloadType payload_type, const std::filesystem::path &file)
        : ServerSession(payload_type), m_ofs(file, std::ios::binary)
    {
        if (!m_ofs.is_open())
        {
            throw std::runtime_error("Could not open file");
        }
    }

    ~ServerReceiverSession()
    {
        m_ofs.close();
    }

    bool onMessage(const Message &msg) override
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

class ServerSenderSession : public Session
{
  public:
};

} // namespace PingPong
