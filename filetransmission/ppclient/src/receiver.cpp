#include "receiver.hpp"

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

bool establishSession(FileClient &c, const Operation &op)
{
    {
        PreMetadata pre;
        {
            pre.payload_type = EPayloadType::File;

            pre.code_phrase.code = op.receival_code_phrase;
            pre.code_phrase.code_size = pre.code_phrase.code.size();
        }

        Message req_receive_msg = encode<EMessageType::RequestReceive>(pre);

        std::cout << req_receive_msg << '\n';
        c.send(std::move(req_receive_msg));
    }

    c.incoming().wait();

    while (!c.incoming().empty())
    {
        auto msg = c.incoming().pop_front().msg;
        if (msg.header.id == EMessageType::Reject)
        {
            std::cout << "Server forbids receiving a file\n";
            return false;
        }
        else if (msg.header.id == EMessageType::Accept)
        {
            PostMetadata response = decode<EMessageType::Accept>(msg);
            std::cout << "Do you want to accept an incoming file \"" << response.file_data.file_name << "\" of size " << response.file_data.file_size << "? [y/N]\n";
            break;
        }
    }

    return true;
}

bool startSession(FileClient &c, const Operation &op)
{
    {
        CodePhrase code_phrase;
        code_phrase.code = op.receival_code_phrase;
        code_phrase.code_size = code_phrase.code.size();

        Message receive_msg = encode<EMessageType::Receive>(code_phrase);
        std::cout << receive_msg << '\n';
        c.send(std::move(receive_msg));
    }

    std::filesystem::path outfile{"./out"};

    ClientReceiverSession session(EPayloadType::File,
                                  c.incoming(),
                                  outfile,
                                  [&c](Message &&msg)
                                  { c.send(std::move(msg)); });

    bool res = session.mainLoop();

    if (!res)
        std::cout << "Receiving routine has failed\n";

    return res;
}
} // namespace

bool receiveRoutine(const Operation &op)
{
    std::cout << __PRETTY_FUNCTION__ << '\n';

    FileClient c;

    if (!waitForConnection(c))
        return false;

    if (!establishSession(c, op))
        return false;

    char ans = 'n';
    std::cin >> ans;

    // Skipping the receival because used declined it
    if (!(ans == 'y' || ans == 'Y'))
        return true;

    if (!startSession(c, op))
        return false;

    // Signal about the successful end of transmission
    {
        Message fin_receive_msg = encode<EMessageType::FinishReceive>(Empty{});
        c.send(std::move(fin_receive_msg));
        std::cout << "Sending FinishReceive. m_pending_writes = " << c.getPendingWrites() << '\n';

        // wait till all outgoing messaged are sent
        c.flush();
        std::cout << "After flush. m_pending_writes = " << c.getPendingWrites() << '\n';
    }

    return true;
}
} // namespace PingPong
