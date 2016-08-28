#ifndef INC_COROUTINES_API_PLATFORM_H_
#define INC_COROUTINES_API_PLATFORM_H_

struct TCoroPlatform {
  void*       fiber;
  bool        is_main;
  unsigned    stack_size;
  
  TCoroPlatform();
  ~TCoroPlatform();
  
  void switchTo(TCoroPlatform* other);
  
  typedef void (TStartFn)(void *);

  void start(TStartFn fn, void* start_arg);
  bool initAsMain();
};

#define HAS_FIBERS 1
#define USE_FIBERS 1


/*


#if defined(WIN32) || defined(__WINS__) || defined(__MINGW32__) || defined(_MSC_VER)
#define inline __inline
#define snprintf _snprintf


#include <Windows.h>

#define ON_WINDOWS 1
#endif



//#include "Common.h"
//#include "PortableUContext.h"
#include "taskimpl.h"

//#define CORO_DEFAULT_STACK_SIZE     (65536/2)
//#define CORO_DEFAULT_STACK_SIZE  (65536*4)

//128k needed on PPC due to parser
#define CORO_DEFAULT_STACK_SIZE (128*1024)
#define CORO_STACK_SIZE_MIN 8192

#define CORO_API


// Pick which coro implementation to use
// The make file can set -DUSE_FIBERS, -DUSE_UCONTEXT or -DUSE_SETJMP to force this choice.
#if !defined(USE_FIBERS) && !defined(USE_UCONTEXT) && !defined(USE_SETJMP)

#if defined(WIN32) && defined(HAS_FIBERS)
#	define USE_FIBERS
#elif defined(HAS_UCONTEXT)
//#elif defined(HAS_UCONTEXT) && !defined(__x86_64__)
#	if !defined(USE_UCONTEXT)
#		define USE_UCONTEXT
#	endif
#else
#	define USE_SETJMP
#endif

#endif

#if defined(USE_FIBERS)
	#define CORO_IMPLEMENTATION "fibers"
#elif defined(USE_UCONTEXT)
	#include <sys/ucontext.h>
	#define CORO_IMPLEMENTATION "ucontext"
#elif defined(USE_SETJMP)
	#include <setjmp.h>
	#define CORO_IMPLEMENTATION "setjmp"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Coro Coro;

struct Coro
{
	size_t requestedStackSize;
	size_t allocatedStackSize;
	void *stack;

#ifdef USE_VALGRIND
	unsigned int valgrindStackId;
#endif

#if defined(USE_FIBERS)
	void *fiber;
#elif defined(USE_UCONTEXT)
	ucontext_t env;
#elif defined(USE_SETJMP)
	jmp_buf env;
#endif

	unsigned char isMain;
};

CORO_API Coro *Coro_new(void);
CORO_API void Coro_free(Coro *self);

// stack

CORO_API void *Coro_stack(Coro *self);
CORO_API size_t Coro_stackSize(Coro *self);
CORO_API void Coro_setStackSize_(Coro *self, size_t sizeInBytes);
CORO_API size_t Coro_bytesLeftOnStack(Coro *self);
CORO_API int Coro_stackSpaceAlmostGone(Coro *self);

CORO_API Coro* Coro_initializeMainCoro(Coro *self);

typedef void (CoroStartCallback)(void *);

CORO_API void Coro_startCoro_(Coro *self, Coro *other, void *context, CoroStartCallback *callback);
CORO_API void Coro_switchTo_(Coro *self, Coro *next);
CORO_API void Coro_setup(Coro *self, void *arg); // private

#ifdef __cplusplus
}
#endif
#endif
*/


#endif