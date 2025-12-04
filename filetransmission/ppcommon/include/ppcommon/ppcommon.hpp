#pragma once

#include <cstdint>
#include <string>

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
} // namespace Net

namespace PingPong
{
namespace Common
{
enum class EMessageType : uint32_t
{
    // Server replies
    Accept,
    Reject,
    // Server and Client replies
    Success,
    Abort,
    // File transmission control
    Send,
    RequestReceive,
    FinishReceive,
    FailedReceive,
    Receive,
    // File transmission process
    Chunk,
    FinalChunk
};

using Buffer = std::vector<uint8_t>;

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
    using Type = Buffer;
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
