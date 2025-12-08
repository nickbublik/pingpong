#pragma once

#include <bitset>
#include <iterator>

#include "net_common.hpp"

namespace Net
{
template <typename T>
struct MessageHeader
{
    T id{};
    uint32_t size = 0;
};

template <typename T>
struct Message
{
    MessageHeader<T> header{};
    std::vector<uint8_t> body;

    size_t size() const
    {
        return body.size();
    }

    friend std::ostream &operator<<(std::ostream &os, const Message<T> &msg)
    {
        os << "id=" << int(msg.header.id) << " size=" << msg.header.size << '\n';

        os << "body:\n";
        for (const uint8_t b : msg.body)
        {
            std::bitset<8> byte_bin(b);
            os << byte_bin << '.';
        }
        os << '\n';

        return os;
    }

    template <typename DataType, typename = std::enable_if_t<std::is_trivially_copyable_v<DataType>>>
    friend Message<T> &operator<<(Message<T> &msg, const DataType &data)
    {
        static_assert(std::is_trivially_copyable_v<DataType>, "Data is to complex to be pushed");

        const size_t offset = msg.body.size();
        msg.body.resize(msg.body.size() + sizeof(DataType));
        std::memcpy(msg.body.data() + offset, &data, sizeof(DataType));

        msg.header.size = msg.size();

        return msg;
    }

    template <typename Container,
              typename Elem = std::remove_reference_t<
                  decltype(*std::data(std::declval<const Container &>()))>,
              std::enable_if_t<!std::is_trivially_copyable_v<Container> &&
                                   std::is_trivially_copyable_v<Elem>,
                               int> = 0>
    friend Message<T> &operator<<(Message<T> &msg, const Container &data)
    {
        auto *ptr = std::data(data);
        const auto count = std::size(data);

        static_assert(std::is_trivially_copyable_v<Elem>, "Data is to complex to be pushed");

        if (count == 0)
        {
            return msg;
        }

        const size_t bytes_to_copy = count * sizeof(Elem);
        const size_t offset = msg.body.size();

        msg.body.resize(msg.body.size() + bytes_to_copy);

        std::memcpy(msg.body.data() + offset, reinterpret_cast<const uint8_t *>(ptr), bytes_to_copy);

        msg.header.size = msg.size();

        return msg;
    }

    template <typename DataType, typename = std::enable_if_t<std::is_trivially_copyable_v<DataType>>>
    friend Message<T> &operator>>(Message<T> &msg, DataType &data)
    {
        static_assert(std::is_trivially_copyable_v<DataType>, "Data is to complex to be assigned to");

        const size_t offset = msg.body.size() - sizeof(DataType);
        std::memcpy(&data, msg.body.data() + offset, sizeof(DataType));

        msg.body.resize(offset);
        msg.header.size = msg.size();

        return msg;
    }

    template <typename Container,
              typename Elem = std::remove_reference_t<
                  decltype(*std::data(std::declval<const Container &>()))>,
              std::enable_if_t<!std::is_trivially_copyable_v<Container> &&
                                   std::is_trivially_copyable_v<Elem>,
                               int> = 0>
    friend Message<T> &operator>>(Message<T> &msg, Container &data)
    {
        auto *ptr = std::data(data);
        const auto count = std::size(data);

        static_assert(std::is_trivially_copyable_v<Elem>, "Data is to complex to be assigned to");

        if (count == 0)
        {
            return msg;
        }

        const size_t bytes_to_copy = std::min(count * sizeof(Elem), msg.body.size());
        const size_t offset = msg.body.size() - bytes_to_copy;

        std::memcpy(ptr, msg.body.data() + offset, bytes_to_copy);

        msg.body.resize(offset);
        msg.header.size = msg.size();

        return msg;
    }
};

template <typename T>
class Connection;

template <typename T>
struct OwnedMessage
{
    std::shared_ptr<Connection<T>> remote = nullptr;
    Message<T> msg;

    friend std::ostream &operator<<(std::ostream &os, const OwnedMessage<T> &msg)
    {
        os << msg.msg;
        return os;
    }
};
} // namespace Net
