#ifndef INC_NET_SYS_DEFINES_H_
#define INC_NET_SYS_DEFINES_H_

#define PLATFORM_WINDOWS    1
#define _WINSOCK_DEPRECATED_NO_WARNINGS

// -----------------------------------------------------
#if defined( PLATFORM_PSP2 ) || defined( PLATFORM_PS4 )

#if defined( PLATFORM_PS4 )
  #include <net.h>
#endif

#define AF_INET						SCE_NET_AF_INET
#define IPPROTO_TCP       SCE_NET_IPPROTO_TCP
#define INADDR_ANY				SCE_NET_INADDR_ANY
#define INADDR_BROADCAST	SCE_NET_INADDR_BROADCAST

#define INET_ADDRSTRLEN	SCE_NET_INET_ADDRSTRLEN

#define MSG_PEEK        SCE_NET_MSG_PEEK

#define SO_REUSEADDR   	SCE_NET_SO_REUSEADDR
#define SO_BROADCAST   	SCE_NET_SO_BROADCAST
#define SO_RCVTIMEO   	SCE_NET_SO_RCVTIMEO
#define SO_SNDTIMEO   	SCE_NET_SO_SNDTIMEO
#define SO_NON_BLOCK    SCE_NET_SO_NBIO
#define SOL_SOCKET   	  SCE_NET_SOL_SOCKET
#define TCP_NODELAY   	SCE_NET_TCP_NODELAY
#define TCP_MAXSEG   	  SCE_NET_TCP_MAXSEG

#define SOCK_DGRAM			SCE_NET_SOCK_DGRAM
#define SOCK_DGRAM_P2P	SCE_NET_SOCK_DGRAM_P2P
#define SOCK_STREAM			SCE_NET_SOCK_STREAM
#define SOCK_STREAM_P2P	SCE_NET_SOCK_STREAM_P2P
#define NET_P2P_SUPPORT_ENABLED			1
#define IPPROTO_UDP     0

#define htons						sceNetHtons
#define ntohs						sceNetNtohs
#define htonl						sceNetHtonl
#define ntohl						sceNetNtohl

#define inet_pton				sceNetInetPton
#define inet_ntop				sceNetInetNtop

#define sys_close       sceNetSocketClose
#define sys_bind(id,addr,sz) sceNetBind( id, (const SceNetSockaddr *)(const void*)(addr), sz )
#define setsockopt      sceNetSetsockopt
#define getsockopt      sceNetGetsockopt
#define sys_sendto(id,msg_data,msg_bytes,flags,addr,addr_len)    sceNetSendto( id, msg_data, msg_bytes, flags, (const SceNetSockaddr *)(const void*)(addr), addr_len)
#define sys_recvfrom(id,msg_data,msg_bytes,flags,addr,addr_len)  sceNetRecvfrom( id, msg_data, msg_bytes, flags, (SceNetSockaddr *)(void*)(addr), addr_len)
#define sys_connect(id,addr,sz) sceNetConnect( id, (const SceNetSockaddr *)(const void*)(addr), sz )
#define sys_accept(id,addr,sz)  sceNetAccept( id, (SceNetSockaddr *)(void*)(addr), sz )
#define sys_send        sceNetSend
#define sys_recv        sceNetRecv
#define sys_listen      sceNetListen
#define sys_socket(x,y,z,port)	sceNetSocket( "UDPSocket", x, y, z )
#define sys_seterror(bytes,sys_errno)                                           last_error_code = bytes

#define sockaddr        SceNetSockaddr
#define socklen_t       SceNetSocklen_t

#define default_vport   SCE_NET_ADHOC_PORT
#define SYSFN_DATA_TYPE   void
#define SYSNTOP_DATA_TYPE const void *

#define sys_errno        errno

#define NET_ADDR_HAS_SIN_LEN_MEMBER

// -----------------------------------------------------
#elif PLATFORM_WINDOWS

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

#define sys_close             ::closesocket
#define sys_bind(id,addr,sz)  ::bind(id, (const sockaddr *) addr, sz )
#define sys_fcntl         ::ioctlsocket
#define sys_errno					::WSAGetLastError()
#define sys_socket(x,y,z,port)	::socket( x, y, z )
#define sys_connect(id,addr,sz) ::connect( id, (const sockaddr*) addr, sz )
#define sys_accept(id,addr,sz)  ::accept( id, (sockaddr*) addr, sz )
#define sys_send          ::send
#define sys_recv          ::recv
#define sys_listen        ::listen
#define sys_sendto(sock_id, data, data_bytes, flags, target_addr, sizeof_target_addr )        ::sendto( sock_id, (const char *)data, (int)data_bytes, flags, (const sockaddr *) target_addr, sizeof_target_addr )
#define sys_recvfrom(sock_id, data, data_bytes, flags, sender_addr, sizeof_sender_addr )      ::recvfrom( sock_id, (char *) data, (int)data_bytes, flags, (sockaddr*) sender_addr, sizeof_sender_addr )
#define sys_seterror(bytes,sys_errno)                                                         last_error_code = sys_errno

#define socklen_t         int

#define default_vport     0
#define SYSFN_DATA_TYPE		char 
#define SYSNTOP_DATA_TYPE PVOID 

