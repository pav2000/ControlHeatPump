/*
 * Copyright (c) 2016-2020 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
 * &                       by Vadim Kulakov vad7@yahoo.com, vad711
 * "Народный контроллер" для тепловых насосов.
 * Данное програмное обеспечение предназначено для управления
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
#ifndef SCharts_h
#define SCharts_h

// Определения нужные в этом файле ====================================================================================
//  Перечисляемый тип -ТИПЫ датчиков сухой контакт
enum TYPE_SENSOR
{
	pALARM,                    // 0 Аварийный датчик, его срабатываение приводит к аварии и останове Тн
	pSENSOR,                   // 1 Обычный датчик, его значение используется в алгоритмах ТН, его изменение не приводит к ошибке.
	pPULSE,                    // 2 Импульсный висит на прерывании и считает частоты - выходная величина ЧАСТОТА не используется
	pEND13                     // Обязательно должен быть последним, добавляем ПЕРЕД!!!
};

struct CORRECT_POWER220_STRUCT {
	uint8_t  num;	// номер реле
	int16_t  value; // Вт
};

enum {
	STATS_OBJ_Temp			= 0,	// °C
	STATS_OBJ_PressTemp,			// °C
	STATS_OBJ_Overheat, 	//= 2,
	STATS_OBJ_Overheat2,
	STATS_OBJ_Overcool,		//= 4,
	STATS_OBJ_TCOMP_TCON,	//= 5,
	STATS_OBJ_Delta_GEO,
	STATS_OBJ_Delta_OUT,
	STATS_OBJ_Press,		//= 8,	// bar
	STATS_OBJ_Flow,			//= 9,	// м³ч
	STATS_OBJ_EEV,			//= 10,	// ЭРВ
	STATS_OBJ_Voltage,		//= 11,	// V
	STATS_OBJ_Power,		//= 12,	// кВт*ч, пока только TYPE_SUM для OBJ_*
	STATS_OBJ_Power_FC,
	STATS_OBJ_Power_GEO,
	STATS_OBJ_Power_OUT,
	STATS_OBJ_Current_FC,	//= 16,
	STATS_OBJ_Compressor,	//= 17,
	STATS_OBJ_COP_Full,		//= 18,
	STATS_OBJ_Sun,			//= 19,
	STATS_OBJ_Relay			//= 20
};

const char *STATS_OBJ_names[] = {
	"Temp",
	"PressTemp",
	"Overheat",
	"Overheat2",
	"Overcool",
	"TCOMP_TCON",
	"Delta_GEO",
	"Delta_OUT",
	"Press",
	"Flow",
	"EEV",
	"Voltage",
	"Power",
	"Power_FC",
	"Power_GEO",
	"Power_OUT",
	"Current_FC",
	"Compressor",
	"COP_Full",
	"Sun",
	"Relay",
	NULL
};

enum {
	STATS_EEV_Steps = 0,	// Шаги
	STATS_EEV_Percent,		// %
	STATS_EEV_OverHeat,		// Перегрев
	STATS_EEV_OverCool,		// Переохлаждение
};

struct History_setup {
	uint8_t		object;		// STATS_OBJ_*
	uint8_t		number;		// номер датчика
	const char	*name;		// Заголовок
};

struct Charts_Mod_setup {
	uint8_t		object;		// STATS_OBJ_*
	uint8_t		number;		// номер датчика
} __attribute__((packed));

struct Charts_Const_setup {
	uint8_t		object;		// STATS_OBJ_*
	const char	*name;
} __attribute__((packed));

typedef int16_t CHART_DATA_TYPE;

// ---------------------------------------------------------------------------------
//  Класс Графики работы   ------------------------------------------------------------
// ---------------------------------------------------------------------------------
// Бинарные данные могут хранится в упакованном виде до 16 значений в одной точке.
// запихиваем уже упакованное значение
// читаем get_boolPointsStr
class statChart                                         // организован кольцевой буфер
{
public:
	void init();                             			 // инициализация класса
	void clear();                                         // очистить статистику
	void add_Point(CHART_DATA_TYPE y);                    // добавить точку в массиве (для бинарных данных добавляем все точки сразу)
	inline CHART_DATA_TYPE get_Point(uint16_t x);         // получить точку нумерация 0-самая старая CHART_POINT - самая новая, (работает кольцевой буфер)
	boolean get_boolPoint(uint16_t x,uint16_t mask);      // БИНАРНЫЕ данные: получить точку нумерация 0-самая старая CHART_POINT - самая новая, (работает кольцевой буфер)

	void get_PointsStrDiv100(char *&b);             		// получить строку в которой перечислены все точки в строковом виде через ";" при этом значения делятся на 100
	void get_PointsStr(char *&b);							// получить строку в которой перечислены все точки в строковом виде через ";"
	void get_PointsStrSubDiv100(char *&b, statChart *sChart); // получить строку, вычесть точки sChart
	void get_PointsStrPower(char *&b, statChart *inChart, statChart *outChart, uint16_t Capacity); // Расчитать мощность на лету используется для графика потока, передаются указатели на температуры

	inline uint16_t get_num()  {return num;}              // Получить число накопленных точек

private:
	CHART_DATA_TYPE *data;                                // указатель на массив для накопления данных
	int16_t pos;                                          // текущая позиция для записи
	int16_t num;                                          // число накопленных точек
	boolean flagFULL;                                     // false в буфере менее CHART_POINT точек
};

void Charts_get_param(uint8_t index, char param, char *ret);
void Charts_set_param(uint8_t index, char param, char *value);

#endif
