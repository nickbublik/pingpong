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

            m_connection = std::make_shared<Connection<T>>(Connection<T>::EOwner::Client, m_context, boost::asio::ip::tcp::socket(m_context), m_messages_in);
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
        if (!m_connection && !m_context_thread.joinable())
            return;

        // Only call through if connection exists
        if (m_connection && m_connection->isConnected())
        {
            m_connection->disconnect();
        }

        m_context.stop();

        if (m_context_thread.joinable())
            m_context_thread.join();

        m_connection.reset();
    }

    bool isConnected() const
    {
        if (!m_connection)
            return false;

        return m_connection->isConnected();
    }

    bool isValidated() const
    {
        return m_connection->isValidated();
    }

    void flush()
    {
        if (isConnected())
            m_connection->waitForOutgoingQueueEmpty();
    }

    size_t getPendingWrites() const
    {
        if (isConnected())
            return m_connection->getPendingWrites();

        return 0;
    }

    bool send(const Message<T> &msg)
    {
        if (isConnected())
            return m_connection->send(msg);
        else
            return false;
    }

    bool send(const Message<T> &&msg)
    {
        if (isConnected())
            return m_connection->send(std::move(msg));
        else
            return false;
    }

    TSQueue<OwnedMessage<T>> &incoming()
    {
        return m_messages_in;
    }

  protected:
    boost::asio::io_context m_context;
    std::thread m_context_thread;
    std::shared_ptr<Connection<T>> m_connection;

  private:
    TSQueue<OwnedMessage<T>> m_messages_in;
};
} // namespace Net
