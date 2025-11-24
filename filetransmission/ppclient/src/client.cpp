#include <iostream>
#include <net_common/net_client.hpp>

#include "ppcommon/ppcommon.hpp"
#include "ppcommon/session.hpp"

#include <filesystem>

namespace PingPong
{

using Message = Net::Message<Common::EMessageType>;

class FileClient : public Net::ClientBase<Common::EMessageType>
{
  public:
    FileClient()
    {
    }

    ~FileClient() override = default;
};

} // namespace PingPong

int main()
{
    using namespace PingPong;

    FileClient c;
    c.connect("127.0.0.1", 60000);

    bool running = true;

    {
        Message msg;
        msg.header.id = Common::EMessageType::Send;
        c.send(msg);

        c.incoming().wait();

        while (!c.incoming().empty())
        {
            auto msg = c.incoming().pop_front().msg;
            if (msg.header.id == Common::EMessageType::Reject)
            {
                std::cout << "Server forbids sending a file\n";
                return 1;
            }
            else if (msg.header.id == Common::EMessageType::Accept)
            {
                std::cout << "Server accepted sending a file\n";
                break;
            }
        }
    }

    const uint64_t c_chunksize{128};
    ClientSenderSession session(Session::EPayloadType::File, c.incoming(), std::filesystem::path{"./test.txt"}, c_chunksize, [&c](const Message &msg)
                                { c.send(msg); });
    bool res = session.mainLoop();
    if (!res)
    {
        std::cout << "Operation failed\n";
        return 1;
    }

    c.incoming().wait();

    while (!c.incoming().empty())
    {
        auto msg = c.incoming().pop_front().msg;
        if (msg.header.id == Common::EMessageType::Accept)
        {
            std::cout << "Server confirmed file receival\n";
        }
    }

    return 0;
}
