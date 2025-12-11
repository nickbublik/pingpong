#include "discovery_client.hpp"

#include <array>
#include <iostream>
#include <optional>
#include <thread>

#include <boost/asio/ip/address_v4.hpp>

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

    // Example: 192.168.0.108 → "192.168.0."
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

    auto local_ip = getLocalIPv4();
    auto netmask_opt = getNetmaskForIP(local_ip);

    if (!netmask_opt)
    {
        std::cout << "[DISCOVERY] failed to get netmask\n";
        return std::nullopt;
    }

    SubnetRange subrange = getSubnetRange(local_ip, *netmask_opt);

    std::cout << "[DISCOVERY] local_ip   = " << local_ip.to_string() << "\n"
              << "[DISCOVERY] netmask    = " << netmask_opt->to_string() << "\n"
              << "[DISCOVERY] network    = " << subrange.network.to_string() << "\n"
              << "[DISCOVERY] broadcast  = " << subrange.broadcast.to_string() << "\n"
              << "[DISCOVERY] first_host = " << subrange.first_host.to_string() << "\n"
              << "[DISCOVERY] last_host  = " << subrange.last_host.to_string() << "\n"
              << "[DISCOVERY] host_count = " << subrange.host_count << "\n";

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
        if (addr == local_ip)
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

        if (ec != boost::asio::error::would_block)
            std::cout << "[DISCOVERY] error : " << ec.message() << '\n';

        if (!ec && bytes > 0)
        {
            std::cout << "[DISCOVERY] response, no error\n";
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

                std::cout << "[DISCOVERY] found server at "
                          << result.address << ":" << result.port << "\n";

                return result;
            }
        }

        if (std::chrono::steady_clock::now() - start > timeout)
            break;

        std::this_thread::sleep_for(polling_delay);
    }

    std::cout << "[DISCOVERY] no servers found via unicast scan.\n";
    return std::nullopt;
}

std::optional<DiscoveredServer> discoverServerByBroadcast(boost::asio::io_context &context,
                                                          uint16_t discovery_port,
                                                          std::chrono::milliseconds timeout,
                                                          std::chrono::milliseconds polling_delay)
{
    std::cout << __PRETTY_FUNCTION__ << " 1\n";
    using boost::asio::ip::udp;

    static constexpr char c_discovery_phrase[] = "pingpong_discover_v1";
    static constexpr char c_response_phrase[] = "pingpong_server_v1";

    udp::socket socket(context);
    socket.open(udp::v4());
    socket.set_option(boost::asio::socket_base::reuse_address(true));
    socket.bind(udp::endpoint(udp::v4(), 0));
    std::cout << __PRETTY_FUNCTION__ << " 2\n";

    socket.set_option(boost::asio::socket_base::broadcast(true));
    udp::endpoint broadcast_endpoint(boost::asio::ip::address_v4::broadcast(), discovery_port);

    socket.send_to(boost::asio::buffer(c_discovery_phrase, std::strlen(c_discovery_phrase)), broadcast_endpoint);

    std::cout << __PRETTY_FUNCTION__ << " 3\n";

    socket.non_blocking(true);

    const auto start_ts = std::chrono::steady_clock::now();

    std::array<char, 1024> buffer;
    udp::endpoint sender_endpoint;

    std::cout << __PRETTY_FUNCTION__ << " 4\n";

    while (true)
    {
        boost::system::error_code ec;
        size_t bytes = socket.receive_from(boost::asio::buffer(buffer), sender_endpoint, 0, ec);

        if (ec != boost::asio::error::would_block)
            std::cout << __PRETTY_FUNCTION__ << " error : " << ec.message() << '\n';

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

                std::cout << "Found DiscoveredServer: address = " << res.address << ", port = " << res.port << '\n';
                return res;
            }
        }

        const auto now_ts = std::chrono::steady_clock::now();

        if (now_ts - start_ts > timeout)
            break;

        std::this_thread::sleep_for(polling_delay);
    }

    std::cout << __PRETTY_FUNCTION__ << " 6\n";

    return std::nullopt;
}
} // namespace PingPong
