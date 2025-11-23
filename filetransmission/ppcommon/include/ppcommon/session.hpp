#pragma once

#include "ppcommon.hpp"

namespace PingPong
{

class Session
{
  public:
    enum class EOwner
    {
        Server,
        Client
    };

    enum class EType
    {
        Sender,
        Receiver
    };

  public:
    Session(EOwner owner, EType type)
    {
    }

  private:
    EOwner m_owner;
    EType m_type;
};

} // namespace PingPong
