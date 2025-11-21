#include <iostream>
#include <net_common/net_client.hpp>

enum class MsgTypes : uint32_t
{
    Receive,
    Send,
    Echo
};

class FileClient : public Net::ClientBase<MsgTypes>
{
  public:
    FileClient()
    {
    }

    ~FileClient() override = default;

    template <typename T>
    void sendToEcho(const T &data)
    {
        Net::Message<MsgTypes> msg;
        msg.header.id = MsgTypes::Echo;
        msg << data;
        send(msg);
    }
};

int main()
{
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

            if (msg.header.id == MsgTypes::Echo)
            {
                int echoed_data;
                msg >> echoed_data;
                std::cout << "Got echo: " << echoed_data << '\n';
            }
        }
    }

    return 0;
}
