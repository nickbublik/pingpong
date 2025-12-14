#pragma once

#include <filesystem>
#include <string>

#include <net_common/net_client.hpp>

#include <logger/logger.hpp>
#include <ppcommon/ppcommon.hpp>

#include "discovery_client.hpp"

namespace PingPong
{
struct TofuDecision
{
    bool trusted = false;
    std::string fingerprint;
};

namespace fs = std::filesystem;

enum class EOperationType
{
    Help,
    Send,
    Receive
};

struct Operation
{
    EOperationType type;
    fs::path filepath;
    std::string receival_code_phrase;
};

using Message = Common::Message;

class FileClient : public Net::ClientBase<Common::EMessageType>
{
  public:
    FileClient();

    ~FileClient() override = default;

    void waitForIncomingQueueMessage(const std::chrono::milliseconds &check_period);

    bool autoConnect(uint16_t discovery_port,
                     std::chrono::milliseconds timeout,
                     std::chrono::milliseconds polling_delay = std::chrono::milliseconds(50));

    boost::asio::ssl::context createSSLContext();

    void setFingerprintVerifier(boost::asio::ssl::stream<boost::asio::ip::tcp::socket> &ssl_socket, const std::string &expected_fingerprint);

    TofuDecision tofuPrompt(const std::string &server_id, const std::string &fingerprint);

    std::optional<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> connectTLSWithTofu(
        boost::asio::io_context &io_context,
        const std::string &host,
        uint16_t port,
        const std::string &server_id);
};

} // namespace PingPong
