#include "discovery_client.hpp"

#include <array>
#include <boost/asio/ip/address_v4.hpp>
#include <iostream>
#include <thread>

namespace PingPong
{

std::optional<DiscoveredServer> discoverServer(boost::asio::io_context &context,
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
    //udp::endpoint broadcast_endpoint(boost::asio::ip::make_address_v4("192.168.0.255"), discovery_port);

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
