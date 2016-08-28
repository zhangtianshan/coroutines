#ifndef INC_COROUTINES_TIMELINE_H_
#define INC_COROUTINES_TIMELINE_H_

#include <cstdint>
#include "list.h"

namespace Coroutines {
  
  typedef uint64_t      TTimeStamp;
  typedef uint64_t      TTimeDelta;

  struct TWatchedEvent;

  TTimeStamp now();
  void updateCurrentTime(TTimeDelta delta_ticks);
  void registerTimeoutEvent(TWatchedEvent* we);
  void unregisterTimeoutEvent(TWatchedEvent* we);

}

#endif

