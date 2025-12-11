#include <bitset>
#include <iostream>
#include <limits>
#include <optional>
#include <unordered_map>

#include <boost/bimap.hpp>

#include "logger/logger.hpp"
#include "net_common/net_server.hpp"
#include "ppcommon/ppcommon.hpp"
#include "ppcommon/session.hpp"

#include "discovery_server.hpp"

namespace PingPong
{

using namespace Common;
using ConnectionPtr = std::shared_ptr<Net::Connection<EMessageType>>;
using SessionUPtr = std::unique_ptr<ServerSession>;

struct TransmissionContext
{
    PreMetadata pre_metadata;
    PostMetadata post_metadata;
};

class ClientStorage
{
  public:
    void removePendingSender(ConnectionPtr sender)
    {
        m_pending_phrase_senders.left.erase(sender);
        m_pending_transmissions.erase(sender);
    }

    void addPendingSender(ConnectionPtr sender, const uint64_t max_chunk_size, const PreMetadata &pre_metadata)
    {
        m_pending_phrase_senders.insert({sender, pre_metadata.code_phrase.code});
        m_pending_transmissions.insert({sender,
                                        TransmissionContext{pre_metadata,
                                                            PostMetadata{pre_metadata.payload_type,
                                                                         max_chunk_size,
                                                                         pre_metadata.code_phrase,
                                                                         pre_metadata.file_data}}});
    }

    std::optional<std::string> getCodeBySender(ConnectionPtr sender) const
    {
        auto it = m_pending_phrase_senders.left.find(sender);
        if (it != m_pending_phrase_senders.left.end())
        {
            return {it->second};
        }

        return {};
    }

    ConnectionPtr getSenderByCode(const std::string &code) const
    {
        auto it = m_pending_phrase_senders.right.find(code);
        if (it != m_pending_phrase_senders.right.end())
        {
            return it->second;
        }

        return nullptr;
    }

    ConnectionPtr getSenderByReceiver(ConnectionPtr receiver) const
    {
        auto it = m_senders_receivers.right.find(receiver);
        if (it != m_senders_receivers.right.end())
        {
            return it->second;
        }

        return nullptr;
    }

    ConnectionPtr getReceiverBySender(ConnectionPtr sender) const
    {
        auto it = m_senders_receivers.left.find(sender);
        if (it != m_senders_receivers.left.end())
        {
            return it->second;
        }

        return nullptr;
    }

    std::optional<TransmissionContext> getContextBySender(ConnectionPtr sender) const
    {
        auto it = m_pending_transmissions.find(sender);
        if (it != m_pending_transmissions.end())
        {
            return {it->second};
        }

        return {};
    }

    ServerSession *getSessionBySender(ConnectionPtr sender) const
    {
        auto it = m_sessions.find(sender);
        if (it != m_sessions.end())
        {
            return it->second.get();
        }

        return nullptr;
    }

    void removeSession(ConnectionPtr sender)
    {
        m_sessions.erase(sender);
        m_senders_receivers.left.erase(sender);
        sender->disconnectAfterFlush();
    }

    void addSession(ConnectionPtr sender, ConnectionPtr receiver, SessionUPtr session)
    {
        m_senders_receivers.insert({sender, receiver});
        m_sessions.insert({sender, std::move(session)});
    }

  private:
    boost::bimap<ConnectionPtr, std::string> m_pending_phrase_senders;              // sender <-> phrase
    std::unordered_map<ConnectionPtr, TransmissionContext> m_pending_transmissions; // sender -> context
    boost::bimap<ConnectionPtr, ConnectionPtr> m_senders_receivers;                 // sender <-> receiver
    std::unordered_map<ConnectionPtr, SessionUPtr> m_sessions;                      // sender -> session
};

class FileServer : public Net::ServerBase<EMessageType>
{
  public:
    FileServer(uint16_t discovery_port, uint16_t port)
        : Net::ServerBase<EMessageType>(port), m_discovery_server(m_asio_context, discovery_port, port)
    {
        DBG_LOG(__PRETTY_FUNCTION__, " discovery_port = ", discovery_port, ", tcp_port = ", port);
    }

