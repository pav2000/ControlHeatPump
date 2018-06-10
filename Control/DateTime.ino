/*
 * Copyright (c) 2016-2018 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav; by vad711 (vad7@yahoo.com)
 * "Народный контроллер" для тепловых насосов.
 * Данное програмноое обеспечение предназначено для управления 
 * различными типами тепловых насосов для отопления и ГВС.
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3.0 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 */
// -------- Работа со временем ---------------------------
#include "Constant.h"                       // Вся конфигурация и константы проекта Должен быть первым !!!!
#include <rtc_clock.h>                      // Hабота со встроенными часами
#include "stdlib.h"


// Объявление функций 
// NTP ----------------------------
#define NTP_PACKET_SIZE 48                  // временная отметка NTP находится в первых 48 байтах сообщения
byte packetBuffer[NTP_PACKET_SIZE+1];       // буфер, в котором будут храниться входящие и исходящие пакеты

// Установка веремени системы (сначала читаем из i2c часов)
// Возвращает код ошибки
int8_t set_time(void)
{
   unsigned long ttime;
   //rtcI2C.begin(); // I2C уже инициализирована.// Запустить i2c часы 
   rtcSAM3X8.init();                             // Запуск внутренних часов
   journal.jprintf(" Init internal RTC sam3x8e\n"); 
   ttime=TimeToUnixTime(getTime_RtcI2C());   // Прочитать время из часов i2c
   rtcSAM3X8.set_clock(ttime,0);                // Установить внутренние часы по i2c   
   _delay(200);         
   journal.jprintf(" Set time internal RTC form i2c RTC DS3231: %s ",NowDateToStr());journal.jprintf("%s\n",NowTimeToStr());  // Одним оператором есть косяк
  
   if (HP.get_updateNTP()) set_time_NTP() ;      // Обновить время по NTP
   HP.set_uptime(ttime);                         // Запомнить время старта контроллера
   return OK;
}

