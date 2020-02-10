// modified for optimisation by vad7
#include "rtc_clock.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Arduino.h"

uint8_t daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
uint8_t meztime;

// Based on https://github.com/PaulStoffregen/Time.cpp
// for 4 digit years
#define switch_years(Y) ( !(Y % 4) && ( (Y % 100) || !(Y % 400) ) )

RTC_clock::RTC_clock(int source)
{
	_current_time = 0;
	_utime = 0;
	_current_date = 0;
	if(source) {
		pmc_switch_sclk_to_32kxtal(0);

		while(!pmc_osc_is_ready_32kxtal())
			;
	}
}

void RTC_clock::init()
{
#ifdef RTC_USE_UTC_TIME
	dst_winter_done = false;
#endif
	RTC_SetHourMode(RTC, 0);

	NVIC_DisableIRQ(RTC_IRQn);
	NVIC_ClearPendingIRQ(RTC_IRQn);
	NVIC_SetPriority(RTC_IRQn, 0);
	//  NVIC_EnableIRQ(RTC_IRQn);
	//  RTC_EnableIt(RTC, RTC_IER_SECEN | RTC_IER_ALREN);
	//  RTC_EnableIt(RTC, RTC_IER_SECEN);
}

void RTC_clock::set_time(uint8_t hour, uint8_t minute, uint8_t second)
{

	RTC_SetTime(RTC, hour, minute, second);
	_utime = 0;
}

/*
 // Based on https://github.com/adafruit/RTClib/blob/master/RTClib.cpp
 int conv2d(char* p)
 {
 int v = 0;
 if ('0' <= *p && *p <= '9')
 v = *p - '0';

 return 10 * v + *++p - '0';
 }
 */

// Based on http://www.geeksforgeeks.org/write-your-own-atoi/
// Better version converts until none number shows up
uint16_t conv2d(char *p)
{
	uint16_t v = 0; // Initialize result

	// Iterate through all characters of input string and update result
	for(int i = 0; p[i] != '\0'; ++i) {
		if('0' <= p[i] && p[i] <= '9') v = v * 10 + p[i] - '0';
		else break;
	}
	// return result.
	return v;
}

void RTC_clock::set_time(char *time)
{

	RTC_SetTime(RTC, conv2d(time), conv2d(time + 3), conv2d(time + 6));
	_utime = 0;
}

// Based on https://github.com/PaulStoffregen/Time.cpp
void RTC_clock::set_clock(unsigned long timestamp, int timezone)
{
	uint32_t monthLength, time, days;

	// Sunday, 01-Jan-40 00:00:00 UTC 70 years after the beginning of the unix timestamp so
	// if the timestamp bigger than this the "offset" will automatic removed
	if(timestamp >= 2208988800UL) time -= 2208988800UL;

	time = timestamp + time_zone_adjustment(timezone);
	uint8_t _second = time % 60;
	time /= 60; // now it is minutes
	uint8_t _minute = time % 60;
	time /= 60; // now it is hours
	uint8_t _hour = time % 24;
	time /= 24; // now it is days

	uint16_t _year = 1970;
	days = 0;
	while((days += (365 + switch_years(_year))) <= time)
		_year++;

	days -= (365 + switch_years(_year));

	time -= days; // now it is days in this year, starting at 0

	days = 0;
	uint8_t _month = 0;
	for(; _month < 12; _month++) {
		if(_month == 1) // february
			monthLength = daysInMonth[_month] + switch_years(_year);
		else monthLength = daysInMonth[_month];

		if(time >= monthLength) time -= monthLength;
		else break;
	}

	_month++;  								// jan is month 1
	uint8_t _day = time + 1;     // day of month
	uint8_t _day_of_week = calculate_day_of_week(_year, _month, _day);

	RTC_SetTime(RTC, _hour, _minute, _second);
	RTC_SetDate(RTC, (uint16_t) _year, (uint8_t) _month, (uint8_t) _day, (uint8_t) _day_of_week);
	_utime = 0;

}

void RTC_clock::get_date(uint8_t *day_of_week, uint8_t *day, uint8_t *month, uint16_t *year)
{
	RTC_GetDate(RTC, (uint16_t*) year, (uint8_t*) month, (uint8_t*) day, (uint8_t*) day_of_week);
}

