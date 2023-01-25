/*
 * Copyright (c) 2016-2023 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
 * &                       by Vadim Kulakov vad7@yahoo.com, vad711
 * Расписание
 * Автор vad711, vad7@yahoo.com
 *
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
//
#ifndef Scheduler_h
#define Scheduler_h

#include "Constant.h"

#define SCHDLR_Profile_off	-1
#define SCHDLR_NotActive	-2

#define bScheduler_active	1

const char WEB_SCH_On[] = "On";
const char WEB_SCH_Active[] = "Active";
const char WEB_SCH_Names[] = "n_list";
const char WEB_SCH_Name[] = "Name";
const char WEB_SCH_Calendar[] = "Calendar";
const char WEB_SCH_ActiveName[] = "AN";
const char WEB_SCH_AutoSelectMonth[] = "ASM";
const char WEB_SCH_AutoSelectWeek[] = "ASW";

struct Scheduler_Calendar_Item {
	uint8_t		WD_Hour; 	// День недели (3bits, 0 = monday)  + час (5bits)
	int8_t		Profile;	// 0x80+Номер профиля(макс 27)+1, 0 = выключен, 127 - ничего не менять, иначе температура -10,0..+12,6
} __attribute__((packed));

struct Scheduler_Data {
	uint8_t		Flags;							// bScheduler_*
	uint8_t		Active;							// Активное расписание
	char 		Names[MAX_CALENDARS][32];		// Названия (русские - 2 байта)
	uint8_t		Timetable[TIMETABLES_MAXSIZE]; 	// буфер для раписаний: {len},{{WD+H},{Profile+1}},..., {len},{{WD+H},{Profile+1}},...,
	uint8_t		AutoSelectMonthWeek[MAX_CALENDARS]; // Автовыбор расписания в 00:00, f + месяц(1..12) + неделя(0..3), 0 - нет
} __attribute__((packed));

class Scheduler
{
public:
	Scheduler();
	int8_t   calc_active_profile(void);					// Возвращает профиль (или -1) по календарю и текущему времени
	int16_t  get_temp_change(void);						// Возвращает на сколько корректировать целевую температуру, сотые градуса
	boolean  IsShedulerOn() { return GETBIT(sch_data.Flags, bScheduler_active); }; // Расписание включено?
	uint16_t Timetable_ptr(uint8_t num);				// Возвращает указатель на запись календаря в TimeTable по его номеру
	void	 web_get_param(char *param, char* result);	// Вернуть строку параметра для веба
	uint8_t  web_set_param(char *param, char *val);		// Установить параметр из веба
	const char* get_note(void); 						// Получить описание
	const char* get_name(void) { return  "Scheduler"; } // Получить имя
	int8_t   save(void);  			               		// Записать настройки в eeprom i2c
	int16_t  load(uint8_t *data = NULL);  		     	// Считать настройки из eeprom i2c, возращает длину или ошибку
	int8_t   loadFromBuf(byte *buf);  					// Считать настройки из буфера, возврат код ошибки
	uint16_t get_crc16(uint8_t *data);             		// Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
	int8_t   check_crc16_eeprom(void);
	uint16_t get_data_size(void) { return sizeof(sch_data); }
	Scheduler_Data sch_data;
private:
	uint8_t current_hour;
	int16_t current_change; // -10,00..+10,00
};

#endif
