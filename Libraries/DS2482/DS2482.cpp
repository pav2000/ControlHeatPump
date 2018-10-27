/*
  DS2482 library for Arduino

  Modified by vad7

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have readd a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

	crc code is from OneWire library

	-Updates:
		* fixed wireReadByte busyWait (thanks Mike Jackson)
		* Modified search function (thanks Gary Fariss)

*/
#include "Arduino.h"

#include "DS2482.h"
#include "WireSam.h"

DS2482::DS2482(uint8_t addr)
{
    mAddress = addr;
    CurrPtr = 0;
//    mTimeout = 0;
}

//-------helpers
__attribute__((always_inline)) inline void DS2482::begin(void)
{
	Wire.beginTransmission(mAddress);
}

// return 1 when success
__attribute__((always_inline)) inline uint8_t DS2482::end(void)
{
	return !Wire.endTransmission();
}

// Simply starts and ends an Wire transmission
// If no devices are present, this returns false
// Проверка наличия чипа на шине (читается статус) если чип найден возврат true
uint8_t DS2482::check_presence(void)
{
	uint8_t status = read_status(true);
	if((status==0x00)||(status==DS2482_I2C_ERROR)) return false; else return true;
}

// return 1 - success
__attribute__((always_inline)) inline uint8_t DS2482::setReadPtr(uint8_t readPtr)
{
	if(CurrPtr == readPtr) return 1;
	begin();
	Wire.write(DS2482_COMMAND_SRP);
	Wire.write(readPtr);
	uint8_t res = end();
	if(res) CurrPtr = readPtr;
	return res;
}

int16_t DS2482::readByte(void)
{
	Wire.requestFrom(mAddress,(uint8_t)1);
	return Wire.read();
}

// return status or DS2482_I2C_ERROR
__attribute__((always_inline)) inline uint8_t DS2482::read_status(bool setPtr)
{
	if (setPtr) {
		if(!setReadPtr(DS2482_POINTER_STATUS)) return DS2482_I2C_ERROR;
	}

	return readByte();
}

// return status or  DS2482_I2C_ERROR
uint8_t DS2482::busyWait(bool setReadPtr)
{
	uint8_t status;
	int loopCount = 100;
	while((status = read_status(setReadPtr)) & DS2482_STATUS_BUSY)
	{
		if(status == DS2482_I2C_ERROR) break;
		if (--loopCount <= 0)
		{
//			mTimeout = 1;
			break;
		}
		delayMicroseconds(20);
		setReadPtr = false;
	}
	return status;
}

//Сброс чипа ds2482, 1 - ok
uint8_t DS2482::reset_bridge(void)
{
//	mTimeout = 0;
	begin();
	Wire.write(0xf0);
	uint8_t res = end();
	CurrPtr = res ? 0xF0 : 0; // Status Register
	return res;
}

// Return 1 when OK
uint8_t DS2482::configure(uint8_t config)
{
	if(busyWait(true) == DS2482_I2C_ERROR) return false;
	begin();
	Wire.write(0xd2);
	Wire.write(config | (~config)<<4);
	if(!end()) return false;
	
	return readByte() == config;
}

// return 1 when success
uint8_t DS2482::select_channel(uint8_t channel)
{
	uint8_t ch, ch_read;

	switch (channel)
	{
		case 0:
		default:
			ch = 0xf0;
			ch_read = 0xb8;
			break;
		case 1:
			ch = 0xe1;
			ch_read = 0xb1;
			break;
		case 2:
			ch = 0xd2;
			ch_read = 0xaa;
			break;
		case 3:
			ch = 0xc3;
			ch_read = 0xa3;
			break;
		case 4:
			ch = 0xb4;
			ch_read = 0x9c;
			break;
		case 5:
			ch = 0xa5;
			ch_read = 0x95;
			break;
		case 6:
			ch = 0x96;
			ch_read = 0x8e;
			break;
		case 7:
			ch = 0x87;
			ch_read = 0x87;
			break;
	};

	if(busyWait(true) == DS2482_I2C_ERROR) return false;
	begin();
	Wire.write(0xc3);
	Wire.write(ch);
	if(!end()) return false;
	if(busyWait() == DS2482_I2C_ERROR) return false;

	uint8_t check = readByte();

	return check == ch_read;
}


// Сброс шины OneWire, return 1 if presence,
uint8_t DS2482::reset(void)
{
	if(busyWait(true) == DS2482_I2C_ERROR) return false;
	begin();
	Wire.write(0xb4);
	if(!end()) return false;
	uint8_t status = busyWait();
	return status != DS2482_I2C_ERROR && (status & DS2482_STATUS_PPD) ? true : false;
}


