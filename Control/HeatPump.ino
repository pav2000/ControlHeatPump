/*
 * Copyright (c) 2016-2019 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
 * &                       by Vadim Kulakov vad7@yahoo.com, vad711
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
// --------------------------------------------------------------------------------
// Описание базового класса для работы Теплового Насоса  ==false
// --------------------------------------------------------------------------------
#include "HeatPump.h"

// Булевые константы для упрощения читаемости кода (используются только в этом файле)
const boolean _stop =  true;   // Команда останов ТН
const boolean _wait =  false;  // Команда перевода в режим ожидания ТН
const boolean _start = true;   // Команда запуска ТН
const boolean _resume = false;  // Команда возобновления работы ТН

#define PUMPS_ON          Pumps(true, DELAY_AFTER_SWITCH_RELAY)               // Включить насосы
#define PUMPS_OFF         Pumps(false, DELAY_AFTER_SWITCH_RELAY)              // Выключить насосы
// Макросы по работе с компрессором в зависимости от наличия инвертора
#define COMPRESSOR_ON     if(dFC.get_present()) dFC.start_FC();else dRelay[RCOMP].set_ON();   // Включить компрессор в зависимости от наличия инвертора
#define COMPRESSOR_OFF    if(dFC.get_present()) dFC.stop_FC(); else dRelay[RCOMP].set_OFF();  // Выключить компрессор в зависимости от наличия инвертора

void HeatPump::initHeatPump()
{
	uint8_t i;
	eraseError();

	for(i = 0; i < TNUMBER; i++) sTemp[i].initTemp(i);            // Инициализация датчиков температуры

#ifdef SENSOR_IP
	for(i=0;i<IPNUMBER;i++) sIP[i].initIP(i);               // Инициализация удаленных датчиков
#endif
	sADC[PEVA].initSensorADC(PEVA, ADC_SENSOR_PEVA, FILTER_SIZE);          // Инициализация аналогово датчика PEVA
	sADC[PCON].initSensorADC(PCON, ADC_SENSOR_PCON, FILTER_SIZE);          // Инициализация аналогово датчика TCON
#ifdef PGEO
	sADC[PGEO].initSensorADC(PGEO, ADC_SENSOR_PGEO, FILTER_SIZE_OTHER);			// Инициализация аналогово датчика PGEO
#endif
#ifdef POUT
	sADC[POUT].initSensorADC(POUT, ADC_SENSOR_POUT, FILTER_SIZE_OTHER);			// Инициализация аналогово датчика POUT
#endif

	for(i = 0; i < INUMBER; i++) sInput[i].initInput(i);           // Инициализация контактных датчиков
	for(i = 0; i < FNUMBER; i++) sFrequency[i].initFrequency(i);  // Инициализация частотных датчиков
	for(i = 0; i < RNUMBER; i++) dRelay[i].initRelay(i);           // Инициализация реле

#ifdef EEV_DEF
	dEEV.initEEV();                                           // Инициализация ЭРВ
#endif

	// Инициалаизация модбаса  перед частотником и счетчиком
	journal.jprintf("Init Modbus RTU via RS485:");
	if(Modbus.initModbus() == OK) journal.jprintf(" OK\r\n");                //  выводим сообщение об установлении связи
	else {
		journal.jprintf(" not present config\r\n");
	}         //  нет в конфигурации

	dFC.initFC();                                              // Инициализация FC
#ifdef USE_ELECTROMETER_SDM
	dSDM.initSDM();                                           // инициалаизация счетчика
#endif
	message.initMessage();                                  // Инициализация Уведомлений
#ifdef MQTT
	clMQTT.initMQTT();                                      // Инициализация MQTT
#endif
	resetSettingHP();                                          // все переменные
}
// Стереть последнюю ошибку
void HeatPump::eraseError()
{
	strcpy(note_error, "OK");          // Строка c описанием ошибки
	strcpy(source_error, "");          // Источник ошибки
	error = OK;                         // Код ошибки
}

// Получить число ошибок чтения ВСЕХ датчиков темпеартуры
uint32_t HeatPump::get_errorReadDS18B20()
{
	uint32_t sum = 0;
	for(uint8_t i = 0; i < TNUMBER; i++) sum += sTemp[i].get_sumErrorRead();     // Суммирование ошибок по всем датчикам
	return sum;
}

void HeatPump::Reset_TempErrors()
{
	for(uint8_t i=0; i<TNUMBER; i++) sTemp[i].Reset_Errors();
}

// Установить состояние ТН, при необходимости пишем состояние в ЕЕПРОМ
// true - есть изменения false - нет изменений
boolean HeatPump::setState(TYPE_STATE_HP st)
{
  if (st==Status.State) return false;     // Состояние не меняется
  switch (st)
  {
  case pOFF_HP:       Status.State=pOFF_HP; if(GETBIT(motoHour.flags,fHP_ON))   {SETBIT0(motoHour.flags,fHP_ON);save_motoHour();}  break;  // 0 ТН выключен, при необходимости записываем в ЕЕПРОМ
  case pSTARTING_HP:  Status.State=pSTARTING_HP; break;                                                                                    // 1 Стартует
  case pSTOPING_HP:   Status.State=pSTOPING_HP;  break;                                                                                    // 2 Останавливается
  case pWORK_HP:      Status.State=pWORK_HP;if(!(GETBIT(motoHour.flags,fHP_ON))) {SETBIT1(motoHour.flags,fHP_ON);save_motoHour();}  break; // 3 Работает, при необходимости записываем в ЕЕПРОМ
  case pWAIT_HP:      Status.State=pWAIT_HP;if(!(GETBIT(motoHour.flags,fHP_ON))) {SETBIT1(motoHour.flags,fHP_ON);save_motoHour();}  break; // 4 Ожидание, при необходимости записываем в ЕЕПРОМ
  case pERROR_HP:     Status.State=pERROR_HP;    break;                                                                                    // 5 Ошибка ТН
  case pERROR_CODE:                                                                                                                        // 6 - Эта ошибка возникать не должна!
  default:            Status.State=pERROR_HP;    break;                                                                                    // Обязательно должен быть последним, добавляем ПЕРЕД!!!
  }   
 return true; 
}

// возвращает строку с найденными датчиками
void HeatPump::scan_OneWire(char *result_str)
{
	if(get_State() == pWORK_HP) {   // ТН работает
		//strcat(result_str, "-:Не доступно - ТН работает!:::;");
		return;
	}
	if(!OW_scan_flags && OW_prepare_buffers()) {
		OW_scan_flags = 1; // Идет сканирование
		char *_result_str = result_str + m_strlen(result_str);
		OneWireBus.Scan(result_str);
#ifdef ONEWIRE_DS2482_SECOND
		OneWireBus2.Scan(result_str);
#endif
#ifdef ONEWIRE_DS2482_THIRD
		OneWireBus3.Scan(result_str);
#endif
#ifdef ONEWIRE_DS2482_FOURTH
		OneWireBus4.Scan(result_str);
#endif
		journal.jprintf("OneWire found(%d): ", OW_scanTableIdx);
		while(strlen(_result_str)) {
			journal.jprintf(_result_str);
			uint16_t l = strlen(_result_str);
			_result_str += l > PRINTF_BUF-1 ? PRINTF_BUF-1 : l;
		}
#ifdef RADIO_SENSORS
		journal.jprintf("\nRadio found(%d): ", radio_received_num);
		for(uint8_t i = 0; i < radio_received_num; i++) {
			OW_scanTable[OW_scanTableIdx].num = OW_scanTableIdx + 1;
			OW_scanTable[OW_scanTableIdx].bus = 7;
			memset(&OW_scanTable[OW_scanTableIdx].address, 0, sizeof(OW_scanTable[0].address));
			OW_scanTable[OW_scanTableIdx].address[0] = tRadio;
			memcpy(&OW_scanTable[OW_scanTableIdx].address[1], &radio_received[i].serial_num, sizeof(radio_received[0].serial_num));
			char *p = result_str + strlen(result_str);
			m_snprintf(p, 64, "%d:RADIO %.1fV/%c:%.2f:%u:7;", OW_scanTable[OW_scanTableIdx].num, (float)radio_received[i].battery/10, Radio_RSSI_to_Level(radio_received[i].RSSI), (float)radio_received[i].Temp/100.0, radio_received[i].serial_num);
			journal.jprintf("%s", p);
			if(++OW_scanTableIdx >= OW_scanTable_max) break;
		}
#endif
		journal.jprintf("\n");
		OW_scan_flags = 0;
	}
}

// Установить синхронизацию по NTP
void HeatPump::set_updateNTP(boolean b)   
{
	if (b) SETBIT1(DateTime.flags,fUpdateNTP); else SETBIT0(DateTime.flags,fUpdateNTP);
}

// Получить флаг возможности синхронизации по NTP
boolean HeatPump::get_updateNTP()
{
  return GETBIT(DateTime.flags,fUpdateNTP);
}

// Установить значение текущий режим работы
void HeatPump::set_testMode(TEST_MODE b)
{
  int i=0; 
  for(i=0;i<TNUMBER;i++) sTemp[i].set_testMode(b);         // датчики температуры
  for(i=0;i<ANUMBER;i++) sADC[i].set_testMode(b);          // Датчик давления
  for(i=0;i<INUMBER;i++) sInput[i].set_testMode(b);        // Датчики сухой контакт
  for(i=0;i<FNUMBER;i++) sFrequency[i].set_testMode(b);    // Частотные датчики
  
  for(i=0;i<RNUMBER;i++) dRelay[i].set_testMode(b);        // Реле
  #ifdef EEV_DEF
  dEEV.set_testMode(b);                                    // ЭРВ
  #endif
  dFC.set_testMode(b);                                     // Частотник
  testMode=b; 
  // новый режим начинаем без ошибок - не надо при старте ошибки трутся
//  eraseError();
}


// -------------------------------------------------------------------------
// СОХРАНЕНИЕ ВОССТАНОВЛЕНИЕ  ----------------------------------------------
// -------------------------------------------------------------------------
// Обновить ВСЕ привязки удаленных датчиков
// Вызывается  при восстановлении настроек и изменении привязки, для упрощения сделана перепривязка Всех датчиков
#ifdef SENSOR_IP
   boolean HeatPump::updateLinkIP()     
    {
    uint8_t i;
    for(i=0;i<TNUMBER;i++) sTemp[i].devIP=NULL;                 // Ссылка на привязаный датчик (класс) если NULL привявязки нет
    for(i=0;i<IPNUMBER;i++)
         {
          if ((sIP[i].get_fUse())&&(sIP[i].get_link()!=-1))              // включено использование
            if ((sIP[i].get_link()>=0)&&(sIP[i].get_link()<TNUMBER))     // находится в диапазоне можно привязывать
                   sTemp[sIP[i].get_link()].devIP= &sIP[i];    
         }
    return true;     
    }
#endif

// РАБОТА с НАСТРОЙКАМИ ТН -----------------------------------------------------
// Записать настройки в память i2c: <size all><Option><DateTime><Network><message><dFC> <TYPE_sTemp><sTemp[TNUMBER]><TYPE_sADC><sADC[ANUMBER]><TYPE_sInput><sInput[INUMBER]>...<0x0000><CRC16>
// старотвый адрес I2C_SETTING_EEPROM
// Возвращает ошибку или число записанных байт
int32_t HeatPump::save(void)
{
	uint16_t crc = 0xFFFF;
	uint32_t addr = I2C_SETTING_EEPROM + 2; // +size all
	Option.numProf = Prof.get_idProfile();      // Запомнить текущий профиль, его записываем ЭТО обязательно!!!! нужно для восстановления настроек
	uint8_t tasks_suspended = TaskSuspendAll(); // Запрет других задач
	if(error == ERR_SAVE_EEPROM) error = OK;
	journal.jprintf(" Save settings ");
	DateTime.saveTime = rtcSAM3X8.unixtime();   // запомнить время сохранения настроек
	while(1) {
		// Сохранить параметры и опции отопления и бойлер, уведомления
		Option.ver = VER_SAVE;
		if(save_struct(addr, (uint8_t *) &Option, sizeof(Option), crc)) break;
		if(save_struct(addr, (uint8_t *) &DateTime, sizeof(DateTime), crc)) break;
		if(save_struct(addr, (uint8_t *) &Network, sizeof(Network), crc)) break;
		if(save_struct(addr, message.get_save_addr(), message.get_save_size(), crc)) break;
		if(save_struct(addr, dFC.get_save_addr(), dFC.get_save_size(), crc)) break; // Сохранение FC
		// Сохранение отдельных объектов ТН
		if(save_2bytes(addr, SAVE_TYPE_sTemp, crc)) break;
		for(uint8_t i = 0; i < TNUMBER; i++) if(save_struct(addr, sTemp[i].get_save_addr(), sTemp[i].get_save_size(), crc)) break; // Сохранение датчиков температуры
		if(error == ERR_SAVE_EEPROM) break;
		if(save_2bytes(addr, SAVE_TYPE_sADC, crc)) break;
		for(uint8_t i = 0; i < ANUMBER; i++) if(save_struct(addr, sADC[i].get_save_addr(), sADC[i].get_save_size(), crc)) break; // Сохранение датчика давления
		if(error == ERR_SAVE_EEPROM) break;
		if(save_2bytes(addr, SAVE_TYPE_sInput, crc)) break;
		for(uint8_t i = 0; i < INUMBER; i++) if(save_struct(addr, sInput[i].get_save_addr(), sInput[i].get_save_size(), crc)) break; // Сохранение контактных датчиков
		if(error == ERR_SAVE_EEPROM) break;
		if(save_2bytes(addr, SAVE_TYPE_sFrequency, crc)) break;
		for(uint8_t i = 0; i < FNUMBER; i++) if(save_struct(addr, sFrequency[i].get_save_addr(), sFrequency[i].get_save_size(), crc)) break; // Сохранение контактных датчиков
		if(error == ERR_SAVE_EEPROM) break;
		#ifdef SENSOR_IP
		if(save_2bytes(addr, SAVE_TYPE_sIP, crc)) break;
		for(uint8_t i = 0; i < IPNUMBER; i++) if(save_struct(addr, sIP[i].get_save_addr(), sIP[i].get_save_size(), crc)) break; // Сохранение удаленных датчиков
		if(error == ERR_SAVE_EEPROM) break;
		#endif
		#ifdef EEV_DEF
		if(save_2bytes(addr, SAVE_TYPE_dEEV, crc)) break;
		if(save_struct(addr, dEEV.get_save_addr(), dEEV.get_save_size(), crc)) break; // Сохранение ЭВР
		#endif
		#ifdef USE_ELECTROMETER_SDM
		if(save_2bytes(addr, SAVE_TYPE_dSDM, crc)) break;
		if(save_struct(addr, dSDM.get_save_addr(), dSDM.get_save_size(), crc)) break; // Сохранение SDM
		#endif
		#ifdef MQTT
		if(save_2bytes(addr, SAVE_TYPE_clMQTT, crc)) break;
		if(save_struct(addr, clMQTT.get_save_addr(), clMQTT.get_save_size(), crc)) break; // Сохранение MQTT
		#endif
		#ifdef CORRECT_POWER220
		if(save_2bytes(addr, SAVE_TYPE_PwrCorr, crc)) break;
		if(save_struct(addr, (uint8_t*)&correct_power220, sizeof(correct_power220), crc)) break; // Сохранение correct_power220
		#endif
		if(save_2bytes(addr, SAVE_TYPE_END, crc)) break;
		if(writeEEPROM_I2C(addr, (uint8_t *) &crc, sizeof(crc))) { error = ERR_SAVE_EEPROM; break; } // CRC
		addr = addr + sizeof(crc) - (I2C_SETTING_EEPROM + 2);
		if(writeEEPROM_I2C(I2C_SETTING_EEPROM, (uint8_t *) &addr, 2)) { error = ERR_SAVE_EEPROM; break; } // size all
		if((error = check_crc16_eeprom(I2C_SETTING_EEPROM + 2, addr - 2)) != OK) {
			journal.jprintf("- Verify Error!\n");
			break;
		}
		addr += 2;
		journal.jprintf("OK, wrote: %d bytes, crc: %04x\n", addr, crc);
		break;
	}
	if(tasks_suspended) xTaskResumeAll(); // Разрешение других задач

	if(error) {
		set_Error(error, (char*)__FUNCTION__);
		return error;
	}
	// суммарное число байт
	return addr;
}

// Считать настройки из памяти i2c или из RAM, если не NULL, на выходе длина или код ошибки (меньше нуля)
int32_t HeatPump::load(uint8_t *buffer, uint8_t from_RAM)
{
	uint16_t size;
	journal.jprintf(" Load settings from ");
	if(from_RAM == 0) {
		journal.jprintf("I2C");
		if(readEEPROM_I2C(I2C_SETTING_EEPROM, (byte*) &size, sizeof(size))) {
x_ReadError:
			error = ERR_CRC16_EEPROM;
x_Error:
			journal.jprintf(" - read error %d!\n", error);
			return error;
		}
		if(size > I2C_PROFILE_EEPROM - I2C_SETTING_EEPROM) { error = ERR_BAD_LEN_EEPROM; goto x_Error; }
		if(readEEPROM_I2C(I2C_SETTING_EEPROM + sizeof(size), buffer, size)) goto x_ReadError;
	} else {
		journal.jprintf("FILE");
		size = *((uint16_t *) buffer);
		buffer += sizeof(size);
	}
	journal.jprintf(", size %d, crc: ", size + 2); // sizeof(crc)
	size -= 2;
	#ifdef LOAD_VERIFICATION
	
	uint16_t crc = 0xFFFF;
	for(uint16_t i = 0; i < size; i++)  crc = _crc16(crc, buffer[i]);
	if(crc != *((uint16_t *)(buffer + size))) {
		journal.jprintf("Error: %04x != %04x!\n", crc, *((uint16_t *)(buffer + size)));
		return error = ERR_CRC16_EEPROM;
	}
	journal.jprintf("%04x ", crc);
	#else
	journal.jprintf("*No verification ");
	#endif
	uint8_t *buffer_max = buffer + size;
	size += 2;
	load_struct(&Option, &buffer, sizeof(Option));
	load_struct(&DateTime, &buffer, sizeof(DateTime));
	load_struct(&Network, &buffer, sizeof(Network));
	load_struct(message.get_save_addr(), &buffer, message.get_save_size());
	load_struct(dFC.get_save_addr(), &buffer,  dFC.get_save_size());
	int16_t type = SAVE_TYPE_LIMIT;
	while(buffer_max > buffer) { // динамические структуры
		if(*((int16_t *) buffer) <= SAVE_TYPE_END && *((int16_t *) buffer) > SAVE_TYPE_LIMIT) {
			type = *((int16_t *) buffer);
			buffer += 2;
		}
		// массивы, длина структуры должна быть меньше 128 байт, <size[1]><<number>struct>
		uint8_t n = buffer[1]; // номер элемента
		if(type == SAVE_TYPE_sTemp) { // первый в структуре идет номер датчика
			if(n < TNUMBER) { load_struct(sTemp[n].get_save_addr(), &buffer, sTemp[n].get_save_size());	sTemp[n].after_load(); } else goto xSkip;
		} else if(type == SAVE_TYPE_sADC) {
			if(n < ANUMBER) load_struct(sADC[n].get_save_addr(), &buffer, sADC[n].get_save_size()); else goto xSkip;
		} else if(type == SAVE_TYPE_sInput) {
			if(n < INUMBER) load_struct(sInput[n].get_save_addr(), &buffer, sInput[n].get_save_size()); else goto xSkip;
		} else if(type == SAVE_TYPE_sFrequency) {
			if(n < FNUMBER) load_struct(sFrequency[n].get_save_addr(), &buffer, sFrequency[n].get_save_size()); else goto xSkip;
#ifdef SENSOR_IP
		} else if(type == SAVE_TYPE_sIP) {
			if(n < IPNUMBER) { load_struct(sIP[n].get_save_addr(), &buffer, sIP[n].get_save_size()); sIP[n].after_load(); } else goto xSkip;
#endif
		// не массивы <size[1|2]><struct>
#ifdef EEV_DEF
		} else if(type == SAVE_TYPE_dEEV) {
			load_struct(dEEV.get_save_addr(), &buffer, dEEV.get_save_size()); dEEV.after_load();
#endif
#ifdef USE_ELECTROMETER_SDM
		} else if(type == SAVE_TYPE_dSDM) {
			load_struct(dSDM.get_save_addr(), &buffer, dSDM.get_save_size());
#endif
#ifdef MQTT
		} else if(type == SAVE_TYPE_clMQTT) {
			load_struct(clMQTT.get_save_addr(), &buffer, clMQTT.get_save_size());
#endif
#ifdef CORRECT_POWER220
		} else if(type == SAVE_TYPE_PwrCorr) {
			load_struct((uint8_t*)&correct_power220, &buffer, sizeof(correct_power220));
#endif
		} else if(type == SAVE_TYPE_END) {
			break;
		} else {
xSkip:		load_struct(NULL, &buffer, 0); // skip unknown type
		}
	}
#ifdef SENSOR_IP
	updateLinkIP();
#endif
	journal.jprintf("OK\n");
	return size + sizeof(crc);
}

// Проверить контрольную сумму в EEPROM для данных на выходе ошибка, длина определяется из заголовка
int8_t HeatPump::check_crc16_eeprom(int32_t addr, uint16_t size)
{
	uint8_t x;
	uint16_t crc = 0xFFFF;
	while(size--) {
		if(readEEPROM_I2C(addr++, &x, sizeof(x))) return ERR_LOAD_EEPROM;
		crc = _crc16(crc, x);
	}
	uint16_t crc2;
	if(readEEPROM_I2C(addr, (uint8_t *)&crc2, sizeof(crc2))) return ERR_LOAD_EEPROM;   // чтение -2 за вычетом CRC16 из длины
	if(crc == crc2) return OK; else return ERR_CRC16_EEPROM;
}

// СЧЕТЧИКИ -----------------------------------
 // запись счетчиков теплового насоса в I2C память
int8_t HeatPump::save_motoHour()
{
	uint8_t i;
	uint8_t errcode;
	motoHour.magic = 0xaa;   // заголовок

	for(i = 0; i < 5; i++)   // Делаем 5 попыток записи
	{
		if(!(errcode = writeEEPROM_I2C(I2C_COUNT_EEPROM, (byte*) &motoHour, sizeof(motoHour)))) break;   // Запись прошла
		journal.jprintf(" ERROR %d save counters #%d\n", errcode, i);
		_delay(i * 50);
	}
	if(errcode) {
		set_Error(ERR_SAVE2_EEPROM, (char*) __FUNCTION__);
		return ERR_SAVE2_EEPROM;
	}  // записать счетчики
	//journal.jprintf("Counters saved\n");
	return OK;
}

// чтение счетчиков теплового насоса в ЕЕПРОМ
int8_t HeatPump::load_motoHour()          
{
 byte x=0xff;
 if (readEEPROM_I2C(I2C_COUNT_EEPROM,  (byte*)&x, sizeof(x)))  { set_Error(ERR_LOAD2_EEPROM,(char*)__FUNCTION__); return ERR_LOAD2_EEPROM;}                // прочитать заголовок
 if (x!=0xaa)  {journal.jprintf("Bad header counters, skip load\n"); return ERR_HEADER2_EEPROM;}                                                  // заголвок плохой выходим
 if (readEEPROM_I2C(I2C_COUNT_EEPROM,  (byte*)&motoHour, sizeof(motoHour)))  { set_Error(ERR_LOAD2_EEPROM,(char*)__FUNCTION__); return ERR_LOAD2_EEPROM;}   // прочитать счетчики
 journal.jprintf(" Load counters OK, read: %d bytes\n",sizeof(motoHour));
 return OK; 

}
// Сборос сезонного счетчика моточасов
// параметр true - сброс всех счетчиков
void HeatPump::resetCount(boolean full)
{
	if(full) // Полный сброс счетчиков
	{
		motoHour.H1 = 0;
		motoHour.C1 = 0;
#ifdef USE_ELECTROMETER_SDM
       if (dSDM.get_link()) motoHour.E1 = dSDM.get_Energy(); // Если счетчик работает (связь не утеряна)
#endif
		motoHour.P1 = 0;
		motoHour.Z1 = 0;
		motoHour.D1 = rtcSAM3X8.unixtime();           // Дата сброса общих счетчиков
	}
	// Сезон
	motoHour.H2 = 0;
	motoHour.C2 = 0;
#ifdef USE_ELECTROMETER_SDM
	if (dSDM.get_link()) motoHour.E2 = dSDM.get_Energy();// Если счетчик работает (связь не утеряна)
    else  journal.jprintf("WARNING: Link with SDM lost - energy was not reseted!\n");
#endif
	motoHour.P2 = 0;
	motoHour.Z2 = 0;
	motoHour.D2 = rtcSAM3X8.unixtime();             // дата сброса сезонных счетчиков
	save_motoHour();  // записать счетчики
	motohour_OUT_work = 0;
}

// Обновление счетчиков моточасов, вызывается раз в минуту
// Электрическая энергия не обновляется, Тепловая энергия обновляется
void HeatPump::updateCount()
{
	if(is_compressor_on()) {
		motoHour.C1++;      // моточасы компрессора ВСЕГО
		motoHour.C2++;      // моточасы компрессора сбрасываемый счетчик (сезон)
	}
	int32_t p;
	//taskENTER_CRITICAL();
	p = motohour_OUT_work;
	motohour_OUT_work = 0;
	//taskEXIT_CRITICAL();
	p /= 1000;
	motoHour.P1 += p;
	motoHour.P2 += p;
	if(get_State() == pWORK_HP) {
		motoHour.H1++;          // моточасы ТН ВСЕГО
		motoHour.H2++;          // моточасы ТН сбрасываемый счетчик (сезон)
	}
}

// После любого изменения часов необходимо пересчитать все времна которые используются
// параметр изменение времени - корректировка
void HeatPump::updateDateTime(int32_t  dTime)
{
  if(dTime!=0)                                   // было изменено время, надо скорректировать переменные времени
    {
    Prof.SaveON.startTime=Prof.SaveON.startTime+dTime; // время пуска ТН (для организации задержки включения включение ЭРВ)
    if (timeON>0)          timeON=timeON+dTime;                               // время включения контроллера для вычисления UPTIME
    if (startCompressor>0) startCompressor=startCompressor+dTime;             // время пуска компрессора
    if (stopCompressor>0)  stopCompressor=stopCompressor+dTime;               // время останова компрессора
    if (countNTP>0)        countNTP=countNTP+dTime;                           // число секунд с последнего обновления по NTP
    if (offBoiler>0)       offBoiler=offBoiler+dTime;                         // время выключения нагрева ГВС ТН (необходимо для переключения на другие режимы на ходу)
    if (startDefrost>0)    startDefrost=startDefrost+dTime;                   // время срабатывания датчика разморозки
    if (timeBoilerOff>0)   timeBoilerOff=timeBoilerOff+dTime;                 // Время переключения (находу) с ГВС на отопление или охлаждения (нужно для временной блокировки защит) если 0 то переключения не было
    if (startSallmonela>0) startSallmonela=startSallmonela+dTime;             // время начала обеззараживания
    } 
}
      

// -------------------------------------------------------------------------
// НАСТРОЙКИ ТН ------------------------------------------------------------
// -------------------------------------------------------------------------
// Сброс настроек теплового насоса
void HeatPump::resetSettingHP()
{  
  uint8_t i;
  Prof.initProfile();                           // Инициализировать профиль по умолчанию
    
  flags = 0;
  NO_Power = 0;
  Status.modWork=pOFF;;                         // Что сейчас делает ТН (7 стадий)
  Status.State=pOFF_HP;                         // Сотояние ТН - выключен
  Status.ret=pNone;                             // точка выхода алгоритма
  motoHour.magic=0xaa;                          // волшебное число
  motoHour.flags=0x00;
  SETBIT0(motoHour.flags,fHP_ON);               // насос выключен
  motoHour.H1=0;                                // моточасы ТН ВСЕГО
  motoHour.H2=0;                                // моточасы ТН сбрасываемый счетчик (сезон)
  motoHour.C1=0;                                // моточасы компрессора ВСЕГО
  motoHour.C2=0;                                // моточасы компрессора сбрасываемый счетчик (сезон)
  motoHour.E1=0.0;                              // Значение потреленный энергии в момент пуска  актуально при использовании счетчика SDM120 (вычитаем текущее и получам итого)
  motoHour.E2=0.0;                              // Значение потреленный энергии в начале сезона актуально при использовании счетчика SDM120 (вычитаем текущее и получам итого)
  motoHour.D1=rtcSAM3X8.unixtime();             // Дата сброса общих счетчиков
  motoHour.D2=rtcSAM3X8.unixtime();             // дата сброса сезонных счетчиков
 
  startPump=false;                              // Признак работы задачи насос
  flagRBOILER=false;                            // не идет нагрев бойлера
  fSD=false;                                    // СД карта не рабоатет
  fSPIFlash=false;                              // Признак наличия (физического) spi диска - диска нет по умолчанию
  
  startWait=false;                              // Начало работы с ожидания
  onBoiler=false;                               // Если true то идет нагрев бойлера
  onSallmonela=false;                           // Если true то идет Обеззараживание
  command=pEMPTY;                               // Команд на выполнение нет
  next_command=pEMPTY;
  PauseStart=0;                                 // начать отсчет задержки пред стартом с начала
  startRAM=0;                                   // Свободная память при старте FREE Rtos - пытаемся определить свободную память при работе
  lastEEV=-1;                                   // значение шагов ЭРВ перед выключением  -1 - первое включение
  num_repeat=0;                                 // текушее число попыток 0 - т.е еще не было работы
  num_resW5200=0;                               // текущее число сбросов сетевого чипа
  num_resMutexSPI=0;                            // текущее число сброса митекса SPI
  num_resMutexI2C=0;                            // текущее число сброса митекса I2C
  num_resMQTT=0;                                // число повторных инициализация MQTT клиента
  num_resPing=0;                                // число не прошедших пингов

  fullCOP=-1000;                                // Полный СОР  сотые -1000 признак невозможности расчета
  COP=-1000;                                    // Чистый COP сотые  -1000 признак невозможности расчета
   
  // Инициализациия различных времен
  DateTime.saveTime=0;                          // дата и время сохранения настроек в eeprom
  timeON=0;                                     // время включения контроллера для вычисления UPTIME
  countNTP=0;                                   // число секунд с последнего обновления по NTP
  startCompressor=0;                            // время пуска компрессора
  stopCompressor=0;                             // время останова компрессора
  offBoiler=0;                                  // время выключения нагрева ГВС ТН (необходимо для переключения на другие режимы на ходу)
  startDefrost=0;                               // время срабатывания датчика разморозки
  timeNTP=0;                                    // Время обновления по NTP в тиках (0-сразу обновляемся)
  startSallmonela=0;                            // время начала обеззараживания
  command_completed = 0;
  time_Sun_ON = 0;
  time_Sun_OFF = 0;
   
  safeNetwork=false;                            // режим safeNetwork

  // Установка сетевых параметров по умолчанию
  if (defaultDHCP) SETBIT1(Network.flags,fDHCP);else SETBIT0(Network.flags,fDHCP); // использование DHCP
  Network.ip=IPAddress(defaultIP);              // ip адрес
  Network.sdns=IPAddress(defaultSDNS);          // сервер dns
  Network.gateway=IPAddress(defaultGateway);    // шлюз
  Network.subnet=IPAddress(defaultSubnet);      // подсеть
  Network.port=defaultPort;                     // порт веб сервера по умолчанию
  memcpy(Network.mac,defaultMAC,6);             // mac адрес
  Network.resSocket=30;                         // Время очистки сокетов
  Network.resW5200=0;                           // Время сброса чипа
  countResSocket=0;                             // Число сбросов сокетов
  SETBIT1(Network.flags, fInitW5200);           // Ежеминутный контроль SPI для сетевого чипа
  SETBIT0(Network.flags,fPass);                 // !save! Использование паролей
  strcpy(Network.passUser,"user");              // !save! Пароль пользователя
  strcpy(Network.passAdmin,"admin");            // !save! Пароль администратора
  Network.sizePacket=1465;                      // !save! размер пакета для отправки
  SETBIT0(Network.flags,fNoAck);                // !save! флаг Не ожидать ответа ACK
  Network.delayAck=10;                          // !save! задержка мсек перед отправкой пакета
  strcpy(Network.pingAdr,PING_SERVER );         // !save! адрес для пинга
  Network.pingTime=60*60;                       // !save! время пинга в секундах
  SETBIT0(Network.flags,fNoPing);               // !save! Запрет пинга контроллера
  
// Время
  SETBIT1(DateTime.flags,fUpdateNTP);           // Обновление часов по NTP
  SETBIT1(DateTime.flags,fUpdateI2C);           // Обновление часов I2C
  
  strcpy(DateTime.serverNTP,(char*)NTP_SERVER);  // NTP сервер по умолчанию
  DateTime.timeZone=TIME_ZONE;                   // Часовой пояс

// Опции теплового насоса
  // Временные задержки
  Option.ver = VER_SAVE;
  Option.delayOnPump         = DEF_DELAY_ON_PUMP;
  Option.delayOffPump		 = DEF_DELAY_OFF_PUMP;    
  Option.delayStartRes	     = DEF_DELAY_START_RES;   
  Option.delayRepeadStart    = DEF_DELAY_REPEAD_START;        
  Option.delayDefrostOn	     = DEF_DELAY_DEFROST_ON;         
  Option.delayDefrostOff	 = DEF_DELAY_DEFROST_OFF;        
  Option.delayTRV		     = DEF_DELAY_TRV;                
  Option.delayBoilerSW	     = DEF_DELAY_BOILER_SW;          
  Option.delayBoilerOff	     = DEF_DELAY_BOILER_OFF;         
  Option.numProf=Prof.get_idProfile(); //  Профиль не загружен по дефолту 0 профиль
  Option.nStart = 3;                   //  Число попыток пуска компрессора
  Option.tempRHEAT = 1000;             //  Значение температуры для управления RHEAT (по умолчанию режим резерв - 10 градусов в доме)
  Option.pausePump = 600;              //  Время паузы  насоса при выключенном компрессоре, сек
  Option.workPump = 15;                //  Время работы  насоса при выключенном компрессоре, сек
  Option.tChart = 60;                  //  период накопления статистики по умолчанию 60 секунд
  SETBIT0(Option.flags,fAddHeat);      //  Использование дополнительного тена при нагреве НЕ ИСПОЛЬЗОВАТЬ
  SETBIT0(Option.flags,fTypeRHEAT);    //  Использование дополнительного тена по умолчанию режим резерв
  SETBIT1(Option.flags,fBeep);         //  Звук
  SETBIT1(Option.flags,fNextion);      //  дисплей Nextion
  SETBIT0(Option.flags,fHistory);      //  Сброс статистика на карту
  SETBIT0(Option.flags,fSaveON);       //  флаг записи в EEPROM включения ТН
  Option.sleep = 5;                    //  Время засыпания минуты
  Option.dim = 80;                     //  Якрость %
  Option.pause = 5 * 60;               // Минимальное время простоя компрессора, секунды
#ifdef USE_SUN_COLLECTOR
  Option.SunTDelta = SUN_TDELTA;
#endif

 // инициализация статистика дополнительно помимо датчиков
  ChartRCOMP.init(!dFC.get_present());               // Статистика по включению компрессора только если нет частотника
  #ifdef EEV_DEF
  ChartOVERHEAT.init(true);                          // перегрев
  ChartOVERHEAT2.init(true);                         // перегрев2
  ChartTPEVA.init(sADC[PEVA].get_present());         // температура расчитанная из давления  кипения
  if   (sADC[PCON].get_present()) ChartTPCON.init(sADC[PCON].get_present());  // температура расчитанная из давления  конденсации
  else ChartTPCON.init(sTemp[TCONOUTG].get_present());    // Если датчика высокого давления нет то конденсацию рассчитываем по формуле sTemp[get_modeHouse()==pCOOL?TEVAOUTG:TCONOUTG].get_Temp() + 200;

  #endif

  for(i=0;i<FNUMBER;i++)   // По всем частотным датчикам
    {
     if (strcmp(sFrequency[i].get_name(),"FLOWCON")==0)                          // если есть датчик потока по конденсатору
       {
       ChartCOP.init(dFC.get_present()&sFrequency[i].get_present()&sTemp[TCONING].get_present()&sTemp[TCONOUTG].get_present()); // Коэффициент преобразования
    //   Serial.print("StatCOP="); Serial.println(dFC.get_present()&sFrequency[i].get_present()&sTemp[TCONING].get_present()&sTemp[TCONOUTG].get_present()) ;
       }
       
    }
   #ifdef USE_ELECTROMETER_SDM  
     #ifdef FLOWCON 
     if((sTemp[TCONOUTG].Chart.get_present())&&(sTemp[TCONING].Chart.get_present())) ChartFullCOP.init(true);  // ПОЛНЫЙ Коэффициент преобразования
     #endif   
   #endif
 };

// --------------------------------------------------------------------
// ФУНКЦИИ РАБОТЫ С НАСТРОЙКАМИ ТН ------------------------------------
// --------------------------------------------------------------------
// Сетевые настройки --------------------------------------------------
//Установить параметр из строки
boolean HeatPump::set_network(char *var, char *c)
{ 
 uint8_t x;
 float zp; 
 if (strcmp(c,cZero)==0) x=0;
 else if (strcmp(c,cOne)==0) x=1;
 else if (strcmp(c,"2")==0) x=2;
 else if (strcmp(c,"3")==0) x=3;
 else if (strcmp(c,"4")==0) x=4;
 else if (strcmp(c,"5")==0) x=5;
 else if (strcmp(c,"6")==0) x=6;
 else x=-1;
 if(strcmp(var,net_IP)==0){          return parseIPAddress(c, '.', Network.ip);                 }else  
 if(strcmp(var,net_DNS)==0){        return parseIPAddress(c, '.', Network.sdns);               }else
 if(strcmp(var,net_GATEWAY)==0){     return parseIPAddress(c, '.', Network.gateway);            }else                
 if(strcmp(var,net_SUBNET)==0){      return parseIPAddress(c, '.', Network.subnet);             }else  
 if(strcmp(var,net_DHCP)==0){        if (strcmp(c,cZero)==0) { SETBIT0(Network.flags,fDHCP); return true;}
                                    else if (strcmp(c,cOne)==0) { SETBIT1(Network.flags,fDHCP);  return true;}
                                    else return false;  
                                    }else  
 if(strcmp(var,net_MAC)==0){         return parseBytes(c, ':', Network.mac, 6, 16);             }else  
 if(strcmp(var,net_RES_SOCKET)==0){ switch (x)
			                       {
			                        case 0: Network.resSocket=0;     return true;  break;
			                        case 1: Network.resSocket=10;    return true;  break;
			                        case 2: Network.resSocket=30;    return true;  break;
			                        case 3: Network.resSocket=90;    return true;  break;
			                        default:                    return false; break;   
			                       }                                          }else  
 if(strcmp(var,net_RES_W5200)==0){ switch (x)
			                       {
			                        case 0: Network.resW5200=0;        return true;  break;
			                        case 1: Network.resW5200=60*60*6;  return true;  break;   // 6 часов хранение в секундах
			                        case 2: Network.resW5200=60*60*24; return true;  break;   // 24 часа
			                        default:                      return false; break;   
			                       }                                          }else   
 if(strcmp(var,net_PASS)==0){        if (strcmp(c,cZero)==0) { SETBIT0(Network.flags,fPass); return true;}
                                    else if (strcmp(c,cOne)==0) {SETBIT1(Network.flags,fPass);  return true;}
                                    else return false;  
                                    }else
 if(strcmp(var,net_PASSUSER)==0){    strcpy(Network.passUser,c);set_hashUser(); return true;   }else                 
 if(strcmp(var,net_PASSADMIN)==0){   strcpy(Network.passAdmin,c);set_hashAdmin(); return true; }else  
 if(strcmp(var,net_SIZE_PACKET)==0){ zp=my_atof(c);  
                                    if (zp==-9876543.00) return   false;    
                                    else if((zp<64)||(zp>2048)) return   false;   
                                    else Network.sizePacket=(int)zp; return true;
                                    }else  
 if(strcmp(var,net_INIT_W5200)==0){    // флаг Ежеминутный контроль SPI для сетевого чипа
                       if (strcmp(c,cZero)==0) { SETBIT0(Network.flags,fInitW5200); return true;}
                       else if (strcmp(c,cOne)==0) { SETBIT1(Network.flags,fInitW5200);  return true;}
                       else return false;  
                       }else 
 if(strcmp(var,net_PORT)==0){        zp=my_atof(c);  
                       if (zp==-9876543.00) return        false;    
                       else if((zp<1)||(zp>65535)) return false;   
                       else Network.port=(int)zp; return  true;
                       }else     
 if(strcmp(var,net_NO_ACK)==0){      if (strcmp(c,cZero)==0) { SETBIT0(Network.flags,fNoAck); return true;}
                       else if (strcmp(c,cOne)==0) { SETBIT1(Network.flags,fNoAck);  return true;}
                       else return false;  
                       }else  
 if(strcmp(var,net_DELAY_ACK)==0){   zp=my_atof(c);  
                       if (zp==-9876543.00) return            false;    
                       else if((zp<1)||(zp>50)) return        false;   
                       else Network.delayAck=(int)zp; return  true;
                       }else         
 if(strcmp(var,net_PING_ADR)==0){     if (strlen(c)<sizeof(Network.pingAdr)) { strcpy(Network.pingAdr,c); return true;} else return false; }else    
 if(strcmp(var,net_PING_TIME)==0){switch (x)
			                       {
			                        case 0: Network.pingTime=0;        return true;  break;
			                        case 1: Network.pingTime=1*60;     return true;  break;
			                        case 2: Network.pingTime=5*60;     return true;  break;
			                        case 3: Network.pingTime=20*60;    return true;  break;
			                        case 4: Network.pingTime=60*60;    return true;  break;
			                        case 5: Network.pingTime=120*60;    return true;  break;
			                        default:                           return false; break;   
			                       }                                          }else   
 if(strcmp(var,net_NO_PING)==0){     if (strcmp(c,cZero)==0) { SETBIT0(Network.flags,fNoPing);      pingW5200(HP.get_NoPing()); return true;}
                       else if (strcmp(c,cOne)==0) { SETBIT1(Network.flags,fNoPing); pingW5200(HP.get_NoPing()); return true;}
                       else return false;  
                       }else                                                                                                                                                                               
   return false;
}
// Сетевые настройки --------------------------------------------------
//Получить параметр из строки
char* HeatPump::get_network(char *var,char *ret)
{
 if(strcmp(var,net_IP)==0){   return strcat(ret,IPAddress2String(Network.ip));          }else  
 if(strcmp(var,net_DNS)==0){      return strcat(ret,IPAddress2String(Network.sdns));   }else
 if(strcmp(var,net_GATEWAY)==0){   return strcat(ret,IPAddress2String(Network.gateway));}else                
 if(strcmp(var,net_SUBNET)==0){    return strcat(ret,IPAddress2String(Network.subnet)); }else  
 if(strcmp(var,net_DHCP)==0){      if (GETBIT(Network.flags,fDHCP)) return  strcat(ret,(char*)cOne);
                                   else      return  strcat(ret,(char*)cZero);          }else
 if(strcmp(var,net_MAC)==0){       return strcat(ret,MAC2String(Network.mac));          }else  
 if(strcmp(var,net_RES_SOCKET)==0){
 	return web_fill_tag_select(ret, "never:0;10 sec:0;30 sec:0;90 sec:0;",
				Network.resSocket == 0 ? 0 :
				Network.resSocket == 10 ? 1 :
				Network.resSocket == 30 ? 2 :
				Network.resSocket == 90 ? 3 : 4);
 }else if(strcmp(var,net_RES_W5200)==0){
	 	return web_fill_tag_select(ret, "never:0;10 sec:0;30 sec:0;90 sec:0;",
					Network.resW5200 == 0 ? 0 :
					Network.resW5200 == 60*60*6 ? 1 :
					Network.resW5200 == 60*60*24 ? 2 : 3);
  }else if(strcmp(var,net_PASS)==0){      if (GETBIT(Network.flags,fPass)) return  strcat(ret,(char*)cOne);
                                    else      return  strcat(ret,(char*)cZero);               }else
  if(strcmp(var,net_PASSUSER)==0){  return strcat(ret,Network.passUser);                      }else                 
  if(strcmp(var,net_PASSADMIN)==0){ return strcat(ret,Network.passAdmin);                     }else   
  if(strcmp(var,net_SIZE_PACKET)==0){return _itoa(Network.sizePacket,ret);          }else   
    if(strcmp(var,net_INIT_W5200)==0){if (GETBIT(Network.flags,fInitW5200)) return  strcat(ret,(char*)cOne);       // флаг Ежеминутный контроль SPI для сетевого чипа
                                      else      return  strcat(ret,(char*)cZero);               }else      
    if(strcmp(var,net_PORT)==0){return _itoa(Network.port,ret);                       }else    // Порт веб сервера
    if(strcmp(var,net_NO_ACK)==0){    if (GETBIT(Network.flags,fNoAck)) return  strcat(ret,(char*)cOne);
                                      else      return  strcat(ret,(char*)cZero);          }else     
    if(strcmp(var,net_DELAY_ACK)==0){return _itoa(Network.delayAck,ret);         }else    
    if(strcmp(var,net_PING_ADR)==0){  return strcat(ret,Network.pingAdr);                  }else
    if(strcmp(var,net_PING_TIME)==0){
    	return web_fill_tag_select(ret, "never:0;1 min:0;5 min:0;20 min:0;60 min:0;120 min:0;",
					Network.pingTime == 0 ? 0 :
					Network.pingTime == 1*60 ? 1 :
					Network.pingTime == 5*60 ? 2 :
					Network.pingTime == 20*60 ? 3 :
					Network.pingTime == 60*60 ? 4 :
					Network.pingTime == 120*60 ? 5 : 6);
    } else if(strcmp(var,net_NO_PING)==0){if (GETBIT(Network.flags,fNoPing)) return  strcat(ret,(char*)cOne);
                                   else      return  strcat(ret,(char*)cZero);                        }else                                                                                          

 return strcat(ret,(char*)cInvalid);
}

// Установить параметр дата и время из строки
boolean  HeatPump::set_datetime(char *var, char *c)
{
	float tz;
	int16_t m,h,d,mo,y;
	int16_t buf[4];
	uint32_t oldTime=rtcSAM3X8.unixtime(); // запомнить время
	int32_t  dTime=0;
	if(strcmp(var,time_TIME)==0){ if (!parseInt16_t(c, ':',buf,2,10)) return false;
						  h=buf[0]; m=buf[1];
						  rtcSAM3X8.set_time (h,m,0);  // внутренние
						  setTime_RtcI2C(rtcSAM3X8.get_hours(), rtcSAM3X8.get_minutes(),rtcSAM3X8.get_seconds()); // внешние
						  dTime=rtcSAM3X8.unixtime()-oldTime;// получить изменение времени
					  }else
	if(strcmp(var,time_DATE)==0){
						  char ch, f = 0; m = 0;
						  do { // ищем разделитель чисел
							  ch = c[m++];
							  if(ch >= '0' && ch <= '9') f = 1; else if(f == 1) { f = 2; break; }
						  } while(ch != '\0');
						  if(f != 2 || !parseInt16_t(c, ch,buf,3,10)) return false;
						  d=buf[0]; mo=buf[1]; y=buf[2];
						  rtcSAM3X8.set_date(d,mo,y); // внутренние
						  setDate_RtcI2C(rtcSAM3X8.get_days(), rtcSAM3X8.get_months(),rtcSAM3X8.get_years()); // внешние
						  dTime=rtcSAM3X8.unixtime()-oldTime;// получить изменение времени
					  }else
	if(strcmp(var,time_NTP)==0){if(strlen(c)==0) return false;                                                 // пустая строка
						  if(strlen(c)>NTP_SERVER_LEN) return false;                                     // слишком длиная строка
						  else { strcpy(DateTime.serverNTP,c); return true;  }                           // ок сохраняем
					  }else
	if(strcmp(var,time_UPDATE)==0){     if (strcmp(c,cZero)==0) { SETBIT0(DateTime.flags,fUpdateNTP); return true;}
						  else if (strcmp(c,cOne)==0) { SETBIT1( DateTime.flags,fUpdateNTP);countNTP=0; return true;}
						  else return false;
					  }else
	if(strcmp(var,time_TIMEZONE)==0){  tz=my_atof(c);
						  if (tz==-9876543.00) return   false;
						  else if((tz<-12)||(tz>12)) return   false;
						  else DateTime.timeZone=(int)tz; return true;
					  }else
	if(strcmp(var,time_UPDATE_I2C)==0){ if (strcmp(c,cZero)==0) { SETBIT0(DateTime.flags,fUpdateI2C); return true;}
						  else if (strcmp(c,cOne)==0) {SETBIT1( DateTime.flags,fUpdateI2C);countNTP=0; return true;}
						  else return false;
					  } else return  false;

	if(dTime!=0)  updateDateTime(dTime);    // было изменено время, надо скорректировать переменные времени
	return true;
}
// Получить параметр дата и время из строки
char* HeatPump::get_datetime(char *var,char *ret)
{
if(strcmp(var,time_TIME)==0)  {return strcat(ret,NowTimeToStr1());                      }else  
if(strcmp(var,time_DATE)==0)  {return strcat(ret,NowDateToStr());                       }else  
if(strcmp(var,time_NTP)==0)   {return strcat(ret,DateTime.serverNTP);                   }else                
if(strcmp(var,time_UPDATE)==0){if (GETBIT(DateTime.flags,fUpdateNTP)) return  strcat(ret,(char*)cOne);
                               else                                   return  strcat(ret,(char*)cZero);}else  
if(strcmp(var,time_TIMEZONE)==0){return  _itoa(DateTime.timeZone,ret);         }else  
if(strcmp(var,time_UPDATE_I2C)==0){ if (GETBIT(DateTime.flags,fUpdateI2C)) return  strcat(ret,(char*)cOne);
                                    else                                   return  strcat(ret,(char*)cZero); }else      
return strcat(ret,(char*)cInvalid);
}

// Установить опции ТН из числа (float), "set_oHP"
boolean HeatPump::set_optionHP(char *var, float x)   
{
   if(strcmp(var,option_ADD_HEAT)==0)         {switch ((int)x)  //использование дополнительного нагревателя (значения 1 и 0)
						                           {
						                            case 0:  SETBIT0(Option.flags,fAddHeat);                                    return true; break;  // использование запрещено
						                            case 1:  SETBIT1(Option.flags,fAddHeat);SETBIT0(Option.flags,fTypeRHEAT);   return true; break;  // резерв
						                            case 2:  SETBIT1(Option.flags,fAddHeat);SETBIT1(Option.flags,fTypeRHEAT);   return true; break;  // бивалент
						                            default: SETBIT1(Option.flags,fAddHeat);SETBIT0(Option.flags,fTypeRHEAT);   return true; break;  // Исправить по умолчанию
						                           } }else  // бивалент
   if(strcmp(var,option_TEMP_RHEAT)==0)       {if ((x>=-30.0)&&(x<=30.0))  {Option.tempRHEAT=rd(x, 100); return true;} else return false; }else     // температура управления RHEAT (градусы)
   if(strcmp(var,option_PUMP_WORK)==0)        {if ((x>=0)&&(x<=65535)) {Option.workPump=x; return true;} else return false;}else                // работа насоса конденсатора при выключенном компрессоре МИНУТЫ
   if(strcmp(var,option_PUMP_PAUSE)==0)       {if ((x>=0)&&(x<=65535)) {Option.pausePump=x; return true;} else return false;}else               // пауза между работой насоса конденсатора при выключенном компрессоре МИНУТЫ
   if(strcmp(var,option_ATTEMPT)==0)          { if ((x>=0)&&(x<=255)) {Option.nStart=x; return true;} else return false;  }else                // число попыток пуска
   if(strcmp(var,option_TIME_CHART)==0)       { if(x>0) { if (get_State()==pWORK_HP) startChart(); Option.tChart = x; return true; } else return false; } else // Сбросить статистистику, начать отсчет заново
   if(strcmp(var,option_BEEP)==0)             {if (x==0) {SETBIT0(Option.flags,fBeep); return true;} else if (x==1) {SETBIT1(Option.flags,fBeep); return true;} else return false;  }else            // Подача звукового сигнала
   if(strcmp(var,option_NEXTION)==0)          { Option.flags = (Option.flags & ~(1<<fNextion)) | ((x!=0)<<fNextion); updateNextion(); return true; } else            // использование дисплея nextion
   if(strcmp(var,option_NEXTION_WORK)==0)     { Option.flags = (Option.flags & ~(1<<fNextionOnWhileWork)) | ((x!=0)<<fNextionOnWhileWork); updateNextion(); return true; } else            // использование дисплея nextion
   if(strcmp(var,option_NEXT_SLEEP)==0)       {if (x>=0.0) {Option.sleep=x; updateNextion(); return true;} else return false;  }else       // Время засыпания секунды NEXTION минуты
#ifdef NEXTION
   if(strcmp(var,option_NEXT_DIM)==0)         {if ((x>=1.0)&&(x<=100.0)) {Option.dim=x; myNextion.set_dim(Option.dim); return true;} else return false; }else       // Якрость % NEXTION
#endif
   if(strcmp(var,option_History)==0)          {if (x==0) {SETBIT0(Option.flags,fHistory); return true;} else if (x==1) {SETBIT1(Option.flags,fHistory); return true;} else return false;       }else       // Сбрасывать статистику на карту
   if(strcmp(var,option_SDM_LOG_ERR)==0)      {if (x==0) {SETBIT0(Option.flags,fSDMLogErrors); return true;} else if (x==1) {SETBIT1(Option.flags,fSDMLogErrors); return true;} else return false;       }else
   if(strcmp(var,option_WebOnSPIFlash)==0)    { Option.flags = (Option.flags & ~(1<<fWebStoreOnSPIFlash)) | ((x!=0)<<fWebStoreOnSPIFlash); return true; } else
   if(strcmp(var,option_LogWirelessSensors)==0){ Option.flags = (Option.flags & ~(1<<fLogWirelessSensors)) | ((x!=0)<<fLogWirelessSensors); return true; } else
   if(strcmp(var,option_SAVE_ON)==0)          {if (x==0) {SETBIT0(Option.flags,fSaveON); return true;} else if (x==1) {SETBIT1(Option.flags,fSaveON); return true;} else return false;    }else             // флаг записи в EEPROM включения ТН (восстановление работы после перезагрузки)
   if(strncmp(var,option_SGL1W, sizeof(option_SGL1W)-1)==0) {
	   uint8_t bit = var[sizeof(option_SGL1W)-1] - '0' - 2;
	   if(bit <= 2) {
		   Option.flags = (Option.flags & ~(1<<(f1Wire2TSngl + bit))) | (x == 0 ? 0 : (1<<(f1Wire2TSngl + bit)));
		   return true;
	   }
   } else
   if(strcmp(var,option_SunRegGeo)==0)        { Option.flags = (Option.flags & ~(1<<fSunRegenerateGeo)) | ((x!=0)<<fSunRegenerateGeo); return true; }else
   if(strcmp(var,option_SunRegGeoTemp)==0)    { Option.SunRegGeoTemp = rd(x, 100); return true; }else
   if(strcmp(var,option_SunTDelta)==0)        { Option.SunTDelta = rd(x, 100); return true; }else
   if(strcmp(var,option_PAUSE)==0) { if ((x>=0)&&(x<=60)) {Option.pause=x*60; return true;} else return false; }else                         // минимальное время простоя компрессора с переводом в минуты но хранится в секундах!!!!!
   if(strcmp(var,option_DELAY_ON_PUMP)==0)    {if ((x>=0.0)&&(x<=900.0)) {Option.delayOnPump=x; return true;} else return false;}else        // Задержка включения компрессора после включения насосов (сек).
   if(strcmp(var,option_DELAY_OFF_PUMP)==0)   {if ((x>=0.0)&&(x<=900.0)) {Option.delayOffPump=x; return true;} else return false;}else       // Задержка выключения насосов после выключения компрессора (сек).
   if(strcmp(var,option_DELAY_START_RES)==0)  {if ((x>=0.0)&&(x<=6000.0)) {Option.delayStartRes=x; return true;} else return false;}else     // Задержка включения ТН после внезапного сброса контроллера (сек.)
   if(strcmp(var,option_DELAY_REPEAD_START)==0){if ((x>=0.0)&&(x<=6000.0)) {Option.delayRepeadStart=x; return true;} else return false;}else // Задержка перед повторным включениме ТН при ошибке (попытки пуска) секунды
   if(strcmp(var,option_DELAY_DEFROST_ON)==0) {if ((x>=0.0)&&(x<=600.0)) {Option.delayDefrostOn=x; return true;} else return false;}else     // ДЛЯ ВОЗДУШНОГО ТН Задержка после срабатывания датчика перед включением разморозки (секунды)
   if(strcmp(var,option_DELAY_DEFROST_OFF)==0){if ((x>=0.0)&&(x<=600.0)) {Option.delayDefrostOff=x; return true;} else return false;}else    // ДЛЯ ВОЗДУШНОГО ТН Задержка перед выключением разморозки (секунды)
   if(strcmp(var,option_DELAY_TRV)==0)        {if ((x>=0.0)&&(x<=600.0)) {Option.delayTRV=x; return true;} else return false;}else           // Задержка между переключением 4-х ходового клапана и включением компрессора, для выравнивания давлений (сек). Если включены эти опции (переключение тепло-холод)
   if(strcmp(var,option_DELAY_BOILER_SW)==0)  {if ((x>=0.0)&&(x<=1200.0)) {Option.delayBoilerSW=x; return true;} else return false;}else     // Пауза (сек) после переключение ГВС - выравниваем температуру в контуре отопления/ГВС что бы сразу защиты не сработали
   if(strcmp(var,option_DELAY_BOILER_OFF)==0) {if ((x>=0.0)&&(x<=1200.0)) {Option.delayBoilerOff=x; return true;} else return false;}        // Время (сек) на сколько блокируются защиты при переходе с ГВС на отопление и охлаждение слишком горяче после ГВС
   return false; 
}

// Получить опции ТН, результат добавляется в ret, "get_oHP"
char* HeatPump::get_optionHP(char *var, char *ret)
{
   if(strcmp(var,option_ADD_HEAT)==0)         {if(!GETBIT(Option.flags,fAddHeat))          return strcat(ret,(char*)"none:1;reserve:0;bivalent:0;");       // использование ТЭН запрещено
                                               else if(!GETBIT(Option.flags,fTypeRHEAT))   return strcat(ret,(char*)"none:0;reserve:1;bivalent:0;");       // резерв
                                               else                                        return strcat(ret,(char*)"none:0;reserve:0;bivalent:1;");}else  // бивалент
   if(strcmp(var,option_TEMP_RHEAT)==0)       {_ftoa(ret,(float)Option.tempRHEAT/100,1); return ret; }else                                         // температура управления RHEAT (градусы)
   if(strcmp(var,option_PUMP_WORK)==0)        {return _itoa(Option.workPump,ret);}else                                                           // работа насоса конденсатора при выключенном компрессоре МИНУТЫ
   if(strcmp(var,option_PUMP_PAUSE)==0)       {return _itoa(Option.pausePump,ret);}else                                                          // пауза между работой насоса конденсатора при выключенном компрессоре МИНУТЫ
   if(strcmp(var,option_ATTEMPT)==0)          {return _itoa(Option.nStart,ret);}else                                                             // число попыток пуска
   if(strcmp(var,option_TIME_CHART)==0)       {return _itoa(Option.tChart,ret);} else
   if(strcmp(var,option_BEEP)==0)             {if(GETBIT(Option.flags,fBeep)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero); }else            // Подача звукового сигнала
   if(strcmp(var,option_NEXTION)==0)          { return strcat(ret, (char*)(GETBIT(Option.flags,fNextion) ? cOne : cZero)); } else         // использование дисплея nextion
   if(strcmp(var,option_NEXTION_WORK)==0)     { return strcat(ret, (char*)(GETBIT(Option.flags,fNextionOnWhileWork) ? cOne : cZero)); } else         // использование дисплея nextion
   if(strcmp(var,option_History)==0)          {if(GETBIT(Option.flags,fHistory)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero);   }else            // Сбрасывать статистику на карту
   if(strcmp(var,option_SDM_LOG_ERR)==0)      {if(GETBIT(Option.flags,fSDMLogErrors)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero);   }else
   if(strcmp(var,option_WebOnSPIFlash)==0)    { return strcat(ret, (char*)(GETBIT(Option.flags,fWebStoreOnSPIFlash) ? cOne : cZero)); } else
   if(strcmp(var,option_LogWirelessSensors)==0){ return strcat(ret, (char*)(GETBIT(Option.flags,fLogWirelessSensors) ? cOne : cZero)); } else
   if(strcmp(var,option_SAVE_ON)==0)          {if(GETBIT(Option.flags,fSaveON)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero);    }else           // флаг записи в EEPROM включения ТН (восстановление работы после перезагрузки)
   if(strcmp(var,option_NEXT_SLEEP)==0)       {return _itoa(Option.sleep,ret);                                                     }else            // Время засыпания секунды NEXTION минуты
   if(strcmp(var,option_NEXT_DIM)==0)         {return _itoa(Option.dim,ret);                                                       }else            // Якрость % NEXTION
   if(strncmp(var,option_SGL1W, sizeof(option_SGL1W)-1)==0) {
	   uint8_t bit = var[sizeof(option_SGL1W)-1] - '0' - 2;
	   if(bit <= 2) {
		   return strcat(ret,(char*)(GETBIT(Option.flags, f1Wire2TSngl + bit) ? cOne : cZero));
	   }
   } else
   if(strcmp(var,option_SunRegGeo)==0)    	  {return _itoa(GETBIT(Option.flags, fSunRegenerateGeo), ret);}else
   if(strcmp(var,option_SunRegGeoTemp)==0)    {_ftoa(ret,(float)Option.SunRegGeoTemp/100,1); return ret; }else
   if(strcmp(var,option_SunTDelta)==0)        {_ftoa(ret,(float)Option.SunTDelta/100,1); return ret; }else
   if(strcmp(var,option_PAUSE)==0)            {return _itoa(Option.pause/60,ret); } else        // минимальное время простоя компрессора с переводом в минуты но хранится в секундах!!!!!
   if(strcmp(var,option_DELAY_ON_PUMP)==0)    {return _itoa(Option.delayOnPump,ret);}else       // Задержка включения компрессора после включения насосов (сек).
   if(strcmp(var,option_DELAY_OFF_PUMP)==0)   {return _itoa(Option.delayOffPump,ret);}else      // Задержка выключения насосов после выключения компрессора (сек).
   if(strcmp(var,option_DELAY_START_RES)==0)  {return _itoa(Option.delayStartRes,ret);}else     // Задержка включения ТН после внезапного сброса контроллера (сек.)
   if(strcmp(var,option_DELAY_REPEAD_START)==0){return _itoa(Option.delayRepeadStart,ret);}else // Задержка перед повторным включениме ТН при ошибке (попытки пуска) секунды
   if(strcmp(var,option_DELAY_DEFROST_ON)==0) {return _itoa(Option.delayDefrostOn,ret);}else    // ДЛЯ ВОЗДУШНОГО ТН Задержка после срабатывания датчика перед включением разморозки (секунды)
   if(strcmp(var,option_DELAY_DEFROST_OFF)==0){return _itoa(Option.delayDefrostOff,ret);}else   // ДЛЯ ВОЗДУШНОГО ТН Задержка перед выключением разморозки (секунды)
   if(strcmp(var,option_DELAY_TRV)==0)        {return _itoa(Option.delayTRV,ret);}else          // Задержка между переключением 4-х ходового клапана и включением компрессора, для выравнивания давлений (сек). Если включены эти опции (переключение тепло-холод)
   if(strcmp(var,option_DELAY_BOILER_SW)==0)  {return _itoa(Option.delayBoilerSW,ret);}else     // Пауза (сек) после переключение ГВС - выравниваем температуру в контуре отопления/ГВС что бы сразу защиты не сработали
   if(strcmp(var,option_DELAY_BOILER_OFF)==0) {return _itoa(Option.delayBoilerOff,ret);} // Время (сек) на сколько блокируются защиты при переходе с ГВС на отопление и охлаждение слишком горяче после ГВС
   return  strcat(ret,(char*)cInvalid);                
}


// Установить рабочий профиль по текущему Prof
void HeatPump::set_profile()
{
	Option.numProf = Prof.get_idProfile();
}

// --------------------------------------------------------------------
// ФУНКЦИИ РАБОТЫ С ГРАФИКАМИ ТН -----------------------------------
// --------------------------------------------------------------------
// обновить статистику, добавить одну точку и если надо записать ее на карту.
// Все значения в графиках целочислены (сотые), выводятся в формате 0.01
void  HeatPump::updateChart()
{
	uint8_t i;
	for(i=0;i<TNUMBER;i++) if(sTemp[i].Chart.get_present())  sTemp[i].Chart.addPoint(sTemp[i].get_Temp());
#ifndef MIN_RAM_CHARTS
	for(i=0;i<ANUMBER;i++)
#else
	for(i=PCON+1;i<ANUMBER;i++)
#endif
		if(sADC[i].Chart.get_present()) sADC[i].Chart.addPoint(sADC[i].get_Press());
	for(i=0;i<FNUMBER;i++) if(sFrequency[i].Chart.get_present()) sFrequency[i].Chart.addPoint(sFrequency[i].get_Value()); // Частотные датчики
#ifdef EEV_DEF
#ifdef EEV_PREFER_PERCENT
	if(dEEV.Chart.get_present())     dEEV.Chart.addPoint(dEEV.get_EEV_percent());
#else
	if(dEEV.Chart.get_present())     dEEV.Chart.addPoint(dEEV.get_EEV());
#endif
	if(ChartOVERHEAT.get_present())  ChartOVERHEAT.addPoint(dEEV.get_Overheat());
	if(ChartOVERHEAT2.get_present()) ChartOVERHEAT2.addPoint(GETBIT(dEEV.get_flags(), fEEV_DirectAlgorithm) ? dEEV.OverheatTCOMP : dEEV.get_tOverheat());
	if(ChartTPEVA.get_present())     ChartTPEVA.addPoint(PressToTemp(sADC[PEVA].get_Press(),dEEV.get_typeFreon()));
//	if (sADC[PCON].get_present())    // Если датчик высокого давления есть считаем честно
//    	{ if(ChartTPCON.get_present()) ChartTPCON.addPoint(PressToTemp(sADC[PCON].get_Press(),dEEV.get_typeFreon()));}
//	else 
//	    { if(ChartTPCON.get_present()) ChartTPCON.addPoint(sTemp[get_modeHouse()==pCOOL?TEVAOUTG:TCONOUTG].get_Temp() + 200);}
if(ChartTPCON.get_present()) ChartTPCON.addPoint(get_temp_condensing());
	
#endif

	if(dFC.ChartFC.get_present())       dFC.ChartFC.addPoint(dFC.get_frequency());       // факт
	if(dFC.ChartPower.get_present())    dFC.ChartPower.addPoint(dFC.get_power()/10);
#ifndef MIN_RAM_CHARTS
	if(dFC.ChartCurrent.get_present())  dFC.ChartCurrent.addPoint(dFC.get_current());
#endif
	if(ChartRCOMP.get_present())     ChartRCOMP.addPoint((int16_t)dRelay[RCOMP].get_Relay());

	if(ChartCOP.get_present())       ChartCOP.addPoint(COP);                     // в сотых долях !!!!!!
#ifdef USE_ELECTROMETER_SDM
#ifndef MIN_RAM_CHARTS
	if(dSDM.ChartVoltage.get_present())   dSDM.ChartVoltage.addPoint(dSDM.get_Voltage()*100);
	if(dSDM.ChartCurrent.get_present())   dSDM.ChartCurrent.addPoint(dSDM.get_Current()*100);
#endif
	if(dSDM.ChartPower.get_present())     dSDM.ChartPower.addPoint(power220);
	//  if(dSDM.ChartPowerFactor.get_present())   dSDM.ChartPowerFactor.addPoint(dSDM.get_PowerFactor()*100);
	if(ChartFullCOP.get_present())      ChartFullCOP.addPoint(fullCOP);  // в сотых долях !!!!!!
#endif
}

// сбросить графики в ОЗУ
void HeatPump::startChart()
{
 uint8_t i; 
 for(i=0;i<TNUMBER;i++) sTemp[i].Chart.clear();
#ifndef MIN_RAM_CHARTS
 for(i=0;i<ANUMBER;i++) sADC[i].Chart.clear();
#else
 for(i=PCON+1;i<ANUMBER;i++) sADC[i].Chart.clear();
#endif
 for(i=0;i<FNUMBER;i++) sFrequency[i].Chart.clear();
 #ifdef EEV_DEF
 dEEV.Chart.clear();
 ChartOVERHEAT.clear();
 ChartOVERHEAT2.clear();
 ChartTPEVA.clear(); 
 ChartTPCON.clear(); 
 #endif
 dFC.ChartFC.clear();
 dFC.ChartPower.clear();
#ifndef MIN_RAM_CHARTS
 dFC.ChartCurrent.clear();
#endif
 ChartRCOMP.clear();
// ChartRELAY.clear();
 ChartCOP.clear();                                     // Коэффициент преобразования
 #ifdef USE_ELECTROMETER_SDM 
#ifndef MIN_RAM_CHARTS
 dSDM.ChartVoltage.clear();                              // Статистика по напряжению
 dSDM.ChartCurrent.clear();                              // Статистика по току
#endif
// dSDM.sAcPower.clear();                              // Статистика по активная мощность
// dSDM.sRePower.clear();                              // Статистика по Реактивная мощность
 dSDM.ChartPower.clear();                                // Статистика по Полная мощность
// dSDM.ChartPowerFactor.clear();                          // Статистика по Коэффициент мощности
 ChartFullCOP.clear();                                     // Коэффициент преобразования
 #endif
// powerCO=0;
// powerGEO=0;
// power220=0;
}


// получить список доступных графиков в виде строки
// cat true - список добавляется в конец, false - строка обнуляется и список добавляется
char * HeatPump::get_listChart(char* str)
{
uint8_t i;  
 strcat(str,"none:1;");
 for(i=0;i<TNUMBER;i++) if(sTemp[i].Chart.get_present()) {strcat(str,sTemp[i].get_name()); strcat(str,":0;");}
#ifndef MIN_RAM_CHARTS
 for(i=0;i<ANUMBER;i++)
#else
 for(i=PCON+1;i<ANUMBER;i++)
#endif
	 if(sADC[i].Chart.get_present()) { strcat(str,sADC[i].get_name()); strcat(str,":0;");}
 for(i=0;i<FNUMBER;i++) if(sFrequency[i].Chart.get_present()) { strcat(str,sFrequency[i].get_name()); strcat(str,":0;");}
 #ifdef EEV_DEF
 if(dEEV.Chart.get_present())      { strcat(str, chart_posEEV); strcat(str,":0;"); }
 if(ChartOVERHEAT.get_present())   { strcat(str,chart_OVERHEAT); strcat(str,":0;"); }
 if(ChartOVERHEAT2.get_present())  { strcat(str,chart_OVERHEAT2); strcat(str,":0;"); }
#ifdef TCONOUT
 strcat(str, chart_OVERCOOL); strcat(str,":0;");
#endif
 if(ChartTPEVA.get_present())      { strcat(str,chart_TPEVA); strcat(str,":0;"); }
 if(ChartTPCON.get_present())      {
	if(sADC[PCON].get_present()) strcat(str,chart_TPCON); else strcat(str,chart_TCON);
	strcat(str,":0;");
    strcat(str,chart_TCOMP_TCON); strcat(str,":0;");
 }
 #endif
 if(dFC.ChartFC.get_present())     { strcat(str,chart_freqFC); strcat(str,":0;"); }
 if(dFC.ChartPower.get_present())  { strcat(str,chart_powerFC); strcat(str,":0;"); }
#ifndef MIN_RAM_CHARTS
 if(dFC.ChartCurrent.get_present()){ strcat(str,chart_currentFC); strcat(str,":0;"); }
#endif
 if(ChartRCOMP.get_present())      { strcat(str,chart_RCOMP); strcat(str,":0;"); }
 if((sTemp[TCONOUTG].Chart.get_present())&&(sTemp[TCONING].Chart.get_present())) { strcat(str,chart_dCO); strcat(str,":0;"); }
 if((sTemp[TEVAING].Chart.get_present())&&(sTemp[TEVAOUTG].Chart.get_present())) { strcat(str,chart_dGEO); strcat(str,":0;"); }
 #ifdef FLOWCON 
 if((sTemp[TCONOUTG].Chart.get_present())&&(sTemp[TCONING].Chart.get_present())) { strcat(str,chart_PowerCO); strcat(str,":0;"); }
 #endif
 #ifdef FLOWEVA
 if((sTemp[TEVAING].Chart.get_present())&&(sTemp[TEVAOUTG].Chart.get_present())) { strcat(str,chart_PowerGEO); strcat(str,":0;"); }
 #endif
 if(ChartCOP.get_present())        { strcat(str,chart_COP); strcat(str,":0;"); }
  #ifdef USE_ELECTROMETER_SDM 
#ifndef MIN_RAM_CHARTS
 if(dSDM.ChartVoltage.get_present()) {   strcat(str,chart_VOLTAGE); strcat(str,":0;"); }
 if(dSDM.ChartCurrent.get_present()) {    strcat(str,chart_CURRENT); strcat(str,":0;"); }
#endif
// if(dSDM.sAcPower.get_present())     strcat(str,"acPOWER:0;");
// if(dSDM.sRePower.get_present())     strcat(str,"rePOWER:0;");
 if(dSDM.ChartPower.get_present())   {    strcat(str,chart_fullPOWER); strcat(str,":0;"); }
// if(dSDM.ChartPowerFactor.get_present()) strcat(str,"kPOWER:0;");
 if(ChartFullCOP.get_present())      { strcat(str,chart_fullCOP); strcat(str,":0;"); }
 #endif
// for(i=0;i<RNUMBER;i++) if(dRelay[i].Chart.get_present()) { strcat(str,dRelay[i].get_name()); strcat(str,":0;");}  
return str;               
}

// получить данные графика  в виде строки, данные ДОБАВЛЯЮТСЯ к str
void HeatPump::get_Chart(char *var, char* str)
{
	uint8_t i;
	// В начале имена совпадающие с именами объектов
	for(i = 0; i < TNUMBER; i++) {
		if((strcmp(var, sTemp[i].get_name()) == 0) && (sTemp[i].Chart.get_present())) {
			sTemp[i].Chart.get_PointsStr(100, str);
			return;
		}
	}
#ifndef MIN_RAM_CHARTS
	for(i = 0; i < ANUMBER; i++) {
#else
	for(i = PCON + 1; i < ANUMBER; i++) {
#endif
		if((strcmp(var, sADC[i].get_name()) == 0) && (sADC[i].Chart.get_present())) {
			sADC[i].Chart.get_PointsStr(100, str);
			return;
		}
	}
	for(i = 0; i < FNUMBER; i++) {
		if((strcmp(var, sFrequency[i].get_name()) == 0) && (sFrequency[i].Chart.get_present())) {
			sFrequency[i].Chart.get_PointsStr(1000, str);
			return;
		}
	}
	if(strcmp(var, chart_NONE) == 0) {
		strcat(str, "");
#ifdef EEV_DEF
	} else if(strcmp(var, chart_posEEV) == 0) {
  #ifdef EEV_PREFER_PERCENT
		dEEV.Chart.get_PointsStr(100, str);
  #else
		dEEV.Chart.get_PointsStr(1, str);
  #endif
	} else if(strcmp(var, chart_OVERHEAT2) == 0) {
		ChartOVERHEAT2.get_PointsStr(100, str);
	} else if(strcmp(var, chart_OVERHEAT) == 0) {
		ChartOVERHEAT.get_PointsStr(100, str);
#ifdef TCONOUT
	} else if(strcmp(var, chart_OVERCOOL) == 0) {
		ChartTPCON.get_PointsStrSub(100, str, &sTemp[TCONOUT].Chart); // считаем график на лету
#endif
	} else if(strcmp(var, chart_TPEVA) == 0) {
		ChartTPEVA.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TPCON) == 0) {
		ChartTPCON.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TCON) == 0)  {
		ChartTPCON.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TCOMP_TCON) == 0) {  // График нагнетание - конденсация
		sTemp[TCOMP].Chart.get_PointsStrSub(100, str, &ChartTPCON); // считаем график на лету
#endif
	} else if(strcmp(var, chart_freqFC) == 0) {
		dFC.ChartFC.get_PointsStr(100, str);
	} else if(strcmp(var, chart_powerFC) == 0) {
		dFC.ChartPower.get_PointsStr(100, str);
#ifndef MIN_RAM_CHARTS
	} else if(strcmp(var, chart_currentFC) == 0) {
		dFC.ChartCurrent.get_PointsStr(100, str);
#endif
	} else if(strcmp(var, chart_RCOMP) == 0) {
		ChartRCOMP.get_PointsStr(1, str);
	} else if(strcmp(var, chart_dCO) == 0) {
		sTemp[TCONOUTG].Chart.get_PointsStrSub(100, str, &sTemp[TCONING].Chart); // считаем график на лету экономим оперативку
	} else if(strcmp(var, chart_dGEO) == 0) {
		sTemp[TEVAING].Chart.get_PointsStrSub(100, str, &sTemp[TEVAOUTG].Chart); // считаем график на лету экономим оперативку
	} else if(strcmp(var, chart_PowerCO) == 0) {
#ifdef FLOWCON
		sFrequency[FLOWCON].Chart.get_PointsStrPower(1000, str, &sTemp[TCONING].Chart, &sTemp[TCONOUTG].Chart, sFrequency[FLOWCON].get_kfCapacity()); // считаем график на лету экономим оперативку
#else
		strcat(str, ";");
#endif
	} else if(strcmp(var, chart_PowerGEO) == 0) {
#ifdef FLOWEVA
		sFrequency[FLOWEVA].Chart.get_PointsStrPower(1000, str, &sTemp[TEVAING].Chart, &sTemp[TEVAOUTG].Chart, sFrequency[FLOWEVA].get_kfCapacity()); // считаем график на лету экономим оперативку
#else
		strcat(str, ";");
#endif
	} else if(strcmp(var, chart_COP) == 0) {
		ChartCOP.get_PointsStr(100, str);
#ifdef USE_ELECTROMETER_SDM
#ifndef MIN_RAM_CHARTS
	} else if(strcmp(var, chart_VOLTAGE) == 0) {
		dSDM.ChartVoltage.get_PointsStr(100, str);
	} else if(strcmp(var, chart_CURRENT) == 0) {
		dSDM.ChartCurrent.get_PointsStr(100, str);
#endif
	} else if(strcmp(var, chart_fullPOWER) == 0) {
		dSDM.ChartPower.get_PointsStr(1, str);
	} else if(strcmp(var, chart_fullCOP) == 0) {
		ChartFullCOP.get_PointsStr(100, str);
#endif
	}
}

// расчитать хеш для пользователя возвращает длину хеша
uint8_t HeatPump::set_hashUser()
{
	char buf[20];
	strcpy(buf, NAME_USER);
	strcat(buf, ":");
	strcat(buf, Network.passUser);
	base64_encode(Security.hashUser, buf, strlen(buf));
	Security.hashUserLen = strlen(Security.hashUser);
	journal.jprintf(" Hash user: %s\n", Security.hashUser);
	return Security.hashUserLen;
}
// расчитать хеш для администратора возвращает длину хеша
uint8_t HeatPump::set_hashAdmin()
{
	char buf[20];
	strcpy(buf, NAME_ADMIN);
	strcat(buf, ":");
	strcat(buf, Network.passAdmin);
	base64_encode(Security.hashAdmin, buf, strlen(buf));
	Security.hashAdminLen = strlen(Security.hashAdmin);
	journal.jprintf(" Hash admin: %s\n", Security.hashAdmin);
	return Security.hashAdminLen;
}

// Обновить настройки дисплея Nextion
void HeatPump::updateNextion()
{
#ifdef NEXTION
	if(GETBIT(Option.flags, fNextion))  // Дисплей подключен
	{
		myNextion.init_display();
		myNextion.set_need_refresh();
	} else                        // Дисплей выключен
	{
//		myNextion.sendCommand("sleep=1");
	}
#endif
}

// Переключение на следующий режим работы отопления (последовательный перебор режимов)
void HeatPump::set_nextMode()
{
   switch ((MODE_HP)get_modeHouse() )
    {
      case  pOFF:   Prof.SaveON.mode=pHEAT;  break;
      case  pHEAT:  Prof.SaveON.mode=pCOOL;  break;
      case  pCOOL:  Prof.SaveON.mode=pOFF;   break;
      default: break;
    }
}

// Изменить целевую температуру с провекой допустимости значений
// Параметр само ИЗМЕНЕНИЕ температуры
int16_t HeatPump::setTargetTemp(int16_t dt)
{
  switch ((MODE_HP)get_modeHouse() )   // проверка для режима ДОМА
  {
  case  pOFF:
	  return 0;
      break;
  case  pHEAT:
      if (get_ruleHeat()==pHYBRID) {if((Prof.Heat.Temp1+dt>=0.0*100)&&(Prof.Heat.Temp1+dt<=30.0*100)) Prof.Heat.Temp1=Prof.Heat.Temp1+dt; return Prof.Heat.Temp1;}
      if(!(GETBIT(Prof.Heat.flags,fTarget))) { if((Prof.Heat.Temp1+dt>=0.0*100)&&(Prof.Heat.Temp1+dt<=30.0*100)) Prof.Heat.Temp1=Prof.Heat.Temp1+dt; return Prof.Heat.Temp1;}
      else  { if((Prof.Heat.Temp2+dt>=10.0*100)&&(Prof.Heat.Temp2+dt<=50.0*100)) Prof.Heat.Temp2=Prof.Heat.Temp2+dt; return Prof.Heat.Temp2; }
      break;
  case  pCOOL:
      if (get_ruleCool()==pHYBRID) {if((Prof.Cool.Temp1+dt>=0.0*100)&&(Prof.Cool.Temp1+dt<=30.0*100)) Prof.Cool.Temp1=Prof.Cool.Temp1+dt; return Prof.Cool.Temp1;}
      if(!(GETBIT(Prof.Cool.flags,fTarget))) {if((Prof.Cool.Temp1+dt>=0.0*100)&&(Prof.Cool.Temp1+dt<=30.0*100)) Prof.Cool.Temp1=Prof.Cool.Temp1+dt; return Prof.Cool.Temp1;}
      else  { if((Prof.Cool.Temp2+dt>=0.0*100)&&(Prof.Cool.Temp2+dt<=30.0*100)) Prof.Cool.Temp2=Prof.Cool.Temp2+dt; return Prof.Cool.Temp2; }
      break;
  default: break;
  }
  return 0;
}

int16_t HeatPump::get_targetTempCool()
{
	int16_t T;
	if(get_ruleCool() == pHYBRID) T = Prof.Cool.Temp1;
	else if(!(GETBIT(Prof.Cool.flags, fTarget))) T = Prof.Cool.Temp1;
	else T = Prof.Cool.Temp2;
	T += Schdlr.get_temp_change();
	return T;
}

int16_t HeatPump::get_targetTempHeat()
{
	int16_t T;
	if(get_ruleHeat() == pHYBRID) T = Prof.Heat.Temp1;
	else if(!(GETBIT(Prof.Heat.flags, fTarget))) T = Prof.Heat.Temp1;
	else T = Prof.Heat.Temp2;
	if(Prof.Heat.add_delta_temp != 0) {
		int8_t h = rtcSAM3X8.get_hours();
		if((Prof.Heat.add_delta_end_hour >= Prof.Heat.add_delta_hour && h >= Prof.Heat.add_delta_hour && h <= Prof.Heat.add_delta_end_hour)
			|| (Prof.Heat.add_delta_end_hour < Prof.Heat.add_delta_hour && (h >= Prof.Heat.add_delta_hour || h <= Prof.Heat.add_delta_end_hour)))
			T += Prof.Heat.add_delta_temp;
	}
	T += Schdlr.get_temp_change();
	return T;
}

// ИЗМЕНИТЬ целевую температуру бойлера с провекой допустимости значений
int16_t HeatPump::setTempTargetBoiler(int16_t dt)
{
  if ((Prof.Boiler.TempTarget+dt>=5.0*100)&&(Prof.Boiler.TempTarget+dt<=90.0*100))   Prof.Boiler.TempTarget=Prof.Boiler.TempTarget+dt;
  return Prof.Boiler.TempTarget;     
}

// Получить целевую температуру бойлера с учетом корректировки
int16_t HeatPump::get_boilerTempTarget()
{
	 if(Prof.Boiler.add_delta_temp != 0) {
		int8_t h = rtcSAM3X8.get_hours();
		if((Prof.Boiler.add_delta_end_hour >= Prof.Boiler.add_delta_hour && h >= Prof.Boiler.add_delta_hour && h <= Prof.Boiler.add_delta_end_hour)
			|| (Prof.Boiler.add_delta_end_hour < Prof.Boiler.add_delta_hour && (h >= Prof.Boiler.add_delta_hour || h <= Prof.Boiler.add_delta_end_hour)))
			return Prof.Boiler.TempTarget + Prof.Boiler.add_delta_temp;
	 }
	 return Prof.Boiler.TempTarget;
}

// Получить целевую температуру отопления
void HeatPump::getTargetTempStr(char *rstr)
{
	switch(HP.get_modeHouse())   // проверка отопления
	{
	case pHEAT:
		ftoa(rstr, (float) HP.get_targetTempHeat() / 100, 1);
		break;
	case pCOOL:
		ftoa(rstr, (float) HP.get_targetTempCool() / 100, 1);
		break;
	default:
		strcpy(rstr, "-.-");
	}
}

// --------------------------------------------------------------------------------------------------------
// ---------------------------------- ОСНОВНЫЕ ФУНКЦИИ РАБОТЫ ТН ------------------------------------------
// --------------------------------------------------------------------------------------------------------

 #ifdef RBOILER  // управление дополнительным ТЭНом бойлера
 // Проверка на необходимость греть бойлер дополнительным теном (true - надо греть) ВСЕ РЕЖИМЫ
 boolean HeatPump::boilerAddHeat()
 {
	 int16_t T = sTemp[TBOILER].get_Temp();
#ifdef RBOILER 	// нужно т.к. гистерезис определяется по реле
	 if ((GETBIT(Prof.SaveON.flags,fBoilerON))&&(GETBIT(Prof.Boiler.flags,fSalmonella))) // Сальмонелла не взирая на расписание если включен бойлер
	 {
		 if((rtcSAM3X8.get_day_of_week()==SALLMONELA_DAY)&&(rtcSAM3X8.get_hours()==SALLMONELA_HOUR)&&(rtcSAM3X8.get_minutes()<=2)&&(!onSallmonela)) {  // Надо начитать процесс обеззараживания
		 	 startSallmonela=rtcSAM3X8.unixtime(); 
		 	 onSallmonela=true; 
		 	 journal.jprintf(" Cycle start salmonella\n"); 
		 }
		 if (onSallmonela) {   // Обеззараживание нужно
			 if (startSallmonela+SALLMONELA_TIME>rtcSAM3X8.unixtime()) { // Время цикла еще не исчерпано
				 if (T < SALLMONELA_TEMP)  return true;// Включить обеззараживание
				 #ifdef SALLMONELA_HARD 
			    	 else if (T > SALLMONELA_TEMP+50) return false; else return dRelay[RBOILER].get_Relay();// Вариант работы - Стабилизация температуры обеззараживания, гистерезис 0.5 градуса
				 #else
					 else {  // Вариант работы только до достижение темпеартуы и сразу выключение
					 onSallmonela=false;
					 startSallmonela=0;
					 journal.jprintf(" Cycle end salmonella\n");	
					 return false;
					 }	
				 #endif 
			 } else {  // Время вышло, выключаем, и идем дальше по алгоритму
				 onSallmonela=false;
				 startSallmonela=0;
				 journal.jprintf(" Cycle end salmonella\n");
			 }
		 }
	 } else  if (onSallmonela)  { onSallmonela=false;  startSallmonela=0;  journal.jprintf(" Off salmonella\n");  } // если сальмонелу отключили на ходу выключаем и идем дальше по алгоритму
#endif	 

	 if(GETBIT(Prof.SaveON.flags, fBoilerON) && scheduleBoiler()) // Если разрешено греть бойлер согласно расписания И Бойлер включен
	 {
		 if(GETBIT(Prof.Boiler.flags,fTurboBoiler))  // Если турбо режим то повторяем за Тепловым насосом (грет или не греть)
		 {
             if(T < Prof.Boiler.tempRBOILER) return onBoiler;   // работа параллельно с ТН  если температура МЕНЬШЕ догрева то повторяем работу ТН
//		  else false;                                                              // Турбо отключаем
		 }
//		 else // Нет турбо
//		 {
			 if(GETBIT(Prof.Boiler.flags,fAddHeating))  // Включен догрев
			 {
				 if((T < get_boilerTempTarget()-Prof.Boiler.dTemp)&&(!flagRBOILER)) {  // Бойлер ниже гистерезиса - ставим признак необходимости включения Догрева (но пока не включаем ТЭН)
					 flagRBOILER = true;
					 return false;
				 }
				 if((!flagRBOILER)||(onBoiler))  return false; // флажка нет или работет бойлер но догрев не включаем
				 else  //flagRBOILER==true and onBoiler==false
				 {
					 if(T < get_boilerTempTarget())                       // Бойлер ниже целевой температуры надо греть
					 {
						 //if(T < Prof.Boiler.tempRBOILER-HYSTERESIS_RBOILER) {flagRBOILER=false; return false;}   // температура ниже включения догрева выключаем и сбрасывам флаг необходимости
						 //else
						 return true; // продолжаем греть бойлер
					 }
					 else {flagRBOILER=false; return false;}               // бойлер выше целевой темпеартуы - цель достигнута - догрев выключаем
				 }
			 }  // догрев
			 else {flagRBOILER=false; return false;}                    // ТЭН не используется (сняты все флажки)
//		 } // Нет турбо
	 }
	 else {flagRBOILER=false; return false;}                            // Бойлер сейчас запрещен

 }
#endif   

// Проверить расписание бойлера true - нужно греть false - греть не надо, если расписание выключено то возвращает true
 boolean HeatPump::scheduleBoiler()
 {
	 if(GETBIT(Prof.Boiler.flags, fSchedule))         // Если используется расписание
	 {  // Понедельник 0 воскресенье 6 это кодирование в расписании функция get_day_of_week возвращает 1-7
		 boolean b = Prof.Boiler.Schedule[rtcSAM3X8.get_day_of_week() - 1] & (0x01 << rtcSAM3X8.get_hours()) ? true : false;
		 if(!b) return false;             // запрещено греть бойлер согласно расписания
	 }
	 return true;
 }
// Все реле выключить
void HeatPump::relayAllOFF()
{
  uint8_t i;
  journal.jprintf(" All relay off\n");
  for(i=0;i<RNUMBER;i++)  dRelay[i].set_OFF();         // Выключить все реле;
  onBoiler=false;                                     // выключить признак нагрева бойлера
}                               
// Поставить 4х ходовой в нужное положение для работы в заваисимости от Prof.SaveON.mode
// функция сама определяет что делать в зависимости от режима
// параметр задержка после включения мсек.
#ifdef RTRV    // Если четырехходовой есть в конфигурации
void HeatPump::set_RTRV(uint16_t d)
{
	//      if (Prof.SaveON.mode==pHEAT)        // Реле переключения четырех ходового крана (переделано для инвертора). Для ОТОПЛЕНИЯ надо выключить,  на ОХЛАЖДЕНИЯ включить конечное устройство
	if(get_modeHouse() == pHEAT)         // Реле переключения четырех ходового крана (переделано для инвертора). Для ОТОПЛЕНИЯ надо выключить,  на ОХЛАЖДЕНИЯ включить конечное устройство
	{
		dRelay[RTRV].set_OFF();         // отопление
	} else   // во всех остальных случаях
	{
		dRelay[RTRV].set_ON();          // охлаждение
	}
	_delay(d);                            // Задержка на 2 сек
}
#endif

// Конфигурирование насосов ТН, вход желаемое состояние(true-бойлер false-отопление/охлаждение) возврат onBoiler
// в зависимости от режима, не забываем менять onBoiler по нему определяется включение ГВС
boolean HeatPump::switchBoiler(boolean b)
{
	if(b && b == onBoiler) return onBoiler;        // Нечего делать выходим
	boolean old = onBoiler;
	onBoiler = b;                                  // запомнить состояние нагрева бойлера ВСЕГДА
	if(!onBoiler) offBoiler = rtcSAM3X8.unixtime(); // запомнить время выключения ГВС (нужно для переключения)
	else offBoiler = 0;
#ifdef R3WAY
	dRelay[R3WAY].set_Relay(onBoiler);            // Установить в нужное полежение 3-х ходового
#else // Нет трехходового - схема с двумя насосами
	// ставим сюда код переключения ГВС/отопление в зависимости от onBoiler=true - ГВС
	//journal.printf(" swBoiler(%d): old:%d, modeHouse:%d\n", b, get_modWork(), get_modeHouse());
	if(onBoiler) { // переключение на ГВС
#ifdef RPUMPBH
		dRelay[RPUMPBH].set_ON();    // ГВС - включить
#endif
		Pump_HeatFloor(false);		 // выключить насос ТП
		dRelay[RPUMPO].set_OFF();    // файнкойлы выключить
	} else { // Переключение с ГВС на Отопление/охлаждение идет анализ по режиму работы дома
#ifdef RPUMPBH
		if(!GETBIT(flags, fHP_BoilerTogetherHeat)) dRelay[RPUMPBH].set_OFF();    // ГВС надо выключить
#endif
		if((Status.modWork != pOFF) && (get_modeHouse() != pOFF) && (get_State() != pSTOPING_HP)) { // Если не пауза И отопление/охлаждение дома НЕ выключено И нет процесса выключения ТН то надо включаться
			dRelay[RPUMPO].set_ON();     // файнкойлы
			Pump_HeatFloor(true);
		} else { // пауза ИЛИ работа дома не задействована - все выключить
			Pump_HeatFloor(false);
			dRelay[RPUMPO].set_OFF();     // файнкойлы
		}
	}
#endif
	if(old && get_State() == pWORK_HP) { // Если грели бойлер и теперь ТН работает, то обеспечить дополнительное время (delayBoilerSW сек) для прокачивания гликоля - т.к разные уставки по температуре подачи
		journal.jprintf(" Pause %d sec, Boiler->House . . .\n", HP.Option.delayBoilerSW);
		_delay(HP.Option.delayBoilerSW * 1000); // выравниваем температуру в контуре отопления/ГВС что бы сразу защиты не сработали
	}
	return onBoiler;
}
// Проверка и если надо включение EVI если надо то выключение возвращает состояние реле
// Если реле нет то ничего не делает возвращает false
// Проверяет паузу 60 секунд после включения компрессора, и только после этого начинает управлять EVI
#ifdef REVI
boolean HeatPump::checkEVI()
{
  if (!dRelay[REVI].get_present())  return false;                                                                            // Реле отсутвует в конфигурации ничего не делаем
  if (!is_compressor_on())     {dRelay[REVI].set_OFF(); return dRelay[REVI].get_Relay();}                                    // Компрессор выключен и реле включено - выключить реле

  // компрессор работает
  if (rtcSAM3X8.unixtime()-startCompressor<60) return dRelay[REVI].get_Relay() ;                                            // Компрессор работает меньше одной минуты еще рано

  // проверяем условия для включения ЭВИ
  if((sTemp[TCONOUTG].get_Temp()>EVI_TEMP_CON)||(sTemp[TEVAOUTG].get_Temp()<EVI_TEMP_EVA)) { dRelay[REVI].set_ON(); return dRelay[REVI].get_Relay();}  // условия выполнены включить ЭВИ
  else  {dRelay[REVI].set_OFF(); return dRelay[REVI].get_Relay();}                                                          // выключить ЭВИ - условий нет
  return dRelay[REVI].get_Relay();
}
#endif

void HeatPump::Pump_HeatFloor(boolean On)
{
#ifdef RPUMPFL
	if(On) {
		if((get_modeHouse() == pHEAT && GETBIT(Prof.Heat.flags, fHeatFloor)) || (get_modeHouse() == pCOOL && GETBIT(Prof.Cool.flags, fHeatFloor)))
			dRelay[RPUMPFL].set_ON();
	} else dRelay[RPUMPFL].set_OFF();
#endif
}

// Включить или выключить насосы первый параметр их желаемое состояние
// Второй параметр параметр задержка после включения/выключения мсек. отдельного насоса (борьба с помехами)
// Идет проверка на необходимость изменения состояния насосов
// Генерятся задержки для защиты компрессора, есть задержки между включенимями насосов для уменьшения помех
void HeatPump::Pumps(boolean b, uint16_t d)
{
#ifdef DEBUG_MODWORK
	journal.printf(" Pumps(%d), modWork: %d\n", b, get_modWork());
#endif

#ifdef DELAY_BEFORE_STOP_IN_PUMP               // Задержка перед выключением насоса геоконтура, насос отопления отключается позже (сек)
	if((!b) && (b!=dRelay[PUMP_IN].get_Relay())) {
		journal.jprintf(" Delay: stop IN pump.\n");
		_delay(DELAY_BEFORE_STOP_IN_PUMP * 1000); // задержка перед выключениме гео насоса после выключения компрессора (облегчение останова)
	}
	
	dRelay[PUMP_IN].set_Relay(b);             // Реле включения насоса входного контура  (геоконтур)
	_delay(d);                                // Задержка на d мсек
	
	if((!b) &&  (dRelay[RPUMPO].get_Relay() // пауза перед выключением насосов контуров, если нужно
#ifdef RPUMPBH
						|| dRelay[RPUMPBH].get_Relay()
#endif
	)){ // Насосы выключены и будут выключены, нужна пауза идет останов компрессора (новое значение выкл  старое значение вкл)
		journal.jprintf(" Delay: stop OUT pump.\n");
		_delay(Option.delayOffPump * 1000); // задержка перед выключениме насосов после выключения компрессора (облегчение останова)
	}
#else
	// пауза перед выключением насосов контуров, если нужно
	if(!b)  // Насосы выключены и будут выключены, нужна пауза идет останов компрессора (новое значение выкл  старое значение вкл)
	{
		journal.jprintf(" Pause before stop pumps %d sec . . .\n",Option.delayOffPump);
		_delay(Option.delayOffPump * 1000); // задержка перед выключениме насосов после выключения компрессора (облегчение останова)
	}
	// переключение насосов если есть что переключать (проверка была выше)
	dRelay[PUMP_IN].set_Relay(b);                   // Реле включения насоса входного контура  (геоконтур)
	_delay(d);                                      // Задержка на d мсек
#endif
#ifdef  TWO_PUMP_IN                                                // второй насос для воздушника если есть
	if (!b) dRelay[PUMP_IN1].set_OFF();    // если насососы выключаем то второй вентилятор ВСЕГДА выключается!!
	else// а если насос включается то смотрим на условия
	{
		if(sTemp[TEVAOUT].get_Temp()<2500) {dRelay[PUMP_IN1].set_ON();} // Реле включения второго насоса входного контура для  воздушника
		else {dRelay[PUMP_IN1].set_OFF();}
	}
	_delay(d);                                 // Задержка на d мсек
#endif
#ifdef R3WAY
	dRelay[PUMP_OUT].set_Relay(b);                 // Реле включения насоса выходного контура  (отопление и ГВС)
	_delay(d);                                     // Задержка на d мсек
#else
	// здесь выключать насос бойлера бесполезно, уже раньше выключили
	#ifdef RPUMPBH
    if((get_modWork()==pBOILER)||(get_modWork()==pNONE_B)) dRelay[RPUMPBH].set_Relay(b); // Если бойлер
    else
    #endif
    {
    	dRelay[RPUMPO].set_Relay(b);                  // насос отопления
    	if(!b) {
#ifdef RPUMPBH
    		if(!b) SETBIT0(HP.flags, fHP_BoilerTogetherHeat);
    		dRelay[RPUMPBH].set_Relay(b);
#endif
#ifdef RPUMPFL
    		// if(get_modWork() == pHEAT || get_modWork() == pNONE_H) // Выкл или Отопление
    		Pump_HeatFloor(b); 				  // насос ТП
#endif
    	}
    }
   _delay(d);                                     // Задержка на d мсек    
#endif
	//  }
   // Пауза Option.delayOnPump перенесена перед вызовом COMPRESSOR_ON
   //if((b) && (!old))   // Насосы включены (старт компрессора), нужна пауза
}
// Сброс инвертора если он стоит в ошибке, проверяется наличие инвертора и проверка ошибки
// Проводится различный сброс в зависимсти от конфигурации
int8_t HeatPump::ResetFC()
{
	if(!dFC.get_present()) return OK;                                                 // Инвертора нет выходим

#ifdef FC_USE_RCOMP                                                               // ЕСЛИ есть провод сброса
#ifdef SERRFC                                                               // Если есть вход от инвертора об ошибке
	if (sInput[SERRFC].get_lastErr()==OK) return OK;                          // Инвертор сбрасывать не надо
	dRelay[RRESET].set_ON(); _delay(100); dRelay[RRESET].set_OFF();// Подать импульс сброса
	journal.jprintf("Reset %s use RRESET\r\n",dFC.get_name());
	_delay(100);
	sInput[SERRFC].Read();
	if (sInput[SERRFC].get_lastErr()==OK) return OK;// Инвертор сброшен
	else return ERR_RESET_FC;// Сброс не прошел
#else                                                                      // нет провода сброса - сбрасываем по модбас
	if(dFC.get_err() != OK)                                                       // инвертор есть ошибки
	{
		dFC.reset_FC();                                                       // подать команду на сброс по модбас
		if(!dFC.get_blockFC()) return OK;                                   // Инвертор НЕ блокирован
		else return ERR_RESET_FC;                                             // Сброс не удачный
	} else return OK;                                                         // Ошибок нет сбрасывать нечего
#endif // SERRFC
#else   //  #ifdef FC_USE_RCOMP
	if (dFC.get_err()!=OK)                                                           //  инвертор есть ошибки
	{
		dFC.reset_FC();                                                               // подать команду на сброс по модбас
		if (!dFC.get_blockFC()) return OK;// Инвертор НЕ блокирован
		else return ERR_RESET_FC;// Сброс не удачный
	}
	else return OK;                                                                  // Ошибок нет сбрасывать нечего
#endif
	return OK;
}
           
// проверить, Если есть ли работа для ТН - true.
boolean HeatPump::CheckAvailableWork()
{
	return Prof.SaveON.mode != pOFF || GETBIT(Prof.SaveON.flags, fBoilerON) || GETBIT(Option.flags, fSunRegenerateGeo) || HP.Schdlr.IsShedulerOn();
}

// START/RESUME -----------------------------------------
// Функция Запуска/Продолжения работы ТН - возвращает ок или код ошибки
// Запускается ВСЕГДА отдельной задачей с приоритетом выше вебсервера
// Параметр задает что делаем true-старт, false-возобновление
int8_t HeatPump::StartResume(boolean start)
{
	volatile MODE_HP mod;

	// Дана команда старт - но возможно надо переходить в ожидание
	// Определяем что делать
	int8_t profile = HP.Schdlr.calc_active_profile();
	if((profile != SCHDLR_NotActive)&&(start)) { // расписание активно и дана команда
		if(profile == SCHDLR_Profile_off)
		{
			journal.jprintf(" Start task UpdateHP\n");
			journal.jprintf(pP_TIME,"%s WAIT . . .\n",(char*)nameHeatPump);
			startWait=true;                    // Начало работы с ожидания=true;
			setState(pWAIT_HP);
			Task_vUpdate_run = true;
			vTaskResume(xHandleUpdate);
			return error;
		} else if(profile != HP.Prof.get_idProfile()) {
			HP.Prof.load(profile);
			HP.set_profile();
			journal.jprintf("Profile changed to #%d\n", profile);
		}
	}
	if (startWait)
	{
		startWait=false;
		start=true;   // Делаем полный запуск, т.к. в положение wait переходили из стопа (расписания)
	}

	// 1. Переменные  установка, остановка ТН имеет более высокий приоритет чем пуск ! -------------------------
	if (start)  // Команда старт
	{
		if ((get_State()==pWORK_HP)||(get_State()==pSTOPING_HP)||(get_State()==pSTARTING_HP)) return error; // Если ТН включен или уже стартует или идет процесс остановки то ничего не делаем (исключается многократный заход в функцию)
		journal.jprintf(pP_DATE,"  Start . . .\n");
		eraseError();                                      // Обнулить ошибку
		//lastEEV=-1;                                      // -1 это признак того что слежение eev еще не рабоатет (выключения компрессора  не было)
	}
	else
	{
		if (get_State()!=pWAIT_HP) return error; // Если состяние НЕ РАВНО ожиданию то ничего не делаем, выходим восстанавливать нечего
		journal.jprintf(pP_DATE,"  Resume . . .\n");
	}

	setState(pSTARTING_HP);                              // Производится старт -  устанавливаем соответсвующее состояние
	Status.ret=pNone;                                    // Состояние алгоритма
	lastEEV=-1;                                          // -1 это признак того что слежение eev еще не рабоатет (выключения компрессора  небыло)

	if (startPump)                                      // Если задача не остановлена то остановить (0 - останов задачи, 1 - запуск, 2 - в работе (выкл), 3 - в работе (вкл))
	{
		startPump=false;                                     // Поставить признак останова задачи насос
		journal.jprintf(" WARNING! %s: Bad startPump, OFF . . .\n",(char*)__FUNCTION__);
	}

	offBoiler=0;                                         // Бойлер никогда не выключался
	onSallmonela=false;                                  // Если true то идет Обеззараживание
	onBoiler=false;                                      // Если true то идет нагрев бойлера
	SETBIT0(flags, fHP_BoilerTogetherHeat);

	// 2.1 Проверка конфигурации, которые можно поменять из морды, по этому проверяем всегда ----------------------------------------
	if(!CheckAvailableWork())   // Нет работы для ТН - ничего не включено
	{
		setState(pOFF_HP);  // Еще ничего не сделали по этому сразу ставим состоение выключено
		set_Error(ERR_NO_WORK, (char*)__FUNCTION__);
		return error;
	}

#ifdef EEV_DEF
	if ((!sADC[PEVA].get_present())&&(dEEV.get_ruleEEV()==TEVAOUT_PEVA))  //  Отсутвует датчик давления, и выбран алгоритм ЭРВ который его использует",
	{
		setState(pOFF_HP);    // Еще ничего не сделали по этому сразу ставим состоение выключено
	    set_Error(ERR_PEVA_EEV,(char*)__FUNCTION__);        // остановить по ошибке;
		return error;
	}
#endif

	// 2.2 Проверка конфигурации, которые определены конфигом (поменять нельзя), по этому проверяем один раз при страте ТН ----------------------------------------
	if (start)  // Команда старт
	{
		if (!dRelay[PUMP_OUT].get_present())  // отсутсвует насос на конденсаторе, пользователь НЕ может изменить в процессе работы проверка при старте
		{
			setState(pOFF_HP);    // Еще ничего не сделали по этому сразу ставим состоение выключено
			set_Error(ERR_PUMP_CON,(char*)__FUNCTION__);        // остановить по ошибке;
			return error;
		}
		if (!dRelay[PUMP_IN].get_present())   // отсутсвует насос на испарителе, пользователь может изменить в процессе работы
		{
			setState(pOFF_HP);    // Еще ничего не сделали по этому сразу ставим состоение выключено
			set_Error(ERR_PUMP_EVA,(char*)__FUNCTION__);        // остановить по ошибке;
			return error;
		}
		if ((!dRelay[RCOMP].get_present())&&(!dFC.get_present()))   // отсутсвует компрессор, пользователь может изменить в процессе работы
		{
			setState(pOFF_HP);    // Еще ничего не сделали по этому сразу ставим состоение выключено
			set_Error(ERR_NO_COMPRESS,(char*)__FUNCTION__);        // остановить по ошибке;
			return error;
		}
	} //  if (start)  // Команда старт

	// 3.  ПОДГОТОВКА ------------------------------------------------------------------------

	if (start)  // Команда старт - Инициализация ЭРВ и очистка графиков при восстановлени не нужны
	{
#ifdef EEV_DEF
		//journal.jprintf(" EEV init\n");
		if (get_State()!=pSTARTING_HP) return error;            // Могли нажать кнопку стоп, выход из процесса запуска
		else  dEEV.Start();                                     // Включить ЭРВ  найти 0 по завершению позиция 0!!!
#endif

	#ifdef CLEAR_CHART_HP_ON
		journal.jprintf(" Charts clear and start\n");
		if (get_State()!=pSTARTING_HP) return error;            // Могли нажать кнопку стоп, выход из процесса запуска
		else  startChart();                                     // Запустить графики <- тут не запуск, тут очистка
	#endif	
		
	}
	
	// 4. Определяем что нужно делать -----------------------------------------------------------
	if (get_State()!=pSTARTING_HP) return error;            // Могли нажать кнопку стоп, выход из процесса запуска
	else  mod=get_Work();                                   // определяем что делаем с компрессором
	if (mod>pBOILER) mod=pOFF;                              // При первом пуске могут быть только состояния pOFF,pHEAT,pCOOL,pBOILER
	journal.jprintf(" Start modWork:%d[%s]\n",(int)mod,codeRet[Status.ret]);
	Status.modWork = mod;  // Установка режима!

  //  set_Error(ERR_PEVA_EEV,(char*)__FUNCTION__);        // остановить по ошибке для проверки EEV

	// 5. Конфигурируем ТН -----------------------------------------------------------------------
	if (get_State()!=pSTARTING_HP) return error;            // Могли нажать кнопку стоп, выход из процесса запуска

   	// 6. Если не старт ТН то проверка на минимальную паузу между включениями, при включении ТН паузе не будет -----------------
	if (!start)  // Команда Resume
	      while(check_compressor_pause()) { _delay(100*1000); if (get_State()!=pSTARTING_HP) return error;    }    // Могли нажать кнопку стоп, выход из процесса запуска
	
	//  7. Конфигурируем 3 и 4-х клапаны и включаем насосы ПАУЗА после включения насосов
	configHP(Status.modWork);  

	// 9. Включение компрессора и запуск обновления EEV -----------------------------------------------------
	if (get_State()!=pSTARTING_HP) return error;            // Могли нажать кнопку стоп, выход из процесса запуска
	if(is_next_command_stop()) return error;			    // следующая команда останов, выходим
#ifdef USE_ELECTROMETER_SDM
	if(!dSDM.get_link()) dSDM.uplinkSDM();
#endif

	if (get_errcode()!=OK)                                 // ОШИБКА перед стартом
	{
		journal.jprintf(" Error before start compressor!\n");
		set_Error(ERR_COMP_ERR,(char*)__FUNCTION__);
		return error;
	}
	if ((mod==pCOOL)||(mod==pHEAT)||(mod==pBOILER)) {
#ifndef DEMO   // проверка блокировки инвертора
		if((dFC.get_present())&&(dFC.get_blockFC()))          // есть инвертор но он блокирован
		{
			if(dFC.get_err() != ERR_485_BUZY) {
				journal.jprintf("%s: is blocked, ignore start\n",dFC.get_name());
				set_Error(ERR_MODBUS_BLOCK,(char*)__FUNCTION__);// ВОТ ЗДЕСЬ КОМАНДА СТОП И ПРОЙДЕТ
				return error;
			}
		}
#endif
		if ((ResetFC())!=OK)                         // Сброс инвертора если нужно
		{
			set_Error(ERR_RESET_FC,(char*)__FUNCTION__);
			return error;
		}
		compressorON(); // Компрессор включить если нет ошибок и надо включаться
	}

	// 10. Сохранение состояния  -------------------------------------------------------------------------------
	if (get_State()!=pSTARTING_HP) return error;                   // Могли нажать кнопку стоп, выход из процесса запуска
	setState(pWORK_HP);

	// 11. Запуск задачи обновления ТН ---------------------------------------------------------------------------
	if(start)
	{
		journal.jprintf(" Start task UpdateHP\n");
		Task_vUpdate_run = true;
		vTaskResume(xHandleUpdate);                                       // Запустить задачу Обновления ТН, дальше она все доделает
		//_delay(1);
	}

	// 12. насос запущен -----------------------------------------------------------------------------------------
	journal.jprintf(pP_TIME,"%s ON . . .\n",(char*)nameHeatPump);
	
	return error;
}

// Инициализировать переменные ПИД регулятора
void HeatPump::resetPID()
{
#ifdef PID_FORMULA2
	pidw.PropOnMeasure = DEF_FC_PID_P_ON_M;
	pidw.pre_err = (Status.modWork == pHEAT ? Prof.Heat.tempPID : Status.modWork == pBOILER ? Prof.Boiler.tempPID : Prof.Cool.tempPID) - FEED;
	pidw.sum = dFC.get_target() * 1000;
	pidw.min = dFC.get_minFreq() * 1000;
	pidw.max = dFC.get_maxFreq() * 1000;
#else
	pidw.pre_err = 0;
	pidw.sum = 0;
	pidw.max = 0;   // ПИД может менять частоту без ограничений
#endif
	updatePidTime = updatePidBoiler = xTaskGetTickCount();                // время обновления ПИДа
	// ГВС Сбросить переменные пид регулятора
}
                     
// STOP/WAIT -----------------------------------------
// Функция Останова/Ожидания ТН  - возвращает код ошибки
// Параметр задает что делаем true-останов, false-ожидание
int8_t HeatPump::StopWait(boolean stop)
{  
  if (stop)
  {
    if ((get_State()==pOFF_HP)||(get_State()==pSTOPING_HP)) return error;    // Если ТН выключен или выключается ничего не делаем
    journal.jprintf(pP_DATE,"Stopping...\n");
    setState(pSTOPING_HP);  // Состояние выключения
  } else {
    if ((get_State()==pOFF_HP)||(get_State()==pSTOPING_HP)||(get_State()==pWAIT_HP)) return error;    // Если ТН выключен или выключается или ожидание ничего не делаем
    journal.jprintf(pP_DATE,"Switch to waiting...\n");
    setState(pSTOPING_HP);  // Состояние выключения
  }

  journal.jprintf(" modWork: %d[%s]\n", get_modWork(), codeRet[Status.ret]);
  compressorOFF();		// Останов компрессора, насосов - PUMP_OFF(), ЭРВ

  if (onBoiler) // Если надо уйти с ГВС для облегчения останова компресора
  {
	#ifdef RPUMPBH
	journal.jprintf(" Delay before stop boiler pump\n");
	_delay(Option.delayOffPump * 1000); // задержка перед выключением насосов после выключения компрессора (облегчение останова)
	#endif
	switchBoiler(false);
  }

  if (stop) //Обновление ТН отключаем только при останове
  {
	Task_vUpdate_run = false;					        // Остановить задачу обновления ТН vUpdate (xHandleUpdate)
    journal.jprintf(" Stop task UpdateHP\n");
	Sun_OFF();											// Выключить СК
	time_Sun_OFF = 0;									// выключить задержку последующего включения
  }
    
  if(startPump)
  {
     startPump=false;                                    // Поставить признак что насос выключен
     journal.jprintf(" %s: Pumps in pause %s. . .\n",(char*)__FUNCTION__, "OFF");
  }

 // Принудительное выключение отдельных узлов ТН если они есть в конфиге
  #ifdef RBOILER  // управление дополнительным ТЭНом бойлера
  if(boilerAddHeat()) { // Если используется тэн
     dRelay[RBOILER].set_OFF();  // выключить тен бойлера
  }
  #endif

  #ifdef RHEAT  // управление  ТЭНом отопления
     dRelay[RHEAT].set_OFF();     // выключить тен отопления
  #endif

  #ifdef RPUMPB  // управление  насосом циркуляции ГВС
     dRelay[RPUMPB].set_OFF();     // выключить насос циркуляции ГВС
  #endif

// Выключается в PUMPS_OFF
//  #ifdef RPUMPFL  // управление  насосом циркуляции ТП
//     Pump_HeatFloor(false);    // выключить насос циркуляции ТП
//  #endif

  #ifdef RPUMPBH  // управление  насосом нагрева ГВС
     dRelay[RPUMPBH].set_OFF();
  #endif
  SETBIT0(flags, fHP_BoilerTogetherHeat);

  relayAllOFF();                                         // Все выключить, все  (на всякий случай)
  if (stop)
  {
     //journal.jprintf(" statChart stop\n");
     setState(pOFF_HP);
     journal.jprintf(pP_TIME,"%s OFF . . .\n",(char*)nameHeatPump);
  }
  else
  {
     setState(pWAIT_HP);
     journal.jprintf(pP_TIME,"%s WAIT . . .\n",(char*)nameHeatPump);
  }
  return error;
}

// Получить информацию что надо делать сейчас (на данной итерации)
// Управляет дополнительным нагревателем бойлера, на выходе что будет делать ТН
MODE_HP HeatPump::get_Work()
{
    MODE_HP ret=pOFF;
    Status.ret=pNone;           // не определено
     
    // 1. Бойлер (определяем что делать с бойлером)
    switch ((int)UpdateBoiler())  // проверка бойлера высший приоритет
    {
      case  pCOMP_OFF:  ret=pOFF;      break;
      case  pCOMP_ON:   ret=pBOILER;   break;
      case  pCOMP_NONE: ret=pNONE_B;   break;
    }

   // 2. Дополнительный нагреватель бойлера включение/выключение
   #ifdef RBOILER  // Управление дополнительным ТЭНом бойлера (функция boilerAddHeat() учитывает все режимы ТУРБО и ДОГРЕВ, сальмонелла)
    if(boilerAddHeat()) dRelay[RBOILER].set_ON(); else dRelay[RBOILER].set_OFF();
   #endif

#ifdef DEBUG_MODWORK
    journal.printf(" gW: Status.ret=%d, ret=%d, B=%d\n", Status.ret, ret, onBoiler);
#endif
    
   if ((ret==pBOILER)||(ret==pNONE_B))  return ret; // работает бойлер больше ничего анализировать не надо выход
   if ((get_modeHouse() ==pOFF)&&(ret==pOFF)) return ret; // режим ДОМА выключен (т.е. запрещено отопление или охлаждение дома) И бойлер надо выключить, то выходим с сигналом pOFF (переводим ТН в паузу)
                  
    // Обеспечить переключение с бойлера на отопление/охлаждение, т.е бойлер нагрет и надо идти дальше
    if(onBoiler && ((Status.ret==pNone || Status.ret==pBh3 || Status.ret==pBh22 || Status.ret==pBp3 || (Status.ret>=pBp22 && Status.ret<=pBp27)))) // если бойлер выключяетя по достижению цели или ограничений И режим ГВС
    {
		switchBoiler(false);                // выключить бойлер (задержка в функции) имеено здесь  - а то дальше защиты сработают
    }
 
    // 3. Отопление/охлаждение
    switch ((int)get_modeHouse() )   // проверка отопления
    {
      case  pOFF:       ret=pOFF;      break;
      case  pHEAT:
                  switch ((int)UpdateHeat())
                  {
                    case  pCOMP_OFF:  ret=pOFF;      break;
                    case  pCOMP_ON:   ret=pHEAT;     break;
                    case  pCOMP_NONE: ret=pNONE_H;   break;                 
                  }
                  break;
      case  pCOOL:
                  switch ((int)UpdateCool())
                  {
                    case  pCOMP_OFF:  ret=pOFF;      break;
                    case  pCOMP_ON:   ret=pCOOL;     break;
                    case  pCOMP_NONE: ret=pNONE_C;   break; 
                  }                 
                 break;
    }
   #ifdef RHEAT  // Дополнительный тен для нагрева отопления
    if (GETBIT(Option.flags,fAddHeat))
    {
        if(!GETBIT(Option.flags,fTypeRHEAT)) // резерв
              {
                if (((sTemp[TIN].get_Temp()>Option.tempRHEAT)&&(dRelay[RHEAT].get_Relay()))||((ret==pOFF)&&(dRelay[RHEAT].get_Relay()))) {journal.jprintf(" TIN=%.2f, add heatting off . . .\n",sTemp[TIN].get_Temp()/100.0); dRelay[RHEAT].set_OFF();} // Гистерезис 0.2 градуса что бы не щелкало
                if ((sTemp[TIN].get_Temp()<Option.tempRHEAT-HYSTERESIS_RHEAD)&&(!dRelay[RHEAT].get_Relay())&&(ret!=pOFF))   {journal.jprintf(" TIN=%.2f, add heatting on . . .\n",sTemp[TIN].get_Temp()/100.0);  dRelay[RHEAT].set_ON();}
              }
        else                                // бивалент
              {
                if ( ((sTemp[TOUT].get_Temp()>Option.tempRHEAT)&&(dRelay[RHEAT].get_Relay()))||((ret==pOFF)&&(dRelay[RHEAT].get_Relay())) ) {journal.jprintf(" TOUT=%.2f, add heatting off . . .\n",sTemp[TOUT].get_Temp()/100.0);dRelay[RHEAT].set_OFF();}// Гистерезис 0.2 градуса что бы не щелкало
                if ((sTemp[TOUT].get_Temp()<Option.tempRHEAT-HYSTERESIS_RHEAD)&&(!dRelay[RHEAT].get_Relay())&&(ret!=pOFF))   {journal.jprintf(" TOUT=%.2f, add heatting on . . .\n",sTemp[TOUT].get_Temp()/100.0); dRelay[RHEAT].set_ON();}     
              }
    }  
   #endif
  return ret;  
}
// Управление температурой в зависимости от режима
#define STR_REDUCED "Reduced FC"   // Экономим место
#define STR_FREQUENCY " FC> %.2f\n" // Экономим место
// Итерация по управлению Бойлером
// возврат что надо делать компрессору, функция НЕ управляет компрессором а только выдает необходимость включения компрессора
MODE_COMP  HeatPump::UpdateBoiler()
{

	uint8_t faddheat = boilerAddHeat();
	// Проверки на необходимость выключения бойлера
	if ((get_State()==pOFF_HP)||(get_State()==pSTOPING_HP)) // Если ТН выключен или выключается ничего не делаем
	{
#ifdef RBOILER  // управление дополнительным ТЭНом бойлера
		if(faddheat) {
			dRelay[RBOILER].set_OFF();flagRBOILER=false;  // Выключение
		}
#endif
		return pCOMP_OFF;
	}

	if(!GETBIT(Prof.SaveON.flags,fBoilerON) || (!scheduleBoiler() && !GETBIT(Prof.Boiler.flags,fScheduleAddHeat))) // Если запрещено греть бойлер согласно расписания ИЛИ  Бойлер выключен, выходим и можно смотреть отопление
	{
#ifdef RBOILER  // управление дополнительным ТЭНом бойлера
		if(GETBIT(Prof.Boiler.flags, fAddHeat)) {
			dRelay[RBOILER].set_OFF();
			flagRBOILER=false; // Выключение
		}
#endif
#ifdef RPUMPBH
		if(GETBIT(flags, fHP_BoilerTogetherHeat)) {
			dRelay[RPUMPBH].set_OFF();   // насос ГВС - выключить
			SETBIT0(flags, fHP_BoilerTogetherHeat);
		}
#endif
		return pCOMP_OFF;             // запрещено греть бойлер согласно расписания
	}
	// -----------------------------------------------------------------------------------------------------
	// Сброс излишней энергии в систему отопления
	// Переключаем 3-х ходовой на отопление на ходу и ждем определенное число минут дальше перекидываем на бойлер

	if(GETBIT(Prof.Boiler.flags,fResetHeat))    // Стоит требуемая опция - Сброс тепла в СО
	{
		// Достигнута максимальная температура подачи - 1 градус или температура нагнетания компрессора больше максимальной - 5 градусов
		if ((FEED>Prof.Boiler.tempIn-100)||(sTemp[TCOMP].get_Temp()>sTemp[TCOMP].get_maxTemp()-500))
		{
			journal.jprintf(" Discharge of excess heat %ds...\n", Prof.Boiler.Reset_Time);
			switchBoiler(false);               // Переключится на ходу на отопление
			_delay(Prof.Boiler.Reset_Time*1000);  // Сброс требуемое число секунд
			journal.jprintf(" Back to heat boiler\n");
			switchBoiler(true);              // Переключится на ходу на ГВС
		}
	}

	Status.ret=pNone;                // Сбросить состояние пида

	int16_t T = sTemp[TBOILER].get_Temp();
	int16_t TRG = get_boilerTempTarget();
#ifdef RPUMPBH
	if(GETBIT(Prof.Boiler.flags, fBoilerTogetherHeat) && (Status.modWork == pHEAT || Status.modWork == pNONE_H)) { // Режим одновременного нагрева бойлера с отоплением до температуры догрева
		if(!is_compressor_on() || T > TRG) {
			dRelay[RPUMPBH].set_OFF();   // насос ГВС - выключить
			SETBIT0(flags, fHP_BoilerTogetherHeat);
		} else if(FEED > T + HYSTERESIS_BoilerTogetherHeatSt) {
			SETBIT1(flags, fHP_BoilerTogetherHeat);
			dRelay[RPUMPBH].set_ON();    // насос ГВС - включить
			return pCOMP_OFF;
		} else if(FEED <= T + HYSTERESIS_BoilerTogetherHeatEn) {
			dRelay[RPUMPBH].set_OFF();   // насос ГВС - выключить
			SETBIT0(flags, fHP_BoilerTogetherHeat);
		} else return pCOMP_OFF;
	}
#endif

	// Алгоритм гистерезис для старт стоп
	if(!dFC.get_present() || !GETBIT(Prof.Boiler.flags, fBoilerPID)) // Алгоритм гистерезис для старт стоп и по опции
	{
		if (FEED>Prof.Boiler.tempIn) {Status.ret=pBh1; return pCOMP_OFF; }    // Достигнута максимальная температура подачи ВЫКЛ)

		// Отслеживание выключения (с учетом догрева)
		if ((!GETBIT(Prof.Boiler.flags,fTurboBoiler))&&(GETBIT(Prof.Boiler.flags,fAddHeating)))  // режим догрева
		{
			if (T > Prof.Boiler.tempRBOILER - (onBoiler ? 0 : HYSTERESIS_BoilerAddHeat))   {Status.ret=pBh22; return pCOMP_OFF; }  // Температура выше целевой температуры ДОГРЕВА надо выключаться!
		} else {
			if (T > TRG)   {Status.ret=pBh3; return pCOMP_OFF; }  // Температура выше целевой температуры БОЙЛЕРА надо выключаться!
		}
		// Отслеживание включения
		if (TRG - Prof.Boiler.dTemp) {Status.ret=pBh2; return pCOMP_ON;  }    // Температура ниже гистрезиса надо включаться!

		// дошли до сюда значить сохранение предыдущего состяния, температура в диапазоне регулирования может быть или нагрев или остывание
		if (onBoiler)  {Status.ret=pBh4; return pCOMP_NONE; }  // Если включен принак работы бойлера (трехходовой) значит ПРОДОЛЖНЕНИЕ нагрева бойлера
		Status.ret=pBh5;  return pCOMP_OFF;    // продолжение ПАУЗЫ бойлера внутри гистрезиса
	} // if(!dFC.get_present())
	else // ИНвертор ПИД
	{
		if(FEED>Prof.Boiler.tempIn) {Status.ret=pBp1; set_Error(ERR_PID_FEED,(char*)__FUNCTION__);return pCOMP_OFF;}         // Достижение максимальной температуры подачи - это ошибка ПИД не рабоатет
		// Отслеживание выключения (с учетом догрева)
		if ((!GETBIT(Prof.Boiler.flags,fTurboBoiler))&&(GETBIT(Prof.Boiler.flags,fAddHeating)))  // режим догрева
		{
			if (T > Prof.Boiler.tempRBOILER - (onBoiler ? 0 : HYSTERESIS_BoilerAddHeat))   {Status.ret=pBp22; return pCOMP_OFF; }  // Температура выше целевой температуры ДОГРЕВА надо выключаться!
		}
		else
		{
			if (T > TRG)   {Status.ret=pBp3; return pCOMP_OFF; }  // Температура выше целевой температуры БОЙЛЕРА надо выключаться!
		}
		// Отслеживание включения
		if (rtcSAM3X8.unixtime()-dFC.get_startTime()<FC_ACCEL_TIME/100 ){Status.ret=pBp10; return pCOMP_NONE;  }  // РАЗГОН частоту не трогаем
		else if ((T < (TRG-Prof.Boiler.dTemp))&&(!(onBoiler))) {Status.ret=pBp2; return pCOMP_ON;} // Достигнут гистерезис и компрессор еще не рабоатет на ГВС - Старт бойлера
		else if ((dFC.isfOnOff())&&(!(onBoiler))) return pCOMP_OFF;                               // компрессор рабатает но ГВС греть не надо  - уходим без изменения состояния
		//    if (T<(TRG-Prof.Boiler.dTemp)) {Status.ret=pBh2; return pCOMP_ON;  }    // Температура ниже гистрезиса надо включаться!
		// ПИД ----------------------------------
		// ЗАЩИТА Компресор работает, достигнута максимальная температура подачи, мощность, температура компрессора то уменьшить обороты на stepFreq
		else if ((dFC.isfOnOff())&&(FEED>Prof.Boiler.tempIn-dFC.get_dtTempBoiler()))             // Подача ограничение
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (FEED: %.2f)\n",STR_REDUCED,dFC.get_stepFreqBoiler()/100.0,FEED/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreqBoiler()<dFC.get_minFreqBoiler()) { Status.ret=pBp23; return pCOMP_OFF; }        // Уменьшать дальше некуда, выключаем компрессор
			dFC.set_target(dFC.get_target()-dFC.get_stepFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler()); Status.ret=pBp6; return pCOMP_NONE;                // Уменьшить частоту
		}
		else if ((dFC.isfOnOff())&&(dFC.get_power()>FC_MAX_POWER_BOILER))                    // Мощность для ГВС меньшая мощность
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (POWER: %.3f kW)\n",STR_REDUCED,dFC.get_stepFreqBoiler()/100.0,(float)dFC.get_power()/1000.0); // КИЛОВАТЫ
#endif
			if (dFC.get_target()-dFC.get_stepFreqBoiler()<dFC.get_minFreqBoiler())  { Status.ret=pBp24; return pCOMP_OFF; }       // Уменьшать дальше некуда, выключаем компрессор
			dFC.set_target(dFC.get_target()-dFC.get_stepFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler()); Status.ret=pBp7; return pCOMP_NONE;               // Уменьшить частоту
		}
		else if ((dFC.isfOnOff())&&(dFC.get_current()>FC_MAX_CURRENT_BOILER))                // ТОК для ГВС меньшая мощность
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (CURRENT: %.2f)\n",STR_REDUCED,dFC.get_stepFreqBoiler()/100.0,(float)dFC.get_current()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreqBoiler()<dFC.get_minFreqBoiler()) { Status.ret=pBp27; return pCOMP_OFF;  }      // Уменьшать дальше некуда, выключаем компрессор
			dFC.set_target(dFC.get_target()-dFC.get_stepFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler()); Status.ret=pBp16; return pCOMP_NONE;               // Уменьшить частоту
		}

		else if ((dFC.isfOnOff())&&((sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp())>sTemp[TCOMP].get_maxTemp()))  // температура компрессора
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (TCOMP: %.2f)\n",STR_REDUCED,dFC.get_stepFreqBoiler()/100.0,sTemp[TCOMP].get_Temp()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreqBoiler()<dFC.get_minFreqBoiler()) { Status.ret=pBp25; return pCOMP_OFF; }      // Уменьшать дальше некуда, выключаем компрессор
			dFC.set_target(dFC.get_target()-dFC.get_stepFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler()); Status.ret=pBp8; return pCOMP_NONE; // Уменьшить частоту
		}
#ifdef PCON			
		else if ((dFC.isfOnOff())&&(sADC[PCON].get_present())&&(sADC[PCON].get_Press()>sADC[PCON].get_maxPress()-FC_DT_CON_PRESS))    // давление конденсатора до максимальной минус 0.5 бара
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (PCON:  %.2f)\n",STR_REDUCED,dFC.get_stepFreqBoiler()/100.0,sADC[PCON].get_Press()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreqBoiler()<dFC.get_minFreqBoiler()) { Status.ret=pBp26; return pCOMP_OFF; }     // Уменьшать дальше некуда, выключаем компрессор
			dFC.set_target(dFC.get_target()-dFC.get_stepFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler()); Status.ret=pBp9;  return pCOMP_NONE;            // Уменьшить частоту
		}
#endif		
		//   else if (((TRG-Prof.Boiler.dTemp)>T)&&(!(dFC.isfOnOff())&&(Status.modWork!=pBOILER))) {Status.ret=7; return pCOMP_ON;} // Достигнут гистерезис и компрессор еще не рабоатет на ГВС - Старт бойлера
		else if(!(dFC.isfOnOff())) {Status.ret=pBp5; return pCOMP_OFF; }                                                          // Если компрессор не рабоатет то ничего не делаем и выходим

		else if(xTaskGetTickCount()-updatePidBoiler<HP.get_timeBoiler()*1000)   {Status.ret=pBp11; return pCOMP_NONE;  }             // время обновления ПИДа еше не пришло
		// Дошли до сюда - ПИД на подачу. Компресор работает
		updatePidBoiler=xTaskGetTickCount();
#ifdef SUPERBOILER
		Status.ret=pBp14;
        int16_t newFC = updatePID((Prof.Boiler.tempPID-PressToTemp(HP.sADC[PCON].get_Press(),HP.dEEV.get_typeFreon())), Prof.Boiler.pid, pidw); // Одна итерация ПИД регулятора (на выходе ИЗМЕНЕНИЕ частоты)
#else
		Status.ret=pBp12;
		int16_t newFC = updatePID(Prof.Boiler.tempPID - FEED, Prof.Boiler.pid, pidw);             // Одна итерация ПИД регулятора (на выходе ИЗМЕНЕНИЕ частоты)
#endif
#ifdef PID_FORMULA2
		if(newFC > dFC.get_target() + dFC.get_PidFreqStep()) newFC = dFC.get_target() + dFC.get_PidFreqStep(); // На увеличение
		//else if(newFC < dFC.get_target() - dFC.get_PidFreqStep()) newFC = dFC.get_target() - dFC.get_PidFreqStep(); // На уменьшение
#else
		if (newFC>dFC.get_PidFreqStep()) newFC=dFC.get_target()+dFC.get_PidFreqStep(); else newFC +=dFC.get_target(); // Расчет целевой частоты с ограничением на ее рост не более dFC.get_PidFreqStep()
#endif
		if (newFC>dFC.get_maxFreqBoiler())   newFC=dFC.get_maxFreqBoiler();                                                 // ограничение диапазона ОТДЕЛЬНО для ГВС!!!! (меньше мощность)
		if (newFC<dFC.get_minFreqBoiler())   newFC=dFC.get_minFreqBoiler(); //return pCOMP_OFF;                             // Уменьшать дальше некуда, выключаем компрессор

		// Смотрим подход к границе защит если идет УВЕЛИЧЕНИЕ частоты
		if (dFC.get_target()<newFC)                                                                                     // Идет увеличение частоты проверяем подход к границам
		{
//			if ((dFC.isfOnOff())&&(FEED>(Prof.Boiler.tempIn-dFC.get_dtTempBoiler())*dFC.get_PidStop()/100))                                                 {Status.ret=pBp17; return pCOMP_NONE;}   // Подача ограничение
			if ((dFC.isfOnOff())&&(dFC.get_power()>(FC_MAX_POWER_BOILER*dFC.get_PidStop()/100)))                                                            {Status.ret=pBp18; return pCOMP_NONE;}   // Мощность для ГВС меньшая мощность
			if ((dFC.isfOnOff())&&(dFC.get_current()>(FC_MAX_CURRENT_BOILER*dFC.get_PidStop()/100)))                                                        {Status.ret=pBp19; return pCOMP_NONE;}   // ТОК для ГВС меньшая мощность
			if ((dFC.isfOnOff())&&((sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp())>(sTemp[TCOMP].get_maxTemp()*dFC.get_PidStop()/100)))                      {Status.ret=pBp20; return pCOMP_NONE;}   // температура компрессора
			if ((dFC.isfOnOff())&&(sADC[PCON].get_present())&&(sADC[PCON].get_Press()>((sADC[PCON].get_maxPress()-FC_DT_CON_PRESS)*dFC.get_PidStop()/100))) {Status.ret=pBp21; return pCOMP_NONE;}   // давление конденсатора до максимальной минус 0.5 бара
		}
		//    надо менять
		if (dFC.get_target()!=newFC)                                                                                     // Установка частоты если нужно менять
		{
#ifdef DEBUG_MODWORK
			journal.jprintf((char*)STR_FREQUENCY,newFC/100.0);
#endif
			dFC.set_target(newFC,false,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler());
		}
		return pCOMP_NONE;
	}

}

int16_t HeatPump::CalcTargetPID(type_settingHP &settings)
{
    int16_t  targetRealPID;				 // Цель подачи
	if(GETBIT(settings.flags, fWeather)) { // включена погодозависимость
		targetRealPID = settings.tempPID + (settings.kWeather * (TEMP_WEATHER - sTemp[TOUT].get_Temp()) / 1000); // включена погодозависимость, коэффициент в ТЫСЯЧНЫХ результат в сотых градуса, определяем цель
		if(targetRealPID > settings.tempIn - 50) targetRealPID = settings.tempIn - 50;  // ограничение целевой подачи = максимальная подача - 0.5 градуса
		if(targetRealPID < MIN_WEATHER) targetRealPID = MIN_WEATHER;
		if(targetRealPID > MAX_WEATHER) targetRealPID = MAX_WEATHER;
	} else targetRealPID = settings.tempPID; // отключена погодозависмость
	return targetRealPID;
}

// -----------------------------------------------------------------------------------------------------
// Итерация по управлению НАГРЕВ  пола
// выдает что надо делать компрессору, но ничем не управляет для старт-стопа для инвертора меняет обороты
MODE_COMP HeatPump::UpdateHeat()
{
	int16_t target,t1;
	int16_t newFC;

	if ((get_State()==pOFF_HP)||(get_State()==pSTOPING_HP)) return pCOMP_OFF;    // Если ТН выключен или выключается ничего не делаем

	if ((rtcSAM3X8.unixtime()-offBoiler>Option.delayBoilerOff)&&((abs(FEED-RET)>Prof.Heat.dt)&&is_compressor_on())){set_Error(ERR_DTEMP_CON,(char*)__FUNCTION__);return pCOMP_NONE;}// Привышение разности температур кондесатора при включеноом компрессорае (есть задержка при переключении ГВС)
#ifdef RTRV    
	if ((dRelay[RTRV].get_Relay())&&is_compressor_on())      dRelay[RTRV].set_OFF();  // отопление Проверить и если надо установить 4-ходовой клапан только если компрессор рабоатет (защита это лишнее)
#endif
	Status.ret=pNone;         // Сбросить состояние пида
	t1 = GETBIT(Prof.Heat.flags,fTarget) ? RET : sTemp[TIN].get_Temp();  // вычислить температуры для сравнения Prof.Heat.Target 0-дом   1-обратка
	target = get_targetTempHeat();
	switch (Prof.Heat.Rule)   // в зависмости от алгоритма
	{
	case pHYSTERESIS:  // Гистерезис нагрев.
		if(t1>target)                       {Status.ret=pHh3;   return pCOMP_OFF;}          // Достигнута целевая температура  ВЫКЛ
		else if((rtcSAM3X8.unixtime()-offBoiler>Option.delayBoilerOff)&&(FEED>Prof.Heat.tempIn)){Status.ret=pHh1;   return pCOMP_OFF;} // Достигнута максимальная температура подачи ВЫКЛ (С учетом времени перехода с ГВС)
		else if(t1<target-Prof.Heat.dTemp)  {Status.ret=pHh2;   return pCOMP_ON; }          // Достигнут гистерезис ВКЛ
		else if(RET<Prof.Heat.tempOut)      {Status.ret=pHh13;  return pCOMP_ON; }          // Достигнут минимальная темература обратки ВКЛ
		else                                {Status.ret=pHh4;   return pCOMP_NONE;}         // Ничего не делаем  (сохраняем состояние)
		break;
	case pPID:   // ПИД регулирует подачу, а целевай функция гистререзис
		// отработка гистререзиса целевой функции (дом/обратка)
		if(t1>target)                       { Status.ret=pHp3; return pCOMP_OFF;}                            // Достигнута целевая температура  ВЫКЛ
		else if((rtcSAM3X8.unixtime()-offBoiler>Option.delayBoilerOff)&&(FEED>Prof.Heat.tempIn)) {Status.ret=pHp1; set_Error(ERR_PID_FEED,(char*)__FUNCTION__);return pCOMP_OFF;}  // Достижение максимальной температуры подачи - это ошибка ПИД не рабоатет (есть задержка срабатывания для переключенияс ГВС)
		//  else if ((t1<target-Prof.Heat.dTemp)&&(!(dFC.isfOnOff())))  {Status.ret=pHp2; return pCOMP_ON; } // Достигнут гистерезис и компрессор еще не рабоатет ВКЛ
		//  else if ((t1<target-Prof.Heat.dTemp)&&(!(dFC.isfOnOff())))  {Status.ret=pHp2; return pCOMP_ON; } // Достигнут гистерезис (компрессор не рабоатет) ВКЛ
		//  else if ((t1<target-Prof.Heat.dTemp)&&(dFC.isfOnOff())&&(dRelay[R3WAY].get_Relay())) {Status.ret=pHp2; return pCOMP_ON;} // Достигнут гистерезис (бойлер нагрет) ВКЛ
		else if ((t1<target-Prof.Heat.dTemp)&&(!(dFC.isfOnOff())))  {Status.ret=pHp2; return pCOMP_ON; }     // Достигнут гистерезис (компрессор не рабоатет) ВКЛ
//		else if ((t1<target-Prof.Heat.dTemp)&&(dFC.isfOnOff())&&(!get_onBoiler())) {Status.ret=pHp2; return pCOMP_ON;} // Достигнут гистерезис (компрессор работает, но это не бойлер) ВКЛ (в принципе это лишнее)

		// ЗАЩИТА Компресор работает, достигнута максимальная температура подачи, мощность, температура компрессора или давление то уменьшить обороты на stepFreq
		else if ((dFC.isfOnOff())&&(FEED>Prof.Heat.tempIn-dFC.get_dtTemp()))         // Подача ограничение (в разделе защита)
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (FEED: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,FEED/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreq()) {  Status.ret=pHp23; return pCOMP_OFF; }               // Уменьшать дальше некуда, выключаем компрессор
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq());  Status.ret=pHp6; return pCOMP_NONE;// Уменьшить частоту
		}

		else if ((dFC.isfOnOff())&&(dFC.get_power()>FC_MAX_POWER))                    // Мощность в ватт
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (POWER: %.3f kW)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,dFC.get_power()/1000.0); // КИЛОВАТЫ
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreq()) {Status.ret=pHp24; return pCOMP_OFF;   }             // Уменьшать дальше некуда, выключаем компрессор
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq());Status.ret=pHp7;  return pCOMP_NONE;                     // Уменьшить частоту
		}
		else if ((dFC.isfOnOff())&&(dFC.get_current()>FC_MAX_CURRENT))                // ТОК
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (CURRENT: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,dFC.get_current()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreq())  {Status.ret=pHp27;return pCOMP_OFF; }               // Уменьшать дальше некуда, выключаем компрессор
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq()); Status.ret=pHp16; return pCOMP_NONE;                     // Уменьшить частоту
		}

		else if ((dFC.isfOnOff())&&((sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp())>sTemp[TCOMP].get_maxTemp()))  // температура компрессора 
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (TCOMP: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,sTemp[TCOMP].get_Temp()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreq()) {Status.ret=pHp25; return pCOMP_OFF;  }              // Уменьшать дальше некуда, выключаем компрессор
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq()); Status.ret=pHp8; return pCOMP_NONE;                     // Уменьшить частоту
		}
#ifdef PCON		
		else if ((dFC.isfOnOff())&&(sADC[PCON].get_present())&&(sADC[PCON].get_Press()>sADC[PCON].get_maxPress()-FC_DT_CON_PRESS))  // давление конденсатора до максимальной минус 0.5 бара
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (PCON:  %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,sADC[PCON].get_Press()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreq()) {   Status.ret=pHp26; return pCOMP_OFF; }               // Уменьшать дальше некуда, выключаем компрессор
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq());    Status.ret=pHp9;  return pCOMP_NONE;               // Уменьшить частоту
		}
#endif		
		else if(!(dFC.isfOnOff())) {Status.ret=pHp5; return pCOMP_NONE;  }                                               // Если компрессор не рабоатет то ничего не делаем и выходим
		else if (rtcSAM3X8.unixtime()-dFC.get_startTime()<FC_ACCEL_TIME/100 ){ Status.ret=pHp10; return pCOMP_NONE;}     // РАЗГОН частоту не трогаем

#ifdef SUPERBOILER                                            // Бойлер греется от предкондесатора
		if (sTemp[TCOMP].get_Temp()-SUPERBOILER_DT>sTemp[TBOILER].get_Temp())  dRelay[RSUPERBOILER].set_ON(); else dRelay[RSUPERBOILER].set_OFF();// исправил плюс на минус
		if(xTaskGetTickCount()-updatePidTime<HP.get_timeHeat()*1000)         { Status.ret=pHp11;   return pCOMP_NONE;}   // время обновления ПИДа еше не пришло
		if (onBoiler) Status.ret=pHp15; else Status.ret=pHp12;                                          // если нужно показывем что бойлер греется от предкондесатора
#else
		else if(xTaskGetTickCount()-updatePidTime<HP.get_timeHeat()*1000)    { Status.ret=pHp11;   return pCOMP_NONE;}   // время обновления ПИДа еше не пришло
		Status.ret=pHp12;   // Дошли до сюда - ПИД на подачу. Компресор работает
#endif

		updatePidTime=xTaskGetTickCount();
		newFC = updatePID(CalcTargetPID(Prof.Heat) - FEED, Prof.Heat.pid, pidw);         // Одна итерация ПИД регулятора (на выходе ИЗМЕНЕНИЕ частоты)
#ifdef PID_FORMULA2
		if(newFC > dFC.get_target() + dFC.get_PidFreqStep()) newFC = dFC.get_target() + dFC.get_PidFreqStep(); // На увеличение
		//else if(newFC < dFC.get_target() - dFC.get_PidFreqStep()) newFC = dFC.get_target() - dFC.get_PidFreqStep(); // На уменьшение
#else
		if (newFC>dFC.get_PidFreqStep()) newFC=dFC.get_target()+dFC.get_PidFreqStep(); else newFC += dFC.get_target(); // Расчет целевой частоты с ограничением на ее рост не более dFC.get_PidFreqStep()
#endif

		if (newFC>dFC.get_maxFreq())   newFC=dFC.get_maxFreq();                                                // ограничение диапазона
		else if (newFC<dFC.get_minFreq())   newFC=dFC.get_minFreq();

		// Смотрим подход к границе защит если идет УВЕЛИЧЕНИЕ частоты
		if (dFC.get_target()<newFC)                                                                        // Идет увеличение частоты проверяем подход к границами если пересекли границы то частоту не меняем
		{
//    		if ((dFC.isfOnOff())&&(FEED>(targetRealPID*dFC.get_PidStop()/100)))                                                                            {Status.ret=pHp17; return pCOMP_NONE;}   // Подача ограничение, с учетом погодозависимости Подход снизу
			if ((dFC.isfOnOff())&&(dFC.get_power()>(FC_MAX_POWER*dFC.get_PidStop()/100)))                                                                  {Status.ret=pHp18; return pCOMP_NONE;}   // Мощность для ГВС меньшая мощность
			if ((dFC.isfOnOff())&&(dFC.get_current()>(FC_MAX_CURRENT*dFC.get_PidStop()/100)))                                                              {Status.ret=pHp19; return pCOMP_NONE;}   // ТОК для ГВС меньшая мощность
			if ((dFC.isfOnOff())&&(sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp())>(sTemp[TCOMP].get_maxTemp()*dFC.get_PidStop()/100))                       {Status.ret=pHp20; return pCOMP_NONE;}   // температура компрессора
			if ((dFC.isfOnOff())&&(sADC[PCON].get_present())&&(sADC[PCON].get_Press()>(sADC[PCON].get_maxPress()-FC_DT_CON_PRESS)*dFC.get_PidStop()/100))  {Status.ret=pHp21; return pCOMP_NONE;}   // давление конденсатора до максимальной минус 0.5 бара
		}
		//    надо менять
		if (dFC.get_target()!=newFC)                                                                     // Установкка частоты если нужно менять
		{
#ifdef DEBUG_MODWORK
			journal.jprintf((char*)STR_FREQUENCY,newFC/100.0);
#endif
			dFC.set_target(newFC,false,dFC.get_minFreq(),dFC.get_maxFreq());
		}
		return pCOMP_NONE;                                                                            // компрессор состояние не меняет
		break;
	case pHYBRID:
		break;
	default: break;
	}
	return pCOMP_NONE;
}

// -----------------------------------------------------------------------------------------------------
// Итерация по управлению ОХЛАЖДЕНИЕ
// выдает что надо делать компрессору, но ничем не управляет
MODE_COMP HeatPump::UpdateCool()
{
	int16_t target,t1;
	int16_t newFC;

	if ((get_State()==pOFF_HP)||(get_State()==pSTOPING_HP)) return pCOMP_OFF;    // Если ТН выключен или выключается ничего не делаем

	if ((rtcSAM3X8.unixtime()-offBoiler>Option.delayBoilerOff)&&((abs(FEED-RET)>Prof.Cool.dt)&&is_compressor_on()))   {set_Error(ERR_DTEMP_CON,(char*)__FUNCTION__);return pCOMP_NONE;}// Привышение разности температур кондесатора при включеноом компрессорае
#ifdef RTRV
	if ((!dRelay[RTRV].get_Relay())&&is_compressor_on())  dRelay[RTRV].set_ON();                                        // Охлаждение Проверить и если надо установить 4-ходовой клапан только если компрессор рабоатет (защита это лишнее)
#endif
	Status.ret=pNone;                                                                                   // Сбросить состояние пида
	t1 = GETBIT(Prof.Cool.flags,fTarget) ? RET : sTemp[TIN].get_Temp(); // вычислить температуры для сравнения Prof.Heat.Target 0-дом   1-обратка
	target = get_targetTempCool();
	switch (Prof.Cool.Rule)   // в зависмости от алгоритма
	{
	case pHYSTERESIS:  // Гистерезис охлаждение.
		if(t1<target)             {Status.ret=pCh3;   return pCOMP_OFF;}                            // Достигнута целевая температура  ВЫКЛ
		else if((rtcSAM3X8.unixtime()-offBoiler>Option.delayBoilerOff)&&(FEED<Prof.Cool.tempIn)){Status.ret=pCh1;return pCOMP_OFF;}// Достигнута минимальная температура подачи ВЫКЛ
		else if(t1>target+Prof.Cool.dTemp)  {Status.ret=pCh2;   return pCOMP_ON; }                       // Достигнут гистерезис ВКЛ
		else if(RET>Prof.Cool.tempOut)      {Status.ret=pCh13;  return pCOMP_ON; }                       // Достигнут Максимальная темература обратки ВКЛ
		else  {Status.ret=pCh4;    return pCOMP_NONE;   }                                                // Ничего не делаем  (сохраняем состояние)
		break;
	case pPID:   // ПИД регулирует подачу, а целевай функция гистререзис
		// отработка гистререзиса целевой функции (дом/обратка)

		if(t1<target)                     { Status.ret=pCp3; return pCOMP_OFF;}                            // Достигнута целевая температура  ВЫКЛ
		else if ((rtcSAM3X8.unixtime()-offBoiler>Option.delayBoilerOff)&&(FEED<Prof.Cool.tempIn)) {Status.ret=pCp1; set_Error(ERR_PID_FEED,(char*)__FUNCTION__);return pCOMP_OFF;}         // Достижение минимальной температуры подачи - это ошибка ПИД не рабоатет
		//  else if ((t1<target-Prof.Cool.dTemp)&&(!(dFC.isfOnOff())))  {Status.ret=pCp2; return pCOMP_ON; }                        // Достигнут гистерезис и компрессор еще не рабоатет ВКЛ
		//             else if ((t1>target+Prof.Cool.dTemp)&&(!(dFC.isfOnOff())))  {Status.ret=pCp2; return pCOMP_ON; }                          // Достигнут гистерезис (компрессор не рабоатет) ВКЛ
		//             else if ((t1>target+Prof.Cool.dTemp)&&(dFC.isfOnOff())&&(dRelay[R3WAY].get_Relay())) {Status.ret=pCp2; return pCOMP_ON;}  // Достигнут гистерезис (бойлер нагрет) ВКЛ
		else if ((t1>target+Prof.Cool.dTemp)&&(!(dFC.isfOnOff())))  {Status.ret=pCp2; return pCOMP_ON; }                          // Достигнут гистерезис (компрессор не рабоатет) ВКЛ
//		else if ((t1>target+Prof.Cool.dTemp)&&(dFC.isfOnOff())&&(!get_onBoiler())) {Status.ret=pCp2; return pCOMP_ON;}             // Достигнут гистерезис (компрессор работает, но это не бойлер) ВКЛ  (это лишнее)

		// ЗАЩИТА Компресор работает, достигнута минимальная температура подачи, мощность, температура компрессора или давление то уменьшить обороты на stepFreq
		else if ((dFC.isfOnOff())&&(FEED<Prof.Cool.tempIn+dFC.get_dtTemp()))                  // Подача
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (FEED: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,FEED/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreqCool()) {  Status.ret=pCp23; return pCOMP_OFF;  }              // Уменьшать дальше некуда, выключаем компрессор
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool());  Status.ret=pCp6;  return pCOMP_NONE;               // Уменьшить частоту
		}

		else if ((dFC.isfOnOff())&&(dFC.get_power()>FC_MAX_POWER))                    // Мощность
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (POWER: %.3f kW)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,dFC.get_power()/1000.0); // КИЛОВАТЫ
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreqCool()) {  Status.ret=pCp24; return pCOMP_OFF;  }         // Уменьшать дальше некуда, выключаем компрессор
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool());   Status.ret=pCp7;  return pCOMP_NONE;               // Уменьшить частоту
		}
		else if ((dFC.isfOnOff())&&(dFC.get_current()>FC_MAX_CURRENT))                    // ТОК
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (CURRENT: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,dFC.get_current()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreqCool()) {Status.ret=pCp27; return pCOMP_OFF; }          // Уменьшать дальше некуда, выключаем компрессор
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool());   Status.ret=pCp16; return pCOMP_NONE;               // Уменьшить частоту
		}
		else if ((dFC.isfOnOff())&&((sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp())>sTemp[TCOMP].get_maxTemp()))  // температура компрессора
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (TCOMP: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,sTemp[TCOMP].get_Temp()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreqCool())  {Status.ret=pCp25;return pCOMP_OFF;}                // Уменьшать дальше некуда, выключаем компрессор
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool());   Status.ret=pCp8; return pCOMP_NONE;               // Уменьшить частоту
		}
#ifdef PCON			
		else if ((dFC.isfOnOff())&&(sADC[PCON].get_present())&&(sADC[PCON].get_Press()>sADC[PCON].get_maxPress()-FC_DT_CON_PRESS))  // давление конденсатора до максимальной минус 0.5 бара
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (PCON:  %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,sADC[PCON].get_Press()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreqCool()) {Status.ret=pCp26;  return pCOMP_OFF;   }        // Уменьшать дальше некуда, выключаем компрессор
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool()); Status.ret=pCp9;  return pCOMP_NONE;               // Уменьшить частоту
		}
#endif		
		else if(!(dFC.isfOnOff())) {Status.ret=pCp5; return pCOMP_NONE;  }                                               // Если компрессор не рабоатет то ничего не делаем и выходим
		else if (rtcSAM3X8.unixtime()-dFC.get_startTime()<FC_ACCEL_TIME/100 ){ Status.ret=pCp10; return pCOMP_NONE;}     // РАЗГОН частоту не трогаем

#ifdef SUPERBOILER                                            // Бойлер греется от предкондесатора
		if (sTemp[TCOMP].get_Temp()+SUPERBOILER_DT>sTemp[TBOILER].get_Temp())  dRelay[RSUPERBOILER].set_ON(); else dRelay[RSUPERBOILER].set_OFF();
		if(xTaskGetTickCount()-updatePidTime<HP.get_timeHeat()*1000)         { Status.ret=pCp11;   return pCOMP_NONE;}   // время обновления ПИДа еше не пришло
		if (onBoiler) Status.ret=pCp15; else Status.ret=pCp12;                                          // если нужно показывем что бойлер греется от предкондесатора
#else
		else if(xTaskGetTickCount()-updatePidTime<HP.get_timeHeat()*1000)    { Status.ret=pCp11;   return pCOMP_NONE;}   // время обновления ПИДа еше не пришло
		Status.ret=pCp12;   // Дошли до сюда - ПИД на подачу. Компресор работает
#endif

		updatePidTime=xTaskGetTickCount();
		newFC = updatePID(CalcTargetPID(Prof.Cool) - FEED, Prof.Cool.pid, pidw);      // Одна итерация ПИД регулятора (на выходе ИЗМЕНЕНИЕ частоты)
#ifdef PID_FORMULA2
		if(newFC > dFC.get_target() + dFC.get_PidFreqStep()) newFC = dFC.get_target() + dFC.get_PidFreqStep(); // На увеличение
		//else if(newFC < dFC.get_target() - dFC.get_PidFreqStep()) newFC = dFC.get_target() - dFC.get_PidFreqStep(); // На уменьшение
#else
		if (newFC>dFC.get_PidFreqStep()) newFC=dFC.get_target()+dFC.get_PidFreqStep(); else newFC += dFC.get_target(); // Расчет целевой частоты с ограничением на ее рост не более dFC.get_PidFreqStep()
#endif
        
		if (newFC>dFC.get_maxFreqCool())   newFC=dFC.get_maxFreqCool();                                       // ограничение диапазона
		if (newFC<dFC.get_minFreqCool())   newFC=dFC.get_minFreqCool(); // return pCOMP_OFF;                                              // Уменьшать дальше некуда, выключаем компрессор// newFC=minFreq;

		//    journal.jprintf("newFC=%.2f\n",newFC/100.0);

		// Смотрим подход к границе защит если идет УВЕЛИЧЕНИЕ частоты
		if (dFC.get_target()<newFC)                                                                                     // Идет увеличение частоты проверяем подход к границам
		{
//    		if ((dFC.isfOnOff())&&(FEED<(targetRealPID*(100+(100-dFC.get_PidStop()))/100)))                                                                     {Status.ret=pCp17; return pCOMP_NONE;}   // Подача ограничение, с учетом погодозависимости Подход сверху
			if ((dFC.isfOnOff())&&(dFC.get_power()>(FC_MAX_POWER*dFC.get_PidStop()/100)))                                                                       {Status.ret=pCp18; return pCOMP_NONE;}   // Мощность для ГВС меньшая мощность
			if ((dFC.isfOnOff())&&(dFC.get_current()>(FC_MAX_CURRENT*dFC.get_PidStop()/100)))                                                                   {Status.ret=pCp19; return pCOMP_NONE;}   // ТОК для ГВС меньшая мощность
			if ((dFC.isfOnOff())&&(sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp()>(sTemp[TCOMP].get_maxTemp()*dFC.get_PidStop()/100)))                            {Status.ret=pCp20; return pCOMP_NONE;}   // температура компрессора
			if ((dFC.isfOnOff())&&(sADC[PCON].get_present())&&(sADC[PCON].get_Press()>(sADC[PCON].get_maxPress()-FC_DT_CON_PRESS)*dFC.get_PidStop()/100))       {Status.ret=pCp21; return pCOMP_NONE;}   // давление конденсатора до максимальной минус 0.5 бара
		}
		//    надо менять

		if (dFC.get_target()!=newFC)                                                                     // Установкка частоты если нужно менять
		{
#ifdef DEBUG_MODWORK
			journal.jprintf((char*)STR_FREQUENCY,newFC/100.0);
#endif
			dFC.set_target(newFC,false,dFC.get_minFreqCool(),dFC.get_maxFreqCool());
		}
		return pCOMP_NONE;                                                                            // компрессор состояние не меняет
		break;
	case pHYBRID:
		break;
	default: break;
	}
	return pCOMP_NONE;

}

// Концигурация 4-х, 3-х ходового крана и включение насосов, тен бойлера, тен отопления
// В зависммости от входного параметра конфигурирует краны
void HeatPump::configHP(MODE_HP conf)
{
     // 1. Обеспечение минимальной паузы компрессора, если мало времени то не конфигурируем  и срузу выходим!!
	 // теперь в другом месте (vad7)
   
     // 2. Конфигурация в нужный режим
     //#ifdef RBOILER // Если надо выключить тен бойлера (если его нет в настройках)
   	 // не будем мешать тэну...
     //  if((!GETBIT(Prof.Boiler.flags,fTurboBoiler))&&(!GETBIT(Prof.Boiler.flags,fAddHeating))) dRelay[RBOILER].set_OFF();   // ТЭН вообще не используется - Выключить ТЭН бойлера если настройки соответсвуют
     //#endif
     switch ((int)conf)
     {
      case  pOFF: // ЭТО может быть пауза! Выключить - установить положение как при включении ( перевод 4-х ходового производится после отключения компрессора (см compressorOFF()))
                 
                 switchBoiler(false);                                            // выключить бойлер
               
                 _delay(DELAY_AFTER_SWITCH_RELAY);                               // Задержка
                 #ifdef SUPERBOILER                                             // Бойлер греется от предкондесатора
                     dRelay[RSUPERBOILER].set_OFF();                            // Евгений добавил выключить супербойлер
                 #endif
                 #ifdef RBOILER
                      if((GETBIT(Prof.Boiler.flags,fTurboBoiler))&&(dRelay[RBOILER].get_present())) dRelay[RBOILER].set_OFF();  // Выключить ТЭН бойлера в режиме турбо (догрев не работате)
                 #endif
                 #ifdef RHEAT
                     if (dRelay[RHEAT].get_present()) dRelay[RHEAT].set_OFF();     // Выключить ТЭН отопления
                 #endif 
                 PUMPS_OFF;
                break;    
      case  pHEAT:    // Отопление
                 PUMPS_ON;                                                     // включить насосы

                 #ifdef RTRV
                  if (is_compressor_on()&&(dRelay[RTRV].get_Relay()==true)) ChangesPauseTRV();    // Компрессор работает и 4-х ходовой стоит на холоде то хитро переключаем 4-х ходовой в положение тепло
                 dRelay[RTRV].set_OFF();                                        // нагрев
                 _delay(DELAY_AFTER_SWITCH_RELAY);                        // Задержка
                 #endif

                 switchBoiler(false);                                            // выключить бойлер это лишнее наверное переключение идет в get_Work() но пусть будет
                 
                 #ifdef SUPERBOILER                                            // Бойлер греется от предкондесатора
                   dRelay[RSUPERBOILER].set_OFF();                             // Евгений добавил выключить супербойлер
                 #endif
                 #ifdef RBOILER
                    if((GETBIT(Prof.Boiler.flags,fTurboBoiler))&&(dRelay[RBOILER].get_present())) dRelay[RBOILER].set_OFF(); // Выключить ТЭН бойлера (режим форсированного нагрева)
                 #endif
                 #ifdef RHEAT
              //     if((GETBIT(Option.flags,fAddHeat))&&(dRelay[RHEAT].get_present())) dRelay[RHEAT].set_ON(); else dRelay[RHEAT].set_OFF(); // Если надо включить ТЭН отопления
                 #endif
                 if (!is_compressor_on())  dFC.set_target(dFC.get_startFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq());  // установить стартовую частоту если компрессор выключен
                break;    
      case  pCOOL:    // Охлаждение
                 PUMPS_ON;                                                     // включить насосы

                 #ifdef RTRV
                 if (is_compressor_on()&&(dRelay[RTRV].get_Relay()==false)) ChangesPauseTRV();    // Компрессор рабатает и 4-х ходовой стоит на тепле то хитро переключаем 4-х ходовой в положение холод
                 dRelay[RTRV].set_ON();                                       // охлаждение
                 _delay(DELAY_AFTER_SWITCH_RELAY);                        // Задержка на 2 сек
                 #endif 

                  switchBoiler(false);                                           // выключить бойлер
                 #ifdef RBOILER
                  if((GETBIT(Prof.Boiler.flags,fTurboBoiler))&&(dRelay[RBOILER].get_present())) dRelay[RBOILER].set_OFF(); // Выключить ТЭН бойлера (режим форсированного нагрева)
                 #endif
                 #ifdef RHEAT
                 if (dRelay[RHEAT].get_present()) dRelay[RHEAT].set_OFF();     // Выключить ТЭН отопления
                 #endif 
                 if (!is_compressor_on())   dFC.set_target(dFC.get_startFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool());   // установить стартовую частоту
                break;
       case  pBOILER:   // Бойлер
                 #ifdef SUPERBOILER                                            // Бойлер греется от предкондесатора
                    dRelay[PUMP_IN].set_ON();                                  // Реле включения насоса входного контура  (геоконтур)
                    dRelay[PUMP_OUT].set_OFF();                                // Евгений добавил
                    dRelay[RSUPERBOILER].set_ON();                             // Евгений добавил
                    _delay(2*1000);                     // Задержка на 2 сек
                    if (!is_compressor_on() && Status.ret<pBp5) dFC.set_target(dFC.get_startFreq(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler());      // В режиме супер бойлер установить частоту SUPERBOILER_FC если не дошли до пида
                 #else  
                    PUMPS_ON;           // включить насосы
                    if (!is_compressor_on() && Status.ret<pBp5) dFC.set_target(dFC.get_startFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler());// установить стартовую частоту
                 #endif

                 #ifdef RTRV
                 if (is_compressor_on()&&(dRelay[RTRV].get_Relay()==true)) ChangesPauseTRV();    // Компрессор рабатает и 4-х ходовой стоит на холоде то хитро переключаем 4-х ходовой в положение тепло
                 dRelay[RTRV].set_OFF();                                        // нагрев
                 _delay(DELAY_AFTER_SWITCH_RELAY);                        // Задержка на сек
                 #endif
                 switchBoiler(true);                                             // включить бойлер
                 #ifdef RHEAT
                 if (dRelay[RHEAT].get_present()) dRelay[RHEAT].set_OFF();     // Выключить ТЭН отопления
                 #endif 
                break;   
         case  pNONE_H:  // Продолжаем греть отопление
         case  pNONE_C:  // Продолжаем охлаждение
         case  pNONE_B:  // Продолжаем греть бойлер
               break;    // конфигурировать ничего не надо, продолжаем движение
         default:  set_Error(ERR_CONFIG,(char*)__FUNCTION__);  break;   // Ошибка!  что то пошло не так
    }
 }

// "Интелектуальная пауза" для перекидывания на "ходу" 4-х ходового
// фактически останов компрессора и обезпечение нужных пауз, компрессор включается далее в vUpdate()
void HeatPump::ChangesPauseTRV()
{
  journal.jprintf("ChangesPauseTRV\n");
  #ifdef EEV_DEF
  dEEV.Pause();                                                    // Поставить на паузу задачу Обновления ЭРВ
  journal.jprintf(" Stop operate EEV\n");
  #endif
  if (is_compressor_on()) {  COMPRESSOR_OFF; stopCompressor=rtcSAM3X8.unixtime(); }                             // Запомнить время выключения компрессора
  #ifdef REVI
   checkEVI();                                                     // выключить ЭВИ
  #endif
    journal.jprintf(" Pause for pressure equalization . . .\n");
  _delay(Option.delayTRV*1000);                                   // Пауза 120 секунд для выравнивания давлений
  #ifdef EEV_DEF  
  lastEEV=dEEV.get_StartPos();                                     // Выставление ЭРВ на стартовую позицию т.к идет смена режима тепло-холод
  #endif
}
// UPDATE --------------------------------------------------------------------------------
// Итерация по управлению всем ТН, для всего, основной цикл управления.
void HeatPump::vUpdate()
{
	
/*  // Защита по протоку переехала в задачу  чтение датчиков а то может быть беда в момент пуска (vUpdate запускается не сразу после включения компрессора)
#ifdef FLOW_CONTROL    // если надо проверяем потоки (защита от отказа насосов) ERR_MIN_FLOW
	if(is_compressor_on())                                                            // Только если компрессор включен
		for(uint8_t i = 0; i < FNUMBER; i++)   // Проверка потока по каждому датчику
			if(sFrequency[i].get_checkFlow() && sFrequency[i].get_Value() < HP.sFrequency[i].get_minValue()) {     // Поток меньше минимального ошибка осанавливаем ТН
				journal.jprintf("Low flow: %.3f\n", (float) sFrequency[i].get_Value() / 1000);
				set_Error(ERR_MIN_FLOW, (char*) sFrequency[i].get_name());
				return;
			}
#endif
*/

