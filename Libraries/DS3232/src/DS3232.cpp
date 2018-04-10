// Arduino DS3232RTC Library
// DS3232RTC.cpp
// Arduino library for the Maxim Integrated DS3232 and DS3231
// Real-Time Clocks.
//
// Jack Christensen 06Mar2013
//
//
// vad7 modified and removed non-standard Time lib
// https://github.com/vad7/Arduino-DUE-WireSam
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//

#include <DS3232.h>

#include <WireSam.h>

/*----------------------------------------------------------------------*
 * Constructor.                                                         *
 *----------------------------------------------------------------------*/
//DS3232::DS3232()
//{
//}
 
void DS3232::begin(uint32_t twiFreq)
{
   Wire.begin();
   if(twiFreq) {
// Include hardware-specific functions for the correct MCU
#if defined(__AVR__)
     TWBR = ( (F_CPU / twiFreq) - 16) / 2;
#elif defined(__arm__)
     Wire.setClock(twiFreq);
#endif
   }
}
 
/*----------------------------------------------------------------------*
 * Reads the current time from the RTC and returns it as a time_t       *
 * value. Returns a zero value if an I2C error occurred (e.g. RTC       *
 * not present).                                                        *
 *----------------------------------------------------------------------*/
/*
time_t DS3232::get()
{
    tmElements_t tm;
    
    if ( read(tm) ) return 0;
    return( makeTime(tm) );
}
*/
/*----------------------------------------------------------------------*
 * Sets the RTC to the given time_t value and clears the                *
 * oscillator stop flag (OSF) in the Control/Status register.           *
 * Returns the I2C status (zero if successful).                         *
 *----------------------------------------------------------------------*/
/*
byte DS3232::set(time_t t)
{
    tmElements_t tm;

    breakTime(t, tm);
    return ( write(tm) );
}
*/
/*----------------------------------------------------------------------*
 * Reads the current time from the RTC and returns it in a tmElements_t *
 * structure. Returns the I2C status (zero if successful).              *
 *----------------------------------------------------------------------*/
byte DS3232::read(tmElements_t &tm)
{
	byte ret;

    Wire.beginTransmission(RTC_ADDR);
    Wire.write((uint8_t)RTC_SECONDS);
    if((ret = Wire.endTransmission())) return ret;
    //request 7 bytes (secs, min, hr, dow, date, mth, yr)
    if(Wire.requestFrom(RTC_ADDR, 7) != 7) return 1;
    tm.Second = bcd2dec(Wire.read() & ~_BV(RTC_DS1307_CH));
    tm.Minute = bcd2dec(Wire.read());
    tm.Hour = bcd2dec(Wire.read() & ~_BV(RTC_HR1224));    //assumes 24hr clock
    tm.Wday = Wire.read();
    tm.Day = bcd2dec(Wire.read());
    tm.Month = bcd2dec(Wire.read() & ~_BV(RTC_CENTURY));  //don't use the Century bit
    tm.Year = 2000 + bcd2dec(Wire.read()); //[V]*
    return 0;
}

/*----------------------------------------------------------------------*
 * Sets the RTC's time from a tmElements_t structure and clears the     *
 * oscillator stop flag (OSF) in the Control/Status register.           *
 * Returns the I2C status (zero if successful).                         *
 *----------------------------------------------------------------------*/
byte DS3232::write(tmElements_t &tm)
{
    Wire.beginTransmission(RTC_ADDR);
    Wire.write((uint8_t)RTC_SECONDS);
    Wire.write(dec2bcd(tm.Second));
    Wire.write(dec2bcd(tm.Minute));
    Wire.write(dec2bcd(tm.Hour));         //sets 24 hour format (Bit 6 == 0)
    Wire.write(tm.Wday);
    Wire.write(dec2bcd(tm.Day));
    Wire.write(dec2bcd(tm.Month));
    Wire.write(dec2bcd(tm.Year) - 2000);
    byte ret = Wire.endTransmission();
    if(ret == 0) {
    	int16_t s = readRTC(RTC_STATUS);        //read the status register
    	if(s != -1) writeRTC( RTC_STATUS, s & ~_BV(RTC_OSF) );  //clear the Oscillator Stop Flag
    }
    return ret;
}

// Return 0 if ok
byte DS3232::setTime(uint8_t hour, uint8_t min, uint8_t sec)
{
	if (hour<24 && min<60 && sec<60)
	{
      Wire.beginTransmission(RTC_ADDR);
      Wire.write((uint8_t)RTC_SECONDS);
      Wire.write(dec2bcd(sec));
      Wire.write(dec2bcd(min));
      Wire.write(dec2bcd(hour));         //sets 24 hour format (Bit 6 == 0)
      byte ret = Wire.endTransmission();
      if(ret == 0) {
          int16_t s = readRTC(RTC_STATUS);        //read the status register
          if(s != -1 && (s & (1<<RTC_OSF))) {
        	  writeRTC(RTC_STATUS, s & ~_BV(RTC_OSF) );  //clear the Oscillator Stop Flag
          }
      }
      return ret;
	} else return 255;
}

