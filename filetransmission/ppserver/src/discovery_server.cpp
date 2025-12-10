#include "discovery_server.hpp"

#include <boost/asio.hpp>

#include <iostream>

namespace PingPong
{
DiscoveryServer::DiscoveryServer(boost::asio::io_context &io_context, const uint16_t discovery_port, const uint16_t tcp_port)
    //: m_socket{io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), discovery_port)}, m_tcp_port{tcp_port}
    : m_socket{io_context}, m_tcp_port{tcp_port}
{
    using udp = boost::asio::ip::udp;
    boost::system::error_code ec;

    m_socket.open(udp::v4(), ec);
    if (ec)
    {
        std::cerr << "[DISCOVERY SERVER] open error: " << ec.message() << '\n';
        return;
    }

    m_socket.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec)
    {
        std::cerr << "[DISCOVERY SERVER] reuse_address error: " << ec.message() << '\n';
        return;
    }

    m_socket.bind(udp::endpoint(udp::v4(), discovery_port), ec);
    if (ec)
    {
        std::cerr << "[DISCOVERY SERVER] bind error: " << ec.message() << '\n';
        return;
    }

    // Print what we actually bound to
    udp::endpoint ep = m_socket.local_endpoint(ec);
    if (!ec)
    {
        std::cout << "[DISCOVERY SERVER] bound to "
                  << ep.address().to_string() << ":" << ep.port() << '\n';
    }

    std::cout << __PRETTY_FUNCTION__
              << " discovery_port = " << discovery_port
              << ", tcp_port = " << tcp_port << '\n';

    listenForRequests();
}

void DiscoveryServer::listenForRequests()
{
    std::cout << __PRETTY_FUNCTION__ << " 1\n";
    m_socket.async_receive_from(boost::asio::buffer(m_buffer), m_remote_endpoint,
                                [this](std::error_code ec, std::size_t bytes)
                                {
                                    if (!ec)
                                    {
                                        std::cout << "[DISCOVERY SERVER] got " << bytes
                                                  << " bytes from " << m_remote_endpoint.address().to_string()
                                                  << ":" << m_remote_endpoint.port() << '\n';

                                        handleRequest(bytes);
                                    }
                                    else
                                    {
                                        std::cout << "[DISCOVERY SERVER] error in listenForRequests : "
                                                  << ec.message() << '\n';
                                    }

                                    listenForRequests();
                                });

    std::cout << __PRETTY_FUNCTION__ << " 2\n";
}

void DiscoveryServer::handleRequest(const std::size_t bytes)
{
    std::cout << __PRETTY_FUNCTION__ << " 1, bytes = " << bytes << '\n';
    std::string_view data(m_buffer.data(), bytes);
    std::cout << "[DISCOVERY SERVER] payload = '" << data << "'\n";

    if (data == c_discovery_phrase)
    {
        std::cout << "[DISCOVERY SERVER]: phrase is ok.\n";
        const std::string response = std::string(c_response_phrase) + "/" + std::to_string(m_tcp_port);

        m_socket.async_send_to(
            boost::asio::buffer(response),
            m_remote_endpoint,
            [this](std::error_code ec, std::size_t)
            {
                if (!ec)
                    std::cout << "[DISCOVERY SERVER] Responded to "
                              << m_remote_endpoint.address().to_string() << '\n';
                else
                    std::cout << "[DISCOVERY SERVER]: error in sending the response: "
                              << ec.message() << '\n';
            });
    }
    else
        std::cout << "[DISCOVERY SERVER]: phrase is not compatible. Skipping.\n";

    std::cout << __PRETTY_FUNCTION__ << " 2\n";
}
} // namespace PingPong
