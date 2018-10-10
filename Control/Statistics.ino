/*
 * Графики, история, статистика
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
#include "Statistics.h"
#include "HeatPump.h"


void Statistics::Init()
{
	Reset();
}

// Сбросить накопленные промежуточные значения
void Statistics::Reset()
{
	for(uint8_t i = 0; i < sizeof(Stats_data) / sizeof(Stats_data[0]); i++) {
		switch(Stats_data[i].type){
		case STATS_TYPE_MIN:
			Stats_data[i].value = 2147483647;
			break;
		case STATS_TYPE_MAX:
			Stats_data[i].value = -2147483647;
			break;
		default:
			Stats_data[i].value = 0;
		}
	}
	counts = 0;
	counts_work = 0;
	day = rtcSAM3X8.get_days();
	previous = millis();
}

// Обновить статистику, вызывается часто, раз в TIME_READ_SENSOR
void Statistics::Update()
{
	uint32_t tm = millis() - previous;
	previous = millis();
	uint8_t d = rtcSAM3X8.get_days();
	if(d != day) {
		Save();
		Reset();
	}
	int32_t newval = 0;
	boolean compressor_on = HP.is_compressor_on();
	for(uint8_t i = 0; i < sizeof(Stats_data) / sizeof(Stats_data[0]); i++) {
		switch(Stats_data[i].object) {
		case STATS_OBJ_Temp:
			newval = HP.sTemp[Stats_data[i].number].get_Temp() / 10;
			break;
		case STATS_OBJ_Press:
			newval = HP.sADC[Stats_data[i].number].get_Press() / 10;
			break;
		case STATS_OBJ_Voltage:
		    #ifdef USE_ELECTROMETER_SDM
			newval = HP.dSDM.get_Voltage();
			#endif
			break;
		case STATS_OBJ_Power:
			if(Stats_data[i].number == OBJ_powerCO) { // Система отопления
				newval = HP.powerCO; // Вт
			} else if(Stats_data[i].number == OBJ_powerGEO) { // Геоконтур
				newval = HP.powerGEO; // Вт
			} else if(Stats_data[i].number == OBJ_power220) { // Геоконтур
				newval = HP.power220; // Вт
			} else continue;
			switch(Stats_data[i].type) {
			case STATS_TYPE_SUM:
			case STATS_TYPE_AVG:
			case STATS_TYPE_AVG_WORK:
				newval = newval * tm / 3600; // в мВт
			}
			break;
		case STATS_OBJ_COP:
			if(Stats_data[i].number == OBJ_COP_Compressor) {
				newval = HP.COP;
			} else if(Stats_data[i].number == OBJ_COP_Full) {
				newval = HP.fullCOP;
			}
			break;
		case STATS_OBJ_Time:
			if(Stats_data[i].number == OBJ_Compressor && compressor_on) break;
			else if(Stats_data[i].number == OBJ_Sun && GETBIT(HP.flags, fHP_SunActive)) break;
			continue;
		default: continue;
		}
		switch(Stats_data[i].type){
		case STATS_TYPE_MIN:
			if(newval < Stats_data[i].value) Stats_data[i].value = newval;
			break;
		case STATS_TYPE_MAX:
			if(newval > Stats_data[i].value) Stats_data[i].value = newval;
			break;
		case STATS_TYPE_AVG:
		case STATS_TYPE_SUM:
			Stats_data[i].value += newval;
			break;
		case STATS_TYPE_AVG_WORK:
			if(!compressor_on) continue;
			Stats_data[i].value += newval;
			break;
		case STATS_TYPE_CNT:
			Stats_data[i].value += tm;
			break;
		}
	}
	counts++;
	if(compressor_on) counts_work++;
	//if(counts % 30 == 0) Save();
}

void Statistics::ReturnFieldHeader(char *ret, uint8_t i, uint8_t flag)
{
	switch(Stats_data[i].object) {
	case STATS_OBJ_Temp:
		if(flag) strcat(ret, "T"); // ось температур
		strcat(ret, HP.sTemp[Stats_data[i].number].get_note());
		break;
	case STATS_OBJ_Press:
		if(flag) strcat(ret, "P"); // ось давление
		strcat(ret, HP.sADC[Stats_data[i].number].get_note());
		break;
	case STATS_OBJ_Voltage:
		if(flag) strcat(ret, "V"); // ось напряжение
		strcat(ret, "Напряжение, V");
		break;
	case STATS_OBJ_Power:
		if(flag) strcat(ret, "W"); // ось мощность
		if(Stats_data[i].number == OBJ_powerCO) { // Система отопления
			strcat(ret, "Выработано, кВтч"); // хранится в Вт
		} else if(Stats_data[i].number == OBJ_powerGEO) { // Геоконтур
			strcat(ret, "Геоконтур, кВтч"); // хранится в Вт
		} else if(Stats_data[i].number == OBJ_power220) { // Геоконтур
			strcat(ret, "Потребление, кВтч"); // хранится в Вт
		}
		break;
	case STATS_OBJ_COP:
		if(flag) strcat(ret, "C"); // ось COP
		if(Stats_data[i].number == OBJ_COP_Compressor) {
			strcat(ret, "КОП");
		} else if(Stats_data[i].number == OBJ_COP_Full) {
			strcat(ret, "Полный КОП");
		}
		break;
	case STATS_OBJ_Time:
		if(flag) strcat(ret, "M"); // ось часы
		if(Stats_data[i].number == OBJ_Compressor) {
			strcat(ret, "Моточасы, м"); break;
		} else if(Stats_data[i].number == OBJ_Sun) {
			strcat(ret, "СК время, м"); break;
		}
	default: strcat(ret, "?");;
	}
	switch(Stats_data[i].type){
	case STATS_TYPE_MIN:
		strcat(ret, " - Мин");
		break;
	case STATS_TYPE_MAX:
		strcat(ret, " - Макс");
		break;
	case STATS_TYPE_AVG:
		strcat(ret, " - Сред");
		break;
	case STATS_TYPE_AVG_WORK:
		strcat(ret, " - Сред(компр)");
		break;
	}
}

// Возвращает файл с заголовками полей
void Statistics::ReturnFileHeader(char *ret)
{
	for(uint8_t i = 0; i < sizeof(Stats_data) / sizeof(Stats_data[0]); i++) {
		if(i > 0) strcat(ret, ";");
		ReturnFieldHeader(ret, i, 1);
	}
}

void Statistics::ReturnFieldString(char *ret, uint8_t i)
{
	float val = Stats_data[i].type == STATS_TYPE_AVG ? Stats_data[i].value / counts : Stats_data[i].type == STATS_TYPE_AVG_WORK ? Stats_data[i].value / counts_work : Stats_data[i].value;
	switch(Stats_data[i].object) {
	case STATS_OBJ_Temp:
	case STATS_OBJ_Press:
		_ftoa(ret, val / 10, 1);	 // bar
		break;
	case STATS_OBJ_Voltage:
		_ftoa(ret, val, 0);
		break;
	case STATS_OBJ_Power:
		_ftoa(ret, val / 1000000, 3); // кВт*ч
		break;
	case STATS_OBJ_COP:
		_ftoa(ret, val / 100, 2);
		break;
	case STATS_OBJ_Time:
		_ftoa(ret, val / 60000, 1); // минуты
		break;
	}
}

// Строка со значениями за день (разделитель ";"), при запуске не из Update() возможны неверные данные!
void Statistics::ReturnFileString(char *ret)
{
	for(uint8_t i = 0; i < sizeof(Stats_data) / sizeof(Stats_data[0]); i++) {
		if(i > 0) strcat(ret, ";");
		ReturnFieldString(ret, i);
	}
}

void Statistics::ReturnWebTable(char *ret)
{
	for(uint8_t i = 0; i < sizeof(Stats_data) / sizeof(Stats_data[0]); i++) {
		ReturnFieldHeader(ret, i, 0);
		strcat(ret, "|");
		ReturnFieldString(ret, i);
		strcat(ret, ";");
	}
}

// Записать статистику на SD
void Statistics::Save()
{
/*	journal.printf("Stats(%d): \n", counts);

	for(uint8_t i = 0; i < sizeof(Stats_data) / sizeof(Stats_data[0]); i++) {
		journal.printf("%d: %d.%d.%d = %d\n", i, Stats_data[i].object, Stats_data[i].type, Stats_data[i].number, Stats_data[i].value);
	}

	if(HP.get_fSD()) {

	}
	*/
}
