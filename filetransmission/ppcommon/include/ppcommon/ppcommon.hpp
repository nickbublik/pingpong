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
    // File transmission control
    Send,
    ReceiveAsk,
    Receive,
    // File transmission process
    Chunk,
    FinalChunk
};

} // namespace Common
} // namespace PingPong