#ifdef HTTP_TIME_REQUEST
// Формат HTTP 1.0 GET запроса: "http://server:port/curr_time.csv"
// Ответ: "UTC time sec;"
EthernetClient tTCP; // For get time
char NTP_buffer[16];
boolean set_time_NTP(void)
{
	unsigned long secs = 0;
	int8_t flag = 0;
	IPAddress ip(0, 0, 0, 0);
	// Если запущен шедулер то захватываем семафор
	if(SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE) {
		return false;
	}  // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
	journal.jprintf(pP_TIME, "Update time from: %s\n", HP.get_serverNTP());
	if(check_address(HP.get_serverNTP(), ip) == 0) {
		SemaphoreGive(xWebThreadSemaphore);
		return false;
	}  // DNS - ошибка выходим
	char *p = strchr(HP.get_serverNTP(), ':');
	uint16_t port = 80;
	if(p != NULL) port = atoi(p + 1);

	// 2. Посылка пакета
	for(uint8_t i = 0; i < NTP_REPEAT; i++)                                       // Делам 5 попыток получить время
	{
		WDT_Restart(WDT);                                            // Сбросить вачдог
		journal.jprintf(" Send request, wait...");
		flag = tTCP.connect(ip, port, W5200_SOCK_SYS);
		if(!flag) {
			journal.jprintf(" connect fail\n");
		} else {
			tTCP.write_buffer_flash((uint8_t *) &http_get_str1, sizeof(http_get_str1)-1);
			tTCP.write_buffer_flash((uint8_t *) HTTP_TIME_REQ, sizeof(HTTP_TIME_REQ)-1);
			tTCP.write_buffer_flash((uint8_t *) &http_get_str2, sizeof(http_get_str2)-1);
			tTCP.write_buffer((uint8_t *) HP.get_serverNTP(), m_strlen(HP.get_serverNTP()));
			tTCP.write_buffer_flash((uint8_t *) &http_get_str3, sizeof(http_get_str3)-1);
			if(tTCP.write((const uint8_t *)NULL, (size_t)0) == 0) {
				journal.jprintf(" send error\n");
			} else {
				uint8_t wait = 20;
				flag = 0;
				while(wait--) { // ожидание ответа
					SemaphoreGive(xWebThreadSemaphore);
					_delay(100);
					if(SemaphoreTake(xWebThreadSemaphore,(W5200_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE) break; // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
					if(tTCP.available()) {
						flag = 1;
						break;
					}
				}
				if(flag > 0) { // Ответ получен
					if(tTCP.read((uint8_t *)&NTP_buffer, sizeof(http_key_ok1)-1) == sizeof(http_key_ok1)-1) {
						if(memcmp(&NTP_buffer, &http_key_ok1, sizeof(http_key_ok1)-1) == 0) {
							if(tTCP.read((uint8_t *)&NTP_buffer, 3 + sizeof(http_key_ok2)-1) == 3 + sizeof(http_key_ok2)-1) { // HTTP/
								if(memcmp((uint8_t *)&NTP_buffer + 3, &http_key_ok2, sizeof(http_key_ok2)-1) == 0) {	// 200 OK
									flag = -6;
									while(tTCP.available()) {
										if(tTCP.read() == '\r' && tTCP.read() == '\n' && tTCP.read() == '\r' && tTCP.read() == '\n') { // тело
											memset(&NTP_buffer, 0, sizeof(NTP_buffer));
											tTCP.read((uint8_t *)&NTP_buffer, sizeof(NTP_buffer));
											char *p = strchr(NTP_buffer, ';');
											if(p != NULL) {
												*p = '\0';
												secs = atoi(NTP_buffer);
												if(secs) flag = 1;
											} else flag = -7;
											break;
										}
									}
								} else flag = -5;
							} else flag = -4;
						} else flag = -3;
					} else flag = -2;
				} else flag = -1;
			}
			tTCP.stop();
			if(flag > 0) {
				journal.jprintf("OK\n");
				break;
			} else journal.jprintf(" Error %d\n", flag);
		}
	}
	if(flag > 0) {  // Обновление времени если оно получено
		rtcSAM3X8.set_clock(secs, TIME_ZONE);    // обновить внутренние часы
		// обновились, можно и часы i2c обновить
		setTime_RtcI2C(rtcSAM3X8.get_hours(), rtcSAM3X8.get_minutes(), rtcSAM3X8.get_seconds());
		setDate_RtcI2C(rtcSAM3X8.get_days(), rtcSAM3X8.get_months(), rtcSAM3X8.get_years());
		journal.jprintf(" Set time from server: %s ", NowDateToStr());
		journal.jprintf("%s\n", NowTimeToStr());  // Через один глобальный буфер
	} else {
		journal.jprintf(" ERROR update time from server! %s ", NowDateToStr());
		journal.jprintf("%s\n", NowTimeToStr()); // Через один глобальный буфер
	}
	SemaphoreGive (xWebThreadSemaphore);
	return flag;
}

#else
// Функция обновления времени по ntp вызывается из задачи или кнопкой. true - если время обновлено
// Запрос времени от NTP сервера, возвращает время как long
boolean set_time_NTP(void)
{
	unsigned long secs;
	int8_t protocol;
	boolean flag = false;
	IPAddress ip(0, 0, 0, 0);

	journal.jprintf(pP_TIME, "Update time from NTP server: %s\n", HP.get_serverNTP());
	//1. Установить адрес  не забываем работаетм через один сокет, опреации строго последовательные,иначе настройки сбиваются
	WDT_Restart(WDT);                                        // Сбросить вачдог  при ошибке долго ждем

	// Если запущен шедулер то захватываем семафор
	if(SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE) {
		return false;
	}  // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
	// DNS запрос для определения адреса

	if(check_address(HP.get_serverNTP(), ip) == 0) {
		SemaphoreGive (xWebThreadSemaphore);
		return false;
	}  // DNS - ошибка выходим

	// 2. Посылка пакета
	if(!Udp.begin(NTP_LOCAL_PORT, W5200_SOCK_SYS)) {
		journal.jprintf(" UDP fail\n");
		SemaphoreGive (xWebThreadSemaphore);
		return false;
	}
	for(uint8_t i = 0; i < NTP_REPEAT; i++)                                       // Делам 5 попыток получить время
	{
		WDT_Restart(WDT);                                            // Сбросить вачдог
		journal.jprintf(" Send packet NTP, wait . . .\n");
		flag = sendNTPpacket(ip);
		_delay(NTP_REPEAT_TIME);                                             // Ждем, чтобы увидеть, доступен ли ответ:
		if(flag) {
			flag = false;
			if(Udp.parsePacket()) {
				// Пакет получен, значит считываем данные оттуда:
				Udp.read(packetBuffer, NTP_PACKET_SIZE); // считываем содержимое пакета в буфер
				flag = true;                                       // Время получили
			}
		}
		if(flag) {
			// Временная отметка начинается с 40 байта полученного пакета и его длина составляет четыре байта или два слова.
			// Для начала извлекаем два этих слова:
			unsigned long highWord = packetBuffer[40] << 8 | packetBuffer[41];
			unsigned long lowWord = packetBuffer[42] << 8 | packetBuffer[43];
			// Совмещаем четыре байта (два слова) в длинное целое. Это и будет NTP-временем (секунды начиная с 1 января 1990 года):
			secs = (highWord << 16 | lowWord) - 2208988800UL; // Время Unix стартует с 1 января 1970 года. В секундах это 2208988800: Вычитаем 70 лет
			break;                                           // ответ получен выходим
		}
	} // for
	Udp.stop();
	if(flag) {  // Обновление времени если оно получено
		rtcSAM3X8.set_clock(secs, TIME_ZONE);    // обновить внутренние часы
		// обновились, можно и часы i2c обновить
		setTime_RtcI2C(rtcSAM3X8.get_hours(), rtcSAM3X8.get_minutes(), rtcSAM3X8.get_seconds());
		setDate_RtcI2C(rtcSAM3X8.get_days(), rtcSAM3X8.get_months(), rtcSAM3X8.get_years());
		journal.jprintf(" Set time from NTP server: %s ", NowDateToStr());
		journal.jprintf("%s\n", NowTimeToStr());  // Одним оператором есть косяк
	} else {
		journal.jprintf(" ERROR update time from NTP server! %s ", NowDateToStr());
		journal.jprintf("%s\n", NowTimeToStr());  // Одним оператором есть косяк
	}
	SemaphoreGive (xWebThreadSemaphore);

	return flag;
}

// send an NTP request to the time server at the given address
// true если нет ошибок
boolean sendNTPpacket(IPAddress &ip)
{
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	// Initialize values needed to form NTP request
	// (see URL above for details on the packets)
	packetBuffer[0] = 0b11100011;   // LI, Version, Mode
	packetBuffer[1] = 0;            // Stratum, or type of clock
	packetBuffer[2] = 6;            // Polling Interval
	packetBuffer[3] = 0xEC;         // Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12] = 49;
	packetBuffer[13] = 0x4E;
	packetBuffer[14] = 49;
	packetBuffer[15] = 52;
	// all NTP fields have been given values, now
	// you can send a packet requesting a timestamp:
	if(Udp.beginPacket(ip, NTP_PORT, W5200_SOCK_SYS) == 1) {
		Udp.write(packetBuffer, NTP_PACKET_SIZE);
		if(Udp.endPacket() != 1) {
			journal.jprintf("Send packet NTP error\n");
			return false;
		}
	} else {
		journal.jprintf("Socket error\n");
		return false;
	}
	return true;
}
#endif // HTTP_TIME_REQUEST

//  Получить текущее время (с секундами!) в виде строки
char* NowTimeToStr()
{
  uint32_t x;
  static char _tmp[12];  // Длина xx:xx:xx - 10+1 символов
  x=rtcSAM3X8.get_hours();
  if (x<10) strcpy(_tmp,cZero); else strcpy(_tmp,"");  _itoa(x,_tmp); strcat(_tmp,":");

  x=rtcSAM3X8.get_minutes();
  if (x<10) strcat(_tmp,cZero);   _itoa(x,_tmp); strcat(_tmp,":");

   x=rtcSAM3X8.get_seconds();
  if (x<10) strcat(_tmp,cZero);   _itoa(x,_tmp); 
  
   return _tmp;
}
//  Получить текущее время (без секунд!) в виде строки
char* NowTimeToStr1()
{
  uint32_t x;
  static char _tmp[8];   // Длина xx:xx - 5+1 символов
  x=rtcSAM3X8.get_hours();
  if (x<10) strcpy(_tmp,cZero); else strcpy(_tmp,"");  _itoa(x,_tmp); strcat(_tmp,":");

  x=rtcSAM3X8.get_minutes();
  if (x<10) strcat(_tmp,cZero);   _itoa(x,_tmp); 
 
  return _tmp;
}

//  Получить текущую дату в виде строки
char* NowDateToStr()
{
  static char _tmp[16];  // Длина xx/xx/xxxx - 10+1 символов
  m_snprintf((char *)&_tmp, sizeof(_tmp), FORMAT_DATE_STR, rtcSAM3X8.get_days(), rtcSAM3X8.get_months(), rtcSAM3X8.get_years());
  return _tmp;
}

// (Длительность инервала в строку) Время в формате день day 12:34 используется для рассчета uptime
// Результат ДОБАВЛЯЕТСЯ в ret
char* TimeIntervalToStr(uint32_t idt,char *ret,uint8_t fSec = 0)
{
    uint32_t Day;
    uint8_t  Hour, Min, Sec;
  /* decode the interval into days, hours, minutes, seconds */
  if(fSec) Sec = idt % 60;
  idt /= 60;
  Min = idt % 60;
  idt /= 60;
  Hour = idt % 24;
  idt /= 24;
  Day = idt;
  if(Day>0)  { _itoa(Day,ret); strcat(ret,"d ");}  // если есть уже дни
  if(Hour>0) { _itoa(Hour,ret);strcat(ret,"h ");}
  if(!fSec || Min > 0) { _itoa(Min,ret); strcat(ret,"m "); }
  if(fSec) { _itoa(Sec, ret); strcat(ret,"s"); }
  return ret;       
}

// вывод Времени в формате 12:34:34 
char* TimeToStr(uint32_t idt)
{
static char _tmp[12];  // Длина xx:xx:xx - 10+1 символов
strcpy(_tmp,"");  // очистить строку

// Время
_itoa((idt % 86400L)/3600,_tmp);       // показываем час (86400  - это количество секунд в сутках)
strcat(_tmp,":");
if (((idt % 3600) / 60) < 10) strcat(_tmp,cZero);  // у первых 10 минут каждого часа впереди должна стоять цифра «0»:
_itoa((idt % 3600)/60,_tmp);           // показываем минуту (3600 – это количество секунд в минуту)  
strcat(_tmp,":");
if ((idt % 60) < 10)  strcat(_tmp,cZero);          // у первых 10 секунд каждой минуты впереди должна стоять цифра «0»:
_itoa(idt % 60,_tmp);                  // показываем секунду

strcat(_tmp," ");

return _tmp;       
}

// вывод Времи и даты  в формате 12:34 11/11/2016
// Результат ДОБАВЛЯЕТСЯ в ret
//http://stackoverflow.com/questions/21593692/convert-unix-timestamp-to-date-without-system-libs
char*  DecodeTimeDate(uint32_t idt,char *ret)
{
  uint32_t seconds, minutes, hours, days, year, month;
  uint32_t dayOfWeek;
  int x;

  if(idt == 0) return ret;
  seconds=idt;
  /* calculate minutes */
  minutes  = seconds / 60;
  seconds -= minutes * 60;
  /* calculate hours */
  hours    = minutes / 60;
  minutes -= hours   * 60;
  /* calculate days */
  days     = hours   / 24;
  hours   -= days    * 24;

  /* Unix time starts in 1970 on a Thursday */
  year      = 1970;
  dayOfWeek = 4;

  while(1)
  {
    bool     leapYear   = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    uint16_t daysInYear = leapYear ? 366 : 365;
    if (days >= daysInYear)
    {
      dayOfWeek += leapYear ? 2 : 1;
      days      -= daysInYear;
      if (dayOfWeek >= 7)
        dayOfWeek -= 7;
      ++year;
    }
    else
    {
      dayOfWeek  += days;
      dayOfWeek  %= 7;
      /* calculate the month and day */
      static const uint8_t daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
      for(month = 0; month < 12; ++month)
      {
        uint8_t dim = daysInMonth[month];
        /* add a day to feburary if this is a leap year */
        if (month == 1 && leapYear)
          ++dim;
        if (days >= dim)
          days -= dim;
        else
          break;
      }
      break;
    }
  }
// формирование строки
  x=hours;
  if (x<10) strcat(ret,cZero); else strcat(ret,"");  _itoa(x,ret); strcat(ret,":");
  x=minutes;
  if (x<10) strcat(ret,cZero);   _itoa(x,ret); strcat(ret,":");
   x=seconds;
  if (x<10) strcat(ret,cZero);   _itoa(x,ret); 
  strcat(ret," "); 
  m_snprintf(ret + m_strlen(ret), 16, FORMAT_DATE_STR, days + 1, month+1, year);
  return ret;   
}

// вывод даты  в формате 010218 для графиков статистики (всегда 6 байт+1)
// forma тип вывода true - краткий (ДДММГГ) false - полный (Д/М/ГГГГ)
// результат ДОБАВЛЯЕТСЯ в ret
//http://stackoverflow.com/questions/21593692/convert-unix-timestamp-to-date-without-system-libs
char*  StatDate(uint32_t idt,boolean forma,char *ret)
{
  uint32_t seconds, minutes, hours, days, year, month;
  uint32_t dayOfWeek;
  int x;
  seconds=idt;

  /* calculate minutes */
  minutes  = seconds / 60;
  seconds -= minutes * 60;
  /* calculate hours */
  hours    = minutes / 60;
  minutes -= hours   * 60;
  /* calculate days */
  days     = hours   / 24;
  hours   -= days    * 24;

  /* Unix time starts in 1970 on a Thursday */
  year      = 1970;
  dayOfWeek = 4;
  while(1)
  {
    bool     leapYear   = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    uint16_t daysInYear = leapYear ? 366 : 365;
    if (days >= daysInYear)
    {
      dayOfWeek += leapYear ? 2 : 1;
      days      -= daysInYear;
      if (dayOfWeek >= 7)
        dayOfWeek -= 7;
      ++year;
    }
    else
    {
      dayOfWeek  += days;
      dayOfWeek  %= 7;
      /* calculate the month and day */
      static const uint8_t daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
      for(month = 0; month < 12; ++month)
      {
        uint8_t dim = daysInMonth[month];
        /* add a day to feburary if this is a leap year */
        if (month == 1 && leapYear)
          ++dim;
        if (days >= dim)
          days -= dim;
        else
          break;
      }
      break;
    }
  }

  if(forma)  // Тип вывода 
   { 
    x=days+1;  if (x<10) strcat(ret,cZero);itoa(x,ret,10);   
    x=month+1; if (x<10) strcat(ret,cZero);itoa(x,ret,10);
    x=year;    if (x>2000) itoa(x-2000,ret,10); else itoa(x-1900,ret,10); // формирование строки формата ДДММГГ (год последние две цифры)
    }
 else
    {itoa(days + 1,ret,10);strcat(ret,"/");itoa(month+1,ret,10);;strcat(ret,"/");itoa(year,ret,10); }
 return ret;   
}

// Перевод формата времени Time в формат Unix (секунды с 1970 года)
#define SEC_1970_TO_2000      946684800
static  const uint8_t dim[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
unsigned long TimeToUnixTime(tmElements_t t) //[V]*
 {
    uint16_t  dc;
    dc = t.Day;
    for (uint8_t i = 0; i<(t.Month-1); i++) dc += dim[i];
    if ((t.Month > 2) && (((t.Year-2000) % 4) == 0))  ++dc;
    dc = dc + (365 * (t.Year-2000)) + (((t.Year-2000) + 3) / 4) - 1;
    return ((((((dc * 24L) + t.Hour) * 60) + t.Minute) * 60) + t.Second) + SEC_1970_TO_2000;
 } 
