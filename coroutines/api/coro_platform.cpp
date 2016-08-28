#include "coro_platform.h"
#include <cassert>

#include <Windows.h>

TCoroPlatform::TCoroPlatform()
  : fiber(nullptr)
  , is_main(false)
  , stack_size( 128 * 1024 )
{}

TCoroPlatform::~TCoroPlatform() {
  if (fiber) {
    if (!is_main)
      ::DeleteFiber(fiber);
    else
      ::ConvertFiberToThread();
  }
}

void TCoroPlatform::switchTo(TCoroPlatform* other) {
  assert(other);
  assert(other->fiber);
  ::SwitchToFiber(other->fiber);
}

bool TCoroPlatform::initAsMain() {
  assert(!is_main);
  is_main = true;
  ::ConvertThreadToFiber(nullptr);
  fiber = ::GetCurrentFiber();
  return (fiber != nullptr);
}

struct startUpInformation {
  TCoroPlatform::TStartFn* func;
  void*                    context;
};

static startUpInformation globalCallbackBlock;

void TCoroPlatform::start(TStartFn fn, void* start_arg) {

  globalCallbackBlock.func = fn;
  globalCallbackBlock.context = start_arg;

  if (fiber && !is_main)
    ::DeleteFiber(fiber);

  fiber = ::CreateFiber(stack_size, (LPFIBER_START_ROUTINE)fn, start_arg);
  assert(fiber);
  switchTo(this);
}



