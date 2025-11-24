#include <iostream>
#include <map>

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
    }

    void onMessage(ConnectionPtr client, Message &msg) override
    {
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
                if (!it->second->onMessage(msg))
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

        if (msg.header.id == Common::EMessageType::Send)
        {
            std::cout << "[" << client->getId() << "]: wants to send some file sent file of size = " << msg.header.size << '\n';
            auto it = m_sessions.find(client);
            if (!(it == m_sessions.end() || it->second == nullptr))
            {
                Message outmsg;
                outmsg.header.id = Common::EMessageType::Reject;
                client->send(outmsg);
                return;
            }

            std::filesystem::path clients_file{std::string("./test") + std::to_string(client->getId())};
            m_sessions[client] = std::make_unique<ServerReceiverSession>(Session::EPayloadType::File, clients_file);
            Message outmsg;
            outmsg.header.id = Common::EMessageType::Accept;
            client->send(outmsg);
        }
        else
        {
            std::cout << "[" << client->getId() << "]: unexpected message from a client. Type = " << static_cast<int>(msg.header.id) << '\n';
        }
    }

  protected:
    std::map<ConnectionPtr, SessionUPtr> m_sessions;
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