#ifdef SEVA  //Если определен лепестковый датчик протока - это переливная схема ТН - надо контролировать проток при работе
 	if(dRelay[RPUMPI].get_Relay())                                                                                             // Только если включен насос геоконтура  (PUMP_IN)
		if (sInput[SEVA].get_Input()==SEVA_OFF) {set_Error(ERR_SEVA_FLOW,(char*)"SEVA"); return;}                              // Выход по ошибке отсутствия протока
#endif

	if((get_State() == pOFF_HP) || (get_State() == pSTARTING_HP) || (get_State() == pSTOPING_HP)) return; // ТН выключен или включается или выключается выходим  ничего не делаем!!!

	// Различные проверки на ошибки и защиты
	if(!CheckAvailableWork()) {  // Нет работы для ТН - ничего не включено, пользователь может изменить в процессе работы
		set_Error(ERR_NO_WORK, (char*) __FUNCTION__);
		return;
	}
#ifdef EEV_DEF
	if((!sADC[PEVA].get_present()) && (dEEV.get_ruleEEV() == TEVAOUT_PEVA)) {  //  Отсутвует датчик давления, и выбран алгоритм ЭРВ который его использует", пользователь может изменить в процессе работы
		set_Error(ERR_PEVA_EEV, dEEV.get_name());
		return;
	}
