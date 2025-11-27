#pragma once

#include <cstdint>
#include <string>

#include "net_common/net_message.hpp"

namespace PingPong
{
namespace Common
{

enum class EMessageType : uint32_t
{
    // Server replies
    Accept,
    Reject,
    Abort,
    // File transmission control
    Send,
    RequestReceive,
    Receive,
    // File transmission process
    Chunk,
    FinalChunk
};

enum class EPayloadType : uint8_t
{
    File
};

struct SendRequest
{
    EPayloadType payload_type;
    uint8_t code_size;
    std::string code;
};

} // namespace Common
} // namespace PingPong

namespace Net
{
// serialize
template <typename T>
Message<T> &operator<<(Message<T> &msg, const PingPong::Common::SendRequest &data)
{
    msg << data.code << data.code_size << data.payload_type;
    return msg;
}

// deserialize (note: reverse order)
template <typename T>
Message<T> &operator>>(Message<T> &msg, PingPong::Common::SendRequest &data)
{
    msg >> data.payload_type >> data.code_size;
    data.code.resize(data.code_size);
    msg >> data.code;
    return msg;
}

} // namespace Net
