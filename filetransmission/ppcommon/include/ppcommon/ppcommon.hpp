#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <openssl/sha.h>

#include "net_common/net_message.hpp"

namespace PingPong
{
namespace Common
{

enum class EPayloadType : uint8_t
{
    File
};

struct CodePhrase
{
    uint8_t code_size;
    std::string code;
};

struct FileData
{
    uint64_t file_size;
    uint8_t file_name_size;
    std::string file_name;
};

struct PreMetadata
{
    EPayloadType payload_type;
    CodePhrase code_phrase;
    FileData file_data;
};

struct PostMetadata
{
    EPayloadType payload_type;
    uint64_t max_chunk_size;
    CodePhrase code_phrase;
    FileData file_data;
};

struct Empty
{
};

using Buffer = std::vector<uint8_t>;
using Hash = std::array<uint8_t, SHA256_DIGEST_LENGTH>;

struct ChunkData
{
    Hash hash;
    Buffer data;
};

} // namespace Common
} // namespace PingPong

namespace Net
{

template <typename T>
Message<T> &operator<<(Message<T> &msg, const PingPong::Common::CodePhrase &data)
{
    msg << data.code << data.code_size;
    return msg;
}

template <typename T>
Message<T> &operator>>(Message<T> &msg, PingPong::Common::CodePhrase &data)
{
    msg >> data.code_size;
    data.code.resize(data.code_size);
    msg >> data.code;
    return msg;
}

template <typename T>
Message<T> &operator<<(Message<T> &msg, const PingPong::Common::FileData &data)
{
    msg << data.file_name << data.file_name_size << data.file_size;
    return msg;
}

template <typename T>
Message<T> &operator>>(Message<T> &msg, PingPong::Common::FileData &data)
{
    msg >> data.file_size >> data.file_name_size;
    data.file_name.resize(data.file_name_size);
    msg >> data.file_name;
    return msg;
}

template <typename T>
Message<T> &operator<<(Message<T> &msg, const PingPong::Common::PreMetadata &data)
{
    msg << data.file_data << data.code_phrase << data.payload_type;
    return msg;
}

template <typename T>
Message<T> &operator>>(Message<T> &msg, PingPong::Common::PreMetadata &data)
{
    msg >> data.payload_type >> data.code_phrase >> data.file_data;
    return msg;
}

template <typename T>
Message<T> &operator<<(Message<T> &msg, const PingPong::Common::PostMetadata &data)
{
    msg << data.file_data << data.code_phrase << data.max_chunk_size << data.payload_type;
    return msg;
}

template <typename T>
Message<T> &operator>>(Message<T> &msg, PingPong::Common::PostMetadata &data)
{
    msg >> data.payload_type >> data.max_chunk_size >> data.code_phrase >> data.file_data;
    return msg;
}

template <typename T>
Message<T> &operator<<(Message<T> &msg, const PingPong::Common::ChunkData &data)
{
    msg << data.data << data.hash;
    return msg;
}

template <typename T>
Message<T> &operator>>(Message<T> &msg, PingPong::Common::ChunkData &data)
{
    msg >> data.hash;
    data.data.resize(msg.size());
    msg >> data.data;
    return msg;
}
} // namespace Net

namespace PingPong
{
namespace Common
{
enum class EMessageType : uint32_t
{
    // Server replies
    Accept = 0,
    Reject = 1,
    // Server and Client replies
    Success = 2,
    Abort = 3,
    // File transmission control
    Send = 4,
    RequestReceive = 5,
    FinishReceive = 6,
    FailedReceive = 7,
    Receive = 8,
    // File transmission process
    Chunk = 9,
    FinalChunk = 10
};

template <EMessageType M>
struct Payload;

template <>
struct Payload<EMessageType::Accept>
{
    using Type = PostMetadata;
};

template <>
struct Payload<EMessageType::Reject>
{
    using Type = Empty;
};

template <>
struct Payload<EMessageType::Success>
{
    using Type = Empty;
};

template <>
struct Payload<EMessageType::Abort>
{
    using Type = Empty;
};

template <>
struct Payload<EMessageType::Send>
{
    using Type = PreMetadata;
};

template <>
struct Payload<EMessageType::RequestReceive>
{
    using Type = PreMetadata;
};

template <>
struct Payload<EMessageType::FinishReceive>
{
    using Type = Empty;
};

template <>
struct Payload<EMessageType::FailedReceive>
{
    using Type = Empty;
};

template <>
struct Payload<EMessageType::Receive>
{
    using Type = CodePhrase;
};

template <>
struct Payload<EMessageType::Chunk>
{
    using Type = ChunkData;
};

template <>
struct Payload<EMessageType::FinalChunk>
{
    using Type = Empty;
};

using Message = Net::Message<Common::EMessageType>;

template <EMessageType M>
Message encode(const typename Payload<M>::Type &data)
{
    Message msg;
    msg.header.id = M;
    msg << data;
    return msg;
}

template <EMessageType M>
typename Payload<M>::Type decode(Message &msg)
{
    typename Payload<M>::Type data;
    msg >> data;
    return data;
}

} // namespace Common
} // namespace PingPong
