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
#define COMPRESSOR_ON   { if(dFC.get_present()) dFC.start_FC(); else dRelay[RCOMP].set_ON(); startCompressor = rtcSAM3X8.unixtime(); }  // Включить компрессор в зависимости от наличия инвертора
#define COMPRESSOR_OFF  { if(dFC.get_present()) dFC.stop_FC(); else dRelay[RCOMP].set_OFF(); stopCompressor = rtcSAM3X8.unixtime(); } // Выключить компрессор в зависимости от наличия инвертора

//struct size
//char checker(int); char checkSizeOfInt1[sizeof(HP.Option)]={checker(&checkSizeOfInt1)};

// Установка критической ошибки для класса ТН вызывает останов ТН
// Возвращает ошибку останова ТН
void set_Error(int8_t _err, char *nam)
{
	if(HP.is_compressor_on())    // СРАЗУ Если компрессор включен, выключить  ГЛАВНАЯ ЗАЩИТА
	{ // Выключить компрессор для обоих вариантов
		journal.jprintf("$Compressor protection ");
#ifdef FC_USE_RCOMP // Использовать отдельный провод для команды ход/стоп
		HP.dRelay[RCOMP].set_OFF();
#else
#ifdef MODBUS_PORT_NUM
		if(HP.dFC.write_0x06_16(FC_CONTROL, FC_C_STOP) == OK) // подать команду ход/стоп через модбас
#endif
#endif
			HP.set_stopCompressor();
	}
	//   if ((HP.get_State()==pOFF_HP)&&(HP.error!=OK)) return HP.error;  // Если ТН НЕ работает, не стартует не останавливается и уже есть ошибка то останавливать нечего и выключать нечего выходим - ошибка не обновляется - важна ПЕРВАЯ ошибка

	if(HP.error == OK) {
		HP.error = _err;
		strcpy(HP.source_error, nam);
		strcpy(HP.note_error, NowTimeToStr());       // Cтереть всю строку и поставить время
		strcat(HP.note_error, " ");
		strcat(HP.note_error, nam);                  // Имя кто сгенерировал ошибку
		strcat(HP.note_error, ": ");
		strcat(HP.note_error, noteError[abs(_err)]); // Описание ошибки
		journal.jprintf_time("$ERROR source: %s, code: %d\n", nam, _err); //journal.jprintf(", code: %d\n",_err);
		if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) HP.save_DumpJournal(true); // вывод отладочной информации для начала  если запущена freeRTOS
		HP.message.setMessage(pMESSAGE_ERROR, HP.note_error, 0);    // сформировать уведомление об ошибке
		// Сюда ставить надо останов ТН !!!!!!!!!!!!!!!!!!!!!
		HP.process_error();
	}
}

void HeatPump::process_error(void)
{
	if(get_State() != pOFF_HP)    // Насос не ВЫКЛЮЧЕН есть что выключать
	{
		if(get_nStart() == 0) sendCommand(pSTOP); // Послать команду на останов ТН, если нет попыток повторного пуска
		else { // сюда ставить повторные пуски ТН при ошибке.
#ifdef NOT_RESTART_ON_CRITICAL_ERRORS
			for(uint8_t i; i < sizeof(CRITICAL_ERRORS)/sizeof(CRITICAL_ERRORS[0]); i++) {
				if(CRITICAL_ERRORS[i] == error) sendCommand(pSTOP);
				goto xExit;
			}
#endif
			if(num_repeat < get_nStart())                    // есть еще попытки
			{
				sendCommand(pREPEAT);                     // Повторный пуск ТН
			} else sendCommand(pSTOP);                    // Послать команду на останов ТН  БЕЗ ПОПЫТОК ПУСКА
		}
#ifdef NOT_RESTART_ON_CRITICAL_ERRORS
xExit:
#endif
		if(get_State() == pSTARTING_HP) { // Ошибка во время старта
			set_HP_error_state();
		}
	}
}

void HeatPump::initHeatPump()
{
	uint8_t i;
	NO_Power = 0;
	fBackupPowerOffDelay = 0;
	flags = (1<<fHP_SunNotInited);
	eraseError();

	for(i = 0; i < TNUMBER; i++) sTemp[i].initTemp(i);            // Инициализация датчиков температуры

#ifdef SENSOR_IP
	for(i=0;i<IPNUMBER;i++) sIP[i].initIP(i);               // Инициализация удаленных датчиков
#endif
	sADC[PEVA].initSensorADC(PEVA, ADC_SENSOR_PEVA, FILTER_SIZE);          // Инициализация аналогового датчика PEVA
	sADC[PCON].initSensorADC(PCON, ADC_SENSOR_PCON, FILTER_SIZE);          // Инициализация аналогового датчика TCON
#ifdef PGEO
	sADC[PGEO].initSensorADC(PGEO, ADC_SENSOR_PGEO, FILTER_SIZE_OTHER);			// Инициализация аналогового датчика PGEO
#endif
#ifdef POUT
	sADC[POUT].initSensorADC(POUT, ADC_SENSOR_POUT, FILTER_SIZE_OTHER);			// Инициализация аналогового датчика POUT
#endif
#ifdef IWR
	sADC[IWR].initSensorADC(IWR, ADC_SENSOR_IWR, FILTER_SIZE_OTHER);			// Инициализация аналогового датчика POUT
#endif

	for(i = 0; i < INUMBER; i++) sInput[i].initInput(i);           // Инициализация контактных датчиков
	for(i = 0; i < FNUMBER; i++) sFrequency[i].initFrequency(i);  // Инициализация частотных датчиков
	for(i = 0; i < RNUMBER; i++) dRelay[i].initRelay(i);           // Инициализация реле

#ifdef EEV_DEF
	dEEV.initEEV();                                           // Инициализация ЭРВ
#endif

	// Инициалаизация модбаса  перед частотником и счетчиком
	journal.jprintf("Init Modbus RTU via RS485:");
	if(Modbus.initModbus() == OK) journal.jprintf(" OK\r\n");  //  выводим сообщение об установлении связи
	else {
		journal.jprintf(" not present config\r\n");
	}         //  нет в конфигурации

	dFC.initFC();                                              // Инициализация FC
#ifdef USE_ELECTROMETER_SDM
	dSDM.initSDM();                                            // инициалаизация счетчика
#endif
	message.initMessage(MAIN_WEB_TASK);                        // Инициализация Уведомлений, параметр - номер потока сервера в котором идет отправка
#ifdef MQTT
	clMQTT.initMQTT(MAIN_WEB_TASK);                            // Инициализация MQTT, параметр - номер потока сервера в котором идет отправка
#endif

	// Графики в памяти
	for(i = 0; i < sizeof(Charts) / sizeof(Charts[0]); i++) Charts[i].init();
	clearChart();

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
  case pOFF_HP:       Status.State=pOFF_HP; if(GETBIT(motoHour.flags,fMH_ON))   {SETBIT0(motoHour.flags,fMH_ON);save_motoHour();}  break;  // 0 ТН выключен, при необходимости записываем в ЕЕПРОМ
  case pSTARTING_HP:  Status.State=pSTARTING_HP; break;                                                                                    // 1 Стартует
  case pSTOPING_HP:   Status.State=pSTOPING_HP;  break;                                                                                    // 2 Останавливается
  case pWORK_HP:      Status.State=pWORK_HP;if(!(GETBIT(motoHour.flags,fMH_ON))) {SETBIT1(motoHour.flags,fMH_ON);save_motoHour();}  break; // 3 Работает, при необходимости записываем в ЕЕПРОМ
  case pWAIT_HP:      Status.State=pWAIT_HP;if(!(GETBIT(motoHour.flags,fMH_ON))) {SETBIT1(motoHour.flags,fMH_ON);save_motoHour();}  break; // 4 Ожидание, при необходимости записываем в ЕЕПРОМ
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
		journal.jprintf("Scan 1-Wire:\n");
//		char *_result_str = result_str + m_strlen(result_str);
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
		journal.jprintf("Found: %d\n", OW_scanTableIdx);
//		while(strlen(_result_str)) {
//			journal.jprintf(_result_str);
//			uint16_t l = strlen(_result_str);
//			_result_str += l > PRINTF_BUF-1 ? PRINTF_BUF-1 : l;
//		}
#ifdef RADIO_SENSORS
		journal.jprintf("Radio found(%d): ", radio_received_num);
		for(uint8_t i = 0; i < radio_received_num; i++) {
			OW_scanTable[OW_scanTableIdx].num = OW_scanTableIdx + 1;
			OW_scanTable[OW_scanTableIdx].bus = tRadio_Bus;
			memset(&OW_scanTable[OW_scanTableIdx].address, 0, sizeof(OW_scanTable[0].address));
			OW_scanTable[OW_scanTableIdx].address[0] = tRadio;
			memcpy(&OW_scanTable[OW_scanTableIdx].address[1], &radio_received[i].serial_num, sizeof(radio_received[0].serial_num));
			char *p = result_str + strlen(result_str);
			m_snprintf(p, 64, "%d:RADIO %.1dV/%c:%.2d:%u:%d;", OW_scanTable[OW_scanTableIdx].num, radio_received[i].battery, Radio_RSSI_to_Level(radio_received[i].RSSI), radio_received[i].Temp, radio_received[i].serial_num, tRadio_Bus+1);
			journal.jprintf("%s", p);
			if(++OW_scanTableIdx >= OW_scanTable_max) break;
		}
		journal.jprintf("\n");
#endif
#ifdef TNTC
		journal.jprintf("NTC found: ");
		for(uint8_t i = 0; i < TNTC; i++) {
			if(TNTC_Value[i] > TNTC_Value_Max) continue;
			OW_scanTable[OW_scanTableIdx].num = OW_scanTableIdx + 1;
			OW_scanTable[OW_scanTableIdx].bus = tADC_Bus;
			memset(&OW_scanTable[OW_scanTableIdx].address, 0, sizeof(OW_scanTable[0].address));
			OW_scanTable[OW_scanTableIdx].address[0] = tADC;
			OW_scanTable[OW_scanTableIdx].address[1] = '0' + i;
			char *p = result_str + strlen(result_str);
			m_snprintf(p, 64, "%d:NTC:%.2d:AD%d:%d;", OW_scanTable[OW_scanTableIdx].num, sTemp->Read_NTC(TNTC_Value[i]), TADC[i], tADC_Bus+1);
			journal.jprintf("%s", p);
			if(++OW_scanTableIdx >= OW_scanTable_max) break;
		}
		journal.jprintf("\n");
#endif
#ifdef TNTC_EXT

#endif
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

// Получить источник загрузки веб морды
TYPE_SOURSE_WEB HeatPump::get_SourceWeb()
{
	if(get_WebStoreOnSPIFlash()) {
		switch (get_fSPIFlash())
		{
		case 0:
			if(get_fSD() == 2) return pSD_WEB;
			break;
		case 2:
			return pFLASH_WEB;
		}
		return pMIN_WEB;
	} else {
		switch (get_fSD())
		{
		case 0:
			if(get_fSPIFlash() == 2) return pFLASH_WEB;
			break;
		case 2:
			return pSD_WEB;
		}
		return pMIN_WEB;
	}
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
	if(error == ERR_SAVE_EEPROM || error == ERR_LOAD_EEPROM || error == ERR_CRC16_EEPROM) error = OK;
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
		#ifdef WATTROUTER
		if(save_2bytes(addr, SAVE_TYPE_Wattrouter, crc)) break;
		if(save_struct(addr, (uint8_t*)&WR, sizeof(WR), crc)) break; // Сохранение WR
		#endif
		if(save_2bytes(addr, SAVE_TYPE_END, crc)) break;
		if(writeEEPROM_I2C(addr, (uint8_t *) &crc, sizeof(crc))) { error = ERR_SAVE_EEPROM; break; } // CRC
		addr = addr + sizeof(crc) - (I2C_SETTING_EEPROM + 2);
		if(writeEEPROM_I2C(I2C_SETTING_EEPROM, (uint8_t *) &addr, 2)) { error = ERR_SAVE_EEPROM; break; } // size all
		int8_t _err;
		if((_err = check_crc16_eeprom(I2C_SETTING_EEPROM + 2, addr - 2)) != OK) {
			error = _err;
			journal.jprintf("- Verify Error!\n");
			break;
		}
		addr += 2;
		journal.jprintf("OK, wrote: %d bytes, crc: %04x\n", addr, crc);
		break;
	}
	if(tasks_suspended) xTaskResumeAll(); // Разрешение других задач

	if(error == ERR_SAVE_EEPROM || error == ERR_LOAD_EEPROM || error == ERR_CRC16_EEPROM) {
		set_Error(error, (char*)__FUNCTION__);
		return error;
	}
	// суммарное число байт
	return addr;
}

// Считать настройки из памяти i2c или из RAM, если не NULL, на выходе длина или код ошибки (меньше нуля)
// flag: b1 - из памяти, b2 - не проверять CRC
int32_t HeatPump::load(uint8_t *buffer, uint8_t flag)
{
	uint16_t size;
	journal.jprintf(" Load settings from ");
	if(!(flag & 1)) {
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
	uint16_t crc = 0xFFFF;
#ifdef LOAD_VERIFICATION
	if(flag & 2) {
		journal.jprintf("*SKIP*");
	} else {
		for(uint16_t i = 0; i < size; i++)  crc = _crc16(crc, buffer[i]);
		if(crc != *((uint16_t *)(buffer + size))) {
			journal.jprintf("Error: %04x != %04x!\n", crc, *((uint16_t *)(buffer + size)));
			return error = ERR_CRC16_EEPROM;
		}
		journal.jprintf("%04x", crc);
	}
#else
	journal.jprintf("*No verification*");
#endif
	uint8_t *buffer_max = buffer + size;
	size += 2;
	load_struct(&Option, &buffer, sizeof(Option));
	journal.jprintf(", v.%d ", Option.ver);  // вывести версию сохранения
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
			if(n < ANUMBER) { load_struct(sADC[n].get_save_addr(), &buffer, sADC[n].get_save_size()); sADC[n].after_load(); } else goto xSkip;
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
#ifdef WATTROUTER
		} else if(type == SAVE_TYPE_Wattrouter) {
			load_struct((uint8_t*)&WR, &buffer, sizeof(WR)); WR_Loads = WR.Loads;
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
//	if(Option.ver <= 133) {
//#ifdef USE_ELECTROMETER_SDM
//		if(dSDM.get_readState(3) == OK) {
//			motoHour.E1 = (dSDM.get_Energy() - motoHour.E1_f) * 1000;
//			motoHour.E2 = (dSDM.get_Energy() - motoHour.E2_f) * 1000;
//		} else
//#endif
//		{
//			motoHour.E1 = 0;
//			motoHour.E2 = 0;
//		}
//	}
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
	motoHour.Header = I2C_COUNT_EEPROM_HEADER;
	uint32_t ptr = 0;
	do {
		while(ptr < sizeof(motoHour)) if(((uint8_t*)&motoHour)[ptr] == ((uint8_t*)&motoHour_saved)[ptr]) ptr++; else break;
		if(ptr == sizeof(motoHour)) break;
		uint32_t ptre = ptr + 1;
xNotEqual:
		while(ptre < sizeof(motoHour)) if(((uint8_t*)&motoHour)[ptre] != ((uint8_t*)&motoHour_saved)[ptre]) ptre++; else break;
		if(ptre < sizeof(motoHour)) {
			uint32_t ptrz = ptre + 1;
			while(ptrz < sizeof(motoHour)) if(((uint8_t*)&motoHour)[ptrz] == ((uint8_t*)&motoHour_saved)[ptrz]) ptrz++; else break;
			if(ptrz < sizeof(motoHour)) {
				if(ptrz - ptre <= 4) {
					ptre = ptrz + 1;
					goto xNotEqual;
				}
			}
		}
		uint8_t errcode;
		if((errcode = writeEEPROM_I2C(I2C_COUNT_EEPROM + ptr, (byte*)&motoHour + ptr, ptre - ptr))) {
			journal.jprintf(" ERROR %d save counters!\n", errcode);
			set_Error(ERR_SAVE2_EEPROM, (char*) __FUNCTION__);
			return ERR_SAVE2_EEPROM;
		}
		ptr = ptre;
	} while(ptr < sizeof(motoHour));
	memcpy(&motoHour_saved, &motoHour, sizeof(motoHour_saved));
	return OK;
}

// чтение счетчиков теплового насоса в ЕЕПРОМ
int8_t HeatPump::load_motoHour()          
{
	if(readEEPROM_I2C(I2C_COUNT_EEPROM, &motoHour.Header, sizeof(motoHour.Header))) { // прочитать заголовок
		set_Error(ERR_LOAD2_EEPROM, (char*) __FUNCTION__);
		return ERR_LOAD2_EEPROM;
	}
	if(motoHour.Header != I2C_COUNT_EEPROM_HEADER) { // заголовок плохой
		journal.jprintf("Bad header counters, skip load\n");
		return ERR_HEADER2_EEPROM;
	}
	if(readEEPROM_I2C(I2C_COUNT_EEPROM + sizeof(motoHour.Header), (byte*) &motoHour + sizeof(motoHour.Header), sizeof(motoHour) - sizeof(motoHour.Header))) { // прочитать счетчики
		set_Error(ERR_LOAD2_EEPROM, (char*) __FUNCTION__);
		return ERR_LOAD2_EEPROM;
	}
	if(Option.ver <= 138) {
		memcpy(&motoHour_saved, &motoHour, sizeof(motoHour_saved));
		type_motoHour_old *p = (type_motoHour_old*)&motoHour_saved;
		motoHour.D1 = p->D1;
		motoHour.D2 = p->D2;
		motoHour.E1 = p->E1;
		motoHour.E2 = p->E2;
		motoHour.P1 = p->P1;
		motoHour.P2 = p->P2;
	}
 /* 
   // Код можно убрать в июле 2020 когда все перейдут 
   // Восстановление значений счетчиков, требуется при переходе с 1.043 на 1.053 версию от 06.01.2020, 
   // в старой версии записываем значения и потом здесь вводим, после обновления, этот фрагмент кода удаляем 
   // перевод дата в юникс формат https://www.cy-pr.com/tools/time/
   motoHour.D1 =       1510999774;  // Дата сброса общих счетчиков
   motoHour.D2 =       1570437710;  // дата сброса сезонных счетчиков
   motoHour.H1 =       18745.4*60;  // моточасы ТН ВСЕГО в минутах (часы умножаем на 60)
   motoHour.H2 =        2405.8*60;  // моточасы ТН сбрасываемый счетчик (сезон)
   motoHour.C1 =        8901.0*60;  // моточасы компрессора ВСЕГО
   motoHour.C2 =        1791.8*60;  // моточасы компрессора сбрасываемый счетчик (сезон)
   motoHour.E1 =  7102.33*1000000;  // Значение потребленной энергии ВСЕГО (кВт*ч умножаем на 1 млн.)
   motoHour.E2 =   699.36*1000000;  // Значение потребленной энергии в начале сезона 
   motoHour.P1 = 30317.47*1000000;  // выработанное тепло  ВСЕГО
   motoHour.P2 =  5304.80*1000000;  // выработанное тепло  сбрасываемый счетчик (сезон)
   save_motoHour();
 */ 	
	memcpy(&motoHour_saved, &motoHour, sizeof(motoHour_saved));
	journal.printf(" Load counters OK, read: %d bytes\n", sizeof(motoHour));
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
		motoHour.P1 = 0;
		motoHour.E1 = 0;
		motoHour.D1 = rtcSAM3X8.unixtime();           // Дата сброса общих счетчиков
	}
	// Сезон
	motoHour.H2 = 0;
	motoHour.C2 = 0;
	motoHour.P2 = 0;
	motoHour.E2 = 0;
	motoHour.D2 = rtcSAM3X8.unixtime();             // дата сброса сезонных счетчиков
	save_motoHour();  // записать счетчики
	motohour_OUT_work = 0;
	motohour_IN_work = 0;
}

// Обновление счетчиков моточасов, вызывается раз в минуту
// Электрическая энергия не обновляется, Тепловая энергия обновляется
void HeatPump::updateCount()
{
	if(is_compressor_on()) {
		motoHour.C1++;      // моточасы компрессора ВСЕГО
		motoHour.C2++;      // моточасы компрессора сбрасываемый счетчик (сезон)
	}
	if(get_State() == pWORK_HP) {
		motoHour.H1++;          // моточасы ТН ВСЕГО
		motoHour.H2++;          // моточасы ТН сбрасываемый счетчик (сезон)
	}
	int32_t p;
	//taskENTER_CRITICAL();
	p = motohour_IN_work;
	motohour_IN_work = 0;
	//taskEXIT_CRITICAL();
	motoHour.E1 += p;
	motoHour.E2 += p;
	//taskENTER_CRITICAL();
	p = motohour_OUT_work;
	motohour_OUT_work = 0;
	//taskEXIT_CRITICAL();
	motoHour.P1 += p;
	motoHour.P2 += p;
}

// После любого изменения часов необходимо пересчитать все времна которые используются
// параметр изменение времени - корректировка
void HeatPump::updateDateTime(int32_t dTime)
{
	if(dTime != 0 && dTime != int32_t(0x80000000))                                   // было изменено время, надо скорректировать переменные времени
	{
		Prof.SaveON.startTime = Prof.SaveON.startTime + dTime; // время пуска ТН (для организации задержки включения включение ЭРВ)
		if(timeON > 0) timeON = timeON + dTime;                               // время включения контроллера для вычисления UPTIME
		if(startCompressor > 0) startCompressor = startCompressor + dTime;             // время пуска компрессора
		if(stopCompressor > 0) stopCompressor = stopCompressor + dTime;               // время останова компрессора
		if(countNTP > 0) countNTP = countNTP + dTime;                           // число секунд с последнего обновления по NTP
		if(offBoiler > 0) offBoiler = offBoiler + dTime;                         // время выключения нагрева ГВС ТН (необходимо для переключения на другие режимы на ходу)
		if(startDefrost > 0) startDefrost = startDefrost + dTime;                   // время срабатывания датчика разморозки
		if(startSalmonella > 0) startSalmonella = startSalmonella + dTime;             // время начала обеззараживания
#ifdef WATTROUTER
		if(WR_LastSwitchTime) WR_LastSwitchTime += dTime;
		for(uint8_t i = 0; i < WR_NumLoads; i++) {
			if(WR_SwitchTime[i]) WR_SwitchTime[i] += dTime;
		}
#endif
	}
}
      

