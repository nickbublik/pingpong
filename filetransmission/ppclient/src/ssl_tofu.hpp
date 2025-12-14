#pragma once

#include <filesystem>
#include <unordered_map>

#include <net_common/net_common.hpp>

namespace PingPong
{
std::string getSHA256CertFingerprintFromSSLStream(boost::asio::ssl::stream<boost::asio::ip::tcp::socket> &socket);
std::string getShortenedCode(const std::string &fingerprint);

std::unordered_map<std::string, std::string> loadKnownHosts(const std::filesystem::path &path);
void saveKnownHosts(const std::filesystem::path &path, const std::unordered_map<std::string, std::string> &m);

} // namespace PingPong

