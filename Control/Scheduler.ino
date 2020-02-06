/*
 * Copyright (c) 2016-2020 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
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
#include "Config.h"

#include "Scheduler.h"

#define ifparam(s) if(strncmp(param, s, sizeof(s)-1) == 0)

Scheduler::Scheduler() {
	memset(&sch_data, 0, sizeof(sch_data));
	for(uint8_t i = 0; i < MAX_CALENDARS; i++) {
		strcpy(sch_data.Names[i], "Расписание 1");
		sch_data.Names[i][strlen(sch_data.Names[i]) - 1] += i;
	}
}

// Получить описание
const char* Scheduler::get_note(void)
{
	return sch_data.Flags & (1<<bScheduler_active) ? " активно" : " выключено";
}

// Возвращает указатель на запись календаря в TimeTable по его номеру
uint16_t Scheduler::Timetable_ptr(uint8_t num)
{
	uint16_t i = 0;
	while(num--) if((i += sch_data.Timetable[i] + 1) >= TIMETABLES_MAXSIZE - 1) return TIMETABLES_MAXSIZE - 1;
	return i;
}

// Установка профиля по календарю и текущему времени
// Возврат: -2 - расписание не активно, -1 - пустой календарь
int8_t Scheduler::calc_active_profile(void)
{
	Scheduler_Calendar_Item *item;
	uint8_t cal_dw, cal_h, dw;
	uint16_t ptr, max, found = 0;

	current_hour = rtcSAM3X8.get_hours();
	if((sch_data.Flags & (1<<bScheduler_active)) == 0) {
		current_change = 0;
		return SCHDLR_NotActive;
	}
	ptr = Timetable_ptr(sch_data.Active);
	dw = rtcSAM3X8.get_day_of_week(); // 0 - вск
	if(dw == 0) dw = 6; else dw--; // 0 - пон
	max = ptr + sch_data.Timetable[ptr];
	if(ptr == max || max > TIMETABLES_MAXSIZE) {
		current_change = 0;
		return SCHDLR_Profile_off; // Пустой календарь
	}
	for(ptr++; ptr < max; ptr += sizeof(Scheduler_Calendar_Item)) {
		item = (Scheduler_Calendar_Item *)&sch_data.Timetable[ptr];
		cal_dw = item->WD_Hour >> 5; 	// День недели
		cal_h = item->WD_Hour & 0x1F; 	// Час
		if(cal_dw > dw || (cal_dw == dw && cal_h > current_hour)) break;
		found = ptr;
	}
	if(found) {
		item = (Scheduler_Calendar_Item *)&sch_data.Timetable[found];
	} else { // Берем последнюю
		item = (Scheduler_Calendar_Item *)&sch_data.Timetable[max - sizeof(Scheduler_Calendar_Item) + 1];
	}
	if((item->Profile == 0)) { // Set profile
		current_change = 0;
		return -1;
	} else if((item->Profile < -100)) { // Set profile
		current_change = 0;
		return 128 + item->Profile - 1;
	} else { // change t
		current_change = item->Profile * 10;
		return HP.Prof.get_idProfile();
	}
}

int16_t Scheduler::get_temp_change(void)
{
	if(rtcSAM3X8.get_hours() != current_hour) calc_active_profile();
	return current_change;
}

// Вернуть строку параметра для веба, get_SCHDLR(param)
void Scheduler::web_get_param(char *param, char *result)
{
	uint8_t cnum;
	result += strlen(result);
	ifparam(WEB_SCH_On) {
		strcat(result, sch_data.Flags & (1<<bScheduler_active) ? cOne : cZero);
	} else ifparam(WEB_SCH_Active) {
		itoa(sch_data.Active, result, 10);
	} else ifparam(WEB_SCH_Names) {
		uint8_t i = 0;
		for(; i < MAX_CALENDARS; i++) {
			strcat(result, sch_data.Names[i]);
			if(i == sch_data.Active) strcat(result, ":1;"); else strcat(result, ":0;");
		}
	} else ifparam(WEB_SCH_Calendar) { // get_SCHDLR(CalendarX), X - расписание, если пусто, то активное.
		// Возврат: {wd+h};{profile+1|};... Если профайл -1(255), то вывод ""
		if((cnum = param[sizeof(WEB_SCH_Calendar)-1]) == '\0') cnum = sch_data.Active; else cnum -= '0';
		if(cnum >= MAX_CALENDARS) {
			strcat(result, "E33");
		} else {
			uint16_t ptr = Timetable_ptr(cnum);
			uint16_t max = ptr + sch_data.Timetable[ptr];
			if(max > TIMETABLES_MAXSIZE) return;
			for(ptr++; ptr <= max; ptr++) {
				result += m_strlen(result);
				if(sch_data.Timetable[ptr]) itoa(sch_data.Timetable[ptr], result, 10);
				if(ptr < max) strcat(result, ";");
			}
		}
	} else ifparam(WEB_SCH_ActiveName) { // get_SCHDLR(AN)
		if(sch_data.Flags & (1<<bScheduler_active)) strcat(result, sch_data.Names[sch_data.Active]); else strcat(result, "-");
	}
//#if DEBUG
//	journal.printf("WEB_GET: %s=%s\n", param, result);
//#endif
}

// Установить параметр из веба, set_SCHDLR(param=val)
// Возврат: 0 - ok, 1 - неверный номер расписания, 2 - не хватает места для календаря
uint8_t Scheduler::web_set_param(char *param, char *val)
{
	uint8_t cnum;
	ifparam(WEB_SCH_On) {
		if(atoi(val)) { // Вкл.
			sch_data.Flags |= (1<<bScheduler_active);
		} else { // Выкл.
			sch_data.Flags &= ~(1<<bScheduler_active);
			if (HP.get_State() == pWAIT_HP) HP.sendCommand(pRESUME);   // Если расписание не активно и есть режим ожидания (т.е.изменение флага расписания) надо подать команду старт
		}
	} else ifparam(WEB_SCH_Active) {
		cnum = atoi(val);
		if(cnum >= MAX_CALENDARS) return 1;
		sch_data.Active = cnum;
	} else ifparam(WEB_SCH_Name) { // NameX, X - номер
		if((cnum = param[sizeof(WEB_SCH_Name)-1]) == '\0') cnum = sch_data.Active; else cnum -= '0';
		if(cnum >= MAX_CALENDARS) return 1;
		urldecode(sch_data.Names[cnum], val, sizeof(sch_data.Names[0]));
	} else ifparam("Clear") {
		memset(&sch_data.Timetable, 0, sizeof(sch_data.Timetable));
	} else ifparam(WEB_SCH_Calendar) { // CalendarX=str, X - расписание, если пусто, то активное; str = length;{wday+h;profile|-1; wday+h;profile|-1; ...}
		char *p;
		uint16_t size, cur_size, cur_ptr = 0xFFFF;
		if((cnum = param[sizeof(WEB_SCH_Calendar)-1]) == '\0') cnum = sch_data.Active; else cnum -= '0';
		if(cnum >= MAX_CALENDARS) return 1;
		while((p = strpbrk(val, ";"))) {
			p[0] = 0;
			uint8_t dec = atoi(val);
			val = p + 1;
			if(cur_ptr == 0xFFFF) {
				size = Timetable_ptr(MAX_CALENDARS-1);
				size += 1 + sch_data.Timetable[size];
				cur_ptr = Timetable_ptr(cnum);
				cur_size = sch_data.Timetable[cur_ptr];
				if(size - cur_size + dec > TIMETABLES_MAXSIZE) { // не лезет в память
					journal.jprintf("Scheduler full: %d - %d + %d > %d\n", size, cur_size, dec, TIMETABLES_MAXSIZE);
					return 2;
				}
				if(cnum < MAX_CALENDARS - 1) {
					cur_size += cur_ptr + 1;
					memmove(sch_data.Timetable + cur_ptr + 1 + dec, sch_data.Timetable + cur_size, size - cur_size - 1);
				}
				sch_data.Timetable[cur_ptr++] = dec;
				if(dec == 0) return OK;
			} else {
				sch_data.Timetable[cur_ptr++] = dec;
			}
		}
	}
	return OK;
}

// Записать настройки в eeprom i2c, если число меньше 0 это код ошибки
int8_t Scheduler::save(void)
{
    if(writeEEPROM_I2C(I2C_SCHEDULER_EEPROM, (byte*)&sch_data, sizeof(sch_data))) {
        set_Error(ERR_SAVE_EEPROM, (char *) get_name());
        return ERR_SAVE_EEPROM;
    }
    uint16_t crc16 = get_crc16((uint8_t *)&sch_data);
    if(writeEEPROM_I2C(I2C_SCHEDULER_EEPROM + sizeof(sch_data), (byte*)&crc16, sizeof(crc16))) {
    	set_Error(ERR_SAVE_PROFILE, (char *) get_name());
    	return ERR_SAVE_PROFILE;
    }
    int8_t err = check_crc16_eeprom();
    if(err == OK) journal.jprintf("Save scheduler to I2C OK, write: %d bytes, crc: %04x\n", sizeof(sch_data) + sizeof(crc16), crc16);
    return err;
}

// Считать настройки в память по адресу из eeprom i2c, если число меньше 0 это код ошибки
int16_t Scheduler::load(uint8_t *data)
{
	journal.jprintf(" Scheduler ");
	uint16_t ret = check_crc16_eeprom();
#ifdef LOAD_VERIFICATION
	if(ret == OK) {
#endif
		if(data == NULL) {
			data = (uint8_t *)&sch_data;
			ret = 0;
			journal.jprintf("load ");
		} else {
			ret = 2; // +crc16
			journal.jprintf("save ");
		}
	    if(readEEPROM_I2C(I2C_SCHEDULER_EEPROM, data, sizeof(sch_data))) {
	        set_Error(ERR_LOAD_EEPROM, (char *) get_name());
	        journal.jprintf("Error!\n");
	        return ERR_SAVE_EEPROM;
	    } else if(ret) {
	    	uint16_t crc = get_crc16((uint8_t *)&sch_data);
	    	*(uint16_t *)(data + sizeof(sch_data)) = crc;
	    	journal.jprintf("crc: %04x", crc);
	    }
	    ret += sizeof(sch_data);
#ifndef LOAD_VERIFICATION
	if(ret >= 0)
#endif
	    journal.jprintf(" %d bytes Ok.\n", ret);
	} else {
		journal.jprintf("CRC mismatch!\n");
	}
    return ret;
}

// Считать настройки из буфера на входе адрес с какого, на выходе код ошибки
int8_t Scheduler::loadFromBuf(byte* buf)
{
	journal.jprintf(" Load scheduler %d bytes, ", sizeof(sch_data) + 2); // sizeof(crc)
#ifdef LOAD_VERIFICATION
	uint16_t crc = get_crc16(buf);
	journal.jprintf("crc: %04x ", crc);
	if(crc != *(uint16_t *)(buf + sizeof(sch_data))) {
		journal.jprintf("!= %04x - ERROR!\n", *(uint16_t *)(buf + sizeof(sch_data)));
		return ERR_CRC16_EEPROM;
	}
#endif
    memcpy((byte*)&sch_data, buf, sizeof(sch_data));
    journal.jprintf("OK\n");
    return OK;
}

// Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
uint16_t Scheduler::get_crc16(uint8_t *data)
{
	uint16_t crc = 0xFFFF;
	for(uint16_t i = 0; i < sizeof(sch_data); i++) crc = _crc16(crc, data[i]);
	return crc;
}

// Проверить контрольную сумму в EEPROM
int8_t Scheduler::check_crc16_eeprom(void)
{
	uint16_t i, crc2, crc = 0xFFFF;
	uint8_t x;
	for(i = 0; i < sizeof(sch_data); i++) {
		if(readEEPROM_I2C(I2C_SCHEDULER_EEPROM + i, (byte*) &x, sizeof(x))) {
			set_Error(ERR_LOAD_EEPROM, (char *) get_name());
			return ERR_LOAD_EEPROM;
		}
		crc = _crc16(crc, x);
	}
	if(readEEPROM_I2C(I2C_SCHEDULER_EEPROM + i, (byte*) &crc2, sizeof(crc2))) {
		set_Error(ERR_LOAD_EEPROM, (char *) get_name());
		return ERR_LOAD_EEPROM;
	}
	return crc == crc2 ? OK : ERR_CRC16_EEPROM;
}