void RTC_clock::get_time(uint8_t *hour, uint8_t *minute, uint8_t *second)
{
	RTC_GetTime(RTC, (uint8_t*) hour, (uint8_t*) minute, (uint8_t*) second);
}

inline uint32_t RTC_clock::current_date()
{
	return RTC->RTC_CALR;
//  not needed - 32bit CPU:
//
//	uint32_t dwTime;
//	/* Get current RTC date */
//	dwTime = RTC->RTC_CALR;
//	while(dwTime != RTC->RTC_CALR) {
//		dwTime = RTC->RTC_CALR;
//	}
//
//	return (dwTime);
}

inline uint32_t RTC_clock::current_time()
{
	return RTC->RTC_TIMR;
//  not needed - 32bit CPU:
//
//	uint32_t dwTime;
//	/* Get current RTC time */
//	dwTime = RTC->RTC_TIMR;
//	while(dwTime != RTC->RTC_TIMR) {
//		dwTime = RTC->RTC_TIMR;
//	}
//
//	return (dwTime);
}

uint8_t RTC_clock::get_hours()
{
	return SAM_RTC_HOUR(current_time());
}

uint8_t RTC_clock::get_minutes()
{
	return SAM_RTC_MIN(current_time());
}

uint8_t RTC_clock::get_seconds()
{
	return SAM_RTC_SEC(current_time());
}

uint16_t RTC_clock::get_years()
{
	return SAM_RTC_YEARS(current_date());
}

uint8_t RTC_clock::get_years_short()
{
	return SAM_RTC_YEARS_SHORT(current_date());
}

uint8_t RTC_clock::get_months()
{
	return SAM_RTC_MONTH(current_date());
}

uint8_t RTC_clock::get_days()
{
	return SAM_RTC_DAYS(current_date());
}

uint8_t RTC_clock::get_day_of_week()
{
	return (((current_date() >> 21) & 0x7));
	//  return RTC_CALR_DAY ( current_date() );
}

uint32_t RTC_clock::get_valid_entry()
{
	return (RTC->RTC_VER);
}

/**
 * \brief Calculate day_of_week from year, month, day.
 * Based on SAM3X rtc_example.c from Atmel Software Framework (Real-Time Clock (RTC) example for SAM) also available
 * https://github.com/eewiki/asf/blob/master/sam/drivers/rtc/example/rtc_example.c
 */
uint8_t RTC_clock::calculate_day_of_week(uint16_t _year, uint8_t _month, uint8_t _day)
{
	uint8_t _week;

	if(_month == 1 || _month == 2) {
		_month += 12;
		--_year;
	}

	_week = (_day + 2 * _month + 3 * (_month + 1) / 5 + _year + _year / 4 - _year / 100 + _year / 400) % 7;

	++_week;

	return _week;
}

void RTC_clock::set_date(uint8_t day, uint8_t month, uint16_t year)
{
	RTC_SetDate(RTC, year, month, day, calculate_day_of_week(year, month, day));
	_utime = 0;
}

// Based on https://github.com/adafruit/RTClib/blob/master/RTClib.cpp
void RTC_clock::set_date(char *date)
{
	uint8_t _month = 1;
	switch(date[0]) {
	case 'J':
		_month = date[1] == 'a' ? 1 : date[2] == 'n' ? 6 : 7;
		break;
	case 'F':
		_month = 2;
		break;
	case 'A':
		_month = date[2] == 'r' ? 4 : 8;
		break;
	case 'M':
		_month = date[2] == 'r' ? 3 : 5;
		break;
	case 'S':
		_month = 9;
		break;
	case 'O':
		_month = 10;
		break;
	case 'N':
		_month = 11;
		break;
	case 'D':
		_month = 12;
		break;
	}

	uint8_t _day = conv2d(date + 4);
	uint16_t _year = conv2d(date + 9);

	RTC_SetDate(RTC, _year, _month, _day, calculate_day_of_week(_year, _month, _day));
	_utime = 0;
}

int RTC_clock::date_already_set()
{
	uint32_t dateregister;

	/* Get current RTC date */
	dateregister = current_date();

	if( RESET_VALUE != dateregister) {
		return 1;
	} else {
		return 0;
	}
}

