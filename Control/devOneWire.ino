 /*
 * Copyright (c) 2016-2018 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
 * vad711, vad7@yahoo.com
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
#include "devOneWire.h"     

const char OW_Error_memory_low[] = { "Scan: Memory low!\n" };

int8_t OW_prepare_buffers(void)
{
	OW_scanTableIdx = 0;
	if(OW_scanTable == NULL) {
		OW_scanTable = (type_scanOneWire *)malloc(sizeof(type_scanOneWire) * (TNUMBER+1));
		if(OW_scanTable == NULL) {
			journal.jprintf(OW_Error_memory_low);
			return 1;
		}
	}
	return 0;
}

// Возврат Temp * 100, Если ошибка возвращает ERROR_TEMPERATURE
int16_t deviceOneWire::CalcTemp(uint8_t addr_0, uint8_t *data)
{
	// Разбор данных
	int16_t raw = (data[1] << 8) | data[0];
	if(addr_0 == tDS18S20) {
		raw = raw << 3; // 9 bit resolution default
		if (data[7] == 0x10) { raw = (raw & 0xFFF0) + 12 - data[6]; }  // "count remain" gives full 12 bit resolution
	} else {
		byte cfg = (data[4] & 0x60);
		// at lower res, the low bits are undefined, so let's zero them
		if (cfg == 0x00) raw = raw & ~7;      // 9 bit resolution, 93.75 ms
		else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
		else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
		// default is 12 bit resolution, 750 ms conversion time
	}
	// Проверка валидности данных анализируем полученное разрешение оно должно быть 0x7f (12 бит) при ошибке обычно ff
	if (data[4] != 0x7f && addr_0 == tDS18S20)   // Дополнительнеая проверка для DS18B20: Прочитано НЕ правильное разрешение
	{ // Прочитаны "плохие данные"
		return ERROR_TEMPERATURE;
	} else {
		return raw * 100 / 16;
	}
}

int8_t	deviceOneWire::Init(void)
{
	err = 0;
	if(lock_bus_reset(true) == OK) {
#ifdef ONEWIRE_DS2482
		if(!OneWireDrv.reset_bridge()) err = ERR_DS2482_ONEWIRE;;
#if DS2482_CONFIG != 0
		if(!err && !OneWireDrv.configure(DS2482_CONFIG)) err = ERR_DS2482_ONEWIRE;
#endif
		release_bus();
#endif
	}
	return err;
}

inline uint8_t deviceOneWire::check_presence(void)
{
	return OneWireDrv.check_presence();
}

// Возвращает OK или err
int8_t  deviceOneWire::lock_bus_reset(uint8_t checkpresence)
{
	uint8_t presence;
	err = OK;
#ifdef ONEWIRE_DS2482
	if(SemaphoreTake(xI2CSemaphore,(I2C_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE) {
		journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexI2CBuzy);
		err = ERR_I2C_BUZY;
		return err;
	}
	if(checkpresence) {
		if(check_presence() == 0){    // Проверяем наличие на i2с шине  ds2482
			err = ERR_DS2482_NOT_FOUND;
			release_bus();
			journal.jprintf("DS2482-%d not found . . .\n", bus + 1);
			return err;
		}
	}
#endif
	for(uint8_t i = 0; i < RES_ONEWIRE_ERR; i++)   // Три попытки сбросить датчики, если не проходит то это ошибка
	{
		if((presence = OneWireDrv.reset())) break;                     // Сброс прошел выходим
#ifdef ONEWIRE_DS2482
		if(!OneWireDrv.reset_bridge()) break;
		#if DS2482_CONFIG != 0
		if(!OneWireDrv.configure(DS2482_CONFIG)) break;
		#endif
#else
        pinMode(PIN_ONE_WIRE_BUS, INPUT_PULLUP);
        _delay(1);
        digitalReadDirect(PIN_ONE_WIRE_BUS);
#endif
		_delay(100);                               // Сброс не прошел, сделаем паузу
	}
	if(!presence){
		err = ERR_ONEWIRE;
		release_bus();
	}
	return err;
}

inline void	deviceOneWire::release_bus()
{
#ifdef ONEWIRE_DS2482
		SemaphoreGive(xI2CSemaphore);
#endif
}

// сканирование шины, с записью в буфер результатов возвращает код ошибки или число найденых датчиков
// строка с найдеными датчика кладется в buf возвращает ошибку или OK
int8_t  deviceOneWire::Scan(char *result_str)
{
#ifdef DEMO // В демо режиме выводим строку констант
	// Строка которая выдается в демо режиме при сканировании onewire
	strcat(result_str,"1:DS18B20:20.1:1111111111111111:1;2:DS18S20:-3.56:2222222222222222:1;3:DS1822:34.6:3333333333333333:2;");
	OW_scanTable[0].num=1;OW_scanTable[0].num=2;OW_scanTable[0].num=3;
	OW_scanTable[0].type_sensor=tDS18B20;OW_scanTable[0].type_sensor=tDS18S20;OW_scanTable[0].type_sensor=tDS1822;
	for (int i=0;i<8;i++)
	{
		OW_scanTable[0].address[i]=(i+1)+(i+1)*16;
		OW_scanTable[1].address[i]=(i+1)+(i+1)*16;
		OW_scanTable[2].address[i]=(i+1)+(i+1)*16;
	}
	OW_scanTable[1].address[0]=0x0;
	OW_scanTable[2].address[0]=0xaa;
#else // сканирование датчика
	byte i;
	byte data[12];
	byte addr[8];

	if(lock_bus_reset(true)) {
		if(err == ERR_ONEWIRE) journal.jprintf("OneWire bus %d is empty. . .\n", bus + 1);
		return err;
	}
	eepromI2C.use_RTOS_delay = 0;
	OneWireDrv.reset_search();
	while(OneWireDrv.search(addr)) // до тех пор пока есть свободные адреса
	{
		watchdogReset();
		err = OK;
		//  Датчик найден!
		// 1. Номер по порядку
		OW_scanTable[OW_scanTableIdx].num = OW_scanTableIdx + 1;
		strcat(result_str, int2str(OW_scanTableIdx + 1)); strcat(result_str,":");
		if(OneWireDrv.crc8(addr, 7) != addr[7]) {
			strcat(result_str,"Error CRC:::;");
			continue;
		}
		// 2. первый байт определяет чип выводим этот тип
		OW_scanTable[OW_scanTableIdx].type_sensor = addr[0];
		switch (addr[0])
		{
			case tDS18S20: strcat(result_str,"DS18S20:"); break;
			case tDS18B20: strcat(result_str,"DS18B20:"); break;
			case tDS1822:  strcat(result_str,"DS1822:");  break;
			default:       strcat(result_str,"Unknown"); strcat(result_str, byteToHex(addr[0])); strcat(result_str, ":"); break;
		}
		// 3. Уменьшить разрешение до 9 бит, для увеличения скорости сканирования для DS18B20
		if(SetResolution(addr, DS18B20_p09BIT, true)) {
			journal.jprintf("SetRes 9b error %d for %s\n", err, addressToHex(addr));
		}
		// 4. Старт преобразования температуры и пауза
		if(OneWireDrv.reset()) {
			OneWireDrv.select(addr);
#ifdef ONEWIRE_DS2482_SECOND_2WAY
			if(bus) OneWireDrv.configure(DS2482_CONFIG | DS2482_CONFIG_SPU);
#endif
			OneWireDrv.write(0x44); // начинаем преобразование, используя OneWireDrv.write(0x44,1) с "паразитным" питанием
		} else err = ERR_ONEWIRE;
		if(err == OK) {
			_delay(95);             // Ожитать время разрешение 9 бит это гуд

			// 5. Получение данных
			if(!OneWireDrv.reset()) err = ERR_ONEWIRE;
		}
		if(err == OK) {
			OneWireDrv.select(addr);
			OneWireDrv.write(0xBE);
			for(i=0; i<9; i++) data[i] = OneWireDrv.read(); // Читаем данные, нам необходимо 9 байт

			// 6. Увеличить разрешение до 12 бит, рабочий режим откат обратно
			SetResolution(addr, DS18B20_p12BIT, true);

			// конвертируем данные в фактическую температуру
			int16_t t = CalcTemp(addr[0], data);
			if(OneWireDrv.crc8(data,8) != data[8] || t == ERROR_TEMPERATURE)  				// Дополнительнеая проверка для DS18B20
				strcat(result_str, "CRC");  	// только правильные значения, проверка по разрешению должно быть 1F
			else strcat(result_str, ftoa((char *)data, (float)t / 100.0, 2)); // для DS18S20 просто выводим температуру
		}
		strcat(result_str, ":");

		// 8. Адрес добавить
		memcpy(OW_scanTable[OW_scanTableIdx].address, addr, 8);
		strcat(result_str, addressToHex(addr));
		strcat(result_str, ":");
#ifdef ONEWIRE_DS2482
		OW_scanTable[OW_scanTableIdx].bus_type = bus;
		strcat(result_str, bus ? "2" : "1");
#else
		OW_scanTable[OW_scanTableIdx].bus_type = 0;
		strcat(result_str, "1");
#endif
		strcat(result_str, ";");
		if(++OW_scanTableIdx >= TNUMBER) break;   // Следующий датчик
	} // while по датчикам
	release_bus();
	eepromI2C.use_RTOS_delay = 1;
#endif  // DEMO
	return OK;
}

// чтение данных DS18B20, возвращает код ошибки, делает все преобразования,
// выполняется после подготовки температуры - PrepareTemp()
// прочитанная температура кладется в val = t * 100
int8_t  deviceOneWire::Read(byte *addr, int16_t &val)
{
	int8_t i;
	byte data[9];

	if(lock_bus_reset(0)) return ERR_ONEWIRE;
	OneWireDrv.select(addr);
	OneWireDrv.write(0xBE); // Команда на чтение регистра температуры
	for(i = 0; i < 9; i++) { data[i] = OneWireDrv.read();}
	release_bus();

	// Данные получены
	if(OneWireDrv.crc8(data,8) != data[8]) return ERR_ONEWIRE_CRC;  // Проверка контрольной суммы
	val = CalcTemp(addr[0], data);
	if(val == ERROR_TEMPERATURE) return ERR_ONEWIRE_CRC; // Прочитаны "плохие данные"
	return OK;
}

// установить разрешение, блокировка шины, возврат OK или err
int8_t deviceOneWire::SetResolution(uint8_t *addr, uint8_t rs, uint8_t dont_lock_bus)
{
	err = 0;
	if(!dont_lock_bus && lock_bus_reset(0)) return err = ERR_ONEWIRE;
	else if(!OneWireDrv.reset()) return err = ERR_ONEWIRE;
    OneWireDrv.select(addr);
    OneWireDrv.write(0x4E);  // WRITE SCRATCHPAD
    OneWireDrv.write(0x00);  // верх и низ для аварийных температур не используется
    OneWireDrv.write(0x00);
    OneWireDrv.write(rs);    // это разрядность конвертации 0x7f - 12бит 0x5f- 11бит 0x3f-10бит 0x1f-9 бит
#ifndef ONEWIRE_DONT_CHG_RES
    if(rs == DS18B20_p12BIT) {
        if(!OneWireDrv.reset()){
        	err = ERR_ONEWIRE;
        	if(!dont_lock_bus) release_bus();
        	return err;
        }
        OneWireDrv.select(addr);
#ifdef ONEWIRE_DS2482_SECOND_2WAY
		if(bus) OneWireDrv.configure(DS2482_CONFIG | DS2482_CONFIG_SPU);
#endif
    	OneWireDrv.write(0x48);  // Записать в чип разрешение на всякий случай
    	_delay(12);
    	OneWireDrv.reset();
    }
#endif
    if(!dont_lock_bus) release_bus();
    return err;
}

// запуск преобразования всех датчиков на шине возвращает код ошибки
int8_t  deviceOneWire::PrepareTemp()
{
	if(lock_bus_reset(0)) return ERR_ONEWIRE;
	// начать преобразование температурных ВСЕХ датчиков две команды
	OneWireDrv.skip();    		// skip ROM command
#ifdef ONEWIRE_DS2482_SECOND_2WAY
	if(bus) OneWireDrv.configure(DS2482_CONFIG | DS2482_CONFIG_SPU);
#endif
	OneWireDrv.write(0x44);	   	// convert temp
	release_bus();
	return OK;
}
