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

#define temp_initbuf Socket[0].outBuf
const char format_date[] = "%04d%02d%02d";
#define format_date_size 8

void Statistics::Init(uint8_t newyear)
{
	if(!newyear) Reset();
	year = rtcSAM3X8.get_years();
#ifdef STATS_DO_NOT_SAVE
	return;
#endif
	if(!HP.get_fSD()) {
		journal.jprintf(" No SD card - statistics will not be saved!\n");
		return;
	}
	if(newyear && SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE) {  // Захват мютекса потока или ОЖИДАНИНЕ W5200_TIME_WAIT
		journal.jprintf((char*) cErrorMutex, __FUNCTION__, MutexWebThreadBuzy);
		return;
	}
	CurrentBlock = 0;
	CurrentPos = 0;
	char filename[sizeof(stats_file_start)-1 + 4 + sizeof(stats_file_ext)];
	strcpy(filename, stats_file_start);
	_itoa(year, filename);
	strcat(filename, stats_file_ext);
	SPI_switchSD();
	uint8_t newfile = 0;
	if(!card.exists(filename)) {
		if(!StatsFile.createContiguous(filename, STATS_MAX_FILE_SIZE)) {
			Error("create");
			goto xEnd;
		} else {
			StatsFile.timestamp(T_CREATE | T_ACCESS | T_WRITE, rtcSAM3X8.get_years(), rtcSAM3X8.get_months(), rtcSAM3X8.get_days(), rtcSAM3X8.get_hours(), rtcSAM3X8.get_minutes(), rtcSAM3X8.get_seconds());
		}
		newfile = 1;
	} else if(!StatsFile.open(filename, O_RDWR)) {
		Error("open");
		goto xEnd;
	}
	if(!StatsFile.contiguousRange(&BlockStart, &BlockEnd)) {
		journal.jprintf(" Error get blocks!\n");
	} else {
		if(newfile) {
			journal.jprintf(" New stats file: %s\n", filename);
			CurrentBlock = BlockStart;
			memset(stats_buffer, 0, SD_BLOCK);
			for(uint32_t b = BlockStart; b <= BlockEnd; b++) {
				if(!card.card()->writeBlock(b, (uint8_t*)stats_buffer)) {
					Error("empty");
					break;
				}
			}
		} else if(!FindEndPosition((uint8_t*)stats_buffer, BlockStart, BlockEnd)) {
			journal.jprintf(" Endpos not found!\n");
		} else if(!newyear) { // read last record
			int32_t pos = (CurrentBlock - BlockStart) * SD_BLOCK + CurrentPos - 1;
			uint8_t b;
			while(--pos >= 0) {
				if(!StatsFile.seekSet(pos)) {
					Error("seek");
					break;
				}
				if(!StatsFile.read(&b, 1)) {
					Error("readb");
					break;
				}
				if(b == '\n' || pos == 0) {
					if(pos) pos++;
					if(!StatsFile.read(temp_initbuf, STATS_MAX_RECORD_LEN)) {
						Error("readl");
						break;
					}
					m_snprintf(temp_initbuf + STATS_MAX_RECORD_LEN, 16, format_date, year, month, day);
					if(memcmp(temp_initbuf, temp_initbuf + STATS_MAX_RECORD_LEN, format_date_size) == 0) { // date the same
						CurrentBlock = BlockStart + pos / SD_BLOCK;
						CurrentPos = pos % SD_BLOCK;
						temp_initbuf[format_date_size] = '\0';
						journal.printf(" %s restored\n", temp_initbuf);
						if(!card.card()->readBlock(CurrentBlock, (uint8_t*)stats_buffer)) {
							Error("readp");
						} else {
							memcpy(temp_initbuf, stats_buffer + CurrentPos, SD_BLOCK - CurrentPos);
							temp_initbuf[STATS_MAX_RECORD_LEN] = '\0';
							char *p = temp_initbuf + format_date_size;
							if((p = strchr(p, '\n'))) *p = '\0';
							p = temp_initbuf + format_date_size;
							while((p = strchr(p, ';'))) *p++ = '\0';
							p = temp_initbuf + format_date_size;
							for(uint8_t i = 0; i < sizeof(Stats_data) / sizeof(Stats_data[0]); i++) {
								float val = my_atof(++p);
								if(val != ATOF_ERROR) {
									switch(Stats_data[i].object) {
									case STATS_OBJ_Temp:
									case STATS_OBJ_Press:
										Stats_data[i].value = val * 10;
										break;
									case STATS_OBJ_Voltage:
										Stats_data[i].value = val;
										break;
									case STATS_OBJ_Power:
										Stats_data[i].value = val * 1000000;
										break;
									case STATS_OBJ_COP:
										Stats_data[i].value = val * 100;
										break;
									case STATS_OBJ_Time:
										Stats_data[i].value = val * 60000;
										break;
									}
									if(Stats_data[i].type == STATS_TYPE_AVG_WORK && Stats_data[i].value) counts_work = 1;
									if((p = (char*)memchr(p, '\0', STATS_MAX_RECORD_LEN)) == NULL) break;
								}
							}
							counts = 1;
						}
					}
					break;
				}
			}
		}
	}
	StatsFile.close();
xEnd:
	SPI_switchW5200();
	if(newyear) SemaphoreGive(xWebThreadSemaphore);  // Отдать мютекс
}

