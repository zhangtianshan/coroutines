#include "net_sys_defines.h"
#include "net_platform.h"
#include "tcp_socket.h"
#include <cassert>
#include <cstdio>

#define CHECK_RC(title)	 //	if( rc < 0 ) sys_printf( "%s : %08x %s\n", title, rc, getSysErrorText( rc ) );
#define sys_printf       printf
#define getSysErrorText(x)    "Unknown"

// --------------------------------------
TTCPSocket::TTCPSocket( )
: id( SOCKET_NOT_INITIALIZED )
{}

// --------------------------------------
void TTCPSocket::destroy( ) {
	
	if( id != SOCKET_NOT_INITIALIZED ) {
		int rc = sys_close( id );
		CHECK_RC( "TTCPSocket::destroy" );
		id = SOCKET_NOT_INITIALIZED;
	}
}

// --------------------------------------
void TTCPSocket::disconnect( ) {
	destroy( );
}

// --------------------------------------
bool TTCPSocket::createServer( int port ) {
   
  if( create( ) 
    && setOption( SO_REUSEADDR, true )
    && bind( port ) 
    && listen( ) 
    ) {
      //setOption( SO_RCVTIMEO, 1*1000*1000 );
      //setOption( SO_SNDTIMEO, 1*1000*1000 );			// 1 second timeout
      sys_printf( "TCP Server created at port %d\n", port );
      return true;
  }

  destroy( );
  return false;
}


bool setNonBlocking(SOCKET_ID fd) {

#ifdef PLATFORM_WINDOWS
  u_long data = 1;
  if (ioctlsocket(fd, FIONBIO, &data) != 0)
    return false;
#else
  int flags = fcntl(fd, F_GETFL);
  if (flags == -1)
    return false;

  flags |= O_NONBLOCK;

  if (fcntl(fd, F_SETFL, flags) == -1)
    return false;
#endif
  return true;
}


// --------------------------------------
bool TTCPSocket::connectTo( const TNetAddress &remote_server, int timeout_sec, bool is_blocking ) {
	
	destroy( );

	if( !create( ) )
		return false;

	remote = remote_server;

	setOption( SO_SNDTIMEO, timeout_sec*1000*1000 );			// 1 second timeout
	setOption( SO_RCVTIMEO, timeout_sec*1000*1000 );
	setOption( TCP_NODELAY, 1 );

  if (!is_blocking)
    setNonBlocking(id);

	sys_printf( "TTCPSocket::Connecting to %s\n", remote.c_str() );

	// Establish the connection to the echo server
	int rc = sys_connect( id, &remote.addr, sizeof(remote));
	if (rc < 0) {
    int sys_err = WSAGetLastError();
    if (sys_err == WSAEWOULDBLOCK)
      return true;
		sys_printf( "TTCPSocket::connectTo( %s ) failed. rc = %08x %s\n", remote.c_str(), rc, getNetErrorStr( rc ) );
		destroy( );
		return false;
	}

	dumpOptions( "connectTo" );

	sys_printf( "Connected to %s\n", remote.c_str() );
	return true;
}
// --------------------------------------
void TTCPSocket::dumpOptions( const char *label ) const {
#ifdef _DEBUG
	assert( isValid() );
	sys_printf( "-----------****----------****----------****------------\n" );
	sys_printf( " Socket %d at %s\n", id, label );

	//int rcv_buf = getOption( SO_RCVBUF );
	//sys_printf( "RCVBUF = %d\n", rcv_buf );

	int no_delay = getOption( TCP_NODELAY );
	sys_printf( "NODELAY = %d\n", no_delay );

	//int max_seg = getOption( TCP_MAXSEG );
	//sys_printf( "TCP_MAXSEG = %d\n", max_seg );

	/*
	int nbio = -1;
	opt_len = sizeof( int );
	rc = sceNetGetsockopt( id, level, SCE_NET_SO_NBIO, &nbio, &opt_len );
	if( rc == 0 ) {
		sys_printf( "NBIO = %d\n", nbio );
	} else {
		sys_printf( "sceNetGetsockopt( NBIO ) failed rc = %08x\n", rc );
	}
	*/

	sys_printf( "-----------****----------****----------****------------\n" );
#endif
}

