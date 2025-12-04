#include "receiver.hpp"

#include "ppcommon/session.hpp"

namespace PingPong
{
bool receiveRoutine(const Operation &op)
{
    std::cout << __PRETTY_FUNCTION__ << '\n';

    FileClient c;
    bool connected = c.connect("127.0.0.1", 60000);

    uint64_t chunksize;

    if (!connected)
    {
        std::cerr << "Failed to connect to the server\n";
        return 1;
    }

    while (!c.isConnected())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

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
        Common::CodePhrase code_phrase;
        code_phrase.code = op.receival_code_phrase;
        code_phrase.code_size = code_phrase.code.size();

        msg.header.id = Common::EMessageType::Receive;
        msg << code_phrase;
        std::cout << msg << '\n';
        c.send(std::move(msg));
    }

    std::filesystem::path outfile{"./out.txt"};

    ClientReceiverSession session(Common::EPayloadType::File, c.incoming(), outfile, [&c](Message &&msg)
                                  { c.send(std::move(msg)); });
    bool res = session.mainLoop();
    if (!res)
    {
        std::cout << "Receiving routine has failed\n";
        return false;
    }

    // Signal about the successful end of transmission
    {
        Message msg;
        msg.header.id = Common::EMessageType::Success;
        Common::CodePhrase code_phrase;
        code_phrase.code = op.receival_code_phrase;
        code_phrase.code_size = code_phrase.code.size();
        msg << code_phrase;
        c.send(std::move(msg));

        std::cout << "Receiver has sent Success message\n";
        std::cout << "Before flush: m_pending_writes = " << c.getPendingWrites() << '\n';
        c.flush();
        std::cout << "After flush: m_pending_writes = " << c.getPendingWrites() << '\n';
    }

    return true;
}
} // namespace PingPong