#endif

#ifdef REVI
	if (dRelay[REVI].get_present()) checkEVI();                           // Проверить необходимость включения ЭВИ
#endif

#ifdef DEFROST
	defrost();                                                          // Разморозка только для воздушных ТН
#endif


//	uint8_t old_mw = Status.modWork;
//	if(old_mw >= pNONE_H) old_mw -= pNONE_H - 1;
	Status.modWork = get_Work();                                         // определяем что делаем
//	if(old_mw != (Status.modWork < pNONE_H ? Status.modWork : Status.modWork - (pNONE_H - 1))) command_completed = rtcSAM3X8.unixtime(); // поменялся режим
#ifdef DEBUG_MODWORK
	save_DumpJournal(false);                                           // Вывод строки состояния
#endif
	//  реализуем требуемый режим
	switch((int) get_modWork()) {
	case pOFF:
		if(is_compressor_on()) {  // ЕСЛИ компрессор работает, то выключить компрессор,и затем сконфигурировать 3 и 4-х клапаны и включаем насосы
			compressorOFF();
			configHP(get_modWork());
			if(!startPump && get_modeHouse() != pOFF)  // Когда режим выключен (не отопление и не охлаждение), то насосы отопления крутить не нужно
			{
				startPump = true;                                 // Поставить признак запуска задачи насос
				journal.jprintf(" %s: Pumps in pause %s. . .\n", (char*) __FUNCTION__, "ON");     // Включить задачу насос кондесатора выключение в переключении насосов
			}
			command_completed = rtcSAM3X8.unixtime(); // поменялся режим
		}
		break;
	case pHEAT:
	case pCOOL:
	case pBOILER: // Включаем задачу насос, конфигурируем 3 и 4-х клапаны включаем насосы и потом включить компрессор
		if(startPump)                                         // Остановить задачу насос
		{
			startPump = false;                                     // Поставить признак останова задачи насос
		    journal.jprintf(" %s: Pumps in pause %s. . .\n",(char*)__FUNCTION__, "OFF");
		    command_completed = rtcSAM3X8.unixtime(); // поменялся режим
		}
		if(!check_compressor_pause()) {
			configHP(get_modWork());                                 // Конфигурируем насосы
			compressorON();                             // Включаем компрессор
		}
		break;
	case pNONE_H:
	case pNONE_C:
	case pNONE_B:
		break;                                    // компрессор уже включен
	default:
		set_Error(ERR_CONFIG, (char*) __FUNCTION__);
		break;
	}
}

