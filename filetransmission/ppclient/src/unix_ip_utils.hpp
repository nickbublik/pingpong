#include <boost/asio/ip/address_v4.hpp>

#include <optional>

namespace PingPong
{
struct SubnetRange
{
    boost::asio::ip::address_v4 network;
    boost::asio::ip::address_v4 broadcast;
    boost::asio::ip::address_v4 first_host;
    boost::asio::ip::address_v4 last_host;
    std::uint32_t host_count; // number of usable hosts
};

std::optional<boost::asio::ip::address_v4> getLocalIPv4(std::string &out_ifc_name);

std::optional<boost::asio::ip::address_v4> getNetmaskForIP(const boost::asio::ip::address_v4 &ip);

SubnetRange getSubnetRange(boost::asio::ip::address_v4 ip,
                           boost::asio::ip::address_v4 mask);

} // namespace PingPong
