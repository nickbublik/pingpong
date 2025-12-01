#include <filesystem>
#include <iostream>

#include <boost/program_options.hpp>

#include <net_common/net_client.hpp>

#include "ppcommon/ppcommon.hpp"
#include "ppcommon/session.hpp"

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

namespace po = boost::program_options;
namespace fs = std::filesystem;

enum class EOperationType
{
    Help,
    Send,
    Receive
};

struct Operation
{
    EOperationType type;
    fs::path file_to_send;
    std::string receival_code_phrase;
};

Operation readArguments(int argc, char **argv)
{
    Operation op;

    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
        ("help", "Available options:\nsend <filepath>\nreceive <code-phrase>")
        ("send", po::value<fs::path>(), "File to send")
        ("receive", po::value<std::string>(), "File to send");
    // clang-format on

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        std::cout << desc << "\n";
        op.type = EOperationType::Help;
        return op;
    }

    if (vm.count("send"))
    {
        op.type = EOperationType::Send;
        op.file_to_send = vm["send"].as<fs::path>();
        std::cout << "Client wants to send file " << op.file_to_send << '\n';
        if (!fs::exists(op.file_to_send))
        {
            throw std::runtime_error("File doesn't exist");
        }
    }
    else if (vm.count("receive"))
    {
        op.type = EOperationType::Receive;
        op.receival_code_phrase = vm["receive"].as<std::string>();
        std::cout << "Client wants to receive file by a code phrase " << op.receival_code_phrase << '\n';
    }
    else
    {
        throw std::runtime_error("Invalid arguments");
    }

    return op;
}

bool sendRoutine(PingPong::FileClient &c, const Operation &op)
{
    std::cout << __PRETTY_FUNCTION__ << '\n';

    using namespace PingPong;

    uint64_t chunksize;

    {
        try
        {
            Message msg;
            msg.header.id = Common::EMessageType::Send;

            Common::PreMetadata request;
            {
                request.payload_type = Common::EPayloadType::File;

                request.code_phrase.code = "abc";
                request.code_phrase.code_size = request.code_phrase.code.size();

                request.file_data.file_name = op.file_to_send.filename();
                request.file_data.file_name_size = request.file_data.file_name.size();
                request.file_data.file_size = fs::file_size(op.file_to_send);
            }

            std::cout << "Code: " << request.code_phrase.code << '\n';

            msg << request;
            std::cout << msg << '\n';
            c.send(std::move(msg));
        }
        catch (const fs::filesystem_error &e)
        {
            std::cerr << "Caught the exception: " << e.what();
            return false;
        }

        c.incoming().wait();

        while (!c.incoming().empty())
        {
            auto msg = c.incoming().pop_front().msg;
            if (msg.header.id == Common::EMessageType::Reject)
            {
                std::cout << "Server forbids sending a file\n";
                return false;
            }
            else if (msg.header.id == Common::EMessageType::Accept)
            {
                msg >> chunksize;
                std::cout << "Server accepted sending a file with max chunksize of " << chunksize << " bytes\n";
                break;
            }
        }
    }

    std::cout << "Preparing to send file. TODO...\n";
    std::this_thread::sleep_for(std::chrono::seconds(30));

    ClientSenderSession session(Common::EPayloadType::File, c.incoming(), fs::path{"./test.txt"}, chunksize, [&c](Message &&msg)
                                { c.send(std::move(msg)); });
    bool res = session.mainLoop();
    if (!res)
    {
        std::cout << "Operation failed\n";
        return false;
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

    return true;
}

bool receiveRoutine(PingPong::FileClient &c, const Operation &op)
{
    std::cout << __PRETTY_FUNCTION__ << '\n';

    using namespace PingPong;

    uint64_t chunksize;

    {
        {
            Message msg;
            msg.header.id = Common::EMessageType::RequestReceive;

            Common::PreMetadata request;
            {
                request.payload_type = Common::EPayloadType::File;

                request.code_phrase.code = op.receival_code_phrase;
                request.code_phrase.code_size = request.code_phrase.code.size();
            }

            msg << request;
            std::cout << msg << '\n';
            c.send(std::move(msg));
        }

        c.incoming().wait();

        while (!c.incoming().empty())
        {
            auto msg = c.incoming().pop_front().msg;
            if (msg.header.id == Common::EMessageType::Reject)
            {
                std::cout << "Server forbids receiving a file\n";
                return false;
            }
            else if (msg.header.id == Common::EMessageType::Accept)
            {
                Common::PostMetadata response;
                msg >> response;
                std::cout << "Do you want to accept an incoming file \"" << response.file_data.file_name << "\" of size " << response.file_data.file_size << "? [y/N]\n";
                break;
            }
        }

        char ans = 'n';
        std::cin >> ans;

        if (!(ans == 'y' || ans == 'Y'))
        {
            return true;
        }
    }

    {
        Message msg;
        msg.header.id = Common::EMessageType::Receive;
        std::cout << msg << '\n';
        c.send(std::move(msg));
    }

    std::cout << "Preparing to receive file. TODO...\n";
    std::this_thread::sleep_for(std::chrono::seconds(30));

    ClientSenderSession session(Common::EPayloadType::File, c.incoming(), fs::path{"./test.txt"}, chunksize, [&c](Message &&msg)
                                { c.send(std::move(msg)); });
    bool res = session.mainLoop();
    if (!res)
    {
        std::cout << "Operation failed\n";
        return false;
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

    return true;
}

int main(int argc, char *argv[])
{
    Operation op;

    try
    {
        op = readArguments(argc, argv);
    }
    catch (std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    catch (...)
    {
        std::cerr << "Exception of unknown type!\n";
        return 1;
    }

    if (op.type == EOperationType::Help)
    {
        return 0;
    }

    PingPong::FileClient c;
    bool connected = c.connect("127.0.0.1", 60000);
    if (!connected)
    {
        std::cerr << "Failed to connect to the server\n";
        return 1;
    }

    while (!c.isConnected())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    bool result;

    if (op.type == EOperationType::Send)
    {
        result = sendRoutine(c, op);
    }
    else if (op.type == EOperationType::Receive)
    {
        result = receiveRoutine(c, op);
    }

    return !result;
}
