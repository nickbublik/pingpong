#pragma once

#include <cstdint>

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

} // namespace Common
} // namespace PingPong
