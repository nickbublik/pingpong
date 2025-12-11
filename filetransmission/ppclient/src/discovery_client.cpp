#include "discovery_client.hpp"

#include <array>
#include <iostream>
#include <optional>
#include <thread>

#include <boost/asio/ip/address_v4.hpp>

#include <logger/logger.hpp>

#include "unix_ip_utils.hpp"

namespace PingPong
{

std::string getLocalSubnet()
{
    using boost::asio::ip::udp;

    boost::asio::io_context ctx;
    udp::socket tmp(ctx);

    tmp.connect(udp::endpoint(boost::asio::ip::make_address_v4("8.8.8.8"), 80));

    auto endpoint = tmp.local_endpoint();
    auto ip = endpoint.address().to_v4();

    // Example: 192.168.0.108 -> "192.168.0."
    auto raw = ip.to_string();
    auto lastDot = raw.rfind('.');
    return raw.substr(0, lastDot + 1);
}

std::optional<DiscoveredServer> discoverServerByUnicastBruteforce(
    boost::asio::io_context &context,
    uint16_t discovery_port,
    std::chrono::milliseconds timeout,
    std::chrono::milliseconds polling_delay)
{
    using boost::asio::ip::udp;
    static constexpr char c_discovery_phrase[] = "pingpong_discover_v1";
    static constexpr char c_response_phrase[] = "pingpong_server_v1";

    std::string ifc;
    auto local_ip_opt = getLocalIPv4(ifc);

    if (!local_ip_opt)
    {
        DBG_LOG("[DISCOVERY] failed to get local ip");
        return std::nullopt;
    }

    DBG_LOG("[DISCOVERY] local ip = ", local_ip_opt->to_string());

    auto netmask_opt = getNetmaskForIP(*local_ip_opt);

    if (!netmask_opt)
    {
        DBG_LOG("[DISCOVERY] failed to get netmask");
        return std::nullopt;
    }

    SubnetRange subrange = getSubnetRange(*local_ip_opt, *netmask_opt);

    DBG_LOG("[DISCOVERY] local_ip   = ", local_ip_opt->to_string());
    DBG_LOG("[DISCOVERY] netmask    = ", netmask_opt->to_string());
    DBG_LOG("[DISCOVERY] network    = ", subrange.network.to_string());
    DBG_LOG("[DISCOVERY] broadcast  = ", subrange.broadcast.to_string());
    DBG_LOG("[DISCOVERY] first_host = ", subrange.first_host.to_string());
    DBG_LOG("[DISCOVERY] last_host  = ", subrange.last_host.to_string());
    DBG_LOG("[DISCOVERY] host_count = ", subrange.host_count);

    udp::socket sock(context);
    sock.open(udp::v4());
    sock.bind(udp::endpoint(udp::v4(), 0));
    sock.non_blocking(true);

    const uint32_t net_int = subrange.network.to_uint();
    const uint32_t first_host_int = subrange.first_host.to_uint();
    const uint32_t last_host_int = subrange.last_host.to_uint();

    for (uint32_t addr_int = first_host_int; addr_int <= last_host_int; ++addr_int)
    {
        boost::asio::ip::address_v4 addr(addr_int);
        if (addr == *local_ip_opt)
            continue;

        udp::endpoint endpoint(addr, discovery_port);
        boost::system::error_code ec;
        sock.send_to(boost::asio::buffer(c_discovery_phrase, strlen(c_discovery_phrase)), endpoint, 0, ec);
        std::ignore = ec;
    }

    std::array<char, 1024> buffer;
    udp::endpoint sender_endpoint;
    auto start = std::chrono::steady_clock::now();

    // Wait for response(s)
    while (true)
    {
        boost::system::error_code ec;
        size_t bytes = sock.receive_from(boost::asio::buffer(buffer),
                                         sender_endpoint, 0, ec);

        if (!ec && ec != boost::asio::error::would_block)
            DBG_LOG("[DISCOVERY] error : ", ec.message());

        if (!ec && bytes > 0)
        {
            DBG_LOG("[DISCOVERY] response, no error");
            std::string_view data(buffer.data(), bytes);

            if (data.rfind(c_response_phrase, 0) == 0)
            {
                auto slash = data.find('/');
                uint16_t port = 0;
                if (slash != std::string_view::npos)
                    port = static_cast<uint16_t>(
                        std::stoi(std::string(data.substr(slash + 1))));

                DiscoveredServer result;
                result.address = sender_endpoint.address().to_string();
                result.port = port;

                DBG_LOG("[DISCOVERY] found server at ", result.address, ":", result.port);

                return result;
            }
        }

        if (std::chrono::steady_clock::now() - start > timeout)
            break;

        std::this_thread::sleep_for(polling_delay);
    }

    DBG_LOG("[DISCOVERY] no servers found via unicast scan.");
    return std::nullopt;
}

std::optional<DiscoveredServer> discoverServerByBroadcast(boost::asio::io_context &context,
                                                          uint16_t discovery_port,
                                                          std::chrono::milliseconds timeout,
                                                          std::chrono::milliseconds polling_delay)
{
    using boost::asio::ip::udp;

    static constexpr char c_discovery_phrase[] = "pingpong_discover_v1";
    static constexpr char c_response_phrase[] = "pingpong_server_v1";

    udp::socket socket(context);
    socket.open(udp::v4());
    socket.set_option(boost::asio::socket_base::reuse_address(true));
    socket.bind(udp::endpoint(udp::v4(), 0));

    socket.set_option(boost::asio::socket_base::broadcast(true));
    udp::endpoint broadcast_endpoint(boost::asio::ip::address_v4::broadcast(), discovery_port);

    socket.send_to(boost::asio::buffer(c_discovery_phrase, std::strlen(c_discovery_phrase)), broadcast_endpoint);

    socket.non_blocking(true);

    const auto start_ts = std::chrono::steady_clock::now();

    std::array<char, 1024> buffer;
    udp::endpoint sender_endpoint;

    while (true)
    {
        boost::system::error_code ec;
        size_t bytes = socket.receive_from(boost::asio::buffer(buffer), sender_endpoint, 0, ec);

        if (!ec && ec != boost::asio::error::would_block)
            DBG_LOG("[DISCOVERY] error : ", ec.message());

        if (!ec && bytes > 0)
        {
            std::string_view data(buffer.data(), bytes);
            // Expect "pingpong_server_v1/60010"
            if (data.rfind(c_response_phrase, 0) == 0)
            {
                auto separator = data.find('/');
                uint16_t port = 0;

                if (separator != std::string_view::npos)
                {
                    port = static_cast<uint16_t>(std::stoi(std::string(data.substr(separator + 1))));
                }

                DiscoveredServer res;
                res.address = sender_endpoint.address().to_string();
                res.port = port;

                DBG_LOG("Found DiscoveredServer: address = ", res.address, ", port = ", res.port);
                return res;
            }
        }

        const auto now_ts = std::chrono::steady_clock::now();

        if (now_ts - start_ts > timeout)
            break;

        std::this_thread::sleep_for(polling_delay);
    }

    return std::nullopt;
}
} // namespace PingPong
