#ifndef INC_COROUTINES_API_PLATFORM_H_
#define INC_COROUTINES_API_PLATFORM_H_

#include <vector>
#include <cstdint>

class TCoroPlatform {
  void*                  fiber;
  bool                   is_main;
  std::vector< uint8_t > stack;
  uint32_t               stack_requested_size;
  
  typedef void (TStartFn)(void *);

public:
  TCoroPlatform();
  ~TCoroPlatform();

  void start(TStartFn fn, void* start_arg);
  void switchTo(TCoroPlatform* other);

  bool isMain() const { return is_main; }
  bool initAsMain();

};

#endif