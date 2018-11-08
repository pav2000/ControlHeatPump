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
//
#ifndef Statistics_h
#define Statistics_h
#include "Constant.h"

//#define STATS_DO_NOT_SAVE
#define SD_BLOCK			512
#define STATS_MAX_RECORD_LEN	(15 + sizeof(Stats_data) / sizeof(Stats_data[0]) * 8)
#define STATS_MAX_FILE_SIZE		((STATS_MAX_RECORD_LEN * 366 / SD_BLOCK + 1) * SD_BLOCK)

#define HISTORY_MAX_RECORD_LEN	(15 + sizeof(HistorySetup) / sizeof(HistorySetup[0]) * 5)
#define HISTORY_MAX_FILE_SIZE	((HISTORY_MAX_RECORD_LEN * 1440 * 366 / SD_BLOCK + 1) * SD_BLOCK)

#define MAX_INT32_VALUE 2147483647
#define MIN_INT32_VALUE -2147483647

// what:
#define ID_STATS 	0
#define ID_HISTORY	1

enum {
	STATS_TYPE_MIN = 0,
	STATS_TYPE_AVG,
	STATS_TYPE_MAX,
	STATS_TYPE_SUM,
	STATS_TYPE_TIME // Time, ms
};

enum { // когда
	STATS_WHEN_ALWAYS = 0,
	STATS_WHEN_WORKD			// Во время работы компрессора, прошло STATS_WORKD_TIME
	//STATS_WORK				// Во время работы компрессора
};
#define STATS_WORKD_TIME 60000 // ms

//static char *stats_format = { "%.1f", "" }; // printf format string

struct Stats_Data {
	int32_t		value;			// Для среднего, макс единица: +-1491308
	uint8_t		object;			// STATS_OBJ_*
	uint8_t		type;			// STATS_TYPE_*
	uint8_t		when;			// STATS_WHEN_*
	uint8_t 	number;			// номер датчика
};

Stats_Data Stats_data[] = {
	{ 0, STATS_OBJ_Temp, STATS_TYPE_MIN, STATS_WHEN_ALWAYS, TOUT },
	{ 0, STATS_OBJ_Temp, STATS_TYPE_AVG, STATS_WHEN_ALWAYS, TOUT },
	{ 0, STATS_OBJ_Temp, STATS_TYPE_MAX, STATS_WHEN_ALWAYS, TOUT },
	{ 0, STATS_OBJ_Temp, STATS_TYPE_AVG, STATS_WHEN_ALWAYS, TIN },
	{ 0, STATS_OBJ_Temp, STATS_TYPE_AVG, STATS_WHEN_WORKD, TEVAING },
	{ 0, STATS_OBJ_Temp, STATS_TYPE_AVG, STATS_WHEN_ALWAYS, TBOILER },
	{ 0, STATS_OBJ_Power, STATS_TYPE_SUM, STATS_WHEN_ALWAYS, OBJ_powerCO },
	{ 0, STATS_OBJ_Power, STATS_TYPE_SUM, STATS_WHEN_ALWAYS, OBJ_power220 },
	{ 0, STATS_OBJ_Power, STATS_TYPE_MAX, STATS_WHEN_ALWAYS, OBJ_power220 },
	{ 0, STATS_OBJ_COP, STATS_TYPE_MIN, STATS_WHEN_WORKD, OBJ_COP_Full },
	{ 0, STATS_OBJ_COP, STATS_TYPE_AVG, STATS_WHEN_WORKD, OBJ_COP_Full },
	{ 0, STATS_OBJ_Voltage, STATS_TYPE_MIN, STATS_WHEN_ALWAYS, 0 },
	{ 0, STATS_OBJ_Voltage, STATS_TYPE_AVG, STATS_WHEN_ALWAYS, 0 },
	{ 0, STATS_OBJ_Voltage, STATS_TYPE_MAX, STATS_WHEN_ALWAYS, 0 },
	{ 0, STATS_OBJ_Compressor, STATS_TYPE_TIME, STATS_WHEN_ALWAYS, 0 }
#ifdef USE_SUN_COLLECTOR
	,{ 0, STATS_OBJ_Sun, STATS_TYPE_TIME, STATS_WHEN_ALWAYS, 0 }
#endif
#ifdef PGEO
	,{ 0, STATS_OBJ_Press, STATS_TYPE_MIN, STATS_WHEN_ALWAYS, PGEO }
#endif
#ifdef POUT
	,{ 0, STATS_OBJ_Press, STATS_TYPE_MIN, STATS_WHEN_ALWAYS, POUT }
#endif
};

const char stats_file_start[] = "stats_";
const char stats_file_header[] = "head";
const char stats_file_ext[] = ".csv";
const char history_file_start[] = "hist_";
uint8_t stats_buffer[SD_BLOCK];
uint8_t history_buffer[SD_BLOCK];

class Statistics
{
public:
	void	Init(uint8_t newyear = 0);
	void	Update();						// Обновить статистику, раз в период
	void	UpdateEnergy();					// Обновить энергию и COP, вызывается часто
	void	Reset();						// Сбросить накопленные промежуточные значения
	int8_t	SaveStats(uint8_t newday);		// Записать статистику на SD
	int8_t 	SaveHistory(uint8_t from_web);	// Записать буфер истории на SD
	void	ReturnFileHeader(char *buffer);	// Возвращает файл с заголовками полей
	void	ReturnFieldHeader(char *ret, uint8_t i, uint8_t flag);
	inline void	ReturnFileString(char *ret);// Строка со значениями за день
	void	ReturnFieldString(char **ret, uint8_t i);
	void	ReturnWebTable(char *ret);
	void	SendFileData(uint8_t thread, SdFile *File, char *filename);
	boolean	FindEndPosition(uint8_t what);
	void	CheckCreateNewFile();
	int8_t	CreateOpenFile(uint8_t what);
	void	History();						// Логирование параметров работы ТН, раз в 1 минуту
private:
	void	Error(const char *text, uint8_t what);
	uint16_t counts;						// Кол-во уже совершенных обновлений
	uint16_t counts_work;					// Кол-во уже совершенных обновлений во время работы компрессора
	uint32_t compressor_on_timer;
	uint32_t previous;
	uint8_t	 day;
	uint8_t	 month;
	uint16_t year;
	uint32_t BlockStart;
	uint32_t BlockEnd;
	uint32_t CurrentBlock;
	uint16_t CurrentPos;
	uint32_t HistoryBlockStart;
	uint32_t HistoryBlockEnd;
	uint32_t HistoryCurrentBlock;
	uint16_t HistoryCurrentPos;
	SdFile   StatsFile;
};

Statistics Stats;

#endif