// -------------------------------------------------------------------------
// НАСТРОЙКИ ТН ------------------------------------------------------------
// -------------------------------------------------------------------------
// Сброс настроек теплового насоса
void HeatPump::resetSettingHP()
{  
	Prof.initProfile();                           // Инициализировать профиль по умолчанию

	Status.modWork = pOFF;                          // Что сейчас делает ТН (7 стадий)
	Status.State = pOFF_HP;                         // Сотояние ТН - выключен
	Status.ret = pNone;                             // точка выхода алгоритма
	motoHour.Header = I2C_COUNT_EEPROM_HEADER;
	motoHour.flags = 0;								// насос выключен
	motoHour.H1 = 0;                                // моточасы ТН ВСЕГО
	motoHour.H2 = 0;                                // моточасы ТН сбрасываемый счетчик (сезон)
	motoHour.C1 = 0;                                // моточасы компрессора ВСЕГО
	motoHour.C2 = 0;                                // моточасы компрессора сбрасываемый счетчик (сезон)
	motoHour.E1 = 0.0;                              // Значение потреленный энергии в момент пуска  актуально при использовании счетчика SDM120 (вычитаем текущее и получам итого)
	motoHour.E2 = 0.0;                              // Значение потреленный энергии в начале сезона актуально при использовании счетчика SDM120 (вычитаем текущее и получам итого)
	motoHour.D1 = motoHour.D2 = rtcSAM3X8.unixtime(); // Дата сброса счетчиков

	startPump = false;                              // Признак работы задачи насос
	flagRBOILER = false;                            // не идет нагрев бойлера
	HeatBoilerUrgently = 0;
	fSD = false;                                    // СД карта не рабоатет
	fSPIFlash = false;                              // Признак наличия (физического) spi диска - диска нет по умолчанию

	startWait = false;                              // Начало работы с ожидания
	onBoiler = false;                               // Если true то идет нагрев бойлера
	onSalmonella = false;                           // Если true то идет Обеззараживание
	command = pEMPTY;                               // Команд на выполнение нет
	next_command = pEMPTY;
	PauseStart = 0;                                 // начать отсчет задержки пред стартом с начала
	startRAM = 0;                                   // Свободная память при старте FREE Rtos - пытаемся определить свободную память при работе
	lastEEV = -1;                                   // значение шагов ЭРВ перед выключением  -1 - первое включение
	num_repeat = 0;                                 // текушее число попыток 0 - т.е еще не было работы
	num_resW5200 = 0;                               // текущее число сбросов сетевого чипа
	num_resMutexSPI = 0;                            // текущее число сброса митекса SPI
	num_resMutexI2C = 0;                            // текущее число сброса митекса I2C
	num_resMQTT = 0;                                // число повторных инициализация MQTT клиента
	num_resPing = 0;                                // число не прошедших пингов

	fullCOP = -1000;                                // Полный СОР  сотые -1000 признак невозможности расчета
//	COP = -1000;                                    // Чистый COP сотые  -1000 признак невозможности расчета

	// Инициализациия различных времен
	DateTime.saveTime = 0;                          // дата и время сохранения настроек в eeprom
	timeON = 0;                                     // время включения контроллера для вычисления UPTIME
	countNTP = 0;                                   // число секунд с последнего обновления по NTP
	startCompressor = 0;                            // время пуска компрессора
	stopCompressor = 0;                             // время останова компрессора
	offBoiler = 0;                                  // время выключения нагрева ГВС ТН (необходимо для переключения на другие режимы на ходу)
	startDefrost = 0;                               // время срабатывания датчика разморозки
	timeNTP = 0;                                    // Время обновления по NTP в тиках (0-сразу обновляемся)
	startSalmonella = 0;                            // время начала обеззараживания
	command_completed = 0;
	time_Sun = 0;
	compressor_in_pause = false;

	safeNetwork = false;                            // режим safeNetwork

	// Установка сетевых параметров по умолчанию
	if(defaultDHCP) SETBIT1(Network.flags, fDHCP);
	else SETBIT0(Network.flags, fDHCP); // использование DHCP
	Network.ip = IPAddress(defaultIP);              // ip адрес
	Network.sdns = IPAddress(defaultSDNS);          // сервер dns
	Network.gateway = IPAddress(defaultGateway);    // шлюз
	Network.subnet = IPAddress(defaultSubnet);      // подсеть
	Network.port = defaultPort;                     // порт веб сервера по умолчанию
	memcpy(Network.mac, defaultMAC, 6);             // mac адрес
	Network.resSocket = 30;                         // Время очистки сокетов
	Network.resW5200 = 0;                           // Время сброса чипа
	countResSocket = 0;                             // Число сбросов сокетов
	SETBIT1(Network.flags, fInitW5200);           // Ежеминутный контроль SPI для сетевого чипа
	SETBIT0(Network.flags, fPass);                 // !save! Использование паролей
	strcpy(Network.passUser, "user");              // !save! Пароль пользователя
	strcpy(Network.passAdmin, "admin");            // !save! Пароль администратора
	Network.sizePacket = 1465;                      // !save! размер пакета для отправки
	SETBIT0(Network.flags, fNoAck);                // !save! флаг Не ожидать ответа ACK
	Network.delayAck = 10;                          // !save! задержка мсек перед отправкой пакета
	strcpy(Network.pingAdr, PING_SERVER);         // !save! адрес для пинга
	Network.pingTime = 60 * 60;                       // !save! время пинга в секундах
	SETBIT0(Network.flags, fNoPing);               // !save! Запрет пинга контроллера

	// Время
	SETBIT1(DateTime.flags, fUpdateNTP);           // Обновление часов по NTP
	SETBIT1(DateTime.flags, fUpdateI2C);           // Обновление часов I2C

	strcpy(DateTime.serverNTP, (char*) NTP_SERVER);  // NTP сервер по умолчанию

	// Опции теплового насоса
	// Временные задержки
	Option.ver = VER_SAVE;
	Option.delayOnPump = DEF_DELAY_ON_PUMP;
	Option.delayOffPump = DEF_DELAY_OFF_PUMP;
	Option.delayStartRes = DEF_DELAY_START_RES;
	Option.delayRepeadStart = DEF_DELAY_REPEAD_START;
	Option.delayDefrostOn = DEF_DELAY_DEFROST_ON;
	Option.delayDefrostOff = DEF_DELAY_DEFROST_OFF;
	Option.delayR4WAY = DEF_DELAY_R4WAY;
	Option.delayBoilerSW = DEF_DELAY_BOILER_SW;
	Option.delayBoilerOff = DEF_DELAY_BOILER_OFF;
	Option.numProf = Prof.get_idProfile(); //  Профиль не загружен по дефолту 0 профиль
	Option.nStart = 3;                   //  Число попыток пуска компрессора
	Option.tempRHEAT = 1000;             //  Значение температуры для управления RHEAT (по умолчанию режим резерв - 10 градусов в доме)
	Option.pausePump = 600;              //  Время паузы  насоса при выключенном компрессоре, сек
	Option.workPump = 15;                //  Время работы  насоса при выключенном компрессоре, сек
	Option.tChart = 10;                  //  период накопления статистики по умолчанию 60 секунд
	SETBIT0(Option.flags, fAddHeat);      //  Использование дополнительного тэна при нагреве НЕ ИСПОЛЬЗОВАТЬ
	SETBIT0(Option.flags, fTypeRHEAT);    //  Использование дополнительного тэна по умолчанию режим резерв
	SETBIT1(Option.flags, fBeep);         //  Звук
	SETBIT1(Option.flags, fNextion);      //  дисплей Nextion
	SETBIT0(Option.flags, fHistory);      //  Сброс статистика на карту
	SETBIT0(Option.flags, fSaveON);       //  флаг записи в EEPROM включения ТН
	Option.sleep = 5;                    //  Время засыпания минуты
	Option.dim = 80;                     //  Якрость %
	Option.pause = 5 * 60;               // Минимальное время простоя компрессора, секунды
#ifdef USE_SUN_COLLECTOR
	Option.SunTDelta = SUN_TDELTA;
	Option.SunGTDelta = SUNG_TDELTA;
	Option.SunMinWorktime = SUN_MIN_WORKTIME;
	Option.SunMinPause = SUN_MIN_PAUSE;
#endif
    SETBIT0(Option.flags, fBackupPower); // Использование резервного питания от генератора (ограничение мощности)
	Option.maxBackupPower=3000;          // Максимальная мощность при питании от генератора (Вт)
#ifdef WATTROUTER
	WR.MinNetLoad = 50;
	WR.NextSwitchPause = 10;
	WR.TurnOnMinTime = 9;
	WR.TurnOnPause = 300;
	WR.LoadAdd = 150;
	WR.LoadHist = 100;
	WR.PWM_Freq = PWM_WRITE_OUT_FREQ_DEFAULT;
	WR.WF_Hour = 5;
#endif

}

// --------------------------------------------------------------------
// ФУНКЦИИ РАБОТЫ С НАСТРОЙКАМИ ТН ------------------------------------
// --------------------------------------------------------------------
// Сетевые настройки --------------------------------------------------
//Установить параметр из строки
boolean HeatPump::set_network(char *var, char *c)
{ 
 int32_t x = atoi(c);
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
 if(strcmp(var,net_PASS)==0){        if (x == 0) { SETBIT0(Network.flags,fPass); return true;}
                                    else if (x == 1) {SETBIT1(Network.flags,fPass);  return true;}
                                    else return false;  
                                    }else
 if(strcmp(var,net_PASSUSER)==0){    strncpy(Network.passUser,c, PASS_LEN);set_hashUser(); return true;   }else
 if(strcmp(var,net_PASSADMIN)==0){   strncpy(Network.passAdmin,c, PASS_LEN);set_hashAdmin(); return true; }else
 if(strcmp(var, net_fWebLogError) == 0) { Network.flags = (Network.flags & ~(1<<fWebLogError)) | ((x == 1)<<fWebLogError); return true; } else
 if(strcmp(var, net_fWebFullLog) == 0) { Network.flags = (Network.flags & ~(1<<fWebFullLog)) | ((x == 1)<<fWebFullLog); return true; } else
 if(strcmp(var,net_SIZE_PACKET)==0){
                                    if((x<64)||(x>2048)) return   false;
                                    else Network.sizePacket=x; return true;
                                    }else  
 if(strcmp(var,net_INIT_W5200)==0){    // флаг Ежеминутный контроль SPI для сетевого чипа
                       if (x == 0) { SETBIT0(Network.flags,fInitW5200); return true;}
                       else if (x == 1) { SETBIT1(Network.flags,fInitW5200);  return true;}
                       else return false;  
                       }else 
 if(strcmp(var,net_PORT)==0){
                       if((x<1)||(x>65535)) return false;
                       else Network.port=x; return  true;
                       }else     
 if(strcmp(var,net_NO_ACK)==0){      if (x == 0) { SETBIT0(Network.flags,fNoAck); return true;}
                       else if (x == 1) { SETBIT1(Network.flags,fNoAck);  return true;}
                       else return false;  
                       }else  
 if(strcmp(var,net_DELAY_ACK)==0){
                       if((x<1)||(x>50)) return        false;
                       else Network.delayAck=x; return  true;
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
 if(strcmp(var,net_NO_PING)==0){     if (x == 0) { SETBIT0(Network.flags,fNoPing);      pingW5200(get_NoPing()); return true;}
                       else if (x == 1) { SETBIT1(Network.flags,fNoPing); pingW5200(get_NoPing()); return true;}
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
	 	return web_fill_tag_select(ret, "never:0;6 hours:0;24 hours:0;",
					Network.resW5200 == 0 ? 0 :
					Network.resW5200 == 60*60*6 ? 1 :
					Network.resW5200 == 60*60*24 ? 2 : 3);
  }else if(strcmp(var,net_PASS)==0){      if (GETBIT(Network.flags,fPass)) return  strcat(ret,(char*)cOne);
                                    else      return  strcat(ret,(char*)cZero);               }else
  if(strcmp(var, net_fWebLogError) == 0) { return strcat(ret, (char*)(Network.flags & (1<<fWebLogError) ? cOne : cZero)); } else
  if(strcmp(var, net_fWebFullLog) == 0) { return strcat(ret, (char*)(Network.flags & (1<<fWebFullLog) ? cOne : cZero)); } else
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
boolean HeatPump::set_datetime(char *var, char *c)
{
	int16_t buf[3];
	uint32_t oldTime = rtcSAM3X8.unixtime(); // запомнить время
	int32_t dTime = 0;
	if(strcmp(var, time_TIME) == 0) {
		if(!parseInt16_t(c, ':', buf, 2, 10)) return false;
		rtcSAM3X8.set_time(buf[0], buf[1], 0);  // внутренние
		setTime_RtcI2C(rtcSAM3X8.get_hours(), rtcSAM3X8.get_minutes(), rtcSAM3X8.get_seconds()); // внешние
		dTime = rtcSAM3X8.unixtime() - oldTime; // получить изменение времени
	} else if(strcmp(var, time_DATE) == 0) {
		uint8_t i = 0, f = 0;
		char ch;
		do { // ищем разделитель чисел
			ch = c[i++];
			if(ch >= '0' && ch <= '9') f = 1;
			else if(f == 1) {
				f = 2;
				break;
			}
		} while(ch != '\0');
		if(f != 2 || !parseInt16_t(c, ch, buf, 3, 10)) return false;
		rtcSAM3X8.set_date(buf[0], buf[1], buf[2]); // внутренние
		setDate_RtcI2C(rtcSAM3X8.get_days(), rtcSAM3X8.get_months(), rtcSAM3X8.get_years()); // внешние
		dTime = rtcSAM3X8.unixtime() - oldTime; // получить изменение времени
	} else if(strcmp(var, time_NTP) == 0) {
		if(strlen(c) == 0) return false;                                                 // пустая строка
		if(strlen(c) > NTP_SERVER_LEN) return false;                                     // слишком длиная строка
		else {
			strcpy(DateTime.serverNTP, c);
			return true;
		}                           // ок сохраняем
	} else if(strcmp(var, time_UPDATE) == 0) {
		if(strcmp(c, cZero) == 0) {
			SETBIT0(DateTime.flags, fUpdateNTP);
			return true;
		} else if(strcmp(c, cOne) == 0) {
			SETBIT1(DateTime.flags, fUpdateNTP);
			countNTP = 0;
			return true;
		} else return false;
	} else if(strcmp(var, time_UPDATE_I2C) == 0) {
		if(strcmp(c, cZero) == 0) {
			SETBIT0(DateTime.flags, fUpdateI2C);
			return true;
		} else if(strcmp(c, cOne) == 0) {
			SETBIT1(DateTime.flags, fUpdateI2C);
			countNTP = 0;
			return true;
		} else return false;
	} else return false;

	updateDateTime(dTime);    // было изменено время, надо скорректировать переменные времени
	return true;
}

// Получить параметр дата и время из строки
void HeatPump::get_datetime(char *var, char *ret)
{
	if(strcmp(var, time_TIME) == 0) {
		ret += strlen(ret);
		NowTimeToStr(ret);
		ret[5] = '\0';
	} else if(strcmp(var, time_DATE) == 0) {
		strcat(ret, NowDateToStr());
	} else if(strcmp(var, time_NTP) == 0) {
		strcat(ret, DateTime.serverNTP);
	} else if(strcmp(var, time_UPDATE) == 0) {
		if(GETBIT(DateTime.flags, fUpdateNTP)) strcat(ret, (char*) cOne); else strcat(ret, (char*) cZero);
	} else if(strcmp(var, time_UPDATE_I2C) == 0) {
		if(GETBIT(DateTime.flags, fUpdateI2C)) strcat(ret, (char*) cOne);
		else strcat(ret, (char*) cZero);
	} else strcat(ret, (char*) cInvalid);
}

// Установить опции ТН из числа (float), "set_oHP"
boolean HeatPump::set_optionHP(char *var, float x)   
{
	int n = x;
	if(strcmp(var,option_ADD_HEAT)==0)         {switch (n)  //использование дополнительного нагревателя (значения 1 и 0)
												   {
													case 0:  SETBIT0(Option.flags,fAddHeat);                                    return true; break;  // использование запрещено
													case 1:  SETBIT1(Option.flags,fAddHeat);SETBIT0(Option.flags,fTypeRHEAT);   return true; break;  // резерв
													case 2:  SETBIT1(Option.flags,fAddHeat);SETBIT1(Option.flags,fTypeRHEAT);   return true; break;  // бивалент
													default: SETBIT1(Option.flags,fAddHeat);SETBIT0(Option.flags,fTypeRHEAT);   return true; break;  // Исправить по умолчанию
												   } }else  // бивалент
	if(strcmp(var,option_TEMP_RHEAT)==0)       {if ((x>=-30)&&(x<=30))  {Option.tempRHEAT=rd(x, 100); return true;} else return false; }else     // температура управления RHEAT (градусы)
	if(strcmp(var,option_SunRegGeoTemp)==0)    { Option.SunRegGeoTemp = rd(x, 100); return true; }else
	if(strcmp(var,option_SunRegGeoTempGOff)==0){ Option.SunRegGeoTempGOff = rd(x, 100); return true; }else
	if(strcmp(var,option_SunTDelta)==0)        { Option.SunTDelta = rd(x, 100); return true; }else
	if(strcmp(var,option_SunGTDelta)==0)       { Option.SunGTDelta = rd(x, 100); return true; }else
	if(strcmp(var,option_SunTempOn)==0)   	   { Option.SunTempOn = rd(x, 100); return true;} else
	if(strcmp(var,option_SunTempOff)==0)   	   { Option.SunTempOff = rd(x, 100); return true;} else
	if(strcmp(var,option_SunRegGeo)==0)        { Option.flags = (Option.flags & ~(1<<fSunRegenerateGeo)) | ((n!=0)<<fSunRegenerateGeo); return true; }else
	if(strcmp(var,option_PUMP_WORK)==0)        {if ((n>=0)&&(n<=65535)) {Option.workPump=n; return true;} else return false;}else                // работа насоса конденсатора при выключенном компрессоре МИНУТЫ
	if(strcmp(var,option_PUMP_PAUSE)==0)       {if ((n>=0)&&(n<=65535)) {Option.pausePump=n; return true;} else return false;}else               // пауза между работой насоса конденсатора при выключенном компрессоре МИНУТЫ
	if(strcmp(var,option_ATTEMPT)==0)          { if ((n>=0)&&(n<=255)) {Option.nStart=n; return true;} else return false;  }else                // число попыток пуска
	if(strcmp(var,option_TIME_CHART)==0)       { if(n>0) { if (get_State()==pWORK_HP) clearChart(); Option.tChart = n; return true; } else return false; } else // Сбросить статистистику, начать отсчет заново
	if(strcmp(var,option_Charts_when_comp_on)==0){ Charts_when_comp_on = n; return true;} else
	if(strcmp(var, option_BEEP) == 0) { // Подача звукового сигнала
		if(n == 0) {
			SETBIT0(Option.flags, fBeep);
			digitalWriteDirect(PIN_BEEP, LOW);
			return true;
		} else if(n == 1) {
			SETBIT1(Option.flags, fBeep);
			return true;
		} else return false;
	} else
	if(strcmp(var, option_NEXTION) == 0) {// использование дисплея nextion
	   bool fl = n != 0;
	   if(fl != GETBIT(Option.flags, fNextion)) {
		   Option.flags = (Option.flags & ~(1 << fNextion)) | (fl << fNextion);
		   updateNextion(true);
	   }
	   return true;
	} else if(strcmp(var,option_NEXTION_WORK)==0)     { Option.flags = (Option.flags & ~(1<<fNextionOnWhileWork)) | ((n!=0)<<fNextionOnWhileWork); updateNextion(false); return true; } else            // использование дисплея nextion
	if(strcmp(var,option_NEXT_SLEEP)==0)       {if (n>=0) {Option.sleep=n; updateNextion(false); return true;} else return false;  }else       // Время засыпания секунды NEXTION минуты
#ifdef NEXTION
	if(strcmp(var,option_NEXT_DIM)==0)         {if ((n>=1)&&(n<=100)) {Option.dim=n; myNextion.set_dim(Option.dim); return true;} else return false; }else       // Якрость % NEXTION
#endif
	if(strcmp(var,option_History)==0)          {if (n==0) {SETBIT0(Option.flags,fHistory); return true;} else if (n==1) {SETBIT1(Option.flags,fHistory); return true;} else return false;       }else       // Сбрасывать статистику на карту
	if(strcmp(var,option_SDM_LOG_ERR)==0)      {if (n==0) {SETBIT0(Option.flags,fSDMLogErrors); return true;} else if (n==1) {SETBIT1(Option.flags,fSDMLogErrors); return true;} else return false;       }else
	if(strcmp(var,option_WebOnSPIFlash)==0)    { Option.flags = (Option.flags & ~(1<<fWebStoreOnSPIFlash)) | ((n!=0)<<fWebStoreOnSPIFlash); return true; } else
	if(strcmp(var,option_LogWirelessSensors)==0){ Option.flags = (Option.flags & ~(1<<fLogWirelessSensors)) | ((n!=0)<<fLogWirelessSensors); return true; } else
	if(strcmp(var,option_SAVE_ON)==0)          {if (n==0) {SETBIT0(Option.flags,fSaveON); return true;} else if (n==1) {SETBIT1(Option.flags,fSaveON); return true;} else return false;    }else             // флаг записи в EEPROM включения ТН (восстановление работы после перезагрузки)
	if(strncmp(var,option_SGL1W, sizeof(option_SGL1W)-1)==0) {
	   uint8_t bit = var[sizeof(option_SGL1W)-1] - '0' - 1;
	   if(bit <= 3) {
		   Option.flags = (Option.flags & ~(1<<(f1Wire1TSngl + bit))) | (n == 0 ? 0 : (1<<(f1Wire1TSngl + bit)));
		   return true;
	   }
	} else
	if(strcmp(var,option_SunMinWorktime)==0)   { Option.SunMinWorktime = n; return true; }else
	if(strcmp(var,option_SunMinPause)==0)      { Option.SunMinPause = n; return true; }else
	if(strcmp(var,option_PAUSE)==0)			   { if ((n>=0)&&(n<=999)) {Option.pause=n*60; return true;} else return false; }else             // минимальное время простоя компрессора с переводом в минуты но хранится в секундах!!!!!
	if(strcmp(var,option_MinCompressorOn)==0)  { Option.MinCompressorOn = n; return true; }else
	if(strcmp(var,option_DELAY_ON_PUMP)==0)    {if ((n>=0)&&(n<=900)) {Option.delayOnPump=n; return true;} else return false;}else        // Задержка включения компрессора после включения насосов (сек).
	if(strcmp(var,option_DELAY_OFF_PUMP)==0)   {if ((n>=0)&&(n<=900)) {Option.delayOffPump=n; return true;} else return false;}else       // Задержка выключения насосов после выключения компрессора (сек).
	if(strcmp(var,option_DELAY_START_RES)==0)  {if ((n>=0)&&(n<=6000)) {Option.delayStartRes=n; return true;} else return false;}else     // Задержка включения ТН после внезапного сброса контроллера (сек.)
	if(strcmp(var,option_DELAY_REPEAD_START)==0){if ((n>=0)&&(n<=6000)) {Option.delayRepeadStart=n; return true;} else return false;}else // Задержка перед повторным включениме ТН при ошибке (попытки пуска) секунды
	if(strcmp(var,option_DELAY_DEFROST_ON)==0) {if ((n>=0)&&(n<=600)) {Option.delayDefrostOn=n; return true;} else return false;}else     // ДЛЯ ВОЗДУШНОГО ТН Задержка после срабатывания датчика перед включением разморозки (секунды)
	if(strcmp(var,option_DELAY_DEFROST_OFF)==0){if ((n>=0)&&(n<=600)) {Option.delayDefrostOff=n; return true;} else return false;}else    // ДЛЯ ВОЗДУШНОГО ТН Задержка перед выключением разморозки (секунды)
	if(strcmp(var,option_DELAY_R4WAY)==0)      {if ((n>=0)&&(n<=600)) {Option.delayR4WAY=n; return true;} else return false;}else         // Задержка между переключением 4-х ходового клапана и включением компрессора, для выравнивания давлений (сек). Если включены эти опции (переключение тепло-холод)
	if(strcmp(var,option_DELAY_BOILER_SW)==0)  {if ((n>=0)&&(n<=1200)) {Option.delayBoilerSW=n; return true;} else return false;}else     // Пауза (сек) после переключение ГВС - выравниваем температуру в контуре отопления/ГВС что бы сразу защиты не сработали
	if(strcmp(var,option_DELAY_BOILER_OFF)==0) {if ((n>=0)&&(n<=1200)) {Option.delayBoilerOff=n; return true;} else return false;}        // Время (сек) на сколько блокируются защиты при переходе с ГВС на отопление и охлаждение слишком горяче после ГВС
	else if(strcmp(var,option_fBackupPower)==0)     {if (n==0) {SETBIT0(Option.flags,fBackupPower); return true;} else if (n==1) {SETBIT1(Option.flags,fBackupPower); return true;} else return false;} // флаг Использование резервного питания от генератора (ограничение мощности)
	else if(strcmp(var, option_f2BackupPowerAuto) == 0) {
	#ifdef SGENERATOR
		if(n == 0) { SETBIT0(Option.flags2, f2BackupPowerAuto);	return true;
		} else if(n == 1) {	SETBIT1(Option.flags2, f2BackupPowerAuto); return true;
		} else return false;
	#else
		return true;
	#endif
	} else if(strcmp(var, option_f2NextionGenFlashing) == 0) {
	#ifdef NEXTION_GENERATOR_FLASHING
		if(n == 0) { SETBIT0(Option.flags2, f2NextionGenFlashing);	return true;
		} else if(n == 1) {	SETBIT1(Option.flags2, f2NextionGenFlashing); return true;
		} else return false;
	#else
		return true;
	#endif
	} else if(strcmp(var,option_maxBackupPower)==0)   {if ((n>=0)&&(n<=10000)) {Option.maxBackupPower=n; return true;} else return false;}       // Максимальная мощность при питании от генератора
#ifdef WATTROUTER
	else if(strncmp(var, option_WR_Loads, sizeof(option_WR_Loads)-1) == 0) {
	   uint8_t bit = var[sizeof(option_WR_Loads)-1] - '0';
	   if(bit < WR_NumLoads) {
		   WR.Loads = WR_Loads = (WR_Loads & ~(1<<bit)) | (n == 0 ? 0 : (1<<bit));
		   //if(GETBIT(WR.Flags, WR_fActive)) WR_Refresh = true;
		   return true;
	   }
	} else if(strncmp(var, option_WR_Loads_PWM, sizeof(option_WR_Loads_PWM)-1) == 0) {
	   uint8_t bit = var[sizeof(option_WR_Loads_PWM)-1] - '0';
	   if(bit < WR_NumLoads) {
#ifdef WR_Boiler_Substitution_INDEX
		   if(bit == WR_Boiler_Substitution_INDEX) return true;
		   if(bit == WR_Load_pins_Boiler_INDEX) WR.PWM_Loads = (WR.PWM_Loads & ~(1<<WR_Boiler_Substitution_INDEX)) | (n == 0 ? 0 : (1<<WR_Boiler_Substitution_INDEX));
#endif
		   WR.PWM_Loads = (WR.PWM_Loads & ~(1<<bit)) | (n == 0 ? 0 : (1<<bit));
		   //if(GETBIT(WR.Flags, WR_fActive)) WR_Refresh = true;
		   return true;
	   }
	} else if(strncmp(var, option_WR_LoadPower, sizeof(option_WR_LoadPower)-1) == 0) {
	   uint8_t bit = var[sizeof(option_WR_LoadPower)-1] - '0';
	   if(bit < WR_NumLoads) {
		   WR.LoadPower[bit] = n;
		   if(GETBIT(WR.PWM_Loads, bit)) WR_Refresh |= (1<<bit);
		   return true;
	   }
	} else if(strcmp(var,option_WR_MinNetLoad)==0) { WR.MinNetLoad = n; return true; }
	else if(strcmp(var,option_WR_TurnOnPause)==0)  { WR.TurnOnPause = n; return true; }
	else if(strcmp(var,option_WR_NextSwitchPause)==0){ WR.NextSwitchPause = n; return true; }
	else if(strcmp(var,option_WR_TurnOnMinTime)==0){ WR.TurnOnMinTime = n; return true; }
	else if(strcmp(var,option_WR_LoadHist)==0)     { WR.LoadHist = n; return true; }
	else if(strcmp(var,option_WR_LoadAdd)==0)      { WR.LoadAdd = n; return true; }
	else if(strcmp(var,option_WR_PWM_FullPowerTime)==0){ WR.PWM_FullPowerTime = n; return true; }
	else if(strcmp(var,option_WR_PWM_FullPowerLimit)==0){ WR.PWM_FullPowerLimit = n; return true; }
	else if(strcmp(var,option_WR_fLog)==0)         { WR.Flags = (WR.Flags & ~(1<<WR_fLog)) | ((n!=0)<<WR_fLog); return true; }
	else if(strcmp(var,option_WR_fLogFull)==0)     { WR.Flags = (WR.Flags & ~(1<<WR_fLogFull)) | ((n!=0)<<WR_fLogFull); return true; }
	else if(strcmp(var,option_WR_WF_Hour)==0)      { if(n >= 0 && n <= 23) { WR.WF_Hour = n; return true; } else return false; }
	else if(strcmp(var,option_WR_PWM_Freq)==0)     {
#ifdef WR_ONE_PERIOD_PWM
		WR.PWM_Freq = PWM_WRITE_OUT_FREQ_DEFAULT;
#else
		if(WR.PWM_Freq != n) {
			WR.PWM_Freq = n;
			memset(TCChanEnabled, 0, sizeof_TCChanEnabled);
			PWMEnabled = 0;
			WR_Refresh |= WR_fLoadMask;
		}
#endif
		return true;
	} else if(strcmp(var,option_WR_fActive)==0) {
		WR.Flags = (WR.Flags & ~(1<<WR_fActive)) | ((n!=0)<<WR_fActive);
		if(n == 0) WR_Refresh = true;
#ifdef WR_PNET_AVERAGE
		else WR_Pnet_avg_init = true;
#endif
		return true;
	}
#endif
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
	if(strcmp(var,option_Charts_when_comp_on)==0){return _itoa(Charts_when_comp_on, ret);} else
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
	   uint8_t bit = var[sizeof(option_SGL1W)-1] - '0' - 1;
	   if(bit <= 3) {
		   return strcat(ret,(char*)(GETBIT(Option.flags, f1Wire1TSngl + bit) ? cOne : cZero));
	   }
	} else
	if(strcmp(var,option_SunRegGeo)==0)    	  {return _itoa(GETBIT(Option.flags, fSunRegenerateGeo), ret);}else
	if(strcmp(var,option_SunRegGeoTemp)==0)    {_dtoa(ret,Option.SunRegGeoTemp/10,1); return ret; }else
	if(strcmp(var,option_SunRegGeoTempGOff)==0){_dtoa(ret,Option.SunRegGeoTempGOff/10,1); return ret; }else
	if(strcmp(var,option_SunTDelta)==0)        {_dtoa(ret,Option.SunTDelta/10,1); return ret; }else
	if(strcmp(var,option_SunGTDelta)==0)       {_dtoa(ret,Option.SunGTDelta/10,1); return ret; }else
	if(strcmp(var,option_SunMinWorktime)==0)   {return _itoa(Option.SunMinWorktime, ret); } else
	if(strcmp(var,option_SunMinPause)==0)      {return _itoa(Option.SunMinPause, ret); } else
	if(strcmp(var,option_PAUSE)==0)            {return _itoa(Option.pause/60,ret); } else        // минимальное время простоя компрессора с переводом в минуты но хранится в секундах!!!!!
	if(strcmp(var,option_MinCompressorOn)==0)  {return _itoa(Option.MinCompressorOn, ret); } else
	if(strcmp(var,option_DELAY_ON_PUMP)==0)    {return _itoa(Option.delayOnPump,ret);}else       // Задержка включения компрессора после включения насосов (сек).
	if(strcmp(var,option_DELAY_OFF_PUMP)==0)   {return _itoa(Option.delayOffPump,ret);}else      // Задержка выключения насосов после выключения компрессора (сек).
	if(strcmp(var,option_DELAY_START_RES)==0)  {return _itoa(Option.delayStartRes,ret);}else     // Задержка включения ТН после внезапного сброса контроллера (сек.)
	if(strcmp(var,option_DELAY_REPEAD_START)==0){return _itoa(Option.delayRepeadStart,ret);}else // Задержка перед повторным включениме ТН при ошибке (попытки пуска) секунды
	if(strcmp(var,option_DELAY_DEFROST_ON)==0) {return _itoa(Option.delayDefrostOn,ret);}else    // ДЛЯ ВОЗДУШНОГО ТН Задержка после срабатывания датчика перед включением разморозки (секунды)
	if(strcmp(var,option_DELAY_DEFROST_OFF)==0){return _itoa(Option.delayDefrostOff,ret);}else   // ДЛЯ ВОЗДУШНОГО ТН Задержка перед выключением разморозки (секунды)
	if(strcmp(var,option_DELAY_R4WAY)==0)      {return _itoa(Option.delayR4WAY,ret);}else        // Задержка между переключением 4-х ходового клапана и включением компрессора, для выравнивания давлений (сек). Если включены эти опции (переключение тепло-холод)
	if(strcmp(var,option_DELAY_BOILER_SW)==0)  {return _itoa(Option.delayBoilerSW,ret);}else     // Пауза (сек) после переключение ГВС - выравниваем температуру в контуре отопления/ГВС что бы сразу защиты не сработали
	if(strcmp(var,option_DELAY_BOILER_OFF)==0) {return _itoa(Option.delayBoilerOff,ret);}        // Время (сек) на сколько блокируются защиты при переходе с ГВС на отопление и охлаждение слишком горяче после ГВС
	if(strcmp(var,option_fBackupPower)==0)     {if(GETBIT(Option.flags,fBackupPower)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero);}else // флаг Использование резервного питания от генератора (ограничение мощности)
	if(strcmp(var,option_fBackupPowerInfo)==0) { // Работа от генератора
	   if(GETBIT(Option.flags,fBackupPower)
	#ifdef USE_UPS
		   && !NO_Power
	#endif
		   ) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero);
	} else
	if(strcmp(var, option_f2BackupPowerAuto) == 0) {
	#ifdef SGENERATOR
	   if(GETBIT(Option.flags2, f2BackupPowerAuto)) return strcat(ret, (char*) cOne); else return strcat(ret, (char*) cZero);
	#else
	   return strcat(ret, (char*) cZero);
	#endif
	} else
	if(strcmp(var, option_f2NextionGenFlashing) == 0) {
	#ifdef NEXTION_GENERATOR_FLASHING
	   if(GETBIT(Option.flags2, f2NextionGenFlashing)) return strcat(ret, (char*) cOne); else return strcat(ret, (char*) cZero);
	#else
	   return strcat(ret, (char*) cZero);
	#endif
	} else
	if(strcmp(var,option_maxBackupPower)==0)   {return _itoa(Option.maxBackupPower,ret);}else    // Максимальная мощность при питании от генератора
	if(strcmp(var,option_SunTempOn)==0)        {_dtoa(ret,Option.SunTempOn/10, 1); return ret; } else
	if(strcmp(var,option_SunTempOff)==0)       {_dtoa(ret,Option.SunTempOff/10, 1); return ret; }
#ifdef WATTROUTER
	else if(strncmp(var, option_WR_Loads, sizeof(option_WR_Loads)-1)==0) {
	   uint8_t bit = var[sizeof(option_WR_Loads)-1] - '0';
	   if(bit < WR_NumLoads) {
		   return strcat(ret,(char*)(GETBIT(WR_Loads, bit) ? cOne : cZero));
	   }
	} else if(strncmp(var, option_WR_Loads_PWM, sizeof(option_WR_Loads_PWM)-1)==0) {
	   uint8_t bit = var[sizeof(option_WR_Loads_PWM)-1] - '0';
	   if(bit < WR_NumLoads && WR_Load_pins[bit] > 0) {
		   return strcat(ret,(char*)(GETBIT(WR.PWM_Loads, bit) ? cOne : cZero));
	   }
	} else if(strncmp(var, option_WR_LoadPower, sizeof(option_WR_LoadPower)-1)==0) {
	   uint8_t bit = var[sizeof(option_WR_LoadPower)-1] - '0';
	   if(bit < WR_NumLoads) {
		   return _itoa(WR.LoadPower[bit], ret);
	   }
	} else if(strcmp(var, option_WR_MinNetLoad)==0){ return _itoa(WR.MinNetLoad, ret); }
	else if(strcmp(var, option_WR_TurnOnPause)==0) { return _itoa(WR.TurnOnPause, ret); }
	else if(strcmp(var, option_WR_NextSwitchPause)==0){ return _itoa(WR.NextSwitchPause, ret); }
	else if(strcmp(var, option_WR_TurnOnMinTime)==0){ return _itoa(WR.TurnOnMinTime, ret); }
	else if(strcmp(var, option_WR_PWM_Freq)==0)    { return _itoa(WR.PWM_Freq, ret); }
	else if(strcmp(var, option_WR_LoadHist)==0)    { return _itoa(WR.LoadHist, ret); }
	else if(strcmp(var, option_WR_LoadAdd)==0)     { return _itoa(WR.LoadAdd, ret); }
	else if(strcmp(var, option_WR_PWM_FullPowerTime)==0){ return _itoa(WR.PWM_FullPowerTime, ret); }
	else if(strcmp(var, option_WR_PWM_FullPowerLimit)==0){ return _itoa(WR.PWM_FullPowerLimit, ret); }
	else if(strcmp(var, option_WR_fLog) == 0)      { if(GETBIT(WR.Flags, WR_fLog)) return strcat(ret, (char*) cOne); else return strcat(ret, (char*) cZero); }
	else if(strcmp(var, option_WR_fLogFull) == 0)  { if(GETBIT(WR.Flags, WR_fLogFull)) return strcat(ret, (char*) cOne); else return strcat(ret, (char*) cZero); }
	else if(strcmp(var, option_WR_fActive) == 0)   { if(GETBIT(WR.Flags, WR_fActive)) return strcat(ret, (char*) cOne); else return strcat(ret, (char*) cZero); }
	else if(strcmp(var, option_WR_WF_Hour) == 0)   { return _itoa(WR.WF_Hour, ret); }
#endif
	return strcat(ret,(char*)cInvalid);
}