// Return 0 if ok
byte DS3232::setDate(uint8_t date, uint8_t month, uint16_t year_NNNN)
{
	if(date<=31 && month<=12 && year_NNNN>=2000 && year_NNNN<3000)
	{
      Wire.beginTransmission(RTC_ADDR);
      Wire.write((uint8_t)RTC_DATE);
      Wire.write(dec2bcd(date));
      Wire.write(dec2bcd(month));
      Wire.write(dec2bcd(year_NNNN - 2000));
      return Wire.endTransmission();
	} else return 255;
}

/*----------------------------------------------------------------------*
 * Write multiple bytes to RTC RAM.                                     *
 * Valid address range is 0x00 - 0xFF, no checking.                     *
 * Number of bytes (nBytes) must be between 1 and 31 (Wire library      *
 * limitation).                                                         *
 * Returns the I2C status (zero if successful).                         *
 *----------------------------------------------------------------------*/
byte DS3232::writeRTC(byte addr, byte *values, byte nBytes)
{
    Wire.beginTransmission(RTC_ADDR, values, nBytes);
    Wire.write(addr);
//    for (byte i=0; i<nBytes; i++) Wire.write(values[i]);
    return Wire.endTransmission(0);
}

/*----------------------------------------------------------------------*
 * Write a single byte to RTC RAM.                                      *
 * Valid address range is 0x00 - 0xFF, no checking.                     *
 * Returns the I2C status (zero if successful).                         *
 *----------------------------------------------------------------------*/
byte DS3232::writeRTC(byte addr, byte value)
{
    return ( writeRTC(addr, &value, 1) );
}

/*----------------------------------------------------------------------*
 * Read multiple bytes from RTC RAM.                                    *
 * Valid address range is 0x00 - 0xFF, no checking.                     *
 * Number of bytes (nBytes) must be between 1 and 32 (Wire library      *
 * limitation).                                                         *
 * Returns the I2C status (zero if successful).                         *
 *----------------------------------------------------------------------*/
byte DS3232::readRTC(byte addr, byte *values, byte nBytes)
{
    Wire.beginTransmission(RTC_ADDR);
    Wire.write(addr);
//    if ( byte e = Wire.endTransmission() ) return e;
//    Wire.requestFrom( (uint8_t)RTC_ADDR, nBytes );
//    for (byte i=0; i<nBytes; i++) values[i] = Wire.read();
    byte e;
    if((e = Wire.endTransmissionReceive(values, nBytes, 0))) return e;
    return 0;
}

/*----------------------------------------------------------------------*
 * Read a single byte from RTC RAM.                                     *
 * Valid address range is 0x00 - 0xFF, if error return -1               *
 *----------------------------------------------------------------------*/
int16_t DS3232::readRTC(byte addr)
{
    byte b;
    
    if(readRTC(addr, &b, 1)) return -1;
    return b;
}

/*----------------------------------------------------------------------*
 * Set an alarm time. Sets the alarm registers only.  To cause the      *
 * INT pin to be asserted on alarm match, use alarmInterrupt().         *
 * This method can set either Alarm 1 or Alarm 2, depending on the      *
 * value of alarmType (use a value from the ALARM_TYPES_t enumeration). *
 * When setting Alarm 2, the seconds value must be supplied but is      *
 * ignored, recommend using zero. (Alarm 2 has no seconds register.)    *
 *----------------------------------------------------------------------*/
void DS3232::setAlarm(RTC_ALARM_TYPES_t alarmType, byte seconds, byte minutes, byte hours, byte daydate)
{
    uint8_t addr;
    
    seconds = dec2bcd(seconds);
    minutes = dec2bcd(minutes);
    hours = dec2bcd(hours);
    daydate = dec2bcd(daydate);
    if (alarmType & 0x01) seconds |= _BV(RTC_A1M1);
    if (alarmType & 0x02) minutes |= _BV(RTC_A1M2);
    if (alarmType & 0x04) hours |= _BV(RTC_A1M3);
    if (alarmType & 0x10) daydate |= _BV(RTC_DYDT);
    if (alarmType & 0x08) daydate |= _BV(RTC_A1M4);
    
    if ( !(alarmType & 0x80) ) {    //alarm 1
        addr = RTC_ALM1_SECONDS;
        writeRTC(addr++, seconds);
    }
    else {
        addr = RTC_ALM2_MINUTES;
    }
    if(writeRTC(addr++, minutes)) return;
    if(writeRTC(addr++, hours)) return;
    if(writeRTC(addr++, daydate)) return;
}