void RTC_clock::set_hours(uint8_t hour)
{
	uint32_t _changed = ((hour % 10) | ((hour / 10) << 4)) << 16;

	change_time((current_time() & 0xFFC0FFFF) ^ _changed);
}

void RTC_clock::set_minutes(uint8_t minute)
{
	uint32_t _changed = ((minute % 10) | ((minute / 10) << 4)) << 8;

	change_time((current_time() & 0xFFFF80FF) ^ _changed);
}

void RTC_clock::set_seconds(uint8_t second)
{
	uint32_t _changed = ((second % 10) | ((second / 10) << 4));

	change_time((current_time() & 0xFFFFFF80) ^ _changed);
}

uint32_t RTC_clock::change_time(uint32_t now)
{

	RTC->RTC_CR |= RTC_CR_UPDTIM;
	while((RTC->RTC_SR & RTC_SR_ACKUPD) != RTC_SR_ACKUPD)
		;

	RTC->RTC_SCCR = RTC_SCCR_ACKCLR;
	RTC->RTC_TIMR = now;
	RTC->RTC_CR &= (uint32_t) (~RTC_CR_UPDTIM);
	RTC->RTC_SCCR |= RTC_SCCR_SECCLR;

	_utime = 0;

	return (int) (RTC->RTC_VER & RTC_VER_NVTIM);
}

void RTC_clock::set_days(uint8_t day)
{

	uint8_t _day_of_week = calculate_day_of_week(get_years(), get_months(), day);

	uint32_t _changed = ((day % 10) | (day / 10) << 4) << 24;

	change_date((current_date() & (0xC0FFFFFF & 0xFF1FFFFF)) ^ (_changed | (((_day_of_week % 10) | (_day_of_week / 10) << 4) << 21)));
}

void RTC_clock::set_months(uint8_t month)
{

	uint8_t _day_of_week = calculate_day_of_week(get_years(), month, get_days());

	uint32_t _changed = ((month % 10) | (month / 10) << 4) << 16;

	change_date((current_date() & (0xFFE0FFFF & 0xFF1FFFFF)) ^ (_changed | (((_day_of_week % 10) | (_day_of_week / 10) << 4) << 21)));
}

void RTC_clock::set_years(uint16_t year)
{

	uint8_t _day_of_week = calculate_day_of_week(year, get_months(), get_days());

	uint32_t _changed = (((year / 100) % 10) | ((year / 1000) << 4)) | ((year % 10) | (((year / 10) % 10)) << 4) << 8;

	change_date((current_date() & (0xFFFF0080 & 0xFF1FFFFF)) ^ (_changed | (((_day_of_week % 10) | (_day_of_week / 10) << 4) << 21)));
}

uint32_t RTC_clock::change_date(uint32_t now)
{

	RTC->RTC_CR |= RTC_CR_UPDCAL;
	while((RTC->RTC_SR & RTC_SR_ACKUPD) != RTC_SR_ACKUPD)
		;

	RTC->RTC_SCCR = RTC_SCCR_ACKCLR;
	RTC->RTC_CALR = now;
	RTC->RTC_CR &= (uint32_t) (~RTC_CR_UPDCAL);
	RTC->RTC_SCCR |= RTC_SCCR_SECCLR;

	_utime = 0;

	return (int) (RTC->RTC_VER & RTC_VER_NVCAL);
}

// Based on https://github.com/adafruit/RTClib/blob/master/RTClib.cpp
void RTC_clock::set_clock(char *date, char *time)
{
	set_date(date);
	set_time(time);
}

void (*useralarmFunc)(void);

void RTC_clock::attachalarm(void (*userFunction)(void))
{
	useralarmFunc = userFunction;
}

void RTC_Handler(void)
{
	uint32_t status = RTC->RTC_SR;

	/* Time or date alarm */
	if((status & RTC_SR_ALARM) == RTC_SR_ALARM) {
		/* Disable RTC interrupt */
		RTC_DisableIt(RTC, RTC_IDR_ALRDIS);

		/* Execute function */
		useralarmFunc();

		/* Clear notification */
		RTC_ClearSCCR(RTC, RTC_SCCR_ALRCLR);
		RTC_EnableIt(RTC, RTC_IER_ALREN);
	}
}