// Установить рабочий профиль по текущему Prof
void HeatPump::set_profile()
{
	Option.numProf = Prof.get_idProfile();
}

// --------------------------------------------------------------------
// ФУНКЦИИ РАБОТЫ С ГРАФИКАМИ ТН -----------------------------------
// --------------------------------------------------------------------

// получить список доступных графиков в виде строки
// cat true - список добавляется в конец, false - строка обнуляется и список добавляется
void HeatPump::get_listChart(char* ret, const char *delimiter)
{
	for(uint8_t index = 0; index < sizeof(ChartsModSetup) / sizeof(ChartsModSetup[0]); index++) {
		if(ChartsModSetup[index].object == STATS_OBJ_Temp) strcat(ret, sTemp[ChartsModSetup[index].number].get_note());
		else if(ChartsModSetup[index].object == STATS_OBJ_Press) strcat(ret, sADC[ChartsModSetup[index].number].get_note());
		else if(ChartsModSetup[index].object == STATS_OBJ_PressTemp) {
			strcat(ret, sADC[ChartsModSetup[index].number].get_note());
			strcat(ret, ", °C");
		} else if(ChartsModSetup[index].object == STATS_OBJ_Flow) strcat(ret, sFrequency[ChartsModSetup[index].number].get_note());
		else strcat(ret, STATS_OBJ_names[ChartsModSetup[index].object]);
		strcat(ret, delimiter);
	}
	for(uint8_t index = 0; index < sizeof(ChartsConstSetup) / sizeof(ChartsConstSetup[0]); index++) {
		strcat(ret, ChartsConstSetup[index].name);
		strcat(ret, delimiter);
	}
	for(uint8_t index = 0; index < sizeof(ChartsOnFlySetup) / sizeof(ChartsOnFlySetup[0]); index++) {
		strcat(ret, ChartsOnFlySetup[index].name);
		strcat(ret, delimiter);
	}
}

// сбросить графики в ОЗУ
void HeatPump::clearChart()
{
    Chart_PressTemp_PCON = Chart_Temp_TCONOUT = Chart_Temp_TCOMP = Chart_Temp_TCONOUTG = Chart_Temp_TCONING = Chart_Temp_TEVAING = Chart_Temp_TEVAOUTG = Chart_Flow_FLOWCON = Chart_Flow_FLOWEVA = 0;
	for(uint8_t i = 0; i < sizeof(Charts) / sizeof(Charts[0]); i++) {
		Charts[i].clear();
		if(i < sizeof(ChartsModSetup) / sizeof(ChartsModSetup[0])) {
			if(ChartsModSetup[i].object == STATS_OBJ_PressTemp && ChartsModSetup[i].number == PCON) Chart_PressTemp_PCON = i;
			else if(ChartsModSetup[i].object == STATS_OBJ_Temp) {
				if(ChartsModSetup[i].number == TCONOUT) Chart_Temp_TCONOUT = i;
				else if(ChartsModSetup[i].number == TCOMP) Chart_Temp_TCOMP = i;
				else if(ChartsModSetup[i].number == TCONOUTG) Chart_Temp_TCONOUTG = i;
				else if(ChartsModSetup[i].number == TCONING) Chart_Temp_TCONING = i;
				else if(ChartsModSetup[i].number == TEVAING) Chart_Temp_TEVAING = i;
				else if(ChartsModSetup[i].number == TEVAOUTG) Chart_Temp_TEVAOUTG = i;
			} else if(ChartsModSetup[i].object == STATS_OBJ_Flow) {
#ifdef FLOWCON
				if(ChartsModSetup[i].number == FLOWCON) Chart_Flow_FLOWCON = i;
				else
#endif
#ifdef FLOWEVA
					if(ChartsModSetup[i].number == FLOWEVA) Chart_Flow_FLOWEVA = i;
#else
					{}
#endif
			}
		}
	}
}

// обновить статистику, добавить одну точку и если надо записать ее на карту.
// Все значения в графиках целочислены (сотые), выводятся в формате 0.01
void  HeatPump::updateChart()
{
	for(uint8_t i = 0; i < sizeof(ChartsModSetup) / sizeof(ChartsModSetup[0]); i++) {
		if(ChartsModSetup[i].object == STATS_OBJ_Temp) Charts[i].add_Point(sTemp[ChartsModSetup[i].number].get_Temp());
		else if(ChartsModSetup[i].object == STATS_OBJ_Press) Charts[i].add_Point(sADC[ChartsModSetup[i].number].get_Value());
		else if(ChartsModSetup[i].object == STATS_OBJ_PressTemp) Charts[i].add_Point(PressToTemp(ChartsModSetup[i].number));
		else if(ChartsModSetup[i].object == STATS_OBJ_Flow) Charts[i].add_Point(sFrequency[ChartsModSetup[i].number].get_Value() / 10);
#ifdef WATTROUTER
#ifdef WR_PowerMeter_Modbus
		else if(ChartsModSetup[i].object == STATS_OBJ_WattRouter_In) Charts[i].add_Point(WR_PowerMeter_Power / 10);
#else
		else if(ChartsModSetup[i].object == STATS_OBJ_WattRouter_In) Charts[i].add_Point(WR_Pnet);
#endif
#endif
	}
	for(uint8_t i = 0; i < sizeof(ChartsConstSetup) / sizeof(ChartsConstSetup[0]); i++) {
		uint8_t j = sizeof(ChartsModSetup) / sizeof(ChartsModSetup[0]) + i;
		if(ChartsConstSetup[i].object == STATS_OBJ_Overheat) Charts[j].add_Point(dEEV.get_Overheat());
#ifdef EEV_DEF
#ifdef EEV_PREFER_PERCENT
		else if(ChartsConstSetup[i].object == STATS_OBJ_EEV) Charts[j].add_Point(dEEV.calc_percent(dEEV.get_EEV()));
#else
		else if(ChartsConstSetup[i].object == STATS_OBJ_EEV) Charts[j].add_Point(dEEV.get_EEV());
#endif
		else if(ChartsConstSetup[i].object == STATS_OBJ_Overheat2) Charts[j].add_Point(GETBIT(dEEV.get_flags(), fEEV_DirectAlgorithm) ? dEEV.OverheatTCOMP : dEEV.get_tOverheat());
#endif
		else if(ChartsConstSetup[i].object == STATS_OBJ_Compressor) Charts[j].add_Point(dFC.get_frequency());
		else if(ChartsConstSetup[i].object == STATS_OBJ_Power_FC) Charts[j].add_Point(dFC.get_power() / 10);
		else if(ChartsConstSetup[i].object == STATS_OBJ_Current_FC) Charts[j].add_Point(dFC.get_current());
#ifdef USE_ELECTROMETER_SDM
		else if(ChartsConstSetup[i].object == STATS_OBJ_Voltage) Charts[j].add_Point(dSDM.get_voltage() * 100);
		else if(ChartsConstSetup[i].object == STATS_OBJ_Power) Charts[j].add_Point((int32_t)power220 / 10);
		else if(ChartsConstSetup[i].object == STATS_OBJ_COP_Full) Charts[j].add_Point(fullCOP);
#endif
	}
}

