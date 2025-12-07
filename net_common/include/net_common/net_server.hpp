#pragma once

#include "net_common.hpp"
#include "net_connection.hpp"
#include "net_message.hpp"
#include "tsqueue/tsqueue.hpp"

namespace Net
{

template <typename T>
class ServerBase
{
    const std::string c_log_prefix{"[SERVER]"};

  public:
    ServerBase(uint16_t port)
        : m_asio_acceptor(m_asio_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
    {
    }

    virtual ~ServerBase()
    {
        stop();
    }

    bool start()
    {
        try
        {
            waitForClientConnection();

            m_context_thread = std::thread([this]()
                                           { m_asio_context.run(); });
        }
        catch (const std::exception &e)
        {
            std::cerr << c_log_prefix << " exception: " << e.what() << '\n';
            return false;
        }

        std::cout << c_log_prefix << " started!\n";
        return true;
    }

    void stop()
    {
        m_asio_context.stop();

        if (m_context_thread.joinable())
            m_context_thread.join();

        std::cout << c_log_prefix << " stopped\n";
    }

    // ASYNC
    void waitForClientConnection()
    {
        m_asio_acceptor.async_accept(
            [this](std::error_code ec, boost::asio::ip::tcp::socket socket)
            {
                if (!ec)
                {
                    std::cout << c_log_prefix << " new connection: " << socket.remote_endpoint() << '\n';

                    std::shared_ptr<Connection<T>> new_conn = std::make_shared<Connection<T>>(
                        Connection<T>::EOwner::Server,
                        m_asio_context, std::move(socket), m_messages_in);

                    if (onClientConnect(new_conn))
                    {
                        m_connections.push_back(new_conn);
                        m_connections.back()->connectToClient(*this, m_id_counter++);

                        std::cout << c_log_prefix << " [" << m_connections.back()->getId() << " ] connection approved\n";
                    }
                    else
                    {
                        std::cout << c_log_prefix << " connection denied\n";
                    }
                }
                else
                {
                    std::cout << c_log_prefix << " new connection error: " << ec.message() << '\n';
                }

                waitForClientConnection();
            });
    }

    void messageClient(std::shared_ptr<Connection<T>> client, Message<T> msg)
    {
        if (client && client->isConnected())
        {
            client->send(std::move(msg));
        }
        else
        {
            onClientDisconnect(client);
            client.reset();
            m_connections.erase(
                std::remove(m_connections.begin(), m_connections.end(), client), m_connections.end());
        }
    }

    void messageAllClients(Message<T> msg, std::shared_ptr<Connection<T>> ignore_client = nullptr)
    {
        bool some_clients_disconnected = false;

        for (auto &client : m_connections)
        {
            if (client && client->isConnected() && client != ignore_client)
            {
                client->send(std::move(msg));
            }
            else
            {
                onClientDisconnect(client);
                client.reset();
                some_clients_disconnected = true;
            }
        }

        if (some_clients_disconnected)
            m_connections.erase(std::remove(m_connections.begin(), m_connections.end(), nullptr), m_connections.end());
    }

    void update(bool wait = false, size_t max_messages = std::numeric_limits<size_t>::max())
    {
        if (wait)
            m_messages_in.wait();

        size_t msg_cnt = 0;
        while (msg_cnt < max_messages && !m_messages_in.empty())
        {
            auto msg = m_messages_in.pop_front();
            onMessage(msg.remote, std::move(msg.msg));
            ++msg_cnt;
        }
    }

  public:
    virtual void onClientValidated(std::shared_ptr<Connection<T>> client)
    {
    }

  protected:
    virtual bool onClientConnect(std::shared_ptr<Connection<T>> client)
    {
        return false;
    }

    virtual void onClientDisconnect(std::shared_ptr<Connection<T>> client)
    {
    }

    virtual void onMessage(std::shared_ptr<Connection<T>> client, Message<T> &&msg)
    {
    }

  protected:
    TSQueue<OwnedMessage<T>> m_messages_in;

    std::deque<std::shared_ptr<Connection<T>>> m_connections;

    boost::asio::io_context m_asio_context;
    std::thread m_context_thread;
    boost::asio::ip::tcp::acceptor m_asio_acceptor;

    uint32_t m_id_counter = 10000;
};

} // namespace Net
