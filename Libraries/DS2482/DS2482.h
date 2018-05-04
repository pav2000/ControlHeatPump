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

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __DS2482_H__
#define __DS2482_H__

#include <inttypes.h>

// you can exclude onewire_search by defining that to 0
#ifndef ONEWIRE_SEARCH
#define ONEWIRE_SEARCH 1
#endif

// Chose between a table based CRC (flash expensive, fast) or a computed CRC (smaller, slow)
#ifndef ONEWIRE_CRC8_TABLE
#define ONEWIRE_CRC8_TABLE     1         // использовать таблицу при расчете crc
#endif

#define DS2482_CONFIG_APU (1<<0)
#define DS2482_CONFIG_PPM (1<<1)
#define DS2482_CONFIG_SPU (1<<2)
#define DS2484_CONFIG_WS  (1<<3)

#define DS2482_STATUS_BUSY 	(1<<0)
#define DS2482_STATUS_PPD 	(1<<1)
#define DS2482_STATUS_SD	(1<<2)
#define DS2482_STATUS_LL	(1<<3)
#define DS2482_STATUS_RST	(1<<4)
#define DS2482_STATUS_SBR	(1<<5)
#define DS2482_STATUS_TSB	(1<<6)
#define DS2482_STATUS_DIR	(1<<7)

#define DS2482_POINTER_STATUS	   	0xF0
#define DS2482_POINTER_DATA			0xE1
#define DS2482_POINTER_CONFIG	   	0xC3
#define DS2482_I2C_ERROR	   		0xFF

#define DS2482_COMMAND_RESET		0xF0	// Device reset
#define DS2482_COMMAND_SRP	        0xE1 	// Set read pointer
#define DS2482_COMMAND_WRITECONFIG	0xD2
#define DS2482_COMMAND_RESETWIRE	0xB4
#define DS2482_COMMAND_WRITEBYTE	0xA5
#define DS2482_COMMAND_READBYTE		0x96
#define DS2482_COMMAND_SINGLEBIT	0x87
#define DS2482_COMMAND_TRIPLET		0x78

#define DS2482_ERROR_TIMEOUT		(1<<0)
#define DS2482_ERROR_SHORT			(1<<1)
#define DS2482_ERROR_CONFIG			(1<<2)

class DS2482
{
public:
	DS2482(uint8_t address); // I2C address
	
	uint8_t configure(uint8_t config); // Return 1 when OK
	uint8_t reset_bridge(void);
    uint8_t check_presence(void);

	//DS2482-800 only
    uint8_t select_channel(uint8_t channel);
	
    uint8_t reset(void); // return true if presence pulse is detected
//	uint8_t read_status(bool setPtr=false); // for stack optimization moved to private
	
	uint8_t write(uint8_t b);
	int16_t read(void);
	
	uint8_t write_bit(uint8_t bit);
	uint8_t read_bit(void);
    // Issue a 1-Wire rom select command, you do the reset first.
    void select(uint8_t rom[8]);
	// Issue skip rom
	void skip(void);
	
//	uint8_t has_timeout() { return mTimeout; }
#if ONEWIRE_SEARCH
    // Clear the search state so that if will start from the beginning again.
    void reset_search(void);

    // Look for the next device. Returns 1 if a new address has been
    // returned. A zero might mean that the bus is shorted, there are
    // no devices, or you have already retrieved all of them.  It
    // might be a good idea to check the CRC to make sure you didn't
    // get garbage.  The order is deterministic. You will always get
    // the same devices in the same order.
    uint8_t search(uint8_t *newAddr);
#endif
    // Compute a Dallas Semiconductor 8 bit CRC, these are used in the
    // ROM and scratchpad registers.
    uint8_t crc8(const uint8_t *addr, uint8_t len);

    void set_address(uint8_t address) { mAddress = address; } ;

private:
	
	uint8_t mAddress;
//	uint8_t mTimeout;
	uint8_t CurrPtr;
	int16_t readByte(void);
	uint8_t setReadPtr(uint8_t readPtr);
	
	uint8_t busyWait(bool setReadPtr=false); //blocks until
	void 	begin(void);
	uint8_t end(void);
	uint8_t read_status(bool setPtr=false);
	
#if ONEWIRE_SEARCH
	uint8_t searchAddress[8];
	uint8_t searchLastDisrepancy;
	uint8_t searchExhausted;
#endif
	
};



#endif
