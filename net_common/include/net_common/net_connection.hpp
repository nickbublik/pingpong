#pragma once

#include <chrono>

#include "net_message.hpp"
#include "net_server.hpp"

namespace Net
{
template <typename T>
class ServerBase;

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
        if (m_owner_type == EOwner::Server)
        {
            m_handshake_out = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
            m_handshake_crypted = obfuscate(m_handshake_out);
            std::cout << (int)m_owner_type << " Connection ctr. m_handshake_out = " << m_handshake_out << ", m_handshake_crypted = " << m_handshake_crypted << '\n';
        }
    }

    virtual ~Connection()
    {
    }

    uint32_t getId() const
    {
        return m_id;
    }

  public:
    void connectToClient(ServerBase<T> &server, uint32_t uid)
    {
        if (m_owner_type == EOwner::Server && m_socket.is_open())
        {
            m_id = uid;

            writeValidation();
            readValidation(&server);
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
                        // readHeader();
                        readValidation();
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

    bool isValidated() const
    {
        return m_validated.load(std::memory_order_acquire);
    }

    void startListening()
    {
    }

  public:
    bool send(const Message<T> msg)
    {
        if (!m_validated.load(std::memory_order_acquire))
            return false;

        {
            std::lock_guard<std::mutex> lk(m_flush_mutex);
            ++m_pending_writes;
        }

        boost::asio::post(m_asio_context, [this, msg = std::move(msg)]()
                          {
                            bool already_writing = !m_messages_out.empty();
                            m_messages_out.push_back(std::move(msg));

                            if (!already_writing)
                                writeHeader(); });

        return true;
    }

    size_t getPendingWrites() const
    {
        std::lock_guard<std::mutex> lk(m_flush_mutex);
        return m_pending_writes;
    }

    void waitForSendQueueEmpty()
    {
        std::unique_lock<std::mutex> lk(m_flush_mutex);
        m_flush_cv.wait(lk, [this]
                        { return m_pending_writes == 0 && m_messages_out.empty(); });
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

                                             {
                                                 std::lock_guard<std::mutex> lk(m_flush_mutex);
                                                 if (m_pending_writes > 0)
                                                     --m_pending_writes;
                                                 if (m_pending_writes == 0)
                                                     m_flush_cv.notify_all();
                                             }

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

                                         {
                                             std::lock_guard<std::mutex> lk(m_flush_mutex);
                                             if (m_pending_writes > 0)
                                                 --m_pending_writes;
                                             if (m_pending_writes == 0)
                                                 m_flush_cv.notify_all();
                                         }

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
    void writeValidation()
    {
        std::cout << (int)m_owner_type << " writeValidation. m_handshake_out = " << m_handshake_out << '\n';
        boost::asio::async_write(m_socket, boost::asio::buffer(&m_handshake_out, sizeof(uint64_t)),
                                 [this](std::error_code ec, std::size_t length)
                                 {
                                     if (!ec)
                                     {
                                         if (m_owner_type == EOwner::Client)
                                         {
                                             m_validated.store(true, std::memory_order_release);
                                             readHeader();
                                         }
                                     }
                                     else
                                     {
                                         m_socket.close();
                                     }
                                 });
    }

    // ASYNC
    void readValidation(ServerBase<T> *server = nullptr)
    {
        boost::asio::async_read(m_socket, boost::asio::buffer(&m_handshake_in, sizeof(m_handshake_in)),
                                [this, server](std::error_code ec, size_t length)
                                {
                                    std::cout << (int)m_owner_type << " readValidation result lambda. m_handshake_in = " << m_handshake_in << '\n';
                                    if (!ec)
                                    {
                                        if (m_owner_type == EOwner::Server)
                                        {
                                            if (m_handshake_in == m_handshake_crypted)
                                            {
                                                std::cout << "[SERVER]: client validated\n";
                                                m_validated.store(true, std::memory_order_release);
                                                server->onClientValidated(this->shared_from_this());

                                                readHeader();
                                            }
                                            else
                                            {
                                                std::cout << "[SERVER]: client failed to be validated\n";
                                                m_socket.close();
                                            }
                                        }
                                        else if (m_owner_type == EOwner::Client)
                                        {
                                            m_handshake_out = obfuscate(m_handshake_in);
                                            std::cout << (int)m_owner_type << " readValidation result lambda. m_handshake_out = " << m_handshake_out << '\n';
                                            writeValidation();
                                        }
                                    }
                                    else
                                    {
                                        std::cout << "Client disconnected (on readValidation)\n";
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

    uint64_t obfuscate(uint64_t in)
    {
        uint64_t out = in ^ 0xBABA15ACAB0011FF;
        out = (out & 0xC0A0C0A0B0B0B0) >> 4 | (out & 0x0C0A0C0A0B0B0B) << 4;
        return out ^ 0xBABA15FACE1EE788;
    }

  protected:
    boost::asio::ip::tcp::socket m_socket;
    boost::asio::io_context &m_asio_context;
    TSQueue<Message<T>> m_messages_out;
    TSQueue<OwnedMessage<T>> &m_messages_in;
    Message<T> m_forming_in_message;
    EOwner m_owner_type = EOwner::Server;
    uint32_t m_id = 0;

    mutable std::mutex m_flush_mutex;
    std::condition_variable m_flush_cv;
    size_t m_pending_writes = 0;

    std::atomic_bool m_validated{false};

    // Handshake Validation
    uint64_t m_handshake_out = 0;
    uint64_t m_handshake_in = 0;
    uint64_t m_handshake_crypted = 0;
};
} // namespace Net