// проверка на паузу между включениями
boolean HeatPump::check_compressor_pause()
{
	if(stopCompressor) {
		int32_t nTime = rtcSAM3X8.unixtime() - stopCompressor;
#ifdef DEMO
		if (nTime<10) {journal.jprintf("Compressor pause\n");return true;} // Обеспечение паузы компрессора Хранится в секундах!!! ТЕСТИРОВАНИЕ
#else
		if((nTime = Option.pause - nTime) > 0) {
			#ifdef DEBUG_MODWORK
				if(nTime <= UPDATE_HP_WAIT_PERIOD / 1000) journal.jprintf(" Compressor pause\n");
			#endif
			return true;
		}
#endif
	}
	return false;
}

// Попытка включить компрессор  с учетом всех защит КОНФИГУРАЦИЯ уже установлена
// Вход режим работы ТН
// Возможно компрессор уже включен и происходит только смена режима
const char *EEV_go={" EEV go "};  // экономим место
void HeatPump::compressorON()
{
	if((get_State()==pOFF_HP)||(get_State()==pSTOPING_HP)) return;  // ТН выключен или выключается выходим ничего не делаем!!!

	if(is_compressor_on()) return;                                  // Компрессор уже работает
	if(is_next_command_stop()) {
		journal.jprintf(" Next command stop(%d), skip start", next_command);
		return;
	}

#ifdef EEV_DEF
	if(lastEEV != -1) {         // Не первое включение компрессора после старта ТН
		// 1. Обеспечение минимальной паузы компрессора
		if(check_compressor_pause()) return;
#ifdef DEBUG_MODWORK
		journal.jprintf(pP_TIME,"compressorON > modWork:%d[%s], now %s\n",get_modWork(),codeRet[Status.ret], is_compressor_on() ? "ON" : "OFF");
#endif
	}//get_fEEVStartPosByTemp()
	// 2. Разбираемся с ЭРВ
	journal.jprintf(EEV_go);
	if(dEEV.get_LightStart()) { // Выйти на пусковую позицию
		dEEV.set_EEV(dEEV.get_preStartPos());
		journal.jprintf("preStartPos: %d\n", dEEV.get_preStartPos());
	} else if(dEEV.get_StartFlagPos()) { // Всегда начинать работу ЭРВ со стартовой позиции
		dEEV.set_EEV(dEEV.get_StartPos());
		journal.jprintf("StartPos: %d\n", dEEV.get_EEV());
	} else if(lastEEV != -1) { // установка последнего значения ЭРВ
		dEEV.set_EEV(lastEEV);
		journal.jprintf("lastEEV: %d\n", lastEEV);
	}
	if(lastEEV != -1 && dEEV.get_EevClose()) {        // Если закрывали то пауза для выравнивания давлений
		_delay(dEEV.get_delayOn());  // Задержка на delayOn сек  для выравнивания давлений
	}
	dEEV.CorrectOverheatInit();
	for(uint8_t i = 1; i && dEEV.stepperEEV.isBuzy(); i++) _delay(100); // wait EEV stop
#endif
	

	// 3. Управление компрессором
	if (get_errcode()==OK)                                 // Компрессор включить если нет ошибок
	{
		// Дополнительные защиты перед пуском компрессора
		if (startPump)                                      // Проверка задачи насос - должен быть выключен
		{
			startPump=false;                               // Поставить признак останова задачи насос
			journal.jprintf(" WARNING! %s: Pumps in pause, OFF . . .\n",(char*)__FUNCTION__);
		}
	#ifdef DEFROST
	  if(get_modWork()!=pDEFROST)  // При разморозке есть лишние проверки
	   {
	#endif		
		// Проверка включения насосов с проверкой и предупреждением (этого не должно быть)
		if(!dRelay[PUMP_IN].get_Relay()) {
			journal.jprintf(" WARNING! %s is off before start compressor!\n", dRelay[PUMP_IN].get_name());
			set_Error(ERR_COMP_NO_PUMP, (char*) dRelay[PUMP_IN].get_name());
			return;
		}
#ifndef SUPERBOILER  // для супербойлера это лишнее
		if(!(dRelay[PUMP_OUT].get_Relay()
#ifdef RPUMPBH
				|| dRelay[RPUMPBH].get_Relay()
#endif
		)) {
			journal.jprintf(" WARNING! %s is off before start compressor!\n", dRelay[PUMP_OUT].get_name());
			set_Error(ERR_COMP_NO_PUMP, (char*) dRelay[PUMP_OUT].get_name());
			return;
		}
#endif

#ifdef DEBUG_MODWORK
		journal.jprintf(pP_TIME, "Pause %ds before start compressor\n", Option.delayOnPump);
#endif
		uint16_t d = Option.delayOnPump;
#ifdef FLOW_CONTROL
		for(uint8_t i = 0; i < FNUMBER; i++) sFrequency[i].reset();  // Сброс счетчиков протока
		if(Option.delayOnPump < BASE_TIME_READ + TIME_READ_SENSOR/1000 + 1) d = BASE_TIME_READ + TIME_READ_SENSOR/1000 + 1;
#endif
		for(; d > 0; d--) { // задержка перед включением компрессора
			_delay(1000);
			if(error || is_next_command_stop()) return; // прерваться по ошибке, еще бы проверить команду на останов...
		}

#ifdef FLOW_CONTROL      // если надо проверяем потоки (защита от отказа насосов) ERR_MIN_FLOW
		for(uint8_t i = 0; i < FNUMBER; i++) {   // Проверка потока по каждому датчику
		#ifdef SUPERBOILER   // Если определен супер бойлер
			#ifdef FLOWCON   // если определен датчик потока конденсатора
			   if ((i==FLOWCON)&&(!dRelay[RPUMPO].get_Relay())) continue; // Для режима супербойлер есть вариант когда не будет протока по контуру отопления
			#endif
		#endif	
			 if(sFrequency[i].get_checkFlow() && sFrequency[i].get_Value() < HP.sFrequency[i].get_minValue()) {  // Поток меньше минимального
				_delay(TIME_READ_SENSOR);
				if(sFrequency[i].get_Value() < HP.sFrequency[i].get_minValue()) {  // Поток меньше минимального
					journal.jprintf(" Flow %s: %.3f\n", sFrequency[i].get_name(), (float)sFrequency[i].get_Value()/1000.0);
					set_Error(ERR_MIN_FLOW, (char*) sFrequency[i].get_name());
					return;
				}
			}
		}	
#endif

#ifdef DEFROST
   }  // if(mod!=pDEFROST)
#endif	
		resetPID(); 										// Инициализировать переменные ПИД регулятора
#ifdef CHART_ONLY_COMP_ON  // Накопление точек для графиков ТОЛЬКО если компрессор работает
		task_updstat_chars = 0;
#endif
	    command_completed = rtcSAM3X8.unixtime();
	  	COMPRESSOR_ON;                                      // Включить компрессор
		if(error || dFC.get_err()) return; // Ошибка - выходим
		startCompressor=rtcSAM3X8.unixtime();   // Запомнить время включения компрессора оно используется для задержки работы ПИД ЭРВ! должно быть перед  vTaskResume(xHandleUpdateEEV) или  dEEV.Resume
	} else { // if (get_errcode()==OK)
		journal.jprintf(" EEV not set before start compressor!\n");
		set_Error(ERR_COMP_ERR,(char*)__FUNCTION__);return;
	}

	// 4. Если нужно облегченный пуск  в зависимости от флага fEEV_light_start
#ifdef EEV_DEF
	if(dEEV.get_LightStart())                  //  ЭРВ ОБЛЕГЧЕННЫЙ ПУСК
	{
		journal.jprintf(" Pause %d second before go starting position EEV . . .\n", dEEV.get_DelayStartPos());
		_delay(dEEV.get_DelayStartPos() * 1000);  // Задержка после включения компрессора до ухода на рабочую позицию
		journal.jprintf(EEV_go);
		if((dEEV.get_StartFlagPos()) || ((lastEEV == -1))) {
			dEEV.set_EEV(dEEV.get_StartPos());
			journal.jprintf("StartPos: %d\n", dEEV.get_EEV());
		}    // если первая итерация или установлен соответсвующий флаг то на стартовую позицию
		else { // установка последнего значения ЭРВ в противном случае
			dEEV.set_EEV(lastEEV);
			journal.jprintf("lastEEV: %d\n", lastEEV);
		}
	}
#endif
	// 5. Обеспечение задержки отслеживания ЭРВ
#ifdef EEV_DEF
	if (lastEEV>0)                                            // НЕ первое включение компрессора после старта ТН
	{
		dEEV.Resume();
		vTaskResume(xHandleUpdateEEV);                               // Запустить задачу Обновления ЭРВ
		journal.jprintf(" Resume task UpdateEEV\n");
		#ifdef DEFROST
		 if(get_modWork()!=pDEFROST) journal.jprintf(pP_TIME,"%s WORK . . .\n",(char*)nameHeatPump);     // Сообщение о работе
		 else journal.jprintf(pP_TIME,"%s DEFROST . . .\n",(char*)nameHeatPump);               // Сообщение о разморозке
		#else
		journal.jprintf(pP_TIME,"%s WORK . . .\n",(char*)nameHeatPump);     // Сообщение о работе
		#endif
	}
	else  // признак первой итерации
	{
		set_startTime(rtcSAM3X8.unixtime());                         // Запомнить время старта ТН
		lastEEV=dEEV.get_EEV();                                 // ЭРВ рабоатет запомнить
		dEEV.Resume();
		vTaskResume(xHandleUpdateEEV);                               // Запустить задачу Обновления ЭРВ
		journal.jprintf(" Start task UpdateEEV\n");
	}
#else
	lastEEV=1;                                                   // Признак первой итерации
	set_startTime(rtcSAM3X8.unixtime());                         // Запомнить время старта ТН
#endif
}

