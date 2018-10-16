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
#include "SdFat.h"

void Statistics::Init(uint8_t noreset)
{
	if(!noreset) Reset();
#ifdef STATS_DO_NOT_SAVE
	return;
#endif
	if(!HP.get_fSD()) {
		journal.jprintf(" No SD card - statistics will not be saved!\n");
		return;
	}
	CurrentBlock = 0;
	CurrentPos = 0;
	char filename[sizeof(stats_file_start)-1 + 4 + sizeof(stats_file_ext)];
	strcpy(filename, stats_file_start);
	_itoa(rtcSAM3X8.get_years(), filename);
	strcat(filename, stats_file_ext);
	SPI_switchSD();
	uint8_t newfile = 0;
	if(!card.exists(filename)) {
		if(!StatsFile.createContiguous(filename, STATS_MAX_FILE_SIZE)) {
			Error("create");
			return;
		}
		newfile = 1;
	} else if(StatsFile.open(filename, O_RDWR)) {
		Error("open");
		return;
	}
	if(!StatsFile.contiguousRange(&BlockStart, &BlockEnd)) {
		journal.jprintf(" Error get blocks!\n");
	}
	if(newfile) {
		journal.jprintf("Stats new file: %s\n", filename);
		CurrentBlock = BlockStart;
		memset(stats_buffer, 0, SD_BLOCK);
		for(uint32_t b = BlockStart; b <= BlockEnd; b++) {
			if(!card.card()->writeBlock(b, (uint8_t*)stats_buffer)) {
				Error("empty");
				break;
			}
		}
	} else if(!FindEndPosition((uint8_t*)stats_buffer, BlockStart, BlockEnd)) {
		journal.jprintf("Stats endpos not found!\n");
	}
}

boolean Statistics::FindEndPosition(uint8_t *buffer, uint32_t bst, uint32_t bend)
{
	uint8_t *pos = NULL;
	uint32_t cur = 0;
	while(cur != bst || cur != bend) {
		WDT_Restart(WDT);
		cur = bst + (bend - bst) / 2;
		card.card()->readBlock(cur, buffer);
		if(buffer[0] != 0) {
			if((pos = (uint8_t*)memchr(buffer, 0, SD_BLOCK))) break;
			bst = cur;
		} else bend = cur;
	}
	if(pos == NULL) return false;
	CurrentBlock = cur;
	CurrentPos = pos - buffer;
#ifdef DEBUG_MODWORK
	journal.jprintf("Stats found pos: %u, %u\n", CurrentBlock, CurrentPos);
#endif
	return true;
}

void Statistics::Error(const char *text)
{
	journal.jprintf("Stats Error %s (%d,%d)!\n", text, card.cardErrorCode(), card.cardErrorData());
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
	month = rtcSAM3X8.get_months();
	year = rtcSAM3X8.get_years();
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
	m_snprintf(ret, 16, "%d%02d%02d", year, month, day);
	for(uint8_t i = 0; i < sizeof(Stats_data) / sizeof(Stats_data[0]); i++) {
		strcat(ret, ";");
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

void Statistics::SendFileData(uint8_t thread, char *filename)
{


}


void Statistics::CheckCreateNewFile()
{
	if(year != rtcSAM3X8.get_years()) {
		Init(1);
	}
}

// Записать статистику на SD
void Statistics::Save()
{
#ifdef STATS_DO_NOT_SAVE
	return;
#endif
	if(!HP.get_fSD()) return;
	char *rbuf = (char*) malloc(STATS_MAX_RECORD_LEN);
	if(rbuf == NULL) {
		journal.jprintf("Stats memory low - not saved!\n");
		return;
	}
	ReturnFileString(rbuf);
	uint16_t lensav, len = m_strlen(rbuf);
	memcpy(stats_buffer + CurrentPos, rbuf, lensav = SD_BLOCK - CurrentPos < len ? SD_BLOCK - CurrentPos : len);
	if(!card.card()->writeBlock(CurrentBlock, (uint8_t*)stats_buffer)) {
		Error("save 1");
	} else if(lensav != len){ // next block
		if(CurrentBlock >= BlockEnd) {
			journal.jprintf("Stats file size exceeded!\n"); // to do: increase file
		} else {
			CurrentBlock++;
			CurrentPos = 0;
			memset(stats_buffer, 0, SD_BLOCK);
			memcpy(stats_buffer, rbuf, len - lensav);
			if(!card.card()->writeBlock(CurrentBlock, (uint8_t*)stats_buffer)) {
				Error("save 2");
			} else CurrentPos += len;
		}
	} else CurrentPos += len;
	free(rbuf);
}
