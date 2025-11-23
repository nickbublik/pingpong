#include <iostream>
#include <net_common/net_client.hpp>
#include <ppcommon/ppcommon.hpp>

namespace PingPong
{

class FileClient : public Net::ClientBase<Common::EMessageType>
{
  public:
    FileClient()
    {
    }

    ~FileClient() override = default;

    template <typename T>
    void sendToEcho(const T &data)
    {
        Net::Message<Common::EMessageType> msg;
        msg.header.id = Common::EMessageType::Send;
        msg << data;
        send(msg);
    }
};

} // namespace PingPong

int main()
{
    using namespace PingPong;

    FileClient c;
    c.connect("127.0.0.1", 60000);

    bool running = true;

    while (running)
    {
        int data;
        if (!(std::cin >> data))
        {
            break;
        }

        if (!c.isConnected())
        {
            std::cout << "Server went down\n";
            break;
        }

        c.sendToEcho(data);

        c.incoming().wait();

        if (!c.incoming().empty())
        {
            std::cout << "try to pop_front\n";
            auto msg = c.incoming().pop_front().msg;

            if (msg.header.id == Common::EMessageType::Send)
            {
                int echoed_data;
                msg >> echoed_data;
                std::cout << "Got echo: " << echoed_data << '\n';
            }
        }
    }

    return 0;
}
