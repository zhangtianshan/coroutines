#include "coroutines/coroutines.h"
#include <cstdarg>
#include <cstdio>
#include <Windows.h>

using namespace Coroutines;

// --------------------------------------------------
void dbg(const char *fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  int n = _vsnprintf_s(buf, sizeof(buf) - 1, fmt, ap);
  if (n < 0)
    buf[1023] = 0x00;
  va_end(ap);
  printf("%04d:%02d.%02d %s", (int)now(), current().id, current().age, buf);
}

void runUntilAllCoroutinesEnd() {
  while (true) {
    updateCurrentTime(1);
    if (!executeActives())
      break;
  }
  dbg("all done\n");
}

struct TSimpleDemo {
  const char* title;
  TSimpleDemo(const char* new_title) : title( new_title ) {
    dbg("-------------------------------\n%s starts\n", title);
  }
  ~TSimpleDemo() {
    dbg("%s waiting co's to finish\n", title);
    runUntilAllCoroutinesEnd();
    dbg("%s ends\n", title);
  }
};

// -----------------------------------------------------------
void demo_yield(const char* title) {
  dbg("%s boots\n", title);
  yield();
  dbg("%s after yield\n", title);
  yield();
  dbg("%s after yield 2\n", title);
  yield();
  dbg("%s leaves\n", title);
}

void test_demo_yield() {
  TSimpleDemo demo("test_demo_yield");
  auto f1 = []() { demo_yield("co1"); };
  auto f2 = []() { demo_yield("co2"); };
  auto co1 = start(f1);
  auto co2 = start(f2);
  auto co3 = start([]() {
    dbg("At co3. Enter and exit\n");
  });
}

// -----------------------------------------------------------
void basic_wait_time(const char* title, int nsecs) {
  dbg("%s boots. Will wait %d secs\n", title, nsecs);
  wait(nullptr, 0, nsecs);
  dbg("%s After waiting %d ticks we leave\n", title, nsecs);
}

void test_wait_time() {
  TScopedTime tm;
  {
    TSimpleDemo demo("test_wait_time");
    auto co1 = start([]() { basic_wait_time("co1", 3); });
    auto co2 = start([]() { basic_wait_time("co2", 5); });
  }
  assert(tm.elapsed() == 6);
}

// -----------------------------------------------------------
void test_wait_co() {
  TScopedTime tm;
  {
    TSimpleDemo demo("test_wait_co");
    auto co1 = start([]() { basic_wait_time("co1", 3); });
    start([co1]() {
      dbg("Co2: Waiting for co1\n");
      wait(co1);
      dbg("Co2: co1 is ready. continuing\n");
    });
  }
  assert(tm.elapsed() == 4);
}
  


// -----------------------------------------------------------
void test_wait_all() {
  TScopedTime tm;
  {
    TSimpleDemo demo("test_wait_all");
    auto co1 = start([]() {
      auto coA = start([]() {basic_wait_time("A", 25); });
      auto coB = start([]() {basic_wait_time("B", 10); });
      auto coC = start([]() {basic_wait_time("C", 15); });

      // Waits for all co end before continuing...
      waitAll({ coA, coB, coC });
      dbg("waitAll continues...\n");
    });
  }
  assert(tm.elapsed() == 28);
}

// ---------------------------------------------------------
// Wait while the key is not pressed 
void waitKey(int c) {
  wait( [c]() { return (::GetAsyncKeyState(c) & 0x8000) == 0; });
}

void test_wait_keys() {
  TSimpleDemo demo("test_wait_keys");
  auto coKeys = start([]() {
    dbg("At coKeys. Press the key 'A'\n");
    waitKey('A');
    dbg("At coKeys. Now press the key 'B'\n");
    waitKey('B');
    dbg("At coKeys. well done\n");
  });
}


// -----------------------------------------------------------
int main(int argc, char** argv) {
  test_demo_yield();
  test_wait_time();
  test_wait_co();
  test_wait_all();
  test_wait_keys();
  return 0;
}


