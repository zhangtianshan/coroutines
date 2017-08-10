#pragma once

// --------------------------------------------------------------------
// Platform independent net address. Because TNetAddress has different
// sizes in PSP2 and PS3
struct TNetBasicAddress {
  union {
    unsigned ipv4;                    // In host format
    unsigned char ipv4_parts[4];      // in PSVita  part[0] = 192, part[1] = 168, part[2] = 1, part[3] = 54
  };
  int      port;                      // In host format
  int      vport;                     // In host format
  int      unused;
  TNetBasicAddress() : port(0), vport(0), unused(0) {
    ipv4 = 0;
  }
  const char *c_str() const;
};

// --------------------------------------------------------------------
struct TNetAddress {
private:
  void from(int port, unsigned ip4_in_host_format);

public:
  TSockAddress addr;
  void fromAnyAddress(int port);												// accept connections from any addr
  bool fromStr(const char *addr_str, int port);					// init from a string and port number
  bool fromMyAddress(int port);													// init from local address
  bool fromAddressAndPort(const TSockAddress &addr, int port);	// init from another address changing the port
  void toBroadcast(int port);														// broadcast address in that port
  const char *c_str() const;															// Returns 127.0.0.1:8080 for example
  const char *ip_str() const;															// Returns 127.0.0.1 as string
  unsigned short getPort() const;												// Returns 8000 in host format
  bool isEqual(const TNetAddress &other_addr) const;		// Compares ip4 and port number
  bool isAddressEqual(const TNetAddress &other_addr) const;		// Compares just ip4 
  bool isBroadcast() const;
  void swapPortAndVPort();
  static bool areEqual(const TSockAddress &addr1, const TSockAddress &addr2);
  void setPort(unsigned short port);										// port in host format. Will set the game app port, not the p2p port

  void getNetBasicAddress(TNetBasicAddress &basic_addr) const;
  void fromBasicAddress(const TNetBasicAddress &basic_addr);
};

