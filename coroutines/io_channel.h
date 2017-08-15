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
    // Will block until all bytes have been recv/sent
    bool       recv(void* dest_buffer, size_t bytes_to_read);
    bool       send(const void* src_buffer, size_t bytes_to_send);
    // Will return -1 if no bytes can been read. Will block until something is read.
    int        recvUpTo(void* dest_buffer, size_t max_bytes_to_read);
    void       close();
  };

}

#endif


