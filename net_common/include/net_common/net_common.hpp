#pragma once

#include <chrono>
#include <exception>
#include <iostream>
#include <limits.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <tsqueue/tsqueue.hpp>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>

namespace Net
{
using tcp = boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;
} // namespace Net
