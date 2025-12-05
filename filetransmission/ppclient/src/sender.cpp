#include "sender.hpp"

#include "ppcommon/session.hpp"

namespace PingPong
{
using namespace Common;

bool sendRoutine(const Operation &op)
{
    std::cout << __PRETTY_FUNCTION__ << '\n';

    FileClient c;
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

    uint64_t chunksize;

    {
        try
        {
            PreMetadata pre;
            {
                pre.payload_type = EPayloadType::File;

                pre.code_phrase.code = "abc";
                pre.code_phrase.code_size = pre.code_phrase.code.size();

                pre.file_data.file_name = op.filepath.filename();
                pre.file_data.file_name_size = pre.file_data.file_name.size();
                pre.file_data.file_size = fs::file_size(op.filepath);
            }
            Message send_msg = encode<EMessageType::Send>(pre);

            std::cout << "Code: " << pre.code_phrase.code << '\n';

            c.send(std::move(send_msg));
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
            if (msg.header.id == EMessageType::Reject)
            {
                std::cout << "Server forbids sending a file\n";
                return false;
            }
            else if (msg.header.id == EMessageType::Accept)
            {
                PostMetadata post_metadata = decode<EMessageType::Accept>(msg);
                chunksize = post_metadata.max_chunk_size;
                std::cout << "Server accepted sending a file with max chunksize of " << chunksize << " bytes\n";
                break;
            }
        }
    }

    ClientSenderSession session(EPayloadType::File, c.incoming(), op.filepath, chunksize, [&c](Message &&msg)
                                { c.send(std::move(msg)); });

    bool res = session.mainLoop();
    if (!res)
    {
        std::cout << "Sending routine has failed\n";
        return false;
    }

    std::cout << __PRETTY_FUNCTION__ << " waiting for Success message" << '\n';

    bool success_from_receiver = false;

    while (!success_from_receiver)
    {
        c.incoming().wait();

        while (!c.incoming().empty())
        {
            auto msg = c.incoming().pop_front().msg;
            if (msg.header.id == EMessageType::Success)
            {
                std::cout << "Server confirmed file receival\n";
                success_from_receiver = true;
                break;
            }
        }
    }

    return true;
}

} // namespace PingPong
