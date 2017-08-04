#ifndef INC_COROUTINES_API_PLATFORM_H_
#define INC_COROUTINES_API_PLATFORM_H_

class TCoroPlatform {
  void*       fiber;
  bool        is_main;
  unsigned    stack_size;
  
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