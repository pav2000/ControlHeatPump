#ifndef ethernetserver_h
#define ethernetserver_h

#include "Server.h"
#define FAST_LIB     // Ключ для ускорения библиотеки https://geektimes.ru/post/259898/

class EthernetClient;

class EthernetServer : 
public Server {
private:
  uint16_t _port;
  #ifdef FAST_LIB  // Переделка
  void accept_(int sock);
  #else
  void accept();
  #endif
public:
  EthernetServer(uint16_t);
  #ifdef FAST_LIB  // Переделка
  EthernetClient available_(int sock);
  virtual void begin_(int sock);
  #else
  EthernetClient available();
  #endif
  virtual void begin();
  virtual void begin(uint16_t port);   // pav2000
  virtual size_t write(uint8_t);
  virtual size_t write(const uint8_t *buf, size_t size);
  using Print::write;
};

#endif
