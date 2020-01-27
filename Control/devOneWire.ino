 /*
 * Copyright (c) 2016-2019 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
 * &                       by Vadim Kulakov vad7@yahoo.com, vad711
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

// Return 1 - success
int8_t OW_prepare_buffers(void)
{
	OW_scanTableIdx = 0;
	if(OW_scanTable == NULL) {
		OW_scanTable = (type_scanOneWire *)malloc(sizeof(type_scanOneWire) * OW_scanTable_max);
		if(OW_scanTable == NULL) {
			journal.jprintf(OW_Error_memory_low);
			return 0;
		}
	}
	return 1;
}

// Возврат Temp * 100, Если ошибка возвращает ERROR_TEMPERATURE
int16_t deviceOneWire::CalcTemp(uint8_t addr_0, uint8_t *data, uint8_t only_temp_readed)
{
	// Разбор данных
	int16_t raw = (data[1] << 8) | data[0];
	if(addr_0 == tDS18S20) {
		raw = raw << 3; // 9 bit resolution default
		if(only_temp_readed) goto xReturnTemp;
		if(data[7] == 0x10) { raw = (raw & 0xFFF0) + 12 - data[6]; }  // "count remain" gives full 12 bit resolution
	} else {
		if(only_temp_readed) goto xReturnTemp;
		byte cfg = data[4];
		// at lower res, the low bits are undefined, so let's zero them
		if(cfg == 0x00 + 0x1F) raw = raw & ~7;      // 9 bit resolution, 93.75 ms
		else if(cfg == 0x20 + 0x1F) raw = raw & ~3; // 10 bit res, 187.5 ms
		else if(cfg == 0x40 + 0x1F) raw = raw & ~1; // 11 bit res, 375 ms
		else if(cfg != 0x60 + 0x1F) return ERROR_TEMPERATURE; // Прочитаны "плохие данные"
		// default is 12 bit resolution, 750 ms conversion time
	}
xReturnTemp:
	return raw * 100 / 16;
}

int8_t	deviceOneWire::Init(void)
{
	err = 0;
	if(lock_I2C_bus_reset(1) == OK) {
#ifdef ONEWIRE_DS2482
		if(!OneWireDrv.reset_bridge()) err = ERR_DS2482_ONEWIRE;
#if DS2482_CONFIG != 0
		if(!err && !OneWireDrv.configure(DS2482_CONFIG)) err = ERR_DS2482_ONEWIRE;
#endif
		release_I2C_bus();
#endif
	}
	return err;
}

// Возвращает OK или err. Если checkpresence=1, то только проверка на присутствие ds2482; семафор взведен, если нет ошибки
int8_t  deviceOneWire::lock_I2C_bus_reset(uint8_t checkpresence)
{
	uint8_t presence = 0;
	err = OK;
#ifdef ONEWIRE_DS2482
	if(SemaphoreTake(xI2CSemaphore,(I2C_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE) {
		journal.printf((char*)cErrorMutex,__FUNCTION__,MutexI2CBuzy);
		err = ERR_I2C_BUZY;
		return err;
	}
	if(checkpresence) {
		if(!OneWireDrv.check_presence()){    // Проверяем наличие на i2с шине  ds2482
			err = ERR_DS2482_NOT_FOUND;
			release_I2C_bus();
		}
		return err;
	}
#else
	if(checkpresence) return OK;
#endif
	for(uint8_t i = 0; i < RES_ONEWIRE_ERR; i++)   // Три попытки сбросить датчики, если не проходит то это ошибка
	{
#ifdef ONEWIRE_DS2482_2WAY
		if((ONEWIRE_2WAY & (1<<bus)) && !OneWireDrv.configure(DS2482_CONFIG)) goto x_Reset_bridge;
#endif
		if((presence = OneWireDrv.reset())) break;                     // Сброс прошел выходим
#ifdef ONEWIRE_DS2482
	#ifdef ONEWIRE_DS2482_2WAY
x_Reset_bridge:
	#endif
		if(!OneWireDrv.reset_bridge()) {
			Recover_I2C_bus();
			if(!OneWireDrv.reset_bridge()) {
				err = ERR_DS2482_NOT_FOUND;
				break;
			}
		}
	#if DS2482_CONFIG != 0
		if(!OneWireDrv.configure(DS2482_CONFIG)) break;
	#endif
#else
        pinMode(PIN_ONE_WIRE_BUS, INPUT_PULLUP);
        _delay(1);
        digitalReadDirect(PIN_ONE_WIRE_BUS);
#endif
		_delay(50);                               // Сброс не прошел, сделаем паузу
	}
	if(!presence){
		if(err == OK) err = ERR_ONEWIRE;
		release_I2C_bus();
	}
	return err;
}

inline void	deviceOneWire::release_I2C_bus()
{
#ifdef ONEWIRE_DS2482
		SemaphoreGive(xI2CSemaphore);
#endif
}

// сканирование шины, с записью в буфер результатов возвращает код ошибки или число найденых датчиков
// строка с найдеными датчика кладется в buf возвращает ошибку или OK
// Формат: <Номер п.п.>:<тип датчика>:<температура>:<серийный номер>:<шина>;
int8_t  deviceOneWire::Scan(char *result_str)
{
#ifdef DEMO // В демо режиме выводим строку констант
	// Строка которая выдается в демо режиме при сканировании onewire
	strcat(result_str,"1:DS18B20:20.1:1111111111111111:1;2:DS18S20:-3.56:2222222222222222:1;3:DS1822:34.6:3333333333333333:2;");
	OW_scanTable[0].num=1;OW_scanTable[1].num=2;OW_scanTable[2].num=3;
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
	WDT_Restart(WDT);

	if(lock_I2C_bus_reset(0)) { // reset 1-wire
		if(err == ERR_ONEWIRE) journal.jprintf("1-Wire bus %d is empty. . .\n", bus + 1);
		return err;
	}
	//eepromI2C.use_RTOS_delay = 0;
	OneWireDrv.skip(); // Все датчики
#ifdef ONEWIRE_DS2482_2WAY
	if((ONEWIRE_2WAY & (1<<bus))) OneWireDrv.configure(DS2482_CONFIG | DS2482_CONFIG_SPU);
#endif
	OneWireDrv.write(0x44); // начинаем преобразование, используя OneWireDrv.write(0x44,1) с "паразитным" питанием
	_delay(cDELAY_DS1820);  // ждем подготовку температуры
	if(
#ifdef ONEWIRE_DS2482_2WAY
		!((ONEWIRE_2WAY & (1<<bus)) && !OneWireDrv.configure(DS2482_CONFIG)) &&
#endif
		OneWireDrv.reset())
	{
		OneWireDrv.reset_search();
		while(OneWireDrv.search(addr)) // до тех пор пока есть свободные адреса
		{
			WDT_Restart(WDT);
			err = OK;
			//  Датчик найден!
			// 1. Номер по порядку
			OW_scanTable[OW_scanTableIdx].num = OW_scanTableIdx + 1;
			_itoa(OW_scanTableIdx + 1, result_str); strcat(result_str,":");
			if(OneWireDrv.crc8(addr, 7) != addr[7]) {
				strcat(result_str,"Error CRC:::;");
				continue;
			}
			// 2. первый байт определяет чип выводим этот тип
			strcat(result_str,"DS18");
			switch (addr[0])
			{
				case tDS18S20: strcat(result_str,"S20"); break;
				case tDS18B20: strcat(result_str,"B20"); break;
				case tDS1822:  strcat(result_str,"22");  break;
				default:       strcat(result_str,"?"); _itoa(addr[0], result_str); break;
			}
			strcat(result_str, ":");
			// 5. Получение данных
			if(!OneWireDrv.reset()) err = ERR_ONEWIRE;
			if(err == OK) {
				if(GETBIT(HP.get_flags(), f1Wire1TSngl + bus)) OneWireDrv.skip(); else OneWireDrv.select(addr);
				OneWireDrv.write(0xBE);
				for(i=0; i<9; i++) data[i] = OneWireDrv.read(); // Читаем данные, нам необходимо 9 байт
				// конвертируем данные в фактическую температуру
				int16_t t = CalcTemp(addr[0], data, 0);
				if(OneWireDrv.crc8(data,8) != data[8] || t == ERROR_TEMPERATURE)  // Дополнительная проверка для DS18B20
					strcat(result_str, "CRC");
				else _dtoa(result_str, t, 2);
				strcat(result_str, "(");
				_itoa(((data[4] & 0x60) >> 5) + 9, result_str);
				strcat(result_str, "b)");
				if(data[4] != DS18B20_p12BIT) if(SetResolution(addr, DS18B20_p12BIT, 1)) strcat(result_str, "!");
			}
			strcat(result_str, ":");
			// 8. Адрес добавить
			memcpy(OW_scanTable[OW_scanTableIdx].address, addr, 8);
			strcat(result_str, addressToHex(addr));
			strcat(result_str, ":");
#ifdef ONEWIRE_DS2482
			OW_scanTable[OW_scanTableIdx].bus = bus;
			_itoa(bus+1, result_str);
#else
			OW_scanTable[OW_scanTableIdx].bus = 0;
			strcat(result_str, "1");
#endif
			strcat(result_str, ";");
			release_I2C_bus();
			journal.jprintf("%s Pad:%s\n", result_str, addressToHex(data));
#ifdef ONEWIRE_DS2482
			if(SemaphoreTake(xI2CSemaphore, (I2C_TIME_WAIT/portTICK_PERIOD_MS)) == pdFALSE) {
				journal.printf((char*)cErrorMutex,__FUNCTION__,MutexI2CBuzy);
				err = ERR_I2C_BUZY;
				break;
			}
#endif
			result_str += strlen(result_str);
			if(++OW_scanTableIdx >= OW_scanTable_max) break;   // Следующий датчик
		} // while по датчикам
	} else {
		journal.jprintf("Error reset 1-Wire bus %d\n", bus + 1);
	}
	//eepromI2C.use_RTOS_delay = 1;
	release_I2C_bus();
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

	if((i = lock_I2C_bus_reset(0))) return i;
	if(GETBIT(HP.get_flags(), f1Wire1TSngl + bus)) OneWireDrv.skip();
	else
		OneWireDrv.select(addr);
	OneWireDrv.write(0xBE); // Команда на чтение регистра температуры
	for(i = 0; i < 9; i++) {
		int16_t r = OneWireDrv.read();
		if(r < 0) { // ошибка во время чтения
			release_I2C_bus();
			if(i > 1) goto xReadedOnly2b; // успели прочитать температуру
			return abs(r) | (i > 1 ? 0x40 : 0);
		}
		data[i] = r;
	}
	release_I2C_bus();

	// Данные получены
	i = OK;
	if(OneWireDrv.crc8(data,8) != data[8]) {
xReadedOnly2b:
		i = ERR_ONEWIRE_CRC;  // Проверка контрольной суммы
	}
	val = CalcTemp(addr[0], data, i != OK);
	if(val == ERROR_TEMPERATURE) i = 0x40; // Прочитаны "плохие данные"
	return i;
}

// установить разрешение, блокировка шины, возврат OK или err
int8_t deviceOneWire::SetResolution(uint8_t *addr, uint8_t rs, uint8_t dont_lock_bus)
{
	err = 0;
	if(addr[0] == tDS18S20) return err; // not supported
	if(!dont_lock_bus && lock_I2C_bus_reset(0)) return err = ERR_ONEWIRE;
	else if(!OneWireDrv.reset()) return err = ERR_ONEWIRE;
    OneWireDrv.select(addr);
    OneWireDrv.write(0x4E);  // WRITE SCRATCHPAD
    OneWireDrv.write(0x00);  // верх и низ для аварийных температур не используется
    OneWireDrv.write(0x00);
    OneWireDrv.write(rs);    // это разрядность конвертации 0x7f - 12бит 0x5f- 11бит 0x3f-10бит 0x1f-9 бит
    if(rs == DS18B20_p12BIT) {
        if(!OneWireDrv.reset()){
        	err = ERR_ONEWIRE;
        	if(!dont_lock_bus) release_I2C_bus();
        	return err;
        }
        OneWireDrv.select(addr);
#ifdef ONEWIRE_DS2482_2WAY
		if((ONEWIRE_2WAY & (1<<bus))) OneWireDrv.configure(DS2482_CONFIG | DS2482_CONFIG_SPU);
#endif
    	OneWireDrv.write(0x48);  // Записать в чип разрешение на всякий случай
#ifdef ONEWIRE_DS2482_2WAY
		if((ONEWIRE_2WAY & (1<<bus))) {
	    	_delay(12);
			OneWireDrv.configure(DS2482_CONFIG);
		}
#endif
    }
    if(!dont_lock_bus) release_I2C_bus();
    return err;
}

// запуск преобразования всех датчиков на шине возвращает код ошибки
int8_t  deviceOneWire::PrepareTemp()
{
	uint8_t err = lock_I2C_bus_reset(0);
	if(err) return err;
	// начать преобразование температурных ВСЕХ датчиков две команды
	OneWireDrv.skip();    		// skip ROM command
#ifdef ONEWIRE_DS2482_2WAY
	if((ONEWIRE_2WAY & (1<<bus))) OneWireDrv.configure(DS2482_CONFIG | DS2482_CONFIG_SPU);
#endif
	OneWireDrv.write(0x44);	   	// convert temp
	release_I2C_bus();
	return OK;
}

// Борьба с зависшими устройствами на шине  I2C (в первую очередь часы) неудачный сброс
void Recover_I2C_bus(void)
{
	// https://forum.arduino.cc/index.php?topic=288573.0
	pinMode(PIN_WIRE_SCL, OUTPUT);
	for(uint8_t i = 0; i < 8; i++) {
		digitalWriteDirect(PIN_WIRE_SCL, HIGH); delayMicroseconds(3);
		digitalWriteDirect(PIN_WIRE_SCL, LOW);  delayMicroseconds(3);
	}
	//pinMode(PIN_WIRE_SCL, INPUT);
	PIO_Configure(
			g_APinDescription[PIN_WIRE_SCL].pPort,
			g_APinDescription[PIN_WIRE_SCL].ulPinType,
			g_APinDescription[PIN_WIRE_SCL].ulPin,
			g_APinDescription[PIN_WIRE_SCL].ulPinConfiguration);
	delayMicroseconds(10);
	Wire.begin();
	delayMicroseconds(10);
	Wire.Stop();
}
