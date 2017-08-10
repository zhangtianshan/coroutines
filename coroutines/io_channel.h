#ifndef INC_COROUTINE_IO_CHANNEL_H_
#define INC_COROUTINE_IO_CHANNEL_H_

#include "coroutines/coroutines.h"
#include "coroutines/io_address.h"

namespace Coroutines {

  class CIOChannel {
    SOCKET_ID  fd = ~(SOCKET_ID(0));
    bool       isValid() const { return fd > 0; }
    bool       setNonBlocking();
  public:
    bool       connect(const TNetAddress &remote_server, int timeout_sec);
    int        recv(void* dest_buffer, size_t bytes_to_read);
    bool       send(const void* src_buffer, size_t bytes_to_send);
    void       close();
  };

}

#endif


