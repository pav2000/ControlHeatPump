#include "utility/w5100.h"
#include "utility/socket.h"
extern "C" {
#include "string.h"
}

#include "Ethernet.h"
#include "EthernetClient.h"
#include "EthernetServer.h"

EthernetServer::EthernetServer(uint16_t port)
{
  _port = port;
}
// Смена порта веб сервера
void EthernetServer::begin(uint16_t port)   //pav2000
{
  _port = port;
//  begin();
  for (int sock = 0; sock < MAX_SOCK_NUM; sock++) {
      close(sock);
      EthernetClient client(sock);
      socket(sock, SnMR::TCP, _port, 0);
      listen(sock);
      EthernetClass::_server_port[sock] = _port;
      break;
  }
}

void EthernetServer::begin()
{
  for (int sock = 0; sock < MAX_SOCK_NUM; sock++) {
    EthernetClient client(sock);
    if (client.status() == SnSR::CLOSED) {
      socket(sock, SnMR::TCP, _port, 0);
      listen(sock);
      EthernetClass::_server_port[sock] = _port;
      break;
    }
  }  
}
#ifdef FAST_LIB  // Переделка
void EthernetServer::begin_(int sock) {
   EthernetClient client(sock);
   if (client.status() == SnSR::CLOSED) {
            socket(sock, SnMR::TCP, _port, 0);
            listen(sock);
       EthernetClass::_server_port[sock] = _port;
    }
}
#endif
#ifdef FAST_LIB  // Переделка
void EthernetServer::accept_(int sock) {
  int listening = 0;
  EthernetClient client(sock);

  if (EthernetClass::_server_port[sock] == _port) {
    if (client.status() == SnSR::LISTEN) {
      listening = 1;
    } 
    else if (client.status() == SnSR::CLOSE_WAIT && !client.available()) {
      client.stop();
    }
  } 

  if (!listening) {
    //begin();
    begin_(sock); // added
  }
}
#else
void EthernetServer::accept()
{
  int listening = 0;

  for (int sock = 0; sock < MAX_SOCK_NUM; sock++) {
    EthernetClient client(sock);

    if (EthernetClass::_server_port[sock] == _port) {
      if (client.status() == SnSR::LISTEN) {
        listening = 1;
      } 
      else if (client.status() == SnSR::CLOSE_WAIT && !client.available()) {
        client.stop();
      }
    } 
  }

  if (!listening) {
    begin();
  }
}
#endif
#ifdef FAST_LIB  // Переделка
EthernetClient EthernetServer::available_(int sock) {
  int listening = 0;             // pav2000 убрал вызовы функции работаю через регистры
  EthernetClient client(sock);

  if (EthernetClass::_server_port[sock] == _port) {
    if (W5100.readSnSR(sock) == SnSR::LISTEN) {
      listening = 1;
    }
    else if (W5100.readSnSR(sock) == SnSR::CLOSE_WAIT && !W5100.getRXReceivedSize(sock)) {
      client.stop();
    }
  }
  if (!listening) begin_(sock);

  if (EthernetClass::_server_port[sock] == _port &&
      (W5100.readSnSR(sock) == SnSR::ESTABLISHED ||
       W5100.readSnSR(sock) == SnSR::CLOSE_WAIT)) {
    if (W5100.getRXReceivedSize(sock)) {
      return client;
    }
  }

//  accept_(sock);    // pav2000 старая версия
//  EthernetClient client(sock);
//  if (EthernetClass::_server_port[sock] == _port &&
//      (client.status() == SnSR::ESTABLISHED ||
//       client.status() == SnSR::CLOSE_WAIT)) {
//    if (client.available()) {
//      return client;
//    }
//  }
  return EthernetClient(MAX_SOCK_NUM);
}
#else
EthernetClient EthernetServer::available()
{
  accept();

  for (int sock = 0; sock < MAX_SOCK_NUM; sock++) {
    EthernetClient client(sock);
    if (EthernetClass::_server_port[sock] == _port &&
        (client.status() == SnSR::ESTABLISHED ||
         client.status() == SnSR::CLOSE_WAIT)) {
      if (client.available()) {
        // XXX: don't always pick the lowest numbered socket.
        return client;
      }
    }
  }

  return EthernetClient(MAX_SOCK_NUM);
}
#endif

size_t EthernetServer::write(uint8_t b) 
{
  return write(&b, 1);
}

#ifdef FAST_LIB  // Переделка
size_t EthernetServer::write(const uint8_t *buffer, size_t size) {
size_t n = 0;
for (int sock = 0; sock < MAX_SOCK_NUM; sock++) {
          accept_(sock); // added
          EthernetClient client(sock);

         if (EthernetClass::_server_port[sock] == _port &&
          client.status() == SnSR::ESTABLISHED) {
          n += client.write(buffer, size);
         }
    }
return n;
}
#else
size_t EthernetServer::write(const uint8_t *buffer, size_t size) 
{
  size_t n = 0;
  
  accept();

  for (int sock = 0; sock < MAX_SOCK_NUM; sock++) {
    EthernetClient client(sock);

    if (EthernetClass::_server_port[sock] == _port &&
      client.status() == SnSR::ESTABLISHED) {
      n += client.write(buffer, size);
    }
  }
  
  return n;
}
#endif
