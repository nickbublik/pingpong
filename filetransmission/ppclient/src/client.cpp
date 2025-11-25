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
    uint64_t chunksize;

    {
        {
            Message msg;
            msg.header.id = Common::EMessageType::Send;

            // EPayloadType payload_type;
            // uint64_t size;
            // std::vector<uint8_t> code;
            std::string code{"my-code-6"};
            std::cout << "TR#0\n";
            msg << Common::EPayloadType::File << static_cast<uint64_t>(72);
            size_t offset = msg.body.size();
            msg.body.resize(msg.body.size() + code.size());
            std::memcpy(msg.body.data() + offset, code.data(), code.size());
            c.send(std::move(msg));
        }

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
                msg >> chunksize;
                std::cout << "Server accepted sending a file with max chunksize of " << chunksize << " bytes\n";
                break;
            }
        }
    }

    ClientSenderSession session(Common::EPayloadType::File, c.incoming(), std::filesystem::path{"./test.txt"}, chunksize, [&c](Message &&msg)
                                { c.send(std::move(msg)); });
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
