#include <llarp/router_contact.h>
#include <llarp/link_message.hpp>
#include "buffer.hpp"
#include "logger.hpp"
#include "router.hpp"

namespace llarp
{
  InboundMessageHandler::InboundMessageHandler(llarp_router* _router)
      : router(_router)
  {
    reader.user   = this;
    reader.on_key = &OnKey;
  }

  bool
  InboundMessageHandler::OnKey(dict_reader* r, llarp_buffer_t* key)
  {
    InboundMessageHandler* handler =
        static_cast< InboundMessageHandler* >(r->user);
    llarp_buffer_t strbuf;

    // we are reading the first key
    if(handler->firstkey)
    {
      // check for empty dict
      if(!key)
        return false;
      // we are expecting the first key to be 'a'
      if(!llarp_buffer_eq(*key, "a"))
      {
        llarp::Warn(__FILE__, "message has no message type");
        return false;
      }

      if(!bdecode_read_string(r->buffer, &strbuf))
      {
        llarp::Warn(__FILE__, "could not read value of message type");
        return false;
      }
      // bad key size
      if(strbuf.sz != 1)
      {
        llarp::Warn(__FILE__, "bad mesage type size: ", strbuf.sz);
        return false;
      }
      handler->msgtype  = *strbuf.cur;
      handler->firstkey = false;
      return true;
    }
    // check for last element
    if(!key)
      return handler->MessageDone();

    switch(handler->msgtype)
    {
        // link introduce
      case 'i':
        return handler->DecodeLIM(*key, r->buffer);
        // immidate dht
      case 'd':
        return handler->DecodeDHT(*key, r->buffer);
        // relay commit
      case 'c':
        return handler->DecodeLRCM(*key, r->buffer);
        // unknown message type
      default:
        return false;
    }
  }

  bool
  InboundMessageHandler::DecodeLIM(llarp_buffer_t key, llarp_buffer_t* buff)
  {
    if(llarp_buffer_eq(key, "r"))
    {
      if(!llarp_rc_bdecode(from->get_remote_router(from), buff))
      {
        llarp::Warn(__FILE__, "failed to decode RC");
        return false;
      }
      return true;
    }
    else if(llarp_buffer_eq(key, "v"))
    {
      if(!bdecode_read_integer(buff, &proto))
        return false;
      if(proto != LLARP_PROTO_VERSION)
      {
        llarp::Warn(__FILE__, "llarp protocol version missmatch ", proto);
        return false;
      }
      return true;
    }
    else
    {
      llarp::Warn(__FILE__, "invalid LIM key: ", *key.cur);
      return false;
    }
  }

  bool
  InboundMessageHandler::DecodeDHT(llarp_buffer_t key, llarp_buffer_t* buf)
  {
    return false;
  }

  bool
  InboundMessageHandler::DecodeLRCM(llarp_buffer_t key, llarp_buffer_t* buf)
  {
    return false;
  }

  bool
  InboundMessageHandler::MessageDone()
  {
    switch(msgtype)
    {
      case 'c':
        return router->ProcessLRCM(lrcm);
      default:
        return true;
    }
  }

  bool
  InboundMessageHandler::ProcessFrom(llarp_link_session* src,
                                     llarp_buffer_t buf)
  {
    from     = src;
    msgtype  = 0;
    firstkey = true;
    return bdecode_read_dict(&buf, &reader);
  }

  bool
  InboundMessageHandler::FlushReplies()
  {
    bool success = true;
    while(sendq.size())
    {
      auto& msg = sendq.front();
      auto buf  = llarp::Buffer< decltype(msg) >(msg);
      success &= from->sendto(from, buf);
      sendq.pop();
    }
    return success;
  }
}