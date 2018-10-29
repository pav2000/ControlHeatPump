 /*
 * Copyright (c) 2016-2018 by vad711 (vad7@yahoo.com); by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
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
#ifndef devOneWire_h
#define devOneWire_h
 
#include "Constant.h"                     // Должен быть первым !!!! кое что берется в стандартных либах для настройки либ, Config.h входит в него
#include <DS2482.h>                       // мост OneWire аппаратная реализация доработана
#include <OneWire.h>                      // OneWire библиотека OneWire шина програмная

#define DS2482_CONFIG	(DS2482_CONFIG_APU)	// 0 - не конфигурить

// Типы датчиков температуры
#define tDS18S20 0x10
#define tDS18B20 0x28
#define tDS1822  0x22
#define tRadio   0x03
// разрешение датчика температур
#define DS18B20_p12BIT 0x7F
#define DS18B20_p11BIT 0x5F
#define DS18B20_p10BIT 0x3F
#define DS18B20_p09BIT 0x1F

#define ERROR_TEMPERATURE	-32768

// Структура для хранения информации при сканировании температурных датчиков на шине OneWire
struct type_scanOneWire
{
  byte num;            // номер по списку
  byte bus;       	   // номер шины: 0..3
  byte address[8];     // адрес
};

#define OW_scanTable_max (TNUMBER)
type_scanOneWire	*OW_scanTable = NULL;				// массив структур для хранения информации о дачиках при сканировании шины onewire
uint8_t				 OW_scanTableIdx;
uint8_t				 OW_scan_flags = 0;					// 1 - идет сканирование
int8_t 				 OW_prepare_buffers(void);

class deviceOneWire 								    // Класс шина   OneWire
{
  public:
#ifdef ONEWIRE_DS2482
	deviceOneWire(uint8_t addr, uint8_t _bus): OneWireDrv(addr) { err = 0; bus = _bus; }
#else
	deviceOneWire(uint8_t pin_num): OneWireDrv(pin_num) { err = 0; bus = 0; }
#endif
	int8_t	Init(void);
    int16_t	CalcTemp(uint8_t addr_0, uint8_t *data, uint8_t only_temp_readed);	// Вычислить температуру в сотых градуса по считанному scratchpad
	int8_t	Scan(char *result_str);                    // сканирование шины, с записью в буфер результатов возвращает число найденых датчиков
	int8_t  PrepareTemp();                              // запуск преобразования всех датчиков на шине возвращает код ошибки
	int8_t  Read(uint8_t *addr, int16_t &val);          // чтение данных DS18B20, возвращает код ошибки, делает все преобразования
	int8_t  SetResolution(uint8_t *addr, uint8_t rs, uint8_t dont_lock_bus = 0);	// установить разрешение конкретного датчика
	int8_t  GetLastErr() { return err; }       			// Получить последнюю ошибку
	int8_t  lock_I2C_bus_reset(uint8_t checkpresence);	// блокирование семафора, проверка наличия ds2482, reset 1-wire.
	void	release_I2C_bus();
  private:
	int8_t err;                                         // ошибка шины (работа) при ошибке останов ТН
	uint8_t	bus;										// 0 - первый DS2482, 1 - второй, 2 - третий, 3 - четвертый
#ifdef ONEWIRE_DS2482
	DS2482	OneWireDrv;                                 // мастер OneWire аппаратная
};
deviceOneWire OneWireBus(I2C_ADR_DS2482, 0);            // Создание шины нужного типа
#ifdef ONEWIRE_DS2482_SECOND
deviceOneWire OneWireBus2(I2C_ADR_DS2482_2, 1);        // Создание шины нужного типа
#endif
#ifdef ONEWIRE_DS2482_THIRD
deviceOneWire OneWireBus3(I2C_ADR_DS2482_3, 2);        // Создание шины нужного типа
#endif
#ifdef ONEWIRE_DS2482_FOURTH
deviceOneWire OneWireBus4(I2C_ADR_DS2482_4, 3);        // Создание шины нужного типа
#endif
#else // ONEWIRE_DS2482
	OneWire	OneWireDrv;                                 // OneWire шина програмная
};
deviceOneWire OneWireBus(PIN_ONE_WIRE_BUS);             // Создание шины
#endif // ONEWIRE_DS2482

#endif
