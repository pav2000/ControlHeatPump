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
	counts = 0;
	previous = rtcSAM3X8.unixtime();
	previous_energy = millis();
}

// Обновить статистику, раз в период
void Statistics::Update()
{
	uint32_t tm = rtcSAM3X8.unixtime();
	if(tm - previous >= STAT_PERIOD) {
		previous = tm;
		for(uint8_t i = 0; i < sizeof(Stats_data) / sizeof(Stats_data[0]); i++) {
			int32_t newval;
			switch(Stats_data[i].object) {
			case STATS_OBJ_Temp:
				newval = HP.sTemp[Stats_data[i].number].get_Temp();
				break;
			}
			//UpdateValue(Stats_data[i].value, Stats_data[i].type);


		}
		counts++;
	}
}

// Обновить статистику по энергии (кВт*ч) и COP, вызывается часто
void Statistics::UpdateEnergy()
{
	uint32_t tm = millis() - previous_energy;
	int32_t newval;
	for(uint8_t i = 0; i < sizeof(Stats_data) / sizeof(Stats_data[0]); i++) {
		switch(Stats_data[i].object) {
		case STATS_OBJ_Power:
			if(Stats_data[i].number == OBJ_powerCO) { // Система отопления
				newval = (int32_t)HP.powerCO * tm / 3600; // в мВт
			} else if(Stats_data[i].number == OBJ_powerGEO) { // Геоконтур
				newval = (int32_t)HP.powerGEO * tm / 3600; // в мВт
			} else if(Stats_data[i].number == OBJ_power220) { // Геоконтур
				newval = (int32_t)HP.power220 * tm / 3600; // в мВт
			}
			break;
		case STATS_OBJ_COP:
			if(Stats_data[i].number == OBJ_COP_Compressor) {
				newval = HP.COP;
			} else if(Stats_data[i].number == OBJ_COP_Full) {
				newval = HP.fullCOP;
			}
			break;
		}
	}
}

void Statistics::UpdateValue(int32_t *value, uint8_t type)
{
	switch(type){
	case STATS_TYPE_MIN:

		break;
	}
}

// Записать статистику на SD
void Statistics::Save()
{
	if(HP.get_fSD()) {



	}
}