// попытка выключить компрессор  с учетом всех защит
const char *MinPauseOnCompressor={" Wait min pause on compressor . . ."};  
void HeatPump::compressorOFF()
{
  if(!dFC.isfOnOff()) return;
   
  #ifdef EEV_DEF
  lastEEV=dEEV.get_EEV();                                             // Запомнить последнюю позицию ЭРВ
  dEEV.Pause();                                                       // Поставить на паузу задачу Обновления ЭРВ
  journal.jprintf(" Stop control EEV\n");
  #endif
  
  command_completed = rtcSAM3X8.unixtime();
  COMPRESSOR_OFF;                                                     // Компрессор выключить
  stopCompressor=rtcSAM3X8.unixtime();                                // Запомнить время выключения компрессора
  
  
  #ifdef REVI
      checkEVI();                                                     // выключить ЭВИ
  #endif

  PUMPS_OFF;                                                          // выключить насосы + задержка
  
  #ifdef EEV_DEF
  if( dEEV.get_EevClose())                                 // Hазбираемся с ЭРВ
     { 
     journal.jprintf(" Pause before closing EEV %d sec . . .\n",dEEV.get_delayOff());
     _delay(dEEV.get_delayOff()*1000);                                // пауза перед закрытием ЭРВ  на инверторе компрессор останавливается до 2 минут
     dEEV.set_EEV(EEV_CLOSE_STEP);                                    // Если нужно, то закрыть ЭРВ
     journal.jprintf(" EEV closed\n");
     } 
  #endif
  
  //journal.jprintf(pP_TIME,"%s PAUSE . . .\n",(char*)nameHeatPump);    // Сообщение о паузе
}

