/*
 * Copyright (c) 2010 by WIZnet <support@wiznet.co.kr>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <string.h>
#include "utility/w5100.h"
#include "utility/spi_dma.h"

#if defined(W5500_ETHERNET_SHIELD)
// W5500 controller instance
W5500Class W5100;

void W5500Class::init(void)
{
    delay(300);

#if 0
  SPI.begin(SPI_CS);
  SPI.setClockDivider(SPI_CS, 3);
  SPI.setDataMode(SPI_CS, SPI_MODE0);
#endif
	spiBegin();
	spiInit(SPI_RATE);

  writeMR(1<<RST);

    for (int i=0; i<MAX_SOCK_NUM; i++) {
        uint8_t cntl_byte = (0x0C + (i<<5));
        write( 0x1E, cntl_byte, 2); //0x1E - Sn_RXBUF_SIZE
        write( 0x1F, cntl_byte, 2); //0x1F - Sn_TXBUF_SIZE
    }
}

uint16_t W5500Class::getTXFreeSize(SOCKET s)
{
  uint16_t val=0, val1=0;
  do {
    val1 = readSnTX_FSR(s);
    if (val1 != 0)
      val = readSnTX_FSR(s);
  }
  while (val != val1);
  return val;
}

uint16_t W5500Class::getRXReceivedSize(SOCKET s)
{
  uint16_t val=0,val1=0;
  do {
    val1 = readSnRX_RSR(s);
    if (val1 != 0)
      val = readSnRX_RSR(s);
  }
  while (val != val1);
  return val;
}

void W5500Class::send_data_processing(SOCKET s, const uint8_t *data, uint16_t len)
{
  // This is same as having no offset in a call to send_data_processing_offset
  send_data_processing_offset(s, 0, data, len);
}

void W5500Class::send_data_processing_offset(SOCKET s, uint16_t data_offset, const uint8_t *data, uint16_t len)
{
    uint16_t ptr = readSnTX_WR(s);
    uint8_t cntl_byte = (0x14+(s<<5));
    ptr += data_offset;
    write(ptr, cntl_byte, data, len);   // передача пакета
    ptr += len;
    writeSnTX_WR(s, ptr);
}

void W5500Class::recv_data_processing(SOCKET s, uint8_t *data, uint16_t len, uint8_t peek)
{
    uint16_t ptr;
    ptr = readSnRX_RD(s);

    read_data(s, ptr, data, len);
    if (!peek)
    {
        ptr += len;
        writeSnRX_RD(s, ptr);
    }
}

void W5500Class::read_data(SOCKET s, volatile uint16_t src, volatile uint8_t *dst, uint16_t len)
{
    uint8_t cntl_byte = (0x18+(s<<5));
    read((uint16_t)src , cntl_byte, (uint8_t *)dst, len);
}

uint8_t W5500Class::write(uint16_t _addr, uint8_t _cb, uint8_t _data)
{
    digitalWriteDirectARM(SS,LOW);
    spiSend(_addr >> 8);
    spiSend(_addr & 0xFF);
    spiSend(_cb);
    spiSend(_data);
    digitalWriteDirectARM(SS,HIGH);
    return 1;
}
// Передать пакет, есть отличия от 5200 чипа - длина передаваемых данных не передается
uint16_t W5500Class::write(uint16_t _addr, uint8_t _cb, const uint8_t *_buf, uint16_t _len)
{
    digitalWriteDirectARM(SS,LOW);
    spiSend(_addr >> 8);
    spiSend(_addr & 0xFF);
    spiSend(_cb);
   // spiSend((0x80 | ((_len & 0x7F00) >> 8)));     // для w5500 длина не передается
   // spiSend(_len & 0x00FF);
    spiSend(_buf,_len);
    digitalWriteDirectARM(SS,HIGH);
  return _len;
}

uint8_t W5500Class::read(uint16_t _addr, uint8_t _cb)
{
digitalWriteDirectARM(SS,LOW);
    spiSend(_addr >> 8);
    spiSend(_addr & 0xFF);
    spiSend(_cb);
    uint8_t _data = spiRec();
    digitalWriteDirectARM(SS,HIGH);
  return _data;
}

uint16_t W5500Class::read(uint16_t _addr, uint8_t _cb, uint8_t *_buf, uint16_t _len)
{
digitalWriteDirectARM(SS,LOW);
    spiSend(_addr >> 8);
    spiSend(_addr & 0xFF);
    spiSend( _cb);
  //  spiSend((0x00 | ((_len & 0x7F00) >> 8)));  // для w5500 длина не передается
  //  spiSend(_len & 0x00FF);
    spiRec(_buf,_len);
    digitalWriteDirectARM(SS,HIGH);
  return _len;
}

void W5500Class::execCmdSn(SOCKET s, SockCMD _cmd) {
    writeSnCR(s, _cmd);    // Send command to socket
    while (readSnCR(s));   // Wait for command to complete
}
#endif
