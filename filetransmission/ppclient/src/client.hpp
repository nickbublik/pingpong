#pragma once

#include <filesystem>
#include <string>

#include <net_common/net_client.hpp>

#include "ppcommon/ppcommon.hpp"

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
