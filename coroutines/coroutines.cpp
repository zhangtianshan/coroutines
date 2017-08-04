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
      , WAITING
      , WAITING_FOR_EVENT
      , FREE
      };

      eState                    state = UNINITIALIZED;
      THandle                   this_handle;
      //uint16_t                  prev_id = INVALID_ID;            // Only used when active
      //uint16_t                  next_id = 0;

      // User 
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

        // This will return to who called us
        jump_fcontext(t.ctx, t.data);
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

      void run() {
        h_current = this_handle;
        ip = jump_fcontext(ip.ctx, ip.data);
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

      return co_new->this_handle;
    }

  }

  // --------------------------
  THandle current() {
    return internal::h_current;
  }

  // --------------------------------------------
  bool isHandle(THandle h) {
    return internal::byHandle(h) != nullptr;
  }

  using namespace internal;

  // --------------------------------------------
  struct TScheduler {
    int nactives;
    int next_idx = 0;
    
    int runActives() {
      nactives = 0;
      next_idx = 0;
      while (next_idx < coros.size()) {
        if (runNextReady())
          ++nactives;
      }
      return nactives;
    }

    bool runNextReady() {
      // Find one ready
      while (next_idx < coros.size()) {
        auto& co = coros[next_idx];
        ++next_idx;
        if (co.state == TCoro::FREE || co.state == TCoro::UNINITIALIZED) {
          continue;
        }
        co.run();
        return true;
      }
      return false;
    }
  };
  TScheduler scheduler;

  // --------------------------------------------
  void yield() {
    auto co = internal::byHandle(current());
    assert(co);
    jump_fcontext(co->caller_ctx.ctx, co->caller_ctx.data);
  }

  // ----------------------------------------------------------
  int executeActives() {
    return scheduler.runActives();
  }

}