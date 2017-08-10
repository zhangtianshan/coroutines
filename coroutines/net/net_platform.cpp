#define _WINSOCK_DEPRECATED_NO_WARNINGS

// System
// This include must be inserted before the windows.h to avoid including the old winsocks.h
#include <string>
#include <cassert>
#include <winsock2.h>
#include <windows.h>

#include "net_sys_defines.h"
#include "net_platform.h"
//#include "utils/utils.h"      // getSysErrorText

#ifdef PLATFORM_PSP
#error Use the PSP/Common/net/net_platform_psp.cpp instead of this file
#endif

using namespace std;

static TNetMode net_mode = NET_MODE_IP4_STANDARD;

// -----------------------------------------------
void setNetMode( TNetMode new_mode ) {
  net_mode = new_mode;
}
 
TNetMode getNetMode( ) { return net_mode; }

// -----------------------------------------------
bool initNetPlatform( ) {
#ifdef PLATFORM_WINDOWS
  static bool initialized = false;
  if( !initialized ) {
    initialized = true;
    WSADATA wsaData;
    return WSAStartup(0x101,&wsaData) == 0;
  }
#endif

  return true;
}

// ---------------------------------------------------------
const char *getNetErrorStr( int rc ) {
#if defined( PLATFORM_PSP2 ) || defined( PLATFORM_PS4 )
       if( rc == SCE_NET_EAFNOSUPPORT )   rc = SCE_NET_ERROR_EAFNOSUPPORT;
  else if( rc == SCE_NET_EADDRNOTAVAIL )  rc = SCE_NET_ERROR_EADDRNOTAVAIL;
  else if( rc == SCE_NET_ENOBUFS )        rc = SCE_NET_ERROR_ENOBUFS;
  else if( rc == SCE_NET_EHOSTDOWN )      rc = SCE_NET_ERROR_EHOSTDOWN;
  else if( rc == SCE_NET_EHOSTUNREACH )   rc = SCE_NET_ERROR_EHOSTUNREACH;
  else if( rc == SCE_NET_ERETURN )        rc = SCE_NET_ERROR_ERETURN;
  else if( rc == SCE_NET_EBADF )          rc = SCE_NET_ERROR_EBADF;

#if defined( PLATFORM_PSP2 )
  else if( rc == SCE_NET_EIPADDRCHANGED ) rc = SCE_NET_ERROR_EIPADDRCHANGED;
#endif
  
#elif PLATFORM_PS3
  if( rc == SYS_NET_EHOSTDOWN )    rc = SYS_NET_ERROR_ESHUTDOWN;
#endif
  return "Unknown error"; // getSysErrorText(rc);
}

