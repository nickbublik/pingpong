#pragma once

#include <cstdint>
#include <vector>

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

enum class EPayloadType
{
    File
};

struct SendRequest
{
    EPayloadType payload_type;
    uint64_t size;
    std::vector<uint8_t> code;
};

} // namespace Common
} // namespace PingPong