/*
#include "../coroutines/channel.h"
    

// --------------------------------------------
void doRange(int n_beg, int n_end) {
  while (n_beg < n_end) {
    dbg("At doRange( %d..%d )\n", n_beg, n_end);
    n_beg++;
    yield();
  }
}

void doFn4(int nelems, int step) {
  dbg("At fn4 begin\n");

  std::vector< THandle > children;
  int n0 = 0;
  while (n0 < nelems) {
    int n1 = std::min(n0 + step, nelems);
    auto h = start([n0, n1]() { doRange(n0, n1); });
    children.push_back(h);
    n0 = n1;
  }

  waitAll(children.begin(), children.end());

  dbg("At fn4 end\n");
}

// ----------------------------------------
// async series + parallel
void demo_async_series() {
  resetTimer();

  // Run some items in serie, then fn2 runs in parallel
  auto co_series = start([]() {

    doFn1();
    doFn2();
    doFn3();
    doFn4(32, 10);

  });

  runUntilAllCoroutinesEnd();
}

// ----------------------------------------
void demo_channels() {
  resetTimer();

  // to send/recv data between co's
  TChannel* ch1 = new TChannel(5, sizeof(int));
  dbg("ch is %p\n", ch1);

  // co1 consumes
  auto co1 = start([ch1]() {
    dbg("co1 begin\n");
    
    while (true) {
      int data = 0;
      if (!pull(ch1, data))
        break;
      dbg("co1 has pulled %d\n", data);
    }

    dbg("co1 end\n");
    assert(now() == 2);
  });

  // co2 produces 10 elems
  auto co2 = start([ch1]() {
    dbg("co2 begin\n");

    // We can only fit 5 elems in the channel. When trying to push the 6th it will block us
    for (int i = 0; i < 10; ++i) {
      int v = 100 + i;
      push(ch1, v);
      dbg("co2 has pushed %d\n", v);
    }

    // pull from ch1 will return false once all elems have been pulled
    ch1->close();

    dbg("co2 ends\n");
    assert(now() == 1);
  });
  
  //for( int i=0; i<3; ++i )
  //  push(ch1, i);
  //dbg("Closing ch1\n");
  //ch1->close();
  runUntilAllCoroutinesEnd();
}

// ----------------------------------------
void demo_channels_send_from_main() {

  // send data between co's
  TChannel* ch1 = new TChannel(5, sizeof(int));
  dbg("ch is %p\n", ch1);
  assert(ch1->bytesPerElem() == 4);

  // co1 consumes
  auto co1 = start([ch1]() {
    dbg("co1 begin\n");
    assert(ch1->bytesPerElem() == 4);
    while (true) {
      int data = 0;
      if (!pull(ch1, data))
        break;
      dbg("co1 has pulled %d from %p\n", data, ch1);
    }

    dbg("co1 end\n");
  });

  int v = 100;
  dbg("Main pushes 100 twice\n");
  push(ch1, v);
  push(ch1, v);
  ch1->close();
  runUntilAllCoroutinesEnd();
}

// ----------------------------------------
// Wait for another coroutine to finish
// ----------------------------------------
void demo05_wait2coroutines() {
  resetTimer();

  auto co1a = start([]() {
    dbg("co1a begin\n");
    wait(nullptr, 0, 5);
    dbg("co1a end\n");
    assert(now() == 5);
  });

  auto co1b = start([]() {
    dbg("co1b begins\n");
    wait(nullptr, 0, 10);
    dbg("co1b ends\n");
    assert(now() == 10);
  });

  auto co2 = start([co1a, co1b]() {
    dbg("co2 begins\n");
    while (true) {
      int n = 0;
      TWatchedEvent evts[2];
      if (isHandle(co1a))
        evts[n++] = TWatchedEvent(co1a);
      if (isHandle(co1b))
        evts[n++] = TWatchedEvent(co1b);
      if (!n)
        break;
      dbg("co2 goes to sleep waiting for co1a or co1b to end (%d)\n", n);
      wait(evts, n);
    }
    dbg("co2 ends\n");
    assert(now() == 10);
  });

  runUntilAllCoroutinesEnd();
}

// ----------------------------------------
//
// ----------------------------------------
void wait_with_timeout() {
  resetTimer();
  auto co1a = start([]() {
    dbg("co1a begin. Will wait for 100 ticks\n");
    wait(nullptr, 0, 100);
    dbg("co1a end\n");
  });
  auto co2 = start([co1a]() {
    dbg("co2 waits for co1a or a timeout of 5 ticks\n" );
    TWatchedEvent evt(co1a);
    int rc = wait(&evt, 1, 5);
    dbg("co2 wait finishes. k = %d\n", rc);
    assert(rc == wait_timedout);
    assert(now() == 5);
    dbg("co2 waiting again, now for 200 ticks\n");
    rc = wait(&evt, 1, 200);
    dbg("co2 wait finishes. k = %d\n", rc);
    assert(rc == 0);
    assert(now() == 100);
  });
  runUntilAllCoroutinesEnd();
}

// ----------------------------------------
int main() {
  //demo00();
  //demo_enter_and_exit();
  demo_async_series();
  demo_channels();
  //demo_channels_send_from_main();
  demo05_wait2coroutines();
  wait_with_timeout();
  
  return 0;
}


*/
