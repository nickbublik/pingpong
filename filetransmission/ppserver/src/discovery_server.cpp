#include "discovery_server.hpp"

#include <iostream>

namespace PingPong
{
DiscoveryServer::DiscoveryServer(boost::asio::io_context &io_context, const uint16_t discovery_port, const uint16_t tcp_port)
    //: m_socket{io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), discovery_port)}, m_tcp_port{tcp_port}
    : m_socket{io_context}
{
    m_socket.open(boost::asio::ip::udp::v4());
    m_socket.set_option(boost::asio::socket_base::reuse_address(true));
    m_socket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), discovery_port));
    std::cout << __PRETTY_FUNCTION__ << " discovery_port = " << discovery_port << ", tcp_port = " << tcp_port << '\n';
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
                                        handleRequest(bytes);
                                    }
                                    else
                                        std::cout << "error in listenForRequests : " << ec.message() << '\n';

                                    listenForRequests();
                                });

    std::cout << __PRETTY_FUNCTION__ << " 2\n";
}

void DiscoveryServer::handleRequest(const std::size_t bytes)
{
    std::cout << __PRETTY_FUNCTION__ << " 1\n";
    std::string_view data(m_buffer.data(), bytes);
    std::cout << "[DISCOVERY SERVER] " << __PRETTY_FUNCTION__ << " data = " << data << '\n';

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
                    std::cout << "[DISCOVERY SERVER] Responded to " << m_remote_endpoint.address().to_string() << '\n';
                else
                    std::cout << "[DISCOVERY SERVER]: error in sending the response\n";
            });
    }
    else
    {
        std::cout << "[DISCOVERY SERVER]: phrase is not compatible. Skipping.\n";
    }

    std::cout << __PRETTY_FUNCTION__ << " 2\n";
}
} // namespace PingPong
