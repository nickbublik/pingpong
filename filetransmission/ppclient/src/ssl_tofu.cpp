#include "ssl_tofu.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <logger/logger.hpp>

namespace PingPong
{
using tcp = boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;

std::string getSHA256CertFingerprintFromSSLStream(ssl::stream<tcp::socket> &socket)
{
    SSL *ssl = socket.native_handle();
    X509 *cert = SSL_get1_peer_certificate(ssl);

    if (!cert)
        return {};

    unsigned char digest[EVP_MAX_MD_SIZE];
    uint32_t digest_len = 0;

    if (!X509_digest(cert, EVP_sha256(), digest, &digest_len))
    {
        X509_free(cert);
        return {};
    }
    X509_free(cert);

    std::stringstream ss;
    for (uint32_t i = 0; i < digest_len; ++i)
    {
        if (i)
            ss << ':';

        ss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }

    return ss.str();
}

std::string getShortenedCode(const std::string &fingerprint)
{
    DBG_LOG("getShortenedCode from fingerprint: ", fingerprint);
    const size_t cnt = 11;
    std::string hex;
    hex.reserve(std::min(cnt, fingerprint.size()));

    size_t i;
    if (fingerprint.size() < cnt)
    {
        i = 0;
    }
    else
    {
        i = fingerprint.size() - cnt;
    }

    for (; i < fingerprint.size(); ++i)
    {
        hex.push_back(fingerprint[i]);
    }

    DBG_LOG("shortened code: ", hex);

    return hex;
}

std::filesystem::path defaultKnownHostsPath()
{
    const char *home = std::getenv("HOME");
    std::filesystem::path p = home ? home : ".";
    return p / ".config" / "pingpong" / "known_hosts";
}

std::unordered_map<std::string, std::string> loadKnownHosts(const std::filesystem::path &path)
{
    std::unordered_map<std::string, std::string> m;
    std::ifstream ifs(path);
    if (!ifs)
        return m;

    std::string id, fingerprint;

    while (ifs >> id >> fingerprint)
        m[id] = fingerprint;

    return m;
}

void saveKnownHosts(const std::filesystem::path &path, const std::unordered_map<std::string, std::string> &m)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs)
        throw std::runtime_error("Cannot write known_hosts: " + path.string());

    for (auto &[id, fingerprint] : m)
        ofs << id << " " << fingerprint << '\n';
}

} // namespace PingPong