// получить данные графика  в виде строки, данные ДОБАВЛЯЮТСЯ к str
void HeatPump::get_Chart(int index, char *str)
{
	if(--index < 0 || index > int(sizeof(Charts) / sizeof(Charts[0]) + sizeof(ChartsOnFlySetup) / sizeof(ChartsOnFlySetup[0]))) return;
	uint8_t obj, idx = index;
	if(idx < int(sizeof(ChartsModSetup) / sizeof(ChartsModSetup[0]))) {
		obj = ChartsModSetup[idx].object;
	} else {
		idx -= sizeof(ChartsModSetup) / sizeof(ChartsModSetup[0]);
		if(idx < int(sizeof(ChartsConstSetup) / sizeof(ChartsConstSetup[0]))) {
			obj = ChartsConstSetup[idx].object;
		} else obj = ChartsOnFlySetup[idx - sizeof(ChartsConstSetup) / sizeof(ChartsConstSetup[0])].object;
	}
	_itoa(obj, str);
	strcat(str, ";");
	switch (obj) {
#ifndef EEV_PREFER_PERCENT
	case STATS_OBJ_EEV:
		Charts[index].get_PointsStr(str);
		break;
#endif
#ifdef TCONOUT
	case STATS_OBJ_Overcool:
		if(Chart_PressTemp_PCON && Chart_Temp_TCONOUT) Charts[Chart_PressTemp_PCON].get_PointsStrSubDiv100(str,&Charts[Chart_Temp_TCONOUT]);
		break;
#endif
	case STATS_OBJ_TCOMP_TCON:
		if(Chart_Temp_TCOMP && Chart_PressTemp_PCON) Charts[Chart_Temp_TCOMP].get_PointsStrSubDiv100(str, &Charts[Chart_PressTemp_PCON]); else // если датчика PCON нет
		if(Chart_Temp_TCOMP && Chart_Temp_TCONOUT)   Charts[Chart_Temp_TCOMP].get_PointsStrSubDiv100(str, &Charts[Chart_Temp_TCONOUT]);
		break;
	case STATS_OBJ_Delta_GEO:
		if(Chart_Temp_TEVAING && Chart_Temp_TEVAOUTG) Charts[Chart_Temp_TEVAING].get_PointsStrSubDiv100(str, &Charts[Chart_Temp_TEVAOUTG]);
		break;
	case STATS_OBJ_Delta_OUT:
		if(Chart_Temp_TCONOUTG && Chart_Temp_TCONING) Charts[Chart_Temp_TCONOUTG].get_PointsStrSubDiv100(str, &Charts[Chart_Temp_TCONING]);
		break;
#ifdef FLOWEVA
	case STATS_OBJ_Power_GEO:
		if(Chart_Flow_FLOWEVA && Chart_Temp_TEVAOUTG && Chart_Temp_TEVAING) Charts[Chart_Flow_FLOWEVA].get_PointsStrPower(str, &Charts[Chart_Temp_TEVAOUTG], &Charts[Chart_Temp_TEVAING], sFrequency[FLOWEVA].get_Capacity());
		break;
#endif
#ifdef FLOWCON
	case STATS_OBJ_Power_OUT:
		if(Chart_Flow_FLOWCON && Chart_Temp_TCONING && Chart_Temp_TCONOUTG) Charts[Chart_Flow_FLOWCON].get_PointsStrPower(str, &Charts[Chart_Temp_TCONING], &Charts[Chart_Temp_TCONOUTG], sFrequency[FLOWCON].get_Capacity());
		break;
#endif
	case STATS_OBJ_WattRouter_In:
		Charts[index].get_PointsStr(str);
		break;
	default:
		Charts[index].get_PointsStrDiv100(str);
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
void HeatPump::updateNextion(bool need_init)
{
#ifdef NEXTION
	if(GETBIT(Option.flags, fNextion))  // Дисплей подключен
	{
		if(need_init) {
			journal.jprintf("Nextion init:");
			myNextion.init();
		}
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
	switch((int)get_modeHouse())   // проверка для режима ДОМА
	{
	case pOFF:
		break;
	case pHEAT:
		if(GETBIT(Prof.Heat.flags,fTarget) == 0 || get_ruleHeat() == pHYBRID) {
			if((Prof.Heat.Temp1 + dt >= 0) && (Prof.Heat.Temp1 + dt <= 4000)) Prof.Heat.Temp1 = Prof.Heat.Temp1 + dt;
			return Prof.Heat.Temp1;
		} else {
			if((Prof.Heat.Temp2 + dt >= 1000) && (Prof.Heat.Temp2 + dt <= 5000)) Prof.Heat.Temp2 = Prof.Heat.Temp2 + dt;
			return Prof.Heat.Temp2;
		}
		break;
	case pCOOL:
		if(GETBIT(Prof.Cool.flags, fTarget) || get_ruleCool() == pHYBRID) {
			if((Prof.Cool.Temp1 + dt >= 0) && (Prof.Cool.Temp1 + dt <= 3000)) Prof.Cool.Temp1 = Prof.Cool.Temp1 + dt;
			return Prof.Cool.Temp1;
		} else {
			if((Prof.Cool.Temp2 + dt >= 0) && (Prof.Cool.Temp2 + dt <= 5000)) Prof.Cool.Temp2 = Prof.Cool.Temp2 + dt;
			return Prof.Cool.Temp2;
		}
		break;
	}
	return 0;
}

int16_t HeatPump::get_targetTempCool()
{
	int T;
	if(get_ruleCool() == pHYBRID) T = Prof.Cool.Temp1;
	else if(!(GETBIT(Prof.Cool.flags, fTarget))) T = Prof.Cool.Temp1;
	else T = Prof.Cool.Temp2;
	T += Schdlr.get_temp_change();
	return T;
}

int16_t HeatPump::get_targetTempHeat()
{
	int T;
	if(get_ruleHeat() == pHYBRID) T = Prof.Heat.Temp1;
	else if(!(GETBIT(Prof.Heat.flags, fTarget))) T = Prof.Heat.Temp1;
	else T = Prof.Heat.Temp2;
	if(Prof.Heat.add_delta_temp != 0) {
		int8_t h = rtcSAM3X8.get_hours();
		if((Prof.Heat.add_delta_end_hour >= Prof.Heat.add_delta_hour && h >= Prof.Heat.add_delta_hour && h <= Prof.Heat.add_delta_end_hour)
			|| (Prof.Heat.add_delta_end_hour < Prof.Heat.add_delta_hour && (h >= Prof.Heat.add_delta_hour || h <= Prof.Heat.add_delta_end_hour)))
			T += Prof.Heat.add_delta_temp;
	}

	if(Prof.Heat.kWeatherTarget != 0) { // Погодозависимость
		int32_t tmp = Prof.Heat.kWeatherTarget * (Prof.Heat.WeatherBase * 100 - sTemp[TOUT].get_Temp()) / 1000;
		int32_t tmp_r = Prof.Heat.WeatherTargetRange * 10;
		if(tmp > tmp_r) tmp = tmp_r;
		else if(tmp < -tmp_r) tmp = -tmp_r;
		T += tmp;
	}
	T += Schdlr.get_temp_change();
	return T;
}

// ИЗМЕНИТЬ целевую температуру бойлера с провекой допустимости значений
int16_t HeatPump::setTempTargetBoiler(int16_t dt) {
	if((Prof.Boiler.TempTarget + dt >= 500) && (Prof.Boiler.TempTarget + dt <= 9000)) Prof.Boiler.TempTarget = Prof.Boiler.TempTarget + dt;
	return Prof.Boiler.TempTarget;
}

// Получить целевую температуру бойлера с учетом корректировки
int16_t HeatPump::get_boilerTempTarget()
{
	int16_t ret = Prof.Boiler.TempTarget;
	int8_t h = rtcSAM3X8.get_hours();
	if(Prof.Boiler.add_delta_temp != 0) {
		if((Prof.Boiler.add_delta_end_hour >= Prof.Boiler.add_delta_hour && h >= Prof.Boiler.add_delta_hour && h <= Prof.Boiler.add_delta_end_hour)
				|| (Prof.Boiler.add_delta_end_hour < Prof.Boiler.add_delta_hour && (h >= Prof.Boiler.add_delta_hour || h <= Prof.Boiler.add_delta_end_hour)))
			ret += Prof.Boiler.add_delta_temp;
	}
#if defined(WATTROUTER) && defined(WEATHER_FORECAST) && defined(WR_Load_pins_Boiler_INDEX)
	if(h <= TARIF_NIGHT_END && WF_BoilerTargetPercent < WF_BOILER_MAX_CLOUDS && GETBIT(WR_Loads, WR_Load_pins_Boiler_INDEX) && GETBIT(WR.Flags, WR_fActive)) {
		ret = Prof.Boiler.WF_MinTarget + (ret - Prof.Boiler.WF_MinTarget) * WF_BoilerTargetPercent / 100;
		if(ret < Prof.Boiler.tempRBOILER) ret = Prof.Boiler.tempRBOILER;
	}
#endif
	return ret;
}

// Получить целевую температуру отопления
void HeatPump::getTargetTempStr(char *rstr)
{
	switch(get_modeHouse())   // проверка отопления
	{
	case pHEAT:
		rstr = dptoa(rstr, get_targetTempHeat(), 2);
		break;
	case pCOOL:
		rstr = dptoa(rstr, get_targetTempCool(), 2);
		break;
	default:
		strcpy(rstr, "-.-");
		return;
	}
	*--rstr = '\0';
}

// Целевая температура в строку, 2 знака после запятой
void HeatPump::getTargetTempStr2(char *rstr)
{
	switch(get_modeHouse())   // проверка отопления
	{
	case pHEAT:
		rstr = dptoa(rstr, get_targetTempHeat(), 2);
		break;
	case pCOOL:
		rstr = dptoa(rstr, get_targetTempCool(), 2);
		break;
	default:
		strcpy(rstr, "-.-");
		return;
	}
}

// --------------------------------------------------------------------------------------------------------
// ---------------------------------- ОСНОВНЫЕ ФУНКЦИИ РАБОТЫ ТН ------------------------------------------
// --------------------------------------------------------------------------------------------------------

// Все реле выключить
void HeatPump::relayAllOFF()
{
  uint8_t i;
  journal.jprintf(" All relay off\n");
  for(i=0;i<RNUMBER;i++)  dRelay[i].set_OFF();         // Выключить все реле;
}                               

// Переключение на бойлер или обратно (true-бойлер false-отопление/охлаждение) возврат onBoiler
// в зависимости от режима, не забываем менять onBoiler по нему определяется включение ГВС
// Функция вызывается на работающем компрессоре
boolean HeatPump::switchBoiler(boolean b)
{
//#ifdef R3WAY
//	if((b == onBoiler)&&(b==dRelay[R3WAY].get_Relay())) return onBoiler; // Нечего делать выходим
//#else
//  if(b == onBoiler) return onBoiler;                                   // Нечего делать выходим
//#endif
	
#ifdef DEBUG_MODWORK
	journal.printf(" swBoiler(%d): %X, mW:%d\n", b, onBoiler, get_modWork());
#endif
#ifdef R3WAY
	dRelay[R3WAY].set_Relay(b);            // Установить в нужное положение 3-х ходового
	Pump_HeatFloor(b);
#else // Нет трехходового - схема с двумя насосами
	// ставим сюда код переключения ГВС/отопление в зависимости от onBoiler=true - ГВС
	if(b) { // переключение на ГВС
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
		//    if (Status.modWork == pBOILER)  return onBoiler; // Идет сброс тепла паузу на переключение делать не надо ***
		} else { // пауза ИЛИ работа дома не задействована - все выключить
			Pump_HeatFloor(false);
			dRelay[RPUMPO].set_OFF();     // файнкойлы
		}
	}
#endif // закрытие Нет трехходового - схема с двумя насосами
	if(onBoiler && get_State() == pWORK_HP) { // Если грели бойлер и теперь ТН работает, то обеспечить дополнительное время (delayBoilerSW сек) для прокачивания гликоля - т.к разные уставки по температуре подачи
		journal.jprintf(" Pause %ds, Boiler->House\n", Option.delayBoilerSW);
		_delay(Option.delayBoilerSW * 1000); // выравниваем температуру в контуре отопления/ГВС что бы сразу защиты не сработали
	}
	offBoiler = b ? 0 : rtcSAM3X8.unixtime(); // запомнить время выключения ГВС (нужно для переключения)
	return onBoiler = b;
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

#ifdef RPUMPFL
void HeatPump::Pump_HeatFloor(boolean On)
{
	if(On) {
		if(!(get_modWork() & pBOILER) && ((get_modeHouse() == pHEAT && GETBIT(Prof.Heat.flags, fHeatFloor)) || (get_modeHouse() == pCOOL && GETBIT(Prof.Cool.flags, fHeatFloor))))
			dRelay[RPUMPFL].set_ON();
	} else dRelay[RPUMPFL].set_OFF();
}
#else
void HeatPump::Pump_HeatFloor(boolean) { }
#endif

// Включить или выключить насосы контуров и то что требуется для ГВС (трех-ходовой или насос) первый параметр их желаемое состояние
// Второй параметр параметр задержка после включения/выключения мсек. отдельного насоса (борьба с помехами)
// Идет проверка на необходимость изменения состояния насосов
// Генерятся задержки для защиты компрессора, есть задержки между включенимями насосов для уменьшения помех
// Не забываем что сдесь устанавливается onBoiler, по этому сначала вызывается Pumps потом switchBoiler
void HeatPump::Pumps(boolean b, uint16_t d)
{
#ifdef DEBUG_MODWORK
	journal.printf(" Pumps(%d), modWork: %X\n", b, get_modWork());
#endif

#ifdef R3WAY           // Если определен трехходовой то в начале переключаем его (при выключении что бы остыл теплообменник)
if(b && (get_modWork() & pBOILER)){
        dRelay[R3WAY].set_Relay(true);             // скорее всего это пуск ТН (не переключение) по этому надо включить ГВС
		_delay(d); 
		onBoiler = true;
		offBoiler = 0;
	} else {
		dRelay[R3WAY].set_Relay(false);            // скорее всего это выключение ТН (не переключение) по этому надо выключить ГВС     
	    if(onBoiler && get_State() == pWORK_HP) {  // Если грели бойлер и теперь ТН работает, то обеспечить дополнительное время (delayBoilerSW сек) для прокачивания гликоля - т.к разные уставки по температуре подачи
		 journal.jprintf(" Pause %ds, Boiler->Pause\n", Option.delayBoilerSW);
		_delay(Option.delayBoilerSW * 1000);    // выравниваем температуру в контуре отопления/ГВС что бы сразу защиты не сработали
	      }
    	onBoiler = false;
		offBoiler = rtcSAM3X8.unixtime();			// запомнить время выключения ГВС (нужно для переключения)
	}
#endif


	if(!b && GETBIT(dRelay[PUMP_IN].flags, fR_StatusMain)) {
		journal.jprintf(" Delay: stop IN pump.\n");
		_delay(DELAY_BEFORE_STOP_IN_PUMP * 1000); // задержка перед выключениме гео насоса после выключения компрессора (облегчение останова)
	}
	
	dRelay[PUMP_IN].set_Relay(b);             // Реле включения насоса входного контура  (геоконтур)
	
	if(!b && (GETBIT(dRelay[RPUMPO].flags, fR_StatusMain) // пауза перед выключением насосов контуров, если нужно
#ifdef RPUMPBH
			|| GETBIT(dRelay[RPUMPBH].flags, fR_StatusMain)
#endif
	)){ // Насосы выключены и будут выключены, нужна пауза идет останов компрессора (новое значение выкл  старое значение вкл)
		journal.jprintf(" Delay: stop OUT pump.\n");
		_delay(Option.delayOffPump * 1000); // задержка перед выключениме насосов после выключения компрессора (облегчение останова)
	} else {
		_delay(d);                                // Задержка на d мсек
	}
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
  #ifdef RPUMPBH									// насос бойлера
   	if(b) {
   		if((get_modWork() & pBOILER)) {
   			dRelay[RPUMPBH].set_ON();
   			onBoiler = true;
   			offBoiler = 0;
   		} else {
   		   	dRelay[RPUMPO].set_ON();               	// насос отопления
   		   	onBoiler = false;
   		}
   	} else {
   		dRelay[RPUMPBH].set_OFF();					// насос бойлера
   		dRelay[RPUMPO].set_OFF();                	// насос отопления
		onBoiler = false;
		offBoiler = rtcSAM3X8.unixtime();			// запомнить время выключения ГВС (нужно для переключения)
   	}
   	_delay(d); 										// Задержка на d мсек
  #else
   	dRelay[RPUMPO].set_Relay(b);                	// насос отопления
   	_delay(d); 										// Задержка на d мсек
  #endif
#endif // R3WAY
   	if(!b) {
   		SETBIT0(flags, fHP_BoilerTogetherHeat);
#ifdef SUPERBOILER
   		dRelay[RSUPERBOILER].set_OFF();
#endif
   	}
  	Pump_HeatFloor(b); 				  				// насос ТП

}
// Сброс инвертора если он стоит в ошибке, проверяется наличие инвертора и проверка ошибки после сброса
// Проводится различный сброс в зависимости от конфигурации
int8_t HeatPump::ResetFC()
{
	if(!dFC.get_present()) return OK;                                       // Инвертора нет выходим
// 1. Проверка наличия ошибки	
#ifdef SERRFC                                                               // Если есть вход от инвертора об ошибке
    sInput[SERRFC].Read();                                                  // Обновить значение
	if (sInput[SERRFC].get_lastErr()==OK) {                                 // Инвертор сбрасывать не надо
#ifdef DEBUG_MODWORK
		journal.jprintf(" %s: OK, no inverter reset required\n",sInput[SERRFC].get_name());
#endif
	return OK; }
#else
	if(dFC.get_err() == OK && !dFC.get_blockFC()) return OK;
#endif
// 2. Собственно сброс
#ifdef RRESET                                                               // Если есть вход от инвертора об ошибке
	dRelay[RRESET].set_ON(); _delay(100); dRelay[RRESET].set_OFF();         // Подать импульс сброса
	journal.jprintf(" Reset %s by RRESET: ",dFC.get_name());
#else
	dFC.reset_FC();                                                         // подать команду на сброс по модбас
    journal.jprintf(" Reset %s by Modbus: ",dFC.get_name());
//	if(dFC.get_blockFC()) return ERR_RESET_FC;                              // Инвертор блокирован
#endif
// 3. Проверка результатов сброса
#ifdef SERRFC                                                               // Если есть вход от инвертора об ошибке
    _delay(100);
	sInput[SERRFC].Read();
	if (sInput[SERRFC].get_lastErr()==OK) {journal.jprintf("%s\r\n",cOk);return OK;}// Инвертор сброшен
	else {journal.jprintf("%s\n",cError); return ERR_RESET_FC;}                   // Сброс не прошел
#else
   if(dFC.get_blockFC()) {journal.jprintf("%s\n",cError); return ERR_RESET_FC;}   // Инвертор блокирован
   else                  {journal.jprintf("%s\n",cOk);return OK;}                 // Инвертор сброшен
#endif
	return OK;
}
           
// проверить, Если есть ли работа для ТН - true.
boolean HeatPump::CheckAvailableWork()
{
	return Prof.SaveON.mode != pOFF || GETBIT(Prof.SaveON.flags, fBoilerON) || GETBIT(Option.flags, fSunRegenerateGeo) || Schdlr.IsShedulerOn();
}

// START/RESUME -----------------------------------------
// Функция Запуска/Продолжения работы ТН - возвращает ок или код ошибки
// Запускается ВСЕГДА отдельной задачей с приоритетом выше вебсервера
// Параметр задает что делаем true-старт, false-возобновление
void HeatPump::StartResume(boolean start)
{
#ifdef USE_UPS
	if(NO_Power) {
		NO_Power = 2; // Resume after
xGoWait:
		journal.jprintf(" Start task UpdateHP\n");
		journal.jprintf_time("%s WAIT . . .\n", (char*) nameHeatPump);
		startWait = true;                    // Начало работы с ожидания=true;
		setState(pWAIT_HP);
		Task_vUpdate_run = true;
		vTaskResume(xHandleUpdate);
		return;
	}
#endif
	// Дана команда старт - но возможно надо переходить в ожидание
	// Определяем что делать
	int8_t profile = Schdlr.calc_active_profile();
	if((profile != SCHDLR_NotActive) && (start)) { // расписание активно и дана команда
		if(profile == SCHDLR_Profile_off) {
#ifdef USE_UPS
			goto xGoWait;
#else
		journal.jprintf(" Start task UpdateHP\n");
		journal.jprintf_time("%s WAIT . . .\n", (char*) nameHeatPump);
		startWait = true;                    // Начало работы с ожидания=true;
		setState(pWAIT_HP);
		Task_vUpdate_run = true;
		vTaskResume(xHandleUpdate);
		return;
#endif			
		} else if(profile != Prof.get_idProfile()) {
			Prof.load(profile);
			set_profile();
			journal.jprintf("Profile changed to #%d\n", profile);
		}
	}
	if(startWait) {
		startWait = false;
		start = true;   // Делаем полный запуск, т.к. в положение wait переходили из стопа (расписания)
	}

	// 1. Переменные  установка, остановка ТН имеет более высокий приоритет чем пуск ! -------------------------
	if(start)  // Команда старт
	{
		if((get_State() == pWORK_HP) || (get_State() == pSTOPING_HP) || (get_State() == pSTARTING_HP)) return; // Если ТН включен или уже стартует или идет процесс остановки то ничего не делаем (исключается многократный заход в функцию)
		journal.jprintf_date( "  Start . . .\n");
	} else {
		if(get_State() != pWAIT_HP) return; // Если состяние НЕ РАВНО ожиданию то ничего не делаем, выходим восстанавливать нечего
		journal.jprintf_date( "  Resume . . .\n");
	}
	setState(pSTARTING_HP);                              // Производится старт -  устанавливаем соответсвующее состояние
	//  Если требуется сбрасываем инвертор  (проверям ошибку и пишем в журнал)
	eraseError();                                      // Обнулить ошибку только после сброса инвертора! иначе она может повторно возникнет при ошибке инвертора

	Status.ret = pNone;                                    // Состояние алгоритма
	lastEEV = -1;                                          // -1 это признак того что слежение eev еще не рабоатет (выключения компрессора  небыло)

	if(startPump)                                      // Если задача не остановлена то остановить (0 - останов задачи, 1 - запуск, 2 - в работе (выкл), 3 - в работе (вкл))
	{
		startPump = 0;                                     // Поставить признак останова задачи насос
	    if(get_workPump()) journal.jprintf(" %s: Pumps in pause %s. . .\n",(char*)__FUNCTION__, "OFF");
	}

	offBoiler = 0;                                         // Бойлер никогда не выключался
	onSalmonella = false;                                  // Если true то идет Обеззараживание
	onBoiler = false;                                      // Если true то идет нагрев бойлера
	SETBIT0(flags, fHP_BoilerTogetherHeat);

	// 2.1 Проверка конфигурации, которые можно поменять из морды, по этому проверяем всегда ----------------------------------------
	if(!CheckAvailableWork())   // Нет работы для ТН - ничего не включено
	{
		setState(pOFF_HP);  // Еще ничего не сделали по этому сразу ставим состоение выключено
		set_Error(ERR_NO_WORK, (char*) __FUNCTION__);
		return;
	}
    if(get_State() != pSTARTING_HP) return;            // Могли нажать кнопку стоп, выход из процесса запуска
#ifdef EEV_DEF
	if((!sADC[PEVA].get_present()) && (dEEV.get_ruleEEV() == TEVAOUT_PEVA))  //  Отсутвует датчик давления, и выбран алгоритм ЭРВ который его использует",
	{
		setState(pOFF_HP);    // Еще ничего не сделали по этому сразу ставим состоение выключено
		set_Error(ERR_PEVA_EEV, (char*) __FUNCTION__);        // остановить по ошибке;
		return;
	}
#endif

	// 2.2 Проверка конфигурации, которые определены конфигом (поменять нельзя), по этому проверяем один раз при страте ТН ----------------------------------------
	if(start)  // Команда старт
	{
		if(!dRelay[PUMP_OUT].get_present())  // отсутсвует насос на конденсаторе, пользователь НЕ может изменить в процессе работы проверка при старте
		{
			setState(pOFF_HP);    // Еще ничего не сделали по этому сразу ставим состоение выключено
			set_Error(ERR_PUMP_CON, (char*) __FUNCTION__);        // остановить по ошибке;
			return;
		}
		if(!dRelay[PUMP_IN].get_present())   // отсутсвует насос на испарителе, пользователь может изменить в процессе работы
		{
			setState(pOFF_HP);    // Еще ничего не сделали по этому сразу ставим состоение выключено
			set_Error(ERR_PUMP_EVA, (char*) __FUNCTION__);        // остановить по ошибке;
			return;
		}
		if((!dRelay[RCOMP].get_present()) && (!dFC.get_present()))   // отсутсвует компрессор, пользователь может изменить в процессе работы
		{
			setState(pOFF_HP);    // Еще ничего не сделали по этому сразу ставим состоение выключено
			set_Error(ERR_NO_COMPRESS, (char*) __FUNCTION__);        // остановить по ошибке;
			return;
		}
	} //  if (start)  // Команда старт

	// 3.  ПОДГОТОВКА ------------------------------------------------------------------------

	if(start)  // Команда старт - Инициализация ЭРВ и очистка графиков при восстановлени не нужны
	{
#ifdef EEV_DEF
		//journal.jprintf(" EEV init\n");
		if(get_State() != pSTARTING_HP) return;            // Могли нажать кнопку стоп, выход из процесса запуска
		else dEEV.Start();                                     // Включить ЭРВ  найти 0 по завершению позиция 0!!!
#endif

#ifdef CLEAR_CHART_HP_ON
		journal.jprintf(" Charts clear and start\n");
		if (get_State()!=pSTARTING_HP) return ;            // Могли нажать кнопку стоп, выход из процесса запуска
		else clearChart();// Запустить графики <- тут не запуск, тут очистка
#endif

	}

	// 4. Определяем что нужно делать -----------------------------------------------------------
	if(get_State() != pSTARTING_HP) return;            // Могли нажать кнопку стоп, выход из процесса запуска
	MODE_HP mod = get_Work();                                   // определяем что делаем с компрессором
	if(mod > pBOILER) mod = pOFF;                              // При первом пуске могут быть только состояния pOFF,pHEAT,pCOOL,pBOILER
	journal.jprintf(" Start modWork:%d[%s]\n", (int) mod, codeRet[Status.ret]);
	Status.modWork = mod;  // Установка режима!

	//  set_Error(ERR_PEVA_EEV,(char*)__FUNCTION__);        // остановить по ошибке для проверки EEV


	// 5. Если не старт ТН то проверка на минимальную паузу между включениями, при включении ТН паузы не будет -----------------
	if(!start)  // Команда Resume
		while(check_compressor_pause()) {
			_delay(1000);
			if(get_State() != pSTARTING_HP) return;
		}    // Могли нажать кнопку стоп, выход из процесса запуска

	//  6. Конфигурируем 3 и 4-х клапаны и включаем насосы ПАУЗА после включения насосов
	if(!configHP(Status.modWork)) return;

	// 7. Включение компрессора и запуск обновления EEV -----------------------------------------------------
	if(get_State() != pSTARTING_HP) return;            // Могли нажать кнопку стоп, выход из процесса запуска
	if(is_next_command_stop()) return;			    // следующая команда останов, выходим
#ifdef USE_ELECTROMETER_SDM
	if(!dSDM.get_link()) dSDM.uplinkSDM();
#endif

	bool start_compressor_now = mod && mod <= pBOILER; // pCOOL or pHEAT or pBOILER
	if(start_compressor_now) {
#ifndef DEMO   // проверка блокировки инвертора
		if((dFC.get_present()) && (dFC.get_blockFC()))          // есть инвертор но он блокирован
		{
			if(dFC.get_err() != ERR_485_BUZY) {
				journal.jprintf("%s: is blocked, ignore start\n", dFC.get_name());
				set_Error(ERR_MODBUS_BLOCK, (char*) __FUNCTION__);          // ВОТ ЗДЕСЬ КОМАНДА СТОП И ПРОЙДЕТ
				return;
			}
		}
#endif
	}
	if(get_errcode() != OK)                                 // ОШИБКА перед стартом
	{
		journal.jprintf(" Error %d before start compressor!\n", get_errcode());
		set_Error(ERR_COMP_ERR, (char*) __FUNCTION__);
		return;
	}
	if(start_compressor_now) compressorON(); // Компрессор включить если нет ошибок и надо включаться

	// 10. Сохранение состояния  -------------------------------------------------------------------------------
	if(get_State() != pSTARTING_HP) return;                   // Могли нажать кнопку стоп, выход из процесса запуска
	setState(pWORK_HP);

	// 11. Запуск задачи обновления ТН ---------------------------------------------------------------------------
	if(start) {
		journal.jprintf(" Start task UpdateHP\n");
		Task_vUpdate_run = true;
		vTaskResume(xHandleUpdate);                                       // Запустить задачу Обновления ТН, дальше она все доделает
		//_delay(1);
	}

	// 12. насос запущен -----------------------------------------------------------------------------------------
	journal.jprintf_time("%s ON . . .\n", (char*) nameHeatPump);

	return;
}

// STOP/WAIT -----------------------------------------
// Функция Останова/Ожидания ТН  - возвращает код ошибки
// Параметр задает что делаем true-останов, false-ожидание
void HeatPump::StopWait(boolean stop)
{  
  if (stop)
  {
    if ((get_State()==pOFF_HP)||(get_State()==pSTOPING_HP)) return;    // Если ТН выключен или выключается ничего не делаем
    journal.jprintf_date("Stopping...\n");
    setState(pSTOPING_HP);  // Состояние выключения
  } else {
    if ((get_State()==pOFF_HP)||(get_State()==pSTOPING_HP)||(get_State()==pWAIT_HP)) return;    // Если ТН выключен или выключается или ожидание ничего не делаем
    journal.jprintf_date("Switch to waiting...\n");
    setState(pSTOPING_HP);  // Состояние выключения
  }

  journal.jprintf(" modWork: %X[%s]\n", get_modWork(), codeRet[Status.ret]);
  compressorOFF();		// Останов компрессора, насосов - PUMP_OFF(), ЭРВ

  if (stop) //Обновление ТН отключаем только при останове
  {
	Task_vUpdate_run = false;					        // Остановить задачу обновления ТН vUpdate (xHandleUpdate)
    journal.jprintf(" Stop task UpdateHP\n");
#ifdef USE_SUN_COLLECTOR
	Sun_OFF();											// Выключить СК
	time_Sun = GetTickCount() - uint32_t(Option.SunMinPause * 1000);	// выключить задержку последующего включения
#endif
  }
    
  if(startPump)
  {
     startPump = 0;                                    // Поставить признак что насос выключен
     if(get_workPump()) journal.jprintf(" %s: Pumps in pause %s. . .\n",(char*)__FUNCTION__, "OFF");
  }

 // Принудительное выключение отдельных узлов ТН если они есть в конфиге
  #ifdef RBOILER  // управление дополнительным ТЭНом бойлера
     dRelay[RBOILER].set_OFF();  // выключить тэн бойлера
  #endif

  #ifdef RHEAT  // управление  ТЭНом отопления
     dRelay[RHEAT].set_OFF();     // выключить тэн отопления
  #endif

  #ifdef RPUMPB  // управление  насосом циркуляции ГВС
     dRelay[RPUMPB].set_OFF();     // выключить насос циркуляции ГВС
  #endif

  #ifdef RPUMPBH  // управление  насосом нагрева ГВС
     dRelay[RPUMPBH].set_OFF();
  #endif
  SETBIT0(flags, fHP_BoilerTogetherHeat);

  #ifdef CONFIG_5  // случаи бывают разные - должно работать без костылей.- но лучше перебдеть -))
   relayAllOFF(); // Все выключить, все (на всякий случай), * внимание - выключатся реле по расписанию!
  #endif 
 
  if(stop)
  {
     //journal.jprintf(" statChart stop\n");
     setState(pOFF_HP);
     journal.jprintf_time("%s OFF . . .\n",(char*)nameHeatPump);
  } else {
     setState(pWAIT_HP);
     journal.jprintf_time("%s WAIT . . .\n",(char*)nameHeatPump);
  }
#ifdef AUTO_START_GENERATOR
  dRelay[RGEN].set_OFF();
#endif
  return;
}

// Инициализировать переменные ПИД регулятора
void HeatPump::resetPID()
{
#ifdef PID_FORMULA2
	pidw.PropOnMeasure = DEF_FC_PID_P_ON_M;
	pidw.pre_err = Status.modWork & pHEAT ? Prof.Heat.tempPID - FEED : Status.modWork & pBOILER ?
#ifdef SUPERBOILER
			Prof.Boiler.tempPID - PressToTemp(PCON)
#else
			Prof.Boiler.tempPID - FEED
#endif
			: Prof.Cool.tempPID - FEED;
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


#ifdef RBOILER  // управление дополнительным ТЭНом бойлера
// Проверка на необходимость греть бойлер дополнительным тэном (true - надо греть) ВСЕ РЕЖИМЫ
boolean HeatPump::boilerAddHeat()
{
	if(get_State() != pWORK_HP) return false; // работа ТЭНа бойлера разрешена если только работает ТН, в противном случае выкл
	if (GETBIT(Option.flags,fBackupPower))  { // если переключение на ходу на резервный источник то сбросить догрев бойлера
		flagRBOILER = false;
		return false;
	}
	int16_t T = sTemp[TBOILER].get_Temp();
	if((GETBIT(Prof.SaveON.flags, fBoilerON)) && (GETBIT(Prof.Boiler.flags, fSalmonella)) && (!GETBIT(Option.flags, fBackupPower))) // Сальмонелла не взирая на расписание если включен бойлер и не питание от резервного источника
	{
		if((rtcSAM3X8.get_day_of_week() == SALMONELLA_DAY) && (rtcSAM3X8.get_hours() == SALMONELLA_HOUR) && (rtcSAM3X8.get_minutes() <= 2) && (!onSalmonella)) { // Надо начитать процесс обеззараживания
			startSalmonella = rtcSAM3X8.unixtime();
			onSalmonella = true;
			journal.jprintf(" Cycle start salmonella, %.2dC°\n", sTemp[TBOILER].get_Temp());
		}
		if(onSalmonella) {   // Обеззараживание нужно
			if(startSalmonella + SALMONELLA_TIME > rtcSAM3X8.unixtime()) { // Время цикла еще не исчерпано
				if(T < SALMONELLA_TEMP) return true; // Включить обеззараживание
#ifdef SALMONELLA_HARD
				else if (T > SALMONELLA_TEMP+50) return false; else return dRelay[RBOILER].get_Relay(); // Вариант работы - Стабилизация температуры обеззараживания, гистерезис 0.5 градуса
#else
				else {  // Вариант работы только до достижение темперaтуpы и сразу выключение
					onSalmonella = false;
					startSalmonella = 0;
					journal.jprintf(" Salmonella cycle finished, %.2dC°\n", sTemp[TBOILER].get_Temp());
					return false;
				}
#endif
			} else {  // Время вышло, выключаем, и идем дальше по алгоритму
				onSalmonella = false;
				startSalmonella = 0;
				journal.jprintf(" Salmonella cycle end, %.2dC°\n", sTemp[TBOILER].get_Temp());
			}
		}
	} else if(onSalmonella) { // если сальмонеллу отключили на ходу выключаем и идем дальше по алгоритму
		onSalmonella = false;
		startSalmonella = 0;
		journal.jprintf(" Off salmonella\n");
	}

	// Догрев бойлера

	if(GETBIT(Prof.SaveON.flags, fBoilerON) && scheduleBoiler()) // Если разрешено греть бойлер согласно расписания И Бойлер включен
	{
		if(GETBIT(Prof.Boiler.flags, fTurboBoiler))  // Если турбо режим, то повторяем за Тепловым насосом (грет или не греть)
		{
			if(T < Prof.Boiler.tempRBOILER) return onBoiler;   // работа параллельно с ТН  если температура МЕНЬШЕ догрева то повторяем работу ТН
		}
		if(GETBIT(Prof.Boiler.flags, fAddHeating))  // Включен догрев
		{
			int16_t b_target = get_boilerTempTarget();
			if(!flagRBOILER && (T < b_target - (HeatBoilerUrgently ? 10 : Prof.Boiler.dTemp))) {  // Бойлер ниже гистерезиса - ставим признак необходимости включения Догрева (но пока не включаем ТЭН)
			    flagRBOILER = true;
				return false;
			}
			if(!flagRBOILER || onBoiler) return false; // флажка нет или работет бойлер, но догрев не включаем
			else {
				if(T < b_target && (T >= Prof.Boiler.tempRBOILER - Prof.Boiler.dAddHeat || dRelay[RBOILER].get_Relay() || GETBIT(Prof.Boiler.flags, fAddHeatingForce))) {  // Греем тэном
					return true;
				} else { // бойлер выше целевой температуры - цель достигнута или греть тэном еще рано
					flagRBOILER = false;
					return false;
				}
			}
		} else { // ТЭН не используется (сняты все флажки)
			flagRBOILER = false;
			return false;
		}
	} else { // Бойлер сейчас запрещен
		flagRBOILER = false;
		return false;
	}

}
#endif

// Проверить расписание бойлера true - нужно греть false - греть не надо, если расписание выключено то возвращает true
boolean HeatPump::scheduleBoiler()
{
	if(HeatBoilerUrgently) return true;
	if(GETBIT(Prof.Boiler.flags, fSchedule))         // Если используется расписание
	{  // Понедельник 0 воскресенье 6 это кодирование в расписании функция get_day_of_week возвращает 1-7
		boolean b = Prof.Boiler.Schedule[rtcSAM3X8.get_day_of_week() - 1] & (0x01 << rtcSAM3X8.get_hours()) ? true : false;
		if(!b) return false;             // запрещено греть бойлер согласно расписания
	}
	return true;
}

// Управление температурой в зависимости от режима
#define STR_REDUCED "Reduced FC"   // Экономим место
#define STR_FREQUENCY " FC> %.2f\n" // Экономим место
// Итерация по управлению Бойлером
// возврат что надо делать компрессору, функция НЕ управляет компрессором а только выдает необходимость включения компрессора
MODE_COMP  HeatPump::UpdateBoiler()
{
#ifdef RBOILER  // управление дополнительным ТЭНом бойлера
	if(boilerAddHeat()) { // Дополнительный нагреватель бойлера - нужно греть
		if(get_State() == pOFF_HP || get_State() == pSTOPING_HP) { // Если ТН выключен или выключается
			dRelay[RBOILER].set_OFF();
			flagRBOILER = false;
			return pCOMP_OFF;
		}
		if(!GETBIT(Option.flags, fBackupPower)) {   // Включение ТЭНа бойлера если не питание от резервного источника
			dRelay[RBOILER].set_ON();
			#ifdef RPUMPBH
			if(!onBoiler && GETBIT(flags, fHP_BoilerTogetherHeat)) dRelay[RPUMPBH].set_OFF();   // насос ГВС - выключить
			#endif
			SETBIT0(flags, fHP_BoilerTogetherHeat);
		}
		if(!GETBIT(Prof.Boiler.flags, fTurboBoiler)) return pCOMP_OFF;
	} else {
		if(dRelay[RBOILER].get_Relay()) set_HeatBoilerUrgently(false);
		dRelay[RBOILER].set_OFF();
		if(get_State() == pOFF_HP || get_State() == pSTOPING_HP) { // Если ТН выключен или выключается
			return pCOMP_OFF;
		}
	}
#else
	if(get_State() == pOFF_HP || get_State() == pSTOPING_HP) { // Если ТН выключен или выключается
		return pCOMP_OFF;
	}
#endif
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
		return pCOMP_OFF;      // запрещено греть бойлер согласно расписания
	}

	// -----------------------------------------------------------------------------------------------------
	// Сброс излишней энергии в систему отопления по температуре подачи/конденсации
	// Переключаемся на отопление на ходу и ждем определенное число секунд, дальше возвращаемся на бойлер
	if(GETBIT(Prof.Boiler.flags,fResetHeat))    // Стоит требуемая опция - Сброс тепла в СО
	{
		if(Status.prev == pBdis) {
			if(offBoiler + Prof.Boiler.Reset_Time < rtcSAM3X8.unixtime()) {
#ifdef DEBUG_MODWORK
				journal.jprintf(" Boiler: Discharged Ok\n");
#endif
				switchBoiler(true);    // Переключиться на ходу на ГВС
			} else {
				Status.ret = pBdis;
				return pCOMP_NONE;
			}
		} else if(
#ifdef SUPERBOILER
		   PressToTemp(PCON)
#else
		   FEED
#endif
		   > Prof.Boiler.tempInLim - Prof.Boiler.DischargeDelta * 10
		   || (sTemp[TCOMP].get_Temp() > sTemp[TCOMP].get_maxTemp() - BOILER_TEMP_COMP_RESET)) // температура нагнетания компрессора больше максимальной -5 градусов
		{
#ifdef DEBUG_MODWORK
			journal.jprintf(" Boiler: Discharging %ds...\n", Prof.Boiler.Reset_Time);
#endif
			switchBoiler(false);       // Переключиться на ходу на отопление
			Status.ret = pBdis;
			return pCOMP_NONE;
		}
	}

	int16_t T = sTemp[TBOILER].get_Temp();  // текущая температура
	int16_t TRG = get_boilerTempTarget();   // целевая температура
#ifdef RPUMPBH
	if(GETBIT(Prof.Boiler.flags, fBoilerTogetherHeat) && (Status.modWork & pHEAT)) { // Режим одновременного нагрева бойлера с отоплением до температуры догрева
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
		if(FEED>Prof.Boiler.tempInLim) {
			Status.ret=pBh1; return pCOMP_OFF;    // Достигнута максимальная температура подачи ВЫКЛ
		}

		// Отслеживание выключения (с учетом догрева)
		if(!GETBIT(Prof.Boiler.flags, fTurboBoiler) && GETBIT(Prof.Boiler.flags, fAddHeating))// режим догрева, не турбо
		{
			if(T > Boiler_Target_AddHeating()) {
				Status.ret=pBh22; return pCOMP_OFF; // Температура выше целевой температуры ДОГРЕВА надо выключаться!
			}
		}
		if (T > TRG) {
			Status.ret=pBh3;
			set_HeatBoilerUrgently(false);
			return pCOMP_OFF;  // Температура выше целевой температуры БОЙЛЕРА надо выключаться!
		}
		// Отслеживание включения
		if(!onBoiler && T < TRG - (HeatBoilerUrgently ? 10 : Prof.Boiler.dTemp)) { // Температура ниже гистерезиса надо включаться!
			Status.ret = pBh2;
			return pCOMP_ON;
		}

		// дошли до сюда значить сохранение предыдущего состяния, температура в диапазоне регулирования может быть или нагрев или остывание
		if (onBoiler) {Status.ret=pBh4; return pCOMP_NONE;}  // Если включен принак работы бойлера (трехходовой) значит ПРОДОЛЖНЕНИЕ нагрева бойлера
		Status.ret=pBh5; return pCOMP_OFF;// продолжение ПАУЗЫ бойлера внутри гистерезиса

	} else {
	// Инвертор ПИД
#if defined(SUPERBOILER) && defined(PCON)
		if(PressToTemp(PCON) > Prof.Boiler.tempInLim) {
#else
 		if(FEED>Prof.Boiler.tempInLim) {
#endif
			Status.ret=pBp1; set_Error(ERR_PID_FEED,(char*)__FUNCTION__); return pCOMP_OFF;  // Достижение максимальной температуры подачи
		}
		// Отслеживание выключения (с учетом догрева)
		if(!GETBIT(Prof.Boiler.flags, fTurboBoiler) && GETBIT(Prof.Boiler.flags, fAddHeating))// режим догрева, не турбо
		{
			if(T > Boiler_Target_AddHeating()) {
				Status.ret=pBp22; return pCOMP_OFF;  // Температура выше целевой температуры ДОГРЕВА надо выключаться!
			}
		}
		if (T > TRG) {
			Status.ret=pBp3;
			set_HeatBoilerUrgently(false);
			return pCOMP_OFF;  // Температура выше целевой температуры БОЙЛЕРА надо выключаться!
		} else if(!onBoiler && T < TRG - (HeatBoilerUrgently ? 10 : Prof.Boiler.dTemp)) { // Отслеживание включения
			Status.ret = pBp2;
			return pCOMP_ON; // Достигнут гистерезис и компрессор еще не работает на ГВС - Старт бойлера
		} else if (is_compressor_on() &&(!(onBoiler))) return pCOMP_OFF;// компрессор работает, но ГВС греть не надо  - уходим без изменения состояния
		
		// ПИД ----------------------------------
        // Питание от резервного источника - ограничение мощности потребления от источника - это жесткое ограничение, по этому оно первое
        else if((GETBIT(Option.flags,fBackupPower))&&(getPower()>get_maxBackupPower())) { // Включено ограничение мощности и текущая мощность уже выше ограничения - надо менять частоту
#ifdef DEBUG_MODWORK
        	journal.jprintf("%s %.2f (BACKUP POWER: %.3f kW)\n",STR_REDUCED,dFC.get_stepFreqBoiler()/100.0,(float)getPower()/1000.0); // КИЛОВАТЫ
#endif
			if (dFC.get_target()-dFC.get_stepFreqBoiler()<dFC.get_minFreqBoiler())  { Status.ret=pBp29; return pCOMP_OFF; }   // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pBp28;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler());   // Уменьшить частоту
			resetPID();
			return pCOMP_NONE;
        }		
        
		// ЗАЩИТА Компресор работает, достигнута максимальная температура подачи, мощность, температура компрессора то уменьшить обороты на stepFreq
#if defined(SUPERBOILER) && defined(PCON)
		else if (is_compressor_on() &&(PressToTemp(PCON)>Prof.Boiler.tempInLim-dFC.get_dtTempBoiler())) // Ограничение, по температуре нагнетания для SUPERBOILER.
#else
		else if (is_compressor_on() &&(FEED>Prof.Boiler.tempInLim-dFC.get_dtTempBoiler()))             // Подача ограничение
#endif
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (FEED: %.2f)\n",STR_REDUCED,dFC.get_stepFreqBoiler()/100.0,FEED/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreqBoiler()<dFC.get_minFreqBoiler()) { Status.ret=pBp23; return pCOMP_OFF; }     // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pBp6;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler());    // Уменьшить частоту
			resetPID();
			return pCOMP_NONE;
		}
		else if (is_compressor_on() &&(dFC.get_power()>FC_MAX_POWER_BOILER))                    // Мощность для ГВС меньшая мощность
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (POWER: %.3f kW)\n",STR_REDUCED,dFC.get_stepFreqBoiler()/100.0,(float)dFC.get_power()/1000.0); // КИЛОВАТЫ
#endif
			if (dFC.get_target()-dFC.get_stepFreqBoiler()<dFC.get_minFreqBoiler())  { Status.ret=pBp24; return pCOMP_OFF; }    // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pBp7;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler());    // Уменьшить частоту
			resetPID();
			return pCOMP_NONE;
		}
		else if (is_compressor_on() &&(dFC.get_current()>FC_MAX_CURRENT_BOILER))                // ТОК для ГВС меньшая мощность
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (CURRENT: %.2f)\n",STR_REDUCED,dFC.get_stepFreqBoiler()/100.0,(float)dFC.get_current()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreqBoiler()<dFC.get_minFreqBoiler()) { Status.ret=pBp27; return pCOMP_OFF;  }    // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pBp16;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler());    // Уменьшить частоту
			resetPID();
			return pCOMP_NONE;
		}

		else if (is_compressor_on() &&((sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp())>sTemp[TCOMP].get_maxTemp()))  // температура компрессора
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (TCOMP: %.2f)\n",STR_REDUCED,dFC.get_stepFreqBoiler()/100.0,sTemp[TCOMP].get_Temp()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreqBoiler()<dFC.get_minFreqBoiler()) { Status.ret=pBp25; return pCOMP_OFF; }     // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pBp8;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler());    // Уменьшить частоту
			resetPID();
			return pCOMP_NONE;
		}
#ifdef PCON			
		else if (is_compressor_on() &&(sADC[PCON].get_present())&&(sADC[PCON].get_Value()>sADC[PCON].get_maxValue()-FC_DT_CON_PRESS)) // давление конденсатора до максимальной минус 0.5 бара
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (PCON:  %.2f)\n",STR_REDUCED,dFC.get_stepFreqBoiler()/100.0,sADC[PCON].get_Value()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreqBoiler()<dFC.get_minFreqBoiler()) { Status.ret=pBp26; return pCOMP_OFF; }     // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pBp9;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler());    // Уменьшить частоту
			resetPID();
			return pCOMP_NONE;
		}
#endif		
		else if(!is_compressor_on()) { Status.ret=pBp5; return pCOMP_OFF; }                                                    // Если компрессор не работает, то ничего не делаем и выходим
		else if(rtcSAM3X8.unixtime() - dFC.get_startTime() < FC_START_PID_DELAY/100 ) {	Status.ret=pBp10; return pCOMP_NONE; } // РАЗГОН частоту не трогаем
		else if(xTaskGetTickCount()-updatePidBoiler < get_timeBoiler()*1000)          { Status.ret=pBp11; return pCOMP_NONE; } // время обновления ПИДа еше не пришло

		// Дошли до сюда - ПИД на подачу. Компресор работает
		updatePidBoiler=xTaskGetTickCount();
		// Одна итерация ПИД регулятора (на выходе: алг.1 - ИЗМЕНЕНИЕ частоты, алг.2 - сама частота)
#ifdef SUPERBOILER
		Status.ret=pBp14;
        int16_t newFC = updatePID((Prof.Boiler.tempPID - PressToTemp(PCON)), Prof.Boiler.pid, pidw);
#else
		Status.ret=pBp12;
		int16_t newFC = updatePID(Prof.Boiler.tempPID - FEED, Prof.Boiler.pid, pidw);
#endif
#ifdef PID_FORMULA2
		if(newFC > dFC.get_target() + dFC.get_PidFreqStep()) newFC = dFC.get_target() + dFC.get_PidFreqStep(); // На увеличение
		else if(newFC < dFC.get_target() - dFC.get_PidMaxStep()) newFC = dFC.get_target() - dFC.get_PidMaxStep(); // На уменьшение
#else
		// Расчет целевой частоты с ограничением
		if(newFC > dFC.get_PidFreqStep()) newFC = dFC.get_PidFreqStep();
		else if(newFC < -dFC.get_PidMaxStep()) newFC = -dFC.get_PidMaxStep();
		newFC += dFC.get_target();
#endif
		if (newFC>dFC.get_maxFreqBoiler())   newFC=dFC.get_maxFreqBoiler();                                                 // ограничение диапазона ОТДЕЛЬНО для ГВС!!!! (меньше мощность)
		if (newFC<dFC.get_minFreqBoiler())   newFC=dFC.get_minFreqBoiler(); //return pCOMP_OFF;                             // Уменьшать дальше некуда, выключаем компрессор
	    if(GETBIT(Option.flags, fBackupPower) && newFC > dFC.get_maxFreqGen()) newFC = dFC.get_maxFreqGen();

		// Смотрим подход к границе защит если идет УВЕЛИЧЕНИЕ частоты
		if (dFC.get_target()<newFC && dFC.get_PidStop() < 100 && is_compressor_on())                                                                                     // Идет увеличение частоты проверяем подход к границам
		{
//			if (&&(FEED>(Prof.Boiler.tempInLim-dFC.get_dtTempBoiler())*dFC.get_PidStop()/100))                                                 {Status.ret=pBp17; resetPID(); return pCOMP_NONE;}   // Подача ограничение
			if ((dFC.get_power()>(FC_MAX_POWER_BOILER*dFC.get_PidStop()/100)))                                                            {Status.ret=pBp18; resetPID(); return pCOMP_NONE;}   // Мощность для ГВС меньшая мощность
			if ((dFC.get_current()>(FC_MAX_CURRENT_BOILER*dFC.get_PidStop()/100)))                                                        {Status.ret=pBp19; resetPID(); return pCOMP_NONE;}   // ТОК для ГВС меньшая мощность
			if (((sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp())>(sTemp[TCOMP].get_maxTemp()*dFC.get_PidStop()/100)))                      {Status.ret=pBp20; resetPID(); return pCOMP_NONE;}   // температура компрессора
			if ((sADC[PCON].get_present())&&(sADC[PCON].get_Value()>((sADC[PCON].get_maxValue()-FC_DT_CON_PRESS)*dFC.get_PidStop()/100))) {Status.ret=pBp21; resetPID(); return pCOMP_NONE;}   // давление конденсатора до максимальной минус 0.5 бара
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
		targetRealPID = settings.tempPID + (settings.kWeatherPID * (TEMP_WEATHER - sTemp[TOUT].get_Temp()) / 1000); // включена погодозависимость, коэффициент в ТЫСЯЧНЫХ результат в сотых градуса, определяем цель
		if(targetRealPID > settings.tempInLim - 50) targetRealPID = settings.tempInLim - 50;  // ограничение целевой подачи = максимальная подача - 0.5 градуса
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

	if ((rtcSAM3X8.unixtime()-offBoiler>Option.delayBoilerOff)&&((abs(FEED-RET)>Prof.Heat.dt)&&is_compressor_on()))	{ // Превышение разности температур кондесатора при включеноом компрессорае (есть задержка при переключении ГВС)
		set_Error(ERR_DTEMP_CON,(char*)__FUNCTION__);
		return pCOMP_NONE;
	}
	t1 = GETBIT(Prof.Heat.flags,fTarget) ? RET : sTemp[TIN].get_Temp();  // вычислить температуры для сравнения Prof.Heat.Target 0-дом, 1-обратка
	target = get_targetTempHeat();
	if(is_compressor_on() && !onBoiler) {
#ifdef R4WAY
		if(dRelay[R4WAY].get_Relay()) {	// 4-х ходовой в другом положении - ошибка
			set_Error(ERR_WRONG_HARD_STATE, (char*)__FUNCTION__);
			return pCOMP_NONE;
		}
#endif
#ifdef RPUMPFL
		if(GETBIT(Prof.Heat.flags, fHeatFloor)) {
			int16_t temp = STARTTEMP;
			for(uint8_t i = 0; i < TNUMBER; i++) {
				if(sTemp[i].get_setup_flag(fTEMP_HeatFloor) && temp > sTemp[i].get_Temp()) temp = sTemp[i].get_Temp();
			}
			if(temp != STARTTEMP) {
				if(temp < target) dRelay[RPUMPFL].set_ON(); else if(temp - HYSTERESIS_HeatFloor > target) dRelay[RPUMPFL].set_OFF();
			}// else if(!dRelay[RPUMPFL].get_Relay()) dRelay[RPUMPFL].set_ON();
		}
#endif
	}
	switch (Prof.Heat.Rule) // в зависмости от алгоритма
	{
	case pHYSTERESIS:  // Гистерезис нагрев.
		if(t1>target && rtcSAM3X8.unixtime() - startCompressor > (onBoiler || GETBIT(Option.flags, fBackupPower) ? 0 : Option.MinCompressorOn)) {Status.ret=pHh3; return pCOMP_OFF;} // Достигнута целевая температура  ВЫКЛ
		else if(t1 < target - (GETBIT(HP.Option.flags, fBackupPower) ? Prof.Heat.dTempGen : Prof.Heat.dTemp) && (!is_compressor_on() || onBoiler))  { Status.ret=pHh2;   return pCOMP_ON; } // Достигнут гистерезис ВКЛ
		else if(onBoiler) { return pCOMP_OFF; } // Бойлер нагрет и отопление не нужно
		else if(rtcSAM3X8.unixtime() - offBoiler > Option.delayBoilerOff && FEED > Prof.Heat.tempInLim) { Status.ret=pHh1; return pCOMP_OFF; } // Достигнута максимальная температура подачи ВЫКЛ (С учетом времени перехода с ГВС)
		else if(RET<Prof.Heat.tempOutLim) { Status.ret = pHh13; return pCOMP_ON; }   // Достигнут минимальная темература обратки ВКЛ
		else                              { Status.ret = pHh4; return pCOMP_NONE; }  // Ничего не делаем  (сохраняем состояние)
		break;
	case pPID:   // ПИД регулирует подачу, а целевай функция гистререзис
		// отработка гистререзиса целевой функции (дом/обратка)
		if(t1 > target && rtcSAM3X8.unixtime() - startCompressor > (onBoiler || GETBIT(Option.flags, fBackupPower) ? 0 : Option.MinCompressorOn)) { Status.ret=pHp3; return pCOMP_OFF; } // Достигнута целевая температура  ВЫКЛ
		else if(t1 < target - (GETBIT(HP.Option.flags, fBackupPower) ? Prof.Heat.dTempGen : Prof.Heat.dTemp) && (!is_compressor_on() || onBoiler)) { Status.ret=pHp2; return pCOMP_ON; } // Достигнут гистерезис (компрессор не работает) ВКЛ
		else if(onBoiler) { return pCOMP_OFF; } // Бойлер нагрет и отопление не нужно
		else if((rtcSAM3X8.unixtime()-offBoiler>Option.delayBoilerOff)&&(FEED>Prof.Heat.tempInLim)) {Status.ret=pHp1; set_Error(ERR_PID_FEED,(char*)__FUNCTION__);return pCOMP_OFF;}  // Достижение максимальной температуры подачи - это ошибка ПИД не работает (есть задержка срабатывания для переключения с ГВС)
       
        // Питание от резервного источника - ограничение мощности потребления от источника - это жесткое ограничение, по этому оно первое
	    else if((GETBIT(Option.flags,fBackupPower))&&(getPower()>get_maxBackupPower())) { // Включено ограничение мощности и текущая мощность уже выше ограничения - надо менять частоту
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (BACKUP POWER: %.3f kW)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,(float)getPower()/1000.0); // КИЛОВАТЫ
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreq())  { Status.ret=pHp29; return pCOMP_OFF; }   // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pHp28;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq());  // Уменьшить частоту
			resetPID();
			return pCOMP_NONE;
        }

		// ЗАЩИТА Компресор работает, достигнута максимальная температура подачи, мощность, температура компрессора или давление то уменьшить обороты на stepFreq
		else if(is_compressor_on() &&(FEED>Prof.Heat.tempInLim-dFC.get_dtTemp()))         // Подача ограничение (в разделе защита)
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (FEED: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,FEED/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreq()) {  Status.ret=pHp23; return pCOMP_OFF; }               // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pHp6;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq()); resetPID(); return pCOMP_NONE;// Уменьшить частоту
		}

		else if(is_compressor_on() &&(dFC.get_power()>FC_MAX_POWER))                    // Мощность в Вт
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (POWER: %.3f kW)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,dFC.get_power()/1000.0); // КИЛОВАТЫ
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreq()) {Status.ret=pHp24; return pCOMP_OFF;   }             // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pHp7;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq()); resetPID(); return pCOMP_NONE;                     // Уменьшить частоту
		}
		else if (is_compressor_on() &&(dFC.get_current()>FC_MAX_CURRENT))                // ТОК
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (CURRENT: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,dFC.get_current()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreq())  {Status.ret=pHp27;return pCOMP_OFF; }               // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pHp16;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq()); resetPID(); return pCOMP_NONE;                     // Уменьшить частоту
		}

		else if (is_compressor_on() &&((sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp())>sTemp[TCOMP].get_maxTemp()))  // температура компрессора
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (TCOMP: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,sTemp[TCOMP].get_Temp()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreq()) {Status.ret=pHp25; return pCOMP_OFF;  }              // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pHp8;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq()); resetPID(); return pCOMP_NONE;                     // Уменьшить частоту
		}
