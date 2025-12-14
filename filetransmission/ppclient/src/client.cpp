#include "client.hpp"
#include "ssl_tofu.hpp"

namespace PingPong
{
using tcp = boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;

FileClient::FileClient()
{
}

void FileClient::waitForIncomingQueueMessage(const std::chrono::milliseconds &check_period)
{
    m_connection->waitForIncomingQueueMessage(check_period);
}

bool FileClient::autoConnect(uint16_t discovery_port,
                             std::chrono::milliseconds timeout,
                             std::chrono::milliseconds polling_delay)
{
    DBG_LOG(" discovery_port = ", discovery_port);
    std::optional<DiscoveredServer> res = discoverServerByUnicastBruteforce(m_context, discovery_port, timeout, polling_delay);

    if (res)
    {
        DBG_LOG("Connecting to ", res->address, ":", res->port);
        std::string server_id = {"PingPongServer[" + res->address + ":" + std::to_string(res->port) + "]"};
        std::optional<ssl::stream<tcp::socket>> ssl_socket_opt = connectTLSWithTofu(m_context, res->address, res->port, server_id);

        if (!ssl_socket_opt)
        {
            std::cerr << "Failed to get ssl_socket. Aborting.\n";
            return false;
        }

        return connect(res->address, res->port, *std::move(ssl_socket_opt));
    }

    DBG_LOG("Discovery failed, trying localhost fallback...");
    const std::string fallback_addr = "127.0.0.1";
    const uint16_t fallback_port = 60010;
    std::string server_id = {"PingPongServer[" + fallback_addr + ":" + std::to_string(fallback_port) + "]"};
    std::optional<ssl::stream<tcp::socket>> ssl_socket_opt = connectTLSWithTofu(m_context, fallback_addr, fallback_port, server_id);

    if (!ssl_socket_opt)
    {
        std::cerr << "Failed to get ssl_socket. Aborting.\n";
        return false;
    }
    return connect(fallback_addr, fallback_port, *std::move(ssl_socket_opt));
}

ssl::context FileClient::createSSLContext()
{
    ssl::context context(ssl::context::tls_client);
    // clang-format off
        context.set_options(
              ssl::context::default_workarounds
            | ssl::context::no_sslv2
            | ssl::context::no_sslv3);
    // clang-format on
    return context;
}

void FileClient::setFingerprintVerifier(ssl::stream<tcp::socket> &ssl_socket, const std::string &expected_fingerprint)
{
    ssl_socket.set_verify_mode(ssl::verify_peer);
    ssl_socket.set_verify_callback(
        [expected_fingerprint](bool, ssl::verify_context &verify_context)
        {
            int depth = X509_STORE_CTX_get_error_depth(verify_context.native_handle());
            if (depth != 0)
                return true;

            X509 *cert = X509_STORE_CTX_get_current_cert(verify_context.native_handle());

            if (!cert)
                return false;

            unsigned char digest[EVP_MAX_MD_SIZE];
            unsigned int digest_length = 0;

            if (!X509_digest(cert, EVP_sha256(), digest, &digest_length))
                return false;

            std::stringstream ss;
            for (auto i = 0; i < digest_length; ++i)
            {
                if (i)
                    ss << ':';

                ss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
            }

            DBG_LOG("ssl_socket fingerprint = ", ss.str());
            DBG_LOG("expected   fingerprint = ", expected_fingerprint);
            return ss.str() == expected_fingerprint;
        });
}

TofuDecision FileClient::tofuPrompt(const std::string &server_id, const std::string &fingerprint)
{
    std::cout << "\n[TOFU] First time seeing server: " << server_id << "\n"
              << "Fingerprint (SHA-256): " << fingerprint << "\n"
              << "Short code: " << getShortenedCode(fingerprint) << "\n"
              << "Trust this server? (y/N): " << std::endl;

    char ans = 'N';
    std::cin >> ans;
    bool trust = ans == 'y' || ans == 'Y';
    return {trust, fingerprint};
}

std::optional<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> FileClient::connectTLSWithTofu(
    boost::asio::io_context &io_context,
    const std::string &host,
    uint16_t port,
    const std::string &server_id)
{
    auto known_path = defaultKnownHostsPath();
    auto known = loadKnownHosts(known_path);

    boost::system::error_code ec;

    tcp::resolver resolver(io_context);

    // Phase 1: probe (no verify), read fingerprint
    {
        ssl::context probe_context = createSSLContext();
        ssl::stream<tcp::socket> probe_stream(io_context, probe_context);

        auto endpoint = resolver.resolve(host, std::to_string(port), ec);
        if (ec)
            return std::nullopt;

        boost::asio::connect(probe_stream.next_layer(), endpoint, ec);
        if (ec)
            return std::nullopt;

        probe_stream.set_verify_mode(ssl::verify_none);
        std::ignore = probe_stream.handshake(ssl::stream_base::client, ec);
        if (ec)
            return std::nullopt;

        std::string fingerprint = getSHA256CertFingerprintFromSSLStream(probe_stream);
        if (fingerprint.empty())
            return std::nullopt;

        probe_stream.next_layer().close();

        // ---- Decide trust
        auto it = known.find(server_id);

        if (it == known.end())
        {
            auto decision = tofuPrompt(server_id, fingerprint);
            if (!decision.trusted)
            {
                std::cerr << "[TOFU] Not trusted. Aborting.\n";
                return std::nullopt;
            }

            known[server_id] = fingerprint;
            saveKnownHosts(known_path, known);
            DBG_LOG("[TOFU] Saved trust for ", server_id);
        }
        else
        {
            if (it->second != fingerprint)
            {
                std::cerr << "[TOFU] WARNING: server fingerprint CHANGED!\n"
                          << "Known: " << it->second << "\n"
                          << "Now:   " << fingerprint << "\n"
                          << "Refusing to connect.\n";
                return std::nullopt;
            }
        }
    }

    // Phase 2: real connection (enforce fingerprint during handshake)
    ssl::context real_context = createSSLContext();
    ssl::stream<tcp::socket> real_stream(io_context, real_context);

    auto endpoint = resolver.resolve(host, std::to_string(port), ec);
    if (ec)
        return std::nullopt;

    boost::asio::connect(real_stream.next_layer(), endpoint, ec);
    if (ec)
        return std::nullopt;

    setFingerprintVerifier(real_stream, known[server_id]);

    std::ignore = real_stream.handshake(ssl::stream_base::client, ec);
    if (ec)
    {
        std::cerr << "TLS handshake failed: " << ec.message() << "\n";
        return std::nullopt;
    }

    return std::optional<ssl::stream<tcp::socket>>(std::move(real_stream));
}

} // namespace PingPong
