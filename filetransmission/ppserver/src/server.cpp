#include <bitset>
#include <iostream>
#include <limits>
#include <unordered_map>

#include <boost/bimap.hpp>

#include "net_common/net_server.hpp"
#include "ppcommon/ppcommon.hpp"
#include "ppcommon/session.hpp"

namespace PingPong
{

using Message = Net::Message<Common::EMessageType>;
using ConnectionPtr = std::shared_ptr<Net::Connection<Common::EMessageType>>;
using SessionUPtr = std::unique_ptr<ServerSession>;

struct TransmissionContext
{
    Common::PreMetadata pre_metadata;
    Common::PostMetadata post_metadata;
};

class FileServer : public Net::ServerBase<Common::EMessageType>
{
  public:
    FileServer(uint16_t port)
        : Net::ServerBase<Common::EMessageType>(port)
    {
    }

    ~FileServer() override = default;

  protected:
    bool onClientConnect(ConnectionPtr client) override
    {
        std::cout << "[" << client->getId() << "] " << __PRETTY_FUNCTION__ << '\n';
        m_sessions[client] = nullptr;
        return true;
    }

    void onClientDisconnect(ConnectionPtr client) override
    {
        std::cout << "[" << client->getId() << "] " << __PRETTY_FUNCTION__ << '\n';
        m_sessions.erase(client);
        m_pending_senders.right.erase(client);
        m_pending_transmissions.erase(client);
    }

    void onSendEstablishment(ConnectionPtr client, Message &&msg)
    {
        std::cout << __PRETTY_FUNCTION__ << '\n';
        m_pending_senders.right.erase(client);
        m_pending_transmissions.erase(client);

        Common::PreMetadata request;
        msg >> request;
        std::cout << "send-request: file_name = " << request.file_data.file_name
                  << " file_size = " << request.file_data.file_size
                  << ", code = " << request.code_phrase.code << '\n';

        auto it = m_sessions.find(client);
        if (!(it == m_sessions.end() || it->second == nullptr))
        {
            Message outmsg;
            outmsg.header.id = Common::EMessageType::Reject;
            client->send(outmsg);
            m_pending_senders.right.erase(client);
            m_sessions.erase(client);
            return;
        }

        std::cout << "[" << client->getId() << "]: new pending sender with code = " << request.code_phrase.code << '\n';
        m_pending_senders.insert({request.code_phrase.code, client});
        m_pending_transmissions.insert({client,
                                        TransmissionContext{request, Common::PostMetadata{request.payload_type, m_max_chunk_size, request.code_phrase, request.file_data}}});
    }

    void onReceiveEstablishment(ConnectionPtr client, Message &&msg)
    {
        std::cout << __PRETTY_FUNCTION__ << '\n';

        Common::PreMetadata request;
        msg >> request;
        std::cout << "receive-request: code = " << request.code_phrase.code << '\n';

        auto it = m_pending_senders.left.find(request.code_phrase.code);

        if (it == m_pending_senders.left.end())
        {
            std::cout << "[" << client->getId() << "]: failed to receive file. Code phrase is invalid\n";
            Message outmsg;
            outmsg.header.id = Common::EMessageType::Reject;
            client->send(outmsg);
            return;
        }

        const Common::PostMetadata &response = m_pending_transmissions[it->second].post_metadata;

        Message outmsg;
        outmsg.header.id = Common::EMessageType::Accept;
        outmsg << response;

        client->send(outmsg);
    }

    void establishTransmissionSession(ConnectionPtr receiver, Message &&msg)
    {
        std::cout << __PRETTY_FUNCTION__ << '\n';

        Common::PostMetadata request;
        msg >> request;
        std::cout << "establishTransmissionSession: code = " << request.code_phrase.code << '\n';

        auto it = m_pending_senders.left.find(request.code_phrase.code);

        if (it == m_pending_senders.left.end())
        {
            std::cout << "[" << receiver->getId() << "]: failed to receive file. Something went wrong\n";
            Message outmsg;
            outmsg.header.id = Common::EMessageType::Abort;
            receiver->send(outmsg);
            return;
        }

        auto sender = it->second;

        std::cout << "Server starts to send files from " << sender->getId() << " to " << receiver->getId() << "TODO...\n";
        // m_pending_senders.right.erase(client);
        // m_pending_transmissions.erase(client);
    }

    void removeSession(ConnectionPtr client)
    {
        std::cout << "[" << client->getId() << "]: error in send-session. Aborting\n";

        Message outmsg;
        outmsg.header.id = Common::EMessageType::Abort;
        client->send(outmsg);
        m_sessions[client]->onMessage(std::move(outmsg));

        m_sessions.erase(client);
    }

    void onMessage(ConnectionPtr client, Message &&msg) override
    {
        std::cout << "[" << client->getId() << "] " << __PRETTY_FUNCTION__ << '\n';
        std::cout << msg << '\n';

        auto it = m_sessions.find(client);

        // If a client has already established a send-session
        if (it != m_sessions.end() && it->second)
        {
            ServerSession &session = *it->second;

            // FinalChunk: Accept -> Sender , FinalChunk -> Receiver
            if (msg.header.id == Common::EMessageType::FinalChunk)
            {
                Message outmsg;
                outmsg.header.id = Common::EMessageType::Accept;
                client->send(outmsg);
                session.onMessage(std::move(msg));

                m_sessions.erase(client);
            }
            // Chunk:
            // good : Chunk -> Receiver
            // bad  : Abort -> Sender, Abort -> Receiver
            else if (msg.header.id == Common::EMessageType::Chunk)
            {
                if (msg.size() > m_max_chunk_size)
                {
                    std::cout << "[" << client->getId() << "]: exceeded max chunk size\n";
                    removeSession(client);
                }
                else if (!session.onMessage(std::move(msg)))
                {
                    std::cout << "[" << client->getId() << "]: message handling went wrong\n";
                    removeSession(client);
                }
            }
            // Other: Abort -> Sender, Abort -> Receiver
            else
            {
                removeSession(client);
            }

            return;
        }

        if (msg.header.id == Common::EMessageType::Send)
        {
            onSendEstablishment(client, std::move(msg));
        }
        else if (msg.header.id == Common::EMessageType::RequestReceive)
        {
            onReceiveEstablishment(client, std::move(msg));
        }
        else if (msg.header.id == Common::EMessageType::Receive)
        {
            establishTransmissionSession(client, std::move(msg));
        }
        else
        {
            std::cout << "[" << client->getId() << "]: unexpected message from a client. Type = " << static_cast<int>(msg.header.id) << '\n';
        }
    }

  protected:
    boost::bimap<std::string, ConnectionPtr> m_pending_senders;
    std::unordered_map<ConnectionPtr, TransmissionContext> m_pending_transmissions;

    std::unordered_map<ConnectionPtr, SessionUPtr> m_sessions;

    uint64_t m_max_chunk_size = 1024;
};

} // namespace PingPong

int main()
{
    using namespace PingPong;

    FileServer server(60000);
    server.start();

    while (true)
    {
        server.update(true);
    }

    return 0;
}