#ifdef PCON		
		else if (is_compressor_on() &&(sADC[PCON].get_present())&&(sADC[PCON].get_Value()>sADC[PCON].get_maxValue()-FC_DT_CON_PRESS))  // давление конденсатора до максимальной минус 0.5 бара
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (PCON:  %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,sADC[PCON].get_Value()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreq()) {   Status.ret=pHp26; return pCOMP_OFF; }               // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pHp9;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq()); resetPID();  return pCOMP_NONE;   // Уменьшить частоту
		}
#endif		
		else if(!(is_compressor_on())) {Status.ret=pHp5; return pCOMP_NONE;  }                                               // Если компрессор не рабоатет то ничего не делаем и выходим
        
		else if (rtcSAM3X8.unixtime()-dFC.get_startTime()<FC_START_PID_DELAY/100 ){ Status.ret=pHp10; return pCOMP_NONE;}     // РАЗГОН частоту не трогаем

#ifdef SUPERBOILER                                            // Бойлер греется от предкондесатора
		else if(sTemp[TCOMP].get_Temp() - (SUPERBOILER_DT - SUPERBOILER_DT_HYST) >= sTemp[TBOILER].get_Temp()
#ifdef RPUMPB
			&& !dRelay[RPUMPB].get_Relay()
#endif
		) dRelay[RSUPERBOILER].set_ON();
		else if(sTemp[TCOMP].get_Temp() - SUPERBOILER_DT < sTemp[TBOILER].get_Temp()
#ifdef RPUMPB
			|| dRelay[RPUMPB].get_Relay()
#endif
		) dRelay[RSUPERBOILER].set_OFF();
		else if(xTaskGetTickCount()-updatePidTime<get_timeHeat()*1000)         { Status.ret=pHp11;   return pCOMP_NONE;}   // время обновления ПИДа еше не пришло
		if(onBoiler) Status.ret=pHp15; else Status.ret=pHp12;            // если нужно показывем что бойлер греется от предкондесатора
