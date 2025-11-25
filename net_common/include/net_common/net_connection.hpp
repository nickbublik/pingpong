#pragma once

#include "net_message.hpp"

namespace Net
{
template <typename T>
class Connection : public std::enable_shared_from_this<Connection<T>>
{
  public:
    enum class EOwner
    {
        Server,
        Client
    };

  public:
    Connection(EOwner parent, boost::asio::io_context &context, boost::asio::ip::tcp::socket socket, TSQueue<OwnedMessage<T>> &messages_in)
        : m_asio_context{context}, m_socket{std::move(socket)}, m_messages_in{messages_in}, m_owner_type{parent}
    {
    }

    virtual ~Connection()
    {
    }

    uint32_t getId() const
    {
        return m_id;
    }

  public:
    void connectToClient(uint32_t uid)
    {
        if (m_owner_type == EOwner::Server)
        {
            if (m_socket.is_open())
            {
                m_id = uid;
                readHeader();
            }
        }
    }

    void connectToServer(const boost::asio::ip::tcp::resolver::results_type &endpoints)
    {
        if (m_owner_type == EOwner::Client)
        {
            boost::asio::async_connect(
                m_socket,
                endpoints,
                [this](std::error_code ec, boost::asio::ip::tcp::endpoint endpoint)
                {
                    if (!ec)
                    {
                        readHeader();
                    }
                });
        }
    }

    void disconnect()
    {
        if (isConnected())
        {
            boost::asio::post(m_asio_context, [this]()
                              { m_socket.close(); });
        }
    }

    bool isConnected() const
    {
        return m_socket.is_open();
    }

    void startListening()
    {
    }

  public:
    void send(const Message<T> msg)
    {
        boost::asio::post(m_asio_context, [this, msg = std::move(msg)]()
                          {
                            bool already_writing = !m_messages_out.empty();
                            m_messages_out.push_back(std::move(msg));
                            if (!already_writing)
                                writeHeader(); });
    }

  private:
    // ASYNC
    void writeHeader()
    {
        boost::asio::async_write(m_socket, boost::asio::buffer(&m_messages_out.front().header, sizeof(MessageHeader<T>)),
                                 [this](std::error_code ec, size_t length)
                                 {
                                     if (!ec)
                                     {
                                         if (m_messages_out.front().body.size() > 0)
                                         {
                                             writeBody();
                                         }
                                         else
                                         {
                                             m_messages_out.pop_front();

                                             if (!m_messages_out.empty())
                                             {
                                                 writeHeader();
                                             }
                                         }
                                     }
                                     else
                                     {
                                         m_socket.close();
                                     }
                                 });
    }

    // ASYNC
    void writeBody()
    {
        boost::asio::async_write(m_socket, boost::asio::buffer(m_messages_out.front().body.data(), m_messages_out.front().body.size()),
                                 [this](std::error_code ec, size_t length)
                                 {
                                     if (!ec)
                                     {
                                         m_messages_out.pop_front();

                                         if (!m_messages_out.empty())
                                         {
                                             writeHeader();
                                         }
                                     }
                                     else
                                     {
                                         m_socket.close();
                                     }
                                 });
    }

    // ASYNC
    void readHeader()
    {
        boost::asio::async_read(m_socket, boost::asio::buffer(&m_forming_in_message.header, sizeof(MessageHeader<T>)),
                                [this](std::error_code ec, size_t length)
                                {
                                    if (!ec)
                                    {
                                        if (m_forming_in_message.header.size > 0)
                                        {
                                            m_forming_in_message.body.resize(m_forming_in_message.header.size);
                                            readBody();
                                        }
                                        else
                                        {
                                            addToIncomingMessageQueue();
                                        }
                                    }
                                    else
                                    {
                                        m_socket.close();
                                    }
                                });
    }

    // ASYNC
    void readBody()
    {
        boost::asio::async_read(m_socket, boost::asio::buffer(m_forming_in_message.body.data(), m_forming_in_message.body.size()),
                                [this](std::error_code ec, size_t length)
                                {
                                    if (!ec)
                                    {
                                        addToIncomingMessageQueue();
                                    }
                                    else
                                    {
                                        m_socket.close();
                                    }
                                });
    }

    void addToIncomingMessageQueue()
    {
        if (m_owner_type == EOwner::Server)
            m_messages_in.push_back({this->shared_from_this(), std::move(m_forming_in_message)});
        else
            m_messages_in.push_back({nullptr, std::move(m_forming_in_message)});

        m_forming_in_message = Message<T>{};

        readHeader();
    }

  protected:
    boost::asio::ip::tcp::socket m_socket;
    boost::asio::io_context &m_asio_context;
    TSQueue<Message<T>> m_messages_out;
    TSQueue<OwnedMessage<T>> &m_messages_in;
    Message<T> m_forming_in_message;
    EOwner m_owner_type = EOwner::Server;
    uint32_t m_id = 0;
};
} // namespace Net
