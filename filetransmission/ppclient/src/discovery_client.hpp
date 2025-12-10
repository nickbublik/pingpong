#pragma once

#include <boost/asio.hpp>

#include <chrono>
#include <optional>

namespace PingPong
{
struct DiscoveredServer
{
    std::string address;
    uint16_t port;
};

std::optional<DiscoveredServer> discoverServer(boost::asio::io_context &context,
                                               uint16_t discovery_port,
                                               std::chrono::milliseconds timeout,
                                               std::chrono::milliseconds polling_delay);

} // namespace PingPong