void RTC_clock::set_alarmtime(uint8_t hour, uint8_t minute, uint8_t second)
{
	uint8_t _hour = hour;
	uint8_t _minute = minute;
	uint8_t _second = second;

	RTC_EnableIt(RTC, RTC_IER_ALREN);
	RTC_SetTimeAlarm(RTC, &_hour, &_minute, &_second);
	NVIC_EnableIRQ(RTC_IRQn);
}

void RTC_clock::set_alarmdate(uint8_t month, uint8_t day)
{
	uint8_t _month = month;
	uint8_t _day = day;

	RTC_EnableIt(RTC, RTC_IER_ALREN);
	RTC_SetDateAlarm(RTC, &_month, &_day);
	NVIC_EnableIRQ(RTC_IRQn);
}

#ifdef RTC_USE_UTC_TIME
uint32_t RTC_clock::unixtime(int timezone)
#else
uint32_t RTC_clock::unixtime(void)
#endif
{
	uint32_t cd = current_date();
	uint32_t ct = current_time();
	if(_utime && _current_date == cd) {
		if(_current_time == ct) {
			return _utime;
		} else {
			uint32_t tst = _current_time ^ ct;
			if(!(tst & 0x3F0000)) { // hours is not changed
				if((tst & 0x7F00)) { // min changed
					_utime += ((int) (((ct & 0x7000) >> 12) * 10 + ((ct & 0xF00) >> 8)) - (((_current_time & 0x7000) >> 12) * 10 + ((_current_time & 0xF00) >> 8))) * 60;
				}
				if((tst & 0x7F)) { // sec changed
					_utime += (int) (((ct & 0x70) >> 4) * 10 + (ct & 0xF)) - (((_current_time & 0x70) >> 4) * 10 + ((_current_time & 0xF)));
				}
				_current_time = ct;
				return _utime;
			}
		}
	} else _current_date = cd;
	_current_time = ct;

	//_day_of_week = ((_current_date >> 21) & 0x7);
	uint8_t _month = ((((_current_date >> 20) & 1) * 10) + ((_current_date >> 16) & 0xF));

	//_year 4 digits
	uint16_t _year = ((((_current_date >> 4) & 0x7) * 1000) + ((_current_date & 0xF) * 100) + (((_current_date >> 12) & 0xF) * 10) + ((_current_date >> 8) & 0xF));

	//_year 2 digits
	//_year   = (((_current_date >> 12) & 0xF) * 10) + ((_current_date >> 8) & 0xF);

	// Based on https://github.com/punkiller/workspace/blob/master/string2UnixTimeStamp.cpp
	// days of the years between start of unixtime and now
	uint16_t _days = 365 * (_year - 1970);

	// add days from switch years in between except year from date
	for(int i = 1970; i < _year; i++) {
		if(switch_years(i)) {
			_days++;
		}
	}

	// Based on https://github.com/adafruit/RTClib/blob/master/RTClib.cpp
	for(int i = 1; i < _month; ++i)
		_days += daysInMonth[i - 1];

	if(_month > 2 && switch_years(_year)) ++_days;

	_days += ((((_current_date >> 28) & 0x3) * 10) + ((_current_date >> 24) & 0xF)) - 1; // day

	uint32_t _ticks = ((_days * 24 + (((_current_time & 0x00300000) >> 20) * 10 + ((_current_time & 0x000F0000) >> 16))) * 60
			+ (((_current_time & 0x00007000) >> 12) * 10 + ((_current_time & 0x00000F00) >> 8))) * 60 + (((_current_time & 0x00000070) >> 4) * 10 + ((_current_time & 0x0000000F)));

#ifdef RTC_USE_UTC_TIME
	_ticks = _ticks - (int)time_zone_adjustment(timezone);
#endif

	_utime = _ticks;

	return _ticks;
}