// --------------------------------------
bool TTCPSocket::accept( TTCPSocket &new_client ) {
	assert( isValid( ) );
	socklen_t bytes = sizeof( local );
	SOCKET_ID rc = sys_accept( id, &new_client.remote.addr, &bytes );
	if( rc >= 0 ) {
		new_client.destroy( );
		new_client.id = rc;
		new_client.setOption( TCP_NODELAY, 1 );
		//new_client.setOption( SO_RCVTIMEO, 1*1000*1000 );
		new_client.dumpOptions( "accept" );
		return true;
	}

	CHECK_RC( "TTCPSocket::accept" );
	return false;
}

// ----------------------------------------------
// TCP only
bool TTCPSocket::send( const void *buf, size_t nbytes ) {
	if( !isValid() )
		return false;

	const char *ptr = (const char *) buf;
	size_t total_bytes_send = 0;
	while( total_bytes_send < nbytes ) {
		auto bytes_sent = sys_send( id, ptr, nbytes - total_bytes_send, 0 );
		if( bytes_sent < 0 ) {
			sys_printf( "sys_send( %d , %d bytes ) failed. rc = %08x (%s)\n", nbytes - total_bytes_send, nbytes, bytes_sent, getSysErrorText( bytes_sent ) );
			return false;
		}
		ptr += bytes_sent;
		total_bytes_send += nbytes;
	}
	return( total_bytes_send == nbytes );
}

struct TGameMsg {
	enum TType {
	  SINGLE_EVENT_TEXT = 0
	, DISABLE_LOGIC_BOX
	, ENABLE_LOGIC_BOX
	, DISABLE_LOGIC_BOX_INPUT
	, ENABLE_LOGIC_BOX_INPUT
	, PAUSE_LOGIC_BOX
	, UNPAUSE_LOGIC_BOX
	, PAUSE_LOGIC_BOX_INPUT
	, UNPAUSE_LOGIC_BOX_INPUT
	, PAUSE_LOGIC_BOX_RENDER
	, UNPAUSE_LOGIC_BOX_RENDER
	, PLAYER_WANTS_TO_MOVE_LEFT
	, PLAYER_WANTS_TO_IDLE
	, PLAYER_WANTS_TO_MOVE_RIGHT
	, PLAYER_STARTS_MOVING_LEFT
	, PLAYER_STOPS
	, PLAYER_STARTS_MOVING_RIGHT
	, PLAYER_SYNC_LOC
	, PLAYER_IOSTATUS
	, PLAYER_TOUCH_STATUS
	, PLAYER_CHANGE_STATE
	, PLAYER_CHANGE_STATE_FROM_CONDITION
	, PLAYER_DAMAGED
	, PLAYER_DIED
	, PLAYER_DIED_LOW
	, PLAYER_STARTS_GRAB
	, SYNC_PING
	, SYNC_PONG
	, DELAY_TO	// Obj: float time_to_launch
	, END_BATTLE
	, END_ROUND
	, TIMEOUT
	, IM_ALIVE

	, EVENT_TYPES_COUNT
	};
	TType    type;
	int      xtra_bytes;
	int			 target_boxes;
	int			 arg;
	int size() const { return sizeof( TGameMsg ) + xtra_bytes; }
};

// ----------------------------------------------
// TCP only
size_t TTCPSocket::recvUpToNBytes( void *buf, size_t max_bytes ) {
	if( !isValid( ) )
		return 0;
	assert( buf );
	auto bytes_read = sys_recv( id, (char *)buf, max_bytes, 0 );
	if( bytes_read < 0 ) {
		sys_printf( "recvUpToNBytes(%d) = %08x\n", max_bytes, bytes_read );
		destroy( );
		return false;
	}
	return bytes_read;
}

