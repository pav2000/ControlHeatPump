#include "utility/w5100.h"
#include "utility/socket.h"

extern "C" {
  #include "string.h"
}

#include "Arduino.h"
#include "Ethernet.h"
#include "EthernetClient.h"
#include "EthernetServer.h"
#include "Dns.h"

#include "FreeRTOS_ARM.h"
#define RTOS_delay(ms) { if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) vTaskDelay(ms/portTICK_PERIOD_MS); else delay(ms); }

uint16_t EthernetClient::_srcport = 1024;

EthernetClient::EthernetClient() : _sock(MAX_SOCK_NUM) {
}

EthernetClient::EthernetClient(uint8_t sock) : _sock(sock) {
}

int EthernetClient::connect(const char* host, uint16_t port) {
  // Look up the host first
  int ret = 0;
  _offset = 0;
  DNSClient dns;
  IPAddress remote_addr;
  dns.begin(Ethernet.dnsServerIP());
  ret = dns.getHostByName(host, remote_addr);

  if (ret == 1) {
    return connect(remote_addr, port);
  } else {
    return ret;
  }
}

int EthernetClient::connect(IPAddress ip, uint16_t port) {
  _offset = 0;
  if (_sock != MAX_SOCK_NUM) {
 //   Serial.println("EXCEPTION: Not all sockets are available");
    return 0;
   }
  for (int i = 0; i < MAX_SOCK_NUM; i++) {
    uint8_t s = W5100.readSnSR(i);
    if (s == SnSR::CLOSED || s == SnSR::FIN_WAIT || s == SnSR::CLOSE_WAIT) {
      _sock = i;
      break;
    }
  }

  if (_sock == MAX_SOCK_NUM) {
//    Serial.println("EXCEPTION: No sockets were available!");
    return 0;
    }

  _srcport++;
  if (_srcport == 0) _srcport = 1024;  // порт последовательно увеличиваем до 65345 потом 1024
  socket(_sock, SnMR::TCP, _srcport, 0);
// socket(_sock, SnMR::TCP, _srcport, SnMR::ND); //pav2000

  if (!::connect(_sock, rawIPAddress(ip), port)) {
    _sock = MAX_SOCK_NUM;
//    Serial.println("EXCEPTION: in Ethernet connect library");
    return 0;
  }
//   Serial.println(ip);

  uint32_t start = millis();
  while (status() != SnSR::ESTABLISHED) {
    RTOS_delay(1);

    if (status() == SnSR::CLOSED) {
      _sock = MAX_SOCK_NUM;
//      Serial.println("EXCEPTION: Socket was closed after being established");
      return 0;
    }
    if(millis() - start > 2000) return 0; // timeout
  }

  return 1;
}

// Открыть клиента по NAME DNS на конкретный сокет, сокет перед этим сбрасывается
int EthernetClient::connect(const char *host, uint16_t port,uint8_t sock)// pav2000
{
  // Look up the host first
  int ret = 0;
  DNSClient dns;
  IPAddress remote_addr;
  dns.begin(Ethernet.dnsServerIP());
  ret = dns.getHostByName(host, remote_addr,sock); // Запрос через конкретный сокет
  if (ret == 1) { return connect(remote_addr, port,sock); }
  else { return ret; }
}

// Открыть клиента  по IP на конкретный сокет, сокет перед этим сбрасывается
int EthernetClient::connect(IPAddress ip, uint16_t port,uint8_t sock)// pav2000
 {
  _offset = 0;
   // Сброс сокета  возможно после сброса нужна задержка для выполения сброса???
  W5100.execCmdSn(sock, Sock_CLOSE);
  W5100.writeSnMR(sock, 0x00);
  W5100.writeSnIR(sock, 0xFF);
  _sock=sock;  // Запомнить сокет
  _srcport++;
  if (_srcport == 0) _srcport = 1024;
  socket(_sock, SnMR::TCP, _srcport, 0);   // инициализировать сокет

  if (!::connect(_sock, rawIPAddress(ip), port)) { // соединение сокета
    _sock = MAX_SOCK_NUM;
     return 0;
  }
//   Serial.println(ip);

  uint32_t start = millis();
  while (status() != SnSR::ESTABLISHED) {
    RTOS_delay(1);

    if (status() == SnSR::CLOSED) {
      _sock = MAX_SOCK_NUM;
      return 0;
    }
    if(millis() - start > 2000) return 0; // timeout
  }
  return 1;
}