/*
 Credits

	Originally based on Edgar Toernig's Minimalistic cooperative multitasking
	http://www.goron.de/~froese/
	reorg by Steve Dekorte and Chis Double
	Symbian and Cygwin support by Chis Double
	Linux/PCC, Linux/Opteron, Irix and FreeBSD/Alpha, ucontext support by Austin Kurahone
	FreeBSD/Intel support by Faried Nawaz
	Mingw support by Pit Capitain
	Visual C support by Daniel Vollmer
	Solaris support by Manpreet Singh
	Fibers support by Jonas Eschenburg
	Ucontext arg support by Olivier Ansaldi
	Ucontext x86-64 support by James Burgess and Jonathan Wright
	Russ Cox for the newer portable ucontext implementions.

 Notes

	This is the system dependent coro code.
	Setup a jmp_buf so when we longjmp, it will invoke 'func' using 'stack'.
	Important: 'func' must not return!

	Usually done by setting the program counter and stack pointer of a new, empty stack.
	If you're adding a new platform, look in the setjmp.h for PC and SP members
	of the stack structure

	If you don't see those members, Kentaro suggests writting a simple
	test app that calls setjmp and dumps out the contents of the jmp_buf.
	(The PC and SP should be in jmp_buf->__jmpbuf).

	Using something like GDB to be able to peek into register contents right
	before the setjmp occurs would be helpful also.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include <setjmp.h>
#include <stdarg.h>
#include <string.h>

#include "coro_platform.h"
#include "taskimpl.h"

#ifdef USE_VALGRIND
#include <valgrind/valgrind.h>
#define STACK_REGISTER(coro) \
{ \
	Coro *c = (coro); \
		c->valgrindStackId = VALGRIND_STACK_REGISTER( \
											 c->stack, \
											 c->stack + c->requestedStackSize); \
}

#define STACK_DEREGISTER(coro) \
VALGRIND_STACK_DEREGISTER((coro)->valgrindStackId)

#else
#define STACK_REGISTER(coro)
#define STACK_DEREGISTER(coro)
#endif

typedef struct CallbackBlock
{
	void *context;
	CoroStartCallback *func;
} CallbackBlock;

static CallbackBlock globalCallbackBlock;

Coro *Coro_new(void)
{
	Coro *self = (Coro *)malloc(sizeof(Coro));
	self->requestedStackSize = CORO_DEFAULT_STACK_SIZE;
	self->allocatedStackSize = 0;
  self->isMain = 0;
  self->stack = NULL;

#ifdef USE_FIBERS
	self->fiber = NULL;
#else
	self->stack = NULL;
#endif
	return self;
}

void Coro_allocStackIfNeeded(Coro *self)
{
	if (self->stack && self->requestedStackSize < self->allocatedStackSize)
	{
		free(self->stack);
		self->stack = NULL;
		self->requestedStackSize = 0;
	}

	if (!self->stack)
	{
		self->stack = malloc(self->requestedStackSize + 16);
		self->allocatedStackSize = self->requestedStackSize;
		//printf("Coro_%p allocating stack size %i\n", (void *)self, self->requestedStackSize);
		STACK_REGISTER(self);
	}
}

void Coro_free(Coro *self)
{
#ifdef USE_FIBERS
	// If this coro has a fiber, delete it.
	// Don't delete the main fiber. We don't want to commit suicide.
	if (self->fiber && !self->isMain)
	{
		DeleteFiber(self->fiber);
	}
#else
	STACK_DEREGISTER(self);
#endif
	if (self->stack)
	{
		free(self->stack);
	}

	//printf("Coro_%p io_free\n", (void *)self);

	free(self);
}

// stack

void *Coro_stack(Coro *self)
{
	return self->stack;
}

size_t Coro_stackSize(Coro *self)
{
	return self->requestedStackSize;
}

void Coro_setStackSize_(Coro *self, size_t sizeInBytes)
{
	self->requestedStackSize = sizeInBytes;
	//self->stack = (void *)io_realloc(self->stack, sizeInBytes);
	//printf("Coro_%p io_reallocating stack size %i\n", (void *)self, sizeInBytes);
}

#if __GNUC__ == 4
uint8_t *Coro_CurrentStackPointer(void) __attribute__ ((noinline));
#endif

uint8_t *Coro_CurrentStackPointer(void)
{
	uint8_t a;
	uint8_t *b = &a; // to avoid compiler warning about unused variables
	return b;
}

size_t Coro_bytesLeftOnStack(Coro *self)
{
	unsigned char dummy;
	ptrdiff_t p1 = (ptrdiff_t)(&dummy);
	ptrdiff_t p2 = (ptrdiff_t)Coro_CurrentStackPointer();
	int stackMovesUp = p2 > p1;
	ptrdiff_t start = ((ptrdiff_t)self->stack);
	ptrdiff_t end   = start + self->requestedStackSize;

	if (stackMovesUp) // like x86
	{
		return end - p1;
	}
	else // like OSX on PPC
	{
		return p1 - start;
	}
}

int Coro_stackSpaceAlmostGone(Coro *self)
{
	return Coro_bytesLeftOnStack(self) < CORO_STACK_SIZE_MIN;
}

Coro* Coro_initializeMainCoro(Coro *self)
{
	self->isMain = 1;
#ifdef USE_FIBERS
	// We must convert the current thread into a fiber if it hasn't already been done.
	if ((LPVOID) 0x1e00 == GetCurrentFiber()) // value returned when not a fiber
	{
		// Make this thread a fiber and set its data field to the main coro's address
		ConvertThreadToFiber(self);
	}
	// Make the main coro represent the current fiber
	self->fiber = GetCurrentFiber();
#endif
  return self;
}

void Coro_startCoro_(Coro *self, Coro *other, void *context, CoroStartCallback *callback)
{
	CallbackBlock sblock;
	CallbackBlock *block = &sblock;
	//CallbackBlock *block = malloc(sizeof(CallbackBlock)); // memory leak
	block->context = context;
	block->func    = callback;
	
	Coro_allocStackIfNeeded(other);
	Coro_setup(other, block);
	Coro_switchTo_(self, other);
}

/*
void Coro_startCoro_(Coro *self, Coro *other, void *context, CoroStartCallback *callback)
{
	globalCallbackBlock.context = context;
	globalCallbackBlock.func    = callback;
	Coro_allocStackIfNeeded(other);
	Coro_setup(other, &globalCallbackBlock);
	Coro_switchTo_(self, other);
}
*/


// old code

