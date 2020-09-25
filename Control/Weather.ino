/*
 * Copyright (c) by Vadim Kulakov vad7@yahoo.com, vad711
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

#include "Constant.h"

#ifdef WEATHER_FORECAST

// <"key">:<value>,
const char WF_JSON_Sunrise[] = "\"sunrise\"";
const char WF_JSON_Hourly[] = "\"hourly\"";
const char WF_JSON_DT[] = "\"dt\"";
const char WF_JSON_Clouds[] = "\"clouds\"";

// Возврат 0 - ОК
int WF_ProcessForecast(char *json)
{
	char *fld = strstr(json, WF_JSON_Sunrise);
	if(!fld) {
		WF_BoilerTargetPercent = 100;
		return -2;
	}
	uint32_t t = strtoul(fld += sizeof(WF_JSON_Sunrise), NULL, 0);
	if(t == ULONG_MAX || t == 0) {
		WF_BoilerTargetPercent = 100;
		return -3;
	}
	t += WF_ForecastAfterSunrise;
	fld = strstr(fld, WF_JSON_Hourly);
	if(!fld) {
		WF_BoilerTargetPercent = 100;
		return -4;
	}
	fld += sizeof(WF_JSON_Hourly);
	int32_t avg = 0;
	uint8_t i = 1;
	for(; i <= WF_ForecastAggregateHours; i++) {
		fld = strstr(fld, WF_JSON_DT);
		if(!fld) break;
		uint32_t n = strtoul(fld += sizeof(WF_JSON_DT), NULL, 0);
		if(n == ULONG_MAX) break;
		if(n < t) {
			i--;
			continue;
		}
		fld = strstr(fld, WF_JSON_Clouds);
		if(!fld) break;
		n = strtoul(fld += sizeof(WF_JSON_Clouds), NULL, 0);
		if(n == ULONG_MAX) break;
		avg += n;
	}
	if(--i) {
		avg /= i;
		if(GETBIT(WR.Flags, WR_fLog)) journal.jprintf("WF: Clouds(%d)=%d", i, avg);
		if(avg < 100) { // 66..99 -> 0..98
			avg = (avg - 66) * 3;
			if(avg < 0) avg = 0;
		}
		avg += WF_SunByMonth[rtcSAM3X8.get_months()-1];
		if(avg > 100) avg = 100;
		if(GETBIT(WR.Flags, WR_fLog)) journal.jprintf(":%d\n", avg);
		WF_BoilerTargetPercent = avg;
		return OK;
	}
	WF_BoilerTargetPercent = 100;
	return -1;
}

#endif
