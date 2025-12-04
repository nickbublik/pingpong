#include <bitset>
#include <iostream>
#include <limits>
#include <optional>
#include <unordered_map>

#include <boost/bimap.hpp>

#include "net_common/net_server.hpp"
#include "ppcommon/ppcommon.hpp"
#include "ppcommon/session.hpp"

namespace PingPong
{

using Message = Net::Message<Common::EMessageType>;
using ConnectionPtr = std::shared_ptr<Net::Connection<Common::EMessageType>>;
using SessionUPtr = std::unique_ptr<ServerSession>;

struct TransmissionContext
{
    Common::PreMetadata pre_metadata;
    Common::PostMetadata post_metadata;
};

class ClientStorage
{
  public:
    void removePendingSender(ConnectionPtr sender)
    {
        m_pending_phrase_senders.left.erase(sender);
        m_pending_transmissions.erase(sender);
    }

    void addPendingSender(ConnectionPtr sender, const uint64_t max_chunk_size, const Common::PreMetadata &pre_metadata)
    {
        m_pending_phrase_senders.insert({sender, pre_metadata.code_phrase.code});
        m_pending_transmissions.insert({sender,
                                        TransmissionContext{pre_metadata,
                                                            Common::PostMetadata{pre_metadata.payload_type,
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

class FileServer : public Net::ServerBase<Common::EMessageType>
{
  public:
    FileServer(uint16_t port)
        : Net::ServerBase<Common::EMessageType>(port)
    {
    }

    ~FileServer() override = default;

  protected:
    bool onClientConnect(ConnectionPtr client) override
    {
        std::cout << "[" << client->getId() << "] " << __PRETTY_FUNCTION__ << '\n';
        return true;
    }

    void onClientDisconnect(ConnectionPtr client) override
    {
        std::cout << "[" << client->getId() << "] " << __PRETTY_FUNCTION__ << '\n';
        m_storage.removeSession(client);
        m_storage.removePendingSender(client);
    }

    void onSendEstablishment(ConnectionPtr client, Message &&msg)
    {
        std::cout << __PRETTY_FUNCTION__ << '\n';
        m_storage.removePendingSender(client);

        Common::PreMetadata request;
        msg >> request;
        std::cout << "send-request: file_name = " << request.file_data.file_name
                  << " file_size = " << request.file_data.file_size
                  << ", code = " << request.code_phrase.code << '\n';

        if (m_storage.getSessionBySender(client) != nullptr)
        {
            Message outmsg;
            outmsg.header.id = Common::EMessageType::Reject;
            client->send(outmsg);

            m_storage.removeSession(client);
            m_storage.removePendingSender(client);
        }

        std::cout << "[" << client->getId() << "]: new pending sender with code = " << request.code_phrase.code << '\n';
        m_storage.addPendingSender(client, m_max_chunk_size, request);
    }

    void onReceiveEstablishment(ConnectionPtr client, Message &&msg)
    {
        std::cout << __PRETTY_FUNCTION__ << '\n';

        Common::PreMetadata request;
        msg >> request;
        std::cout << "receive-request: code = " << request.code_phrase.code << '\n';

        ConnectionPtr sender = m_storage.getSenderByCode(request.code_phrase.code);

        if (!sender)
        {
            std::cout << "[" << client->getId() << "]: failed to find a valid sender. Code phrase is invalid\n";
            Message outmsg;
            outmsg.header.id = Common::EMessageType::Reject;
            client->send(outmsg);
            return;
        }

        auto opt_context = m_storage.getContextBySender(sender);

        if (!opt_context)
        {
            std::cout << "[" << client->getId() << "]: failed to find sender's context. \n";
            Message outmsg;
            outmsg.header.id = Common::EMessageType::Reject;
            client->send(outmsg);
            return;
        }

        const Common::PostMetadata &response = (*opt_context).post_metadata;

        Message outmsg;
        outmsg.header.id = Common::EMessageType::Accept;
        outmsg << response;

        client->send(outmsg);
    }

    void establishTransmissionSession(ConnectionPtr receiver, Message &&msg)
    {
        std::cout << __PRETTY_FUNCTION__ << '\n';

        Common::CodePhrase request;
        msg >> request;
        std::cout << "establishTransmissionSession: code = " << request.code << '\n';

        ConnectionPtr sender = m_storage.getSenderByCode(request.code);

        if (!sender)
        {
            std::cout << "[" << receiver->getId() << "]: failed to receive file. Something went wrong\n";
            Message outmsg;
            outmsg.header.id = Common::EMessageType::Abort;
            receiver->send(outmsg);
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

        {
            Message outmsg;
            outmsg.header.id = Common::EMessageType::Accept;
            outmsg << (*context).post_metadata;
            sender->send(outmsg);
        }

        std::cout << "Server starts to send files from " << sender->getId() << " to " << receiver->getId() << '\n';
    }

    void finishSession(const Common::CodePhrase &code_phrase)
    {
        std::cout << __PRETTY_FUNCTION__ << " session of phrase = " << code_phrase.code << '\n';
        ConnectionPtr sender = m_storage.getSenderByCode(code_phrase.code);

        if (sender)
        {
            Message outmsg;
            outmsg.header.id = Common::EMessageType::Success;
            std::cout << "Sending Success to the sender\n";
            sender->send(outmsg);

            m_storage.removePendingSender(sender);
            m_storage.removeSession(sender);
        }
        else
        {
            std::cout << __PRETTY_FUNCTION__ << " session was not found\n";
        }
    }

    void removeSessionAbruptly(ConnectionPtr client)
    {
        std::cout << "[" << client->getId() << "]: error in send-session. Aborting\n";

        // client->send(outmsg);
        // ConnectionPtr receiver = m_storage.getReceiverBySender(client);

        // if (receiver)
        //{
        //     receiver->send(outmsg);
        // }

        ServerSession *session = m_storage.getSessionBySender(client);

        if (session)
        {
            Message outmsg;
            outmsg.header.id = Common::EMessageType::Abort;
            session->onMessage(std::move(outmsg));
        }

        m_storage.removePendingSender(client);
        m_storage.removeSession(client);
    }

    void onSessionedMessage(ConnectionPtr client, ServerSession *session, Message &&msg)
    {
        std::cout << __PRETTY_FUNCTION__ << '\n';

        // FinalChunk: Success -> Sender , FinalChunk -> Receiver
        if (msg.header.id == Common::EMessageType::FinalChunk)
        {
            // Message outmsg;
            // outmsg.header.id = Common::EMessageType::Success;
            // client->send(outmsg);
            session->onMessage(std::move(msg));
        }
        // Chunk:
        // good : Chunk -> Receiver
        // bad  : Abort -> Sender, Abort -> Receiver
        else if (msg.header.id == Common::EMessageType::Chunk)
        {
            if (msg.size() > m_max_chunk_size)
            {
                std::cout << "[" << client->getId() << "]: exceeded max chunk size\n";
                removeSessionAbruptly(client);
            }
            else if (!session->onMessage(std::move(msg)))
            {
                std::cout << "[" << client->getId() << "]: message handling went wrong\n";
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
        std::cout << "[" << client->getId() << "] " << __PRETTY_FUNCTION__ << '\n';

        ServerSession *session = m_storage.getSessionBySender(client);

        // If a client has already established a send-session
        if (session)
        {
            onSessionedMessage(client, session, std::move(msg));
            return;
        }

        if (msg.header.id == Common::EMessageType::Send)
        {
            onSendEstablishment(client, std::move(msg));
        }
        else if (msg.header.id == Common::EMessageType::RequestReceive)
        {
            onReceiveEstablishment(client, std::move(msg));
        }
        else if (msg.header.id == Common::EMessageType::Receive)
        {
            establishTransmissionSession(client, std::move(msg));
        }
        else if (msg.header.id == Common::EMessageType::Success)
        {
            std::cout << "on Success message\n";
            std::cout << msg << '\n';
            Common::CodePhrase code_phrase;
            msg >> code_phrase;
            finishSession(code_phrase);
        }
        else
        {
            std::cout << "[" << client->getId() << "]: unexpected message from a client. Type = " << static_cast<int>(msg.header.id) << '\n';
        }
    }

  protected:
    ClientStorage m_storage;
    uint64_t m_max_chunk_size = 512;
};

} // namespace PingPong

int main()
{
    using namespace PingPong;

    FileServer server(60000);
    server.start();

    while (true)
    {
        server.update(true);
    }

    return 0;
}