/*
 // APPLE coros are handled by PortableUContext now
#elif defined(_BSD_PPC_SETJMP_H_)

#define buf (self->env)
#define setjmp  _setjmp
#define longjmp _longjmp

 void Coro_setup(Coro *self, void *arg)
 {
	 size_t *sp = (size_t *)(((intptr_t)Coro_stack(self) + Coro_stackSize(self) - 64 + 15) & ~15);

	 setjmp(buf);

	 //printf("self = %p\n", self);
	 //printf("sp = %p\n", sp);
	 buf[0]  = (int)sp;
	 buf[21] = (int)Coro_Start;
	 //sp[-4] = (size_t)self; // for G5 10.3
	 //sp[-6] = (size_t)self; // for G4 10.4

	 //printf("self = %p\n", (void *)self);
	 //printf("sp = %p\n", sp);
 }

#elif defined(_BSD_I386_SETJMP_H)

#define buf (self->env)

 void Coro_setup(Coro *self, void *arg)
 {
	 size_t *sp = (size_t *)((intptr_t)Coro_stack(self) + Coro_stackSize(self));

	 setjmp(buf);

	 buf[9] = (int)(sp); // esp
	 buf[12] = (int)Coro_Start; // eip
						   //buf[8] = 0; // ebp
 }
 */

/* Solaris supports ucontext - so we don't need this stuff anymore

void Coro_setup(Coro *self, void *arg)
{
	// this bit goes before the setjmp call
	// Solaris 9 Sparc with GCC
#if defined(__SVR4) && defined (__sun)
#if defined(_JBLEN) && (_JBLEN == 12) && defined(__sparc)
#if defined(_LP64) || defined(_I32LPx)
#define JBTYPE long
	JBTYPE x;
#else
#define JBTYPE int
	JBTYPE x;
	asm("ta 3"); // flush register window
#endif

#define SUN_STACK_END_INDEX   1
#define SUN_PROGRAM_COUNTER   2
#define SUN_STACK_START_INDEX 3

	// Solaris 9 i386 with GCC
#elif defined(_JBLEN) && (_JBLEN == 10) && defined(__i386)
#if defined(_LP64) || defined(_I32LPx)
#define JBTYPE long
					JBTYPE x;
#else
#define JBTYPE int
					JBTYPE x;
#endif
#define SUN_PROGRAM_COUNTER 5
#define SUN_STACK_START_INDEX 3
#define SUN_STACK_END_INDEX 4
#endif
#endif
					*/

/* Irix supports ucontext - so we don't need this stuff anymore

#elif defined(sgi) && defined(_IRIX4_SIGJBLEN) // Irix/SGI

void Coro_setup(Coro *self, void *arg)
{
	setjmp(buf);
	buf[JB_SP] = (__uint64_t)((char *)stack + stacksize - 8);
	buf[JB_PC] = (__uint64_t)Coro_Start;
}
*/

/* Linux supports ucontext - so we don't need this stuff anymore

#elif defined(linux)
// Various flavors of Linux.
#if defined(JB_GPR1)
// Linux/PPC
buf->__jmpbuf[JB_GPR1] = ((int) stack + stacksize - 64 + 15) & ~15;
buf->__jmpbuf[JB_LR]   = (int) Coro_Start;
return;

#elif defined(JB_RBX)
// Linux/Opteron
buf->__jmpbuf[JB_RSP] = (long int )stack + stacksize;
buf->__jmpbuf[JB_PC]  = Coro_Start;
return;

#elif defined(JB_SP)

// Linux/x86 with glibc2
buf->__jmpbuf[JB_SP] = (int)stack + stacksize;
buf->__jmpbuf[JB_PC] = (int)Coro_StartWithArg;
// Push the argument on the stack (stack grows downwards)
// note: stack is stacksize + 16 bytes long
((int *)stack)[stacksize/sizeof(int) + 1] = (int)self;
return;

#elif defined(_I386_JMP_BUF_H)
// x86-linux with libc5
buf->__sp = (int)stack + stacksize;
buf->__pc = Coro_Start;
return;

#elif defined(__JMP_BUF_SP)
// arm-linux on the sharp zauras
buf->__jmpbuf[__JMP_BUF_SP]   = (int)stack + stacksize;
buf->__jmpbuf[__JMP_BUF_SP+1] = (int)Coro_Start;
return;

#else

*/