// return 1 when success
uint8_t DS2482::write(uint8_t b)
{
	if(busyWait(true) == DS2482_I2C_ERROR) return false;
	begin();
	Wire.write(0xa5);
	Wire.write(b);
	return end();
}

// return 1 when success
int16_t DS2482::read(void)
{
	if(busyWait(true) == DS2482_I2C_ERROR) return -2;
	begin();
	Wire.write(0x96);
	if(!end()) return -3;
	if(busyWait() == DS2482_I2C_ERROR) return -4;
	if(setReadPtr(DS2482_POINTER_DATA)) return readByte();
	return -5;
}

// return 1 when success
uint8_t DS2482::write_bit(uint8_t bit)
{
	if(busyWait(true) == DS2482_I2C_ERROR) return false;
	begin();
	Wire.write(0x87);
	Wire.write(bit ? 0x80 : 0);
	return end();
}

uint8_t DS2482::read_bit(void)
{
	write_bit(1);
	uint8_t status = busyWait(true);
	return status & DS2482_STATUS_SBR ? 1 : 0;
}

void DS2482::skip(void)
{
	write(0xcc);
}

void DS2482::select(uint8_t rom[8])
{
	write(0x55);
	for (int i=0;i<8;i++)
		write(rom[i]);
}

#if ONEWIRE_SEARCH
void DS2482::reset_search(void)
{
	searchExhausted = 0;
	searchLastDisrepancy = 0;

	for(uint8_t i = 0; i<8; i++)
		searchAddress[i] = 0;
}

uint8_t DS2482::search(uint8_t *newAddr)
{
	uint8_t i;
	uint8_t direction;
	uint8_t last_zero=0;

	if (searchExhausted)
		return 0;

	if (!reset())
		return 0;

	if(busyWait(true) == DS2482_I2C_ERROR) return 0;
	write(0xf0);

	for(i=1;i<65;i++)
	{
		int romByte = (i-1)>>3;
		int romBit = 1<<((i-1)&7);

		if (i < searchLastDisrepancy)
			direction = searchAddress[romByte] & romBit;
		else
			direction = i == searchLastDisrepancy;

		if(busyWait() == DS2482_I2C_ERROR) return 0;
		begin();
		Wire.write(0x78);
		Wire.write(direction ? 0x80 : 0);
		if(!end()) return 0;
		uint8_t status = busyWait();
		if(status == DS2482_I2C_ERROR) return 0;

		uint8_t id = status & DS2482_STATUS_SBR;
		uint8_t comp_id = status & DS2482_STATUS_TSB;
		direction = status & DS2482_STATUS_DIR;

		if (id && comp_id)
			return 0;
		else
		{
			if (!id && !comp_id && !direction)
				last_zero = i;
		}

		if (direction)
			searchAddress[romByte] |= romBit;
		else
			searchAddress[romByte] &= (uint8_t)~romBit;
	}

	searchLastDisrepancy = last_zero;

	if (last_zero == 0)
		searchExhausted = 1;

	for (i=0;i<8;i++)
		newAddr[i] = searchAddress[i];

	return 1;
}
#endif

#if ONEWIRE_CRC8_TABLE
// This table comes from Dallas sample code where it is freely reusable,
// though Copyright (C) 2000 Dallas Semiconductor Corporation
static const uint8_t PROGMEM dscrc_table[] = {
      0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
    157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
     35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
    190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
     70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
    219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
    101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
    248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
    140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
     17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
    175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
     50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
    202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
     87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
    233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
    116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};

//
// Compute a Dallas Semiconductor 8 bit CRC. These show up in the ROM
// and the registers.  (note: this might better be done without to
// table, it would probably be smaller and certainly fast enough
// compared to all those delayMicrosecond() calls.  But I got
// confused, so I use this table from the examples.)
//
uint8_t DS2482::crc8(const uint8_t *addr, uint8_t len)
{
	uint8_t crc = 0;

	while (len--) {
		crc = pgm_read_byte(dscrc_table + (crc ^ *addr++));
	}
	return crc;
}
#else
//
// Compute a Dallas Semiconductor 8 bit CRC directly.
// this is much slower, but much smaller, than the lookup table.
//
uint8_t DS2482::crc8(const uint8_t *addr, uint8_t len)
{
	uint8_t crc = 0;

	while (len--) {
		uint8_t inbyte = *addr++;
		for (uint8_t i = 8; i; i--) {
			uint8_t mix = (crc ^ inbyte) & 0x01;
			crc >>= 1;
			if (mix) crc ^= 0x8C;
			inbyte >>= 1;
		}
	}
	return crc;
}
#endif
