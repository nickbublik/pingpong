#include "unix_ip_utils.hpp"

#include <boost/asio.hpp>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>

namespace PingPong
{

boost::asio::ip::address_v4 getLocalIPv4()
{
    using boost::asio::ip::udp;

    boost::asio::io_context ctx;
    udp::socket tmp(ctx);

    tmp.connect(udp::endpoint(boost::asio::ip::make_address_v4("8.8.8.8"), 80));

    auto endpoint = tmp.local_endpoint();
    auto ip = endpoint.address().to_v4();
    return ip;
}

std::optional<boost::asio::ip::address_v4> getNetmaskForIP(const boost::asio::ip::address_v4 &ip)
{
    struct ifaddrs *ifaddr = nullptr;

    if (getifaddrs(&ifaddr) == -1)
        return std::nullopt;

    std::optional<boost::asio::ip::address_v4> res;

    for (auto *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
            continue;

        auto *addr = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
        auto *mask = reinterpret_cast<sockaddr_in *>(ifa->ifa_netmask);

        if (!addr || !mask)
            continue;

        boost::asio::ip::address_v4 addr_ip(ntohl(addr->sin_addr.s_addr));

        if (addr_ip == ip)
        {
            boost::asio::ip::address_v4 mask_ip(ntohl(mask->sin_addr.s_addr));
            res = mask_ip;
            break;
        }
    }

    freeifaddrs(ifaddr);
    return res;
}

SubnetRange getSubnetRange(boost::asio::ip::address_v4 ip,
                           boost::asio::ip::address_v4 mask)
{
    using boost::asio::ip::address_v4;

    const uint32_t ip_int = ip.to_uint();
    const uint32_t mask_int = mask.to_uint();
    const uint32_t net_int = ip_int & mask_int;
    const uint32_t broadcast_int = net_int | ~mask_int;

    SubnetRange res;
    res.network = address_v4(net_int);
    res.broadcast = address_v4(broadcast_int);

    if (broadcast_int - net_int >= 2)
    {
        res.first_host = address_v4(net_int + 1);
        res.last_host = address_v4(broadcast_int - 1);
        res.host_count = broadcast_int - net_int - 1;
    }
    else
    {
        res.first_host = address_v4(net_int);
        res.last_host = address_v4(net_int);
        res.host_count = 0;
    }

    return res;
}

} // namespace PingPong
