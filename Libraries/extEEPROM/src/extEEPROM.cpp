/*-----------------------------------------------------------------------------*
 * extEEPROM.cpp - Arduino library to support external I2C EEPROMs.            *
 *                                                                             *
 * This library will work with most I2C serial EEPROM chips between 2k bits    *
 * and 2048k bits (2M bits) in size. Multiple EEPROMs on the bus are supported *
 * as a single address space. I/O across block, page and device boundaries     *
 * is supported. Certain assumptions are made regarding the EEPROM             *
 * device addressing. These assumptions should be true for most EEPROMs        *
 * but there are exceptions, so read the datasheet and know your hardware.     *
 *                                                                             *
 * The library should also work for EEPROMs smaller than 2k bits, assuming     *
 * that there is only one EEPROM on the bus and also that the user is careful  *
 * to not exceed the maximum address for the EEPROM.                           *
 *                                                                             *
 * Library tested with:                                                        *
 *   Microchip 24AA02E48 (2k bit)                                              *
 *   24xx32 (32k bit, thanks to Richard M)                                     *
 *   Microchip 24LC256 (256k bit)                                              *
 *   Microchip 24FC1026 (1M bit, thanks to Gabriele B on the Arduino forum)    *
 *   ST Micro M24M02 (2M bit)                                                  *
 *                                                                             *
 * Library will NOT work with Microchip 24xx1025 as its control byte does not  *
 * conform to the following assumptions.                                       *
 *                                                                             *
 * Device addressing assumptions:                                              *
 * 1. The I2C address sequence consists of a control byte followed by one      *
 *    address byte (for EEPROMs <= 16k bits) or two address bytes (for         *
 *    EEPROMs > 16k bits).                                                     *
 * 2. The three least-significant bits in the control byte (excluding the R/W  *
 *    bit) comprise the three most-significant bits for the entire address     *
 *    space, i.e. all chips on the bus. As such, these may be chip-select      *
 *    bits or block-select bits (for individual chips that have an internal    *
 *    block organization), or a combination of both (in which case the         *
 *    block-select bits must be of lesser significance than the chip-select    *
 *    bits).                                                                   *
 * 3. Regardless of the number of bits needed to address the entire address    *
 *    space, the three most-significant bits always go in the control byte.    *
 *    Depending on EEPROM device size, this may result in one or more of the   *
 *    most significant bits in the I2C address bytes being unused (or "don't   *
 *    care").                                                                  *
 * 4. An EEPROM contains an integral number of pages.                          *
 *                                                                             *
 * To use the extEEPROM library, the Arduino Wire library must also            *
 * be included.                                                                *
 *                                                                             *
 * Jack Christensen 23Mar2013 v1                                               *
 * 29Mar2013 v2 - Updated to span page boundaries (and therefore also          *
 * device boundaries, assuming an integral number of pages per device)         *
 * 08Jul2014 v3 - Generalized for 2kb - 2Mb EEPROMs.                           *
 *-----------------------------------------------------------------------------*
 * Modified by vad7 - speed optimization, RTOS support, FRAM support           *
 *                    only work with modified Wire library!                    *
 * https://github.com/vad7/Arduino-DUE-WireSam                                 *
 *-----------------------------------------------------------------------------*/

#include <extEEPROM.h>
#include <WireSam.h>

//extern void _delay(int ms); // RTOS external delay - 1 ms
//#define RTOS_delay(ms) _delay(ms)
#include "FreeRTOS_ARM.h"
#define RTOS_delay(ms) { if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) vTaskDelay(ms/portTICK_PERIOD_MS); else delay(ms); }