// -----------------------------------------------
// port and vport are in host format
void TNetAddress::from( int port, unsigned ip4_in_host_format, int vport ) {
  assert( port > 0 && port < 65535 );

  memset( &addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
#if defined( NET_ADDR_HAS_SIN_LEN_MEMBER )
  addr.sin_len    = sizeof( addr );
#endif
  addr.sin_addr.s_addr = htonl(ip4_in_host_format);
  addr.sin_port = htons( port );

#if NET_P2P_SUPPORT_ENABLED
  if( getNetMode( ) != NET_MODE_IP4_STANDARD ) {
    assert( vport > 0 && vport < 65535 );
    addr.sin_vport = htons( vport );
  }
  if( getNetMode( ) == NET_MODE_PSN_P2P )
    swapPortAndVPort( );

#endif
}

void TNetAddress::setPort( unsigned short new_app_port ) {
  addr.sin_port = htons( new_app_port );
}

// This is only required for certain operations in udp
void TNetAddress::swapPortAndVPort( ) {
#if NET_P2P_SUPPORT_ENABLED
  unsigned short tmp = addr.sin_port;
  addr.sin_port = addr.sin_vport;
  addr.sin_vport = tmp;
#endif
}

void TNetAddress::fromAnyAddress( int port ) {
  from( port, INADDR_ANY, default_vport );
}

void TNetAddress::toBroadcast( int port ) {
  from( port, INADDR_BROADCAST, default_vport );
}

bool TNetAddress::isBroadcast( ) const {
  return addr.sin_addr.s_addr == 0xffffffff;
}

bool TNetAddress::fromMyAddress( int port ) {
#ifdef PLATFORM_PSP2

  if( getNetMode( ) == NET_MODE_IP4_STANDARD ) {
    SceNetCtlInfo info;
    int rc = sceNetCtlInetGetInfo( SCE_NET_CTL_INFO_IP_ADDRESS, &info );
    if( rc < 0 ) {
      sys_warn( "sceNetCtlInetGetInfo failed rc = %08x (%s)\n", rc, getSysErrorText( rc ) );
      return false;
    }
    return fromStr( info.ip_address, port ); 

  } else if( getNetMode( ) == NET_MODE_ADHOC_P2P ) {
    SceNetInAddr addr;
    int rc = sceNetCtlAdhocGetInAddr( &addr );
      if( rc < 0 ) {
        sys_warn( "sceNetCtlAdhocGetInAddr failed rc = %08x (%s)\n", rc, getSysErrorText( rc ) );
      return false;
      }
    from( port, ntohl( addr.s_addr ), default_vport );
  
  } else if( getNetMode( ) == NET_MODE_PSN_P2P ) {


  }
  return true;

#elif PLATFORM_PS3

  CellNetCtlInfo info;
  int rc = cellNetCtlGetInfo( CELL_NET_CTL_INFO_IP_ADDRESS, &info );
  if( rc < 0 ) {
    sys_warn( "cellNetCtlGetInfo failed rc = %08x (%s)\n", rc, getSysErrorText( rc ) );
    return false;
  }
  return fromStr( info.ip_address, port ); 
  
#elif PLATFORM_PS4

  SceNetCtlInfo info;
  int rc = sceNetCtlGetInfo( SCE_NET_CTL_INFO_IP_ADDRESS, &info );
  if( rc < 0 ) {
    sys_warn( "sceNetCtlGetInfo failed rc = %08x (%s)\n", rc, getSysErrorText( rc ) );
    return false;
  }

  return fromStr( info.ip_address, port ); 

#else

  char szHostName[255];
  gethostname(szHostName, 255);
  struct hostent *host_entry = gethostbyname( szHostName );
  if( !host_entry || !host_entry->h_addr_list )
    return false;
  unsigned ip4 = *(unsigned int *) (*host_entry->h_addr_list);
  from( port, ntohl( ip4 ), default_vport );
  return true;

#endif

}

bool TNetAddress::fromStr( const char *addr_str, int port ) {
  fromAnyAddress( port );
  return inet_pton( AF_INET, addr_str, &addr.sin_addr ) == 1;
}

// We keep the vport, but allow to change the port, which is app specific.
// When using PSN, the vport comes from the signalling, instead of being
// the default_vport, but the port is still the application specific port.
bool TNetAddress::fromAddressAndPort( const TSockAddress &addr, int port ) {

#if NET_P2P_SUPPORT_ENABLED
  // Recover it, we have stored in network format, recover it to host
  // as the 'from' functions requires it in host format
  unsigned short vport = ntohs( addr.sin_vport );
#else
  unsigned short vport = default_vport;
#endif

  // Recover the IPv4 in host format
  from( port, ntohl( addr.sin_addr.s_addr ), vport );
  return true;
}

bool TNetAddress::isAddressEqual( const TNetAddress &other_addr ) const {
  return addr.sin_addr.s_addr == other_addr.addr.sin_addr.s_addr;
}

bool TNetAddress::isEqual( const TNetAddress &other_addr ) const {
  return areEqual( addr, other_addr.addr );
}

bool TNetAddress::areEqual( const TSockAddress &addr1, const TSockAddress &addr2 ) {
  return addr1.sin_port        == addr2.sin_port
      && addr1.sin_addr.s_addr == addr2.sin_addr.s_addr;
}

unsigned short TNetAddress::getPort( ) const {
  return ntohs( addr.sin_port );
}

const char *TNetAddress::c_str() const {
  static char str[ INET_ADDRSTRLEN + 8 * 2 ];
  memset( str, 0, sizeof( str ) );
  inet_ntop( addr.sin_family, (SYSNTOP_DATA_TYPE) &addr.sin_addr, str, INET_ADDRSTRLEN);
  snprintf( str, sizeof( str )-1, "%s:%d", ip_str(), getPort( ) );
#if NET_P2P_SUPPORT_ENABLED
  std::snprintf( str + std::strlen( str ), 8, ":%d", ntohs( addr.sin_vport ) );
#endif
  return str;
}

const char *TNetAddress::ip_str() const {
  if( addr.sin_family == 0 )
    return "0:0:0:0";
  static char str[ INET_ADDRSTRLEN ];
  memset( str, 0, sizeof( str ) );
  inet_ntop( addr.sin_family, (SYSNTOP_DATA_TYPE) &addr.sin_addr, str, INET_ADDRSTRLEN);
  return str;
}

// ----------------------------------------------------------------------------------
void TNetAddress::getNetBasicAddress( TNetBasicAddress &basic_addr ) const {
  basic_addr.ipv4  = ntohl( addr.sin_addr.s_addr );
  basic_addr.port  = getPort( );
#if NET_P2P_SUPPORT_ENABLED
  basic_addr.vport = htons( addr.sin_vport );
#endif
}

void TNetAddress::fromBasicAddress( const TNetBasicAddress &basic_addr ) {
  from( basic_addr.port, basic_addr.ipv4, basic_addr.vport );
}

const char *TNetBasicAddress::c_str() const {
//  static int counter = 0;
//  static const int max_strs = 4;
//  static TStr32 str[ max_strs ];
//  TStr32 &s = str[ counter ];
//  counter = ( counter + 1 ) % max_strs;
//#ifdef PLATFORM_IS_BIG_ENDIAN
//  s.format( "%d.%d.%d.%d:%d:%d", ipv4_parts[0], ipv4_parts[1], ipv4_parts[2], ipv4_parts[3], port, vport );
//#else
//  s.format( "%d.%d.%d.%d:%d:%d", ipv4_parts[3], ipv4_parts[2], ipv4_parts[1], ipv4_parts[0], port, vport );
//#endif
//  return s.c_str();
  return ":";
}
