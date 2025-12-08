#pragma once

#include <filesystem>
#include <string>

#include <net_common/net_client.hpp>

#include "ppcommon/ppcommon.hpp"

namespace PingPong
{
using Message = Common::Message;

class FileClient : public Net::ClientBase<Common::EMessageType>
{
  public:
    FileClient()
    {
    }

    ~FileClient() override = default;

    void waitForIncomingQueueMessage(const std::chrono::milliseconds &check_period)
    {
        m_connection->waitForIncomingQueueMessage(check_period);
    }
};

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
    fs::path filepath;
    std::string receival_code_phrase;
};
} // namespace PingPong