#define SECONDS_PER_HOUR_DIV100 (SECONDS_PER_HOUR/100)
int RTC_clock::time_zone_adjustment(int timezone)
{
	int adjustment;

#ifdef RTC_USE_UTC_TIME
	if (timezone == Germany) timezone = 1 + summertime();
#endif

	switch(timezone) {
	case -12:
		adjustment = -1200 * SECONDS_PER_HOUR_DIV100;
		break;
	case -11:
		adjustment = -1100 * SECONDS_PER_HOUR_DIV100;
		break;
	case -10:
		adjustment = -1000 * SECONDS_PER_HOUR_DIV100;
		break;
	case -930:
		adjustment = -950 * SECONDS_PER_HOUR_DIV100;
		break;
	case -9:
		adjustment = -900 * SECONDS_PER_HOUR_DIV100;
		break;
	case -8:
		adjustment = -800 * SECONDS_PER_HOUR_DIV100;
		break;
	case -7:
		adjustment = -700 * SECONDS_PER_HOUR_DIV100;
		break;
	case -6:
		adjustment = -600 * SECONDS_PER_HOUR_DIV100;
		break;
	case -5:
		adjustment = -500 * SECONDS_PER_HOUR_DIV100;
		break;
	case -4:
		adjustment = -400 * SECONDS_PER_HOUR_DIV100;
		break;
	case -330:
		adjustment = -350 * SECONDS_PER_HOUR_DIV100;
		break;
	case -3:
		adjustment = -300 * SECONDS_PER_HOUR_DIV100;
		break;
	case -2:
		adjustment = -200 * SECONDS_PER_HOUR_DIV100;
		break;
	case -1:
		adjustment = -100 * SECONDS_PER_HOUR_DIV100;
		break;
	case 0:
	default:
		adjustment = 0;
		break;
	case 1:
		adjustment = 100 * SECONDS_PER_HOUR_DIV100;
		break;
	case 2:
		adjustment = 200 * SECONDS_PER_HOUR_DIV100;
		break;
	case 3:
		adjustment = 300 * SECONDS_PER_HOUR_DIV100;
		break;
	case 330:
		adjustment = 350 * SECONDS_PER_HOUR_DIV100;
		break;
	case 4:
		adjustment = 400 * SECONDS_PER_HOUR_DIV100;
		break;
	case 430:
		adjustment = 450 * SECONDS_PER_HOUR_DIV100;
		break;
	case 5:
		adjustment = 500 * SECONDS_PER_HOUR_DIV100;
		break;
	case 530:
		adjustment = 550 * SECONDS_PER_HOUR_DIV100;
		break;
	case 545:
		adjustment = 575 * SECONDS_PER_HOUR_DIV100;
		break;
	case 6:
		adjustment = 600 * SECONDS_PER_HOUR_DIV100;
		break;
	case 630:
		adjustment = 650 * SECONDS_PER_HOUR_DIV100;
		break;
	case 7:
		adjustment = 700 * SECONDS_PER_HOUR_DIV100;
		break;
	case 8:
		adjustment = 800 * SECONDS_PER_HOUR_DIV100;
		break;
	case 845:
		adjustment = 875 * SECONDS_PER_HOUR_DIV100;
		break;
	case 9:
		adjustment = 900 * SECONDS_PER_HOUR_DIV100;
		break;
	case 930:
		adjustment = 950 * SECONDS_PER_HOUR_DIV100;
		break;
	case 10:
		adjustment = 1000 * SECONDS_PER_HOUR_DIV100;
		break;
	case 1030:
		adjustment = 1050 * SECONDS_PER_HOUR_DIV100;
		break;
	case 11:
		adjustment = 1100 * SECONDS_PER_HOUR_DIV100;
		break;
	case 1130:
		adjustment = 1150 * SECONDS_PER_HOUR_DIV100;
		break;
	case 12:
		adjustment = 1200 * SECONDS_PER_HOUR_DIV100;
		break;
	case 1245:
		adjustment = 1275 * SECONDS_PER_HOUR_DIV100;
		break;
	case 13:
		adjustment = 1300 * SECONDS_PER_HOUR_DIV100;
		break;
	case 14:
		adjustment = 1400 * SECONDS_PER_HOUR_DIV100;
		break;
	}

	return adjustment;
}

#ifdef RTC_USE_UTC_TIME
int RTC_clock::UTC_abbreviation ()
{
	if ( summertime () )
		return CEST;
	else
		return CET;
}

