#include "coroutines.h"
#include "channel.h"
#include "timeline.h"
#define NOMINMAX
#include "api/coro_platform.h"   
#include <vector>

namespace Coroutines {

  namespace internal {

    THandle h_current;
    THandle h_main;

    static const uint16_t INVALID_ID = 0xffff;

    struct TCoro : public TCoroPlatform {

      enum eState {
        UNINITIALIZED
        , RUNNING
        , WAITING
        , WAITING_FOR_EVENT
        , FREE
      };

      eState                    state;
      THandle                   this_handle;
      TWaitConditionFn          must_wait;
      TWatchedEvent*            event_waking_me_up; // Which event took us from the WAITING_FOR_EVENT
      uint16_t                  prev_id;            // Only used when active
      uint16_t                  next_id;
      TList                     waiting_for_me;

      TCoro() : state(UNINITIALIZED), event_waking_me_up(nullptr), prev_id(INVALID_ID), next_id(0) { }
    };

    std::vector< TCoro > coros;
    uint16_t             first_free = 0;
    uint16_t             last_free = 0;
    uint16_t             first_in_use = INVALID_ID;
    uint16_t             last_in_use = INVALID_ID;

    // ----------------------------------------------------------
    TCoro* byHandle(THandle h) {
      if (h.id >= coros.size())
        return nullptr;
      TCoro* c = &coros[h.id];
      assert(c->this_handle.id == h.id);
      if (h.age != c->this_handle.age)
        return nullptr;
      return c;
    }

    // ----------------------------------------------------------
    void dump(const char* title) {
      printf("Dump FirstFree: %d LastFree:%d FirstInUse:%d - LastInUse:%d %s\n", first_free, last_free, first_in_use, last_in_use, title);
      int idx = 0;
      for (auto& co : coros) {
        printf("%04x : prev:%04x next:%04x state:%d\n", idx, co.prev_id, co.next_id, co.state);
        ++idx;
      }
    }

    // ----------------------------------------------------------
    TCoro* findFree() {

      for (auto& co : coros) {
        if (co.state != TCoro::FREE && co.state != TCoro::UNINITIALIZED)
          continue;
        co.state = TCoro::RUNNING;
        return &co;
      }

      return nullptr;

      /*
      //dump("findfree");
      // Find a free slot
      int idx = first_free;
      assert(idx != INVALID_ID);

      auto& co = coros[idx];
      assert(co.state == TCoro::FREE || co.state == TCoro::UNINITIALIZED);
      assert(co.this_handle.id == idx);
      co.state = TCoro::RUNNING;

      // Update the next id to the next in the chain
      first_free = co.next_id;

      // I become the last in the active list... 
      if (last_in_use != INVALID_ID)
        coros[last_in_use].next_id = idx;
      // Maybe I'm the first in the chain of actives
      if (first_in_use == INVALID_ID)
        first_in_use = idx;
      co.prev_id = last_in_use;
      // I'm the last of the active list
      co.next_id = INVALID_ID;
      last_in_use = idx;
      if (first_free != INVALID_ID)
        coros[first_free].prev_id = INVALID_ID;

      return &co;
      */
    }

    // --------------------------
    THandle prologue(void(*boot_fn)(void*), void* context) {

      auto* co_new = findFree();
      assert(co_new);                               // Run out of free coroutines slots
      assert(co_new->state == TCoro::RUNNING);

      auto co_curr = byHandle(current());
      assert(co_curr);

      THandle h_prev_current = h_current;
      h_current = co_new->this_handle;
      co_new->start(boot_fn, context);
      h_current = h_prev_current;
      return co_new->this_handle;
    }