size_t EthernetClient::write(uint8_t b) {
  return write(&b, 1);
}

size_t EthernetClient::write(const uint8_t *buf, size_t size) {
  _offset = 0;
  if (_sock == MAX_SOCK_NUM) {
    setWriteError();
    return 0;
  }
  if(!send(_sock, buf, size)) {
    setWriteError();
    return 0;
  }
  return size ? size : 1;
}

size_t EthernetClient::write_buffer(const uint8_t *buffer, size_t size)
{
  uint16_t bytes_written = bufferData(_sock, _offset, buffer, size);
  _offset += bytes_written;
  return bytes_written;
}

int EthernetClient::available() {
  if (_sock != MAX_SOCK_NUM)
    return W5100.getRXReceivedSize(_sock);
  return 0;
}

int EthernetClient::read() {
  uint8_t b;
  if ( recv(_sock, &b, 1) > 0 )
  {
    // recv worked
    return b;
  }
  else
  {
    // No data available
    return -1;
  }
}

int EthernetClient::read(uint8_t *buf, size_t size) {
  return recv(_sock, buf, size);
}

int EthernetClient::peek() {
  uint8_t b;
  // Unlike recv, peek doesn't check to see if there's any data available, so we must
  if (!available())
    return -1;
  ::peek(_sock, &b);
  return b;
}

void EthernetClient::flush() {
  ::flush(_sock);
}

void EthernetClient::stop() {
  if (_sock == MAX_SOCK_NUM)
    return;

  // attempt to close the connection gracefully (send a FIN to other side)
  disconnect(_sock);
  unsigned long start = millis();

  // wait a second for the connection to close
  while (status() != SnSR::CLOSED && millis() - start < 1000)
    RTOS_delay(1);

  // if it hasn't closed, close it forcefully
  if (status() != SnSR::CLOSED)
    close(_sock);

  EthernetClass::_server_port[_sock] = 0;
  _sock = MAX_SOCK_NUM;
}

uint8_t EthernetClient::connected() {
  if (_sock == MAX_SOCK_NUM) return 0;
  
//  uint8_t s = status();              // pav2000
//  return !(s == SnSR::LISTEN || s == SnSR::CLOSED || s == SnSR::FIN_WAIT || /
//    (s == SnSR::CLOSE_WAIT && !available()));
  uint8_t s = W5100.readSnSR(_sock); // pav2000 немного ускорим и стек уменьшим
  return !(s == SnSR::LISTEN || s == SnSR::CLOSED || s == SnSR::FIN_WAIT ||
    (s == SnSR::CLOSE_WAIT && !W5100.getRXReceivedSize(_sock)));
}

uint8_t EthernetClient::status() {
  if (_sock == MAX_SOCK_NUM) return SnSR::CLOSED;
  return W5100.readSnSR(_sock);
}

// the next function allows us to use the client returned by
// EthernetServer::available() as the condition in an if-statement.

EthernetClient::operator bool() {
  return _sock != MAX_SOCK_NUM;
}

bool EthernetClient::operator==(const EthernetClient& rhs) {
  return _sock == rhs._sock && _sock != MAX_SOCK_NUM && rhs._sock != MAX_SOCK_NUM;
}

// Возврат объема сободного места в буфере  на запись
int EthernetClient::get_freeTX() {
    if (_sock != MAX_SOCK_NUM)
        return W5100.getTXFreeSize(_sock);
   return 0;
}

// Возврат объема данных в буфере  на чтение
int EthernetClient::get_ReceivedSizeRX() {
    if (_sock != MAX_SOCK_NUM)
        return W5100.getRXReceivedSize(_sock);
   return 0;
}
