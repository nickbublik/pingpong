#pragma once

#include <filesystem>
#include <optional>
#include <unordered_map>

#include <openssl/x509.h>

#include <net_common/net_common.hpp>

namespace PingPong
{
namespace Tofu
{
std::optional<std::string> getSHA256Fingerprint(X509 *cert);
std::optional<std::string> getSHA256CertFingerprintFromSSLStream(boost::asio::ssl::stream<boost::asio::ip::tcp::socket> &socket);
std::string getShortenedCode(const std::string &fingerprint);

std::unordered_map<std::string, std::string> loadKnownHosts(const std::filesystem::path &path);
void saveKnownHosts(const std::filesystem::path &path, const std::unordered_map<std::string, std::string> &m);

} // namespace Tofu
} // namespace PingPong
