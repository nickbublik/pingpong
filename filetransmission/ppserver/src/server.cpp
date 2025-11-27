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

        if (msg.header.id == Common::EMessageType::Send)
        {
            m_pending_senders.right.erase(client);

            std::cout << msg << '\n';

            Common::SendRequest request;
            {
                request.code = "abc";
                request.code_size = request.code.size();
                request.payload_type = Common::EPayloadType::File;
            }

            msg >> request;
            std::cout << "request: code_size = " << (int)request.code_size << ", code = " << request.code << '\n';

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
            // std::make_unique<ServerOneToOneRetranslatorSession>(Common::EPayloadType::File, clients_file);
            // Message outmsg;
            // outmsg.header.id = Common::EMessageType::Accept;
            // outmsg << m_max_chunk_size;
            // client->send(outmsg);
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