boolean Statistics::FindEndPosition(uint8_t *buffer, uint32_t bst, uint32_t bend)
{
	uint8_t *pos = NULL;
	uint32_t cur = 0;
	while(cur != bst || cur != bend) {
		WDT_Restart(WDT);
		cur = bst + (bend - bst) / 2;
		if(!card.card()->readBlock(cur, buffer)) {
			Error("fndpos");
			break;
		}
		if(buffer[0] != 0) {
			if((pos = (uint8_t*)memchr(buffer, 0, SD_BLOCK))) break;
			bst = cur;
		} else if(cur == bst) { // empty
			pos = buffer;
			break;
		} else bend = cur;
	}
	if(pos == NULL) return false;
	CurrentBlock = cur;
	CurrentPos = pos - buffer;
#ifdef DEBUG_MODWORK
	journal.jprintf(" Found pos: %u/%u\n", CurrentBlock, CurrentPos);
#endif
	return true;
}

void Statistics::Error(const char *text)
{
	journal.jprintf(" Stats Error %s (%d,%d)!\n", text, card.cardErrorCode(), card.cardErrorData());
}

// Сбросить накопленные промежуточные значения
void Statistics::Reset()
{
	for(uint8_t i = 0; i < sizeof(Stats_data) / sizeof(Stats_data[0]); i++) {
		switch(Stats_data[i].type){
		case STATS_TYPE_MIN:
			Stats_data[i].value = MAX_INT32_VALUE;
			break;
		case STATS_TYPE_MAX:
			Stats_data[i].value = MIN_INT32_VALUE;
			break;
		default:
			Stats_data[i].value = 0;
		}
	}
	counts = 0;
	counts_work = 0;
	day = rtcSAM3X8.get_days();
	month = rtcSAM3X8.get_months();
	previous = millis();
}