    ~FileServer() override = default;

  protected:
    void onClientValidated(ConnectionPtr client) override
    {
        DBG_LOG("[", client->getId(), "] ", __PRETTY_FUNCTION__);
    }

    bool onClientConnect(ConnectionPtr client) override
    {
        DBG_LOG("[", client->getId(), "] ", __PRETTY_FUNCTION__);
        return true;
    }

    void onClientDisconnect(ConnectionPtr client) override
    {
        DBG_LOG("[", client->getId(), "] ", __PRETTY_FUNCTION__);
        m_storage.removeSession(client);
        m_storage.removePendingSender(client);
    }

    void onSendEstablishment(ConnectionPtr client, Message &&msg)
    {
        DBG_LOG(__PRETTY_FUNCTION__);
        m_storage.removePendingSender(client);

        PreMetadata pre = decode<EMessageType::Send>(msg);
        DBG_LOG("send-request: file_name = ", pre.file_data.file_name, " file_size = ", pre.file_data.file_size, ", code = ", pre.code_phrase.code);

        if (m_storage.getSessionBySender(client) != nullptr)
        {
            Message reject_msg = encode<EMessageType::Reject>(Empty{});
            client->send(reject_msg);

            m_storage.removeSession(client);
            m_storage.removePendingSender(client);
        }

        DBG_LOG("[", client->getId(), "]: new pending sender with code = ", pre.code_phrase.code);
        m_storage.addPendingSender(client, m_max_chunk_size, pre);
    }

    void onReceiveEstablishment(ConnectionPtr client, Message &&msg)
    {
        DBG_LOG(__PRETTY_FUNCTION__);

        PreMetadata request;
        msg >> request;
        DBG_LOG("receive-request: code = ", request.code_phrase.code);

        ConnectionPtr sender = m_storage.getSenderByCode(request.code_phrase.code);

        if (!sender)
        {
            DBG_LOG("[", client->getId(), "]: failed to find a valid sender. Code phrase is invalid");
            Message reject_msg = encode<EMessageType::Reject>(Empty{});
            client->send(reject_msg);
            return;
        }

        auto opt_context = m_storage.getContextBySender(sender);

        if (!opt_context)
        {
            DBG_LOG("[", client->getId(), "]: failed to find sender's context.");
            Message reject_msg = encode<EMessageType::Reject>(Empty{});
            client->send(reject_msg);
            return;
        }

        const PostMetadata &response = (*opt_context).post_metadata;
        Message accept_msg = encode<EMessageType::Accept>(response);
        client->send(accept_msg);
    }

    void establishTransmissionSession(ConnectionPtr receiver, Message &&msg)
    {
        DBG_LOG(__PRETTY_FUNCTION__);

        CodePhrase request;
        msg >> request;
        DBG_LOG("establishTransmissionSession: code = ", request.code);

        ConnectionPtr sender = m_storage.getSenderByCode(request.code);

        if (!sender)
        {
            DBG_LOG("[", receiver->getId(), "]: failed to receive file. Something went wrong");
            Message abort_msg = encode<EMessageType::Abort>(Empty{});
            receiver->send(abort_msg);
            return;
        }

        auto context = m_storage.getContextBySender(sender);

        if (!context)
        {
            removeSessionAbruptly(sender);
            return;
        }

        auto session_ptr = std::make_unique<ServerOneToOneRetranslatorSession>(context->pre_metadata.file_data.file_size, m_max_chunk_size, receiver);

        ServerSession &session = *session_ptr;
        m_storage.addSession(sender, receiver, std::move(session_ptr));

        Message accept_msg = encode<EMessageType::Accept>((*context).post_metadata);
        sender->send(accept_msg);

        DBG_LOG("Server starts to send files from ", sender->getId(), " to ", receiver->getId());
    }

