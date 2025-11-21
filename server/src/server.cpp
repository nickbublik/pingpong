#include <iostream>
#include <net_common/net_server.hpp>

enum class MsgTypes : uint32_t
{
    Receive,
    Send,
    Echo
};

class FileServer : public Net::ServerBase<MsgTypes>
{
  public:
    FileServer(uint16_t port)
        : Net::ServerBase<MsgTypes>(port)
    {
    }

    ~FileServer() override = default;

  protected:
    bool onClientConnect(std::shared_ptr<Net::Connection<MsgTypes>> client) override
    {
        return true;
    }

    void onClientDisconnect(std::shared_ptr<Net::Connection<MsgTypes>> client) override
    {
    }

    void onMessage(std::shared_ptr<Net::Connection<MsgTypes>> client, Net::Message<MsgTypes> &msg) override
    {
        if (msg.header.id == MsgTypes::Echo)
        {
            std::cout << "[" << client->getId() << "]: Requested echo \n";
            client->send(msg);
        }
    }
};

int main()
{
    FileServer server(60000);
    server.start();

    while (true)
    {
        server.update(true);
    }

    return 0;
}