// Обновить статистику, вызывается часто, раз в TIME_READ_SENSOR
void Statistics::Update()
{
	if(year == 0) return; // waiting to switch a next year
	uint32_t tm = millis() - previous;
	previous = millis();
	if(rtcSAM3X8.get_days() != day) {
		Save(1);
		Reset();
		if(year != rtcSAM3X8.get_years()) year = 0; // waiting to switch a next year
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
			if(newval == 0) continue;
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
	int32_t val = Stats_data[i].type == STATS_TYPE_AVG ? Stats_data[i].value / counts : Stats_data[i].type == STATS_TYPE_AVG_WORK ? Stats_data[i].value / counts_work : Stats_data[i].value;
	if(val == MIN_INT32_VALUE || val == MAX_INT32_VALUE) val = 0;
	switch(Stats_data[i].object) {
	case STATS_OBJ_Temp:
	case STATS_OBJ_Press:
		int_to_dec_str(val, 10, ret, 1); 	 // bar
		break;
	case STATS_OBJ_Voltage:
		int_to_dec_str(val, 1, ret, 0);
		break;
	case STATS_OBJ_Power:
		int_to_dec_str(val, 1000000, ret, 3);  // кВт*ч
		break;
	case STATS_OBJ_COP:
		int_to_dec_str(val, 100, ret, 2);
		break;
	case STATS_OBJ_Time:
		int_to_dec_str(val, 60000, ret, 1);  // минуты
		break;
	}
}

// Строка со значениями за день (разделитель ";"), при запуске не из Update() возможны неверные данные!
void Statistics::ReturnFileString(char *ret)
{
	m_snprintf(ret, 20, format_date, year, month, day);
	for(uint8_t i = 0; i < sizeof(Stats_data) / sizeof(Stats_data[0]); i++) {
		strcat(ret, ";");
		ReturnFieldString(ret, i);
	}
	strcat(ret, "\n");
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
	if(BlockStart == 0) return;
	uint32_t readed = m_strlen(Socket[thread].outBuf);
	if(sendPacketRTOS(thread, (byte*)Socket[thread].outBuf, readed, 0) != readed) {
		Error("send dh");
		return;
	}
	SPI_switchSD();
	readed = 0;
	for(uint32_t i = BlockStart; i <= BlockEnd; i++) {
		if(!card.card()->readBlock(i, (uint8_t*)Socket[thread].outBuf + readed)) {
			Error("read data");
			break;
		}
		if(Socket[thread].outBuf[readed + SD_BLOCK - 1] == 0) {  // end of data
			readed = (uint8_t*)memchr((uint8_t*)Socket[thread].outBuf + readed, 0, SD_BLOCK) - (uint8_t*)Socket[thread].outBuf;
		} else if((readed += SD_BLOCK) < sizeof(Socket[thread].outBuf)) continue;
		SPI_switchW5200();
		if(sendPacketRTOS(thread, (byte*)Socket[thread].outBuf, readed, 0) != readed) {
			Error("send data");
			break;
		}
		SPI_switchSD();
		readed = 0;
	}
	SPI_switchW5200();
}

void Statistics::CheckCreateNewFile()
{
	if(year == 0) {
		year = rtcSAM3X8.get_years();
		Init(1);
	}
}

// Записать статистику на SD, 0 - только записать, 1 - новый день
void Statistics::Save(uint8_t newday)
{
#ifdef STATS_DO_NOT_SAVE
	return;
#endif
	if(!HP.get_fSD() || CurrentBlock == 0) return;
	char *rbuf = (char*) malloc(STATS_MAX_RECORD_LEN);
	if(rbuf == NULL) {
		journal.jprintf("Stats memory low - not saved!\n");
		return;
	}
	ReturnFileString(rbuf);
	uint16_t lensav, len = m_strlen(rbuf) + 1;
	memcpy(stats_buffer + CurrentPos, rbuf, lensav = SD_BLOCK - CurrentPos < len ? SD_BLOCK - CurrentPos : len);


	journal.printf("Sav: %d, %d\n", CurrentPos, lensav);


#ifdef USE_UPS
	if(!newday || lensav != len) { // save when there is no space in buffer
#endif
		if(SemaphoreTake(xWebThreadSemaphore, 0) == pdFALSE) return;
		SPI_switchSD();
		if(!card.card()->writeBlock(CurrentBlock, (uint8_t*)stats_buffer)) {
			Error("save 1");
//			if(card.cardErrorCode() > SD_CARD_ERROR_NONE && card.cardErrorCode() < SD_CARD_ERROR_READ && card.cardErrorData() == 255) { // reinit card
//				if(card.begin(PIN_SPI_CS_SD, SD_SCK_MHZ(SD_CLOCK))) goto xContinue;
//				else journal.jprintf("Reinit SD card failed!\n");
//			}
		} else if(lensav != len){ // next block
			if(CurrentBlock >= BlockEnd) {
				journal.jprintf("Stats file size exceeded!\n"); // to do: increase file
			} else {
				memset(stats_buffer, 0, SD_BLOCK);
				memcpy(stats_buffer, rbuf, lensav = len - lensav);
				if(!card.card()->writeBlock(CurrentBlock + 1, (uint8_t*)stats_buffer)) {
					Error("save 2");
				} else if(newday) {
					CurrentBlock++;
					CurrentPos = lensav;
				} else { // reread current block
					if(!card.card()->readBlock(CurrentBlock, (uint8_t*)stats_buffer)) {
						Error("read 1");
					}
				}
			}
		} else if(newday) CurrentPos += lensav;
	    SPI_switchW5200();
	    SemaphoreGive(xWebThreadSemaphore);
#ifdef USE_UPS
	} else CurrentPos += lensav;
#endif
	free(rbuf);
}
