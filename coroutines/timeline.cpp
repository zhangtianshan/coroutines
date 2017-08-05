#include "timeline.h"
#include "coroutines.h"

namespace Coroutines {
  
  void wakeUp(TWatchedEvent* we);

  TTimeStamp current_timestamp;
  TList      waiting_for_timeouts;

  TTimeStamp now() {
    return current_timestamp;
  }

  void resetTimer() {
    current_timestamp = 0;
  }

  void updateCurrentTime(TTimeDelta delta_ticks) {
    current_timestamp += delta_ticks;
    auto we = static_cast<TWatchedEvent*>( waiting_for_timeouts.first );
    while (we) {
      assert(we->event_type == EVT_TIMEOUT);
      if (we->time.time_to_trigger <= current_timestamp)
        wakeUp( we );
      we = static_cast<TWatchedEvent*>(we->next);
    }
  }

  void registerTimeoutEvent(TWatchedEvent* we) {
    assert(we->event_type == EVT_TIMEOUT);
    waiting_for_timeouts.append(we);
  }

  void unregisterTimeoutEvent(TWatchedEvent* we) {
    assert(we->event_type == EVT_TIMEOUT);
    waiting_for_timeouts.detach(we);
  }

}
