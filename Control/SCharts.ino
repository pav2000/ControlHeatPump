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
void statChart::init(boolean pres)
{
  err=OK;
  present=pres;                                 // наличие статистики - зависит от конфигурации
  pos=0;                                        // текущая позиция для записи
  num=0;                                        // число накопленных точек
  flagFULL=false;                               // false в буфере менее CHART_POINT точек
  if (pres)                                     // отводим память если используем статистику
  {
    data=(int16_t*)malloc(sizeof(int16_t)*CHART_POINT);
    if (data==NULL) {err=ERR_OUT_OF_MEMORY; set_Error(err,(char*)__FUNCTION__);return;}  // ОШИБКА если память не выделена
    for(int i=0;i<CHART_POINT;i++) data[i]=0;     // обнуление
  }
}

// Очистить статистику
void statChart::clear()
{
  pos=0;                                        // текущая позиция для записи
  num=0;                                        // число накопленных точек
  flagFULL=false;                               // false в буфере менее CHART_POINT точек
  for(int i=0;i<CHART_POINT;i++) data[i]=0;     // обнуление
}

 // добавить точку в массиве
void statChart::add_Point(int16_t y)
{
 if (!present) return;
 data[pos]=y;
 if (pos<CHART_POINT-1) pos++; else { pos=0; flagFULL=true; }
 if (!flagFULL) num++ ;   // буфер пока не полный
}

// получить точку нумерация 0-самая новая CHART_POINT-1 - самая старая, (работает кольцевой буфер)
inline int16_t statChart::get_Point(uint16_t x)
{
 if (!present) return 0;
 if (!flagFULL) return data[x];
 else
 {
    if ((pos+x)<CHART_POINT) return data[pos+x];else return data[pos+x-CHART_POINT];
 }
}

// БИНАРНЫЕ данные по маске: получить точку нумерация 0-самая старая CHART_POINT - самая новая, (работает кольцевой буфер)
boolean statChart::get_boolPoint(uint16_t x,uint16_t mask)
{
 if (!present) return 0;
 if (!flagFULL) return data[x]&mask?true:false;
 else
 {
    if ((pos+x)<CHART_POINT) return data[pos+x]&mask?true:false;
    else                     return data[pos+x-CHART_POINT]&mask?true:false;
 }
}

// получить строку в которой перечислены все точки в строковом виде через; при этом значения делятся на 100
// строка не обнуляется перед записью
void statChart::get_PointsStrDiv100(char *&b)
{
	if((!present) || (num == 0)) {
		//strcat(b, ";");
		return;
	}
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
	if((!present) || (num == 0)) {
		//strcat(b, ";");
		return;
	}
	b += m_strlen(b);
	for(uint16_t i = 0; i < num; i++) {
		b += m_itoa(get_Point(i), b, 10, 0);
		*b++ = ';';
		*b = '\0';
	}
}

void statChart::get_PointsStrSubDiv100(char *&b, statChart *sChart)
{
	if(!present || num == 0 || !sChart->get_present() || sChart->get_num() == 0) {
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
	if(!present || num == 0 || !inChart->get_present() || inChart->get_num() == 0 || !outChart->get_present() || outChart->get_num() == 0) {
		//strcat(b, ";");
		return;
	}
	b += m_strlen(b);
	for(uint16_t i = 0; i < num; i++) {
	//	b = dptoa(b, (int32_t)(outChart->get_Point(i)-inChart->get_Point(i)) * get_Point(i) * Capacity / (360 *100), 3);// выходная мощность в кВт
		b = dptoa(b, (int32_t)(outChart->get_Point(i)-inChart->get_Point(i)) * get_Point(i) * 10 / kfCapacity, 3);// выходная мощность в кВт, используем get_kfCapacity() 10 - поток в десятках дитров в час
		*(b-1) = ';';
		//*b = '\0';
	}
}

void Charts_get_param(uint8_t index, char param, char *ret)
{
	if(param == 'O') strcat(ret, STATS_OBJ_names[ChartsSetup[index].object]);	// .object
	else if(param == 'N') strcat(ret, ChartsSetup[index].name);					// .name
	else if(param == 'X') {														// .number
		uint8_t num = ChartsSetup[index].number;
		if(ChartsSetup[index].object == STATS_OBJ_Temp) strcat(ret, HP.sTemp[num].get_name());
		else if(ChartsSetup[index].object == STATS_OBJ_Press || ChartsSetup[index].object == STATS_OBJ_PressTemp) strcat(ret, HP.sADC[num].get_name());
		else if(ChartsSetup[index].object == STATS_OBJ_Flow) strcat(ret, HP.sFrequency[num].get_name());
	}
}

void Charts_set_param(uint8_t index, char param, char *value)
{
	uint8_t i = 0;
	if(param == 'O') { // .object
		for(; STATS_OBJ_names[i]; i++) if(strcmp(value, STATS_OBJ_names[i]) == 0) break;
		if(STATS_OBJ_names[i] && ChartsSetup[index].object != i) {
			ChartsSetup[index].number = 0;
			ChartsSetup[index].object = i;
			if(ChartsSetup[index].object == STATS_OBJ_Temp) ChartsSetup[index].name = noteTemp[0];
		}
	} else if(param == 'X') { // .number
		if(ChartsSetup[index].object == STATS_OBJ_Temp) {
			for(i = 0; i < TNUMBER; i++) if(strcmp(value, HP.sTemp[i].get_name()) == 0) break;
			if(i < TNUMBER) {
				ChartsSetup[index].number = i;
				ChartsSetup[index].name = noteTemp[i];
			}
		} else if(ChartsSetup[index].object == STATS_OBJ_Press || ChartsSetup[index].object == STATS_OBJ_PressTemp) {
			for(i = 0; i < ANUMBER; i++) if(strcmp(value, HP.sADC[i].get_name()) == 0) break;
			if(i < ANUMBER) {
				ChartsSetup[index].number = i;
				ChartsSetup[index].name = notePress[i];
			}
		} else if(ChartsSetup[index].object == STATS_OBJ_Flow) {
			for(i = 0; i < FNUMBER; i++) if(strcmp(value, HP.sFrequency[i].get_name()) == 0) break;
			if(i < FNUMBER) {
				ChartsSetup[index].number = i;
				ChartsSetup[index].name = noteFrequency[i];
			}
		}
	}
}

