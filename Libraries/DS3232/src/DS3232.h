// Arduino DS3232RTC Library
// DS3232RTC.h
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

#ifndef DS3232_H_INCLUDED
#define DS3232_H_INCLUDED

#include <Arduino.h> 

//DS3232 I2C Address
#define RTC_ADDR 0x68

//DS3232 Register Addresses
#define RTC_SECONDS 0x00
#define RTC_MINUTES 0x01
#define RTC_HOURS 0x02
#define RTC_DAY 0x03
#define RTC_DATE 0x04
#define RTC_MONTH 0x05
#define RTC_YEAR 0x06
#define RTC_ALM1_SECONDS 0x07
#define RTC_ALM1_MINUTES 0x08
#define RTC_ALM1_HOURS 0x09
#define RTC_ALM1_DAYDATE 0x0A
#define RTC_ALM2_MINUTES 0x0B
#define RTC_ALM2_HOURS 0x0C
#define RTC_ALM2_DAYDATE 0x0D
#define RTC_CONTROL 0x0E
#define RTC_STATUS 0x0F
#define RTC_AGING 0x10
#define RTC_TEMP_MSB 0x11
#define RTC_TEMP_LSB 0x12
#define RTC_SRAM_START_ADDR 0x14    //first SRAM address
#define RTC_SRAM_SIZE 236           //number of bytes of SRAM

//Alarm mask bits
#define RTC_A1M1 7
#define RTC_A1M2 7
#define RTC_A1M3 7
#define RTC_A1M4 7
#define RTC_A2M2 7
#define RTC_A2M3 7
#define RTC_A2M4 7

//Control register bits
#define RTC_EOSC 7
#define RTC_BBSQW 6
#define RTC_CONV 5
#define RTC_RS2 4
#define RTC_RS1 3
#define RTC_INTCN 2
#define RTC_A2IE 1
#define RTC_A1IE 0

//Status register bits
#define RTC_OSF 7
#define RTC_BB32KHZ 6
#define RTC_CRATE1 5
#define RTC_CRATE0 4
#define RTC_EN32KHZ 3
#define RTC_BSY 2
#define RTC_A2F 1
#define RTC_A1F 0

//Square-wave output frequency (TS2, RS1 bits)
enum RTC_SQWAVE_FREQS_t {SQWAVE_1_HZ, SQWAVE_1024_HZ, SQWAVE_4096_HZ, SQWAVE_8192_HZ, SQWAVE_NONE};

//Alarm masks
enum RTC_ALARM_TYPES_t {
    ALM1_EVERY_SECOND = 0x0F,
    ALM1_MATCH_SECONDS = 0x0E,
    ALM1_MATCH_MINUTES = 0x0C,     //match minutes *and* seconds
    ALM1_MATCH_HOURS = 0x08,       //match hours *and* minutes, seconds
    ALM1_MATCH_DATE = 0x00,        //match date *and* hours, minutes, seconds
    ALM1_MATCH_DAY = 0x10,         //match day *and* hours, minutes, seconds
    ALM2_EVERY_MINUTE = 0x8E,
    ALM2_MATCH_MINUTES = 0x8C,     //match minutes
    ALM2_MATCH_HOURS = 0x88,       //match hours *and* minutes
    ALM2_MATCH_DATE = 0x80,        //match date *and* hours, minutes
    ALM2_MATCH_DAY = 0x90,         //match day *and* hours, minutes
};

#define RTC_ALARM_1 1                  //constants for calling functions
#define RTC_ALARM_2 2

//Other
#define RTC_DS1307_CH 7                //for DS1307 compatibility, Clock Halt bit in Seconds register
#define RTC_HR1224 6                   //Hours register 12 or 24 hour mode (24 hour mode==0)
#define RTC_CENTURY 7                  //Century bit in Month register
#define RTC_DYDT 6                     //Day/Date flag bit in alarm Day/Date registers

//[V]- #include <TimeLib.h>
//[V]+
typedef struct  { 
  byte Second; 
  byte Minute; 
  byte Hour; 
  byte Wday;   // day of week, sunday is day 1
  byte Day;
  byte Month; 
  uint16_t Year; // 2000..2100
} tmElements_t;
//

class DS3232
{
    public:
//        DS3232();
        void begin(uint32_t twiFreq); // 0 = don't set twi speed
//[V]-
//        static time_t get();       //must be static to work with setSyncProvider() in the Time library
//        byte set(time_t t);
//[V]+
        byte setTime(uint8_t hour, uint8_t min, uint8_t sec);
        byte setDate(uint8_t date, uint8_t month, uint16_t year_NNNN);
//
        byte read(tmElements_t &tm);
        byte write(tmElements_t &tm);
        byte writeRTC(byte addr, byte *values, byte nBytes);
        byte writeRTC(byte addr, byte value);
        byte readRTC(byte addr, byte *values, byte nBytes);
        int16_t readRTC(byte addr);
        void setAlarm(RTC_ALARM_TYPES_t alarmType, byte seconds, byte minutes, byte hours, byte daydate);
        void setAlarm(RTC_ALARM_TYPES_t alarmType, byte minutes, byte hours, byte daydate);
        void alarmInterrupt(byte alarmNumber, bool alarmEnabled);
        bool alarm(byte alarmNumber);
        void squareWave(RTC_SQWAVE_FREQS_t freq);
        bool oscStopped(bool clearOSF = false);
        int16_t temperature(); // C * 100
        static byte errCode;

    private:
        uint8_t dec2bcd(uint8_t n);
        static uint8_t bcd2dec(uint8_t n);
};

#if defined ARDUINO_ARCH_AVR
extern DS3232 RTC;
#endif

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

#endif