// Constructor.
// - deviceCapacity is the capacity of a single EEPROM device in
//   kilobits (kb) and should be one of the values defined in the
//   eeprom_size_t enumeration in the extEEPROM.h file. (Most
//   EEPROM manufacturers use kbits in their part numbers.)
// - nDevice is the number of EEPROM devices on the I2C bus (all must
//   be identical).
// - pageSize is the EEPROM's page size in bytes.
// - eepromAddr is the EEPROM's I2C address and defaults to 0x50 which is common.
extEEPROM::extEEPROM(uint16_t deviceCapacity, byte nDevice, uint32_t pageSize, byte eepromAddr, byte FRAM)
{
    _nDevice = nDevice;
    _eepromAddr = eepromAddr;
    _nAddrBytes = deviceCapacity > 16 ? 2 : 1;       //two address bytes needed for eeproms > 16kbits
    _FRAM = FRAM;
    _dvcCapacity = deviceCapacity * 1024UL / 8;
    _totalCapacity = _nDevice * _dvcCapacity;
    if(_FRAM) _pageSize =  _totalCapacity;
    else _pageSize = pageSize;
#ifdef USE_RTOS_DELAY_AFTER_BYTES
    DelayAfterBytes = USE_RTOS_DELAY_AFTER_BYTES;
    use_RTOS_delay = 0;
#endif

    //determine the bitshift needed to isolate the chip select bits from the address to put into the control byte
    uint16_t kb = deviceCapacity;
    if ( kb <= 16 ) _csShift = 8;
    else if ( kb >= 512 ) _csShift = 16;
    else {
        kb >>= 6;
        _csShift = 12;
        while ( kb >= 1 ) {
            ++_csShift;
            kb >>= 1;
        }
    }
}

//initialize the I2C bus and do a dummy write (no data sent)
//to the device so that the caller can determine whether it is responding.
//when using a 400kHz bus speed and there are multiple I2C devices on the
//bus (other than EEPROM), call extEEPROM::begin() after any initialization
//calls for the other devices to ensure the intended I2C clock speed is set.
byte extEEPROM::begin(unsigned int twiFreq)
{
    Wire.begin();
    if(twiFreq) {
#ifdef USE_RTOS_DELAY_AFTER_BYTES
    	DelayAfterBytes = USE_RTOS_DELAY_AFTER_BYTES * (twiFreq / 1000) / 400;
#endif
// Include hardware-specific functions for the correct MCU
#if defined(__AVR__)
      TWBR = ( (F_CPU / twiFreq) - 16) / 2;
#elif defined(__arm__)
      Wire.setClock(twiFreq);
#endif
    }
    Wire.beginTransmission(_eepromAddr);
    if (_nAddrBytes == 2) Wire.write(0);      //high addr byte
    Wire.write(0);                            //low addr byte
    return Wire.endTransmission(0);
}

