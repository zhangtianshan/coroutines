#include "coroutines.h"
#include "fcontext/fcontext.h"
#include <vector>

#pragma comment(lib, "fcontext.lib")

namespace Coroutines {

  static const uint16_t INVALID_ID = 0xffff;

  namespace internal {

    // -----------------------------------------
    THandle h_current;

    // -----------------------------------------
    struct TCoro {

      enum eState {
        UNINITIALIZED
      , RUNNING
      , WAITING_FOR_CONDITION
      , WAITING_FOR_EVENT
      , FREE
      };

      eState                    state = UNINITIALIZED;
      THandle                   this_handle;
      //uint16_t                  prev_id = INVALID_ID;            // Only used when active
      //uint16_t                  next_id = 0;

      // Wait
      TWaitConditionFn          must_wait;
      TList                     waiting_for_me;
      TWatchedEvent*            event_waking_me_up = nullptr;      // Which event took us from the WAITING_FOR_EVENT

      // User entry point 
      TBootFn                   boot_fn = nullptr;
      void*                     boot_fn_arg = nullptr;

      // Low Level 
      fcontext_transfer_t       ip;
      fcontext_stack_t          stack;
      fcontext_transfer_t       caller_ctx;

      void runUserFn() {
        assert(boot_fn);
        assert(boot_fn_arg);
        (*boot_fn)(boot_fn_arg);

        // epilogue
        state = FREE;
        this_handle.age++;
      }

      static void ctxEntryFn(fcontext_transfer_t t) {
        TCoro* co = reinterpret_cast<TCoro*>(t.data);
        assert(co);
        co->caller_ctx = t;
        co->runUserFn();
        co->wakeOthersWaitingForMe();
        co->returnToCaller();
      }

      static const size_t default_stack_size = 16 * 1024;
      void createStack() {
        stack = ::create_fcontext_stack(default_stack_size);
      }

      void destroyStack() {
        destroy_fcontext_stack(&stack);
      }

      // This has not yet started, it's only ready to run in the stack provided
      void resetIP() {
        ip.ctx = make_fcontext( stack.sptr, stack.ssize, &ctxEntryFn );
        ip.data = this;
      }

      void resume() {
        h_current = this_handle;
        ip = jump_fcontext(ip.ctx, ip.data);
      }

      void returnToCaller() {
        h_current = THandle();
        caller_ctx = jump_fcontext(caller_ctx.ctx, caller_ctx.data);
      }

      void wakeOthersWaitingForMe() {
        // Wake up those coroutines that were waiting for me to finish
        while (true) {
          auto we = waiting_for_me.detachFirst< TWatchedEvent >();
          if (!we)
            break;
          wakeUp(we);
        }
      }

    };

    std::vector< TCoro > coros;
    //uint16_t             first_free = 0;
    //uint16_t             last_free = 0;
    //uint16_t             first_in_use = INVALID_ID;
    //uint16_t             last_in_use = INVALID_ID;

    // ----------------------------------------------------------
    TCoro* byHandle(THandle h) {

      // id must be in the valid range
      if (h.id >= coros.size())
        return nullptr;

      // if what we found matches the current age, we are a valid co
      TCoro* c = &coros[h.id];
      assert(c->this_handle.id == h.id);
      if (h.age != c->this_handle.age)
        return nullptr;
      return c;
    }

    // ----------------------------------------------------------
    TCoro* findFree() {

      // Right now, search linearly for a free one
      for (auto& co : coros) {
        if (co.state != TCoro::FREE && co.state != TCoro::UNINITIALIZED)
          continue;
        co.state = TCoro::RUNNING;
        return &co;
      }

      return nullptr;
    }

    // ----------------------------------------------------------
    void initialize() {

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

        co.createStack();

        ++idx;
      }
      //first_free = 0;
      //last_free = idx - 1;
      //dump("OnBoot");

      //auto co_main = findFree();
      //assert(co_main);
      //co_main->initAsMain();
      //h_main = co_main->this_handle;
      //h_current = h_main;
    }

    // --------------------------
    THandle start(TBootFn boot_fn, void* boot_fn_arg) {

      if (coros.empty())
        initialize();

      TCoro* co_new = findFree();
      assert(co_new);                               // Run out of free coroutines slots
      assert(co_new->state == TCoro::RUNNING);

      // Save arguments
      co_new->boot_fn = boot_fn;
      co_new->boot_fn_arg = boot_fn_arg;
      co_new->resetIP();

      // Can't start the new co from another co. Just register it
      // and the main thread will take care of starting it when possible
      if( !isHandle( current() ))
        co_new->resume();

      return co_new->this_handle;
    }

