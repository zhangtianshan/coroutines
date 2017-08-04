#ifndef INC_JABA_COROUTINES_H_
#define INC_JABA_COROUTINES_H_

#include <cinttypes>
#include <cassert>
#include <functional>

namespace Coroutines {

  // --------------------------------------------------
  typedef uint64_t      u64;
  typedef uint8_t       u8;

  struct THandle {
    uint16_t id = 0;
    uint16_t age = 0;
    bool operator==(const THandle& other) const { return id == other.id && age == other.age; }
  };

  typedef void(*TBootFn)(void*);

  // --------------------------
  namespace internal {
    THandle start(TBootFn boot_fn, void* fn_addr);

    // This will translate any signature to a common calling interface
    template< typename TFn >
    static void userBoot(void* user_fn_addr) {
      TFn* fn = static_cast<TFn*> (user_fn_addr);
      (*fn)();
    }
  }

  // --------------------------
  template< typename TFn >
  THandle start(TFn user_fn) {
    return internal::start(&internal::userBoot<TFn>, &user_fn);
  }

  // --------------------------------------------------
  bool    isHandle(THandle h);
  THandle current();
  void    yield();
  int     executeActives();

  // --------------------------------------------------


  // --------------------------------------------------


  // --------------------------------------------------
//  typedef std::function<bool(void)> TWaitConditionFn;





}



#endif
