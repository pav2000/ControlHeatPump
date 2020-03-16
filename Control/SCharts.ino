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

// ---------------------------------------------------------------------------------
//  Класс ГРАФИКИ    ------------------------------------------------------------
// ---------------------------------------------------------------------------------
// Инициализация
void statChart::init()
{
	pos = 0;                                        // текущая позиция для записи
	num = 0;                                        // число накопленных точек
	flagFULL = false;                               // false в буфере менее CHART_POINT точек
	data = (CHART_DATA_TYPE *) malloc(sizeof(CHART_DATA_TYPE) * CHART_POINT);
	if(data == NULL) { // ОШИБКА если память не выделена
		set_Error(ERR_OUT_OF_MEMORY, (char*) __FUNCTION__);
	} else clear();
}

// Очистить статистику
void statChart::clear()
{
	pos = 0;                                        // текущая позиция для записи
	num = 0;                                        // число накопленных точек
	flagFULL = false;                               // false в буфере менее CHART_POINT точек
	memset(data, 0, sizeof(CHART_DATA_TYPE));
}

// добавить точку в массиве
void statChart::add_Point(CHART_DATA_TYPE y)
{
	data[pos] = y;
	if(pos < CHART_POINT - 1) pos++;
	else {
		pos = 0;
		flagFULL = true;
	}
	if(!flagFULL) num++;   // буфер пока не полный
}

// получить точку нумерация 0-самая новая CHART_POINT-1 - самая старая, (работает кольцевой буфер)
inline CHART_DATA_TYPE statChart::get_Point(uint16_t x)
{
	if(!flagFULL) return data[x];
	else {
		if((pos + x) < CHART_POINT) return data[pos + x];
		else return data[pos + x - CHART_POINT];
	}
}

// БИНАРНЫЕ данные по маске: получить точку нумерация 0-самая старая CHART_POINT - самая новая, (работает кольцевой буфер)
boolean statChart::get_boolPoint(uint16_t x, uint16_t mask)
{
	if(!flagFULL) return data[x] & mask ? true : false;
	else {
		if((pos + x) < CHART_POINT) return data[pos + x] & mask ? true : false;
		else return data[pos + x - CHART_POINT] & mask ? true : false;
	}
}

// получить строку в которой перечислены все точки в строковом виде через; при этом значения делятся на 100
// строка не обнуляется перед записью
void statChart::get_PointsStrDiv100(char *&b)
{
	if(num == 0) return;
	b += m_strlen(b);
	for(uint16_t i = 0; i < num; i++) {
		b = dptoa(b, get_Point(i), 2);
		*b++ = ';';
		*b = '\0';
	}
}

// получить строку в которой перечислены все точки в строковом виде через;
// строка не обнуляется перед записью
void statChart::get_PointsStr(char *&b)
{
	if(num == 0) return;
	b += m_strlen(b);
	for(uint16_t i = 0; i < num; i++) {
		b += m_itoa(get_Point(i), b, 10, 0);
		*b++ = ';';
		*b = '\0';
	}
}

void statChart::get_PointsStrSubDiv100(char *&b, statChart *sChart)
{
	if(num == 0 || sChart->get_num() == 0) {
		//strcat(b, ";");
		return;
	}
	b += m_strlen(b);
	for(uint16_t i = 0; i < num; i++) {
		b = dptoa(b, get_Point(i) - sChart->get_Point(i), 2);
		*b++ = ';';
		*b = '\0';
	}
}
// Расчитать мощность на лету используется для графика потока, передаются указатели на графики температуры + теплоемкость
// График температур - сотые градуса, график потока это ДЕСЯТКИ литров в час
// Теплоемкость kfCapacity =  (3600*100/Capacity)
void statChart::get_PointsStrPower(char *&b, statChart *inChart, statChart *outChart, uint16_t kfCapacity)
{
	if(num == 0 || inChart->get_num() == 0 || outChart->get_num() == 0) {
		//strcat(b, ";");
		return;
	}
	b += m_strlen(b);
	for(uint16_t i = 0; i < num; i++) {
		//	b = dptoa(b, (int32_t)(outChart->get_Point(i)-inChart->get_Point(i)) * get_Point(i) * Capacity / (360 *100), 3);// выходная мощность в кВт
		b = dptoa(b, (int32_t)(outChart->get_Point(i) - inChart->get_Point(i)) * get_Point(i) * 10 / kfCapacity, 3); // выходная мощность в кВт, используем get_kfCapacity() 10 - поток в десятках дитров в час
		*(b - 1) = ';';
		//*b = '\0';
	}
}

void Charts_get_param(uint8_t index, char param, char *ret)
{
	if(param == 'O') strcat(ret, STATS_OBJ_names[ChartsModSetup[index].object]);	// .object
	else if(param == 'N') {
		if(ChartsModSetup[index].object == STATS_OBJ_Temp) strcat(ret, HP.sTemp[ChartsModSetup[index].number].get_note());
		else if(ChartsModSetup[index].object == STATS_OBJ_Press) strcat(ret, HP.sADC[ChartsModSetup[index].number].get_note());
		else if(ChartsModSetup[index].object == STATS_OBJ_PressTemp) {
			strcat(ret, HP.sADC[ChartsModSetup[index].number].get_note());
			strcat(ret, ", °C");
		} else if(ChartsModSetup[index].object == STATS_OBJ_Flow) strcat(ret, HP.sFrequency[ChartsModSetup[index].number].get_note());
	} else if(param == 'X') {														// .number
		uint8_t num = ChartsModSetup[index].number;
		if(ChartsModSetup[index].object == STATS_OBJ_Temp) strcat(ret, HP.sTemp[num].get_name());
		else if(ChartsModSetup[index].object == STATS_OBJ_Press || ChartsModSetup[index].object == STATS_OBJ_PressTemp) strcat(ret, HP.sADC[num].get_name());
		else if(ChartsModSetup[index].object == STATS_OBJ_Flow) strcat(ret, HP.sFrequency[num].get_name());
	}
}

void Charts_set_param(uint8_t index, char param, char *value)
{
	uint8_t i = 0;
	if(param == 'O') { // .object
		for(; STATS_OBJ_names[i]; i++) if(strcmp(value, STATS_OBJ_names[i]) == 0) break;
		if(STATS_OBJ_names[i] && ChartsModSetup[index].object != i) {
			ChartsModSetup[index].number = 0;
			ChartsModSetup[index].object = i;
		}
	} else if(param == 'X') { // .number
		if(ChartsModSetup[index].object == STATS_OBJ_Temp) {
			for(i = 0; i < TNUMBER; i++) if(strcmp(value, HP.sTemp[i].get_name()) == 0) break;
			if(i < TNUMBER) ChartsModSetup[index].number = i;
		} else if(ChartsModSetup[index].object == STATS_OBJ_Press || ChartsModSetup[index].object == STATS_OBJ_PressTemp) {
			for(i = 0; i < ANUMBER; i++) if(strcmp(value, HP.sADC[i].get_name()) == 0) break;
			if(i < ANUMBER) ChartsModSetup[index].number = i;
		} else if(ChartsModSetup[index].object == STATS_OBJ_Flow) {
			for(i = 0; i < FNUMBER; i++) if(strcmp(value, HP.sFrequency[i].get_name()) == 0) break;
			if(i < FNUMBER) ChartsModSetup[index].number = i;
		}
	}
}