// РАЗМОРОЗКА ВОЗДУШНИКА ----------------------------------------------------------
// Все что касается разморозки воздушника. Функция работает следющим образом: пока не закончена разморозка из функции нет выхода, т.е. она автономна
#ifdef DEFROST
#define TEMP_NO_DEFROST    600   // температура выше которой разморозка не включается
#define TEMP_STEAM_DEFROST 200   // температура ниже которой оттаиваем паром
#define TEMP_END_DEFROST   1500  // температура окончания отттайки
void HeatPump::defrost()
{
      if (get_State()==pOFF_HP) return;                                    // если ТН не работает то выходим
      
      #ifdef RTRV            // Нет четырехходового - нет режима охлаждения
  if (dRelay[RTRV].get_Relay() == false) return;                          // режим охлаждения - размораживать не надо, у меня false это охлаждение.
#else
 set_Error(ERR_DEFROST_RTRV, (char*)__FUNCTION__);return;                 // Четырех ходового нет разморозка не возможна - это косяк конфигурации
      #endif
         
// Реализовано два алгоритма выбор по наличию SFROZEN
#ifdef SFROZEN    // Алгоритм разморозки по датчику  SFROZEN             
      if (sInput[SFROZEN].get_Input()==SFROZEN_OFF) {startDefrost=0;return;  }    // размораживать не надо - датчик говорит что все ок
      
      // организация задержки перед включением
      if (startDefrost==0) startDefrost=xTaskGetTickCount();               // первое срабатывание датчика - запоминаем время (тики)
      if (xTaskGetTickCount()-startDefrost<Option.delayDefrostOn*1000)  return; //  Еще рано размораживать
      // придется размораживать
       journal.jprintf("Start defrost\n"); 
       #ifdef RTRV
         if (is_compressor_on()&&(dRelay[RTRV].get_Relay()==false)) ChangesPauseTRV();    // Компрессор рабатает и 4-х ходовой стоит на тепле то хитро переключаем 4-х ходовой в положение холод
         dRelay[RTRV].set_ON();                                              // охлаждение
         _delay(2*1000);                               // Задержка на 2 сек
       #endif
       
       compressorON();                                                 // включить компрессор на холод
      
      while (sInput[SFROZEN].get_Input()!=SFROZEN_OFF)                     // ждем оттаивания
      {
      _delay(10*1000);                              // Задержка на 10 сек
        journal.jprintf(" Wait process defrost . . .\n"); 
        if((get_State()==pOFF_HP)||(get_State()==pSTARTING_HP)||(get_State()==pSTOPING_HP)) break;     // ТН выключен или включается или выключается выходим из разморозки
      }
#else          // Алгоритм разморозки по температуре
  if (sTemp[TOUT].get_Temp()>TEMP_NO_DEFROST) return;                      // Если температура на улице выше TEMP_NO_DEFROST ТН не обмерзает
  if ((is_compressor_on()) && (rtcSAM3X8.unixtime()-startCompressor<15*60)) return;   // компрессор работает, но прошло менее 15 минут - размораживать не надо
  if (sTemp[TEVAIN].get_Temp()-sTemp[TOUT].get_Temp()>-1200) {startDefrost=0;return;  }   // размораживать не надо - условие не наступило
  journal.jprintf("Next step, defrost . . .\n");
  // организация задержки перед включением
  if (startDefrost==0) startDefrost=xTaskGetTickCount();                    // первое срабатывание датчика - запоминаем время (тики)// Наступило условие, средняя температура испарителя ниже воздуха, более чем 15С - запоминаем время (тики)
  if (xTaskGetTickCount()-startDefrost<Option.delayDefrostOn*125)  return;  // Еще рано размораживать
  journal.jprintf("Start defrost\n");                                       // Пошла оттайка
	 // Дальше в зависимости от работает компрессор или нет - это главное условие
	  if (is_compressor_on())
	  {
	    if (dRelay[RTRV].get_Relay() == true) // Если четырехходовой стоит на тепло - Главный вариант
	    {
		     ChangesPauseTRV();                                                     // Компрессор рабатает и 4-х ходовой стоит на тепле то хитро переключаем 4-х ходовой в положение холод
		     if (sTemp[TOUT].get_Temp()<=TEMP_STEAM_DEFROST)                        // Если температура на улице ниже или равна TEMP_STEAM_DEFROST то оттаиваем паром 	
			     {
			      dRelay[PUMP_IN].set_OFF();                                        // выключаем вентиляторы
			     _delay(1*1000);
			      dRelay[PUMP_IN1].set_OFF();                                       // выключаем вентиляторы	
			     }
		    }
	    else  { set_Error(ERR_DEFROST_RTRV, (char*)__FUNCTION__);return; }           // ХА ХА  Работаем на охлаждение но при этом требуется разморозка - Это косяк Карл, но до сюда не доедит
	  	
	  }
	  else  // Компрессор не работает - тяжелый случай все делаем руками
	  {
	   dRelay[RTRV].set_OFF();                                                       // переключаем 4ходовик на оттайку
	   _delay(1*1000);
	   dRelay[PUMP_OUT].set_OFF();                                                   // Включение насоса выходного контура
		 if (sTemp[TOUT].get_Temp()>TEMP_STEAM_DEFROST)                              // Если температура на улице ниже или равна TEMP_STEAM_DEFROST то оттаиваем паром 	
		 {
		  dRelay[PUMP_IN].set_ON();                                                  // включаем вентиляторы
		 _delay(1*1000);
		  dRelay[PUMP_IN1].set_ON();                                                 // включаем вентиляторы	
		 }
	      compressorON(pDEFROST);                                                    // включить компрессор на разморозку
	  } // Компрессор не рабоатет
	
	 // ТН в нужном состояниии надо только ждать 
	 while (sTemp[TEVAOUT].get_Temp()<TEMP_END_DEFROST)                     // Ждем оттаивания, поднятия температууры выхода испарителя. Испаритель и конденсатор меняются местами?
		{
		  _delay(10*1000);                                                      // Задержка на 10 сек
		  journal.jprintf(" Wait process HEAT GAS defrost . . .\n");
		  if (error ||(get_State() == pOFF_HP) || (get_State() == pSTARTING_HP) || (get_State() == pSTOPING_HP)) break; // ТН выключен или включается или выключается выходим из разморозки
		}
#endif // #ifdef SFROZEN 
    // Завершение
      journal.jprintf(" Finish defrost, wait delayDefrostOff min.\n"); 
	compressorOFF();                                                         // выключить компрессор   Пока пусть будет так, в далнейшем надо дорабоать
	_delay(Option.delayDefrostOff*1000);                                     // Задержка путь стекут остатки воды    
   journal.jprintf("Finish defrost\n");                                      // выходим ТН сам определит что надо делать
     
}
#endif

