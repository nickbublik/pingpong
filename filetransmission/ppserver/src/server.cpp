#include <iostream>
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
        m_sessions[client] = nullptr;
        return true;
    }

    void onClientDisconnect(ConnectionPtr client) override
    {
        m_sessions.erase(client);
        m_pending_senders.right.erase(client);
    }

    void onMessage(ConnectionPtr client, Message &&msg) override
    {
        std::cout << "TR#0\n";
        auto it = m_sessions.find(client);
        if (it != m_sessions.end() && it->second)
        {
            if (msg.header.id == Common::EMessageType::FinalChunk)
            {
                Message outmsg;
                outmsg.header.id = Common::EMessageType::Accept;
                client->send(outmsg);

                m_sessions.erase(client);
            }
            else
            {
                if (!it->second->onMessage(std::move(msg)))
                {
                    std::cout << "Error in ServerSession. Aborting\n";

                    Message outmsg;
                    outmsg.header.id = Common::EMessageType::Abort;
                    client->send(outmsg);

                    m_sessions.erase(client);
                }
            }

            return;
        }

        std::cout << "TR#1\n";

        if (msg.header.id == Common::EMessageType::Send)
        {
            std::cout << "TR#2\n";

            m_pending_senders.right.erase(client);

            std::cout << "TR#2.1\n";

            Common::SendRequest send_request;
            const size_t offset = sizeof(send_request.payload_type) + sizeof(send_request.size);
            send_request.code.resize(msg.body.size() - offset);
            std::cout << "TR#2.2\n";
            std::memcpy(send_request.code.data(), msg.body.data(), msg.body.size() - offset);
            msg >> send_request.payload_type >> send_request.size;

            std::cout << "TR#2.3\n";

            std::string code_phrase(send_request.code.begin(), send_request.code.end());
            std::cout << "[" << client->getId()
                << "]: wants to send some file with size = " << send_request.size
                << " ,code = " << code_phrase
                << '\n';

            std::cout << "TR#3\n";

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

            std::cout << "TR#4\n";
            
            m_pending_senders.insert({code_phrase, client});
            //std::make_unique<ServerOneToOneRetranslatorSession>(Common::EPayloadType::File, clients_file);
            //Message outmsg;
            //outmsg.header.id = Common::EMessageType::Accept;
            //outmsg << m_max_chunk_size;
            //client->send(outmsg);
        }
        else
        {
            std::cout << "[" << client->getId() << "]: unexpected message from a client. Type = " << static_cast<int>(msg.header.id) << '\n';
        }

        std::cout << "TR#5\n";
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
