#include "sender.hpp"

#include <chrono>

#include "ppcommon/session.hpp"

namespace PingPong
{

using namespace Common;

namespace
{
bool waitForConnection(FileClient &c)
{
    bool connected = c.autoConnect(60009, std::chrono::seconds(2));

    if (!connected)
    {
        std::cerr << "Failed to connect to the server\n";
        return false;
    }

    while (!(c.isConnected() && c.isValidated()))
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return true;
}

bool establishSession(FileClient &c, const Operation &op, uint64_t &out_chunksize)
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

        std::cout << pre.code_phrase.code << '\n';

        if (!c.send(std::move(send_msg)))
            return false;
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
            std::cerr << "Server forbids sending a file\n";
            return false;
        }
        else if (msg.header.id == EMessageType::Accept)
        {
            PostMetadata post_metadata = decode<EMessageType::Accept>(msg);
            out_chunksize = post_metadata.max_chunk_size;
            DBG_LOG("Server accepted sending a file with max chunksize of ", out_chunksize, " bytes");
            break;
        }
    }

    return true;
}

bool startSession(FileClient &c, const Operation &op, const uint64_t chunksize)
{
    ClientSenderSession session(EPayloadType::File, c.incoming(), op.filepath, chunksize, [&c](Message &&msg)
                                { return c.send(std::move(msg)); });

    bool res = session.mainLoop();
    if (!res)
    {
        std::cerr << "Sending routine has failed\n";
        return false;
    }

    return true;
}

bool waitForConfirmation(FileClient &c)
{
    DBG_LOG(__PRETTY_FUNCTION__, " waiting for Success message");

    bool is_completed = false;

    while (!is_completed)
    {
        using namespace std::chrono_literals;
        c.waitForIncomingQueueMessage(50ms);

        while (!c.incoming().empty())
        {
            auto msg = c.incoming().pop_front().msg;
            if (msg.header.id == EMessageType::Abort)
            {
                std::cerr << "Server aborted file receival\n";
                return false;
            }
            else if (msg.header.id == EMessageType::Success)
            {
                DBG_LOG("Server confirmed file receival");
                return true;
            }
        }
    }

    return false;
}

} // namespace

bool sendRoutine(const Operation &op)
{
    DBG_LOG(__PRETTY_FUNCTION__);

    FileClient c;

    if (!waitForConnection(c))
        return false;

    uint64_t chunksize;

    if (!establishSession(c, op, chunksize))
        return false;

    if (!startSession(c, op, chunksize))
        return false;

    return waitForConfirmation(c);
}

} // namespace PingPong
