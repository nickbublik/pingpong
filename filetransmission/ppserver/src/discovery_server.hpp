#pragma once

#include <net_common/net_common.hpp>

namespace PingPong
{

class DiscoveryServer
{
  public:
    DiscoveryServer(boost::asio::io_context &io_context, const uint16_t discovery_port, const uint16_t tcp_port);

    void listenForRequests();
    void handleRequest(const std::size_t bytes);

  private:
    static constexpr char c_discovery_phrase[] = "pingpong_discover_v1";
    static constexpr char c_response_phrase[] = "pingpong_server_v1";

    boost::asio::ip::udp::socket m_socket;
    boost::asio::ip::udp::endpoint m_remote_endpoint;
    std::array<char, 1024> m_buffer;
    uint16_t m_tcp_port;
};

} // namespace PingPong
