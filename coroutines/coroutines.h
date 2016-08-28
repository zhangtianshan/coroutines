#ifndef INC_COROUTINES_H_
#define INC_COROUTINES_H_

#include <cstdint>
#include <functional>
#include "list.h"
#include "timeline.h"

namespace Coroutines {

  typedef uint64_t      u64;
  typedef uint8_t       u8;

  struct THandle {
    uint16_t id;
    uint16_t age;
    THandle() : id(0), age(0) {}
  };

  typedef std::function<bool(void)> TWaitConditionFn;

  // --------------------------------------------
  bool    isHandle(THandle h);
  THandle current();
  void    yield();
  void    wait(TWaitConditionFn fn);
  int     executeActives();
  void    initialize();

  namespace internal {
    THandle prologue(void (*boot)(void*), void* ctxs);

    template< typename TFn >
    static void bootstrap(void* context) {
      TFn* fn = static_cast<TFn*> (context);
      (*fn)();
      epilogue( );
    }

    void epilogue();
  }

  // --------------------------
  template< typename TFn >
  THandle start(TFn fn) {
    return internal::prologue( &internal::bootstrap<TFn>, &fn);
  }

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

  // WAIT_FOR_EVER means no timeout
  static const TTimeDelta no_timeout = ~((TTimeDelta)0);
  static const int wait_timedout = ~((int)0);
  int wait(TWatchedEvent* watched_events, int nevents_to_watch, TTimeDelta timeout = no_timeout);
  void wakeUp(TWatchedEvent* we);
  void switchTo(THandle h);
  THandle createOne(void* new_fiber);
  void destroyCurrent();

}

#endif

