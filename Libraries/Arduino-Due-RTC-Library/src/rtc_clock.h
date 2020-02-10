// modified for optimisation by vad7
#ifndef RTC_clock_h
#define RTC_clock_h

#include "Arduino.h"

// Includes Atmel CMSIS
#include <chip.h>

#define SUPC_KEY     0xA5u
#define RESET_VALUE  0x01210720

#define RC           0
#define	XTAL         1
/*
 // Unixtimeseconds from 1. Januar 1970  00:00:00 to 1. Januar 2000   00:00:00 UTC-0
 #define SECONDS_FROM_1970_TO_2000 946684800
 */
#define SECONDS_PER_HOUR 3600

//#define RTC_USE_UTC_TIME
#define UTC 0
#define Germany 2000

#define CET  1   //MEZ
#define CEST 2   //MESZ

#define SAM_RTC_HOUR(_ctime) (((_ctime & 0x00300000) >> 20) * 10 + ((_ctime & 0x000F0000) >> 16))
#define SAM_RTC_MIN(_ctime)  (((_ctime & 0x00007000) >> 12) * 10 + ((_ctime & 0x00000F00) >> 8))
#define SAM_RTC_SEC(_ctime)  (((_ctime & 0x00000070) >> 4) * 10 + ((_ctime & 0x0000000F)))
#define SAM_RTC_YEARS(_cdate) ((((_cdate >> 4) & 0x7) * 1000) + ((_cdate & 0xF) * 100) + (((_cdate >> 12) & 0xF) * 10) + ((_cdate >> 8) & 0xF))
#define SAM_RTC_YEARS_SHORT(_cdate) (((_cdate >> (RTC_CALR_YEAR_Pos + 4)) & 0xF) * 10) + ((_cdate >> RTC_CALR_YEAR_Pos) & 0xF)
#define SAM_RTC_MONTH(_cdate) ((((_cdate >> 20) & 1) * 10) + ((_cdate >> 16) & 0xF))
#define SAM_RTC_DAYS(_cdate) ((((_cdate >> 28) & 0x3) * 10) + ((_cdate >> 24) & 0xF))

class RTC_clock {
public:
	RTC_clock(int source);
	void init();
	void set_time(uint8_t hour, uint8_t minute, uint8_t second);
	void set_time(char *time);
	uint8_t get_hours();
	uint8_t get_minutes();
	uint8_t get_seconds();
	void set_date(uint8_t day, uint8_t month, uint16_t year);
	void set_date(char *date);
	void set_clock(char *date, char *time);
	void set_clock(unsigned long timestamp, int timezone = 0);
#ifdef RTC_USE_UTC_TIME
	uint32_t unixtime(int timezone = 0);
	int summertime ();
	int UTC_abbreviation();
	void dst_followup();
#else
	uint32_t unixtime(void);
#endif
	int time_zone_adjustment(int timezone);
	uint16_t get_years();
	uint8_t get_years_short();
	uint8_t get_months();
	int date_already_set();
	uint8_t get_days();
	uint8_t get_day_of_week();
	uint32_t get_valid_entry();
	uint8_t calculate_day_of_week(uint16_t _year, uint8_t _month, uint8_t _day);
	void set_hours(uint8_t _hour);
	void set_minutes(uint8_t minute);
	void set_seconds(uint8_t second);
	void set_days(uint8_t day);
	void set_months(uint8_t month);
	void set_years(uint16_t year);
	void set_alarmtime(uint8_t hour, uint8_t minute, uint8_t second);
	void set_alarmdate(uint8_t month, uint8_t day);

	void attachalarm(void (*)(void));
	void get_time(uint8_t *hour, uint8_t *minute, uint8_t *second);
	void get_date(uint8_t *day_of_week, uint8_t *day, uint8_t *month, uint16_t *year);
	//int switch_years (uint16_t year);
	uint32_t current_time();
	uint32_t current_date();

private:
#ifdef RTC_USE_UTC_TIME
	bool dst_winter_done;
#endif
	uint32_t change_time(uint32_t _now);
	uint32_t change_date(uint32_t _now);
	uint32_t _current_time;
	uint32_t _current_date;
	uint32_t _utime; // current unix time
};

#endif