/*----------------------------------------------------------------------*
 * Set an alarm time. Sets the alarm registers only.  To cause the      *
 * INT pin to be asserted on alarm match, use alarmInterrupt().         *
 * This method can set either Alarm 1 or Alarm 2, depending on the      *
 * value of alarmType (use a value from the ALARM_TYPES_t enumeration). *
 * However, when using this method to set Alarm 1, the seconds value    *
 * is set to zero. (Alarm 2 has no seconds register.)                   *
 *----------------------------------------------------------------------*/
void DS3232::setAlarm(RTC_ALARM_TYPES_t alarmType, byte minutes, byte hours, byte daydate)
{
    setAlarm(alarmType, 0, minutes, hours, daydate);
}

/*----------------------------------------------------------------------*
 * Enable or disable an alarm "interrupt" which asserts the INT pin     *
 * on the RTC.                                                          *
 *----------------------------------------------------------------------*/
void DS3232::alarmInterrupt(byte alarmNumber, bool interruptEnabled)
{
    uint8_t controlReg, mask;
    
    controlReg = readRTC(RTC_CONTROL);
    mask = _BV(RTC_A1IE) << (alarmNumber - 1);
    if (interruptEnabled)
        controlReg |= mask;
    else
        controlReg &= ~mask;
    writeRTC(RTC_CONTROL, controlReg); 
}

/*----------------------------------------------------------------------*
 * Returns true or false depending on whether the given alarm has been  *
 * triggered, and resets the alarm flag bit.                            *
 *----------------------------------------------------------------------*/
bool DS3232::alarm(byte alarmNumber)
{
    uint8_t statusReg, mask;
    
    statusReg = readRTC(RTC_STATUS);
    mask = _BV(RTC_A1F) << (alarmNumber - 1);
    if (statusReg & mask) {
        statusReg &= ~mask;
        writeRTC(RTC_STATUS, statusReg);
        return true;
    }
    else {
        return false;
    }
}

/*----------------------------------------------------------------------*
 * Enable or disable the square wave output.                            *
 * Use a value from the SQWAVE_FREQS_t enumeration for the parameter.   *
 *----------------------------------------------------------------------*/
void DS3232::squareWave(RTC_SQWAVE_FREQS_t freq)
{
    uint8_t controlReg;

    controlReg = readRTC(RTC_CONTROL);
    if (freq >= SQWAVE_NONE) {
        controlReg |= _BV(RTC_INTCN);
    }
    else {
        controlReg = (controlReg & 0xE3) | (freq << RTC_RS1);
    }
    writeRTC(RTC_CONTROL, controlReg);
}

/*----------------------------------------------------------------------*
 * Returns the value of the oscillator stop flag (OSF) bit in the       *
 * control/status register which indicates that the oscillator is or    *
 * was stopped, and that the timekeeping data may be invalid.           *
 * Optionally clears the OSF bit depending on the argument passed.      *
 *----------------------------------------------------------------------*/
bool DS3232::oscStopped(bool clearOSF)
{
    int16_t s = readRTC(RTC_STATUS);    //read the status register
    if(s == -1) return 0;
    bool ret = s & _BV(RTC_OSF);            //isolate the osc stop flag to return to caller
    if (ret && clearOSF) {              //clear OSF if it's set and the caller wants to clear it
        writeRTC( RTC_STATUS, s & ~_BV(RTC_OSF) );
    }
    return ret;
}

/*----------------------------------------------------------------------*
 * Returns the temperature in Celsius * 100                       *
 *----------------------------------------------------------------------*/
int16_t DS3232::temperature()
{
    byte rtcTemp[2];
    
    if(readRTC(RTC_TEMP_MSB, rtcTemp, 2)) return -32768;
    return (((int16_t)((rtcTemp[0] << 8) | rtcTemp[1])) >> 6) * 25;
}

/*----------------------------------------------------------------------*
 * Decimal-to-BCD conversion                                            *
 *----------------------------------------------------------------------*/
uint8_t DS3232::dec2bcd(uint8_t n)
{
    return n + 6 * (n / 10);
}

/*----------------------------------------------------------------------*
 * BCD-to-Decimal conversion                                            *
 *----------------------------------------------------------------------*/
uint8_t DS3232::bcd2dec(uint8_t n)
{
    return n - 6 * (n >> 4);
}

#if defined ARDUINO_ARCH_AVR
DS3232 RTC;      //instantiate an RTC object
#endif