/* Windows supports fibers - so we don't need this stuff anymore

#elif defined(__MINGW32__)

void Coro_setup(Coro *self, void *arg)
{
	setjmp(buf);
	buf[4] = (int)((unsigned char *)stack + stacksize - 16);   // esp
	buf[5] = (int)Coro_Start; // eip
}

#elif defined(_MSC_VER)

void Coro_setup(Coro *self, void *arg)
{
	setjmp(buf);
	// win32 visual c
	// should this be the same as __MINGW32__?
	buf[4] = (int)((unsigned char *)stack + stacksize - 16);  // esp
	buf[5] = (int)Coro_Start; // eip
}
*/


/* FreeBSD supports ucontext - so we don't need this stuff anymore

#elif defined(__FreeBSD__)
// FreeBSD.
#if defined(_JBLEN) && (_JBLEN == 81)
// FreeBSD/Alpha
buf->_jb[2] = (long)Coro_Start;     // sc_pc
buf->_jb[26+4] = (long)Coro_Start;  // sc_regs[R_RA]
buf->_jb[27+4] = (long)Coro_Start;  // sc_regs[R_T12]
buf->_jb[30+4] = (long)(stack + stacksize); // sc_regs[R_SP]
return;

#elif defined(_JBLEN)
// FreeBSD on IA32
buf->_jb[2] = (long)(stack + stacksize);
buf->_jb[0] = (long)Coro_Start;
return;

#else
Coro_UnsupportedPlatformError();
#endif
*/

/* NetBSD supports ucontext - so we don't need this stuff anymore

#elif defined(__NetBSD__)

void Coro_setup(Coro *self, void *arg)
{
	setjmp(buf);
#if defined(_JB_ATTRIBUTES)
	// NetBSD i386
	buf[2] = (long)(stack + stacksize);
	buf[0] = (long)Coro_Start;
#else
	Coro_UnsupportedPlatformError();
#endif
}
*/

/* Sun supports ucontext - so we don't need this stuff anymore

// Solaris supports ucontext - so we don't need this stuff anymore

void Coro_setup(Coro *self, void *arg)
{
	// this bit goes before the setjmp call
	// Solaris 9 Sparc with GCC
#if defined(__SVR4) && defined (__sun)
#if defined(_JBLEN) && (_JBLEN == 12) && defined(__sparc)
#if defined(_LP64) || defined(_I32LPx)
#define JBTYPE long
	JBTYPE x;
#else
#define JBTYPE int
	JBTYPE x;
	asm("ta 3"); // flush register window
#endif

#define SUN_STACK_END_INDEX   1
#define SUN_PROGRAM_COUNTER   2
#define SUN_STACK_START_INDEX 3

	// Solaris 9 i386 with GCC
#elif defined(_JBLEN) && (_JBLEN == 10) && defined(__i386)
#if defined(_LP64) || defined(_I32LPx)
#define JBTYPE long
					JBTYPE x;
#else
#define JBTYPE int
					JBTYPE x;
#endif
#define SUN_PROGRAM_COUNTER 5
#define SUN_STACK_START_INDEX 3
#define SUN_STACK_END_INDEX 4
#endif
#endif


#elif defined(__SVR4) && defined(__sun)
					// Solaris
#if defined(SUN_PROGRAM_COUNTER)
					// SunOS 9
					buf[SUN_PROGRAM_COUNTER] = (JBTYPE)Coro_Start;

					x = (JBTYPE)stack;
					while ((x % 8) != 0) x --; // align on an even boundary
					buf[SUN_STACK_START_INDEX] = (JBTYPE)x;
					x = (JBTYPE)((JBTYPE)stack-stacksize / 2 + 15);
					while ((x % 8) != 0) x ++; // align on an even boundary
					buf[SUN_STACK_END_INDEX] = (JBTYPE)x;

					*/