//Write bytes to external EEPROM.
//If the I/O would extend past the top of the EEPROM address space,
//a status of EEPROM_ADDR_ERR is returned. For I2C errors, the status
//from the Arduino Wire library is passed back through to the caller.
byte extEEPROM::write(uint32_t addr, byte *values, uint32_t nBytes)
{
    uint8_t ctrlByte;       //control byte (I2C device address & chip/block select bits)
    uint8_t txStatus = 0;   //transmit status
    uint32_t nWrite;        //number of bytes to write

    if (addr + nBytes > _totalCapacity) {   //will this write go past the top of the EEPROM?
        return EEPROM_ADDR_ERR;             //yes, tell the caller
    }
    while(nBytes > 0) {
   		uint32_t nPage;         //number of bytes remaining on current page, starting at addr
   		nPage = _pageSize - ( addr & (_pageSize - 1) );
   		//find min(nBytes, nPage, BUFFER_LENGTH) -- BUFFER_LENGTH is defined in the Wire library.
   		nWrite = nBytes < nPage ? nBytes : nPage;
		//nWrite = BUFFER_LENGTH - _nAddrBytes < nWrite ? BUFFER_LENGTH - _nAddrBytes : nWrite;
        ctrlByte = _eepromAddr | (byte) (addr >> _csShift);
        Wire.beginTransmission(ctrlByte, values, nWrite);
        if(_nAddrBytes == 2) Wire.write( (byte) (addr >> 8) );   //high addr byte
        Wire.write( (byte) addr );                               //low addr byte
#ifdef USE_RTOS_DELAY_AFTER_BYTES
        uint8_t dly = use_RTOS_delay && nWrite > DelayAfterBytes;
       	if((txStatus = Wire.endTransmission(dly))) return txStatus; // choice RTOS delay(1) or not
#else
       	if((txStatus = Wire.endTransmission(0))) return txStatus;
#endif
        if(!_FRAM) {
           //wait EEPROM up to 50ms for the write to complete
           for (uint8_t i=50; i; --i) {
        	  if(dly) {
        		  Wire.beginTransmission(ctrlByte);
        		  if (_nAddrBytes == 2) Wire.write(0);        //high addr byte
        		  Wire.write(0);                              //low addr byte
        		  txStatus = Wire.endTransmission(0);         // without delay
        		  if(txStatus == 0) break; 					  // success? - end
        	  } else dly = 1;
              if(use_RTOS_delay) { RTOS_delay(1); } else delayMicroseconds(300);
           }
           if(txStatus) return txStatus;
        }

        addr += nWrite;         //increment the EEPROM address
        values += nWrite;       //increment the input data pointer
        nBytes -= nWrite;       //decrement the number of bytes left to write
    }
    return txStatus;
}

// Read bytes from external EEPROM.
// Success return 0, if read less than ordered return 1, otherwise error number
byte extEEPROM::read(uint32_t addr, byte *values, uint32_t nBytes)
{
    byte ctrlByte;
    byte rxStatus;
    uint32_t nRead;             //number of bytes to read

    if (addr + nBytes > _totalCapacity) {   //will this read take us past the top of the EEPROM?
        return EEPROM_ADDR_ERR;             //yes, tell the caller
    }

    while(nBytes > 0) {
		uint32_t nPage;             //number of bytes remaining on current page, starting at addr
		nPage = _dvcCapacity - ( addr & (_dvcCapacity - 1) );
		nRead = nBytes < nPage ? nBytes : nPage;

        ctrlByte = _eepromAddr | (byte) (addr >> _csShift);
        Wire.beginTransmission(ctrlByte);
        if (_nAddrBytes == 2) Wire.write( (byte) (addr >> 8) );   //high addr byte
        Wire.write( (byte) addr );                                //low addr byte

#ifdef USE_RTOS_DELAY_AFTER_BYTES
        if((rxStatus = Wire.endTransmissionReceive(values, nRead, use_RTOS_delay && nRead > DelayAfterBytes))) return rxStatus; // choice RTOS delay(1) or not
#else
        if((rxStatus = Wire.endTransmissionReceive(values, nRead, 0))) return rxStatus;
#endif

        if(Wire.available() != nRead) return 1;

        addr += nRead;          //increment the EEPROM address
        values += nRead;        //increment the input data pointer
        nBytes -= nRead;        //decrement the number of bytes left to write
    }
    return 0;
}

//Write a single byte to external EEPROM.
//If the I/O would extend past the top of the EEPROM address space,
//a status of EEPROM_ADDR_ERR is returned. For I2C errors, the status
//from the Arduino Wire library is passed back through to the caller.
byte extEEPROM::write(uint32_t addr, byte value)
{
	uint8_t data = value;
    return write(addr, &data, 1);
}

//Read a single byte from external EEPROM.
//If the I/O would extend past the top of the EEPROM address space,
//a status of EEPROM_ADDR_ERR is returned. For I2C errors, the status
//from the Arduino Wire library is passed back through to the caller.
//To distinguish error values from valid data, error values are returned as negative numbers.
int extEEPROM::read(uint32_t addr)
{
    uint8_t data;
    int ret;
    
    ret = read(addr, &data, 1);
    return ret == 0 ? data : -ret;
}
