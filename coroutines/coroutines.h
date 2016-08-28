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

  struct TWatchedEvent;

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
    void epilogue();

    template< typename TFn >
    static void bootstrap(void* context) {
      TFn* fn = static_cast<TFn*> (context);
      (*fn)();
      epilogue( );
    }
  }

  // --------------------------
  template< typename TFn >
  THandle start(TFn fn) {
    return internal::prologue( &internal::bootstrap<TFn>, &fn);
  }

  // WAIT_FOR_EVER means no timeout
  static const TTimeDelta no_timeout = ~((TTimeDelta)0);
  static const int wait_timedout = ~((int)0);
  int wait(TWatchedEvent* watched_events, int nevents_to_watch, TTimeDelta timeout = no_timeout);
  void wakeUp(TWatchedEvent* we);
  void switchTo(THandle h);

}

#include "events.h"

#endif

