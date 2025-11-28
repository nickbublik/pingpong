#include <bitset>
#include <iostream>
#include <limits>
#include <map>

#include <boost/bimap.hpp>

#include "net_common/net_server.hpp"
#include "ppcommon/ppcommon.hpp"
#include "ppcommon/session.hpp"

namespace PingPong
{

using Message = Net::Message<Common::EMessageType>;
using ConnectionPtr = std::shared_ptr<Net::Connection<Common::EMessageType>>;
using SessionUPtr = std::unique_ptr<ServerSession>;

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
    }

    void onSendEstablishment(ConnectionPtr client, Message &&msg)
    {
        std::cout << __PRETTY_FUNCTION__ << '\n';
        m_pending_senders.right.erase(client);

        Common::PreMetadata request;
        msg >> request;
        std::cout << "send-request: code_size = " << (int)request.code_size << ", code = " << request.code << '\n';

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

        std::cout << "[" << client->getId() << "]: new pending sender with code = " << request.code << '\n';
        m_pending_senders.insert({request.code, client});
    }

    void onReceiveEstablishment(ConnectionPtr client, Message &&msg)
    {
        std::cout << __PRETTY_FUNCTION__ << '\n';

        Common::PreMetadata request;
        msg >> request;
        std::cout << "receive-request: code_size = " << (int)request.code_size << ", code = " << request.code << '\n';

        auto it = m_pending_senders.find(request.code);

        if (it == m_pending_senders.end())
        {
            std::cout << "[" << client->getId() << "]: failed to receive file. Code phrase is invalid\n";
            Message outmsg;
            outmsg.header.id = Common::EMessageType::Reject;
            client->send(outmsg);
            return;
        }

        m_sessions[it->right] = std::make_unique<ServerOneToOneRetranslatorSession>(Common::EPayloadType::File, client);
        Message outmsg;
        outmsg.header.id = Common::EMessageType::Accept;

        Common::PostMetadata response;
        response.payload_type = Common::EPayloadType::File;
        response.max_chunk_size = m_max_chunk_size;
        outmsg << m_max_chunk_size;
        client->send(outmsg);
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
        else
        {
            std::cout << "[" << client->getId() << "]: unexpected message from a client. Type = " << static_cast<int>(msg.header.id) << '\n';
        }
    }

  protected:
    boost::bimap<std::string, ConnectionPtr> m_pending_senders;
    std::map<ConnectionPtr, SessionUPtr> m_sessions;
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