// ОБРАБОТЧИК КОМАНД УПРАВЛЕНИЯ ТН
// Послать команду на управление ТН
void HeatPump::sendCommand(TYPE_COMMAND c)
{
	if (c == command) return;      // Игнорируем повторы
	if (command != pEMPTY) // Если команда выполняется (не pEMPTY), то следующую в очередь, если есть место
	{
		if(next_command != c){
			next_command = c;
			journal.jprintf("Active command: %s, next: %s\n", get_command_name(command), get_command_name(next_command));
		}
		return;
	}
	if ((c==pSTART)&&(get_State()==pSTOPING_HP)) return;    // Пришла команда на старт а насос останавливается ничего не делаем игнорируем
	command=c;
	vTaskResume(xHandleUpdateCommand);                   	// Запустить выполнение команды
}  
// Выполнить команду по управлению ТН true-команда выполнена
int8_t HeatPump::runCommand()
{
	uint16_t i;
	while(1) {
		journal.jprintf("Run command: %s\n", get_command_name(command));

		HP.PauseStart = 0;                    // Необходимость начать задачу xHandlePauseStart с начала
		switch(command)
		{
		case pEMPTY:  return true; break;     // 0 Команд нет
		case pSTART:                          // 1 Пуск теплового насоса
			num_repeat=0;           // обнулить счетчик повторных пусков
			StartResume(_start);    // включить ТН
			break;
		case pAUTOSTART:                      // 2 Пуск теплового насоса автоматический
			StartResume(_start);    // включить ТН
			break;
		case pSTOP:                           // 3 Стоп теплового насоса
			StopWait(_stop);        // Выключить ТН
			break;
		case pRESET:                          // 4 Сброс контроллера
			StopWait(_stop);        // Выключить ТН
			journal.jprintf("$SOFTWARE RESET control . . .\n\n");
			save_motoHour();
			Stats.SaveStats(0);
			Stats.SaveHistory(0);
			_delay(500);            // задержка что бы вывести сообщение в консоль
			Software_Reset() ;      // Сброс
			break;
		case pREPEAT:
			if(NO_Power) { // Нет питания - ожидание
				NO_Power = 2;
				goto xWait;
			}
			StopWait(_stop);                            // Попытка запустит ТН (по числу пусков)
			num_repeat++;                               // увеличить счетчик повторов пуска ТН
			journal.jprintf("Repeat start %s (attempts remaining %d) . . .\n",(char*)nameHeatPump,get_nStart()-num_repeat);
			PauseStart = 1;   							// Запустить выполнение отложенного старта
			break;
		case pRESTART:
			// Stop();                                          // пуск Тн после сброса - есть задержка
			journal.jprintf("Restart %s . . .\n",(char*)nameHeatPump);
			PauseStart = 1;									// Запустить выполнение отложенного старта
			break;
		case pNETWORK:
			_delay(1000);               						// задержка что бы вывести сообщение в консоль и на веб морду
			if(SemaphoreTake(xWebThreadSemaphore,(W5200_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE) {journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexWebThreadBuzy); command=pEMPTY; return 0;} // Захват мютекса потока или ОЖИДАНИНЕ W5200_TIME_WAIT
			initW5200(true);                                  // Инициализация сети с выводом инфы в консоль
			for (i=0;i<W5200_THREAD;i++) SETBIT1(Socket[i].flags,fABORT_SOCK);                                 // Признак инициализации сокета, надо прерывать передачу в сервере
			SemaphoreGive(xWebThreadSemaphore);                                                                // Мютекс потока отдать
			break;
//		case pSFORMAT:                                             // Форматировать журнал в I2C памяти
//			#ifdef I2C_EEPROM_64KB
//				_delay(2000);              				           // задержка что бы вывести сообщение в консоль и на веб морду
//				Stats.Format();                                    // Послать команду форматирование статистики
//			#endif
//			break;
		case pSAVE:                                                      // Сохранить настройки
			_delay(2000);              				      		 // задержка что бы вывести сообщение в консоль и на веб морду
			save();                                            // сохранить настройки
			break;
		case pWAIT:   // Перевод в состяние ожидания  - особенность возможна блокировка задач - используем семафор
xWait:
			if(SemaphoreTake(xCommandSemaphore,(60*1000/portTICK_PERIOD_MS))==pdPASS)    // Cемафор  захвачен ОЖИДАНИНЕ ДА 60 сек
			{
				Task_vUpdate_run = false;					      // Остановить задачу обновления ТН vUpdate (xHandleUpdate)
				StopWait(_wait);                                  // Ожидание
				SemaphoreGive(xCommandSemaphore);                 // Семафор отдать
				Task_vUpdate_run = true;
				vTaskResume(xHandleUpdate);
			}
			else  journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexCommandBuzy);
			break;
		case pRESUME:   // Восстановление работы после ожиданияя -особенность возможна блокировка задач - используем семафор
			if(SemaphoreTake(xCommandSemaphore,(60*1000/portTICK_PERIOD_MS))==pdPASS)    // Cемафор  захвачен ОЖИДАНИНЕ ДА 60 сек
			{
				Task_vUpdate_run = false;					      // Остановить задачу обновления ТН vUpdate (xHandleUpdate)
				StartResume(_resume);                             // восстановление ТН
				SemaphoreGive(xCommandSemaphore);                 // Семафор отдать
				Task_vUpdate_run = true;
				vTaskResume(xHandleUpdate);
			}
			else  journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexCommandBuzy);
			break;

		default:                                                         // Не известная команда
			journal.jprintf("Unknown command: %d !!!", command);
			break;
		}
		if(command != pSFORMAT && command != pSAVE && command != pNETWORK) command_completed = rtcSAM3X8.unixtime();
		if(next_command != pEMPTY) { // следующая команда
			command = next_command;
			next_command = pEMPTY;
			_delay(1);
		} else {
			command=pEMPTY;   // Сбросить команду
			break;
		}
	}
	return error;
}

