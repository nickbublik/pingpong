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

struct PreMetadata
{
    EPayloadType payload_type;
    uint64_t file_size;
    uint8_t code_size;
    std::string code;
};

struct PostMetadata
{
    EPayloadType payload_type;
    uint32_t max_chunk_size;
};

} // namespace Common
} // namespace PingPong

namespace Net
{
template <typename T>
Message<T> &operator<<(Message<T> &msg, const PingPong::Common::PreMetadata &data)
{
    msg << data.code << data.code_size << data.file_size << data.payload_type;
    return msg;
}

template <typename T>
Message<T> &operator>>(Message<T> &msg, PingPong::Common::PreMetadata &data)
{
    msg >> data.payload_type >> data.file_size >> data.code_size;
    data.code.resize(data.code_size);
    msg >> data.code;
    return msg;
}

template <typename T>
Message<T> &operator<<(Message<T> &msg, const PingPong::Common::PostMetadata &data)
{
    msg << data.max_chunk_size << data.payload_type;
    return msg;
}

template <typename T>
Message<T> &operator>>(Message<T> &msg, PingPong::Common::PostMetadata &data)
{
    msg >> data.payload_type >> data.max_chunk_size;
    return msg;
}
} // namespace Net
