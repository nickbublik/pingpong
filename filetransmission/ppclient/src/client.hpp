#pragma once

#include <filesystem>
#include <string>

#include <net_common/net_client.hpp>

#include "ppcommon/ppcommon.hpp"

#include "discovery_client.hpp"

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

    bool autoConnect(uint16_t discovery_port, std::chrono::milliseconds timeout, std::chrono::milliseconds polling_delay = std::chrono::milliseconds(50))
    {
        std::cout << __PRETTY_FUNCTION__ << " discovery_port = " << discovery_port << '\n';
        // std::optional<DiscoveredServer> res = discoverServer(m_context, discovery_port, timeout, polling_delay);
        std::optional<DiscoveredServer> res = discoverServerByUnicastBruteforce(m_context, discovery_port, timeout, polling_delay);

        if (res)
        {
            std::cout << "Connecting to " << res->address << ":" << res->port << '\n';
            return connect(res->address, res->port);
        }

        std::cout << "Discovery failed, trying localhost fallback...\n";
        return connect("127.0.0.1", 60010);
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
