#include "coro_platform.h"
#include <cassert>

TCoroPlatform::TCoroPlatform()
  : fiber(nullptr)
  , is_main(false)
  , stack_requested_size( 128 * 1024 )
{
}

TCoroPlatform::~TCoroPlatform() {
}

void TCoroPlatform::switchTo(TCoroPlatform* other) {
  assert(other);
}

bool TCoroPlatform::initAsMain() {
  assert(!is_main);
  is_main = true;
}

void TCoroPlatform::start(TStartFn fn, void* start_arg) {
	stack.resize( stack_requested_size + 16 );
}