/* 
int RTC_clock::switch_years (uint16_t year)
{
	if ( ((year %4 == 0) && (year % 100 != 0)) || (year % 400 == 0) ) {
		return 1;
	} else {
		return 0;
	}
}
 */

int RTC_clock::summertime ()
{
	int sundaysommertime, sundaywintertime, today, sundaysommertimehours, sundaywintertimehours, todayhours;

	uint32_t _cdate = current_date();
	uint32_t _ctime = current_time();

	_hour   = (((_ctime & 0x00300000) >> 20) * 10 + ((_ctime & 0x000F0000) >> 16));
	_day    = ((((_cdate >> 28) & 0x3) *   10) + ((_cdate >> 24) & 0xF));
	_month  = ((((_cdate >> 20) &   1) *   10) + ((_cdate >> 16) & 0xF));
	//_year 4 digits
	_year   = ((((_cdate >>  4) & 0x7) * 1000) + ((_cdate & 0xF) * 100)
			+ (((_cdate >> 12) & 0xF) * 10) + ((_cdate >> 8) & 0xF));

	//_year 2 digits
	//_year   = (((_cdate >> 12) & 0xF) * 10) + ((_cdate >> 8) & 0xF);

	// Based on http://www.webexhibits.org/daylightsaving/i.html
	// Equations by Wei-Hwa Huang (US), and Robert H. van Gent (EC)
	// Slightly modified for use in micro controller for integer use
	// also found there http://manfred.wilzeck.de/Datum_berechnen.html#Auch_mit_Osterdatum_berechnen
	// Number 5 (Works for the Years 2000 - 2099)
	sundaysommertime = 31 - ( 5 + _year * 5 / 4 ) % 7;
	sundaywintertime = 31 - ( 2 + _year * 5 / 4 ) % 7;
	today = _day;

	// Summertime begin in March
	for (int i = 1; i < 2; ++i) {
		if ( (i - 1) == 1)
			sundaysommertime += daysInMonth[i - 1] + switch_years(_year);
		else
			sundaysommertime += daysInMonth[i - 1];
	}

	// Wintertime begin in October
	for (int i = 1; i < 9; ++i) {
		if ( (i - 1) == 1)
			sundaywintertime += daysInMonth[i - 1] + switch_years(_year);
		else
			sundaywintertime += daysInMonth[i - 1];
	}

	// Total actually days
	for (int i = 1; i < (_month - 1); ++i) {
		if ( (i - 1) == 1)
			today += daysInMonth[i - 1] + switch_years(_year);
		else
			today += daysInMonth[i - 1];
	}

	sundaysommertimehours = sundaysommertime * 24 + 2;
	sundaywintertimehours = sundaywintertime * 24 + 3;
	todayhours = today * 24 + _hour;

	if ( todayhours >= sundaysommertimehours && (todayhours + 1) < sundaywintertimehours )
		return 1;
	else
		return 0;
}

void RTC_clock::dst_followup ()
{
	int sundaysommertime, sundaywintertime;

	uint32_t _cdate = current_date();

	//_year 4 digits
	_year   = ((((_cdate >>  4) & 0x7) * 1000) + ((_cdate & 0xF) * 100)
			+ (((_cdate >> 12) & 0xF) * 10) + ((_cdate >> 8) & 0xF));

	//_year 2 digits
	//_year   = (((_cdate >> 12) & 0xF) * 10) + ((_cdate >> 8) & 0xF);

	sundaysommertime = 31 - ( 5 + _year * 5 / 4 ) % 7;
	sundaywintertime = 31 - ( 2 + _year * 5 / 4 ) % 7;

	if (get_months () == 3 && get_days () == sundaysommertime) {
		if (get_hours () == 2 && get_minutes () == 0 && get_seconds () == 0) {
			set_hours (3);
			dst_winter_done = false;
		}
	}

	if (!dst_winter_done) {
		if (get_months () == 10 && get_days () == sundaywintertime ) {
			if (get_hours () == 3 && get_minutes () == 0 && get_seconds () == 0) {
				set_hours (2);
				dst_winter_done = true;
			}
		}
	}
}
#endif
