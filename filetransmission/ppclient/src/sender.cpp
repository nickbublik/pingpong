#include "sender.hpp"

#include "ppcommon/session.hpp"

namespace PingPong
{
bool sendRoutine(const Operation &op)
{
    std::cout << __PRETTY_FUNCTION__ << '\n';

    FileClient c;
    bool connected = c.connect("127.0.0.1", 60000);

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

                request.file_data.file_name = op.filepath.filename();
                request.file_data.file_name_size = request.file_data.file_name.size();
                request.file_data.file_size = fs::file_size(op.filepath);
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
                Common::PostMetadata post_metadata;
                msg >> post_metadata;
                chunksize = post_metadata.max_chunk_size;
                std::cout << "Server accepted sending a file with max chunksize of " << chunksize << " bytes\n";
                break;
            }
        }
    }

    ClientSenderSession session(Common::EPayloadType::File, c.incoming(), op.filepath, chunksize, [&c](Message &&msg)
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
            if (msg.header.id == Common::EMessageType::Success)
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