#else
		else if(xTaskGetTickCount()-updatePidTime<get_timeHeat()*1000)    { Status.ret=pHp11;   return pCOMP_NONE;}   // время обновления ПИДа еше не пришло
		Status.ret=pHp12;   // Дошли до сюда - ПИД на подачу. Компресор работает
#endif

		// Одна итерация ПИД регулятора (на выходе: алг.1 - ИЗМЕНЕНИЕ частоты, алг.2 - сама частота)
		updatePidTime=xTaskGetTickCount();
		newFC = updatePID(CalcTargetPID(Prof.Heat) - FEED, Prof.Heat.pid, pidw);
#ifdef PID_FORMULA2
		if(newFC > dFC.get_target() + dFC.get_PidFreqStep()) newFC = dFC.get_target() + dFC.get_PidFreqStep(); // На увеличение
		else if(newFC < dFC.get_target() - dFC.get_PidMaxStep()) newFC = dFC.get_target() - dFC.get_PidMaxStep(); // На уменьшение
#else
		// Расчет целевой частоты с ограничением
		if(newFC > dFC.get_PidFreqStep()) newFC = dFC.get_PidFreqStep();
		else if(newFC < -dFC.get_PidMaxStep()) newFC = -dFC.get_PidMaxStep();
		newFC += dFC.get_target();
#endif

		if (newFC>dFC.get_maxFreq())   newFC=dFC.get_maxFreq();                                                // ограничение диапазона
		else if (newFC<dFC.get_minFreq())   newFC=dFC.get_minFreq();
	    if(GETBIT(Option.flags, fBackupPower) && newFC > dFC.get_maxFreqGen()) newFC = dFC.get_maxFreqGen();

		// Смотрим подход к границе защит если идет УВЕЛИЧЕНИЕ частоты
		if (dFC.get_target()<newFC && dFC.get_PidStop() < 100 && is_compressor_on())                                                                        // Идет увеличение частоты проверяем подход к границами если пересекли границы то частоту не меняем
		{
//    		if ((FEED>(targetRealPID*dFC.get_PidStop()/100)))                                                                            {Status.ret=pHp17; resetPID(); return pCOMP_NONE;}   // Подача ограничение, с учетом погодозависимости Подход снизу
			if ((dFC.get_power()>(FC_MAX_POWER*dFC.get_PidStop()/100)))                                                                  {Status.ret=pHp18; resetPID(); return pCOMP_NONE;}   // Мощность для ГВС меньшая мощность
			if ((dFC.get_current()>(FC_MAX_CURRENT*dFC.get_PidStop()/100)))                                                              {Status.ret=pHp19; resetPID(); return pCOMP_NONE;}   // ТОК для ГВС меньшая мощность
			if ((sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp())>(sTemp[TCOMP].get_maxTemp()*dFC.get_PidStop()/100))                       {Status.ret=pHp20; resetPID(); return pCOMP_NONE;}   // температура компрессора
			if ((sADC[PCON].get_present())&&(sADC[PCON].get_Value()>(sADC[PCON].get_maxValue()-FC_DT_CON_PRESS)*dFC.get_PidStop()/100))  {Status.ret=pHp21; resetPID(); return pCOMP_NONE;}   // давление конденсатора до максимальной минус 0.5 бара
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
	if(is_compressor_on()) {
		if(rtcSAM3X8.unixtime()-offBoiler > Option.delayBoilerOff && abs(FEED-RET) > Prof.Cool.dt) { // Привышение разности температур кондесатора при включеноом компрессорае
			set_Error(ERR_DTEMP_CON,(char*)__FUNCTION__);
			return pCOMP_NONE;
		}
//#ifdef R4WAY
//		if(!dRelay[R4WAY].get_Relay()) { // 4-х ходовой в другом положении - ошибка
//			set_Error(ERR_WRONG_HARD_STATE, (char*)__FUNCTION__);
//			return pCOMP_NONE;
//		}
//#endif
	}
	t1 = GETBIT(Prof.Cool.flags,fTarget) ? RET : sTemp[TIN].get_Temp(); // вычислить температуры для сравнения Prof.Heat.Target 0-дом   1-обратка
	target = get_targetTempCool();
	switch (Prof.Cool.Rule)   // в зависмости от алгоритма
	{
	case pHYSTERESIS:  // Гистерезис охлаждение.
		if(t1<target)             {Status.ret=pCh3;   return pCOMP_OFF;}                            // Достигнута целевая температура  ВЫКЛ
		else if(t1>target+Prof.Cool.dTemp && !is_compressor_on())  {Status.ret=pCh2;   return pCOMP_ON; }                       // Достигнут гистерезис ВКЛ
		else if((rtcSAM3X8.unixtime()-offBoiler>Option.delayBoilerOff)&&(FEED<Prof.Cool.tempInLim)){Status.ret=pCh1;return pCOMP_OFF;}// Достигнута минимальная температура подачи ВЫКЛ
		else if(onBoiler) { return pCOMP_OFF; } // Бойлер нагрет и охлаждение не нужно
		else if(RET>Prof.Cool.tempOutLim)      {Status.ret=pCh13;  return pCOMP_ON; }                       // Достигнут Максимальная темература обратки ВКЛ
		else  {Status.ret=pCh4;    return pCOMP_NONE;   }                                                // Ничего не делаем  (сохраняем состояние)
		break;
	case pPID:   // ПИД регулирует подачу, а целевай функция гистререзис
		// отработка гистререзиса целевой функции (дом/обратка)

		if(t1<target)     { Status.ret=pCp3; return pCOMP_OFF;}    // Достигнута целевая температура  ВЫКЛ
		else if ((t1>target+Prof.Cool.dTemp)&& !is_compressor_on())  {Status.ret=pCp2; return pCOMP_ON; }                          // Достигнут гистерезис (компрессор не рабоатет) ВКЛ
		else if(onBoiler) { return pCOMP_OFF; } // Бойлер нагрет и охлаждение не нужно
		else if ((rtcSAM3X8.unixtime()-offBoiler>Option.delayBoilerOff)&&(FEED<Prof.Cool.tempInLim)) {Status.ret=pCp1; set_Error(ERR_PID_FEED,(char*)__FUNCTION__);return pCOMP_OFF;}         // Достижение минимальной температуры подачи - это ошибка ПИД не рабоатет

        // Питание от резервного источника - ограничение мощности потребления от источника - это жесткое ограничение, по этому оно первое
	    else if((GETBIT(Option.flags,fBackupPower))&&(getPower()>get_maxBackupPower())) { // Включено ограничение мощности и текущая мощность уже выше ограничения - надо менять частоту
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (BACKUP POWER: %.3f kW)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,(float)getPower()/1000.0); // КИЛОВАТЫ
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreqCool())  { Status.ret=pCp29; return pCOMP_OFF; }   // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pCp28;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool());  // Уменьшить частоту
			resetPID();
			return pCOMP_NONE;
        }


		// ЗАЩИТА Компресор работает, достигнута минимальная температура подачи, мощность, температура компрессора или давление то уменьшить обороты на stepFreq
		else if (is_compressor_on() && (FEED<Prof.Cool.tempInLim+dFC.get_dtTemp()))                  // Подача
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (FEED: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,FEED/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreqCool()) {  Status.ret=pCp23; return pCOMP_OFF;  }              // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pCp6;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool());  resetPID();  return pCOMP_NONE;               // Уменьшить частоту
		}

		else if (is_compressor_on() &&(dFC.get_power()>FC_MAX_POWER))                    // Мощность
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (POWER: %.3f kW)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,dFC.get_power()/1000.0); // КИЛОВАТЫ
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreqCool()) {  Status.ret=pCp24; return pCOMP_OFF;  }         // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pCp7;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool());  resetPID();  return pCOMP_NONE;               // Уменьшить частоту
		}
		else if (is_compressor_on() &&(dFC.get_current()>FC_MAX_CURRENT))                    // ТОК
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (CURRENT: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,dFC.get_current()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreqCool()) {Status.ret=pCp27; return pCOMP_OFF; }          // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pCp16;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool());  resetPID(); return pCOMP_NONE;               // Уменьшить частоту
		}
		else if (is_compressor_on() &&((sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp())>sTemp[TCOMP].get_maxTemp()))  // температура компрессора
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (TCOMP: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,sTemp[TCOMP].get_Temp()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreqCool())  {Status.ret=pCp25;return pCOMP_OFF;}                // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pCp8;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool()); resetPID(); return pCOMP_NONE;               // Уменьшить частоту
		}