    void finishSession(ConnectionPtr receiver)
    {
        ConnectionPtr sender = m_storage.getSenderByReceiver(receiver);

        if (sender)
        {
            DBG_LOG("Sending Success to the sender");
            Message success_msg = encode<EMessageType::Success>(Empty{});
            sender->send(success_msg);

            m_storage.removePendingSender(sender);
            m_storage.removeSession(sender);
        }
        else
        {
            DBG_LOG(__PRETTY_FUNCTION__, " session was not found");
        }
    }

    void removeSessionAbruptly(ConnectionPtr client)
    {
        DBG_LOG("[", client->getId(), "]: error in send-session. Aborting");

        ServerSession *session = m_storage.getSessionBySender(client);

        if (session)
        {
            Message abort_msg = encode<EMessageType::Abort>(Empty{});
            session->onMessage(std::move(abort_msg));
        }

        m_storage.removePendingSender(client);
        m_storage.removeSession(client);
    }

    void onSessionedMessage(ConnectionPtr client, ServerSession *session, Message &&msg)
    {
        DBG_LOG(__PRETTY_FUNCTION__);

        // FinalChunk: Success -> Sender , FinalChunk -> Receiver
        if (msg.header.id == EMessageType::FinalChunk)
        {
            session->onMessage(std::move(msg));
        }
        // Chunk:
        // good : Chunk -> Receiver
        // bad  : Abort -> Sender, Abort -> Receiver
        else if (msg.header.id == EMessageType::Chunk)
        {
            // Checking chunk's size
            const auto offset = SHA256_DIGEST_LENGTH;
            if (msg.size() - offset > m_max_chunk_size)
            {
                DBG_LOG("[", client->getId(), "]: exceeded max chunk size");
                removeSessionAbruptly(client);
            }
            else if (!session->onMessage(std::move(msg)))
            {
                DBG_LOG("[", client->getId(), "]: message handling went wrong");
                removeSessionAbruptly(client);
            }
        }
        // Other: Abort -> Sender, Abort -> Receiver
        else
        {
            removeSessionAbruptly(client);
        }
    }

    void onMessage(ConnectionPtr client, Message &&msg) override
    {
        DBG_LOG("[", client->getId(), "] ", __PRETTY_FUNCTION__);

        ServerSession *session = m_storage.getSessionBySender(client);

        // If a client has already established a send-session
        if (session)
        {
            onSessionedMessage(client, session, std::move(msg));
            return;
        }

        if (msg.header.id == EMessageType::Send)
        {
            onSendEstablishment(client, std::move(msg));
        }
        else if (msg.header.id == EMessageType::RequestReceive)
        {
            onReceiveEstablishment(client, std::move(msg));
        }
        else if (msg.header.id == EMessageType::Receive)
        {
            establishTransmissionSession(client, std::move(msg));
        }
        else if (msg.header.id == EMessageType::FinishReceive)
        {
            DBG_LOG("on FinishReceive message");
            finishSession(client);
        }
        else if (msg.header.id == EMessageType::FailedReceive)
        {
            DBG_LOG("on FailedReceive message");
            DBG_LOG(msg);
            ConnectionPtr sender = m_storage.getSenderByReceiver(client);

            if (sender)
            {
                removeSessionAbruptly(sender);
            }
        }
        else
        {
            DBG_LOG("[", client->getId(), "]: unexpected message from a client. Type = ", static_cast<int>(msg.header.id));
        }
    }

  protected:
    ClientStorage m_storage;
    uint64_t m_max_chunk_size = 512;
    DiscoveryServer m_discovery_server;
};

} // namespace PingPong

int main()
{
    using namespace PingPong;

    FileServer server(60009, 60010);
    server.start();

    while (true)
    {
        server.update(true);
    }

    return 0;
}
