#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <Windows.h>
#else
# include <time.h>
#endif

#include <cstdio>
#include <cassert>
#include <vector>
#include <set>

#include "fcontext/fcontext.h"

class TCoroutine;

struct CScheduler {
  std::set< TCoroutine* > running;
public:
  CScheduler();
  void execute(TCoroutine* co);
};

class TCoroutine {

  enum eState {
    UNKNOWN
  , RUNNING
  , READY_TO_RUN
  , WAITING
  , TERMINATED
  };

  const char*         name = "<unnamed>";
  eState              state = UNKNOWN;
  fcontext_t          ctx;
  fcontext_transfer_t ip;
  fcontext_stack_t    stack;
  typedef void(*TUserFn)();
  TUserFn             user_fn;
  fcontext_transfer_t ctx_calling_me = { nullptr, nullptr };
  CScheduler*         scheduler = nullptr;

  static TCoroutine*  this_co;
  static CScheduler*  default_scheduler;
  static const size_t default_stack_size = 16 * 1024;

  void create( pfn_fcontext fn ) {
    stack = create_fcontext_stack(default_stack_size);
    ctx = make_fcontext(stack.sptr, stack.ssize, fn);
  }

  void destroy() {
    destroy_fcontext_stack(&stack);
  }
  
  static void entryPoint(fcontext_transfer_t t) {
    assert(t.data);
    TCoroutine* co = (TCoroutine*)t.data;
    printf("%p %s starts. called from %p,%p\n", co, co->name, t.ctx, t.data);
    co->state = RUNNING;
    co->ctx_calling_me = t;
    assert(co->user_fn);
    co->user_fn();
    co->state = TERMINATED;
    jump_fcontext(t.ctx, t.data);
  }
  
  friend struct CScheduler;

public:

  TCoroutine() = default;

  TCoroutine(TUserFn new_user_fn, const char* new_name) {
    name = new_name;
    assert(default_scheduler);
    user_fn = new_user_fn;
    create(entryPoint);
    ip.ctx = ctx;
    ip.data = this;
    printf("%p %s ctor. %p\n", this, name, ctx);
    state = TCoroutine::READY_TO_RUN;

    scheduler = default_scheduler;
    scheduler->execute(this);
  }

  void join() {
    if (!scheduler)
      return;
    while ( state != TCoroutine::TERMINATED ) {
      scheduler->execute(this);
    }
  }
  
  static void yield() {
    assert(this_co);
    assert(this_co->ctx_calling_me.ctx);
    printf("%p %s yield. calling to %p,%p\n", this_co, this_co->name, this_co->ctx_calling_me.ctx, this_co->ctx_calling_me.data);
    this_co = (TCoroutine*)this_co->ctx_calling_me.data;
    assert(this_co);
    TCoroutine* co = this_co;
    co->ip = jump_fcontext(this_co->ctx_calling_me.ctx, this_co->ctx_calling_me.data);
  }

};

// -----------------------------------------------------
CScheduler::CScheduler() {
  TCoroutine::default_scheduler = this;
}

void CScheduler::execute(TCoroutine* co) {
  assert(co->scheduler == this);
  running.insert(co);
  TCoroutine::this_co = co;
  co->state = TCoroutine::RUNNING;
  printf("%p %s execute. will switch to %p,%p\n", co, co->name, co->ctx, co);
  co->ip = jump_fcontext(co->ip.ctx, co->ip.data);
  TCoroutine::this_co = nullptr;
  if(co->state == TCoroutine::TERMINATED)
    running.erase(co);
  else {
    co->state = TCoroutine::READY_TO_RUN;
  }
}

TCoroutine* TCoroutine::this_co = nullptr;
CScheduler* TCoroutine::default_scheduler = nullptr;

void fco1() {
  printf("Hi from co1\n");
  TCoroutine::yield();
  printf("co1 ends\n");
}

void fco2() {
  printf("Hi from co2\n");
  TCoroutine::yield();
  printf("co2 ends\n");
}

void test2() {

  CScheduler s;

  TCoroutine co1(&fco1, "co1");
  TCoroutine co2(&fco2, "co2");
  printf("both co's have started\n");

  co2.join();
  printf("co1 is done\n");

  co1.join();
  printf("both co's have started\n");
}


int main() {
  test2();
  return 0;
}



/*


static void doo(fcontext_transfer_t t)
{
puts("DOO");
jump_fcontext(t.ctx, NULL);
}

static void foo(fcontext_transfer_t t)
{
puts("FOO");
auto t2 = jump_fcontext(t.ctx, NULL);
puts("FOO 2");
jump_fcontext(t2.ctx, NULL);
}


void test3() {
fcontext_stack_t s = create_fcontext_stack();
fcontext_stack_t s2 = create_fcontext_stack();
fcontext_t ctx = make_fcontext(s.sptr, s.ssize, foo);
fcontext_t ctx2 = make_fcontext(s2.sptr, s2.ssize, doo);

auto t2 = jump_fcontext(ctx, NULL);
puts("main");
jump_fcontext(t2.ctx, NULL);
puts("main 2");

}

*/