    // ----------------------------------
    // Executed after running the user defined function
    void epilogue() {
      auto co_curr = byHandle(h_current);
      assert(co_curr);
      auto co_main = byHandle(h_main);
      assert(co_main);

      //dump("about to exit");

      // Add myself to the list of coro's to be recycled...
      co_curr->state = TCoro::FREE;
      co_curr->this_handle.age++;

      uint16_t my_id = co_curr->this_handle.id;

      // Remove me from the chain of actives
      if (co_curr->prev_id != INVALID_ID)
        coros[co_curr->prev_id].next_id = co_curr->next_id;
      if (co_curr->next_id != INVALID_ID)
        coros[co_curr->next_id].prev_id = co_curr->prev_id;
      if (my_id == first_in_use)
        first_in_use = co_curr->next_id;

      if (my_id == last_in_use)
        last_in_use = co_curr->prev_id;

      // This becomes the last free
      co_curr->prev_id = last_free;
      co_curr->next_id = INVALID_ID;
      if (last_free != INVALID_ID)
        coros[last_free].next_id = my_id;
      last_free = my_id;

      // Wake up those coroutines that were waiting for me to finish
      while (true) {
        auto we = co_curr->waiting_for_me.detachFirst< TWatchedEvent >();
        if (!we)
          break;
        wakeUp(we);
      }

      //dump("after exit");

      // Return to main coroutine
      co_curr->switchTo(co_main);
    }

  }

  // --------------------------------------------
  bool isHandle(THandle h) {
    return internal::byHandle(h) != nullptr;
  }

  // --------------------------
  THandle current() {
    return internal::h_current;
  }

  // --------------------------
  void yield() {
    auto co_curr = internal::byHandle(current());
    assert(co_curr);

    auto co_main = internal::byHandle(internal::h_main);
    assert(co_main);

    // You can't yield with the main co, or we will not be able
    // to activate other co's to unlock us
    assert(co_curr != co_main);

    // Return control to main co
    co_curr->switchTo(co_main);
  }

  // --------------------------
  void wait(TWaitConditionFn fn) {
    // If the condition does not apply now, don't wait
    if (!fn())
      return;
    auto co = internal::byHandle(current());
    assert(co);
    co->state = internal::TCoro::WAITING;
    co->must_wait = fn;
    yield();
  }

  // ----------------------------------------------------------
  int executeActives() {
    using namespace internal;

    auto co_main = byHandle(h_main);
    assert(co_main);

    int nactives = 0;
    for (auto& co : coros) {
      if (co.is_main)
        continue;
      if (co.state == TCoro::FREE || co.state == TCoro::UNINITIALIZED)
        continue;

      ++nactives;
      if (co.state == TCoro::WAITING_FOR_EVENT)
        continue;

      h_current = co.this_handle;
      co_main->switchTo(&co);
      h_current = h_main;
    }
      /*

    auto co_main = byHandle(h_main);
    assert(co_main);

    int nactives = 0;
    int idx = first_in_use;
    while (idx != INVALID_ID) {
      auto& co = coros[idx];
      idx = co.next_id;
      assert(co.state != TCoro::FREE);
      if (co.is_main)
        continue;

      nactives++;
      if (co.state == TCoro::WAITING_FOR_EVENT)
        continue;
      if (co.state == TCoro::WAITING) {
        if (co.must_wait())
          continue;
        co.state = TCoro::RUNNING;
      }
      assert(co.state == internal::TCoro::RUNNING);

      // asumming we are in the main coroutine!
      h_current = co.this_handle;
      co_main->switchTo(&co);
      h_current = THandle();
    }
    h_current = h_main;
    */

    return nactives;
  }

  // ----------------------------------------------------------
  void initialize() {
    using namespace internal;

    coros.resize(8);
    int idx = 0;
    for (auto& co : coros) {
      co.this_handle.id = idx;
      co.this_handle.age = 1;

      //if (idx > 0)
      //  co.prev_id = idx - 1;
      //else
      //  co.prev_id = INVALID_ID;

      //if (idx != coros.size() - 1)
      //  co.next_id = idx + 1;
      //else
      //  co.next_id = INVALID_ID;

      ++idx;
    }
    first_free = 0;
    last_free = idx - 1;
    //dump("OnBoot");

    auto co_main = findFree();
    assert(co_main);
    co_main->initAsMain();
    h_main = co_main->this_handle;
    h_current = h_main;
  }