// Возвращает 1, если ТН в паузе
uint8_t HeatPump::is_pause()
{
	return ((get_State() == pWORK_HP && (get_modWork() == pOFF || !is_compressor_on())) || get_State() == pWAIT_HP);
}

// --------------------------Строковые функции ----------------------------
const char *strRusPause={"Пауза"};
const char *strEngPause={"Pause"};
// Получить строку состояния ТН в виде строки
char *HeatPump::StateToStr()
{
	switch ((int)get_State())  //TYPE_STATE_HP
	{
	case pOFF_HP:     return (char*)(HP.PauseStart == 0 ? "Выключен" : "Перезапуск...");  break;   // 0 ТН выключен или Перезапуск
	case pSTARTING_HP:return (char*)"Пуск...";   break;         // 1 Стартует
	case pSTOPING_HP: return (char*)"Останов...";break;         // 2 Останавливается
	case pWORK_HP:                                              // 3 Работает
		if(!is_compressor_on()) {
			switch ((int)get_modWork()) {                       // MODE_HP
			case  pHEAT:   return (char*)"Ожид. Нагр.";         // 1 Включить отопление
			case  pCOOL:   return (char*)"Ожид. Охл.";          // 2 Включить охлаждение
			case  pBOILER: return (char*)"Ожид. ГВС";           // 3 Включить бойлер
			}
			return (char*)strRusPause;
		} else {
			switch ((int)get_modWork()) {                       // MODE_HP
			case  pOFF:    return (char*)strRusPause;   break;  // 0 Пауза
			case  pHEAT:   return (char*)"Нагрев";      break;  // 1 Включить отопление
			case  pCOOL:   return (char*)"Заморозка";   break;  // 2 Включить охлаждение
			case  pBOILER: return (char*)"Нагрев ГВС";  break;  // 3 Включить бойлер
			case  pNONE_H: return (char*)"Отопление";   break;  // 4 Продолжаем греть отопление
			case  pNONE_C: return (char*)"Охлаждение";  break;  // 5 Продолжаем охлаждение
			case  pNONE_B: return (char*)"ГВС";         break;  // 6 Продолжаем греть бойлер
			case  pDEFROST:return (char*)"Разморозка";  break;  // 7 Разморозка
			}
			return (char*)"Статус Р.?";
		}
	case pWAIT_HP:    return (char*)"Ожидание";  break;         // 4 Ожидание
	case pERROR_HP:   return (char*)"Ошибка";    break;         // 5 Ошибка ТН
	}
	return (char*)"Статус ?"; 							        // 6 - Эта ошибка возникать не должна!
}

// Получить строку состояния ТН в виде строки АНГЛИСКИЕ буквы
char *HeatPump::StateToStrEN()
{
	switch ((int)get_State())  //TYPE_STATE_HP
	{
	case pOFF_HP:     return (char*)(HP.PauseStart == 0 ? "Off" : "Restart...");  break;   // 0 ТН выключен или Перезапуск
	case pSTARTING_HP:return (char*)"Start...";   break;         // 1 Стартует
	case pSTOPING_HP: return (char*)"Stop...";    break;         // 2 Останавливается
	case pWORK_HP:                                               // 3 Работает
		if(!is_compressor_on()) {
			switch ((int)get_modWork()) {                       // MODE_HP
			case  pHEAT:   return (char*)"Wait Heat";          // 1 Включить отопление
			case  pCOOL:   return (char*)"Wait Cool";          // 2 Включить охлаждение
			case  pBOILER: return (char*)"Wait Boiler";        // 3 Включить бойлер
			}
			return (char*)strRusPause;
		} else {
			switch ((int)get_modWork()) {                       // MODE_HP
			case  pOFF:    return (char*)strEngPause;   break;  // 0 Пауза
			case  pHEAT:   return (char*)"Heat";        break;  // 1 Включить отопление
			case  pCOOL:   return (char*)"Cool";        break;  // 2 Включить охлаждение
			case  pBOILER: return (char*)"Boiler";      break;  // 3 Включить бойлер
			case  pNONE_H: return (char*)"Heating";     break;  // 4 Продолжаем греть отопление
			case  pNONE_C: return (char*)"Cooling";     break;  // 5 Продолжаем охлаждение
			case  pNONE_B: return (char*)"Boiler";      break;  // 6 Продолжаем греть бойлер
			case  pDEFROST:return (char*)"Defrost";     break;  // 7 Разморозка
			}
			return (char*)"Work ?";
		}
	case pWAIT_HP:    return (char*)"Wait";  break;         // 4 Ожидание
	case pERROR_HP:   return (char*)"Error"; break;       // 5 Ошибка ТН
	}
	return (char*)"Status ?"; 							   // 6 - Эта ошибка возникать не должна!
}

// получить режим тестирования
char * HeatPump::TestToStr()
{
 switch ((int)get_testMode())
             {
              case NORMAL:    return (char*)"NORMAL";    break;
              case SAFE_TEST: return (char*)"SAFE_TEST"; break;
              case TEST:      return (char*)"TEST";      break;
              case HARD_TEST: return (char*)"HARD_TEST"; break;
              default:        return (char*)cError;     break;    
             }    
}
// Записать состояние теплового насоса в журнал
// Параметр на входе true - вывод в журнал и консоль false - консоль
int8_t HeatPump::save_DumpJournal(boolean f)
{
  uint8_t i;
  if(f)  // вывод в журнал
      {
        journal.jprintf(" modWork:%d[%s]",(int)get_modWork(),codeRet[Status.ret]); 
        for(i = 0; i < RNUMBER; i++) journal.jprintf(" %s:%d", HP.dRelay[i].get_name(), HP.dRelay[i].get_Relay());
        if(dFC.get_present())               journal.jprintf(" freqFC:%.2f",dFC.get_frequency()/100.0);
        if(dFC.get_present())               journal.jprintf(" Power:%.3f",dFC.get_power()/1000.0);
        #ifdef EEV_DEF
        if (dEEV.get_present())             journal.jprintf(" EEV:%d",dEEV.get_EEV());
        #endif
         journal.jprintf(cStrEnd);
         // Доп инфо
        for(i=0;i<TNUMBER;i++)   // Информация по  датчикам температуры
             if (sTemp[i].get_present() && sTemp[i].Chart.get_present()) journal.jprintf(" %s:%.2f",sTemp[i].get_name(),sTemp[i].get_Temp()/100.0);
        if (sADC[PEVA].get_present())         journal.jprintf(" PEVA:%.2f",sADC[PEVA].get_Press()/100.0); 
        if (sADC[PCON].get_present())         journal.jprintf(" PCON:%.2f",sADC[PCON].get_Press()/100.0);  
        journal.jprintf(cStrEnd);
      }
   else
     {
        journal.printf(" modWork:%d[%s]",(int)get_modWork(),codeRet[Status.ret]); 
        for(i = 0; i < RNUMBER; i++) journal.printf(" %s:%d", HP.dRelay[i].get_name(), HP.dRelay[i].get_Relay());
 //      Serial.print(" dEEV.stepperEEV.isBuzy():");  Serial.print(dEEV.stepperEEV.isBuzy());
 //      Serial.print(" dEEV.setZero: ");  Serial.print(dEEV.setZero);  
        if(dFC.get_present()) journal.printf(" freqFC:%.2f",dFC.get_frequency()/100.0);
        if(dFC.get_present()) journal.printf(" Power:%.3f",dFC.get_power()/1000.0);
        #ifdef EEV_DEF
        journal.printf(" EEV:%d",dEEV.get_EEV()); 
        #endif
        journal.printf(cStrEnd);
                 
     }
  return OK;
}

// Температура конденсации
#define MAGIC_CONST_CONDENS 200   // Магическая поправка для перевода температуры выхода конденсатора по гликолю  в температуру конденсации   
int16_t HeatPump::get_temp_condensing(void)
{
#ifdef EEV_DEF  
	if(sADC[PCON].get_present()) {
		return PressToTemp(sADC[PCON].get_Press(), dEEV.get_typeFreon());
	} else {
		return sTemp[get_modeHouse()  == pCOOL ? TEVAOUTG : TCONOUTG].get_Temp() + MAGIC_CONST_CONDENS; // +2C
	}
#else
  return sTemp[get_modeHouse()  == pCOOL ? TEVAOUTG : TCONOUTG].get_Temp() + MAGIC_CONST_CONDENS; // +2C 
#endif
}

// Переохлаждение
int16_t HeatPump::get_overcool(void)
{
	if(get_modeHouse() == pCOOL) {
#ifdef TCONOUT
		return get_temp_condensing() - sTemp[TEVAIN].get_Temp();
#else
	return 0;
#endif
	} else {
#ifdef TCONOUT
		return get_temp_condensing() - sTemp[TCONOUT].get_Temp();
#else
	return 0;
#endif
	}
}

// Кипение
int16_t HeatPump::get_temp_evaporating(void)
{
#ifdef EEV_DEF
	if(sADC[PEVA].get_present()) {
		return PressToTemp(sADC[PEVA].get_Press(), dEEV.get_typeFreon());
	} else {
		return 0; // Пока не поддерживается
	}
#else
 return 0;  // ЭРВ нет
#endif
}

// Возвращает 0 - Нет ошибок или ни одного активного датчика, 1 - ошибка, 2 - превышен предел ошибок
int8_t	 HeatPump::Prepare_Temp(uint8_t bus)
{
	int8_t i, ret = 0;
  #ifdef ONEWIRE_DS2482_SECOND
	if(bus == 1) i = OneWireBus2.PrepareTemp();
	else
  #endif
  #ifdef ONEWIRE_DS2482_THIRD
	if(bus == 2) i = OneWireBus3.PrepareTemp();
	else
  #endif
  #ifdef ONEWIRE_DS2482_FOURTH
	if(bus == 3) i = OneWireBus4.PrepareTemp();
	else
  #endif
	i = OneWireBus.PrepareTemp();
	if(i) {
		for(uint8_t j = 0; j < TNUMBER; j++) {
			if(sTemp[j].get_fAddress() && sTemp[j].get_bus() == bus) {
				if(sTemp[j].inc_error()) {
					ret = 2;
					break;
				}
				ret = 1;
			}
		}
		if(ret) {
			journal.jprintf(pP_TIME, "Error %d PrepareTemp bus %d\n", i, bus+1);
			if(ret == 2) set_Error(i, (char*) __FUNCTION__);
		}
	}
	return ret ? (1<<bus) : 0;
}

// Обновление расчетных величин мощностей и СОР
void HeatPump::calculatePower()
{
// Мощности контуров	
#ifdef  FLOWCON 
	if(sTemp[TCONING].get_present() & sTemp[TCONOUTG].get_present()) powerCO = (float) (FEED-RET) * sFrequency[FLOWCON].get_Value() / sFrequency[FLOWCON].get_kfCapacity();
#ifdef RHEAT_POWER   // Для Дмитрия. его специфика Вычитаем из общей мощности системы отопления мощность электрокотла
#ifdef RHEAT
	if (dRelay[RHEAT].get_Relay()) powerCO=powerCO-RHEAT_POWER;  // если включен электрокотел
#endif
#endif
#else
	powerCO=0.0;
#endif

#ifdef  FLOWEVA 
	if(sTemp[TEVAING].get_present() & sTemp[TEVAOUTG].get_present()) powerGEO = (float) (sTemp[TEVAING].get_Temp()-sTemp[TEVAOUTG].get_Temp()) * sFrequency[FLOWEVA].get_Value() / sFrequency[FLOWEVA].get_kfCapacity();
#else
	powerGEO=0.0;
#endif

#ifndef COP_ALL_CALC    // если КОП надо считать не всегда То отбрасываем отрицательные мощности, это переходные процессы, возможно это надо делать всегда
if (powerCO<0) powerCO=0;
if (powerGEO<0) powerGEO=0;
#endif

// Получение мощностей потребления электроэнергии
COP = dFC.get_power();  // получить текущую мощность компрессора 
#ifdef USE_ELECTROMETER_SDM  // Если есть электросчетчик можно рассчитать полное потребление (с насосами)
    if (dSDM.get_link()){  // Если счетчик работает (связь не утеряна)
	power220 = dSDM.get_Power();
	#ifdef CORRECT_POWER220
		for(uint8_t i = 0; i < sizeof(correct_power220)/sizeof(correct_power220[0]); i++) if(dRelay[correct_power220[i].num].get_Relay()) power220 += correct_power220[i].value;
	#endif
    } else power220=0; // свзяи со счетчиком нет
#else
   power220=0; // электросчетчика нет
#endif

// Расчет КОП
#ifndef COP_ALL_CALC    // если КОП надо считать не всегда 
if(is_compressor_on()){      // Если компрессор рабоатет
#endif	
	if(COP>0) COP = powerCO / COP * 100; else COP=0; // ЧИСТЫЙ КОП в сотых долях !!!!!!
	if(power220 != 0) fullCOP = powerCO / power220 * 100; else fullCOP = 0; // ПОЛНЫЙ КОП в сотых долях !!!!!!
		#ifndef COP_ALL_CALC        // Ограничение переходных процессов для варианта расчета КОП только при работающем компрессоре, что бы графики нормально масштабировались
		if(COP>10*100) COP=10*100;  // КОП не более 10
		if(fullCOP>8*100) COP=8*100;// полный КОП не более 8
		#endif
#ifndef COP_ALL_CALC   // если КОП надо считать не всегда 
} else { COP=0; fullCOP=0; }  // компрессор не рабоатет
#endif
}

void HeatPump::Sun_ON(void)
{
#ifdef USE_SUN_COLLECTOR
	if(time_Sun_OFF == 0 || millis() - time_Sun_OFF > SUN_MIN_PAUSE) { // ON
		flags |= (1<<fHP_SunActive);
		dRelay[RSUN].set_Relay(fR_StatusSun);
		dRelay[PUMP_IN].set_Relay(fR_StatusSun);
		time_Sun_ON = millis();
		time_Sun_OFF = 0;
	}
#endif
}

void HeatPump::Sun_OFF(void)
{
#ifdef USE_SUN_COLLECTOR
	if(flags & (1<<fHP_SunActive)) {
		dRelay[RSUN].set_Relay(-fR_StatusSun);
		dRelay[PUMP_IN].set_Relay(-fR_StatusSun);
		flags &= ~(1<<fHP_SunActive);
		time_Sun_ON = 0;
		time_Sun_OFF = millis();
	}
#endif
}

// Уравнение ПИД регулятора в конечных разностях.
// errorPid - Ошибка ПИД = (Цель - Текущее состояние)  в СОТЫХ
// pid - настройки ПИДа в тысячных
// sum, pre_err - сумма для расчета и предыдущая ошибка
// Выход управляющее воздействие (в СОТЫХ)
int16_t updatePID(int32_t errorPid, PID_STRUCT &pid, PID_WORK_STRUCT &pidw)
{
	int32_t newVal;
#ifdef DEBUG_PID
	journal.printf("PID(%x): %d,%d,S:%d(%d,%d,%d). ", &pid, errorPid, pidw.pre_err, pidw.sum, pid.Kp, pid.Ki, pid.Kd);
#endif
#ifdef PID_FORMULA2
	pidw.sum += pid.Ki * errorPid;
	if(pidw.PropOnMeasure) {
		pidw.sum -= pid.Kp * (pidw.pre_err - errorPid);
		newVal = 0;
#ifdef DEBUG_PID
		journal.printf("P:%d,", -pid.Kp * (pidw.pre_err - errorPid));
#endif
	} else {
		newVal = pid.Kp * errorPid;
#ifdef DEBUG_PID
		journal.printf("P:%d,", pid.Kp * errorPid);
#endif
	}
#ifdef DEBUG_PID
	journal.printf("I:%d,", pid.Ki * errorPid);
#endif
	if(pidw.sum > pidw.max) pidw.sum = pidw.max;
	else if(pidw.sum < pidw.min) pidw.sum = pidw.min;
	newVal += pidw.sum - pid.Kd * (pidw.pre_err - errorPid);
	//проверка на ограничения не здесь
	//if(newVal > pidw.max) newVal = pidw.max;
	//else if(newVal < pidw.min) newVal = pidw.min;
#ifdef DEBUG_PID
	journal.printf("D=%d,Sum(%d)=%d\n", -pid.Kd * (pidw.pre_err - errorPid), pidw.sum, newVal);
#endif
#else  // Алгоритм 1 Классический ПИД
	// Cp, Ci, Cd – коэффициенты дискретного ПИД регулятора;
	// u(t) = P (t) + I (t) + D (t);
	// P (t) = Kp * e (t);
	// I (t) = I (t — 1) + Ki * e (t);
	// D (t) = Kd * {e (t) — e (t — 1)};
	// T – период дискретизации(период, с которым вызывается ПИД регулятор).
	if(pid.Ki > 0)// Расчет интегральной составляющей
	{
		pidw.sum += (int32_t) pid.Ki * errorPid;     // Интегральная составляющая, с накоплением, в СТО ТЫСЯЧНЫХ (градусы 100 и интегральный коэффициент 1000)
		if(pidw.sum > pidw.max) pidw.sum = pidw.max; // Ограничение диапазона изменения ПИД интегральной составляющей, произведение в СТО ТЫСЯЧНЫХ
		else if(pidw.sum < -pidw.max) pidw.sum = -pidw.max;
	} else pidw.sum = 0;              // если Кi равен 0 то интегрирование не используем
	newVal = pidw.sum;
//	if (abs(pidw.sum)>pidw.max) pidw.sum=0; // Сброс интегральной составляющей при достижении максимума
#ifdef DEBUG_PID
	journal.printf("I:%d,", newVal);
#endif
	// Пропорциональная составляющая
	if(abs(errorPid) < pidw.Kp_dmin) newVal += (int32_t) abs(errorPid) * pid.Kp * errorPid / pidw.Kp_dmin; // Вблизи уменьшить воздействие
	else newVal += (int32_t) pid.Kp * errorPid;
#ifdef DEBUG_PID
	journal.printf("P:%d,", newVal);
#endif
	// Дифференцальная составляющая
	newVal += (int32_t) pid.Kd * (pidw.pre_err - errorPid);// ДЕСЯТИТЫСЯЧНЫЕ Положительная составляющая - ошибка растет (воздействие надо увеличиить)  Отрицательная составляющая - ошибка уменьшается (воздействие надо уменьшить)
	if ((abs(newVal)>pidw.max)&&(pidw.max>0)) pidw.sum=0; // Сброс интегральной составляющей при движении на один шаг 
#ifdef DEBUG_PID
	journal.printf("+D:%d=%d\n", pid.Kd * (pidw.pre_err - errorPid), newVal);
#endif
#endif
	pidw.pre_err = errorPid; // запомнить предыдущую ошибку
	newVal = round_div_int32(newVal, 1000); // Учесть разрядность коэффициентов (ТЫСЯЧНЫЕ), выход в СОТЫХ
	if(newVal > 32767) newVal = 32767; else if(newVal < -32767) newVal = -32767; // фикс переполнения
	return newVal;
}

#ifdef PID_FORMULA2
void UpdatePIDbyTime(uint16_t new_time, uint16_t curr_time, PID_STRUCT &pid)
{
	if(new_time) {
		pid.Ki = (int32_t) pid.Ki * new_time / curr_time;
		pid.Kd = (int32_t) pid.Kd * curr_time / new_time;
	}
}
#else
void UpdatePIDbyTime(uint16_t, uint16_t, PID_STRUCT &) {}
#endif