// No specific support for p2p
#define SOCK_DGRAM_P2P    SOCK_DGRAM
#define SOCK_STREAM_P2P   SOCK_STREAM
#define NET_P2P_SUPPORT_ENABLED   0

//const char* inet_ntop(int af, const void* src, char* dst, int cnt);			// win32 do not define const the second param

// -----------------------------------------------------
#elif PLATFORM_PSP

#include <pspnet_adhoc.h>
const struct SceNetEtherAddr *getLocalMacAddress();

#define sockaddr                  SceNetEtherAddr

#define sys_close(x)              sceNetAdhocPtpClose(x,0)
#define sys_socket(x,y,z,port)    sceNetAdhocPdpCreate( getLocalMacAddress(), port, 8192, 0 )
#define sys_bind(a1,a2,a3)        1     // Not used
#define setsockopt(a1,a2,a3,a4,a5)  a2  // Not used
#define sys_sendto(sock_id, data, data_bytes, flags, target_addr, sizeof_target_addr ) (sceNetAdhocPdpSend( sock_id, target_addr, addr.getPort(), data, data_bytes, 0, 0 ) == 0) ? (int)data_bytes : -1
#define sys_recvfrom(sock_id, data, data_bytes, flags, sender_addr, sizeof_sender_addr ) (sceNetAdhocPdpRecv( sock_id, sender_addr, &addr.addr.port, data, (int*) &nbytes, 0, 0 ) == 0) ? (int)nbytes : -1

#define SOCK_DGRAM_P2P            0     // Not used
#define SOCK_DGRAM                0     // Not used
#define SO_BROADCAST              0     // Not used
#define SOL_SOCKET                0     // Not used
#define SYSFN_DATA_TYPE           void  // Not used
#define sys_errno                 0
#define socklen_t                 int

#define EWOULDBLOCK               -1
#define EINPROGRESS               -2
#define EACCES                    -3
#define EAFNOSUPPORT              -4
#define EHOSTDOWN                 -5
#define EADDRNOTAVAIL             -6

// -----------------------------------------------------
#elif PLATFORM_PS3
#include <netinet/tcp.h>

#define sys_bind(id,addr,sz) ::bind(id, (const sockaddr *) addr, sz )
#define sys_connect(id,addr,sz) ::connect( id, (const sockaddr*) addr, sz )
#define sys_accept(id,addr,sz)  ::accept( id, (sockaddr*) addr, sz )
#define sys_listen        ::listen
#define sys_close         ::socketclose
#define sys_socket(x,y,z,port)	::socket( x, y, 0 )
#define sys_errno         sys_net_errno
#define sys_fcntl         ::fcntl
#define sys_send          ::send
#define sys_recv          ::recv
#define sys_sendto(sock_id, data, data_bytes, flags, target_addr, sizeof_target_addr )        ::sendto( sock_id, data, data_bytes, flags, (const sockaddr*) target_addr, sizeof_target_addr)
#define sys_recvfrom(sock_id, data, data_bytes, flags, sender_addr, sizeof_sender_addr )      ::recvfrom( sock_id, data, data_bytes, flags, (sockaddr*) sender_addr, sizeof_sender_addr )
#define sys_select        socketselect
#define sys_seterror(bytes,sys_errno)                                           last_error_code = bytes

// No specific support for p2p
#define NET_P2P_SUPPORT_ENABLED			1

#define EWOULDBLOCK       SYS_NET_EWOULDBLOCK


#define default_vport     3658
#define SYSFN_DATA_TYPE		void 
#define SYSNTOP_DATA_TYPE const void *

// -----------------------------------------------------
// UNIX/OSX/IOS
#else

#include <fcntl.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#ifdef PLATFORM_ANDROID
#include <fcntl.h>
#else
#include <sys/fcntl.h>
#endif
#include <netinet/tcp.h>      // Required by IOS

#define sys_bind(id,addr,sz)          ::bind(id, (const sockaddr *) addr, sz )
#define sys_connect(id,addr,sz) ::connect( id, (const sockaddr*) addr, sz )
#define sys_accept(id,addr,sz)  ::accept( id, (sockaddr*) addr, sz )
#define sys_listen        ::listen
#define sys_close         ::close
#define sys_socket(x,y,z,port)	::socket( x, y, z )
#define sys_errno         errno
#define sys_fcntl         ::fcntl
#define sys_send          ::send
#define sys_recv          ::recv
#define sys_sendto(sock_id, data, data_bytes, flags, target_addr, sizeof_target_addr )         ::sendto( sock_id, data, data_bytes, flags, (const sockaddr*) target_addr, sizeof_target_addr)
#define sys_recvfrom(sock_id, data, data_bytes, flags, sender_addr, sizeof_sender_addr )      ::recvfrom( sock_id, data, data_bytes, flags, (sockaddr*) sender_addr, sizeof_sender_addr )
#define sys_seterror(bytes,sys_errno)                                           last_error_code = bytes

// No specific support for p2p
#define SOCK_DGRAM_P2P	SOCK_DGRAM
#define SOCK_STREAM_P2P	SOCK_STREAM
#define NET_P2P_SUPPORT_ENABLED			0

#define default_vport     0
#define SYSFN_DATA_TYPE		void 
#define SYSNTOP_DATA_TYPE const void *

#endif

const char *getNetErrorStr( int rc );

#endif
 