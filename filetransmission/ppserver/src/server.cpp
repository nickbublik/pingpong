#include <iostream>
#include <net_common/net_server.hpp>
#include <ppcommon/ppcommon.hpp>

namespace PingPong
{

class FileServer : public Net::ServerBase<Common::EMessageType>
{
  public:
    FileServer(uint16_t port)
        : Net::ServerBase<Common::EMessageType>(port)
    {
    }

    ~FileServer() override = default;

  protected:
    bool onClientConnect(std::shared_ptr<Net::Connection<Common::EMessageType>> client) override
    {
        return true;
    }

    void onClientDisconnect(std::shared_ptr<Net::Connection<Common::EMessageType>> client) override
    {
    }

    void onMessage(std::shared_ptr<Net::Connection<Common::EMessageType>> client, Net::Message<Common::EMessageType> &msg) override
    {
        if (msg.header.id == Common::EMessageType::Send)
        {
            std::cout << "[" << client->getId() << "]: Requested echo \n";
            client->send(msg);
        }
    }
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
