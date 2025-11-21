#include <future>
#include <iostream>
#include <thread>

#include <boost/asio.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>

#include "net_common/net_message.hpp"

enum class MsgTypes : uint32_t
{
    Receive,
    Send
};

int main()
{
    Net::Message<MsgTypes> msg;
    msg.header.id = MsgTypes::Receive;

    int a = 1;
    bool b = true;
    float c = 3.141f;

    struct
    {
        float x;
        float y;
    } d[5];

    msg << a << b << c << d;

    {
        int inc_a = 1;
        bool inc_b = true;
        float inc_c = 3.141f;
    }

    msg >> d >> c >> b >> a;

    return 0;
}
