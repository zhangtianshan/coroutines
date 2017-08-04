#include "coroutines/coroutines.h"
#include <cstdarg>
#include <cstdio>

using namespace Coroutines;

// --------------------------------------------------
int current_time = 0;
int now() { return current_time; }
void updateCurrentTime(int delta) { current_time += delta; }

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

void basicTask(const char* title) {
  dbg("%s boots\n", title);
  yield();
  dbg("%s after yield\n", title);
  yield();
  //wait(nullptr, 0, 30);
  //dbg("After waiting 30 ticks we leave\n");
  dbg("%s leaves\n", title);
}


int main(int argc, char** argv) {
  dbg("Main boots\n");

  auto f1 = []() { basicTask("co1"); };
  auto f2 = []() { basicTask("co2"); };

  auto co1 = start(f1);
  auto co2 = start(f2);

  runUntilAllCoroutinesEnd();

  runUntilAllCoroutinesEnd();


  return 0;
}


/*
#include "../coroutines/coroutines.h"
#include "../coroutines/channel.h"
    
// -------------------------------------------------------
// -------------------------------------------------------
// -------------------------------------------------------
// -------------------------------------------------------
using namespace Coroutines;

// Wait for another coroutine to finish
// wait while h is a coroutine handle
void wait(THandle h) {
  TWatchedEvent we(h);
  wait(&we, 1);
}

template< typename iterator >
void waitAll(iterator beg, iterator end) {
  while (beg != end) {
    wait(*beg);
    ++beg;
  }
}

// Wait until all coroutines have finished
void waitAll( std::initializer_list<THandle> handles ) {
  waitAll(handles.begin(), handles.end());
}

// Wait while the key is not pressed 
void waitKey(int c) {
  wait([c]() { return (::GetAsyncKeyState( c ) & 0x8000 ) == 0; });
}

void runUntilAllCoroutinesEnd() {
  while (true) {
    updateCurrentTime(1);
    if (!executeActives())
      break;
  }
  dbg("all done\n");
}


// --------------------------------------------------
// Wait for keys, time, other co's
void demo00() {

  auto co1 = start([]() {
    dbg("co1 boots\n");
    yield();
    dbg("At co1\n");
    wait(nullptr, 0, 30);
    dbg("After waiting 30 ticks we leave\n");
  });

  auto co3 = start([]() {
    dbg("At co3. Enter and exit\n");
  });

  auto co2 = start([co1]() {
    dbg("co2 boots\n");

    wait(co1);
    dbg("co2 continues now that co1 is not a handle anymore\n");

    int k = 0;
    while (k < 10 ) {
      dbg("At co2\n");
      yield();
      ++k;
    }
  });

  auto co4 = start([]() {
    dbg("At co4. Press the key 'A'\n");
    waitKey('A');
    dbg("At co4. Now press the key 'B'\n");
    waitKey('B');
    dbg("At co4. well done\n");
  });

  runUntilAllCoroutinesEnd();
  dbg("No coroutines active\n");

}

// --------------------------------------------
// Two simple empty co's
void demo_enter_and_exit() {
  auto co1 = start([]() {
    dbg("At co1. Enter and exit\n");
  });
  auto co2 = start([]() {
    dbg("At co2. Enter and exit\n");
  });
  runUntilAllCoroutinesEnd();
}

// --------------------------------------------
void doFn1() {
  dbg("At fn1\n");
}
void doFn2() {
  dbg("At fn2 starts\n");

  auto coA = start([]() {
    dbg("At A begins\n");
    wait(nullptr, 0, 25);
    dbg("At A ends\n");
  });
  auto coB = start([]() {
    dbg("At B begins\n");
    wait(nullptr, 0, 10);
    dbg("At B ends\n");
  });
  auto coC = start([]() {
    dbg("At C begins\n");
    wait(nullptr, 0, 15);
    dbg("At C ends\n");
  });
  
  // Waits for all co end before continuing...
  waitAll({ coA, coB, coC });

  dbg("At fn2 ends\n");
}
void doFn3() {
  dbg("At fn3\n");
}

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
  Coroutines::initialize();
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