#ifdef PCON			
		else if (is_compressor_on() &&(sADC[PCON].get_present())&&(sADC[PCON].get_Value()>sADC[PCON].get_maxValue()-FC_DT_CON_PRESS))  // давление конденсатора до максимальной минус 0.5 бара
		{
#ifdef DEBUG_MODWORK
			journal.jprintf("%s %.2f (PCON:  %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,sADC[PCON].get_Value()/100.0);
#endif
			if (dFC.get_target()-dFC.get_stepFreq()<dFC.get_minFreqCool()) {Status.ret=pCp26;  return pCOMP_OFF;   }        // Уменьшать дальше некуда, выключаем компрессор
			Status.ret=pCp9;
			dFC.set_target(dFC.get_target()-dFC.get_stepFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool()); resetPID();  return pCOMP_NONE;               // Уменьшить частоту
		}
#endif		
		else if(!is_compressor_on()) {Status.ret=pCp5; return pCOMP_NONE;  }                                               // Если компрессор не рабоатет то ничего не делаем и выходим
		else if (rtcSAM3X8.unixtime()-dFC.get_startTime()<FC_START_PID_DELAY/100 ){ Status.ret=pCp10; return pCOMP_NONE;}     // РАЗГОН частоту не трогаем

#ifdef SUPERBOILER                                            // Бойлер греется от предкондесатора
		if (sTemp[TCOMP].get_Temp()+SUPERBOILER_DT>sTemp[TBOILER].get_Temp())  dRelay[RSUPERBOILER].set_ON(); else dRelay[RSUPERBOILER].set_OFF();
		if(xTaskGetTickCount()-updatePidTime<get_timeHeat()*1000)         { Status.ret=pCp11;   return pCOMP_NONE;}   // время обновления ПИДа еше не пришло
		if (onBoiler) Status.ret=pCp15; else Status.ret=pCp12;                                          // если нужно показывем что бойлер греется от предкондесатора
#else
		else if(xTaskGetTickCount()-updatePidTime<get_timeHeat()*1000)    { Status.ret=pCp11;   return pCOMP_NONE;}   // время обновления ПИДа еше не пришло
		Status.ret=pCp12;   // Дошли до сюда - ПИД на подачу. Компресор работает
#endif

		// Одна итерация ПИД регулятора (на выходе: алг.1 - ИЗМЕНЕНИЕ частоты, алг.2 - сама частота)
		updatePidTime=xTaskGetTickCount();
		newFC = updatePID(FEED - CalcTargetPID(Prof.Cool), Prof.Cool.pid, pidw);      // Одна итерация ПИД регулятора (на выходе ИЗМЕНЕНИЕ частоты)
#ifdef PID_FORMULA2
		if(newFC > dFC.get_target() + dFC.get_PidFreqStep()) newFC = dFC.get_target() + dFC.get_PidFreqStep(); // На увеличение
		else if(newFC < dFC.get_target() - dFC.get_PidMaxStep()) newFC = dFC.get_target() - dFC.get_PidMaxStep(); // На уменьшение
#else
		// Расчет целевой частоты с ограничением
		if(newFC > dFC.get_PidFreqStep()) newFC = dFC.get_PidFreqStep();
		else if(newFC < -dFC.get_PidMaxStep()) newFC = -dFC.get_PidMaxStep();
		newFC += dFC.get_target();
#endif

		if (newFC>dFC.get_maxFreqCool())   newFC=dFC.get_maxFreqCool();                                       // ограничение диапазона
		if (newFC<dFC.get_minFreqCool())   newFC=dFC.get_minFreqCool(); // return pCOMP_OFF;                                              // Уменьшать дальше некуда, выключаем компрессор// newFC=minFreq;
	    if(GETBIT(Option.flags, fBackupPower) && newFC > dFC.get_maxFreqGen()) newFC = dFC.get_maxFreqGen();

		//    journal.jprintf("newFC=%.2f\n",newFC/100.0);

		// Смотрим подход к границе защит если идет УВЕЛИЧЕНИЕ частоты
		if (dFC.get_target()<newFC && is_compressor_on())                                                                                     // Идет увеличение частоты проверяем подход к границам
		{
//    		if ((FEED<(targetRealPID*(100+(100-dFC.get_PidStop()))/100)))                                                                     {Status.ret=pCp17; resetPID(); return pCOMP_NONE;}   // Подача ограничение, с учетом погодозависимости Подход сверху
			if ((dFC.get_power()>(FC_MAX_POWER*dFC.get_PidStop()/100)))                                                                       {Status.ret=pCp18; resetPID(); return pCOMP_NONE;}   // Мощность для ГВС меньшая мощность
			if ((dFC.get_current()>(FC_MAX_CURRENT*dFC.get_PidStop()/100)))                                                                   {Status.ret=pCp19; resetPID(); return pCOMP_NONE;}   // ТОК для ГВС меньшая мощность
			if ((sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp()>(sTemp[TCOMP].get_maxTemp()*dFC.get_PidStop()/100)))                            {Status.ret=pCp20; resetPID(); return pCOMP_NONE;}   // температура компрессора
			if ((sADC[PCON].get_present())&&(sADC[PCON].get_Value()>(sADC[PCON].get_maxValue()-FC_DT_CON_PRESS)*dFC.get_PidStop()/100))       {Status.ret=pCp21; resetPID(); return pCOMP_NONE;}   // давление конденсатора до максимальной минус 0.5 бара
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

// UPDATE --------------------------------------------------------------------------------
// Итерация по управлению всем ТН, для всего, основной цикл управления.
void HeatPump::vUpdate()
{

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

	Status.modWork = get_Work();                                         // определяем что делаем
#ifdef DEBUG_MODWORK
	save_DumpJournal(false);                                           // Вывод строки состояния
#endif
	if(error) return;
	//  реализуем требуемый режим
	if(Status.modWork == pOFF) {
		if(is_compressor_on()) {  // ЕСЛИ компрессор работает, то выключить компрессор,и затем сконфигурировать 3 и 4-х клапаны и включаем насосы
			compressorOFF();
			configHP(Status.modWork);
			if(!startPump && get_modeHouse() != pOFF)  // Когда режим выключен (не отопление и не охлаждение), то насосы отопления крутить не нужно
			{
				startPump = true;                                 // Поставить признак запуска задачи насос
				if(get_workPump()) journal.jprintf(" %s: Pumps in pause %s. . .\n", (char*) __FUNCTION__, "ON");     // Включить задачу насос кондесатора выключение в переключении насосов
			}
			command_completed = rtcSAM3X8.unixtime(); // поменялся режим
		}
	} else if(!(Status.modWork & pCONTINUE)) { // Начало режимов, Включаем задачу насос, конфигурируем 3 и 4-х клапаны включаем насосы и потом включить компрессор
		if(startPump)                                     // Остановить задачу насос
		{
			startPump = false;                            // Поставить признак останова задачи насос
			if(get_workPump()) journal.jprintf(" %s: Pumps in pause %s. . .\n",(char*)__FUNCTION__, "OFF");
		    command_completed = rtcSAM3X8.unixtime(); // поменялся режим
		}
		if(!check_compressor_pause()) {
			if(configHP(Status.modWork)) {                   // Конфигурируем насосы
				compressorON();                              // Включаем компрессор
			}
		}
	}
}

// Получить информацию что надо делать сейчас (на данной итерации)
// Управляет дополнительным нагревателем бойлера, на выходе что будет делать ТН
MODE_HP HeatPump::get_Work()
{
	MODE_HP ret = pOFF;
	Status.prev = Status.ret;
	Status.ret = pNone;           // не определено

	// 1. Бойлер (определяем что делать с бойлером)
	switch((int) UpdateBoiler())  // проверка бойлера высший приоритет
	{
	case pCOMP_OFF:
		if(onBoiler) {
			if(Status.ret == pBh22 || Status.ret == pBp22) flagRBOILER = true;
			journal.jprintf(" Stop Boiler [%s]\n", (char *)codeRet[get_ret()]);
		}
		ret = pOFF;
		break;
	case pCOMP_ON:
		if(GETBIT(Option.flags, fBackupPower) && !GETBIT(Prof.Boiler.flags, fWorkOnGenerator)) {
			Status.ret = pBgen;
			ret = pOFF;
		} else ret = pBOILER;
		break;
	case pCOMP_NONE:
		ret = pBOILER + pCONTINUE;
		break;
	}

#ifdef DEBUG_MODWORK
	journal.printf(" get_Work-Boiler: %s(%s), ret=%X, onBoiler=%d, flagRBOILER=%d\n", codeRet[Status.ret], codeRet[Status.prev], ret, onBoiler, flagRBOILER);
#endif
	if(((get_modeHouse() == pOFF) && (ret == pOFF)) || error) return ret; // режим ДОМА выключен (т.е. запрещено отопление или охлаждение дома) И бойлер надо выключить, то выходим с сигналом pOFF (переводим ТН в паузу)
	if((ret & pBOILER)) return ret; // работает бойлер больше ничего анализировать не надо выход

	// 3. Отопление/охлаждение
	switch((int) get_modeHouse())   // проверка отопления
	{
	case pOFF:
		ret = pOFF;
		break;
	case pHEAT:
		switch((int) UpdateHeat()) {
		case pCOMP_OFF:
			ret = pOFF;
			break;
		case pCOMP_ON:
			ret = pHEAT;
			break;
		case pCOMP_NONE:
			if(is_compressor_on()) {
				ret = pHEAT;
				if(!onBoiler) ret += pCONTINUE;
			}
			break;
		}
		break;
	case pCOOL:
		switch((int) UpdateCool()) {
		case pCOMP_OFF:
			ret = pOFF;
			break;
		case pCOMP_ON:
			ret = pCOOL;
			break;
		case pCOMP_NONE:
			if(is_compressor_on()) {
				ret = pCOOL;
				if(!onBoiler) ret += pCONTINUE;
			}
			break;
		}
		break;
	}
#ifdef RHEAT  // Дополнительный тэн для нагрева отопления
if(!GETBIT(Option.flags,fBackupPower)){ // Нет питания от резервного источника
	if (GETBIT(Option.flags,fAddHeat))
	{
		if(!GETBIT(Option.flags,fTypeRHEAT)) // резерв
		{
			if (((sTemp[TIN].get_Temp()>Option.tempRHEAT)&&(dRelay[RHEAT].get_Relay()))||((ret==pOFF)&&(dRelay[RHEAT].get_Relay()))) {journal.jprintf(" TIN=%.2f, add heatting off . . .\n",sTemp[TIN].get_Temp()/100.0); dRelay[RHEAT].set_OFF();} // Гистерезис 0.2 градуса что бы не щелкало
			if ((sTemp[TIN].get_Temp()<Option.tempRHEAT-HYSTERESIS_RHEAD)&&(!dRelay[RHEAT].get_Relay())&&(ret!=pOFF)) {journal.jprintf(" TIN=%.2f, add heatting on . . .\n",sTemp[TIN].get_Temp()/100.0); dRelay[RHEAT].set_ON();}
		}
		else                                // бивалент
		{
			if ( ((sTemp[TOUT].get_Temp()>Option.tempRHEAT)&&(dRelay[RHEAT].get_Relay()))||((ret==pOFF)&&(dRelay[RHEAT].get_Relay())) ) {journal.jprintf(" TOUT=%.2f, add heatting off . . .\n",sTemp[TOUT].get_Temp()/100.0);dRelay[RHEAT].set_OFF();} // Гистерезис 0.2 градуса что бы не щелкало
			if ((sTemp[TOUT].get_Temp()<Option.tempRHEAT-HYSTERESIS_RHEAD)&&(!dRelay[RHEAT].get_Relay())&&(ret!=pOFF)) {journal.jprintf(" TOUT=%.2f, add heatting on . . .\n",sTemp[TOUT].get_Temp()/100.0); dRelay[RHEAT].set_ON();}
		}
	}
}	
else if(dRelay[RHEAT].get_Relay()) dRelay[RHEAT].set_OFF();// есть питание от резервного источника - запрет использования электрокотла, если надо выключаем
#endif
#ifdef DEBUG_MODWORK
	journal.printf(" get_Work: %s, ret=%X\n", codeRet[Status.ret], ret);
#endif
	return ret;
}

// Концигурация 4-х, 3-х ходового крана и включение насосов, тен бойлера, тен отопления
// В зависммости от входного параметра конфигурирует краны, выход true - разрешен запуск компрессора
boolean HeatPump::configHP(MODE_HP conf)
{
	if(conf == pOFF) { // ЭТО может быть пауза! Выключить - установить положение как при включении ( перевод 4-х ходового производится после отключения компрессора (см compressorOFF()))
		#ifdef SUPERBOILER                                             // Бойлер греется от предкондесатора
		 dRelay[RSUPERBOILER].set_OFF();                            // Евгений добавил выключить супербойлер
		#endif
		#ifdef RBOILER
			if((GETBIT(Prof.Boiler.flags,fTurboBoiler))&&(dRelay[RBOILER].get_present())) dRelay[RBOILER].set_OFF();  // Выключить ТЭН бойлера в режиме турбо (догрев не работате)
		#endif
		#ifdef RHEAT
			if(dRelay[RHEAT].get_present()) dRelay[RHEAT].set_OFF();     // Выключить ТЭН отопления
		#endif
		PUMPS_OFF;
		//switchBoiler(false);                                            // выключить бойлер
		//_delay(DELAY_AFTER_SWITCH_RELAY);                               // Задержка
	} else if((conf & pHEAT)) {    // Отопление
		if(Switch_R4WAY(false)) return false; 							  // 4-х ходовой на нагрев
		if(is_compressor_on()) {                                          // Компрессор рабоатет, переключаемся на ходу 
			switchBoiler(false);                                          // выключить бойлер 
		} else {
			PUMPS_ON;                                                     // включить насосы
			dFC.set_target(dFC.get_startFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq());  // установить стартовую частоту если компрессор выключен
		}
		#ifdef SUPERBOILER                                            // Бойлер греется от предкондесатора
			dRelay[RSUPERBOILER].set_OFF();                             // Евгений добавил выключить супербойлер
		#endif
		#ifdef RBOILER
		 	 if((GETBIT(Prof.Boiler.flags,fTurboBoiler))&&(dRelay[RBOILER].get_present())) dRelay[RBOILER].set_OFF(); // Выключить ТЭН бойлера (режим форсированного нагрева)
		#endif
		#ifdef RHEAT
		//if((GETBIT(Option.flags,fAddHeat))&&(dRelay[RHEAT].get_present())) dRelay[RHEAT].set_ON(); else dRelay[RHEAT].set_OFF(); // Если надо включить ТЭН отопления
		#endif
	} else if((conf & pCOOL)) {  // Охлаждение
		if(Switch_R4WAY(true)) return false; 							   // 4-х ходовой на охлаждение
		if(is_compressor_on()) {                                           // Компрессор рабоатет, переключаемся на ходу   
			switchBoiler(false);                                           // выключить бойлер
		} else {
			PUMPS_ON;                                                     // включить насосы
			dFC.set_target(dFC.get_startFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool());   // установить стартовую частоту
		}
		#ifdef RBOILER
			if((GETBIT(Prof.Boiler.flags,fTurboBoiler)) && (dRelay[RBOILER].get_present())) dRelay[RBOILER].set_OFF(); // Выключить ТЭН бойлера (режим форсированного нагрева)
		#endif
		#ifdef RHEAT
		  	  if (dRelay[RHEAT].get_present()) dRelay[RHEAT].set_OFF();     // Выключить ТЭН отопления
		#endif
	} else if((conf & pBOILER)) {  // Бойлер
		if(Switch_R4WAY(false)) return false; 	 					   // 4-х ходовой на нагрев
		if(is_compressor_on()) {                                       // Компрессор рабоатет, переключаемся на ходу
			switchBoiler(true);                                        // включить бойлер
#ifdef SUPERBOILER
			dRelay[PUMP_OUT].set_OFF();                                // Евгений добавил
			_delay(DELAY_AFTER_SWITCH_RELAY);                          // Задержка
			dRelay[RSUPERBOILER].set_ON();                             // Евгений добавил
#endif
		} else {
#ifdef SUPERBOILER
			dRelay[PUMP_IN].set_ON();                                  // Реле включения насоса входного контура  (геоконтур)
			_delay(DELAY_AFTER_SWITCH_RELAY);                          // Задержка
			dRelay[PUMP_OUT].set_OFF();                                // Евгений добавил
			_delay(DELAY_AFTER_SWITCH_RELAY);                          // Задержка
			dRelay[RSUPERBOILER].set_ON();                             // Евгений добавил
			_delay(DELAY_AFTER_SWITCH_RELAY);                          // Задержка
			switchBoiler(true);                                        // включить бойлер
			if(Status.ret<pBp5) dFC.set_target(SUPERBOILER_FC,true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler()); // В режиме супер бойлер установить частоту SUPERBOILER_FC если не дошли до пида
#else
			PUMPS_ON;           // включить насосы
			if(Status.ret < pBp5) dFC.set_target(dFC.get_startFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler()); // установить стартовую частоту
#endif
		}
		#ifdef RHEAT
			if (dRelay[RHEAT].get_present()) dRelay[RHEAT].set_OFF();     // Выключить ТЭН отопления
		#endif
	} else if((conf & pDEFROST)) {  // Разморозка
		// to do...
	} else { // ?
		set_Error(ERR_CONFIG,(char*)__FUNCTION__);   // Ошибка!  что то пошло не так
		return false;
	}
	return true;
}

// Переключение реверсивного 4-х ходового клапана (true - охлаждение, false - нагрев), выход - компрессор остановлен
// останов компрессора и обезпечение нужных пауз, компрессор включается далее в vUpdate()
boolean HeatPump::Switch_R4WAY(boolean fCool)
{
#ifdef R4WAY
	if(fCool == dRelay[R4WAY].get_Relay()) return false;
	journal.jprintf("Switch to %s\n", fCool ? "COOL" : "HEAT");
	boolean ret = false;
	if(is_compressor_on()) {
		compressorOFF();
		ret = true;
	}
	if(fCool) dRelay[R4WAY].set_ON(); else dRelay[R4WAY].set_OFF();
	_delay(DELAY_AFTER_SWITCH_RELAY);                        // Задержка
//	пауза не нужна, отстоялись в насосах
//	journal.jprintf(" Pressure equalization...\n");
//	_delay(Option.delayR4WAY * 1000);                                   // Пауза 120 секунд для выравнивания давлений
#else // R4WAY
    boolean ret = false; 
	if(fCool) journal.jprintf("No 4-way valve avalilable!\n");
#endif
	return ret;
}

// проверка на паузу между включениями, возврат true - в паузе
boolean HeatPump::check_compressor_pause()
{
	uint16_t pause = (Status.modWork & (pHEAT | pCOOL)) ? Prof.Heat.CompressorPause : Option.pause;
	if(stopCompressor && rtcSAM3X8.unixtime() - stopCompressor < pause) {
		if(!compressor_in_pause) journal.jprintf_time("Waiting compressor, pause %d s...\n", pause - (rtcSAM3X8.unixtime() - stopCompressor));
		return compressor_in_pause = true;
	}
	return compressor_in_pause = false;
}

// Попытка включить компрессор  с учетом всех защит КОНФИГУРАЦИЯ уже установлена
// Вход режим работы ТН
// Возможно компрессор уже включен и происходит только смена режима
const char *EEV_go={" EEV go "};  // экономим место
void HeatPump::compressorON()
{
	if(get_State() == pOFF_HP || get_State() == pSTOPING_HP || error) return;  // ТН выключен или выключается выходим ничего не делаем!!!

	if(is_compressor_on()) return;                                  // Компрессор уже работает
	if(is_next_command_stop()) {
xNextStop:
		journal.jprintf(" Next command stop(%d), skip start", next_command);
		return;
	}

#ifdef AUTO_START_GENERATOR
	if(GETBIT(Option.flags, fBackupPower)) {
		dRelay[RGEN].set_ON(); // Включаем или не даем выключиться
		if(dFC.get_state() == ERR_LINK_FC) {
			_delay(AUTO_START_GENERATOR * 1000); // Задержка на запуск, в том числе и для прогрева генератора
			for(uint16_t i = AUTO_START_GEN_TIMEOUT / (FC_TIME_READ / 1000); i > 0; i--) {
				if(NO_Power) return;
				if(is_next_command_stop()) goto xNextStop;
				if(dFC.get_err() == OK) break;
				_delay(FC_TIME_READ);
			}
			if(dFC.get_err() != OK) {
				set_Error(ERR_FC_NO_LINK, (char*) __FUNCTION__);
				return;
			}
		}
	}
#endif
	if(ResetFC() != OK) {                                // Сброс инвертора если нужно
		set_Error(ERR_RESET_FC, (char*) __FUNCTION__);
		return;
	}

#ifdef EEV_DEF
	if(lastEEV != -1) {         // Не первое включение компрессора после старта ТН
		// 1. Обеспечение минимальной паузы компрессора
		if(compressor_in_pause) return;
#ifdef DEBUG_MODWORK
		journal.jprintf_time("compressorON > modWork:%X[%s], now %s\n",get_modWork(),codeRet[Status.ret], is_compressor_on() ? "ON" : "OFF");
#endif
	}//get_fEEVStartPosByTemp()
	// 2. Разбираемся с ЭРВ
	journal.jprintf(EEV_go);
	if(dEEV.get_LightStart()) { // Выйти на пусковую позицию
		dEEV.set_EEV(dEEV.get_preStartPos());
#ifdef EEV_PREFER_PERCENT
		journal.jprintf("preStartPos: %.2d\n", dEEV.calc_percent(dEEV.get_preStartPos()));
#else
		journal.jprintf("preStartPos: %d\n", dEEV.get_preStartPos());
#endif
	} else if(dEEV.get_StartFlagPos()) { // Всегда начинать работу ЭРВ со стартовой позиции
		dEEV.set_EEV(dEEV.get_StartPos());
#ifdef EEV_PREFER_PERCENT
		journal.jprintf("StartPos: %.2d\n", dEEV.calc_percent(dEEV.get_EEV()));
#else
		journal.jprintf("StartPos: %d\n", dEEV.get_EEV());
#endif

	} else if(lastEEV != -1) { // установка последнего значения ЭРВ
		dEEV.set_EEV(lastEEV);
#ifdef EEV_PREFER_PERCENT
		journal.jprintf("lastEEV: %.2d\n", dEEV.calc_percent(lastEEV));
#else
		journal.jprintf("lastEEV: %d\n", lastEEV);
#endif
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
			if(get_workPump()) journal.jprintf(" WARNING! %s: Pumps in pause, OFF . . .\n",(char*)__FUNCTION__);
		}
	#ifdef DEFROST
	  if(!(get_modWork() & pDEFROST))  // При разморозке есть лишние проверки
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
		journal.jprintf_time("Pause %d s before start compressor\n", Option.delayOnPump);
#endif
		uint16_t d = Option.delayOnPump;
#ifdef FLOW_CONTROL
		//for(uint8_t i = 0; i < FNUMBER; i++) sFrequency[i].reset();  // Сброс счетчиков протока
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
			 if(sFrequency[i].get_checkFlow() && sFrequency[i].get_Value() < sFrequency[i].get_minValue()) {  // Поток меньше минимального
				_delay(TIME_READ_SENSOR);
				if(sFrequency[i].get_Value() < sFrequency[i].get_minValue()) {  // Поток меньше минимального
					journal.jprintf(" Flow %s: %.3d\n", sFrequency[i].get_name(), sFrequency[i].get_Value());
					set_Error(ERR_MIN_FLOW, (char*) sFrequency[i].get_name());
					return;
				}
			}
		}	
#endif
#ifdef SEVA  //Если определен лепестковый датчик протока - это переливная схема ТН - надо контролировать проток при работе
		if(dRelay[RPUMPI].get_Relay())                                                                                             // Только если включен насос геоконтура  (PUMP_IN)
			if (sInput[SEVA].get_Input()==SEVA_OFF) {set_Error(ERR_SEVA_FLOW,(char*)"SEVA"); return;}                              // Выход по ошибке отсутствия протока
#endif

#ifdef DEFROST
   }  // if(!(mod & pDEFROST))
#endif	
		resetPID(); 										// Инициализировать переменные ПИД регулятора
		if(Charts_when_comp_on) task_updstat_chars = 0;
	    command_completed = rtcSAM3X8.unixtime();
	  	COMPRESSOR_ON;                                      // Включить компрессор
		if(error || dFC.get_err()) return; // Ошибка - выходим
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
#ifdef EEV_PREFER_PERCENT
			journal.jprintf("StartPos: %.2d\n", dEEV.calc_percent(dEEV.get_EEV()));
#else
			journal.jprintf("StartPos: %d\n", dEEV.get_EEV());
#endif
		}    // если первая итерация или установлен соответсвующий флаг то на стартовую позицию
		else { // установка последнего значения ЭРВ в противном случае
			dEEV.set_EEV(lastEEV);
#ifdef EEV_PREFER_PERCENT
			journal.jprintf("lastEEV: %.2d\n", dEEV.calc_percent(lastEEV));
#else
			journal.jprintf("lastEEV: %d\n", lastEEV);
#endif
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
		 if(!(get_modWork() & pDEFROST)) journal.jprintf_time("%s WORK . . .\n",(char*)nameHeatPump);     // Сообщение о работе
		 else journal.jprintf_time("%s DEFROST . . .\n",(char*)nameHeatPump);               // Сообщение о разморозке
		#else
		journal.jprintf_time("%s WORK . . .\n",(char*)nameHeatPump);     // Сообщение о работе
		#endif
	}
	else  // признак первой итерации
	{
		set_startTime(rtcSAM3X8.unixtime());                         // Запомнить время старта ТН
		lastEEV = dEEV.get_EEV();                                 // ЭРВ рабоатет запомнить
		dEEV.Resume();
		vTaskResume(xHandleUpdateEEV);                               // Запустить задачу Обновления ЭРВ
		journal.jprintf(" Start task UpdateEEV\n");
	}
#else
	lastEEV = 1;                                                   // Признак первой итерации
	set_startTime(rtcSAM3X8.unixtime());                         // Запомнить время старта ТН
#endif
}