  // --------------------------------------------------------------
  int wait(TWatchedEvent* watched_events, int nwatched_events, TTimeDelta timeout) {
    int n = nwatched_events;
    auto we = watched_events;

    // Check if any of the wait conditions are false, so there is no need to enter in the wait
    // for event mode
    int idx = 0;
    while (idx < n) {

      switch(we->event_type) {
      case EVT_CHANNEL_CAN_PULL:
        if (!we->channel.channel->empty() || we->channel.channel->closed())
          return idx;
        break;
      case EVT_CHANNEL_CAN_PUSH:
        if( !we->channel.channel->full() && !we->channel.channel->closed())
          return idx;
        break;
      case EVT_COROUTINE_ENDS: {
        auto co_to_wait = internal::byHandle(we->coroutine.handle);
        if (!co_to_wait)
          return idx;
        break; }
      default:
        break;
      }

      ++idx;
    }

    // Attach to event watchers
    while (n--) {
      if (we->event_type == EVT_CHANNEL_CAN_PULL)
        we->channel.channel->waiting_for_pull.append(we);
      else if (we->event_type == EVT_CHANNEL_CAN_PUSH)
        we->channel.channel->waiting_for_push.append(we);
      else if (we->event_type == EVT_COROUTINE_ENDS) {
        // Check if the handle that we want to wait, still exists
        auto co_to_wait = internal::byHandle(we->coroutine.handle);
        if (co_to_wait) 
          co_to_wait->waiting_for_me.append(we);
      }
      else {
        // Unsupported event type
        assert(false);
      }
      ++we;
    }

    // Do we have to install a timeout event watch?
    TWatchedEvent time_we;
    if (timeout != no_timeout) {
      assert(timeout >= 0);
      time_we = TWatchedEvent(timeout);
      registerTimeoutEvent(&time_we);
    }

    // Put ourselves to sleep
    auto co = internal::byHandle(current());
    assert(co);
    co->state = internal::TCoro::WAITING_FOR_EVENT;
    co->event_waking_me_up = nullptr;
    yield();
    // There should be a reason to exit the waiting_for_event
    assert(co->event_waking_me_up != nullptr);
    int event_idx = 0;

    // If we had programmed a timeout, remove it
    if (timeout != no_timeout ) {
      unregisterTimeoutEvent(&time_we);
      event_idx = wait_timedout;
    }

    // Detach from event watchers
    n = 0; ;
    we = watched_events;
    while (n <  nwatched_events) {
      if (we->event_type == EVT_CHANNEL_CAN_PULL)
        we->channel.channel->waiting_for_pull.detach(we);
      else if (we->event_type == EVT_CHANNEL_CAN_PUSH)
        we->channel.channel->waiting_for_push.detach(we);
      else if (we->event_type == EVT_COROUTINE_ENDS) {
        // The coroutine we were waiting for is already gone, but 
        // we might be waiting for several co's to finish
        auto co_to_wait = internal::byHandle(we->coroutine.handle);
        if (co_to_wait)
          co_to_wait->waiting_for_me.detach(we);
      }
      else {
        // Unsupported event type
        assert(false);
      }
      if (co->event_waking_me_up == we)
        event_idx = n;
      ++we;
      ++n;
    }

    return event_idx;
  }

  // ---------------------------------------------------
  void wakeUp(TWatchedEvent* we) {
    assert(we);
    auto co = internal::byHandle(we->owner);
    if (co) {
      co->event_waking_me_up = we;
      co->state = internal::TCoro::RUNNING;
    }
  }

  // ---------------------------------------------------
  void switchTo(THandle h) {
    auto co = internal::byHandle(h);
    if (co) {
      co->switchTo(co);
    }
  }

  THandle createOne(void* new_fiber) {
    auto co = internal::findFree();
    co->fiber = new_fiber;
    co->state = internal::TCoro::RUNNING;

    auto co_main = internal::byHandle(internal::h_main);
    internal::h_current = co->this_handle;
    co_main->switchTo(co);

    return co->this_handle;
  }

  void destroyCurrent() {
    THandle h_curr = current();
    auto co = internal::byHandle(h_curr);
    assert(co->is_main == false);
    co->state = internal::TCoro::FREE;
    auto co_main = internal::byHandle(internal::h_main);
    co_main->switchTo(co_main);
  }

}