    // ----------------------------------------------------------
    void dump(const char* title) {
      //printf("Dump FirstFree: %d LastFree:%d FirstInUse:%d - LastInUse:%d %s\n", first_free, last_free, first_in_use, last_in_use, title);
      printf("Dump %s\n", title);
      int idx = 0;
      for (auto& co : coros) {
        printf("%04x : state:%d\n", idx, co.state);
        ++idx;
      }
    }

    // --------------------------------------------
    struct TScheduler {
      int nactives = 0;
      int next_idx = 0;
      int runActives();
      bool runNextReady();
    };
    TScheduler scheduler;

  }

  // --------------------------
  THandle current() {
    return internal::h_current;
  }

  // --------------------------------------------
  bool isHandle(THandle h) {
    return internal::byHandle(h) != nullptr;
  }

  // --------------------------------------------
  void yield() {
    assert(isHandle( current() ));
    auto co = internal::byHandle(current());
    assert(co);
    co->returnToCaller();
  }


  // -------------------------------
  // WAIT --------------------------
  // -------------------------------
  
  // --------------------------
  void wait(TWaitConditionFn fn) {
    // If the condition does not apply now, don't wait
    if (!fn())
      return;
    // We must be a valid co
    auto co = internal::byHandle(current());
    assert(co);
    co->state = internal::TCoro::WAITING_FOR_CONDITION;
    co->must_wait = fn;
    yield();
  }

  // Wait for another coroutine to finish
  // wait while h is a coroutine handle
  void wait(THandle h) {
    TWatchedEvent we(h);
    wait(&we, 1);
  }

  // Wait until all coroutines have finished
  void waitAll(std::initializer_list<THandle> handles) {
    waitAll(handles.begin(), handles.end());
  }

  // --------------------------------------------------------------
  int wait(TWatchedEvent* watched_events, int nwatched_events, TTimeDelta timeout) {
    assert(isHandle(current()));

    int n = nwatched_events;
    auto we = watched_events;

    // Check if any of the wait conditions are false, so there is no need to enter in the wait
    // for event mode
    int idx = 0;
    while (idx < n) {

      switch (we->event_type) {

      case EVT_CHANNEL_CAN_PULL:
        if (!we->channel.channel->empty() || we->channel.channel->closed())
          return idx;
        break;

      case EVT_CHANNEL_CAN_PUSH:
        if (!we->channel.channel->full() && !we->channel.channel->closed())
          return idx;
        break;

      case EVT_COROUTINE_ENDS: {
        if (!isHandle(we->coroutine.handle))
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
      else
      if (we->event_type == EVT_CHANNEL_CAN_PUSH)
        we->channel.channel->waiting_for_push.append(we);
      else
      if (we->event_type == EVT_COROUTINE_ENDS) {
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

    // After we wakeup, we should be ready to go

    // There should be a reason to exit the waiting_for_event
    assert(co->event_waking_me_up != nullptr);
    int event_idx = 0;

    // If we had programmed a timeout, remove it
    if (timeout != no_timeout) {
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
      else 
      if (we->event_type == EVT_COROUTINE_ENDS) {
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
  // Try to wake up all the coroutines which were waiting for the event
  void wakeUp(TWatchedEvent* we) {
    assert(we);
    auto co = internal::byHandle(we->owner);
    if (co) {
      co->event_waking_me_up = we;
      co->state = internal::TCoro::RUNNING;
    }
  }


















  // ----------------------------------------------------------
  int executeActives() {
    return internal::scheduler.runActives();
  }

  // --------------------------------------------
  int internal::TScheduler::runActives() {
    nactives = 0;
    next_idx = 0;
    while (next_idx < coros.size()) {
      if (runNextReady())
        ++nactives;
    }
    return nactives;
  }

  bool internal::TScheduler::runNextReady() {
    // Find one ready
    while (next_idx < coros.size()) {

      auto& co = coros[next_idx];
      ++next_idx;

      if (co.state == TCoro::FREE || co.state == TCoro::UNINITIALIZED)
        continue;

      if (co.state == TCoro::WAITING_FOR_EVENT)
        return true;

      if (co.state == TCoro::WAITING_FOR_CONDITION) {
        if (co.must_wait())
          return true;
        co.state = TCoro::RUNNING;
      }

      assert(co.state == internal::TCoro::RUNNING);
      co.resume();

      return true;
    }
    return false;
  }

}