// попытка выключить компрессор  с учетом всех защит
void HeatPump::compressorOFF()
{
#ifdef EEV_DEF
	lastEEV = dEEV.get_EEV();                                             // Запомнить последнюю позицию ЭРВ
	dEEV.Pause();                                                       // Поставить на паузу задачу Обновления ЭРВ
	journal.jprintf(" Stop control EEV\n");
#endif

	command_completed = rtcSAM3X8.unixtime();
	if(is_compressor_on()) {
		COMPRESSOR_OFF;                                             // Компрессор выключить
	}

#ifdef REVI
	checkEVI();                                                     // выключить ЭВИ
#endif

	PUMPS_OFF;                                                          // выключить насосы + задержка

	onBoiler = false;
	offBoiler = rtcSAM3X8.unixtime();

#ifdef EEV_DEF
	if(dEEV.get_EevClose())                                 // Hазбираемся с ЭРВ
	{
		journal.jprintf(" Pause before closing EEV %d sec . . .\n", dEEV.get_delayOff());
		_delay(dEEV.get_delayOff() * 1000);                                // пауза перед закрытием ЭРВ  на инверторе компрессор останавливается до 2 минут
		dEEV.set_EEV(EEV_CLOSE_STEP);                                    // Если нужно, то закрыть ЭРВ
		journal.jprintf(" EEV closed\n");
	}
#endif
	//journal.jprintf_time("%s PAUSE . . .\n",(char*)nameHeatPump);    // Сообщение о паузе
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
      
      #ifdef R4WAY            // Нет четырехходового - нет режима охлаждения
  if (dRelay[R4WAY].get_Relay() == false) return;                          // режим охлаждения - размораживать не надо, у меня false это охлаждение.
#else
 set_Error(ERR_DEFROST_R4WAY, (char*)__FUNCTION__);return;                 // Четырех ходового нет разморозка не возможна - это косяк конфигурации
      #endif
         
// Реализовано два алгоритма выбор по наличию SFROZEN
#ifdef SFROZEN    // Алгоритм разморозки по датчику  SFROZEN             
      if (sInput[SFROZEN].get_Input()==SFROZEN_OFF) {startDefrost=0;return;  }    // размораживать не надо - датчик говорит что все ок
      
      // организация задержки перед включением
      if (startDefrost==0) startDefrost=xTaskGetTickCount();               // первое срабатывание датчика - запоминаем время (тики)
      if (xTaskGetTickCount()-startDefrost<Option.delayDefrostOn*1000)  return; //  Еще рано размораживать
      // придется размораживать
       journal.jprintf("Start defrost\n");
       Switch_R4WAY(true);  // охлаждение
       
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
	    if (dRelay[R4WAY].get_Relay() == true) // Если четырехходовой стоит на тепло - Главный вариант
	    {
	    	// Компрессор рабатает и 4-х ходовой стоит на тепле то хитро переключаем 4-х ходовой в положение холод
	    	compressorOFF();
		    if (sTemp[TOUT].get_Temp()<=TEMP_STEAM_DEFROST)                        // Если температура на улице ниже или равна TEMP_STEAM_DEFROST то оттаиваем паром
			     {
			      dRelay[PUMP_IN].set_OFF();                                        // выключаем вентиляторы
			     _delay(1*1000);
			      dRelay[PUMP_IN1].set_OFF();                                       // выключаем вентиляторы	
			     }
		    }
	    else  { set_Error(ERR_DEFROST_R4WAY, (char*)__FUNCTION__);return; }           // ХА ХА  Работаем на охлаждение но при этом требуется разморозка - Это косяк Карл, но до сюда не доедит
	  	
	  }
	  else  // Компрессор не работает - тяжелый случай все делаем руками
	  {
	   dRelay[R4WAY].set_OFF();                                                       // переключаем 4ходовик на оттайку
	   _delay(1*1000);
	   dRelay[PUMP_OUT].set_OFF();                                                   // Включение насоса выходного контура
		 if (sTemp[TOUT].get_Temp()>TEMP_STEAM_DEFROST)                              // Если температура на улице ниже или равна TEMP_STEAM_DEFROST то оттаиваем паром 	
		 {
		  dRelay[PUMP_IN].set_ON();                                                  // включаем вентиляторы
		 _delay(1*1000);
		  dRelay[PUMP_IN1].set_ON();                                                 // включаем вентиляторы	
		 }
	      compressorON();                                                    // включить компрессор на разморозку
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
		journal.jprintf_time("Run: %s\n", get_command_name(command));

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
		    PauseStart = 0;
			StopWait(_stop);        // Выключить ТН
			break;
		case pRESET:                          // 4 Сброс контроллера
		    PauseStart = 0;
			StopWait(_stop);        // Выключить ТН
			journal.jprintf_time("$SOFTWARE RESET . . .\n\n");
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
		case pPROG_FC:                                             // Программировать инвертор первоначальными данными (настройка инвертора)
		    #ifndef FC_VACON  // Omron , если в ваком будет определен метод progFC этот дефайн убрать
            dFC.progFC();
            #endif
			break;
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
	case pOFF_HP:     return (char*)(PauseStart == 0 ? "Выключен" : "Перезапуск...");  break;   // 0 ТН выключен или Перезапуск
	case pSTARTING_HP:return (char*)"Пуск...";   break;         // 1 Стартует
	case pSTOPING_HP: return (char*)"Останов...";break;         // 2 Останавливается
	case pWORK_HP:                                              // 3 Работает
		if(!is_compressor_on()) {
			if(get_modWork() == pHEAT) return (char*)"Ожид. Нагр.";         // Включить отопление
			if(get_modWork() == pCOOL) return (char*)"Ожид. Охл.";          // Включить охлаждение
			if(get_modWork() == pBOILER) return (char*)"Ожид. ГВС";         // Включить бойлер
			return (char*)strRusPause;
		} else {
			if(get_modWork() == pOFF) return (char*)strRusPause;
			if(get_modWork() & pHEAT)    return (char*)"Отопление";
			if(get_modWork() & pCOOL)    return (char*)"Охлаждение";
			if(get_modWork() & pBOILER)  return (char*)"ГВС";
			if(get_modWork() & pDEFROST) return (char*)"Разморозка";
			return (char*)"...";
		}
	case pWAIT_HP:    return (char*)"Ожидание";         	// 4 Ожидание
	case pERROR_HP:   return (char*)"Ошибка";  				// 5 Ошибка ТН
	}
	return (char*)"...."; 							        // 6 - Эта ошибка возникать не должна!
}

// Получить строку состояния ТН в виде строки АНГЛИСКИЕ буквы
char *HeatPump::StateToStrEN()
{
	switch ((int)get_State())  //TYPE_STATE_HP
	{
	case pOFF_HP:     return (char*)(PauseStart == 0 ? "Off" : "Restart...");  break;   // 0 ТН выключен или Перезапуск
	case pSTARTING_HP:return (char*)"Start...";   break;         // 1 Стартует
	case pSTOPING_HP: return (char*)"Stop...";    break;         // 2 Останавливается
	case pWORK_HP:                                               // 3 Работает
		if(!is_compressor_on()) {
			if(get_modWork() == pHEAT)   return (char*)"Wait Heat";         // Включить отопление
			if(get_modWork() == pCOOL)   return (char*)"Wait Cool";         // Включить охлаждение
			if(get_modWork() == pBOILER) return (char*)"Wait Boiler";       // Включить бойлер
			return (char*)strEngPause;
		} else {
			if(get_modWork() == pOFF)    return (char*)strEngPause;
			if(get_modWork() & pHEAT)    return (char*)"Heating";
			if(get_modWork() & pCOOL)    return (char*)"Cooling";
			if(get_modWork() & pBOILER)  return (char*)"Boiler";
			if(get_modWork() & pDEFROST) return (char*)"Defrost";
			return (char*)"...";
		}
	case pWAIT_HP:    return (char*)"Wait";  break;         // 4 Ожидание
	case pERROR_HP:   return (char*)"Error"; break;       // 5 Ошибка ТН
	}
	return (char*)"Status ?"; 							   // 6 - Эта ошибка возникать не должна!
}

void HeatPump::get_StateModworkStr(char *strReturn)
{
	if(get_State() == pOFF_HP) {
		strcat(strReturn, MODE_HP_STR[0]);
	} else if(get_State() == pWAIT_HP) {
#ifdef USE_UPS
		if(NO_Power) strcat(strReturn,"No Power!");
		else
#endif
			strcat(strReturn, "...");
	} else /*if(get_State() == pWORK_HP)*/ {
		if((get_modWork() & pHEAT)) strcat(strReturn, MODE_HP_STR[1]);
		else if((get_modWork() & pCOOL)) strcat(strReturn, MODE_HP_STR[2]);
		else if((get_modWork() & pBOILER)) strcat(strReturn, MODE_HP_STR[3]);
		else if((get_modWork() & pDEFROST)) strcat(strReturn, MODE_HP_STR[4]);
		else strcat(strReturn, MODE_HP_STR[0]);
		if((get_modWork() & pCONTINUE)) strcat(strReturn, MODE_HP_STR[5]);
		strcat(strReturn, " ["); strcat(strReturn, (char *)codeRet[get_ret()]); strcat(strReturn, "]");
	}
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
	void (Journal::*fn)(const char *format, ...) = f ? &Journal::jprintf : &Journal::printf;
	((journal).*(fn))(" modWork:%X[%s]", (int) get_modWork(), codeRet[Status.ret]);
	for(i = 0; i < RNUMBER; i++) ((journal).*(fn))(" %s:%d", dRelay[i].get_name(), dRelay[i].get_Relay());
	if(dFC.get_present()) ((journal).*(fn))(" freqFC:%.2d", dFC.get_frequency());
	if(dFC.get_present()) ((journal).*(fn))(" Power:%.3d", dFC.get_power());
#ifdef EEV_DEF
#ifdef EEV_PREFER_PERCENT
	((journal).*(fn))(" EEV:%.2d", dEEV.calc_percent(dEEV.get_EEV()));
#else
	((journal).*(fn))(" EEV:%d", dEEV.get_EEV());
#endif
#endif
	for(i = 0; i < INUMBER; i++) ((journal).*(fn))(" %s:%d", sInput[i].get_name(), sInput[i].get_Input());
	((journal).*(fn))(cStrEnd);
	// Доп инфо
	for(i = 0; i < TNUMBER; i++) if(sTemp[i].get_present() && !(SENSORTEMP[i] & 4)) ((journal).*(fn))(" %s:%.2d", sTemp[i].get_name(), sTemp[i].get_Temp());
	for(i = 0; i < ANUMBER; i++) if(sADC[i].get_present()) ((journal).*(fn))(" %s:%.2d", sADC[i].get_name(), sADC[i].get_Value());
	((journal).*(fn))(cStrEnd);
	return OK;
}

// Температура конденсации
#define MAGIC_CONST_CONDENS 200   // Магическая поправка для перевода температуры выхода конденсатора по гликолю  в температуру конденсации   
int16_t HeatPump::get_temp_condensing(void)
{
#ifdef EEV_DEF  
	if(sADC[PCON].get_present()) {
		return PressToTemp(PCON);
	} else {
		return sTemp[is_heating() ? TCONOUTG : TEVAOUTG].get_Temp() + MAGIC_CONST_CONDENS; // +2C
	}
#else
  return sTemp[is_heating() ? TCONOUTG : TEVAOUTG].get_Temp() + MAGIC_CONST_CONDENS; // +2C
#endif
}

// Переохлаждение
int16_t HeatPump::get_overcool(void)
{
	if(!is_heating()) {
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
		return PressToTemp(PEVA);
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
	if(testMode != NORMAL) return 0;
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
			journal.jprintf_time("Error %d PrepareTemp bus %d\n", i, bus+1);
			if(ret == 2) set_Error(i, (char*) __FUNCTION__);
		}
	}
	return ret ? (1<<bus) : 0;
}

// Обновление расчетных величин мощностей и СОР
// Вызывается из задачи чтения датчиков 
void HeatPump::calculatePower()
{
	// Мощности контуров
	if(is_heating()) {
#ifdef  FLOWCON 
 		powerOUT = ((int32_t)FEED - RET) * sFrequency[FLOWCON].get_Value() / 100 * sFrequency[FLOWCON].get_Capacity() / 3600;
#endif
#ifdef  FLOWEVA
		powerGEO = ((int32_t)sTemp[TEVAING].get_Temp() - sTemp[TEVAOUTG].get_Temp()) * sFrequency[FLOWEVA].get_Value() / 100 * sFrequency[FLOWEVA].get_Capacity() / 3600;
#endif
	} else {
#ifdef  FLOWCON
		powerOUT = ((int32_t)RET - FEED) * sFrequency[FLOWCON].get_Value() / 100 * sFrequency[FLOWCON].get_Capacity() / 3600;
#endif
#ifdef  FLOWEVA 
		powerGEO = ((int32_t)sTemp[TEVAOUTG].get_Temp() - sTemp[TEVAING].get_Temp()) * sFrequency[FLOWEVA].get_Value() / 100 * sFrequency[FLOWEVA].get_Capacity() / 3600;
#endif
	}
#ifdef RHEAT_POWER   // Для Дмитрия. его специфика Вычитаем из общей мощности системы отопления мощность электрокотла
#ifdef RHEAT
		if (dRelay[RHEAT].get_Relay()) powerOUT=powerOUT-RHEAT_POWER;  // если включен электрокотел
#endif
#endif

#ifndef COP_ALL_CALC    // если КОП надо считать не всегда То отбрасываем отрицательные мощности, это переходные процессы, возможно это надо делать всегда
	if (powerOUT<0) powerOUT=0;
	if (powerGEO<0) powerGEO=0;
#endif

	// Получение мощностей потребления электроэнергии
	int32_t _power220 = 0;
#ifdef CORRECT_POWER220_EXCL_RBOILER
  #ifdef WR_Load_pins_Boiler_INDEX
   #ifdef WR_Boiler_Substitution_INDEX
	_power220 -= WR_LoadRun[digitalReadDirect(PIN_WR_Boiler_Substitution) ? WR_Boiler_Substitution_INDEX : WR_Load_pins_Boiler_INDEX];
   #else
	_power220 -= WR_LoadRun[WR_Load_pins_Boiler_INDEX];
   #endif
   #ifndef PWM_ACCURATE_POWER
	if(_power220) _power220 = _power220 * dSDM.get_voltage() / 220;
   #endif
  #else
	if(dRelay[RBOILER].get_Relay()) _power220 -= CORRECT_POWER220_EXCL_RBOILER * dSDM.get_voltage() / 220;
  #endif
#else
	#ifdef WATTROUTER
	if(!dRelay[RBOILER].get_Relay()) { // Если греем ваттроутером, то вычесть
		#ifdef WR_Load_pins_Boiler_INDEX
		 #ifdef WR_Boiler_Substitution_INDEX
		_power220 -= WR_LoadRun[digitalReadDirect(PIN_WR_Boiler_Substitution) ? WR_Boiler_Substitution_INDEX : WR_Load_pins_Boiler_INDEX];
		 #else
		_power220 -= WR_LoadRun[WR_Load_pins_Boiler_INDEX];
		 #endif
		#endif
		#ifndef PWM_ACCURATE_POWER
		if(_power220) _power220 = _power220 * dSDM.get_voltage() / 220;
		#endif
	}
	#endif
#endif
#ifdef USE_ELECTROMETER_SDM  // Если есть электросчетчик можно рассчитать полное потребление (с насосами)
	if(dSDM.get_link()) {  // Если счетчик работает (связь не утеряна)
		_power220 += dSDM.get_power();
		if(_power220 < 0) _power220 = 0;
	}
#endif
#ifdef CORRECT_POWER220
	int32_t corr_power220 = 0;
	for(uint8_t i = 0; i < sizeof(correct_power220)/sizeof(correct_power220[0]); i++) if(dRelay[correct_power220[i].num].get_Relay()) corr_power220 += correct_power220[i].value;
	if(corr_power220) {
		corr_power220 = corr_power220 * dSDM.get_voltage() / 220;
		_power220 += corr_power220;
	}
#endif
#ifdef ADD_FC_POWER_WHEN_GENERATOR
	if(GETBIT(Option.flags, fBackupPower)) _power220 += dFC.get_power();  // получить текущую мощность компрессора
#endif
	power220 = _power220;

	// Расчет COP
#ifndef COP_ALL_CALC    	// если COP надо считать не всегда
if(is_compressor_on()){		// Если компрессор работает
#endif	
//	uint16_t fc_pwr = dFC.get_power();  // получить текущую мощность компрессора
//	if(fc_pwr) COP = powerOUT * 100 / fc_pwr; else COP=0; // Компрессорный COP в сотых долях !!!!!!
	if(_power220 != 0) fullCOP = powerOUT * 100 / _power220; else fullCOP = 0; // ПОЛНЫЙ COP в сотых долях !!!!!!
	#ifndef COP_ALL_CALC        // Ограничение переходных процессов для варианта расчета КОП только при работающем компрессоре, что бы графики нормально масштабировались
//		if(COP>10*100) COP=8*100;       // COP не более 8
		if(fullCOP>8*100) fullCOP=7*100; // полный COP не более 7
	#endif
#ifndef COP_ALL_CALC		// если COP надо считать не всегда
	} else {				// компрессор не рабоатет
		fullCOP=0;
//		COP=0;

	}
#endif
}

void HeatPump::Sun_ON(void)
{
#ifdef USE_SUN_COLLECTOR
	if(GetTickCount() - time_Sun > uint32_t(Option.SunMinPause * 1000)) { // ON
		if(flags & (1<<fHP_SunReady)) {
			flags |= (1<<fHP_SunWork);
			dRelay[RSUN].set_Relay(fR_StatusSun);
			dRelay[PUMP_IN].set_Relay(fR_StatusSun);
			time_Sun = GetTickCount();
		} else {
			if(!(flags & (1<<fHP_SunSwitching))) {
				if(sTemp[TSUN].get_Temp() > Option.SunTempOn) {
					flags = (flags & ~(1<<fHP_SunNotInited)) | (1<<fHP_SunSwitching);
					dRelay[RSUNOFF].set_OFF();
					dRelay[RSUNON].set_ON();
					time_Sun = GetTickCount();
				}
			}
		}
	}
#endif
}

void HeatPump::Sun_OFF(void)
{
#ifdef USE_SUN_COLLECTOR
	if(flags & (1<<fHP_SunWork)) {
		dRelay[RSUN].set_Relay(-fR_StatusSun);
		dRelay[PUMP_IN].set_Relay(-fR_StatusSun);
		flags &= ~(1<<fHP_SunWork);
		time_Sun = GetTickCount();
	}
#endif
}

// Получить мощность потребления всего ТН (нужно при ограничении мощности при питании от резерва)
// Предполагается что Электросчетчик стоит на входе ТН (ТЭН не включены) это наиболее точный метод определения мощности
// Если электросчетчика нет, то пытаемся получить из частотного преобразователя.
// если не получается определить мощность то функция возвращает 0
#ifndef NOLINK_SUM_POWER_PUMP
#define NOLINK_SUM_POWER_PUMP 200   // Мощность потребления насосов, для добавления к мощности компрессора, если нет связи со электро-счетчиком
#endif
int16_t HeatPump::getPower(void) {
#ifdef USE_ELECTROMETER_SDM  // Пытаемся получить мощность по электросчетчику
	if(!dSDM.get_link()) dSDM.uplinkSDM();	// Попытаемся реанимировать счетчик (связь по модбасу)
	if(dSDM.get_link()) return dSDM.get_power();
#endif
	// Дошли до сюда - значит не получилось мощность по электросчетчику определить,	работаем с ПЧ
	if(dFC.get_present()) return dFC.get_power() + NOLINK_SUM_POWER_PUMP;
	return 0; // Мощность не определилась
}

void HeatPump::set_HeatBoilerUrgently(boolean onoff)
{
	if(HeatBoilerUrgently != onoff) {
		journal.jprintf("Boiler Urgent = %s\n", (HeatBoilerUrgently = onoff) ? "ON" : "OFF");
	}
}


// Уравнение ПИД регулятора в конечных разностях.
// errorPid - Ошибка ПИД = (Цель - Текущее состояние)  в СОТЫХ
// pid - настройки ПИДа в тысячных
// sum, pre_err - сумма для расчета и предыдущая ошибка
// Выход управляющее воздействие (в СОТЫХ)
int32_t updatePID(int32_t errorPid, PID_STRUCT &pid, PID_WORK_STRUCT &pidw)
{
	int32_t newVal;
#ifdef DEBUG_PID
	journal.printf("PID(%x): err:%d,pre_err:%d,sum:%d (%d,%d,%d). ", &pid, errorPid, pidw.pre_err, pidw.sum, pid.Kp, pid.Ki, pid.Kd);
#endif
#ifdef PID_FORMULA2
	// Алгоритм 2 - стандартный ардуиновский ПИД, выдает значение (не дельту)
	pidw.sum += (int32_t)pid.Ki * errorPid;
	if(pidw.PropOnMeasure) {
		pidw.sum -= (int32_t)pid.Kp * (pidw.pre_err - errorPid);
		newVal = 0;
#ifdef DEBUG_PID
		journal.printf("P:%d,", -pid.Kp * (pidw.pre_err - errorPid));
#endif
	} else {
		newVal = (int32_t)pid.Kp * errorPid;
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
#else  // Алгоритм 1 - оригинальный ПИД, выдает дельту, коэффициенты не корректируются по времени
	// Cp, Ci, Cd – коэффициенты дискретного ПИД регулятора;
	// u(t) = P (t) + I (t) + D (t);
	// P (t) = Kp * e (t);
	// I (t) = I (t — 1) + Ki * e (t);
	// D (t) = Kd * {e (t) — e (t — 1)};
	// T – период дискретизации(период, с которым вызывается ПИД регулятор).
  
	if(pid.Ki != 0)// Расчет интегральной составляющей, если она не равна 0
	{
		pidw.sum += (int32_t) pid.Ki * errorPid;     // Интегральная составляющая, с накоплением, в СТО ТЫСЯЧНЫХ (градусы 100 и интегральный коэффициент 1000)
		if(pidw.sum > pidw.max) pidw.sum = pidw.max; // Ограничение диапазона изменения ПИД интегральной составляющей, произведение в СТО ТЫСЯЧНЫХ
		else if(pidw.sum < -pidw.max) pidw.sum = -pidw.max;
	} else pidw.sum = 0;              // если Кi равен 0 то интегрирование не используем
	newVal = pidw.sum;
//	if (abs(pidw.sum)>=pidw.max) pidw.sum=0; // Сброс интегральной составляющей при достижении максимума
#ifdef DEBUG_PID
	journal.printf("I:%d,", newVal);
#endif
	// Пропорциональная составляющая
	if(abs(errorPid) < pidw.Kp_dmin) newVal += (int32_t) abs(errorPid) * pid.Kp * errorPid / pidw.Kp_dmin; // Вблизи уменьшить воздействие
	else newVal += (int32_t) pid.Kp * errorPid;
#ifdef DEBUG_PID
	journal.printf("P:%d,", newVal-pidw.sum);
#endif
	// Дифференцальная составляющая
	newVal += (int32_t) pid.Kd * (pidw.pre_err - errorPid);// ДЕСЯТИТЫСЯЧНЫЕ Положительная составляющая - ошибка растет (воздействие надо увеличиить)  Отрицательная составляющая - ошибка уменьшается (воздействие надо уменьшить)
	if ((abs(newVal)>=pidw.max)&&(pidw.max>0)) pidw.sum=0; // Сброс интегральной составляющей при движении на один шаг (оптимизация классического ПИДа) 100000 Это один шаг
//    if ((abs(newVal)>=pidw.max-50*1000)&&(pidw.max>0)) pidw.sum=0; // Сброс интегральной составляющей при движении на pidw.max шагов (оптимизация классического ПИДа) Округление (50000 это один шаг)

#ifdef DEBUG_PID
	journal.printf("D:%d PID:%d\n", pid.Kd * (pidw.pre_err - errorPid), newVal);
#endif
#endif
	pidw.pre_err = errorPid; // запомнить предыдущую ошибку
	return round_div_int32(newVal, 1000); // Учесть разрядность коэффициентов (ТЫСЯЧНЫЕ), выход в СОТЫХ
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
