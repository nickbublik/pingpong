#pragma once

#include "net_connection.hpp"
#include "net_message.hpp"

namespace Net
{
template <typename T>
class ClientBase
{
  public:
    ClientBase()
    {
    }

    virtual ~ClientBase()
    {
        disconnect();
    }

  public:
    bool connect(const std::string &host, const uint16_t port)
    {
        try
        {
            boost::asio::ip::tcp::resolver resolver(m_context);
            boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));

            m_connection = std::make_unique<Connection<T>>(Connection<T>::EOwner::Client, m_context, boost::asio::ip::tcp::socket(m_context), m_messages_in);
            m_connection->connectToServer(endpoints);
            m_context_thread = std::thread([this]()
                                           { m_context.run(); });
        }
        catch (const std::exception &e)
        {
            std::cerr << "Client exception: " << e.what() << '\n';
            return false;
        }

        return true;
    }

    void disconnect()
    {
        if (isConnected())
        {
            m_connection->disconnect();
        }

        m_context.stop();

        if (m_context_thread.joinable())
            m_context_thread.join();

        m_connection.release();
    }

    bool isConnected() const
    {
        if (m_connection)
            return m_connection->isConnected();
        else
            return false;
    }

    void flush()
    {
        if (isConnected())
            m_connection->waitForSendQueueEmpty();
    }

    size_t getPendingWrites() const
    {
        if (isConnected())
            return m_connection->getPendingWrites();

        return 0;
    }

    void send(const Message<T> &msg)
    {
        if (isConnected())
            m_connection->send(msg);
    }

    void send(const Message<T> &&msg)
    {
        if (isConnected())
            m_connection->send(std::move(msg));
    }

    TSQueue<OwnedMessage<T>> &incoming()
    {
        return m_messages_in;
    }

  protected:
    boost::asio::io_context m_context;
    std::thread m_context_thread;
    std::unique_ptr<Connection<T>> m_connection;

  private:
    TSQueue<OwnedMessage<T>> m_messages_in;
};
} // namespace Net
