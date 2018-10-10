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

#define STAT_PERIOD		60	// Период статистики, сек

enum {
	STATS_OBJ_Temp = 0,		// °C
	STATS_OBJ_Power,		// кВт*ч, пока только TYPE_SUM для OBJ_*
	STATS_OBJ_Press,		// bar or °C
	STATS_OBJ_Flow,			// м³ч
	STATS_OBJ_COP,			//
	STATS_OBJ_Voltage,		// V
	STATS_OBJ_Time,			// время работы, для OBJ_*
};

enum {
	OBJ_Compressor = 0,
	OBJ_Boiler,
	OBJ_Sun,
	OBJ_powerCO,
	OBJ_powerGEO,
	OBJ_power220,
	OBJ_COP_Compressor,
	OBJ_COP_Full
};

enum {
	STATS_TYPE_MIN = 0,
	STATS_TYPE_AVG,
	STATS_TYPE_MAX,
	STATS_TYPE_SUM,
	STATS_TYPE_CNT, // Time, ms
};

//static char *stats_format = { "%.1f", "" }; // printf format string

struct Stats_Data {
	int32_t		value;			// Для среднего, макс единица: +-1491308
	uint8_t		object;			// STATS_OBJ_*
	uint8_t		type;			// STATS_TYPE_*
	uint8_t 	number;			// номер датчика
};

Stats_Data Stats_data[] = {
							{ 0, STATS_OBJ_Temp, STATS_TYPE_MIN, TOUT },
							{ 0, STATS_OBJ_Temp, STATS_TYPE_AVG, TOUT },
							{ 0, STATS_OBJ_Temp, STATS_TYPE_MAX, TOUT },
							{ 0, STATS_OBJ_Temp, STATS_TYPE_AVG, TIN },
							{ 0, STATS_OBJ_Temp, STATS_TYPE_AVG, TBOILER },
							{ 0, STATS_OBJ_Power, STATS_TYPE_SUM, OBJ_powerCO },
							{ 0, STATS_OBJ_Power, STATS_TYPE_SUM, OBJ_power220 },
							{ 0, STATS_OBJ_Power, STATS_TYPE_MAX, OBJ_power220 },
							{ 0, STATS_OBJ_COP, STATS_TYPE_MIN, OBJ_COP_Full },
							{ 0, STATS_OBJ_COP, STATS_TYPE_AVG, OBJ_COP_Full },
							{ 0, STATS_OBJ_Voltage, STATS_TYPE_MIN, 0 },
							{ 0, STATS_OBJ_Voltage, STATS_TYPE_AVG, 0 },
							{ 0, STATS_OBJ_Voltage, STATS_TYPE_MAX, 0 },
							{ 0, STATS_OBJ_Time, STATS_TYPE_CNT, OBJ_Compressor }
#ifdef USE_SUN_COLLECTOR
							,{ 0, STATS_OBJ_Time, STATS_TYPE_CNT, OBJ_Sun }
#endif
#ifdef PGEO
							,{ 0, STATS_OBJ_Press, STATS_TYPE_MIN, PGEO }
#endif
#ifdef POUT
							,{ 0, STATS_OBJ_Press, STATS_TYPE_MIN, POUT }
#endif
						};

class Statistics
{
public:
	void Init();
	void Update();							// Обновить статистику, раз в период
	void UpdateEnergy();					// Обновить энергию и COP, вызывается часто
	void Reset();							// Сбросить накопленные промежуточные значения
	void Save();							// Записать статистику на SD
	void ReturnFileHeader(char *buffer);	// Возвращает файл с заголовками полей
	void ReturnFieldHeader(char *ret, uint8_t i, uint8_t flag));
	void ReturnFileString(char *ret);		// Строка со значениями за день
	void ReturnFieldString(char *ret, uint8_t i);
	void ReturnWebTable(char *ret);
private:
	uint16_t counts;						// Кол-во уже совершенных обновлений
	uint32_t previous;
	uint8_t	 day;
};

Statistics Stats;

#endif

