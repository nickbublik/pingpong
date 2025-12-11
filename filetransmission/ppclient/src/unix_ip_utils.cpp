#include "unix_ip_utils.hpp"

#include <boost/asio.hpp>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>

namespace PingPong
{

namespace
{
using boost::asio::ip::address_v4;

// Check if an IPv4 address (host byte order) is private
bool isPrivateIPv4(uint32_t horder_addr)
{
    // 10.0.0.0/8
    if ((horder_addr & 0xFF000000u) == 0x0A000000u)
        return true;

    // 172.16.0.0/12
    if ((horder_addr & 0xFFF00000u) == 0xAC100000u)
        return true;

    // 192.168.0.0/16
    if ((horder_addr & 0xFFFF0000u) == 0xC0A80000u)
        return true;

    return false;
}

// Link-local 169.254.0.0/16
bool isLinkLocalIPv4(uint32_t hostOrderAddr)
{
    return (hostOrderAddr & 0xFFFF0000u) == 0xA9FE0000u;
}

// Prefer private IPv4.
// Otherwise: fallback to any non-loopback, non–link-local IPv4
} // namespace

std::optional<address_v4> getLocalIPv4(std::string &out_ifc_name)
{
    struct ifaddrs *ifaddr = nullptr;

    if (getifaddrs(&ifaddr) == -1)
        return std::nullopt;

    std::optional<address_v4> best_private;
    std::optional<address_v4> best_nonloopback;
    std::string best_private_ifc, best_nonloopback_if;

    for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr)
            continue;

        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;

        unsigned flags = ifa->ifa_flags;

        if (!(flags & IFF_UP)) // skip inactive ifc
            continue;

        if (flags & IFF_LOOPBACK) // skip lo ifc
            continue;

        auto *sa = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
        uint32_t addr_net = sa->sin_addr.s_addr;
        uint32_t addr_host = ntohl(addr_net);

        if (isLinkLocalIPv4(addr_host))
            continue;

        address_v4 addr = address_v4(addr_host);

        std::string ifc_name = ifa->ifa_name ? ifa->ifa_name : "";

        if (isPrivateIPv4(addr_host))
        {
            if (!best_private)
            {
                best_private = addr;
                best_private_ifc = ifc_name;
            }
        }
        else
        {
            if (!best_nonloopback)
            {
                best_nonloopback = addr;
                best_nonloopback_if = ifc_name;
            }
        }
    }

    freeifaddrs(ifaddr);

    if (best_private)
    {
        out_ifc_name = best_private_ifc;
        return best_private;
    }

    if (best_nonloopback)
    {
        out_ifc_name = best_nonloopback_if;
        return best_nonloopback;
    }

    return std::nullopt;
}

// boost::asio::ip::address_v4 getLocalIPv4()
//{
//     using boost::asio::ip::udp;
//
//     boost::asio::io_context ctx;
//     udp::socket tmp(ctx);
//
//     tmp.connect(udp::endpoint(boost::asio::ip::make_address_v4("8.8.8.8"), 80));
//
//     auto endpoint = tmp.local_endpoint();
//     auto ip = endpoint.address().to_v4();
//     return ip;
// }

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
