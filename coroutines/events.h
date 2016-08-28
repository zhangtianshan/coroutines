#ifndef INC_COROUTINES_EVENTS_H_
#define INC_COROUTINES_EVENTS_H_

#include "channel.h"

namespace Coroutines {
  
  enum eEventType {
    EVT_USER_EVENT = 0
  , EVT_CHANNEL_CAN_PUSH
  , EVT_CHANNEL_CAN_PULL
  , EVT_TIMEOUT
  , EVT_COROUTINE_ENDS
  , EVT_INVALID
  , EVT_TYPES_COUNT
  };

  // --------------------------
  class TChannel;
  struct TWatchedEvent : public TListItem {
    THandle        owner;         // maps to current()
    eEventType     event_type;    // Set by the ctor

    union {
      
      struct {
        TChannel*  channel;
        void*      data_addr;
        size_t     data_size;
      } channel;

      struct {
        TTimeStamp time_programmed;    // Timestamp when it was programmed
        TTimeStamp time_to_trigger;    // Timestamp when will fire
      } time;

      struct {
        THandle    handle;
      } coroutine;
    
    };

    // Specialized ctors
    TWatchedEvent() : event_type(EVT_INVALID) { }

    // Wait until the we can push/pull an item into/from that channel
    template< class TObj >
    TWatchedEvent(TChannel* new_channel, const TObj &obj, eEventType evt)
    {
      channel.channel = new_channel;
      channel.data_addr = (TObj*) &obj;
      channel.data_size = sizeof(TObj);
      event_type = evt;
      owner = current();
    }

    // Wait until the coroutine has finished
    TWatchedEvent(THandle handle_to_wait)
    {
      coroutine.handle = handle_to_wait;
      event_type = EVT_COROUTINE_ENDS;
      owner = current();
    }

    TWatchedEvent(TTimeDelta timeout) {
      event_type = EVT_TIMEOUT;
      time.time_programmed = now();
      time.time_to_trigger= now() + timeout;
      owner = current();
    }

  };

  // -----------------------------------------------------
  template< typename TObj >
  bool pull(TChannel* ch, TObj& obj) {
    assert(ch);
    assert(&obj);
    while (ch->empty() && !ch->closed()) {
      TWatchedEvent evt(ch, obj, EVT_CHANNEL_CAN_PULL);
      wait(&evt, 1);
    }

    if (ch->closed() && ch->empty())
      return false;
    ch->pull(&obj, sizeof(obj));
    return true;
  }

  template< typename TObj >
  bool push(TChannel* ch, const TObj& obj) {
    assert(ch);
    assert(&obj);
    while (ch->full() && !ch->closed()) {
      TWatchedEvent evt(ch, obj, EVT_CHANNEL_CAN_PUSH);
      wait(&evt, 1);
    }
    if (ch->closed())
      return false;
    ch->push(&obj, sizeof(obj));
    return true;
  }

}



#endif
