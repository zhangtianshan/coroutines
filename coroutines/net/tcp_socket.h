#ifndef INC_NET_SOCKET_H_
#define INC_NET_SOCKET_H_

#include "net_platform.h"

struct TTCPSocket {

	// --------------------------------------
	SOCKET_ID    id;
	TNetAddress  local;
	TNetAddress  remote;
 
	static const SOCKET_ID SOCKET_NOT_INITIALIZED = -1;
	static const int       default_tcp_app_port           = 6890;

	// --------------------------------------
	TTCPSocket( );
	// Don't call the destroy in the destructor

	bool isValid( ) const { return id != SOCKET_NOT_INITIALIZED; }
	void destroy( );
	void disconnect( );

	// ----------------------------------------------
	bool createServer( int port = default_tcp_app_port );
	bool connectTo( const TNetAddress &remote_server, int timeout_sec = 1, bool is_blocking = false );
	bool accept( TTCPSocket &new_client );


	size_t recvUpToNBytes( void *buf, size_t max_bytes );

	// ----------------------------------------------
	// TCP Template I/O
	template< class T>
	bool send( const T &obj ) {
		return send( &obj, sizeof( T ) );
	}
	template< class T>
	bool recv( T &obj ) {
		return recv( &obj, sizeof( T ) );
	}
  
	// ----------------------------------------------
	// TCP only
	bool send( const void *buf, size_t nbytes );
	bool recv( void *buf, size_t max_bytes );

	// --------------------------------------
	bool setOption( int option, int how );
	int  getOption( int option ) const;
	void dumpOptions( const char *label ) const;

	static void setP2PMode( bool how );
	static void setInAdhocMode( bool how );

private:

	// --------------------------------------
	bool create( );
	bool bind( int port );
	bool listen( );


};

#endif