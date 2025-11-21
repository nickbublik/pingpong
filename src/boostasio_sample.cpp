#include <future>
#include <iostream>
#include <thread>

#include <boost/asio.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>

void readMoreData(boost::asio::ip::tcp::socket &socket, std::string &buf, std::promise<void> &eof)
{
    socket.async_read_some(boost::asio::buffer(buf.data(), buf.size()),
                           [&](std::error_code ec, std::size_t length)
                           {
                               if (!ec)
                               {
                                   std::cout << "\n!!!! Read " << length << " bytes\n\n";
                                   std::cout.write(buf.data(), length);
                                   readMoreData(socket, buf, eof);
                               }
                               else
                               {
                                   std::cout << "async_read_some returned an error:\n"
                                             << ec.message() << '\n';
                                   eof.set_value();
                               }
                           });
}

int main()
{
    std::cout << "Hello from AsioDraft\n";

    boost::system::error_code ec;

    // Platform specific interface
    boost::asio::io_context context;

    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
    // Create fake work to prevent context leave run() to early
    std::unique_ptr<WorkGuard> work_guard = std::make_unique<WorkGuard>(boost::asio::make_work_guard(context));

    // Separate thread for the context
    std::thread context_thread = std::thread([&context]()
                                             { context.run(); });

    // Address
    // boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("172.66.155.130"), 80);
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("51.38.81.49", ec), 80);
    // boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("example.com"), 80);

    // Socket
    boost::asio::ip::tcp::socket socket(context);

    // Tell socket to try and connect
    std::ignore = socket.connect(endpoint, ec);

    if (!ec)
    {
        std::cout << "Connected\n";
    }
    else
    {
        std::cout << "Failed to connect to address:\n"
                  << ec.message() << '\n';
        return 1;
    }

    std::size_t bufsize{512};
    std::string buf;
    buf.resize(bufsize);

    std::promise<void> read_eof;

    if (socket.is_open())
    {
        readMoreData(socket, buf, read_eof);

        std::string request{"GET /index.html HTTP/1.1\r\n"
                            "Host: example.com\r\n"
                            "Connection: close\r\n\r\n"};

        socket.write_some(boost::asio::buffer(request.data(), request.size()), ec);
    }

    read_eof.get_future().get();

    work_guard.reset();

    if (context_thread.joinable())
    {
        context_thread.join();
    }
}