// ----------------------------------------------
// TCP only
bool TTCPSocket::recv( void *buf, size_t max_bytes ) {
	if( !isValid() ) 
		return false;
	
	size_t total_bytes = 0;
//	int nretries = 0;
	while( total_bytes < max_bytes ) {
		auto bytes_read = sys_recv( id, ((char *)buf) + total_bytes, max_bytes - total_bytes, 0 );
		if( bytes_read == 0 ) {
      int sys_err = WSAGetLastError();
			sys_printf( "recv(%d) returned 0 bytes: %d\n", max_bytes, sys_err);
			destroy( );
			return false;

		} else if( bytes_read == EWOULDBLOCK ) {
			bytes_read = 0;
			//static const int max_retries = 30;
			//sys_printf( "recv(%d) would block. Try %d/%d\n", max_bytes, nretries, max_retries );
			//++nretries;
			//if( nretries > max_retries ) {
			//	sys_printf( "recv(%d) Too many retries (%d)\n", max_bytes, nretries );
			//	return false;
			//}


		} else if( bytes_read < 0 ) {
			sys_printf( "recv(%d) returned an error: %08x (%s)\n", max_bytes - total_bytes, bytes_read, getSysErrorText( bytes_read ) );
			destroy( );
			return false;

		} else if( bytes_read != ( max_bytes - total_bytes ) ) {
			sys_printf( "recv(%d) returned %d/%d bytes\n", max_bytes, bytes_read, max_bytes - total_bytes );

		} else if( total_bytes != 0 ) { 
			sys_printf( "recv(%d) received last %d/%d bytes\n", max_bytes, bytes_read, max_bytes - total_bytes );
		}
		total_bytes += bytes_read;
	}
	return true;
}

// --------------------------------------
bool TTCPSocket::setOption( int option, int how ) {
	int val = how;
	int level = SOL_SOCKET;
	if( option == TCP_NODELAY )
		level = IPPROTO_TCP;
	int rc = ::setsockopt( id, level, option, (const SYSFN_DATA_TYPE *) &val, sizeof(val));
	CHECK_RC( "setsockopt" );
	return (rc == 0);
}

// --------------------------------------
int TTCPSocket::getOption( int option ) const {
	int val = 0;
	int level = SOL_SOCKET;
	if( option == TCP_NODELAY )
		level = IPPROTO_TCP;
	socklen_t opt_len = sizeof(val);
	int rc = ::getsockopt( id, level, option, (SYSFN_DATA_TYPE *) &val, &opt_len );
	CHECK_RC( "getsockopt" );
	return val;
}

// --------------------------------------
bool TTCPSocket::create( ) {
	assert( !isValid() );

  int os_type = ( getNetMode( ) != NET_MODE_IP4_STANDARD ) ? SOCK_STREAM_P2P : SOCK_STREAM;
	SOCKET_ID rc = sys_socket( AF_INET, os_type, 0, 0 );
	id = rc;
  if( rc < 0 || !isValid( ) ) {
    CHECK_RC( "sys_socket" );
    return false;
  }
	return isValid();
}

bool TTCPSocket::bind( int port ) {

 
  local.fromAnyAddress( port );
	int rc = sys_bind( id, &local.addr, sizeof(local.addr) );
	CHECK_RC( "TTCPSocket::bind" );
  return rc == 0;
}

bool TTCPSocket::listen( ) {
	int rc = sys_listen( id, 5);
	if( rc < 0 ) {
		CHECK_RC( "sceNetListen" );
		destroy( );
	}
	return ( rc == 0 );
}

/*
// --------------------------------------
void TTCPSocket::initAddr( TSockAddress &addr, int port, SceUInt32 address_ip4, int vport ) {
	memset(&addr, 0x00, sizeof(TSockAddress));
	addr.sin_len    = sizeof(TSockAddress);
	addr.sin_family = SCE_NET_AF_INET;

	if( use_p2p_mode ) {
		addr.sin_vport  = sceNetHtons( vport );
		addr.sin_port   = sceNetHtons( port  );
	} else {
		addr.sin_port   = sceNetHtons( port );
	}
	addr.sin_addr.s_addr = address_ip4;			
	trace( "Initializing socket addr to %08x:%d: VPort:%d\n", addr.sin_addr.s_addr, sceNetNtohs( addr.sin_port ), sceNetNtohs( addr.sin_vport ) );
}
*/