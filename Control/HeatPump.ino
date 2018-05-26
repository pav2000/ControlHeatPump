/*
 * Copyright (c) 2016-2018 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
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

#define PUMPS_ON          Pumps(true,2000)                                                   // Включить насосы
#define PUMPS_OFF         Pumps(false,2000)                                                  // Выключить насосы
// Макросы по работе с компрессором в зависимости от наличия инвертора
#define COMPRESSOR_ON     if(dFC.get_present()) dFC.start_FC();else dRelay[RCOMP].set_ON();   // Включить компрессор в зависимости от наличия инвертора
#define COMPRESSOR_OFF    if(dFC.get_present()) dFC.stop_FC(); else dRelay[RCOMP].set_OFF();  // Выключить компрессор в зависимости от наличия инвертора
#define COMPRESSOR_IS_ON  (dRelay[RCOMP].get_Relay()||dFC.isfOnOff()?true:false)             // Проверка работает ли компрессор

void HeatPump::initHeatPump()
{
  uint8_t i;
  eraseError();

  for(i=0;i<TNUMBER;i++) sTemp[i].initTemp(i);            // Инициализация датчиков температуры

  #ifdef SENSOR_IP
   for(i=0;i<IPNUMBER;i++) sIP[i].initIP();               // Инициализация удаленных датчиков
  #endif
  
  sADC[PEVA].initSensorADC(PEVA,ADC_SENSOR_PEVA);          // Инициализация аналогово датчика PEVA
  sADC[PCON].initSensorADC(PCON,ADC_SENSOR_PCON);          // Инициализация аналогово датчика TCON
 
  for(i=0;i<INUMBER;i++) sInput[i].initInput(i);           // Инициализация контактных датчиков
  for(i=0;i<FNUMBER;i++)  sFrequency[i].initFrequency(i);  // Инициализация частотных датчиков
  for(i=0;i<RNUMBER;i++) dRelay[i].initRelay(i);           // Инициализация реле

  #ifdef I2C_EEPROM_64KB  
     Stat.Init();                                           // Инициализовать статистику
  #endif
  #ifdef EEV_DEF
  dEEV.initEEV();                                           // Инициализация ЭРВ
  #endif

  // Инициалаизация модбаса  перед частотником и счетчиком
  journal.jprintf("Init Modbus RTU via RS485:");  
  if (Modbus.initModbus()==OK) journal.jprintf(" OK\r\n");//  выводим сообщение об установлении связи
  else {journal.jprintf(" not present config\r\n");}         //  нет в конфигурации

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
 strcpy(note_error,"OK");          // Строка c описанием ошибки
 strcpy(source_error,""); 
 error=OK;                         // Код ошибки
}

// Получить число ошибок чтения ВСЕХ датчиков темпеартуры
uint32_t HeatPump::get_errorReadDS18B20()
{
  uint8_t i;
  static uint32_t sum;
  sum=0;
  for(i=0;i<TNUMBER;i++) sum=sum+sTemp[i].get_sumErrorRead();     // Суммирование ошибок по всем датчикам
return sum;    
}


// Установить состояние ТН, при необходимости пишем состояние в ЕЕПРОМ
// true - есть изменения false - нет изменений
boolean HeatPump::setState(TYPE_STATE_HP st)
{
  if (st==Status.State) return false;     // Состояние не меняется
  switch (st)
  {
  case pOFF_HP:       Status.State=pOFF_HP; if(GETBIT(Prof.SaveON.flags,fHP_ON)) {SETBIT0(Prof.SaveON.flags,fHP_ON);Prof.save(Prof.get_idProfile());}  break;// 0 ТН выключен, при необходимости записываем в ЕЕПРОМ
  case pSTARTING_HP:  Status.State=pSTARTING_HP; break;                                                                                    // 1 Стартует
  case pSTOPING_HP:   Status.State=pSTOPING_HP;  break;                                                                                    // 2 Останавливается
  case pWORK_HP:      Status.State=pWORK_HP;if(!(GETBIT(Prof.SaveON.flags,fHP_ON))) {SETBIT1(Prof.SaveON.flags,fHP_ON);Prof.save(Prof.get_idProfile());}  break;// 3 Работает, при необходимости записываем в ЕЕПРОМ
  case pWAIT_HP:      Status.State=pWAIT_HP;if(!(GETBIT(Prof.SaveON.flags,fHP_ON))) {SETBIT1(Prof.SaveON.flags,fHP_ON);Prof.save(Prof.get_idProfile());}  break;// 4 Ожидание, при необходимости записываем в ЕЕПРОМ
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
	if(!OW_prepare_buffers() && SemaphoreTake(xScan1WireSemaphore, 0)) {
		char *_result_str = result_str + m_strlen(result_str);
		OneWireBus.Scan(result_str);
#ifdef ONEWIRE_DS2482_SECOND
		OneWireBus2.Scan(result_str);
#endif
		journal.jprintf("OneWire found(%d): ", OW_scanTableIdx);
		while(m_strlen(_result_str)) {
			journal.jprintf(_result_str);
			uint16_t l = m_strlen(_result_str);
			_result_str += l > PRINTF_BUF-1 ? PRINTF_BUF-1 : l;
		}
		journal.jprintf("\n");
		xSemaphoreGive(xScan1WireSemaphore);
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
// Записать настройки в eeprom i2c на входе адрес с какого, на выходе код ошибки (меньше нуля) или количество записанных  байт
// старотвый адрес I2C_SETTING_EEPROM
// Возвращает ошибку или число записанных байт
int32_t HeatPump::save()
{
	int16_t i;
	int32_t adr, adr_len;
	uint16_t crc_mem;
	adr=I2C_SETTING_EEPROM;
	Option.numProf=Prof.get_idProfile();      // Запомнить текущий профиль, его записываем ЭТО обязательно!!!! нужно для восстановления настроек

	uint8_t tasks_suspended = TaskSuspendAll(); // Запрет других задач

	DateTime.saveTime=rtcSAM3X8.unixtime();   // запомнить время сохранения настроек

	// Заголовок
	headerEEPROM.magic=0xaa;
	headerEEPROM.zero=0x00;
	headerEEPROM.ver=VER_SAVE;
	headerEEPROM.len=0;
	int8_t err = ERR_SAVE_EEPROM;

	while(1) {
		if(writeEEPROM_I2C(adr, (byte*)&headerEEPROM, sizeof(headerEEPROM))) {  break; }  adr=adr+sizeof(headerEEPROM);// записать заголовок

		// Сохранение переменных ТН
		if(writeEEPROM_I2C(adr, (byte*)&DateTime, sizeof(DateTime))) { break; } adr=adr+sizeof(DateTime);  // записать структуры  DateTime
		if(writeEEPROM_I2C(adr, (byte*)& Network, sizeof(Network))) {  break; } adr=adr+sizeof(Network);     // записать структуры Network

		// Сохранить параметры и опции отопления и бойлер, уведомления
		if(writeEEPROM_I2C(adr, (byte*)&Option, sizeof(Option))) { break; }  adr=adr+sizeof(Option);           // записать опции ТН

		if((adr=message.save(adr)) == ERR_SAVE_EEPROM) { break; }

		// Сохранение отдельных объектов ТН
		for(i=0;i<TNUMBER;i++) if((adr = sTemp[i].save(adr)) == ERR_SAVE_EEPROM) { break; }           // Сохранение датчиков температуры
		for(i=0;i<ANUMBER;i++) if((adr = sADC[i].save(adr)) == ERR_SAVE_EEPROM) { break; }            // Сохранение датчика давления
		for(i=0;i<INUMBER;i++) if((adr = sInput[i].save(adr)) == ERR_SAVE_EEPROM) { break; }        // Сохранение контактных датчиков
		for(i=0;i<FNUMBER;i++) if((adr = sFrequency[i].save(adr)) == ERR_SAVE_EEPROM) { break; }    // Сохранение частотных датчиков

		#ifdef SENSOR_IP
		for(i=0;i<IPNUMBER;i++) if((adr = sIP[i].save(adr)) == ERR_SAVE_EEPROM) { break; }         // Сохранение удаленных датчиков
		#endif
		#ifdef EEV_DEF
		if((adr = dEEV.save(adr)) == ERR_SAVE_EEPROM) { break; }                                      // Сохранение ЭВР
		#endif
		if((adr = dFC.save(adr)) == ERR_SAVE_EEPROM) { break; }                                      // Сохранение FC
		#ifdef USE_ELECTROMETER_SDM
		if((adr=dSDM.save(adr)) == ERR_SAVE_EEPROM) { break; }                                    // Сохранение SDM
		#endif
		#ifdef MQTT
		if((adr=clMQTT.save(adr)) == ERR_SAVE_EEPROM) { break; }                                   // Сохранение MQTT
		#endif
		// В конце процедуры записи пишем в структуру заголовка записанную длину в байтах
		adr_len=I2C_SETTING_EEPROM+sizeof(headerEEPROM)-sizeof(headerEEPROM.len);
		headerEEPROM.len=adr-I2C_SETTING_EEPROM+2;  // добавляем два байта для контрольной суммы
		if(writeEEPROM_I2C(adr_len, (byte*)&headerEEPROM.len, sizeof(headerEEPROM.len))) { break; }  // записать длину, без изменения числа записанных байт

		// Расчет контрольной суммы и запись ее в конец
		crc_mem=get_crc16_mem();
		if(writeEEPROM_I2C(adr, (byte*)&crc_mem, sizeof(crc_mem))) { break; }                       // записать crc16, без изменения числа записанных байт

		if((err=check_crc16_eeprom(I2C_SETTING_EEPROM))!=OK) { journal.jprintf(" Verification error, setting not write eeprom/file\n"); break;} // ВЕРИФИКАЦИЯ Контрольные суммы не совпали
		journal.jprintf(" Save setting to eeprom OK, write: %d bytes crc16: 0x%x\n",headerEEPROM.len,crc_mem);                                                      // дошли до конца значит ошибок нет

		// Сохранение текущего профиля
		i=Prof.save(Prof.get_idProfile());
		err = OK;
		break;
	}

	if(tasks_suspended) xTaskResumeAll(); // Разрешение других задач

	if(err) {
		set_Error(err,(char*)nameHeatPump);
		return err;
	}

	// По результатам или ошибка или суммарное число байт
	if(i<OK) return i; else return (int32_t)(headerEEPROM.len+Prof.get_lenProfile());
}

// Считать настройки из eeprom i2c на входе адрес с какого, на выходе код ошибки (меньше нуля)
int8_t HeatPump::load()  
{
	uint16_t i;
	int32_t adr=I2C_SETTING_EEPROM;
	#ifdef LOAD_VERIFICATION
	if((error=check_crc16_eeprom(I2C_SETTING_EEPROM))!=OK) { journal.jprintf(" Error load setting from eeprom, CRC16 is wrong!\n"); return error;} // проверка контрольной суммы
	#endif

	// Прочитать заголовок
	if(readEEPROM_I2C(adr, (byte*)&headerEEPROM, sizeof(headerEEPROM))) { set_Error(ERR_LOAD_EEPROM,(char*)nameHeatPump); return ERR_LOAD_EEPROM;}  adr=adr+sizeof(headerEEPROM);     // заголовок
	if(headerEEPROM.magic!=0xaa)   { set_Error(ERR_HEADER_EEPROM,(char*)nameHeatPump); return ERR_HEADER_EEPROM;}                                          // Заголовок не верен, данных нет
	if(headerEEPROM.zero!=0x00)    { set_Error(ERR_HEADER_EEPROM,(char*)nameHeatPump); return ERR_HEADER_EEPROM;}                                          // Заголовок не верен, данных нет
	#ifdef LOAD_VERIFICATION
	if(headerEEPROM.ver!=VER_SAVE) { set_Error(ERR_HEADER_EEPROM,(char*)nameHeatPump); return ERR_HEADER_EEPROM;}                                          // Версии сохранения не совпали
	if(headerEEPROM.len==0)        { set_Error(ERR_HEADER_EEPROM,(char*)nameHeatPump); return ERR_HEADER_EEPROM;}                                          // Длина данных равна 0
	#endif

	// Прочитать переменные ТН
	if(readEEPROM_I2C(adr, (byte*)& DateTime, sizeof(DateTime))) { set_Error(ERR_LOAD_EEPROM,(char*)nameHeatPump); return ERR_LOAD_EEPROM;}  adr=adr+sizeof(DateTime);  // прочитать структуры  DateTime
	if(readEEPROM_I2C(adr, (byte*)& Network, sizeof(Network))) { set_Error(ERR_LOAD_EEPROM,(char*)nameHeatPump); return ERR_LOAD_EEPROM;}  adr=adr+sizeof(Network);     // прочитать структуры  Network
	// Прочитать параметры и опции отопления
	if(readEEPROM_I2C(adr, (byte*)&Option, sizeof(Option))) { set_Error(ERR_LOAD_EEPROM,(char*)nameHeatPump); return ERR_LOAD_EEPROM;} adr=adr+sizeof(Option);           // прочитать опции ТН

	if((adr=message.load(adr)) == ERR_LOAD_EEPROM) return ERR_LOAD_EEPROM;                                                                                                                                          // Прочитать параметры уведомлений

	// Чтение отдельных объектов ТН
	for(i=0;i<TNUMBER;i++) if((adr=sTemp[i].load(adr)) == ERR_LOAD_EEPROM) return ERR_LOAD_EEPROM;         // Чтение датчиков температуры
	for(i=0;i<ANUMBER;i++) if((adr=sADC[i].load(adr)) == ERR_LOAD_EEPROM) return ERR_LOAD_EEPROM;          // Чтение датчика давления
	for(i=0;i<INUMBER;i++) if((adr= sInput[i].load(adr)) == ERR_LOAD_EEPROM) return ERR_LOAD_EEPROM;       // Чтение контактных датчиков
	for(i=0;i<FNUMBER;i++) if((adr= sFrequency[i].load(adr)) == ERR_LOAD_EEPROM) return ERR_LOAD_EEPROM;   // Сохранение частотных датчиков

	#ifdef SENSOR_IP
	for(i=0;i<IPNUMBER;i++) if((adr= sIP[i].load(adr)) == ERR_LOAD_EEPROM) return ERR_LOAD_EEPROM;       // Чтение удаленных датчиков
	updateLinkIP();                                      // Обновить привязку
	#endif
	#ifdef EEV_DEF
	if((adr=dEEV.load(adr)) == ERR_LOAD_EEPROM) return ERR_LOAD_EEPROM;                                    // Чтение ЭВР
	#endif
	if((adr=dFC.load(adr)) == ERR_LOAD_EEPROM) return ERR_LOAD_EEPROM;                                     // Чтение FC
	#ifdef USE_ELECTROMETER_SDM
	if((adr=dSDM.load(adr)) == ERR_LOAD_EEPROM) return ERR_LOAD_EEPROM;                                  // Чтение SDM
	#endif
	#ifdef MQTT
	if((adr=clMQTT.load(adr)) == ERR_LOAD_EEPROM) return ERR_LOAD_EEPROM;                                  // чтение MQTT
	#endif
	// ВСЕ ОК
	#ifdef LOAD_VERIFICATION
	if (readEEPROM_I2C(adr, (byte*)&i, sizeof(i))) { set_Error(ERR_LOAD_EEPROM,(char*)nameHeatPump); return ERR_LOAD_EEPROM;}  adr=adr+sizeof(i);                    // прочитать crc16
	if (headerEEPROM.len!=adr-I2C_SETTING_EEPROM)  {error=ERR_BAD_LEN_EEPROM;set_Error(ERR_BAD_LEN_EEPROM,(char*)nameHeatPump); return error;}   // Проверка длины
	journal.jprintf(" Load setting from eeprom OK, read: %d bytes crc16: 0x%x\n",adr-I2C_SETTING_EEPROM,i);
	#else
	journal.jprintf(" Load setting from eeprom OK, read: %d bytes VERIFICATION OFF!\n",adr-I2C_SETTING_EEPROM+2);
	#endif

	// Загрузка текущего профиля
	Prof.load(Option.numProf);   // Считали настройки и знаем какой профиль загружать
	return OK;
}
// Считать настройки из буфера на входе адрес с какого, на выходе код ошибки (меньше нуля)
int8_t HeatPump::loadFromBuf(int32_t adr,byte *buf)  
{
  uint16_t i;
  uint32_t aStart=adr;

   // проверка контрольной суммы
   #ifdef LOAD_VERIFICATION 
  if ((error=check_crc16_buf(adr,buf)!=OK)) {journal.jprintf(" Error load setting from file, crc16 is wrong!\n"); return error;}
  #endif
  // Прочитать заголовок
  memcpy((byte*)&headerEEPROM,buf+adr,sizeof(headerEEPROM)); adr=adr+sizeof(headerEEPROM);                                                                // заголовок
  if (headerEEPROM.magic!=0xaa)   { set_Error(ERR_HEADER_EEPROM,(char*)nameHeatPump); return ERR_HEADER_EEPROM;}                                          // Заголовок не верен, данных нет
  if (headerEEPROM.zero!=0x00)    { set_Error(ERR_HEADER_EEPROM,(char*)nameHeatPump); return ERR_HEADER_EEPROM;}                                          // Заголовок не верен, данных нет
  #ifdef LOAD_VERIFICATION
  if (headerEEPROM.ver!=VER_SAVE) { set_Error(ERR_HEADER_EEPROM,(char*)nameHeatPump); return ERR_HEADER_EEPROM;}                                          // Версии сохранения не совпали
  if (headerEEPROM.len==0)        { set_Error(ERR_HEADER_EEPROM,(char*)nameHeatPump); return ERR_HEADER_EEPROM;}                                          // Длина данных равна 0
  #endif
  
  // Прочитать переменные ТН
   memcpy((byte*)&DateTime,buf+adr,sizeof(DateTime)); adr=adr+sizeof(DateTime);                                                                           // прочитать структуры  DateTime
   memcpy((byte*)&Network,buf+adr,sizeof(Network)); adr=adr+sizeof(Network);                                                                              // прочитать структуры  Network
   memcpy((byte*)&Option,buf+adr,sizeof(Option)); adr=adr+sizeof(Option);                                                                                 // прочитать опции ТН
 
   adr=message.loadFromBuf(adr,buf);                                                                                                                      // Прочитать параметры уведомлений
  
 // Чтение отдельных объектов ТН
  for(i=0;i<TNUMBER;i++) adr=sTemp[i].loadFromBuf(adr,buf);         // Чтение датчиков температуры
  for(i=0;i<ANUMBER;i++) adr=sADC[i].loadFromBuf(adr,buf);          // Чтение датчика давления
  for(i=0;i<INUMBER;i++) adr= sInput[i].loadFromBuf(adr,buf);       // Чтение контактных датчиков
  for(i=0;i<FNUMBER;i++) adr= sFrequency[i].loadFromBuf(adr,buf);    // Сохранение частотных датчиков
 
  #ifdef SENSOR_IP
    for(i=0;i<IPNUMBER;i++) adr= sIP[i].loadFromBuf(adr,buf);      // Чтение удаленных датчиков
    updateLinkIP();                                                // Обновить привязку
  #endif
  #ifdef EEV_DEF
  adr=dEEV.loadFromBuf(adr,buf);                                   // Чтение ЭВР
  #endif
  adr=dFC.loadFromBuf(adr,buf);                                    // Чтение FC
  #ifdef USE_ELECTROMETER_SDM  
  adr=dSDM.loadFromBuf(adr,buf);                                    // Чтение SDM
  #endif
  #ifdef MQTT
  adr=clMQTT.loadFromBuf(adr,buf);                                    // Чтение MQTT
  #endif  
 #ifdef LOAD_VERIFICATION
    memcpy((byte*)&i,buf+adr,sizeof(i)); adr=adr+sizeof(i);                                                                                                     // прочитать crc16
    if (headerEEPROM.len!=adr-aStart)  {error=ERR_BAD_LEN_EEPROM; set_Error(ERR_BAD_LEN_EEPROM,(char*)nameHeatPump); return error;}    // Проверка длины
    journal.jprintf(" Load setting from file OK, read: %d bytes crc16: 0x%x\n",adr-aStart,i);                                                                    // ВСЕ ОК
  #else
    journal.jprintf(" Load setting from file OK, read: %d bytes VERIFICATION OFF!\n",adr-aStart+2);
  #endif
  // 
  return OK;       
}
// Функции расчета контрольных сумм ----------------------------------------------
static uint16_t crc= 0xFFFF;  // рабочее значение
// Рассчитать контрольную сумму в ПАМЯТИ по структурам дынных (по структуре!) на выходе crc16
uint16_t HeatPump::get_crc16_mem()
{
  uint16_t i;
  crc= 0xFFFF;
  for(i=0;i<sizeof(headerEEPROM);i++) crc=_crc16(crc,*((byte *)&headerEEPROM+i));  // CRC16 заголовок должен быть заполнен
  for(i=0;i<sizeof(DateTime);i++) crc=_crc16(crc,*((byte*)&DateTime+i));           // CRC16 структуры  DateTime
  for(i=0;i<sizeof(Network);i++) crc=_crc16(crc,*((byte*)&Network+i));             // CRC16 структуры  Network
  for(i=0;i<sizeof(Option);i++) crc=_crc16(crc,*((byte*)&Option+i));               // CRC16 структуры  Option
  crc=message.get_crc16(crc);   
 // Чтение отдельных объектов ТН
  for(i=0;i<TNUMBER;i++) crc=sTemp[i].get_crc16(crc);         // CRC16 датчиков температуры
  for(i=0;i<ANUMBER;i++) crc=sADC[i].get_crc16(crc);          // CRC16 датчика давления
  for(i=0;i<INUMBER;i++) crc=sInput[i].get_crc16(crc);        // CRC16 контактных датчиков
  for(i=0;i<FNUMBER;i++) crc=sFrequency[i].get_crc16(crc);    // Сохранение частотных датчиков
   
  
  #ifdef SENSOR_IP
    for(i=0;i<IPNUMBER;i++) crc= sIP[i].get_crc16(crc);      // CRC16 удаленных датчиков
  #endif
  #ifdef EEV_DEF
  crc=dEEV.get_crc16(crc);                                    // CRC16 ЭВР
  #endif
  crc=dFC.get_crc16(crc);                                     // CRC16 FC 
  
  #ifdef USE_ELECTROMETER_SDM   
    crc=dSDM.get_crc16(crc);                                 // CRC16 SDM  
  #endif
  #ifdef MQTT
    crc=clMQTT.get_crc16(crc);                               // CRC16 MQTT 
  #endif
    
  return crc;          
}

// Проверить контрольную сумму в EEPROM для данных на выходе ошибка, длина определяется из заголовка
int8_t HeatPump::check_crc16_eeprom(int32_t adr)
{
  type_headerEEPROM hEEPROM;
  uint16_t i;
  byte x;
  crc= 0xFFFF;
  if (readEEPROM_I2C(adr, (byte*)&hEEPROM, sizeof(type_headerEEPROM))) { set_Error(ERR_LOAD_EEPROM,(char*)nameHeatPump); return ERR_LOAD_EEPROM;}                                 // Прочитать заголовок - длину данных
  for (i=0;i<hEEPROM.len-2;i++) {if (readEEPROM_I2C(adr+i, (byte*)&x, sizeof(x))) { set_Error(ERR_LOAD_EEPROM,(char*)nameHeatPump); return ERR_LOAD_EEPROM;}  crc=_crc16(crc,x);} // расчет -2 за вычетом CRC16 из длины
  if (readEEPROM_I2C(adr+hEEPROM.len-2, (byte*)&i, sizeof(i))) { set_Error(ERR_LOAD_EEPROM,(char*)nameHeatPump); return ERR_LOAD_EEPROM;}                                         // чтение -2 за вычетом CRC16 из длины
  if (crc==i) return OK; 
  else        return ERR_CRC16_EEPROM;
}
// Проверить контрольную сумму в буфере для данных на выходе ошибка, длина определяется из заголовка
int8_t HeatPump::check_crc16_buf(int32_t adr, byte* buf)   
{
  type_headerEEPROM hEEPROM;
  uint16_t i;
  byte x;
  crc= 0xFFFF;
  memcpy((byte*)&hEEPROM,buf+adr,sizeof(type_headerEEPROM));                                         // Прочитать заголовок - длину данных
  for (i=0;i<hEEPROM.len-2;i++) {memcpy((byte*)&x,buf+adr+i,sizeof(x)); crc=_crc16(crc,x);}          // расчет -2 за вычетом CRC16 из длины
  memcpy((byte*)&i,buf+adr+hEEPROM.len-2,sizeof(i));                                                 // чтение -2 за вычетом CRC16 из длины
  if (crc==i) return OK; 
  else        return ERR_CRC16_EEPROM;
}

// СЧЕТЧИКИ -----------------------------------
 // запись счетчиков теплового насоса в ЕЕПРОМ
int8_t HeatPump::save_motoHour()
{
uint8_t i;
boolean flag;
motoHour.magic=0xaa;   // заголовок

for (i=0;i<5;i++)   // Делаем 5 попыток записи
 {
  if (!(flag=writeEEPROM_I2C(I2C_COUNT_EEPROM, (byte*)&motoHour, sizeof(motoHour)))) break;   // Запись прошла
  journal.jprintf(" ERROR save countes to eeprom #%d\n",i);    
  _delay(i*50);
 }
if (flag) {set_Error(ERR_SAVE2_EEPROM,(char*)nameHeatPump); return ERR_SAVE2_EEPROM;}  // записать счетчики
  journal.jprintf(" Save counters to eeprom, write: %d bytes\n",sizeof(motoHour)); 
return OK;        
}

// чтение счетчиков теплового насоса в ЕЕПРОМ
int8_t HeatPump::load_motoHour()          
{
 byte x=0xff;
 if (readEEPROM_I2C(I2C_COUNT_EEPROM,  (byte*)&x, sizeof(x)))  { set_Error(ERR_LOAD2_EEPROM,(char*)nameHeatPump); return ERR_LOAD2_EEPROM;}                // прочитать заголовок
 if (x!=0xaa)  {journal.jprintf("Bad header counters in eeprom, skip load\n"); return ERR_HEADER2_EEPROM;}                                                  // заголвок плохой выходим
 if (readEEPROM_I2C(I2C_COUNT_EEPROM,  (byte*)&motoHour, sizeof(motoHour)))  { set_Error(ERR_LOAD2_EEPROM,(char*)nameHeatPump); return ERR_LOAD2_EEPROM;}   // прочитать счетчики
 journal.jprintf(" Load counters from eeprom, read: %d bytes\n",sizeof(motoHour)); 
 return OK; 

}
// Сборос сезонного счетчика моточасов
// параметр true - сброс всех счетчиков
void HeatPump::resetCount(boolean full)
{ 
if (full) // Полный сброс счетчиков
  {  
    motoHour.H1=0;
    motoHour.C1=0;
    #ifdef USE_ELECTROMETER_SDM
    motoHour.E1=dSDM.get_Energy();
    #endif
    motoHour.P1=0;
    motoHour.Z1=0;
    motoHour.D1=rtcSAM3X8.unixtime();           // Дата сброса общих счетчиков
  } 
  // Сезон
  motoHour.H2=0;
  motoHour.C2=0;
  #ifdef USE_ELECTROMETER_SDM
  motoHour.E2=dSDM.get_Energy();
  #endif 
  motoHour.P2=0;
  motoHour.Z2=0;
  motoHour.D2=rtcSAM3X8.unixtime();             // дата сброса сезонных счетчиков
  save_motoHour();  // записать счетчики
}
// Обновление счетчиков моточасов
// Электрическая энергия не обновляется, Тепловая энергия обновляется
volatile uint32_t  t1=0,t2=0,t;
volatile uint8_t   countMin=0;  // счетчик минут
void HeatPump::updateCount()
{
float power;  
if (get_State()==pOFF_HP) {t1=0;t2=0; return;}         // ТН не работает, вообще этого не должно быть
 
t=rtcSAM3X8.unixtime(); 
if (t1==0) t1=t; 
if (t2==0) t2=t; // первоначальная инициализация

// Время работы компрессора и выработанная энергия
   if ((COMPRESSOR_IS_ON)&&((t-t2)>=60)) // прошла 1 минута  и компрессор работает
    {
      t2=t;
      motoHour.C1++;    // моточасы компрессора ВСЕГО
      motoHour.C2++;    // моточасы компрессора сбрасываемый счетчик (сезон)
      
     if(ChartPowerCO.get_present()) // Расчет выработанной энергии Если есть соответсвующее оборудование
     {
        power=(float)(FEED-RET)*(float)sFrequency[FLOWCON].get_Value()/sFrequency[FLOWCON].get_kfCapacity(); // Мгновенная мощность в ВАТТАХ
        motoHour.P1=motoHour.P1+(int)(power/60.0);   // потребленная энергия за минуту
        motoHour.P2=motoHour.P2+(int)(power/60.0);   // потребленная энергия за минуту
     }  
      
    }
   if (!(COMPRESSOR_IS_ON)) t2=t;  // Компрессор не работает то сбросить время

// Время работы ТН
    if ((get_State()==pWORK_HP)&&((t-t1)>=60)) // прошла 1 минута и ТН работает
    {
      t1=t;
      motoHour.H1++;          // моточасы ТН ВСЕГО
      motoHour.H2++;          // моточасы ТН сбрасываемый счетчик (сезон)
      countMin++;
      // Пишем именно здесь т.к. Время работы ТН всегда больше компрессора
      if (countMin>=60) {countMin=0; save_motoHour(); } // Записать  счетчики раз в час, экономим ресурс флехи
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
    if (timeBoilerOff>0)   timeBoilerOff=startDefrost+dTime;                  // Время переключения (находу) с ГВС на отопление или охлаждения (нужно для временной блокировки защит) если 0 то переключения не было
    if (startSallmonela>0) startSallmonela=startDefrost+dTime;                // время начала обеззараживания
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
    
  Status.modWork=pOFF;;                         // Что сейчас делает ТН (7 стадий)
  Status.State=pOFF_HP;                         // Сотояние ТН - выключен
  Status.ret=pNone;                             // точка выхода алгоритма
  motoHour.magic=0xaa;                          // волшебное число
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
  startWait=false;                              // Начало работы с ожидания
  onBoiler=false;                               // Если true то идет нагрев бойлера
  onSallmonela=false;                           // Если true то идет Обеззараживание
  command=pEMPTY;                               // Команд на выполнение нет
  PauseStart=true;                              // начать отсчет задержки пред стартом с начала
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
   
  // Структура для хранения заголовка при сохранении настроек EEPROM
  headerEEPROM.magic=0xaa;                      // признак данных, должно быть  0xaa
  headerEEPROM.ver=VER_SAVE;                    // номер версии для сохранения
  headerEEPROM.len=0;                           // длина данных, сколько записано байт в еепром
  
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
  SETBIT1(DateTime.flags,fUpdateNTP);           // Обновление часов по NTP  запрещено
  SETBIT1(DateTime.flags,fUpdateI2C);           // Обновление часов I2C     запрещено
  
  strcpy(DateTime.serverNTP,(char*)NTP_SERVER);  // NTP сервер по умолчанию
  DateTime.timeZone=TIME_ZONE;                   // Часовой пояс

// Опции теплового насоса
  // Временные задержки
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
  Option.nStart=3;                     //  Число попыток пуска компрессора
  Option.tempRHEAT=1000;               //  Значение температуры для управления RHEAT (по умолчанию режим резерв - 10 градусов в доме)
  Option.pausePump=10;                 //  Время паузы  насоса при выключенном компрессоре МИНУТЫ
  Option.workPump=1;                   //  Время работы  насоса при выключенном компрессоре  МИНУТЫ
  Option.tChart=60;                    //  период накопления статистики по умолчанию 60 секунд
  SETBIT0(Option.flags,fAddHeat);      //  Использование дополнительного тена при нагреве НЕ ИСПОЛЬЗОВАТЬ
  SETBIT0(Option.flags,fTypeRHEAT);    //  Использование дополнительного тена по умолчанию режим резерв
  SETBIT1(Option.flags,fBeep);         //  Звук
  SETBIT1(Option.flags,fNextion);      //  дисплей Nextion
  SETBIT0(Option.flags,fEEV_close);    //  Закрытие ЭРВ при выключении компрессора
  SETBIT0(Option.flags,fSD_card);      //  Сброс статистика на карту
  SETBIT0(Option.flags,fSaveON);       //  флаг записи в EEPROM включения ТН
  Option.sleep=5;                      //  Время засыпания минуты
  Option.dim=80;                       //  Якрость %

 // инициализация статистика дополнительно помимо датчиков
  ChartRCOMP.init(!dFC.get_present());                                         // Статистика по включению компрессора только если нет частотника
//  ChartRELAY.init(true);                                                     // Хоть одно реле будет всегда
  #ifdef EEV_DEF
  ChartOVERHEAT.init(true);                                                    // перегрев
  ChartTPEVA.init( sADC[PEVA].get_present());                                  // температура расчитанная из давления  испарения
  ChartTPCON.init( sADC[PCON].get_present());                                  // температура расчитанная из давления  конденсации
  #endif

  for(i=0;i<FNUMBER;i++)   // По всем частотным датчикам
    {
     if (strcmp(sFrequency[i].get_name(),"FLOWCON")==0)                          // если есть датчик потока по конденсатору
       {
       ChartPowerCO.init(sFrequency[i].get_present()&sTemp[TCONING].get_present()&sTemp[TCONOUTG].get_present());               // выходная мощность насоса
       ChartCOP.init(dFC.get_present()&sFrequency[i].get_present()&sTemp[TCONING].get_present()&sTemp[TCONOUTG].get_present()); // Коэффициент преобразования
    //   Serial.print("StatCOP="); Serial.println(dFC.get_present()&sFrequency[i].get_present()&sTemp[TCONING].get_present()&sTemp[TCONOUTG].get_present()) ;
       }
    else  if (strcmp(sFrequency[i].get_name(),"FLOWEVA") ==0)                         // если есть датчик потока по испарителю
       {
       ChartPowerGEO.init(sFrequency[i].get_present()&sTemp[TEVAOUTG].get_present()&sTemp[TEVAING].get_present());     // выходная мощность насоса
       }     
       
    }
   #ifdef USE_ELECTROMETER_SDM  
   ChartFullCOP.init(ChartPowerCO.get_present());                              // ПОЛНЫЙ Коэффициент преобразования
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
                     switch (Network.resSocket)
                       {
                        case 0:   return strcat(ret,(char*)"never:1;10 sec:0;30 sec:0;90 sec:0;");  break;
                        case 10:  return strcat(ret,(char*)"never:0;10 sec:1;30 sec:0;90 sec:0;");  break;
                        case 30:  return strcat(ret,(char*)"never:0;10 sec:0;30 sec:1;90 sec:0;");  break;  // 30 секунд
                        case 90:  return strcat(ret,(char*)"never:0;10 sec:0;30 sec:0;90 sec:1;");  break;
                        default:  Network.resSocket=30; return strcat(ret,(char*)"never:0;10 sec:0;30 sec:1;90 sec:0;"); break; // Этого не должно быть, но если будет то установить по умолчанию
                      }                                                      }else   
 if(strcmp(var,net_RES_W5200)==0){ 
                    switch (Network.resW5200)
                       {
                        case 0:       return strcat(ret,(char*)"never:1;6 hour:0;24 hour:0;");  break;
                        case 60*60*6: return strcat(ret,(char*)"never:0;6 hour:1;24 hour:0;");  break;   // 6 часов
                        case 60*60*24:return strcat(ret,(char*)"never:0;6 hour:0;24 hour:1;");  break;   // 24 часа
                        default:      Network.resW5200=0;return strcat(ret,(char*)"never:1;6 hour:0;24 hour:0;"); break;   // Этого не должно быть, но если будет то установить по умолчанию
                       }                                     }else   
  if(strcmp(var,net_PASS)==0){      if (GETBIT(Network.flags,fPass)) return  strcat(ret,(char*)cOne);
                                    else      return  strcat(ret,(char*)cZero);               }else
  if(strcmp(var,net_PASSUSER)==0){  return strcat(ret,Network.passUser);                      }else                 
  if(strcmp(var,net_PASSADMIN)==0){ return strcat(ret,Network.passAdmin);                     }else   
  if(strcmp(var,net_SIZE_PACKET)==0){return strcat(ret,int2str(Network.sizePacket));          }else   
    /*
                     switch (Network.sizePacket)
                       {
                        case 256:   return (char*)"256 byte:1;512 byte:0;1024 byte:0;2048 byte:0;";  break;
                        case 512:   return (char*)"256 byte:0;512 byte:1;1024 byte:0;2048 byte:0;";  break;
                        case 1024:  return (char*)"256 byte:0;512 byte:0;1024 byte:1;2048 byte:0;";  break;
                        case 2048:  return (char*)"256 byte:0;512 byte:0;1024 byte:0;2048 byte:1;";  break;
                        default:    Network.sizePacket=2048; return (char*)"256 byte:0;512 byte:0;1024 byte:0;2048 byte:1;";  break;     // Этого не должно быть, но если будет то установить по умолчанию
                      }  
     */                 
    if(strcmp(var,net_INIT_W5200)==0){if (GETBIT(Network.flags,fInitW5200)) return  strcat(ret,(char*)cOne);       // флаг Ежеминутный контроль SPI для сетевого чипа
                                      else      return  strcat(ret,(char*)cZero);               }else      
    if(strcmp(var,net_PORT)==0){return strcat(ret,int2str(Network.port));                       }else    // Порт веб сервера
    if(strcmp(var,net_NO_ACK)==0){    if (GETBIT(Network.flags,fNoAck)) return  strcat(ret,(char*)cOne);
                                      else      return  strcat(ret,(char*)cZero);          }else     
    if(strcmp(var,net_DELAY_ACK)==0){return strcat(ret,int2str(Network.delayAck));         }else    
    if(strcmp(var,net_PING_ADR)==0){  return strcat(ret,Network.pingAdr);                  }else
    if(strcmp(var,net_PING_TIME)==0){ 
                     switch (Network.pingTime)
                       {
                        case 0:      return strcat(ret,(char*)"never:1;1 min:0;5 min:0;20 min:0;60 min:0;");  break; // никогда
                        case 1*60:   return strcat(ret,(char*)"never:0;1 min:1;5 min:0;20 min:0;60 min:0;");  break; // 1 минута
                        case 5*60:   return strcat(ret,(char*)"never:0;1 min:0;5 min:1;20 min:0;60 min:0;");  break; // 5 минут
                        case 20*60:  return strcat(ret,(char*)"never:0;1 min:0;5 min:0;20 min:1;60 min:0;");  break; // 20 миут
                        case 60*60:  return strcat(ret,(char*)"never:0;1 min:0;5 min:0;20 min:0;60 min:1;");  break; // 60 минут
                        default:  Network.resSocket=0; return strcat(ret,(char*)"never:1;1 min:0;5 min:0;20 min:0;60 min:0;"); break; // Этого не должно быть, но если будет то установить по умолчанию
                      }                                                      }else   
    if(strcmp(var,net_NO_PING)==0){if (GETBIT(Network.flags,fNoPing)) return  strcat(ret,(char*)cOne);
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
if(strcmp(var,time_DATE)==0){       if (!parseInt16_t(c, '/',buf,3,10)) return false;
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
if(strcmp(var,time_TIMEZONE)==0){return  strcat(ret,int2str(DateTime.timeZone));         }else  
if(strcmp(var,time_UPDATE_I2C)==0){ if (GETBIT(DateTime.flags,fUpdateI2C)) return  strcat(ret,(char*)cOne);
                                    else                                   return  strcat(ret,(char*)cZero); }else      
return strcat(ret,(char*)cInvalid);
}

// Установить опции ТН из числа (float)
boolean HeatPump::set_optionHP(char *var, float x)   
{
   if(strcmp(var,option_ADD_HEAT)==0)         {switch ((int)x)  //использование дополнительного нагревателя (значения 1 и 0)
						                           {
						                            case 0:  SETBIT0(Option.flags,fAddHeat);                                    return true; break;  // использование запрещено
						                            case 1:  SETBIT1(Option.flags,fAddHeat);SETBIT0(Option.flags,fTypeRHEAT);   return true; break;  // резерв
						                            case 2:  SETBIT1(Option.flags,fAddHeat);SETBIT1(Option.flags,fTypeRHEAT);   return true; break;  // бивалент
						                            default: SETBIT1(Option.flags,fAddHeat);SETBIT0(Option.flags,fTypeRHEAT);   return true; break;  // Исправить по умолчанию
						                           } }else  // бивалент
   if(strcmp(var,option_TEMP_RHEAT)==0)       {if ((x>=-30.0)&&(x<=30.0))  {Option.tempRHEAT=x*100.0; return true;} else return false; }else     // температура управления RHEAT (градусы)
   if(strcmp(var,option_PUMP_WORK)==0)        {if ((x>=0.0)&&(x<=60.0)) {Option.workPump=x; return true;} else return false;}else                // работа насоса конденсатора при выключенном компрессоре МИНУТЫ
   if(strcmp(var,option_PUMP_PAUSE)==0)       {if ((x>=0.0)&&(x<=60.0)) {Option.pausePump=x; return true;} else return false;}else               // пауза между работой насоса конденсатора при выключенном компрессоре МИНУТЫ
   if(strcmp(var,option_ATTEMPT)==0)          { if ((x>=0.0)&&(x<10.0)) {Option.nStart=x; return true;} else return false;  }else                // число попыток пуска
   if(strcmp(var,option_TIME_CHART)==0)       { if (get_State()==pWORK_HP) startChart(); // Сбросить статистистику, начать отсчет заново
						                           switch ((int)x)  // период обновления ститистики
						                           {
						                            case 0:  Option.tChart=10;    return true; break;
						                            case 1:  Option.tChart=60;    return true; break;
						                            case 2:  Option.tChart=3*60;  return true; break;
						                            case 3:  Option.tChart=10*60; return true; break;
						                            case 4:  Option.tChart=30*60; return true; break;
						                            case 5:  Option.tChart=60*60; return true; break;       
						                            default: Option.tChart=60;    return true; break;    // Исправить по умолчанию
						                           }   } else
   if(strcmp(var,option_BEEP)==0)             {if (x==0) {SETBIT0(Option.flags,fBeep); return true;} else if (x==1) {SETBIT1(Option.flags,fBeep); return true;} else return false;  }else            // Подача звукового сигнала
   if(strcmp(var,option_NEXTION)==0)          {if (x==0) {SETBIT0(Option.flags,fNextion); updateNextion(); return true;} 
					                           else if (x==1) {SETBIT1(Option.flags,fNextion); updateNextion(); return true;} 
					                           else return false;  } else            // использование дисплея nextion
					   
   if(strcmp(var,option_EEV_CLOSE)==0)        {if (x==0) {SETBIT0(Option.flags,fEEV_close); return true;} else if (x==1) {SETBIT1(Option.flags,fEEV_close); return true;} else return false;   }else            // Закрытие ЭРВ при выключении компрессора
   if(strcmp(var,option_EEV_LIGHT_START)==0)  {if (x==0) {SETBIT0(Option.flags,fEEV_light_start); return true;} else if (x==1) {SETBIT1(Option.flags,fEEV_light_start); return true;} else return false; }else  // Облегчение старта компрессора
   if(strcmp(var,option_START_POS)==0)        {if (x==0) {SETBIT0(Option.flags,fEEV_start); return true;} else if (x==1) {SETBIT1(Option.flags,fEEV_start); return true;} else return false;              }else  // Всегда начинать работу ЭРВ со стратовой позици
     
   if(strcmp(var,option_SD_CARD)==0)          {if (x==0) {SETBIT0(Option.flags,fSD_card); return true;} else if (x==1) {SETBIT1(Option.flags,fSD_card); return true;} else return false;       }else       // Сбрасывать статистику на карту
   if(strcmp(var,option_SAVE_ON)==0)          {if (x==0) {SETBIT0(Option.flags,fSaveON); return true;} else if (x==1) {SETBIT1(Option.flags,fSaveON); return true;} else return false;    }else             // флаг записи в EEPROM включения ТН (восстановление работы после перезагрузки)
   if(strcmp(var,option_NEXT_SLEEP)==0)       {if ((x>=0.0)&&(x<=60.0)) {Option.sleep=x; updateNextion(); return true;} else return false;                                                      }else       // Время засыпания секунды NEXTION минуты
   if(strcmp(var,option_NEXT_DIM)==0)         {if ((x>=5.0)&&(x<=100.0)) {Option.dim=x; updateNextion(); return true;} else return false;                                                       }else       // Якрость % NEXTION
   if(strcmp(var,option_OW2TS)==0)            {Option.flags = (Option.flags & ~(1<<f1Wire2TSngl)) | (x == 0 ? 0 : (1<<f1Wire2TSngl)); return true;}else
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

// Получить опции ТН, результат добавляется в ret
char* HeatPump::get_optionHP(char *var, char *ret)
{
	
 char static temp[16];
   if(strcmp(var,option_ADD_HEAT)==0)         {if(!GETBIT(Option.flags,fAddHeat))          return strcat(ret,(char*)"none:1;reserve:0;bivalent:0;");       // использование ТЭН запрещено
                                               else if(!GETBIT(Option.flags,fTypeRHEAT))   return strcat(ret,(char*)"none:0;reserve:1;bivalent:0;");       // резерв
                                               else                                        return strcat(ret,(char*)"none:0;reserve:0;bivalent:1;");}else  // бивалент
   if(strcmp(var,option_TEMP_RHEAT)==0)       {return strcat(ret,ftoa(temp,(float)Option.tempRHEAT/100.0,1));}else                                         // температура управления RHEAT (градусы)
   if(strcmp(var,option_PUMP_WORK)==0)        {return strcat(ret,int2str(Option.workPump));}else                                                           // работа насоса конденсатора при выключенном компрессоре МИНУТЫ
   if(strcmp(var,option_PUMP_PAUSE)==0)       {return strcat(ret,int2str(Option.pausePump));}else                                                          // пауза между работой насоса конденсатора при выключенном компрессоре МИНУТЫ
   if(strcmp(var,option_ATTEMPT)==0)          {return strcat(ret,int2str(Option.nStart));}else                                                             // число попыток пуска
   if(strcmp(var,option_TIME_CHART)==0)       { switch (Option.tChart)  // период обновления ститистики
						                           {
						                            case 10:    return strcat(ret,(char*)"10 sec:1;1 min:0;3 min:0;10 min:0;30 min:0;60 min:0;"); break;
						                            case 60:    return strcat(ret,(char*)"10 sec:0;1 min:1;3 min:0;10 min:0;30 min:0;60 min:0;"); break;
						                            case 3*60:  return strcat(ret,(char*)"10 sec:0;1 min:0;3 min:1;10 min:0;30 min:0;60 min:0;"); break;
						                            case 10*60: return strcat(ret,(char*)"10 sec:0;1 min:0;3 min:0;10 min:1;30 min:0;60 min:0;"); break;
						                            case 30*60: return strcat(ret,(char*)"10 sec:0;1 min:0;3 min:0;10 min:0;30 min:1;60 min:0;"); break;
						                            case 60*60: return strcat(ret,(char*)"10 sec:0;1 min:0;3 min:0;10 min:0;30 min:0;60 min:1;"); break;       
						                            default:    Option.tChart=60; return strcat(ret,(char*)"10 sec:0;1 min:1;3 min:0;10 min:0;30 min:0;60 min:0;"); break;  // Исправить по умолчанию
						                           } } else
   if(strcmp(var,option_BEEP)==0)             {if(GETBIT(Option.flags,fBeep)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero); }else            // Подача звукового сигнала
   if(strcmp(var,option_NEXTION)==0)          {if(GETBIT(Option.flags,fNextion)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero); } else         // использование дисплея nextion
   
   if(strcmp(var,option_EEV_CLOSE)==0)        {if(GETBIT(Option.flags,fEEV_close)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero); }else            // Закрытие ЭРВ при выключении компрессора
   if(strcmp(var,option_EEV_LIGHT_START)==0)  {if(GETBIT(Option.flags,fEEV_light_start)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero); }else      // Облегчение старта компрессора
   if(strcmp(var,option_START_POS)==0)        {if(GETBIT(Option.flags,fEEV_start)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero); }else           // Всегда начинать работу ЭРВ со стратовой позици
     
   if(strcmp(var,option_SD_CARD)==0)          {if(GETBIT(Option.flags,fSD_card)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero);   }else            // Сбрасывать статистику на карту
   if(strcmp(var,option_SAVE_ON)==0)          {if(GETBIT(Option.flags,fSaveON)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero);    }else           // флаг записи в EEPROM включения ТН (восстановление работы после перезагрузки)
   if(strcmp(var,option_NEXT_SLEEP)==0)       {return strcat(ret,int2str(Option.sleep));                                                     }else            // Время засыпания секунды NEXTION минуты
   if(strcmp(var,option_NEXT_DIM)==0)         {return strcat(ret,int2str(Option.dim));                                                       }else            // Якрость % NEXTION
   if(strcmp(var,option_OW2TS)==0)            {return strcat(ret,(char*)(GETBIT(Option.flags, f1Wire2TSngl) ? cOne : cZero)); }else
   if(strcmp(var,option_DELAY_ON_PUMP)==0)    {return strcat(ret,int2str(Option.delayOnPump));}else     // Задержка включения компрессора после включения насосов (сек).
   if(strcmp(var,option_DELAY_OFF_PUMP)==0)   {return strcat(ret,int2str(Option.delayOffPump));}else      // Задержка выключения насосов после выключения компрессора (сек).
   if(strcmp(var,option_DELAY_START_RES)==0)  {return strcat(ret,int2str(Option.delayStartRes));}else     // Задержка включения ТН после внезапного сброса контроллера (сек.)
   if(strcmp(var,option_DELAY_REPEAD_START)==0){return strcat(ret,int2str(Option.delayRepeadStart));}else  // Задержка перед повторным включениме ТН при ошибке (попытки пуска) секунды
   if(strcmp(var,option_DELAY_DEFROST_ON)==0) {return strcat(ret,int2str(Option.delayDefrostOn));}else    // ДЛЯ ВОЗДУШНОГО ТН Задержка после срабатывания датчика перед включением разморозки (секунды)
   if(strcmp(var,option_DELAY_DEFROST_OFF)==0){return strcat(ret,int2str(Option.delayDefrostOff));}else   // ДЛЯ ВОЗДУШНОГО ТН Задержка перед выключением разморозки (секунды)
   if(strcmp(var,option_DELAY_TRV)==0)        {return strcat(ret,int2str(Option.delayTRV));}else           // Задержка между переключением 4-х ходового клапана и включением компрессора, для выравнивания давлений (сек). Если включены эти опции (переключение тепло-холод)
   if(strcmp(var,option_DELAY_BOILER_SW)==0)  {return strcat(ret,int2str(Option.delayBoilerSW));}else    // Пауза (сек) после переключение ГВС - выравниваем температуру в контуре отопления/ГВС что бы сразу защиты не сработали
   if(strcmp(var,option_DELAY_BOILER_OFF)==0) {return strcat(ret,int2str(Option.delayBoilerOff));} // Время (сек) на сколько блокируются защиты при переходе с ГВС на отопление и охлаждение слишком горяче после ГВС
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
// обновить статистику, добавить одну точку и если надо записать ее на карту
void  HeatPump::updateChart()
{
 uint8_t i; 

 for(i=0;i<TNUMBER;i++) if(sTemp[i].Chart.get_present())  sTemp[i].Chart.addPoint(sTemp[i].get_Temp());
 for(i=0;i<ANUMBER;i++) if(sADC[i].Chart.get_present()) sADC[i].Chart.addPoint(sADC[i].get_Press());
 for(i=0;i<FNUMBER;i++) if(sFrequency[i].Chart.get_present()) sFrequency[i].Chart.addPoint(sFrequency[i].get_Value()); // Частотные датчики
 #ifdef EEV_DEF
 if(dEEV.Chart.get_present())     dEEV.Chart.addPoint(dEEV.get_EEV());  
 if(ChartOVERHEAT.get_present())  ChartOVERHEAT.addPoint(dEEV.get_Overheat());
 if(ChartTPEVA.get_present())     ChartTPEVA.addPoint(PressToTemp(sADC[PEVA].get_Press(),dEEV.get_typeFreon()));
 if(ChartTPCON.get_present())     ChartTPCON.addPoint(PressToTemp(sADC[PCON].get_Press(),dEEV.get_typeFreon()));
 #endif
 
 if(dFC.ChartFC.get_present())       dFC.ChartFC.addPoint(dFC.get_freqFC());       // факт
 if(dFC.ChartPower.get_present())    dFC.ChartPower.addPoint(dFC.get_power());
 if(dFC.ChartCurrent.get_present())  dFC.ChartCurrent.addPoint(dFC.get_current());
  
 if(ChartRCOMP.get_present())     ChartRCOMP.addPoint((int16_t)dRelay[RCOMP].get_Relay());
   
 if(ChartPowerCO.get_present())   ChartPowerCO.addPoint((int16_t)powerCO);  // Мощность контура в вт!!!!!!!!!
 //{
//  powerCO=(float)(FEED-RET)*(float)sFrequency[FLOWCON].get_Value()/sFrequency[FLOWCON].get_kfCapacity();
//  #ifdef RHEAT_POWER   // Для Дмитрия. его специфика Вычитаем из общей мощности системы отопления мощность электрокотла
//    #ifdef RHEAT
//      if (dRelay[RHEAT].get_Relay()]) powerCO=powerCO-RHEAT_POWER;  // если включен электрокотел
//    #endif    
//  #endif
//  ChartPowerCO.addPoint((int16_t)powerCO);
//  }
 
 #ifdef FLOWEVA 
 if(ChartPowerGEO.get_present())  {powerGEO=(float)(sTemp[TEVAING].get_Temp()-sTemp[TEVAOUTG].get_Temp())*(float)sFrequency[FLOWEVA].get_Value()/sFrequency[FLOWEVA].get_kfCapacity(); ChartPowerGEO.addPoint((int16_t)powerGEO);} // Мощность контура в Вт!!!!!!!!!
 #endif
 if(ChartCOP.get_present())       {if (dFC.get_power()>0) ChartCOP.addPoint(COP);  else ChartCOP.addPoint(0);}  // в сотых долях !!!!!!
 #ifdef USE_ELECTROMETER_SDM 
    if(dSDM.ChartVoltage.get_present())   dSDM.ChartVoltage.addPoint(dSDM.get_Voltage()*100);
    if(dSDM.ChartCurrent.get_present())   dSDM.ChartCurrent.addPoint(dSDM.get_Current()*100);
  //  if(dSDM.sAcPower.get_present())   dSDM.sAcPower.addPoint(dSDM.get_AcPower());
  //  if(dSDM.sRePower.get_present())   dSDM.sRePower.addPoint(dSDM.get_RePower());  
    if(dSDM.ChartPower.get_present())   power220=dSDM.get_Power();  dSDM.ChartPower.addPoint(power220); 
  //  if(dSDM.ChartPowerFactor.get_present())   dSDM.ChartPowerFactor.addPoint(dSDM.get_PowerFactor()*100);    
    if(ChartFullCOP.get_present())     { if ((dSDM.get_Power()>0)&&(COMPRESSOR_IS_ON)) ChartFullCOP.addPoint(fullCOP); else  ChartFullCOP.addPoint(0);} // в сотых долях !!!!!!
 #endif


// ДАННЫЕ Запись графика в файл
 if(GETBIT(Option.flags,fSD_card))
   {
	 if(SemaphoreTake(xWebThreadSemaphore,(W5200_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE) {journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexWebThreadBuzy);return;} // Захват мютекса потока или ОЖИДАНИНЕ W5200_TIME_WAIT
     SPI_switchSD();
 //   _delay(10);   // подождать очистку буфера
        if (!statFile.open(FILE_CHART,O_WRITE| O_AT_END)) 
           {
            journal.jprintf("$ERROR - opening %s for write stat data is failed!\n",FILE_CHART);
           }
         else     // Заголовок
           { 
           statFile.print(NowDateToStr());statFile.print(" ");statFile.print(NowTimeToStr());statFile.print(";");  // дата и время
           for(i=0;i<TNUMBER;i++) if(sTemp[i].Chart.get_present()) {statFile.print((float)sTemp[i].get_Temp()/100.0); statFile.print(";");}
           for(i=0;i<ANUMBER;i++) if(sADC[i].Chart.get_present()) {statFile.print((float)sADC[i].get_Press()/100.0);statFile.print(";");} 
           for(i=0;i<FNUMBER;i++) if(sFrequency[i].Chart.get_present()) {statFile.print((float)sFrequency[i].get_Value()/1000.0);statFile.print(";");} // Частотные датчики
           #ifdef EEV_DEF
           if(dEEV.Chart.get_present())       { statFile.print(dEEV.get_EEV()); statFile.print(";");}
           if(ChartOVERHEAT.get_present())    { statFile.print((float)dEEV.get_Overheat()/100.0); statFile.print(";");}
           if(ChartTPCON.get_present())       { statFile.print((float)(PressToTemp(sADC[PCON].get_Press(),dEEV.get_typeFreon()))/100.0); statFile.print(";");}
           if(ChartTPEVA.get_present())       { statFile.print((float)(PressToTemp(sADC[PEVA].get_Press(),dEEV.get_typeFreon()))/100.0); statFile.print(";");}
           #endif
         
           if(dFC.ChartFC.get_present())      { statFile.print((float)dFC.get_freqFC()/100.0);   statFile.print(";");} 
           if(dFC.ChartPower.get_present())   { statFile.print((float)dFC.get_power()/10.0);    statFile.print(";");}
           if(dFC.ChartCurrent.get_present()) { statFile.print((float)dFC.get_current()/100.0); statFile.print(";");}
           
           if(ChartRCOMP.get_present())       { statFile.print((int16_t)dRelay[RCOMP].get_Relay()); statFile.print(";");}
           if ((sTemp[TCONOUTG].Chart.get_present())&&(sTemp[TCONING].Chart.get_present()))  { statFile.print((float)(FEED-RET)/100.0); statFile.print(";");}
           if ((sTemp[TEVAING].Chart.get_present())&&(sTemp[TEVAOUTG].Chart.get_present()))  { statFile.print((float)(sTemp[TEVAING].get_Temp()-sTemp[TEVAOUTG].get_Temp())/100.0); statFile.print(";");}

                
           if(ChartPowerCO.get_present())  { statFile.print((int16_t)(powerCO)); statFile.print(";"); } // Мощность контура в ваттах!!!!!!!!!
           if(ChartPowerGEO.get_present()) { statFile.print((int16_t)(powerGEO)); statFile.print(";");} // Мощность контура в ваттах!!!!!!!!!
           if(ChartCOP.get_present())      { statFile.print((float)(powerCO/dFC.get_power())/100.0); statFile.print(";"); }    // в еденицах
           #ifdef USE_ELECTROMETER_SDM 
           if(dSDM.ChartVoltage.get_present())     { statFile.print((float)dSDM.get_Voltage());    statFile.print(";");} 
           if(dSDM.ChartCurrent.get_present())     { statFile.print((float)dSDM.get_Current());    statFile.print(";");} 
 //          if(dSDM.sAcPower.get_present())     { statFile.print((float)dSDM.get_AcPower());    statFile.print(";");} 
 //          if(dSDM.sRePower.get_present())     { statFile.print((float)dSDM.get_RePower());    statFile.print(";");}   
           if(dSDM.ChartPower.get_present())       { statFile.print((float)dSDM.get_Power());      statFile.print(";");}  
 //          if(dSDM.ChartPowerFactor.get_present()) { statFile.print((float)dSDM.get_PowerFactor());statFile.print(";");}  
           if(ChartFullCOP.get_present())       { if ((dSDM.get_Power()>0)&&(COMPRESSOR_IS_ON)){ statFile.print((float)(powerCO/dSDM.get_Power())/100.0);statFile.print(";");}  else statFile.print("0.0;");}
           #endif
           statFile.println("");
           statFile.flush();
           statFile.close();
           }   
      SPI_switchW5200();        
      SemaphoreGive(xWebThreadSemaphore);                                      // Отдать мютекс
  }  
          
}

// сбросить статистику и запустить новую запись
void HeatPump::startChart()
{
 uint8_t i; 
 for(i=0;i<TNUMBER;i++) sTemp[i].Chart.clear();
 for(i=0;i<ANUMBER;i++) sADC[i].Chart.clear();
 for(i=0;i<FNUMBER;i++) sFrequency[i].Chart.clear();
 #ifdef EEV_DEF
 dEEV.Chart.clear();
 ChartOVERHEAT.clear();
 ChartTPEVA.clear(); 
 ChartTPCON.clear(); 
 #endif
 dFC.ChartFC.clear();
 dFC.ChartPower.clear();
 dFC.ChartCurrent.clear();
 ChartRCOMP.clear();
// ChartRELAY.clear();
 ChartPowerCO.clear();                                 // выходная мощность насоса
 ChartPowerGEO.clear();                                // Мощность геоконтура
 ChartCOP.clear();                                     // Коэффициент преобразования
 #ifdef USE_ELECTROMETER_SDM 
 dSDM.ChartVoltage.clear();                              // Статистика по напряжению
 dSDM.ChartCurrent.clear();                              // Статистика по току
// dSDM.sAcPower.clear();                              // Статистика по активная мощность
// dSDM.sRePower.clear();                              // Статистика по Реактивная мощность
 dSDM.ChartPower.clear();                                // Статистика по Полная мощность
// dSDM.ChartPowerFactor.clear();                          // Статистика по Коэффициент мощности
 ChartFullCOP.clear();                                     // Коэффициент преобразования
 #endif
 powerCO=0;
 powerGEO=0;
 power220=0;                                   
 vTaskResume(xHandleUpdateStat); // Запустить задачу обновления статистики

 if(GETBIT(Option.flags,fSD_card))  // ЗАГОЛОВОК Запись статистики в файл
   {
	 if(SemaphoreTake(xWebThreadSemaphore,(W5200_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE) {journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexWebThreadBuzy);return;} // Захват мютекса потока или ОЖИДАНИНЕ W5200_TIME_WAIT
        SPI_switchSD();
        if (!statFile.open(FILE_CHART,O_WRITE |O_CREAT | O_TRUNC)) 
           {
               journal.jprintf("$ERROR - opening %s for write stat header is failed!\n",FILE_CHART);
           }
         else     // Заголовок
           { 
           statFile.print("time;");
           for(i=0;i<TNUMBER;i++) if(sTemp[i].Chart.get_present()) {statFile.print(sTemp[i].get_name()); statFile.print(";");}
           for(i=0;i<ANUMBER;i++) if(sADC[i].Chart.get_present()) {statFile.print(sADC[i].get_name());statFile.print(";");} 
           for(i=0;i<FNUMBER;i++) if(sFrequency[i].Chart.get_present()) {statFile.print(sFrequency[i].get_name());statFile.print(";");} 
           
           #ifdef EEV_DEF
           if(dEEV.Chart.get_present())        statFile.print("posEEV;");
           if(ChartOVERHEAT.get_present())     statFile.print("OVERHEAT;");
           if(ChartTPEVA.get_present())        statFile.print("T[PEVA];");
           if(ChartTPCON.get_present())        statFile.print("T[PCON];");
           #endif
           
           if(dFC.ChartFC.get_present())       statFile.print("freqFC;");
           if(dFC.ChartPower.get_present())    statFile.print("powerFC;");
           if(dFC.ChartCurrent.get_present())  statFile.print("currentFC;");
           
           if(ChartRCOMP.get_present())        statFile.print("RCOMP;");
           
           if ((sTemp[TCONOUTG].Chart.get_present())&&(sTemp[TCONING].Chart.get_present())) statFile.print("dCO;");
           if ((sTemp[TEVAING].Chart.get_present())&&(sTemp[TEVAOUTG].Chart.get_present())) statFile.print("dGEO;");
           
           if(ChartPowerCO.get_present())      statFile.print("PowerCO;");
           if(ChartPowerGEO.get_present())     statFile.print("PowerGEO;");
           if(ChartCOP.get_present())          statFile.print("COP;");

           #ifdef USE_ELECTROMETER_SDM 
           if(dSDM.ChartVoltage.get_present())    statFile.print("VOLTAGE;");
           if(dSDM.ChartCurrent.get_present())    statFile.print("CURRENT;");
   //      if(dSDM.sAcPower.get_present())    statFile.print("acPOWER;");
  //       if(dSDM.sRePower.get_present())    statFile.print("rePOWER;");
           if(dSDM.ChartPower.get_present())      statFile.print("fullPOWER;");
  //         if(dSDM.ChartPowerFactor.get_present())statFile.print("kPOWER;");
           if(ChartFullCOP.get_present())      statFile.print("fullCOP;");
           #endif
           
           statFile.println("");
           statFile.flush();
           statFile.close();
           journal.jprintf(" Write header %s  on SD card Ok\n",FILE_CHART);
           }  
  //    _delay(10);   // подождать очистку буфера
      SPI_switchW5200();  
      SemaphoreGive(xWebThreadSemaphore);                                      // Отдать мютекс
  }
     
}


// получить список доступных графиков в виде строки
// cat true - список добавляется в конец, false - строка обнуляется и список добавляется
char * HeatPump::get_listChart(char* str, boolean cat)
{
uint8_t i;  
if (!cat) strcpy(str,"");  //Обнулить строку если есть соответсвующий флаг
 strcat(str,"none:1;");
 for(i=0;i<TNUMBER;i++) if(sTemp[i].Chart.get_present()) {strcat(str,sTemp[i].get_name()); strcat(str,":0;");}
 for(i=0;i<ANUMBER;i++) if(sADC[i].Chart.get_present()) { strcat(str,sADC[i].get_name()); strcat(str,":0;");} 
 for(i=0;i<FNUMBER;i++) if(sFrequency[i].Chart.get_present()) { strcat(str,sFrequency[i].get_name()); strcat(str,":0;");} 
 #ifdef EEV_DEF
 if(dEEV.Chart.get_present())       strcat(str,"posEEV:0;");
 if(ChartOVERHEAT.get_present())    strcat(str,"OVERHEAT:0;");
 if(ChartTPEVA.get_present())       strcat(str,"T[PEVA]:0;");
 if(ChartTPCON.get_present())       strcat(str,"T[PCON]:0;");
 #endif
 if(dFC.ChartFC.get_present())      strcat(str,"freqFC:0;");
 if(dFC.ChartPower.get_present())   strcat(str,"powerFC:0;");
 if(dFC.ChartCurrent.get_present()) strcat(str,"currentFC:0;");
 if(ChartRCOMP.get_present())       strcat(str,"RCOMP:0;");
 if ((sTemp[TCONOUTG].Chart.get_present())&&(sTemp[TCONING].Chart.get_present()))  strcat(str,"dCO:0;");
 if ((sTemp[TEVAING].Chart.get_present())&&(sTemp[TEVAOUTG].Chart.get_present()))  strcat(str,"dGEO:0;");
 if(ChartPowerCO.get_present())     strcat(str,"PowerCO:0;");
 if(ChartPowerGEO.get_present())    strcat(str,"PowerGEO:0;");
 if(ChartCOP.get_present())         strcat(str,"COP:0;"); 
  #ifdef USE_ELECTROMETER_SDM 
 if(dSDM.ChartVoltage.get_present())     strcat(str,"VOLTAGE:0;");
 if(dSDM.ChartCurrent.get_present())     strcat(str,"CURRENT:0;");
// if(dSDM.sAcPower.get_present())     strcat(str,"acPOWER:0;");
// if(dSDM.sRePower.get_present())     strcat(str,"rePOWER:0;");
 if(dSDM.ChartPower.get_present())       strcat(str,"fullPOWER:0;");
// if(dSDM.ChartPowerFactor.get_present()) strcat(str,"kPOWER:0;");
 if(ChartFullCOP.get_present())       strcat(str,"fullCOP:0;"); 
 #endif
// for(i=0;i<RNUMBER;i++) if(dRelay[i].Chart.get_present()) { strcat(str,dRelay[i].get_name()); strcat(str,":0;");}  
return str;               
}

// получить данные графика  в виде строки
// cat=true - не обнулять входную строку а добавить в конец
char * HeatPump::get_Chart(char *var, char* str, boolean cat)
{
	char buf[10];
	if(!cat) strcpy(str, "");  //Обнулить строку если есть соответсвующий флаг false
	if(strcmp(var, chart_NONE) == 0) {
		strcat(str, "");
		return str;
	} else if(strcmp(var, chart_TOUT) == 0) {
		return sTemp[TOUT].Chart.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TIN) == 0) {
		return sTemp[TIN].Chart.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TEVAIN) == 0) {
		return sTemp[TEVAIN].Chart.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TEVAOUT) == 0) {
		return sTemp[TEVAOUT].Chart.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TCONIN) == 0) {
		return sTemp[TCONIN].Chart.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TCONOUT) == 0) {
		return sTemp[TCONOUT].Chart.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TBOILER) == 0) {
		return sTemp[TBOILER].Chart.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TACCUM) == 0) {
		return sTemp[TACCUM].Chart.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TRTOOUT) == 0) {
		return sTemp[TRTOOUT].Chart.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TCOMP) == 0) {
		return sTemp[TCOMP].Chart.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TEVAING) == 0) {
		return sTemp[TEVAING].Chart.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TEVAOUTG) == 0) {
		return sTemp[TEVAOUTG].Chart.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TCONING) == 0) {
		return sTemp[TCONING].Chart.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TCONOUTG) == 0) {
		return sTemp[TCONOUTG].Chart.get_PointsStr(100, str);
	} else if(strcmp(var, chart_PEVA) == 0) {
		return sADC[PEVA].Chart.get_PointsStr(100, str);
	} else if(strcmp(var, chart_PCON) == 0) {
		return sADC[PCON].Chart.get_PointsStr(100, str);
	} else if(strcmp(var, chart_FLOWCON) == 0) {
#ifdef FLOWCON
		return sFrequency[FLOWCON].Chart.get_PointsStr(1000, str);
#endif
	} else if(strcmp(var, chart_FLOWEVA) == 0) {
#ifdef FLOWEVA
		return sFrequency[FLOWEVA].Chart.get_PointsStr(1000, str);
#endif
	} else if(strcmp(var, chart_FLOWPCON) == 0) {
#ifdef FLOWPCON
		return sFrequency[FLOWPCON].Chart.get_PointsStr(1000,str);
#endif
	} else
#ifdef EEV_DEF
	if(strcmp(var, chart_posEEV) == 0) {
		return dEEV.Chart.get_PointsStr(1, str);
	} else if(strcmp(var, chart_OVERHEAT) == 0) {
		return ChartOVERHEAT.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TPEVA) == 0) {
		return ChartTPEVA.get_PointsStr(100, str);
	} else if(strcmp(var, chart_TPCON) == 0) {
		return ChartTPCON.get_PointsStr(100, str);
	} else
#endif
	if(strcmp(var, chart_freqFC) == 0) {
		return dFC.ChartFC.get_PointsStr(100, str);
	} else if(strcmp(var, chart_powerFC) == 0) {
		return dFC.ChartPower.get_PointsStr(10, str);
	} else if(strcmp(var, chart_currentFC) == 0) {
		return dFC.ChartCurrent.get_PointsStr(100, str);
	} else

	if(strcmp(var, chart_RCOMP) == 0) {
		return ChartRCOMP.get_PointsStr(1, str);
	} else if(strcmp(var, chart_dCO) == 0) {
		if((sTemp[TCONOUTG].Chart.get_present()) && (sTemp[TCONING].Chart.get_present())) // считаем график на лету экономим оперативку
				{
			for(int i = 0; i < sTemp[TCONOUTG].Chart.get_num(); i++) {
				strcat(str, ftoa(buf,
								((float) sTemp[TCONOUTG].Chart.get_Point(i) - (float) sTemp[TCONING].Chart.get_Point(i)) / 100, 2));
				strcat(str, (char*) ";");
			}
		} else return (char*) ";"; // График не определен - нет данных
	} else if(strcmp(var, chart_dGEO) == 0) {
		if((sTemp[TEVAING].Chart.get_present()) && (sTemp[TEVAOUTG].Chart.get_present())) // считаем график на лету экономим оперативку
				{
			for(int i = 0; i < sTemp[TEVAING].Chart.get_num(); i++) {
				strcat(str,	ftoa(buf, ((float) sTemp[TEVAING].Chart.get_Point(i) - (float) sTemp[TEVAOUTG].Chart.get_Point(i)) / 100, 2));
				strcat(str, (char*) ";");
			}
		} else return (char*) ";"; // График не определен - нет данных
	} else

	if(strcmp(var, chart_PowerCO) == 0) {
		return ChartPowerCO.get_PointsStr(1000, str);
	} else if(strcmp(var, chart_PowerGEO) == 0) {
		return ChartPowerGEO.get_PointsStr(1000, str);
	} else if(strcmp(var, chart_COP) == 0) {
		return ChartCOP.get_PointsStr(100, str);
	} else

#ifdef USE_ELECTROMETER_SDM
	if(strcmp(var, chart_VOLTAGE) == 0) {
		return dSDM.ChartVoltage.get_PointsStr(100, str);
	} else if(strcmp(var, chart_CURRENT) == 0) {
		return dSDM.ChartCurrent.get_PointsStr(100, str);
	} else
	//   if(strcmp(var,chart_acPOWER)==0){   return dSDM.sAcPower.get_PointsStr(1,str);           }else
	//   if(strcmp(var,chart_rePOWER)==0){   return dSDM.sRePower.get_PointsStr(1,str);           }else
	if(strcmp(var, chart_fullPOWER) == 0) {
		return dSDM.ChartPower.get_PointsStr(1, str);
	} else
	//   if(strcmp(var,chart_kPOWER)==0){    return dSDM.ChartPowerFactor.get_PointsStr(100,str);     }else
	if(strcmp(var, chart_fullCOP) == 0) {
		return ChartFullCOP.get_PointsStr(100, str);
	} else
#endif
	{}
	return str;
}

// расчитать хеш для пользователя возвращает длину хеша
uint8_t HeatPump::set_hashUser()
{
char buf[20];
strcpy(buf,NAME_USER);
strcat(buf,":");
strcat(buf,Network.passUser);
base64_encode(Security.hashUser, buf, strlen(buf)); 
Security.hashUserLen=strlen(Security.hashUser);
journal.jprintf("Hash user: %s\n",Security.hashUser);
return Security.hashUserLen;
}
// расчитать хеш для администратора возвращает длину хеша
uint8_t HeatPump::set_hashAdmin()
{
char buf[20];
strcpy(buf,NAME_ADMIN);
strcat(buf,":");
strcat(buf,Network.passAdmin);
base64_encode(Security.hashAdmin,buf,strlen(buf)); 
Security.hashAdminLen=strlen(Security.hashAdmin);
journal.jprintf("Hash admin: %s\n",Security.hashAdmin);
return Security.hashAdminLen;  
}


// Обновить настройки дисплея Nextion
void HeatPump::updateNextion() 
{
  #ifdef NEXTION   
  char temp[16];
  if (GETBIT(Option.flags,fNextion))  // Дисплей подключен
      {
          if(Option.sleep>0)   // установлено засыпание дисплея
              {
              strcpy(temp,"thsp=");
              strcat(temp,int2str(Option.sleep*60)); // секунды
              myNextion.sendCommand(temp);
              myNextion.sendCommand("thup=1");     // sleep режим активировать
              }  
           else
           {
               myNextion.sendCommand("thsp=0")  ;   // sleep режим выключен  - ЭТО  РАБОТАЕТ
               myNextion.sendCommand("thup=0");      // sleep режим активировать
  /* 
               myNextion.sendCommand("rest");         // Запретить режим сна получается только через сброс экрана
               _delay(50);
               myNextion.sendCommand("page 0");   
               myNextion.sendCommand("bkcmd=0");     // Ответов нет от дисплея
               myNextion.sendCommand("sendxy=0");
               myNextion.sendCommand("thup=1");      // sleep режим активировать
  */             
           }
           
          strcpy(temp,"dim=");
          strcat(temp,int2str(Option.dim));
          myNextion.sendCommand(temp);
          myNextion.set_fPageID();
          vTaskResume(xHandleUpdateNextion);   // включить задачу обновления дисплея
       }
  else                        // Дисплей выключен
      {
          vTaskSuspend(xHandleUpdateNextion);   // выключить задачу обновления дисплея
          myNextion.sendCommand("thsp=0")  ;    // sleep режим выключен
          myNextion.sendCommand("dim=0");
          myNextion.sendCommand("sleep=1");  
      }
 #endif     
}

int16_t HeatPump::get_targetTempCool()
{
  if (get_ruleCool()==pHYBRID) return Prof.Cool.Temp1;  
  if(!(GETBIT(Prof.Cool.flags,fTarget))) return Prof.Cool.Temp1; 
  else  return Prof.Cool.Temp2;
}

int16_t  HeatPump::get_targetTempHeat()
{
  if (get_ruleHeat()==pHYBRID) return Prof.Heat.Temp1;  
  if(!(GETBIT(Prof.Heat.flags,fTarget))) return Prof.Heat.Temp1;
  else  return Prof.Heat.Temp2;
}

// Изменить целевую температуру с провекой допустимости значений
// Параметр само ИЗМЕНЕНИЕ температуры
int16_t HeatPump::setTargetTemp(int16_t dt)
 {
  switch ((MODE_HP)get_mode())   // проверка отопления
    {
      case  pOFF:   return 0;                       break;
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
 // Переключение на следующий режим работы отопления (последовательный перебор режимов)
 void HeatPump::set_nextMode()
 {
   switch ((MODE_HP)get_mode())  
    {
      case  pOFF:   Prof.SaveON.mode=pHEAT;  break;
      case  pHEAT:  Prof.SaveON.mode=pCOOL;  break; 
      case  pCOOL:  Prof.SaveON.mode=pOFF;   break; 
      default: break;
    }  
 }
 // ИЗМЕНИТЬ целевую температуру бойлера с провекой допустимости значений
 int16_t HeatPump::setTempTargetBoiler(int16_t dt)  
 {
  if ((Prof.Boiler.TempTarget+dt>=5.0*100)&&(Prof.Boiler.TempTarget+dt<=60.0*100))   Prof.Boiler.TempTarget=Prof.Boiler.TempTarget+dt; 
  return Prof.Boiler.TempTarget;     
 }
                                 
// --------------------------------------------------------------------------------------------------------     
// ---------------------------------- ОСНОВНЫЕ ФУНКЦИИ РАБОТЫ ТН ------------------------------------------
// --------------------------------------------------------------------------------------------------------    
  
 #ifdef RBOILER  // управление дополнительным ТЭНом бойлера
 // Проверка на необходимость греть бойлер дополнительным теном (true - надо греть) ВСЕ РЕЖИМЫ
 boolean HeatPump::boilerAddHeat()
 {
	 if ((GETBIT(Prof.SaveON.flags,fBoilerON))&&(GETBIT(Prof.Boiler.flags,fSalmonella))) // Сальмонелла не взирая на расписание если включен бойлер
	 {
		 if((rtcSAM3X8.get_day_of_week()==SALLMONELA_DAY)&&(rtcSAM3X8.get_hours()==SALLMONELA_HOUR)&&(rtcSAM3X8.get_minutes()<=1)&&(!onSallmonela)) {startSallmonela=rtcSAM3X8.unixtime(); onSallmonela=true; } // Надо начитать процесс обеззараживания
		 if (onSallmonela)    // Обеззараживание нужно
			 if (startSallmonela+SALLMONELA_END<rtcSAM3X8.unixtime()) // Время цикла еще не исчерпано
			 {
				 if (sTemp[TBOILER].get_Temp()<SALLMONELA_TEMP) return true; // Включить обеззараживание
				 else if (sTemp[TBOILER].get_Temp()>SALLMONELA_TEMP+50) return false; // Стабилизация температуры обеззараживания
			 }
	 }

	 onSallmonela=false; // Обеззараживание не включено смотрим дальше
	 if (((scheduleBoiler())&&(GETBIT(Prof.SaveON.flags,fBoilerON)))) // Если разрешено греть бойлер согласно расписания И Бойлер включен
	 {
		 if (GETBIT(Prof.Boiler.flags,fTurboBoiler))  // Если турбо режим то повторяем за Тепловым насосом (грет или не греть)
		 {
			 return onBoiler;                          // работа параллельно с ТН (если он греет ГВС)
		 }
		 else // Нет турбо
		 {
			 if  (GETBIT(Prof.Boiler.flags,fAddHeating))  // Включен догрев
			 {
				 if ((sTemp[TBOILER].get_Temp()<Prof.Boiler.TempTarget-Prof.Boiler.dTemp)&&(!flagRBOILER)) {flagRBOILER=true; return false;} // Бойлер ниже гистерезиса - ставим признак необходимости включения Догрева (но пока не включаем ТЭН)
				 if ((!flagRBOILER)||(onBoiler))  return false; // флажка нет или работет бойлер но догрев не включаем
				 else  //flagRBOILER==true
				 {
					 if (sTemp[TBOILER].get_Temp()<Prof.Boiler.TempTarget)                       // Бойлер ниже целевой темпеартуры надо греть
					 {
						 if (sTemp[TBOILER].get_Temp()>Prof.Boiler.tempRBOILER) return true;      // Включения тена если температура бойлера больше температуры догрева и темпеартура бойлера меньше целевой темпеартуры
						 if (sTemp[TBOILER].get_Temp()<Prof.Boiler.tempRBOILER-HYSTERESIS_RBOILER) {flagRBOILER=false; return false;}   // температура ниже включения догрева выключаем и сбрасывам флаг необходимости
						 else {return true;} // продолжаем греть бойлер
					 }
					 else  {flagRBOILER=false; return false;}                                    // бойлер выше целевой темпеартуы - цель достигнута - догрев выключаем
				 }
			 }  // догрев
			 else  {flagRBOILER=false; return false;}                    // ТЭН не используется (сняты все флажки)
		 } // Нет турбо
	 }
	 else  {flagRBOILER=false; return false;}                            // Бойлер сейчас запрещен

 }
#endif   

// Проверить расписание бойлера true - нужно греть false - греть не надо, если расписание выключено то возвращает true
boolean HeatPump::scheduleBoiler()
{
boolean b;  
if(GETBIT(Prof.Boiler.flags,fSchedule))         // Если используется расписание
 {  // Понедельник 0 воскресенье 6 это кодирование в расписании функция get_day_of_week возвращает 1-7
  b=Prof.Boiler.Schedule[rtcSAM3X8.get_day_of_week()-1]&(0x01<<rtcSAM3X8.get_hours())?true:false; 
  if(!b) return false;             // запрещено греть бойлер согласно расписания
 }
return true; 
}
// Все реле выключить
void HeatPump::relayAllOFF()
{
  uint8_t i;
  for(i=0;i<RNUMBER;i++)  dRelay[i].set_OFF();         // Выключить все реле;
  onBoiler=false;                                     // выключить признак нагрева бойлера
  journal.jprintf(" All relay off\n");
}                               
// Поставить 4х ходовой в нужное положение для работы в заваисимости от Prof.SaveON.mode
// функция сама определяет что делать в зависимости от режима
// параметр задержка после включения мсек.
#ifdef RTRV    // Если четырехходовой есть в конфигурации
    void HeatPump::set_RTRV(uint16_t d)
    {
       if (Prof.SaveON.mode==pHEAT)        // Реле переключения четырех ходового крана (переделано для инвертора). Для ОТОПЛЕНИЯ надо выключить,  на ОХЛАЖДЕНИЯ включить конечное устройство
        { 
          dRelay[RTRV].set_OFF();         // отопление
        }
        else   // во всех остальных случаях
        {
          dRelay[RTRV].set_ON();          // охлаждение
        }  
       _delay(d);                            // Задержка на 2 сек
    }
#endif

// Переключение на нагрев бойлера ТН true-бойлер false-отопление/охлаждение
// в зависимости от режима, не забываем менять onBoiler по нему определяется включение ГВС
boolean HeatPump::switchBoiler(boolean b)
{
	if (b==onBoiler) return onBoiler;            // Нечего делать выходим
	onBoiler=b;                                  // запомнить состояние нагрева бойлера ВСЕГДА
	if(!onBoiler) offBoiler=rtcSAM3X8.unixtime();// запомнить время выключения ГВС (нужно для переключения)
	else           offBoiler=0;
#ifdef R3WAY
	dRelay[R3WAY].set_Relay(onBoiler);           // Установить в нужное полежение 3-х ходового
#else // Нет трехходового - схема с двумя насосами
	// ставим сюда код переключения ГВС/отопление в зависимости от onBoiler=true - ГВС
	if (onBoiler) // ГВС
	{
		#ifdef RPUMPBH
		dRelay[RPUMPBH].set_ON();    // ГВС
		#endif
		#ifdef RPUMPFL
		dRelay[RPUMPFL].set_OFF();   // ТП
		#endif
		dRelay[RPUMPO].set_OFF();   // файнкойлы
	}
	else  // Отопление/охлаждение
	{
		if ((Status.modWork==pHEAT)||(Status.modWork==pNONE_H)) // Отопление
		{
			#ifdef RPUMPBH
			dRelay[RPUMPBH].set_OFF();    // ГВС
			#endif
			#ifdef RPUMPFL
			dRelay[RPUMPFL].set_ON();     // ТП
			#endif
			dRelay[RPUMPO].set_ON();     // файнкойлы
		}
		else if ((Status.modWork==pCOOL)||(Status.modWork==pNONE_C)) // Охлаждение
		{
			#ifdef RPUMPBH
			dRelay[RPUMPBH].set_OFF();    // ГВС
			#endif
			#ifdef RPUMPFL
			dRelay[RPUMPFL].set_OFF();    // ТП
			#endif
			dRelay[RPUMPO].set_ON();     // файнкойлы
		}
		else   // Все осталное
		{
			#ifdef RPUMPBH
			dRelay[RPUMPBH].set_OFF();    // ГВС
			#endif
			#ifdef RPUMPFL
			dRelay[RPUMPFL].set_OFF();    // ТП
			#endif
			dRelay[RPUMPO].set_OFF();    // файнкойлы
		}
	}
#endif
	if(get_State()==pWORK_HP)   // Если было перекидывание 3-х ходового и ТН работает то обеспечить дополнительное время (delayBoilerSW сек) для прокачивания гликоля - т.к разные уставки по температуре подачи
	{
		journal.jprintf(" Pause %d sec after switching the 3-way valve . . .\n",HP.Option.delayBoilerSW);
		_delay(HP.Option.delayBoilerSW*1000);  // выравниваем температуру в контуре отопления/ГВС что бы сразу защиты не сработали
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
  if (!(COMPRESSOR_IS_ON))     {dRelay[REVI].set_OFF(); return dRelay[REVI].get_Relay();}                                    // Компрессор выключен и реле включено - выключить реле

  // компрессор работает
  if (rtcSAM3X8.unixtime()-startCompressor<60) return dRelay[REVI].get_Relay() ;                                            // Компрессор работает меньше одной минуты еще рано

  // проверяем условия для включения ЭВИ
  if((sTemp[TCONOUTG].get_Temp()>EVI_TEMP_CON)||(sTemp[TEVAOUTG].get_Temp()<EVI_TEMP_EVA)) { dRelay[REVI].set_ON(); return dRelay[REVI].get_Relay();}  // условия выполнены включить ЭВИ
  else  {dRelay[REVI].set_OFF(); return dRelay[REVI].get_Relay();}                                                          // выключить ЭВИ - условий нет
  return dRelay[REVI].get_Relay();
}
#endif

// Включить или выключить насосы первый параметр их желаемое состояние
// Второй параметр параметр задержка после включения/выключения мсек. отдельного насоса (борьба с помехами)
// Идет проверка на необходимость изменения состояния насосов
// Генерятся задержки для защиты компрессора, есть задержки между включенимями насосов для уменьшения помех
void HeatPump::Pumps(boolean b, uint16_t d)
{
	boolean old = dRelay[PUMP_IN].get_Relay(); // Входное (текущее) состояние определяется по Гео  (СО - могут быть варианты)
	if(b == old) return;                                                        // менять нечего выходим

	// пауза перед выключением насосов контуров, если нужно
	if((!b) && (old)) // Насосы выключены и будут выключены, нужна пауза идет останов компрессора (новое значение выкл  старое значение вкл)
	{
		journal.jprintf(" Pause before stop pumps %d sec . . .\n", Option.delayOffPump);
		_delay(Option.delayOffPump * 1000); // задержка перед выключениме насосов после выключения компрессора (облегчение останова)
	}

	// переключение насосов если есть что переключать (проверка была выше)
	dRelay[PUMP_IN].set_Relay(b);                                 // Реле включения насоса входного контура  (геоконтур)
	_delay(d);                                      // Задержка на d мсек
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
	dRelay[PUMP_OUT].set_Relay(b);                  // Реле включения насоса выходного контура  (отопление и ГВС)
	_delay(d);                                     // Задержка на d мсек
#else
    #ifdef RPUMPBH
    if ((Status.modWork==pBOILER)||(Status.modWork==pNONE_B)) dRelay[RPUMPBH].set_Relay(b);  else // Если бойлер 
    #endif
    dRelay[RPUMPO].set_Relay(b);                  // не бойлер
   _delay(d);                                     // Задержка на d мсек    
#endif
	//  }
	// пауза после включения насосов, если нужно
	if((b) && (!old))                                                // Насосы включены (старт компрессора), нужна пауза
	{
		journal.jprintf(" Pause %d seconds before starting compressor . . .\n", Option.delayOnPump);
		_delay(Option.delayOnPump * 1000);                 // задержка перед включением компрессора
	}

}
// Сброс инвертора если он стоит в ошибке, проверяется наличие инвертора и проверка ошибки
// Проводится различный сброс в зависимсти от конфигурации
int8_t HeatPump::ResetFC() 
{
  if (!dFC.get_present()) return OK;                                                 // Инвертора нет выходим
  
  #ifdef FC_USE_RCOMP                                                               // ЕСЛИ есть провод сброса
          #ifdef SERRFC                                                               // Если есть вход от инвертора об ошибке
            if (sInput[SERRFC].get_lastErr()==OK) return OK;                          // Инвертор сбрасывать не надо
            dRelay[RRESET].set_ON();  _delay(100); dRelay[RRESET].set_OFF();           // Подать импульс сброса
            journal.jprintf("Reset %s use RRESET\r\n",dFC.get_name());
            _delay(100);
            sInput[SERRFC].Read();
            if (sInput[SERRFC].get_lastErr()==OK) return OK;                          // Инвертор сброшен
            else return ERR_reset_FC;                                                 // Сброс не прошел
            #else                                                                      // нет провода сброса - сбрасываем по модбас
              if (dFC.get_err()!=OK)                                                       // инвертор есть ошибки
                {
                dFC.reset_FC();                                                       // подать команду на сброс по модбас
                if (!dFC.get_blockFC())  return OK;                                   // Инвертор НЕ блокирован
                else return ERR_reset_FC;                                             // Сброс не удачный
                }    
              else  return OK;                                                         // Ошибок нет сбрасывать нечего
            #endif // SERRFC
   #else   //  #ifdef FC_USE_RCOMP 
      if (dFC.get_err()!=OK)                                                           //  инвертор есть ошибки
      {
        dFC.reset_FC();                                                               // подать команду на сброс по модбас
      if (!dFC.get_blockFC())  return OK;                                             // Инвертор НЕ блокирован
      else return ERR_reset_FC;                                                       // Сброс не удачный
      }
     else  return OK;                                                                  // Ошибок нет сбрасывать нечего
  #endif  
   return OK;         
  }
           

// START/RESUME -----------------------------------------
// Функция Запуска/Продолжения работы ТН - возвращает ок или код ошибки
// Запускается ВСЕГДА отдельной задачей с приоритетом выше вебсервера
// Параметр задает что делаем true-старт, false-возобновление
int8_t HeatPump::StartResume(boolean start)
{
  volatile MODE_HP mod; 

 // Дана команда старт - но возможно надо переходить в ожидание
#ifdef USE_SCHEDULER  // Определяем что делать
  int8_t profile = HP.Schdlr.calc_active_profile();
  if((profile != SCHDLR_NotActive)&&(start))  // распиание активно и дана команда
	  if (profile == SCHDLR_Profile_off)
	  {
		  startWait=true;                    // Начало работы с ожидания=true;
		  setState(pWAIT_HP);
		  vTaskResume(xHandleUpdate);
      journal.jprintf(" Start task update %s\n",(char*)__FUNCTION__); 
      journal.jprintf(pP_TIME,"%s WAIT . . .\n",(char*)nameHeatPump);
		  return error;
	  }
  if (startWait)
  {
	  startWait=false;
	  start=true;   // Делаем полный запуск, т.к. в положение wait переходили из стопа (расписания)
  }
#endif

 
  #ifndef DEMO   // проверка блокировки инвертора
  if((dFC.get_present())&&(dFC.get_blockFC()))                         // есть инвертор но он блокирован
       {
        journal.jprintf("%s: is blocked, ignore start\n",dFC.get_name());
        setState(pOFF_HP);                                             // Еще ничего не сделали по этому сразу ставим состоение выключено
        error=ERR_MODBUS_BLOCK; set_Error(error,(char*)__FUNCTION__);  return error;
       }   
  #endif
  
   // 1. Переменные  установка, остановка ТН имеет более высокий приоритет чем пуск ! -------------------------
  if (start)  // Команда старт
    {
    if ((get_State()==pWORK_HP)||(get_State()==pSTOPING_HP)||(get_State()==pSTARTING_HP)) return error; // Если ТН включен или уже стартует или идет процесс остановки то ничего не делаем (исключается многократный заход в функцию)
    journal.jprintf(pP_DATE,"  Start . . .\n");
 
    eraseError();                                      // Обнулить ошибку
    if ((error=ResetFC())!=OK)                         // Сброс инвертора если нужно
      {
        setState(pOFF_HP);  // Еще ничего не сделали по этому сразу ставим состоение выключено
        set_Error(error,(char*)__FUNCTION__);  
        return error; 
      } 
    //lastEEV=-1;                                          // -1 это признак того что слежение eev еще не рабоатет (выключения компрессора  небыло)
    }
  else
    {
    if (get_State()!=pWAIT_HP) return error; // Если состяние НЕ РАВНО ожиданию то ничего не делаем, выходим восстанавливать нечего
    journal.jprintf(pP_DATE,"  Resume . . .\n");
    }

    setState(pSTARTING_HP);                              // Производится старт -  флаг
    Status.ret=pNone;                                    // Состояние алгоритма
    lastEEV=-1;                                          // -1 это признак того что слежение eev еще не рабоатет (выключения компрессора  небыло)

    if (startPump)                                       // Проверка задачи насос
        {
          startPump=false;                               // Поставить признак останова задачи насос
          vTaskSuspend(xHandleUpdatePump);               // Остановить задачу насос
          journal.jprintf(" WARNING! %s: Bad startPump, task vUpdatePump RPUMPO pause  . . .\n",(char*)__FUNCTION__);
        } 
        
    stopCompressor=0;                                    // Компрессор никогда не выключался пауза при старте не нужна
    offBoiler=0;                                         // Бойлер никогда не выключался
    onSallmonela=false;                                  // Если true то идет Обеззараживание
    onBoiler=false;                                      // Если true то идет нагрев бойлера
    // Сбросить переменные пид регулятора
    temp_int = 0;                                        // Служебная переменная интегрирования
    errPID=0;                                            // Текущая ошибка ПИД регулятора
    pre_errPID=0;                                        // Предыдущая ошибка ПИД регулятора
    updatePidTime=0;                                     // время обновления ПИДа
    // ГВС Сбросить переменные пид регулятора
    temp_intBoiler = 0;                                  // Служебная переменная интегрирования
    errPIDBoiler=0;                                      // Текущая ошибка ПИД регулятора
    pre_errPIDBoiler=0;                                  // Предыдущая ошибка ПИД регулятора
    updatePidBoiler=0;                                   // время обновления ПИДа
    
    
   // 2.1 Проверка конфигурации, которые можно поменять из морды, по этому проверяем всегда ----------------------------------------
      if ((Prof.SaveON.mode==pOFF)&&(!(GETBIT(Prof.SaveON.flags,fBoilerON))))   // Нет работы для ТН - ничего не включено
         {
          setState(pOFF_HP);  // Еще ничего не сделали по этому сразу ставим состоение выключено
          error=ERR_NO_WORK;
          set_Error(error,(char*)__FUNCTION__);  
          return error; 
         }
         
       #ifdef EEV_DEF
      if ((!sADC[PEVA].get_present())&&(dEEV.get_ruleEEV()==TEVAOUT_PEVA))  //  Отсутвует датчик давления, и выбран алгоритм ЭРВ который его использует",
        {
         setState(pOFF_HP);    // Еще ничего не сделали по этому сразу ставим состоение выключено
         error=ERR_PEVA_EEV;
         set_Error(error,(char*)__FUNCTION__);        // остановить по ошибке;
         return error;
        }
       #endif
       
      // 2.2 Проверка конфигурации, которые определены конфигом (поменять нельзя), по этому проверяем один раз при страте ТН ----------------------------------------
      if (start)  // Команда старт
        {
          if (!dRelay[PUMP_OUT].get_present())  // отсутсвует насос на конденсаторе, пользователь НЕ может изменить в процессе работы проверка при старте
           {
            setState(pOFF_HP);    // Еще ничего не сделали по этому сразу ставим состоение выключено
            error=ERR_PUMP_CON;
            set_Error(error,(char*)__FUNCTION__);        // остановить по ошибке;
             return error;
            }
          if (!dRelay[PUMP_IN].get_present())   // отсутсвует насос на испарителе, пользователь может изменить в процессе работы
           {
             setState(pOFF_HP);    // Еще ничего не сделали по этому сразу ставим состоение выключено
             error=ERR_PUMP_EVA;
             set_Error(ERR_PUMP_EVA,(char*)__FUNCTION__);        // остановить по ошибке;
             return error;
            }
          if ((!dRelay[RCOMP].get_present())&&(!dFC.get_present()))   // отсутсвует компрессор, пользователь может изменить в процессе работы
            {
             setState(pOFF_HP);    // Еще ничего не сделали по этому сразу ставим состоение выключено
             error=ERR_NO_COMPRESS;
             set_Error(error,(char*)__FUNCTION__);        // остановить по ошибке;
             return error;
            }
        } //  if (start)  // Команда старт
      
  // 3.  ПОДГОТОВКА ------------------------------------------------------------------------
    relayAllOFF();                                          // Выключить все реле, в принципе это лишнее
   
   if (start)  // Команда старт - Инициализация ЭРВ и очистка графиков при восстановлени не нужны
     {
     #ifdef EEV_DEF
     journal.jprintf(" EEV init\n");
     if (get_State()!=pSTARTING_HP) return error;            // Могли нажать кнопку стоп, выход из процесса запуска
     else  dEEV.Start();                                     // Включить ЭРВ  найти 0 по завершению позиция 0!!!
     #endif
        
     journal.jprintf(" Charts clear and start\n");
     if (get_State()!=pSTARTING_HP) return error;            // Могли нажать кнопку стоп, выход из процесса запуска
     else  startChart();                                      // Запустить графики
     }
     
     // 4. Определяем что нужно делать -----------------------------------------------------------
   if (get_State()!=pSTARTING_HP) return error;            // Могли нажать кнопку стоп, выход из процесса запуска
   else  mod=get_Work();                                   // определяем что делаем с компрессором
   if (mod>pBOILER) mod=pOFF;                              // При первом пуске могут быть только состояния pOFF,pHEAT,pCOOL,pBOILER
   journal.jprintf( " Start modWork:%d[%s]\n",(int)mod,codeRet[Status.ret]);

   // 5. Конфигурируем ТН -----------------------------------------------------------------------
   if (get_State()!=pSTARTING_HP) return error;            // Могли нажать кнопку стоп, выход из процесса запуска
   else  configHP(mod);                                    // Конфигурируем 3 и 4-х клапаны и включаем насосы ПАУЗА после включения насосов

   // 7. Дополнительнеая проверка перед пуском компрессора ----------------------------------------
   if (get_State()!=pSTARTING_HP) return error;          // Могли нажать кнопку стоп, выход из процесса запуска
     if (get_errcode()!=OK)                              // ОШИБКА компрессор уже работает
     {
      journal.jprintf(" There is an error, the compressor is not on\n");
      set_Error(ERR_COMP_ERR,(char*)__FUNCTION__); return error; 
     }   
     
    // 9. Включение компрессора и запуск обновления EEV -----------------------------------------------------
    if (get_State()!=pSTARTING_HP) return error;                         // Могли нажать кнопку стоп, выход из процесса запуска
    if ((mod==pCOOL)||(mod==pHEAT)||(mod==pBOILER))   compressorON(mod); // Компрессор включить если нет ошибок и надо включаться
          
     // 10. Запуск задачи обновления ТН ---------------------------------------------------------------------------
     if(start)
     {
     vTaskResume(xHandleUpdate);                                       // Запустить задачу Обновления ТН, дальше она все доделает
     journal.jprintf(" Start task update %s\n",(char*)__FUNCTION__); 
     }
     
     // 11. Сохранение состояния  -------------------------------------------------------------------------------
     if (get_State()!=pSTARTING_HP) return error;                   // Могли нажать кнопку стоп, выход из процесса запуска
     setState(pWORK_HP);
     journal.jprintf(pP_TIME,"%s ON . . .\n",(char*)nameHeatPump);
  return error;
}

                     
// STOP/WAIT -----------------------------------------
// Функция Останова/Ожидания ТН  - возвращает код ошибки
// Параметр задает что делаем true-останов, false-ожидание
int8_t HeatPump::StopWait(boolean stop)
{  
  if (stop)
    {
    if ((get_State()==pOFF_HP)||(get_State()==pSTOPING_HP)) return error;    // Если ТН выключен или выключается ничего не делаем
    setState(pSTOPING_HP);  // Состояние выключения
    journal.jprintf(pP_DATE,"   Stop . . .\n"); 
    }
  else
    {
    if ((get_State()==pOFF_HP)||(get_State()==pSTOPING_HP)||(get_State()==pWAIT_HP)) return error;    // Если ТН выключен или выключается или ожидание ничего не делаем
    setState(pSTOPING_HP);  // Состояние выключения
    journal.jprintf(pP_DATE,"   Switch to waiting . . .\n");    
    }
    
  if (onBoiler) // Если надо уйти с ГВС для облегчения останова компресора
      {
        switchBoiler(false);
      }
  if (COMPRESSOR_IS_ON) { COMPRESSOR_OFF;  stopCompressor=rtcSAM3X8.unixtime();}      // Выключить компрессор и запомнить веремя

  if (stop) //Обновление ТН отключаем только при останове
    {
    vTaskSuspend(xHandleUpdate);                           // Остановить задачу обновления ТН
    journal.jprintf(" Stop task update %s\n",(char*)__FUNCTION__); 
    } 
    
  if(startPump)
  {
     startPump=false;                                    // Поставить признак что насос выключен
     vTaskSuspend(xHandleUpdatePump);                    // Остановить задачу насос
     journal.jprintf(" %s: Task vUpdatePump RPUMPO off . . .\n",(char*)__FUNCTION__);
  }

 // Принудительное выключение отдельных узлов ТН если они есть в конфиге
  #ifdef RBOILER  // управление дополнительным ТЭНом бойлера
  if(boilerAddHeat()) { // Если используется тэн
     if (dRelay[RBOILER].get_Relay()) dRelay[RBOILER].set_OFF();  // выключить тен бойлера
  }
  #endif

  #ifdef RHEAT  // управление  ТЭНом отопления
     if (dRelay[RHEAT].get_Relay()) dRelay[RHEAT].set_OFF();     // выключить тен отопления
  #endif

  #ifdef RPUMPB  // управление  насосом циркуляции ГВС
     if (dRelay[RPUMPB].get_Relay()) dRelay[RPUMPB].set_OFF();     // выключить насос циркуляции ГВС
  #endif

  #ifdef RPUMPFL  // управление  насосом циркуляции ТП
     if (dRelay[RPUMPFL].get_Relay()) dRelay[RPUMPFL].set_OFF();    // выключить насос циркуляции ТП
  #endif

  #ifdef RPUMPBH  // управление  насосом нагрева ГВС
     if (dRelay[RPUMPBH].get_Relay()) dRelay[RPUMPBH].set_OFF();
  #endif

  PUMPS_OFF;                                                       // выключить насосы контуров
  
  #ifdef EEV_DEF
  if(GETBIT(Option.flags,fEEV_close))            //ЭРВ само выключится по State
     { 
     journal.jprintf(" Pause before closing EEV %d sec . . .\n",dEEV.get_delayOff());
     _delay(dEEV.get_delayOff()*1000); // пауза перед закрытием ЭРВ  на инверторе компрессор останавливается до 2 минут
     dEEV.set_EEV(dEEV.get_minSteps());                          // Если нужно, то закрыть ЭРВ
     journal.jprintf(" EEV go minSteps\n"); 
     } 
   #endif
   
 // ЭРВ само выключится по State
//  vTaskSuspend(xHandleUpdateEEV);                       // Остановить задачу обновления ЭРВ
//  #ifdef DEBUG 
//      Serial.println(" Stop task update EEV"); 
//  #endif
   
 
  relayAllOFF();                                         // Все выключить, все  (на всякий случай)
  if (stop)
    {
     vTaskSuspend(xHandleUpdateStat);                    // Остановить задачу обновления статистики
     journal.jprintf(" statChart stop\n");      
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

// Получить информацию что надо делать сейчас Реализует логику и приоритеты режимов
// Управляет дополнительным нагревателем бойлера
MODE_HP HeatPump::get_Work()
{
    MODE_HP ret=pOFF;
    Status.ret=pNone;           // не определено
     
    // 1. Бойлер
    switch ((int)UpdateBoiler())  // проверка бойлера высший приоритет
    {
      case  pCOMP_OFF:  ret=pOFF;      break;
      case  pCOMP_ON:   ret=pBOILER;   break;
      case  pCOMP_NONE: ret=pNONE_B;   break;
    }

   // 2. Дополнительный нагреватель бойлера включение/выключение
   #ifdef RBOILER  // Управление дополнительным ТЭНом бойлера (все режимы ТУРБО и ДОГРЕВ, сальмонелла)
   if (boilerAddHeat()) dRelay[RBOILER].set_ON(); else dRelay[RBOILER].set_OFF(); 
   #endif
    
   if ((ret==pBOILER)||(ret==pNONE_B)) return ret;                   // работает бойлер больше ничего анализировать не надо
               
    // Обеспечить переключение с бойлера на отопление/охлаждение
    if(((Status.ret==pBp22)||(Status.ret==pBh3))&&(onBoiler)) // если бойлер выключяетя по достижению цели И режим ГВС
     {
      switchBoiler(false);                       // выключить бойлер (задержка в функции) имеено здесь  - а то дальше защиты сработают
     }
   

    // 3. Отопление/охлаждение
    switch ((int)get_mode())   // проверка отопления
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
#define STR_FREQUENCY " FC> %.2f Hz\n"            // Экономим место
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

	if ((!scheduleBoiler())||(!GETBIT(Prof.SaveON.flags,fBoilerON))) // Если запрещено греть бойлер согласно расписания ИЛИ  Бойлер выключен, выходим и можно смотреть отопление
	{
	 #ifdef RBOILER  // управление дополнительным ТЭНом бойлера
		if(faddheat) {
			dRelay[RBOILER].set_OFF();flagRBOILER=false; // Выключение
		}
	 #endif
	 return pCOMP_OFF;             // запрещено греть бойлер согласно расписания
	}
// -----------------------------------------------------------------------------------------------------
// Сброс излишней энергии в систему отопления
// Переключаем 3-х ходовой на отопление на ходу и ждем определенное число минут дальше перекидываем на бойлер
float u, u_dif, u_int, u_pro;
int16_t newFC;               //Новая частота инвертора

if(GETBIT(Prof.Boiler.flags,fResetHeat))                   // Стоит требуемая опция - Сброс тепла в СО
 {
 // Достигнута максимальная температура подачи - 1 градус или температура нагнетания компрессора больше максимальной - 5 градусов
 if ((FEED>Prof.Boiler.tempIn-100)||(sTemp[TCOMP].get_Temp()>sTemp[TCOMP].get_maxTemp()-500)) 
   {
    journal.jprintf(" Discharge of excess heat in the heating system\n");
    switchBoiler(false);               // Переключится на ходу на отопление
    journal.jprintf(" Pause %d  minutes after switching the 3-way valve . . .\n",Prof.Boiler.Reset_Time/60);
    _delay(Prof.Boiler.Reset_Time*1000);  // Сброс требуемое число  минут для системы отопления
    switchBoiler(true);              // Переключится на ходу на ГВС
    journal.jprintf(" Switching on the boiler and the heating on\n");
   }      
 }  

    Status.ret=pNone;                // Сбросить состояние пида
 // Алгоритм гистерезис для старт стоп
 if(!dFC.get_present()) // Алгоритм гистерезис для старт стоп
 {
     if (FEED>Prof.Boiler.tempIn)                                         {Status.ret=pBh1; return pCOMP_OFF; }    // Достигнута максимальная температура подачи ВЫКЛ)
    
     // Отслеживание выключения (с учетом догрева)
     if ((!GETBIT(Prof.Boiler.flags,fTurboBoiler))&&(GETBIT(Prof.Boiler.flags,fAddHeating)))  // режим догрева
     {
      if (sTemp[TBOILER].get_Temp()>Prof.Boiler.tempRBOILER)   {Status.ret=pBh22; return pCOMP_OFF; }  // Температура выше целевой температуры ДОГРЕВА надо выключаться!
     }
     else 
     {
       if (sTemp[TBOILER].get_Temp()>Prof.Boiler.TempTarget)   {Status.ret=pBh3; return pCOMP_OFF; }  // Температура выше целевой температуры БОЙЛЕРА надо выключаться!
     }
     // Отслеживание включения
     if (sTemp[TBOILER].get_Temp()<(Prof.Boiler.TempTarget-Prof.Boiler.dTemp)) {Status.ret=pBh2; return pCOMP_ON;  }    // Температура ниже гистрезиса надо включаться!
  
      // дошли до сюда значить сохранение предыдущего состяния, температура в диапазоне регулирования может быть или нагрев или остывание
     if (onBoiler)                                                           {Status.ret=pBh4; return pCOMP_NONE; }  // Если включен принак работы бойлера (трехходовой) значит ПРОДОЛЖНЕНИЕ нагрева бойлера
     Status.ret=pBh5;  return pCOMP_OFF;    // продолжение ПАУЗЫ бойлера внутри гистрезиса
 } // if(!dFC.get_present())
 else // ИНвертор ПИД
 {
     if(FEED>Prof.Boiler.tempIn) {Status.ret=pBp1; set_Error(ERR_PID_FEED,(char*)__FUNCTION__);return pCOMP_OFF;}         // Достижение максимальной температуры подачи - это ошибка ПИД не рабоатет
     // Отслеживание выключения (с учетом догрева)
     if ((!GETBIT(Prof.Boiler.flags,fTurboBoiler))&&(GETBIT(Prof.Boiler.flags,fAddHeating)))  // режим догрева
     {
      if (sTemp[TBOILER].get_Temp()>Prof.Boiler.tempRBOILER)   {Status.ret=pBp22; return pCOMP_OFF; }  // Температура выше целевой температуры ДОГРЕВА надо выключаться!
     }
     else 
     {
       if (sTemp[TBOILER].get_Temp()>Prof.Boiler.TempTarget)   {Status.ret=pBp3; return pCOMP_OFF; }  // Температура выше целевой температуры БОЙЛЕРА надо выключаться!
     }
     // Отслеживание включения
    if ((sTemp[TBOILER].get_Temp()<(Prof.Boiler.TempTarget-Prof.Boiler.dTemp))&&(!(onBoiler))) {Status.ret=pBp2; return pCOMP_ON;} // Достигнут гистерезис и компрессор еще не рабоатет на ГВС - Старт бойлера
    else if ((dFC.isfOnOff())&&(!(onBoiler))) return pCOMP_OFF;                               // компрессор рабатает но ГВС греть не надо  - уходим без изменения состояния
     
   //    if (sTemp[TBOILER].get_Temp()<(Prof.Boiler.TempTarget-Prof.Boiler.dTemp)) {Status.ret=pBh2; return pCOMP_ON;  }    // Температура ниже гистрезиса надо включаться!
    // ПИД ----------------------------------
   // ЗАЩИТА Компресор работает, достигнута максимальная температура подачи, мощность, температура компрессора то уменьшить обороты на stepFreq
   else if ((dFC.isfOnOff())&&(FEED>Prof.Boiler.tempIn-dFC.get_dtTempBoiler()))             // Подача ограничение
   { 
    Status.ret=pBp6;
    journal.jprintf("%s %.2f (FEED: %.2f)\n",STR_REDUCED,dFC.get_stepFreqBoiler()/100.0,FEED/100.0);
    if (dFC.get_targetFreq()-dFC.get_stepFreqBoiler()<dFC.get_minFreqBoiler())  return pCOMP_OFF;         // Уменьшать дальше некуда, выключаем компрессор
    dFC.set_targetFreq(dFC.get_targetFreq()-dFC.get_stepFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler()); return pCOMP_NONE;                // Уменьшить частоту
   }  
  else if ((dFC.isfOnOff())&&(dFC.get_power()>FC_MAX_POWER_BOILER))                    // Мощность для ГВС меньшая мощность
   { 
    Status.ret=pBp7;
    journal.jprintf("%s %.2f (POWER: %.2f kW)\n",STR_REDUCED,dFC.get_stepFreqBoiler()/100.0,dFC.get_power()/10.0); // КИЛОВАТЫ
    if (dFC.get_targetFreq()-dFC.get_stepFreqBoiler()<dFC.get_minFreqBoiler())  return pCOMP_OFF;        // Уменьшать дальше некуда, выключаем компрессор
    dFC.set_targetFreq(dFC.get_targetFreq()-dFC.get_stepFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler()); return pCOMP_NONE;               // Уменьшить частоту
   } 
  else if ((dFC.isfOnOff())&&(dFC.get_current()>FC_MAX_CURRENT_BOILER))                // ТОК для ГВС меньшая мощность
   { 
    Status.ret=pBp16;
    journal.jprintf("%s %.2f (CURRENT: %.2f)\n",STR_REDUCED,dFC.get_stepFreqBoiler()/100.0,dFC.get_current()/100.0);
    if (dFC.get_targetFreq()-dFC.get_stepFreqBoiler()<dFC.get_minFreqBoiler())  return pCOMP_OFF;        // Уменьшать дальше некуда, выключаем компрессор
    dFC.set_targetFreq(dFC.get_targetFreq()-dFC.get_stepFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler()); return pCOMP_NONE;               // Уменьшить частоту
   } 
   
 else if ((dFC.isfOnOff())&&((sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp())>sTemp[TCOMP].get_maxTemp()))  // температура компрессора
   { 
    Status.ret=pBp8;
    journal.jprintf("%s %.2f (TCOMP: %.2f)\n",STR_REDUCED,dFC.get_stepFreqBoiler()/100.0,sTemp[TCOMP].get_Temp()/100.0);
    if (dFC.get_targetFreq()-dFC.get_stepFreqBoiler()<dFC.get_minFreqBoiler())  return pCOMP_OFF;       // Уменьшать дальше некуда, выключаем компрессор
    dFC.set_targetFreq(dFC.get_targetFreq()-dFC.get_stepFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler()); return pCOMP_NONE;              // Уменьшить частоту
   } 
 else if ((dFC.isfOnOff())&&(sADC[PCON].get_present())&&(sADC[PCON].get_Press()>sADC[PCON].get_maxPress()-FC_DT_CON_PRESS))    // давление конденсатора до максимальной минус 0.5 бара
   { 
    Status.ret=pBp9;
    journal.jprintf("%s %.2f (PCON:  %.2f)\n",STR_REDUCED,dFC.get_stepFreqBoiler()/100.0,sADC[PCON].get_Press()/100.0);
    if (dFC.get_targetFreq()-dFC.get_stepFreqBoiler()<dFC.get_minFreqBoiler())  return pCOMP_OFF;      // Уменьшать дальше некуда, выключаем компрессор
    dFC.set_targetFreq(dFC.get_targetFreq()-dFC.get_stepFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler());  return pCOMP_NONE;            // Уменьшить частоту
   } 
   
  //   else if (((Prof.Boiler.TempTarget-Prof.Boiler.dTemp)>sTemp[TBOILER].get_Temp())&&(!(dFC.isfOnOff())&&(Status.modWork!=pBOILER))) {Status.ret=7; return pCOMP_ON;} // Достигнут гистерезис и компрессор еще не рабоатет на ГВС - Старт бойлера
     else if(!(dFC.isfOnOff())) {Status.ret=pBp5; return pCOMP_OFF; }                                                             // Если компрессор не рабоатет то ничего не делаем и выходим
     else if (rtcSAM3X8.unixtime()-dFC.get_startTime()<FC_ACCEL_TIME/100 ){Status.ret=pBp10; return pCOMP_NONE;  }             // РАЗГОН частоту не трогаем
     
     else if(xTaskGetTickCount()/1000-updatePidBoiler<HP.get_timeBoiler())   {Status.ret=pBp11; return pCOMP_NONE;  }             // время обновления ПИДа еше не пришло
     // Дошли до сюда - ПИД на подачу. Компресор работает
     updatePidBoiler=xTaskGetTickCount()/1000; 
     // Уравнение ПИД регулятора в конечных разностях. ------------------------------------
     // Cp, Ci, Cd – коэффициенты дискретного ПИД регулятора;
     // u(t) = P (t) + I (t) + D (t);
     // P (t) = Kp * e (t);
     // I (t) = I (t — 1) + Ki * e (t);
     // D (t) = Kd * {e (t) — e (t — 1)};
     // T – период дискретизации(период, с которым вызывается ПИД регулятор).
     #ifdef SUPERBOILER     
       Status.ret=pBp14;          
       errPIDBoiler=((float)(Prof.Boiler.tempPID-PressToTemp(HP.sADC[PCON].get_Press(),HP.dEEV.get_typeFreon())))/100.0; // Текущая ошибка (для Жени, по давлению), переводим в градусы
     #else
        Status.ret=pBp12;
        errPIDBoiler=((float)(Prof.Boiler.tempPID-FEED))/100.0;                                       // Текущая ошибка, переводим в градусы ("+" недогрев частоту увеличивать "-" перегрев частоту уменьшать)
     #endif
         // Расчет отдельных компонент
         if (Prof.Boiler.Ki>0)                                                                        // Расчет интегральной составляющей
         {
          temp_int=temp_int+((float)Prof.Boiler.Ki*errPIDBoiler)/100.0;                               // Интегральная составляющая, с накоплением делить на 10
          #define BOILER_MAX_STEP  2                                                                  // Ограничение диапзона изменения 2 герц за один шаг ПИД
          if (temp_int>BOILER_MAX_STEP)  temp_int=BOILER_MAX_STEP; 
          if (temp_int<-1.0*BOILER_MAX_STEP)  temp_int=-1.0*BOILER_MAX_STEP; 
         }
         else temp_int=0;                                                                             // если Кi равен 0 то интегрирование не используем
         u_int=temp_int;
        
         // Дифференцальная составляющая
         u_dif=((float)Prof.Boiler.Kd*(errPIDBoiler-pre_errPIDBoiler))/10.0;                          // Положительная составляющая - ошибка растет (воздействие надо увеличиить)  Отрицательная составляющая - ошибка уменьшается (воздействие надо уменьшить)
         
         // Пропорциональная составляющая (десятые)
         u_pro=(float)Prof.Boiler.Kp*errPIDBoiler/10.0;
         
         // Общее воздействие
         u=u_pro+u_int+u_dif;
         if (u>dFC.get_PidFreqStep()/100.0) u=dFC.get_PidFreqStep()/100.0;                                    // Ограничить увеличение частоты на PidFreqStep гц
         
         newFC=100.0*u+dFC.get_targetFreq();                                                                  // Округление не нужно и добавление предудущего значения, умногжжение на 100 это перевод в 0.01 герцах
         pre_errPIDBoiler=errPIDBoiler;                                                               // Сохранние ошибки, теперь это прошлая ошибка                                                                           // запомнить предыдущую ошибку
       
       if (newFC>dFC.get_maxFreqBoiler())   newFC=dFC.get_maxFreqBoiler();                                                 // ограничение диапазона ОТДЕЛЬНО для ГВС!!!! (меньше мощность)
       if (newFC<dFC.get_minFreqBoiler())   newFC=dFC.get_minFreqBoiler(); //return pCOMP_OFF;                             // Уменьшать дальше некуда, выключаем компрессор
       
       // Смотрим подход к границе защит если идет УВЕЛИЧЕНИЕ частоты
       if (dFC.get_targetFreq()<newFC)                                                                                     // Идет увеличение частоты проверяем подход к границам
         {
         if ((dFC.isfOnOff())&&(FEED>(Prof.Boiler.tempIn-dFC.get_dtTempBoiler())*dFC.get_PidStop()/100))                                                 {Status.ret=pBp17; return pCOMP_NONE;}   // Подача ограничение
         if ((dFC.isfOnOff())&&(dFC.get_power()>(FC_MAX_POWER_BOILER*dFC.get_PidStop()/100)))                                                            {Status.ret=pBp18; return pCOMP_NONE;}   // Мощность для ГВС меньшая мощность
         if ((dFC.isfOnOff())&&(dFC.get_current()>(FC_MAX_CURRENT_BOILER*dFC.get_PidStop()/100)))                                                        {Status.ret=pBp19; return pCOMP_NONE;}   // ТОК для ГВС меньшая мощность
         if ((dFC.isfOnOff())&&((sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp())>(sTemp[TCOMP].get_maxTemp()*dFC.get_PidStop()/100)))                      {Status.ret=pBp20; return pCOMP_NONE;}   // температура компрессора
         if ((dFC.isfOnOff())&&(sADC[PCON].get_present())&&(sADC[PCON].get_Press()>((sADC[PCON].get_maxPress()-FC_DT_CON_PRESS)*dFC.get_PidStop()/100))) {Status.ret=pBp21; return pCOMP_NONE;}   // давление конденсатора до максимальной минус 0.5 бара
         }
       //    надо менять
       if (dFC.get_targetFreq()!=newFC)                                                                                     // Установка частоты если нужно менять
         {
         journal.jprintf((char*)STR_FREQUENCY,newFC/100.0); 
         dFC.set_targetFreq(newFC,false,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler());
         }
       return pCOMP_NONE;  
 }
   
}
// -----------------------------------------------------------------------------------------------------
// Итерация по управлению НАГРЕВ  пола
// выдает что надо делать компрессору, но ничем не управляетдля старт-стопа для инвертора меняет обороты
MODE_COMP HeatPump::UpdateHeat()
{
int16_t target,t1,targetRealPID;
float u, u_dif, u_int, u_pro;      
int16_t newFC;               //Новая частота инвертора

if ((get_State()==pOFF_HP)||(get_State()==pSTOPING_HP)) return pCOMP_OFF;    // Если ТН выключен или выключается ничего не делаем

if ((rtcSAM3X8.unixtime()-offBoiler>Option.delayBoilerOff)&&((abs(FEED-RET)>Prof.Heat.dt)&&(COMPRESSOR_IS_ON)))   {set_Error(ERR_DTEMP_CON,(char*)__FUNCTION__);return pCOMP_NONE;}// Привышение разности температур кондесатора при включеноом компрессорае
#ifdef RTRV    
   if ((dRelay[RTRV].get_Relay())&&(COMPRESSOR_IS_ON))      dRelay[RTRV].set_OFF();  // отопление Проверить и если надо установить 4-ходовой клапан только если компрессор рабоатет (защита это лишнее)
#endif
Status.ret=pNone;                                                                                   // Сбросить состояние пида
switch (Prof.Heat.Rule)   // в зависмости от алгоритма
         {
          case pHYSTERESIS:  // Гистерезис нагрев.
               if (GETBIT(Prof.Heat.flags,fTarget)){ target=Prof.Heat.Temp2; t1=RET;}  else { target=Prof.Heat.Temp1; t1=sTemp[TIN].get_Temp();}  // вычислить темературы для сравнения Prof.Heat.Target 0-дом   1-обратка
                    if(t1>target)                  {Status.ret=pHh3;   return pCOMP_OFF;}                            // Достигнута целевая температура  ВЫКЛ
               else if((rtcSAM3X8.unixtime()-offBoiler>Option.delayBoilerOff)&&(FEED>Prof.Heat.tempIn)){Status.ret=pHh1;   return pCOMP_OFF;} // Достигнута максимальная температура подачи ВЫКЛ
                 else if(t1<target-Prof.Heat.dTemp) {Status.ret=pHh2;   return pCOMP_ON; }                           // Достигнут гистерезис ВКЛ
               else if(RET<Prof.Heat.tempOut)      {Status.ret=pHh13;  return pCOMP_ON; }                            // Достигнут минимальная темература обратки ВКЛ
               else                                {Status.ret=pHh4;   return pCOMP_NONE;}                           // Ничего не делаем  (сохраняем состояние)
              break;
          case pPID:   // ПИД регулирует подачу, а целевай функция гистререзис
            // отработка гистререзиса целевой функции (дом/обратка)
            if (GETBIT(Prof.Heat.flags,fTarget)) { target=Prof.Heat.Temp2; t1=RET;}  else { target=Prof.Heat.Temp1; t1=sTemp[TIN].get_Temp();} // вычислить темературы для сравнения Prof.Heat.Target 0-дом  1-обратка
            
             if(t1>target)             { Status.ret=pHp3; return pCOMP_OFF;}                            // Достигнута целевая температура  ВЫКЛ
             else if((rtcSAM3X8.unixtime()-offBoiler>Option.delayBoilerOff)&&(FEED>Prof.Heat.tempIn)) {Status.ret=pHp1; set_Error(ERR_PID_FEED,(char*)__FUNCTION__);return pCOMP_OFF;}        // Достижение максимальной температуры подачи - это ошибка ПИД не рабоатет
           //  else if ((t1<target-Prof.Heat.dTemp)&&(!(dFC.isfOnOff())))  {Status.ret=pHp2; return pCOMP_ON; }                     // Достигнут гистерезис и компрессор еще не рабоатет ВКЛ
           //  else if ((t1<target-Prof.Heat.dTemp)&&(!(dFC.isfOnOff())))  {Status.ret=pHp2; return pCOMP_ON; }                       // Достигнут гистерезис (компрессор не рабоатет) ВКЛ
           //  else if ((t1<target-Prof.Heat.dTemp)&&(dFC.isfOnOff())&&(dRelay[R3WAY].get_Relay())) {Status.ret=pHp2; return pCOMP_ON;} // Достигнут гистерезис (бойлер нагрет) ВКЛ
             else if ((t1<target-Prof.Heat.dTemp)&&(!(dFC.isfOnOff())))  {Status.ret=pHp2; return pCOMP_ON; }                       // Достигнут гистерезис (компрессор не рабоатет) ВКЛ
             else if ((t1<target-Prof.Heat.dTemp)&&(dFC.isfOnOff())&&(onBoiler)) {Status.ret=pHp2; return pCOMP_ON;} // Достигнут гистерезис (бойлер нагрет) ВКЛ
             
             // ЗАЩИТА Компресор работает, достигнута максимальная температура подачи, мощность, температура компрессора или давление то уменьшить обороты на stepFreq
              else if ((dFC.isfOnOff())&&(FEED>Prof.Heat.tempIn-dFC.get_dtTemp()))                  // Подача ограничение
             { 
              Status.ret=pHp6;
              journal.jprintf("%s %.2f (FEED: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,FEED/100.0);
              if (dFC.get_targetFreq()-dFC.get_stepFreq()<dFC.get_minFreq())  return pCOMP_OFF;                // Уменьшать дальше некуда, выключаем компрессор
              dFC.set_targetFreq(dFC.get_targetFreq()-dFC.get_stepFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq());  return pCOMP_NONE;                     // Уменьшить частоту
             }  
  
            else if ((dFC.isfOnOff())&&(dFC.get_power()>FC_MAX_POWER))                    // Мощность в 100 ватт
             { 
              Status.ret=pHp7;
              journal.jprintf("%s %.2f (POWER: %.2f kW)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,dFC.get_power()/10.0); // КИЛОВАТЫ
              if (dFC.get_targetFreq()-dFC.get_stepFreq()<dFC.get_minFreq())  return pCOMP_OFF;                // Уменьшать дальше некуда, выключаем компрессор
              dFC.set_targetFreq(dFC.get_targetFreq()-dFC.get_stepFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq());  return pCOMP_NONE;                     // Уменьшить частоту
             } 
            else if ((dFC.isfOnOff())&&(dFC.get_current()>FC_MAX_CURRENT))                // ТОК
             { 
              Status.ret=pHp16;
              journal.jprintf("%s %.2f (CURRENT: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,dFC.get_current()/100.0);
              if (dFC.get_targetFreq()-dFC.get_stepFreq()<dFC.get_minFreq())  return pCOMP_OFF;                // Уменьшать дальше некуда, выключаем компрессор
              dFC.set_targetFreq(dFC.get_targetFreq()-dFC.get_stepFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq());  return pCOMP_NONE;                     // Уменьшить частоту
             } 
           
           else if ((dFC.isfOnOff())&&((sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp())>sTemp[TCOMP].get_maxTemp()))  // температура компрессора
             { 
              Status.ret=pHp8;
              journal.jprintf("%s %.2f (TCOMP: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,sTemp[TCOMP].get_Temp()/100.0);
              if (dFC.get_targetFreq()-dFC.get_stepFreq()<dFC.get_minFreq())  return pCOMP_OFF;                // Уменьшать дальше некуда, выключаем компрессор
              dFC.set_targetFreq(dFC.get_targetFreq()-dFC.get_stepFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq());  return pCOMP_NONE;                     // Уменьшить частоту
             } 
          else if ((dFC.isfOnOff())&&(sADC[PCON].get_present())&&(sADC[PCON].get_Press()>sADC[PCON].get_maxPress()-FC_DT_CON_PRESS))  // давление конденсатора до максимальной минус 0.5 бара
           {
            Status.ret=pHp9; 
            journal.jprintf("%s %.2f (PCON:  %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,sADC[PCON].get_Press()/100.0);
            if (dFC.get_targetFreq()-dFC.get_stepFreq()<dFC.get_minFreq())  return pCOMP_OFF;                // Уменьшать дальше некуда, выключаем компрессор
            dFC.set_targetFreq(dFC.get_targetFreq()-dFC.get_stepFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq());  return pCOMP_NONE;               // Уменьшить частоту
           }           
           else if(!(dFC.isfOnOff())) {Status.ret=pHp5; return pCOMP_NONE;  }                                               // Если компрессор не рабоатет то ничего не делаем и выходим
           else if (rtcSAM3X8.unixtime()-dFC.get_startTime()<FC_ACCEL_TIME/100 ){ Status.ret=pHp10; return pCOMP_NONE;}  // РАЗГОН частоту не трогаем

           #ifdef SUPERBOILER                                            // Бойлер греется от предкондесатора
               if (sTemp[TCOMP].get_Temp()-SUPERBOILER_DT>sTemp[TBOILER].get_Temp())  dRelay[RSUPERBOILER].set_ON(); else dRelay[RSUPERBOILER].set_OFF();// исправил плюс на минус
               if(xTaskGetTickCount()/1000-updatePidTime<HP.get_timeHeat())         { Status.ret=pHp11;   return pCOMP_NONE;}   // время обновления ПИДа еше не пришло
               if (onBoiler) Status.ret=pHp15; else Status.ret=pHp12;                                          // если нужно показывем что бойлер греется от предкондесатора
           #else
               else if(xTaskGetTickCount()/1000-updatePidTime<HP.get_timeHeat())    { Status.ret=pHp11;   return pCOMP_NONE;}   // время обновления ПИДа еше не пришло
               Status.ret=pHp12;   // Дошли до сюда - ПИД на подачу. Компресор работает
           #endif      
                   
            updatePidTime=xTaskGetTickCount()/1000; 
           // Уравнение ПИД регулятора в конечных разностях. ------------------------------------
           // Cp, Ci, Cd – коэффициенты дискретного ПИД регулятора;
           // u(t) = P (t) + I (t) + D (t);
           // P (t) = Kp * e (t);
           // I (t) = I (t — 1) + Ki * e (t);
           // D (t) = Kd * {e (t) — e (t — 1)};
           // T – период дискретизации(период, с которым вызывается ПИД регулятор).
           
           if(GETBIT(Prof.Heat.flags,fWeather))  // включена погодозависимость
           { 
            targetRealPID=Prof.Heat.tempPID+(Prof.Heat.kWeather*(TEMP_WEATHER-sTemp[TOUT].get_Temp())/1000);  // включена погодозависимость, коэффициент в ТЫСЯЧНЫХ результат в сотых градуса
  //          journal.jprintf("targetRealPID=%d \n",targetRealPID); 
  //          journal.jprintf("Prof.Heat.tempPID=%d \n",Prof.Heat.tempPID); 
  //          journal.jprintf("Prof.Heat.kWeather=%d \n",Prof.Heat.kWeather); 
  //          journal.jprintf("TEMP_WEATHER=%d \n",TEMP_WEATHER); 
  //          journal.jprintf("sTemp[TOUT].get_Temp()=%d \n",sTemp[TOUT].get_Temp()); 
            if (targetRealPID>Prof.Heat.tempIn-50) targetRealPID=Prof.Heat.tempIn-50;  // ограничение целевой подачи = максимальная подача - 0.5 градуса
            if (targetRealPID<MIN_WEATHER) targetRealPID=MIN_WEATHER;                 // 12 градусов
            if (targetRealPID>MAX_WEATHER) targetRealPID=MAX_WEATHER;                 // 42 градусов
           }
           else targetRealPID=Prof.Heat.tempPID;                                                        // отключена погодозависмость
           
           errPID=((float)(targetRealPID-FEED))/100.0;                                                  // Текущая ошибка, переводим в градусы
          // Расчет отдельных компонент
           if (Prof.Heat.Ki>0)                                                                           // Расчет интегральной составляющей
           {
            temp_int=temp_int+((float)Prof.Heat.Ki*errPID)/100.0;                                        // Интегральная составляющая, с накоплением делить на 10
            #define HEAT_MAX_STEP  5                                                                     // Ограничение диапзона изменения 5 герц за один шаг ПИД
            if (temp_int>HEAT_MAX_STEP)  temp_int=HEAT_MAX_STEP; 
            if (temp_int<-1.0*HEAT_MAX_STEP)  temp_int=-1.0*HEAT_MAX_STEP; 
           }
           else temp_int=0;                                                                             // если Кi равен 0 то интегрирование не используем
           u_int=temp_int;
          
           // Дифференцальная составляющая
           u_dif=((float)Prof.Heat.Kd*(errPID-pre_errPID))/10.0;                                        // Положительная составляющая - ошибка растет (воздействие надо увеличиить)  Отрицательная составляющая - ошибка уменьшается (воздействие надо уменьшить)
           
           // Пропорциональная составляющая (десятые)
           u_pro=(float)Prof.Heat.Kp*errPID/10.0;
           
           // Общее воздействие
           u=u_pro+u_int+u_dif;
           if (u>dFC.get_PidFreqStep()/100.0) u=dFC.get_PidFreqStep()/100.0;                                    // Ограничить увеличение частоты на PidFreqStep гц
       
           newFC=100.0*u+dFC.get_targetFreq();                                                                  // Округление не нужно и добавление предудущего значения, умногжжение на 100 это перевод в 0.01 герцах
           pre_errPID=errPID;                                                                           // Сохранние ошибки, теперь это прошлая ошибка

/*           
           errPID=((float)(targetRealPID-FEED))/100.0;                                                   // Текущая ошибка, переводим в градусы
           if (Prof.Heat.Ki>0) temp_int=temp_int+abs((1/(float)Prof.Heat.Ki)*errPID);                             // Интегральная составляющая, с накоплением
           else temp_int=0;                                                                             // если Кi равен 0 то интегрирование не используем
           if (errPID<0) work_int=-1.0*temp_int; else work_int=temp_int;                                // Определение знака (перегрев меньше цели знак минус)
           // Корректировка Kp
           u=100.0*(float)Prof.Heat.Kp*(errPID+work_int+((float)Prof.Heat.Kd*(errPID-pre_errPID)/1.0))/1.0;       // Сложение всех трех компонент
           newFC=round(u)+dFC.get_targetFreq();                                                                  // Округление и добавление предудущего значения
           pre_errPID=errPID;                                                                            // Сохранние ошибки, теперь это прошлая ошибка
*/           
           if (newFC>dFC.get_maxFreq())   newFC=dFC.get_maxFreq();                                                // ограничение диапазона
           if (newFC<dFC.get_minFreq())   newFC=dFC.get_minFreq();

           // Смотрим подход к границе защит если идет УВЕЛИЧЕНИЕ частоты
           if (dFC.get_targetFreq()<newFC)                                                                                     // Идет увеличение частоты проверяем подход к границам
             {
             if ((dFC.isfOnOff())&&(FEED>(Prof.Boiler.tempIn-dFC.get_dtTemp())*dFC.get_PidStop()/100))                                                      {Status.ret=pHp17; return pCOMP_NONE;}   // Подача ограничение
             if ((dFC.isfOnOff())&&(dFC.get_power()>(FC_MAX_POWER*dFC.get_PidStop()/100)))                                                                  {Status.ret=pHp18; return pCOMP_NONE;}   // Мощность для ГВС меньшая мощность
             if ((dFC.isfOnOff())&&(dFC.get_current()>(FC_MAX_CURRENT*dFC.get_PidStop()/100)))                                                              {Status.ret=pHp19; return pCOMP_NONE;}   // ТОК для ГВС меньшая мощность
             if ((dFC.isfOnOff())&&(sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp())>(sTemp[TCOMP].get_maxTemp()*dFC.get_PidStop()/100))                       {Status.ret=pHp20; return pCOMP_NONE;}   // температура компрессора
             if ((dFC.isfOnOff())&&(sADC[PCON].get_present())&&(sADC[PCON].get_Press()>(sADC[PCON].get_maxPress()-FC_DT_CON_PRESS)*dFC.get_PidStop()/100))  {Status.ret=pHp21; return pCOMP_NONE;}   // давление конденсатора до максимальной минус 0.5 бара
             }
            //    надо менять
           if (dFC.get_targetFreq()!=newFC)                                                                     // Установкка частоты если нужно менять
             {
             journal.jprintf((char*)STR_FREQUENCY,newFC/100.0); 
             dFC.set_targetFreq(newFC,false,dFC.get_minFreq(),dFC.get_maxFreq());
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
int16_t target,t1,targetRealPID;
float u, u_dif, u_int, u_pro;       
int16_t newFC;               //Новая частота инвертора

if ((get_State()==pOFF_HP)||(get_State()==pSTOPING_HP)) return pCOMP_OFF;    // Если ТН выключен или выключается ничего не делаем

if ((rtcSAM3X8.unixtime()-offBoiler>Option.delayBoilerOff)&&((abs(FEED-RET)>Prof.Cool.dt)&&(COMPRESSOR_IS_ON)))   {set_Error(ERR_DTEMP_CON,(char*)__FUNCTION__);return pCOMP_NONE;}// Привышение разности температур кондесатора при включеноом компрессорае
#ifdef RTRV
  if ((!dRelay[RTRV].get_Relay())&&(COMPRESSOR_IS_ON))  dRelay[RTRV].set_ON();                                        // Охлаждение Проверить и если надо установить 4-ходовой клапан только если компрессор рабоатет (защита это лишнее)
#endif
 Status.ret=pNone;                                                                                   // Сбросить состояние пида
switch (Prof.Cool.Rule)   // в зависмости от алгоритма
         {
          case pHYSTERESIS:  // Гистерезис охлаждение.
               if (GETBIT(Prof.Cool.flags,fTarget)) { target=Prof.Cool.Temp2; t1=RET;}  else { target=Prof.Cool.Temp1; t1=sTemp[TIN].get_Temp();}  // вычислить темературы для сравнения Prof.Heat.Target 0-дом   1-обратка
                    if(t1<target)             {Status.ret=pCh3;   return pCOMP_OFF;}                            // Достигнута целевая температура  ВЫКЛ
               else if((rtcSAM3X8.unixtime()-offBoiler>Option.delayBoilerOff)&&(FEED<Prof.Cool.tempIn)){Status.ret=pCh1;return pCOMP_OFF;}// Достигнута минимальная температура подачи ВЫКЛ
               else if(t1>target+Prof.Cool.dTemp)  {Status.ret=pCh2;   return pCOMP_ON; }                       // Достигнут гистерезис ВКЛ
               else if(RET>Prof.Cool.tempOut)      {Status.ret=pCh13;  return pCOMP_ON; }                       // Достигнут Максимальная темература обратки ВКЛ
               else  {Status.ret=pCh4;    return pCOMP_NONE;   }                                                // Ничего не делаем  (сохраняем состояние)
              break;
          case pPID:   // ПИД регулирует подачу, а целевай функция гистререзис
            // отработка гистререзиса целевой функции (дом/обратка)
            if (GETBIT(Prof.Cool.flags,fTarget)) { target=Prof.Cool.Temp2; t1=RET;}  else { target=Prof.Cool.Temp1; t1=sTemp[TIN].get_Temp();} // вычислить темературы для сравнения Prof.Heat.Target 0-дом  1-обратка
            
             if(t1<target)                     { Status.ret=pCp3; return pCOMP_OFF;}                            // Достигнута целевая температура  ВЫКЛ
             else if ((rtcSAM3X8.unixtime()-offBoiler>Option.delayBoilerOff)&&(FEED<Prof.Cool.tempIn)) {Status.ret=pCp1; set_Error(ERR_PID_FEED,(char*)__FUNCTION__);return pCOMP_OFF;}         // Достижение минимальной температуры подачи - это ошибка ПИД не рабоатет
           //  else if ((t1<target-Prof.Cool.dTemp)&&(!(dFC.isfOnOff())))  {Status.ret=pCp2; return pCOMP_ON; }                        // Достигнут гистерезис и компрессор еще не рабоатет ВКЛ
//             else if ((t1>target+Prof.Cool.dTemp)&&(!(dFC.isfOnOff())))  {Status.ret=pCp2; return pCOMP_ON; }                          // Достигнут гистерезис (компрессор не рабоатет) ВКЛ
//             else if ((t1>target+Prof.Cool.dTemp)&&(dFC.isfOnOff())&&(dRelay[R3WAY].get_Relay())) {Status.ret=pCp2; return pCOMP_ON;}  // Достигнут гистерезис (бойлер нагрет) ВКЛ
             else if ((t1>target+Prof.Cool.dTemp)&&(!(dFC.isfOnOff())))  {Status.ret=pCp2; return pCOMP_ON; }                          // Достигнут гистерезис (компрессор не рабоатет) ВКЛ
             else if ((t1>target+Prof.Cool.dTemp)&&(dFC.isfOnOff())&&(onBoiler)) {Status.ret=pCp2; return pCOMP_ON;}  // Достигнут гистерезис (бойлер нагрет) ВКЛ
             
             // ЗАЩИТА Компресор работает, достигнута минимальная температура подачи, мощность, температура компрессора или давление то уменьшить обороты на stepFreq
              else if ((dFC.isfOnOff())&&(FEED<Prof.Cool.tempIn+dFC.get_dtTemp()))                  // Подача
             { 
              Status.ret=pCp6;
              journal.jprintf("%s %.2f (FEED: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,FEED/100.0);
              if (dFC.get_targetFreq()-dFC.get_stepFreq()<dFC.get_minFreqCool())  return pCOMP_OFF;                // Уменьшать дальше некуда, выключаем компрессор
              dFC.set_targetFreq(dFC.get_targetFreq()-dFC.get_stepFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool());  return pCOMP_NONE;               // Уменьшить частоту
             }  
  
            else if ((dFC.isfOnOff())&&(dFC.get_power()>FC_MAX_POWER))                    // Мощность
             { 
              Status.ret=pCp7;
              journal.jprintf("%s %.2f (POWER: %.2f kW)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,dFC.get_power()/10.0); // КИЛОВАТЫ
              if (dFC.get_targetFreq()-dFC.get_stepFreq()<dFC.get_minFreqCool())  return pCOMP_OFF;           // Уменьшать дальше некуда, выключаем компрессор
              dFC.set_targetFreq(dFC.get_targetFreq()-dFC.get_stepFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool());  return pCOMP_NONE;               // Уменьшить частоту
             } 
             else if ((dFC.isfOnOff())&&(dFC.get_current()>FC_MAX_CURRENT))                    // ТОК
             { 
              Status.ret=pCp16;
              journal.jprintf("%s %.2f (CURRENT: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,dFC.get_current()/100.0);
              if (dFC.get_targetFreq()-dFC.get_stepFreq()<dFC.get_minFreqCool())  return pCOMP_OFF;           // Уменьшать дальше некуда, выключаем компрессор
              dFC.set_targetFreq(dFC.get_targetFreq()-dFC.get_stepFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool());  return pCOMP_NONE;               // Уменьшить частоту
             } 
           else if ((dFC.isfOnOff())&&((sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp())>sTemp[TCOMP].get_maxTemp()))  // температура компрессора
             { 
              Status.ret=pCp8;
              journal.jprintf("%s %.2f (TCOMP: %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,sTemp[TCOMP].get_Temp()/100.0);
              if (dFC.get_targetFreq()-dFC.get_stepFreq()<dFC.get_minFreqCool())  return pCOMP_OFF;                // Уменьшать дальше некуда, выключаем компрессор
              dFC.set_targetFreq(dFC.get_targetFreq()-dFC.get_stepFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool());  return pCOMP_NONE;               // Уменьшить частоту
             } 
          else if ((dFC.isfOnOff())&&(sADC[PCON].get_present())&&(sADC[PCON].get_Press()>sADC[PCON].get_maxPress()-FC_DT_CON_PRESS))  // давление конденсатора до максимальной минус 0.5 бара
           {
            Status.ret=pCp9; 
            journal.jprintf("%s %.2f (PCON:  %.2f)\n",STR_REDUCED,dFC.get_stepFreq()/100.0,sADC[PCON].get_Press()/100.0);
            if (dFC.get_targetFreq()-dFC.get_stepFreq()<dFC.get_minFreqCool())  return pCOMP_OFF;           // Уменьшать дальше некуда, выключаем компрессор
            dFC.set_targetFreq(dFC.get_targetFreq()-dFC.get_stepFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool());  return pCOMP_NONE;               // Уменьшить частоту
           }           
           else if(!(dFC.isfOnOff())) {Status.ret=pCp5; return pCOMP_NONE;  }                                               // Если компрессор не рабоатет то ничего не делаем и выходим
           else if (rtcSAM3X8.unixtime()-dFC.get_startTime()<FC_ACCEL_TIME/100 ){ Status.ret=pCp10; return pCOMP_NONE;}     // РАЗГОН частоту не трогаем

           #ifdef SUPERBOILER                                            // Бойлер греется от предкондесатора
             if (sTemp[TCOMP].get_Temp()+SUPERBOILER_DT>sTemp[TBOILER].get_Temp())  dRelay[RSUPERBOILER].set_ON(); else dRelay[RSUPERBOILER].set_OFF();
             if(xTaskGetTickCount()/1000-updatePidTime<HP.get_timeHeat())         { Status.ret=pCp11;   return pCOMP_NONE;}   // время обновления ПИДа еше не пришло
             if (onBoiler) Status.ret=pCp15; else Status.ret=pCp12;                                          // если нужно показывем что бойлер греется от предкондесатора
           #else
              else if(xTaskGetTickCount()/1000-updatePidTime<HP.get_timeHeat())    { Status.ret=pCp11;   return pCOMP_NONE;}   // время обновления ПИДа еше не пришло
               Status.ret=pCp12;   // Дошли до сюда - ПИД на подачу. Компресор работает
           #endif      
                   
            updatePidTime=xTaskGetTickCount()/1000; 
            Serial.println("------ PID ------");
           // Уравнение ПИД регулятора в конечных разностях. ------------------------------------
           // Cp, Ci, Cd – коэффициенты дискретного ПИД регулятора;
           // u(t) = P (t) + I (t) + D (t);
           // P (t) = Kp * e (t);
           // I (t) = I (t — 1) + Ki * e (t);
           // D (t) = Kd * {e (t) — e (t — 1)};
           // T – период дискретизации(период, с которым вызывается ПИД регулятор).
           if(GETBIT(Prof.Cool.flags,fWeather))  // включена погодозависимость
           { 
            targetRealPID=Prof.Cool.tempPID-(Prof.Cool.kWeather*(TEMP_WEATHER-sTemp[TOUT].get_Temp())/1000);  // включена погодозависимость
            if (targetRealPID<Prof.Cool.tempIn+50) targetRealPID=Prof.Cool.tempIn+50;                          // ограничение целевой подачи = минимальная подача + 0.5 градуса
            if (targetRealPID<MIN_WEATHER) targetRealPID=MIN_WEATHER;                                         // границы диапазона
            if (targetRealPID>MAX_WEATHER) targetRealPID=MAX_WEATHER;                                         // 
           }
           else targetRealPID=Prof.Cool.tempPID;                                                             // отключена погодозависмость
       
           errPID=((float)(FEED-targetRealPID))/100.0;                                                // Текущая ошибка, переводим в градусы ПОДАЧА Охлаждение - ошибка на оборот
           
           // Это охлаждение
        //     errPID=((float)(targetRealPID-RET))/100.0;     // Обратка поменялась
        //     journal.jprintf("RET=%.2f\n",RET/100.0);
        //     journal.jprintf("FEED=%.2f\n",FEED/100.0);
        //     journal.jprintf("errPID=%.2f\n",errPID);
       
          // Расчет отдельных компонент
           if (Prof.Cool.Ki>0)                                                                           // Расчет интегральной составляющей
           {
            temp_int=temp_int+((float)Prof.Cool.Ki*errPID)/100.0;                                        // Интегральная составляющая, с накоплением делить на 10
            #define COOL_MAX_STEP  5                                                                     // Ограничение диапзона изменения 5 герц за один шаг ПИД
            if (temp_int>COOL_MAX_STEP)  temp_int=COOL_MAX_STEP; 
            if (temp_int<-1.0*COOL_MAX_STEP)  temp_int=-1.0*COOL_MAX_STEP; 
           }
           else temp_int=0;                                                                             // если Кi равен 0 то интегрирование не используем
           u_int=temp_int;
          
           // Дифференцальная составляющая
           u_dif=((float)Prof.Cool.Kd*(errPID-pre_errPID))/10.0;                                        // Положительная составляющая - ошибка растет (воздействие надо увеличиить)  Отрицательная составляющая - ошибка уменьшается (воздействие надо уменьшить)
           
           // Пропорциональная составляющая (десятые)
           u_pro=(float)Prof.Cool.Kp*errPID/10.0;
           
           // Общее воздействие
           u=u_pro+u_int+u_dif;
           if (u>dFC.get_PidFreqStep()/100.0) u=dFC.get_PidFreqStep()/100.0;                                    // Ограничить увеличение частоты на PidFreqStep гц
  
           newFC=100.0*u+dFC.get_targetFreq();                                                                  // Округление не нужно и добавление предудущего значения, умногжжение на 100 это перевод в 0.01 герцах
           pre_errPID=errPID;                                                                           // Сохранние ошибки, теперь это прошлая ошибка

          
           if (newFC>dFC.get_maxFreqCool())   newFC=dFC.get_maxFreqCool();                                       // ограничение диапазона
           if (newFC<dFC.get_minFreqCool())   newFC=dFC.get_minFreqCool(); // return pCOMP_OFF;                                              // Уменьшать дальше некуда, выключаем компрессор// newFC=minFreq;
      
       //    journal.jprintf("newFC=%.2f\n",newFC/100.0);
      
           // Смотрим подход к границе защит если идет УВЕЛИЧЕНИЕ частоты
           if (dFC.get_targetFreq()<newFC)                                                                                     // Идет увеличение частоты проверяем подход к границам
             {
             if ((dFC.isfOnOff())&&(FEED<((Prof.Boiler.tempIn-dFC.get_dtTemp())*dFC.get_PidStop()/100)))                                                         {Status.ret=pCp17; return pCOMP_NONE;}   // Подача ограничение
             if ((dFC.isfOnOff())&&(dFC.get_power()>(FC_MAX_POWER*dFC.get_PidStop()/100)))                                                                       {Status.ret=pCp18; return pCOMP_NONE;}   // Мощность для ГВС меньшая мощность
             if ((dFC.isfOnOff())&&(dFC.get_current()>(FC_MAX_CURRENT*dFC.get_PidStop()/100)))                                                                   {Status.ret=pCp19; return pCOMP_NONE;}   // ТОК для ГВС меньшая мощность
             if ((dFC.isfOnOff())&&(sTemp[TCOMP].get_Temp()+dFC.get_dtCompTemp()>(sTemp[TCOMP].get_maxTemp()*dFC.get_PidStop()/100)))                            {Status.ret=pCp20; return pCOMP_NONE;}   // температура компрессора
             if ((dFC.isfOnOff())&&(sADC[PCON].get_present())&&(sADC[PCON].get_Press()>(sADC[PCON].get_maxPress()-FC_DT_CON_PRESS)*dFC.get_PidStop()/100))       {Status.ret=pCp21; return pCOMP_NONE;}   // давление конденсатора до максимальной минус 0.5 бара
             }
            //    надо менять
           
           if (dFC.get_targetFreq()!=newFC)                                                                     // Установкка частоты если нужно менять
             {
             journal.jprintf((char*)STR_FREQUENCY,newFC/100.0); 
             dFC.set_targetFreq(newFC,false,dFC.get_minFreqCool(),dFC.get_maxFreqCool());
             }
           return pCOMP_NONE;                                                                            // компрессор состояние не меняет
           break;
           case pHYBRID:     
           break; 
           default: break;
         } 
 return pCOMP_NONE;
         
}
// Установить КОД состояния и вывести его в консоль о текущем состоянии + пауза t сек
void HeatPump::setStatusRet(TYPE_RET_HP ret,uint32_t t)
{
     Status.ret=ret;   // Отрабаываем паузу между включенимами
     journal.printf( "Status:%d[%s]\n",(int)Status.modWork,codeRet[Status.ret]);
     _delay(t*1000); // Выводим сообщение раз в 10 сек
}

// Концигурация 4-х, 3-х ходового крана и включение насосов, тен бойлера, тен отопления
// В зависммости от входного параметра конфигурирует краны
void HeatPump::configHP(MODE_HP conf)
{
    // 1. Обеспечение минимальной паузы компрессора, если мало времени то не конфигурируем  и срузу выходим!!
   if (!(COMPRESSOR_IS_ON))    // Только при неработающем компрессоре
   {
    #ifdef DEMO
    if (rtcSAM3X8.unixtime()-stopCompressor<10) {setStatusRet(pMinPauseOn,10);return;  }           // Обеспечение паузы компрессора Хранится в секундах!!! ТЕСТИРОВАНИЕ
    #else
        switch ((int)conf)   // Обеспечение паузы компрессора Хранится в секундах!!!
        {
            case  pCOOL:   if (rtcSAM3X8.unixtime()-stopCompressor<Prof.Cool.pause)  {setStatusRet(pMinPauseOn,10); return;}   break;  
            case  pHEAT:   if (rtcSAM3X8.unixtime()-stopCompressor<Prof.Heat.pause)  {setStatusRet(pMinPauseOn,10); return;}   break; 
            case  pBOILER: if (rtcSAM3X8.unixtime()-stopCompressor<Prof.Boiler.pause){setStatusRet(pMinPauseOn,10); return;}   break;  
            case  pOFF:    
            case  pNONE_C:
            case  pNONE_H:
            case  pNONE_B:
            default:  break;
         }     
    #endif
   }
   
     // 2. Конфигурация в нужный режим
     //#ifdef RBOILER // Если надо выключить тен бойлера (если его нет в настройках)
   	 // не будем мешать тэну...
     //  if((!GETBIT(Prof.Boiler.flags,fTurboBoiler))&&(!GETBIT(Prof.Boiler.flags,fAddHeating))) dRelay[RBOILER].set_OFF();   // ТЭН вообще не используется - Выключить ТЭН бойлера если настройки соответсвуют
     //#endif
     switch ((int)conf)
    {
      case  pOFF: // ЭТО может быть пауза! Выключить - установить положение как при включении ( перевод 4-х ходового производится после отключения компрессора (см compressorOFF()))
                 switchBoiler(false);                                            // выключить бойлер
               
                 _delay(10*1000);                        // Задержка на 10 сек
                 #ifdef SUPERBOILER                                             // Бойлер греется от предкондесатора
                     dRelay[RSUPERBOILER].set_OFF();                              // Евгений добавил выключить супербойлер
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
                 _delay(2*1000);                        // Задержка на 2 сек

                  #ifdef RPUMPFL
                  dRelay[RPUMPFL].set_ON();     // ТП
                  #endif 
                           
                 #ifdef RTRV
                  if ((COMPRESSOR_IS_ON)&&(dRelay[RTRV].get_Relay()==true)) ChangesPauseTRV();    // Компрессор рабатает и 4-х ходовой стоит на холоде то хитро переключаем 4-х ходовой в положение тепло
                 dRelay[RTRV].set_OFF();                                        // нагрев
                 _delay(2*1000);                        // Задержка на 2 сек
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
                 if (!(COMPRESSOR_IS_ON))  dFC.set_targetFreq(dFC.get_startFreq(),true,dFC.get_minFreq(),dFC.get_maxFreq());  // установить стартовую частоту если компрессор выключен
                break;    
      case  pCOOL:    // Охлаждение
                 PUMPS_ON;                                                     // включить насосы
                 _delay(2*1000);                        // Задержка на 2 сек

                 #ifdef RPUMPFL
                 dRelay[RPUMPFL].set_OFF();     // ТП
                 #endif
                
                 #ifdef RTRV
                 if ((COMPRESSOR_IS_ON)&&(dRelay[RTRV].get_Relay()==false)) ChangesPauseTRV();    // Компрессор рабатает и 4-х ходовой стоит на тепле то хитро переключаем 4-х ходовой в положение холод
                 dRelay[RTRV].set_ON();                                       // охлаждение
                 _delay(2*1000);                        // Задержка на 2 сек
                 #endif 

                  switchBoiler(false);                                           // выключить бойлер
                 #ifdef RBOILER
                  if((GETBIT(Prof.Boiler.flags,fTurboBoiler))&&(dRelay[RBOILER].get_present())) dRelay[RBOILER].set_OFF(); // Выключить ТЭН бойлера (режим форсированного нагрева)
                 #endif
                 #ifdef RHEAT
                 if (dRelay[RHEAT].get_present()) dRelay[RHEAT].set_OFF();     // Выключить ТЭН отопления
                 #endif 
                 if (!(COMPRESSOR_IS_ON))   dFC.set_targetFreq(dFC.get_startFreq(),true,dFC.get_minFreqCool(),dFC.get_maxFreqCool());   // установить стартовую частоту
                break;
       case  pBOILER:   // Бойлер
                 #ifdef SUPERBOILER                                            // Бойлер греется от предкондесатора
                    dRelay[PUMP_IN].set_ON();                                  // Реле включения насоса входного контура  (геоконтур)
                    dRelay[PUMP_OUT].set_OFF();                                // Евгений добавил
                    dRelay[RSUPERBOILER].set_ON();                             // Евгений добавил
                    _delay(2*1000);                     // Задержка на 2 сек
                    if (Status.ret<pBp5) dFC.set_targetFreq(dFC.get_startFreq(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler());      // В режиме супер бойлер установить частоту SUPERBOILER_FC если не дошли до пида
                 #else  
                    PUMPS_ON; 
                    
                    #ifdef RPUMPFL
                    dRelay[RPUMPFL].set_OFF();     // ТП
                    #endif
                    // включить насосы
                    if (Status.ret<pBp5) dFC.set_targetFreq(dFC.get_startFreqBoiler(),true,dFC.get_minFreqBoiler(),dFC.get_maxFreqBoiler());// установить стартовую частоту
                 #endif
                 _delay(2*1000);                        // Задержка на 2 сек

                 #ifdef RTRV
                 if ((COMPRESSOR_IS_ON)&&(dRelay[RTRV].get_Relay()==true)) ChangesPauseTRV();    // Компрессор рабатает и 4-х ходовой стоит на холоде то хитро переключаем 4-х ходовой в положение тепло
                 dRelay[RTRV].set_OFF();                                        // нагрев
                 _delay(2*1000);                        // Задержка на 2 сек
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
// фактически останов компрессора и обезпечени нужных пауз, компрессор включается далее в vUpdate()
void HeatPump::ChangesPauseTRV()
{
  journal.jprintf("ChangesPauseTRV\n");
  #ifdef EEV_DEF
  dEEV.Pause();                                                    // Поставить на паузу задачу Обновления ЭРВ
  journal.jprintf(" Pause task update EEV\n"); 
  #endif
  COMPRESSOR_OFF;                                                  //  Компрессор выключить
  stopCompressor=rtcSAM3X8.unixtime();                             // Запомнить время выключения компрессора
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
// Итерация по управлению всем ТН   для всего, основной цикл управления.
void HeatPump::vUpdate()
{
     #ifdef FLOW_CONTROL    // если надо проверяем потоки (защита от отказа насосов) ERR_MIN_FLOW
      uint8_t i;  
      if ((COMPRESSOR_IS_ON))                                                                                                                      // Только если компрессор включен
      for(i=0;i<FNUMBER;i++)   // Проверка потока по каждому датчику
          if (sFrequency[i].get_Value()<HP.sFrequency[i].get_minValue())   {set_Error(ERR_MIN_FLOW,(char*)sFrequency[i].get_name());  return; }    // Поток меньше минимального ошибка осанавливаем ТН
     #endif

     #ifdef SEVA  //Если определен лепестковый датчик протока - это переливная схема ТН - надо контролировать проток при работе
       if(dRelay[RPUMPI].get_Relay())                                                                                             // Только если включен насос геоконтура  (PUMP_IN)
       if (sInput[SEVA].get_Input()==SEVA_OFF)  {set_Error(ERR_SEVA_FLOW,(char*)"SEVA");  return; }                               // Выход по ошибке отсутствия протока
     #endif
     
     if((get_State()==pOFF_HP)||(get_State()==pSTARTING_HP)||(get_State()==pSTOPING_HP)) return; // ТН выключен или включается или выключается выходим  ничего не делаем!!!
  
     // Различные проверки на ошибки и защиты
      if ((Prof.SaveON.mode==pOFF)&&(!(GETBIT(Prof.SaveON.flags,fBoilerON))))          {set_Error(ERR_NO_WORK,(char*)__FUNCTION__);  return; } // Нет работы для ТН - ничего не включено, пользователь может изменить в процессе работы
      #ifdef EEV_DEF
      if ((!sADC[PEVA].get_present())&&(dEEV.get_ruleEEV()==TEVAOUT_PEVA))             {set_Error(ERR_PEVA_EEV,dEEV.get_name());     return; } //  Отсутвует датчик давления, и выбран алгоритм ЭРВ который его использует", пользователь может изменить в процессе работы
      #endif
            
      #ifdef REVI
      if (dRelay[REVI].get_present()) checkEVI() ;                           // Проверить необходимость включения ЭВИ
      #endif
      updateCount();                                                         // Обновить счетчики моточасов
      
      #ifdef DEFROST 
         defrost();                                                          // Разморозка только для воздушных ТН
      #endif
                                                                             // Собственно итерация
      {
        
          Status.modWork=get_Work();                                         // определяем что делаем
          save_DumpJournal(false);                                           // Строка состояния

         //  реализуем требуемый режим
          switch ((int)Status.modWork)
          {
              case  pOFF: if (COMPRESSOR_IS_ON){  // ЕСЛИ компрессор рабоатет то выключить компрессор,и затем сконфигурировать 3 и 4-х клапаны и включаем насосы
                             compressorOFF();
                             configHP(Status.modWork); 
                             if(!startPump)                                   // запустить задачу насос
                             {
                               startPump=true;                                 // Поставить признак запуска задачи насос
                               vTaskResume(xHandleUpdatePump);                 // Запустить задачу насос
                               journal.jprintf(" %s: Task vUpdatePump RPUMPO on . . .\n",(char*)__FUNCTION__);     // Включить задачу насос кондесатора выключение в переключении насосов
                             }
                          } break;
              case  pHEAT:   
              case  pCOOL:  
              case  pBOILER: // Включаем задачу насос, конфигурируем 3 и 4-х клапаны включаем насосы и потом включить компрессор
							 if (startPump)                                         // Остановить задачу насос
							 {
								startPump=false;                                     // Поставить признак останова задачи насос
								vTaskSuspend(xHandleUpdatePump);                     // Остановить задачу насос
								journal.jprintf(" %s: Task vUpdatePump RPUMPO pause  . . .\n",(char*)__FUNCTION__);
							 }
                             configHP(Status.modWork);                                 // Конфигурируем насосы
                             compressorON(Status.modWork);                             // Включаем компрессор
                             break;
              case  pNONE_H:
              case  pNONE_C:
              case  pNONE_B: break;                                    // компрессор уже включен
              default:  set_Error(ERR_CONFIG,(char*)__FUNCTION__); break;
           }
      } 
// Обновление расчетных величин (djpvjжность расчета определяем пографикам)
if(ChartPowerCO.get_present())   // Мощность контура в вт!!!!!!!!!
 {
  powerCO=(float)(FEED-RET)*(float)sFrequency[FLOWCON].get_Value()/sFrequency[FLOWCON].get_kfCapacity();
  #ifdef RHEAT_POWER   // Для Дмитрия. его специфика Вычитаем из общей мощности системы отопления мощность электрокотла
    #ifdef RHEAT
      if (dRelay[RHEAT].get_Relay()]) powerCO=powerCO-RHEAT_POWER;  // если включен электрокотел
    #endif    
  #endif
  } 
if(ChartCOP.get_present())     { if (dFC.get_power()>0) COP=(int16_t)(powerCO/dFC.get_power()); else COP=0;}  // в сотых долях !!!!!!
if(ChartFullCOP.get_present()) { if ((dSDM.get_Power()>0)&&(COMPRESSOR_IS_ON)) fullCOP=(int16_t)((powerCO/dSDM.get_Power())/100.0); else  fullCOP=0;} // в сотых долях !!!!!!
         
}


// Попытка включить компрессор  с учетом всех защит КОНФИГУРАЦИЯ уже установлена
// Вход режим работы ТН
// Возможно компрессор уже включен и происходит только смена режима
const char *EEV_go={" EEV go "};  // экономим место
const char *MinPauseOffCompressor={" Wait %d sec min pause off compressor . . .\n"};  // экономим место
void HeatPump::compressorON(MODE_HP mod)
{
  uint32_t nTime=rtcSAM3X8.unixtime();
  if((get_State()==pOFF_HP)||(get_State()==pSTOPING_HP)) return;  // ТН выключен или выключается выходим ничего не делаем!!!
  
  if (COMPRESSOR_IS_ON) return;                                  // Компрессор уже работает
  else                                                           // надо включать компрессор
  { 
   journal.jprintf(pP_TIME,"compressorON > modWork:%d[%s], COMPRESSOR_IS_ON:%d\n",mod,codeRet[Status.ret],COMPRESSOR_IS_ON);

    #ifdef EEV_DEF
    if (lastEEV!=-1)              // Не первое включение компрессора после старта ТН
     {
     // 1. Обеспечение минимальной паузы компрессора
     #ifdef DEMO
       if (nTime-stopCompressor<10) {journal.jprintf(MinPauseOffCompressor);return;} // Обеспечение паузы компрессора Хранится в секундах!!! ТЕСТИРОВАНИЕ
     #else
      switch ((int)mod)   // Обеспечение паузы компрессора Хранится в секундах!!!
      {
          case  pCOOL:   if (nTime-stopCompressor<Prof.Cool.pause)   {journal.jprintf(MinPauseOffCompressor,nTime-stopCompressor-Prof.Cool.pause);return;} break;  
          case  pHEAT:   if (nTime-stopCompressor<Prof.Heat.pause)   {journal.jprintf(MinPauseOffCompressor,nTime-stopCompressor-Prof.Heat.pause);return;} break; 
          case  pBOILER: if (nTime-stopCompressor<Prof.Boiler.pause) {journal.jprintf(MinPauseOffCompressor,nTime-stopCompressor-Prof.Boiler.pause);return;} break;  
          case  pOFF:    
          case  pNONE_C:
          case  pNONE_H:
          case  pNONE_B:
          default: {set_Error(ERR_CONFIG,(char*)__FUNCTION__);} break;
       }     
     #endif

      // 2. Разбираемся с ЭРВ
           journal.jprintf(EEV_go);   
           if (GETBIT(Option.flags,fEEV_light_start)) {dEEV.set_EEV(dEEV.get_preStartPos());journal.jprintf("preStartPos: %d\n",dEEV.get_preStartPos());  }    // Выйти на пусковую позицию
           else if (GETBIT(Option.flags,fEEV_start))  {dEEV.set_EEV(dEEV.get_StartPos()); journal.jprintf("StartPos: %d\n",dEEV.get_StartPos());    }    // Всегда начинать работу ЭРВ со стратовой позиции
           else                                       {dEEV.set_EEV(lastEEV);   journal.jprintf("lastEEV: %d\n",lastEEV);        }    // установка последнего значения ЭРВ
           if(GETBIT(Option.flags,fEEV_close))           // Если закрывали то пауза для выравнивания давлений
           _delay(dEEV.get_delayOn());  // Задержка на delayOn сек  для выравнивания давлений
       
     }   //  if (lastEEV!=-1)   
     else // первое включение компресора lastEEV=-1
     { // 2. Разбираемся с ЭРВ
        journal.jprintf(EEV_go);      
        if (GETBIT(Option.flags,fEEV_light_start)) { dEEV.set_EEV(dEEV.get_preStartPos());  journal.jprintf("preStartPos: %d\n",dEEV.get_preStartPos());  }      // Выйти на пусковую позицию
        else                                       { dEEV.set_EEV(dEEV.get_StartPos());   journal.jprintf("StartPos: %d\n",dEEV.get_StartPos());    }      // Всегда начинать работу ЭРВ со стратовой позиции
     }
      #endif
          
      // 3. Управление компрессором
      if (get_errcode()==OK)                                 // Компрессор включить если нет ошибок
           { 
           // Дополнительные защиты перед пуском компрессора
           if (startPump)                                      // Проверка задачи насос - должен быть выключен
              {
                startPump=false;                               // Поставить признак останова задачи насос
                vTaskSuspend(xHandleUpdatePump);               // Остановить задачу насос
                journal.jprintf(" WARNING! %s: Bad startPump, task vUpdatePump RPUMPO pause  . . .\n",(char*)__FUNCTION__);
              } 
           // Проверка включения насосов с проверкой и предупреждением (этого не должно быть)
           if (!dRelay[PUMP_IN].get_Relay()) {journal.jprintf(" WARNING! PUMP_IN is off Compressor on\n");  dRelay[PUMP_IN].set_ON(); _delay(Option.delayOnPump * 1000); }
           #ifndef SUPERBOILER  // для супербойлера это лишнее
           if (!dRelay[PUMP_OUT].get_Relay()){journal.jprintf(" WARNING! PUMP_OUT is off Compressor on\n"); dRelay[PUMP_OUT].set_ON(); _delay(Option.delayOnPump * 1000); }
           #endif
           
           #ifdef FLOW_CONTROL      // если надо проверяем потоки (защита от отказа насосов) ERR_MIN_FLOW
           for(i=0;i<FNUMBER;i++)   // Проверка потока по каждому датчику
           if (sFrequency[i].get_Value()<HP.sFrequency[i].get_minValue())   { set_Error(ERR_MIN_FLOW,(char*)sFrequency[i].get_name());  return; }    // Поток меньше минимального ошибка осанавливаем ТН
           #endif
           
           COMPRESSOR_ON;                                        // Включить компрессор
           startCompressor=rtcSAM3X8.unixtime();                 // Запомнить время включения компрессора оно используется для задержки работы ПИД ЭРВ! должно быть перед  vTaskResume(xHandleUpdateEEV) или  dEEV.Resume
           }
       else // if (get_errcode()==OK)
           {
            journal.jprintf(" There is an error, the compressor is not on\n"); 
            set_Error(ERR_COMP_ERR,(char*)__FUNCTION__);return; 
           }

    // 4. Если нужно облегченный пуск  в зависимости от флага fEEV_light_start
    #ifdef EEV_DEF
    if(GETBIT(Option.flags,fEEV_light_start))                  //  ЭРВ ОБЛЕГЧЕННЫЙ ПУСК
         {
         journal.jprintf(" Pause %d second before go starting position EEV . . .\n",dEEV.get_DelayStartPos());
         _delay(dEEV.get_DelayStartPos()*1000);  // Задержка после включения компрессора до ухода на рабочую позицию
         journal.jprintf(EEV_go);  
         if ((GETBIT(Option.flags,fEEV_start))||((lastEEV==-1)  ))  {dEEV.set_EEV(dEEV.get_StartPos()); journal.jprintf("StartPos: %d\n",dEEV.get_StartPos());    }    // если первая итерация или установлен соответсвующий флаг то на стартовую позицию
         else                                                       {dEEV.set_EEV(lastEEV);   journal.jprintf("lastEEV: %d\n",lastEEV);        }    // установка последнего значения ЭРВ в противном случае
         }
     #endif      
     // 5. Обеспечение задержки отслеживания ЭРВ
     #ifdef EEV_DEF
     if (lastEEV>0)                                            // НЕ первое включение компрессора после старта ТН
      {  
 //      journal.jprintf(" Pause %d second before enabling tracking EEV . . .\n",delayOnPid);    // Задержка внутри задачи!!!
       if (GETBIT(Option.flags,fEEV_start))  dEEV.Resume(dEEV.get_StartPos());     // Снять с паузы задачу Обновления ЭРВ  PID  со стратовой позиции
       else                                  dEEV.Resume(lastEEV);       // Снять с паузы задачу Обновления ЭРВ  PID c последнего значения ЭРВ
       journal.jprintf(" Resume task update EEV\n"); 
       journal.jprintf(pP_TIME,"%s WORK . . .\n",(char*)nameHeatPump);     // Сообщение о работе
     }
      else  // признак первой итерации
      {
      lastEEV=dEEV.get_StartPos();                                           // ЭРВ рабоатет запомнить
      set_startTime(rtcSAM3X8.unixtime());                         // Запомнить время старта ТН
      vTaskResume(xHandleUpdateEEV);                               // Запустить задачу Обновления ЭРВ
      journal.jprintf(" Start task update EEV\n");
      }
      #else
      lastEEV=1;                                                   // Признак первой итерации
      set_startTime(rtcSAM3X8.unixtime());                         // Запомнить время старта ТН
      #endif
  } // else (COMPRESSOR_IS_ON) 
             
}

// попытка выключить компрессор  с учетом всех защит
const char *MinPauseOnCompressor={" Wait min pause on compressor . . ."};  
void HeatPump::compressorOFF()
{
  if (dFC.get_present()) // Есть частотник
  {
    if (!dFC.isfOnOff()) return; // он выключен
  }
  else  if (!dRelay[RCOMP].get_Relay()) return; // Не частотник и реле компрессора выключено
   
  if((get_State()==pOFF_HP)||(get_State()==pSTARTING_HP)||(get_State()==pSTOPING_HP)) return;     // ТН выключен или включается или выключается выходим ничего не делаем!!!


 journal.jprintf(pP_TIME,"compressorOFF > modWork:%d[%s], COMPRESSOR_IS_ON:%d\n",get_mode(),codeRet[Status.ret],COMPRESSOR_IS_ON);
  #ifdef DEMO
    if (rtcSAM3X8.unixtime()-startCompressor<10)   {return;journal.jprintf(MinPauseOnCompressor);}     // Обеспечение минимального времени работы компрессора 2 минуты ТЕСТИРОВАНИЕ
  #else
    if (rtcSAM3X8.unixtime()-startCompressor<2*60) {return;journal.jprintf(MinPauseOnCompressor);}     // Обеспечение минимального времени работы компрессора 2 минуты
  #endif
  
  #ifdef EEV_DEF
  lastEEV=dEEV.get_EEV();                                             // Запомнить последнюю позицию ЭРВ
  dEEV.Pause();                                                       // Поставить на паузу задачу Обновления ЭРВ
  journal.jprintf(" Pause task update EEV\n"); 
  #endif
  
  
  COMPRESSOR_OFF;                                                     // Компрессор выключить
  stopCompressor=rtcSAM3X8.unixtime();                                // Запомнить время выключения компрессора
  
  #ifdef REVI
      checkEVI();                                                     // выключить ЭВИ
  #endif

  PUMPS_OFF;                                                          // выключить насосы + задержка
  
  #ifdef EEV_DEF
  if(GETBIT(Option.flags,fEEV_close))                                 // Hазбираемся с ЭРВ
     { 
     journal.jprintf(" Pause before closing EEV %d sec . . .\n",dEEV.get_delayOff());
     _delay(dEEV.get_delayOff()*1000);                                     // пауза перед закрытием ЭРВ  на инверторе компрессор останавливается до 2 минут
     dEEV.set_EEV(dEEV.get_minSteps());                                    // Если нужно, то закрыть ЭРВ
     journal.jprintf(" EEV go minSteps\n"); 
     } 
  #endif
  
  journal.jprintf(pP_TIME,"%s PAUSE . . .\n",(char*)nameHeatPump);    // Сообщение о паузе
}

// РАЗМОРОЗКА ВОЗДУШНИКА ----------------------------------------------------------
// Все что касается разморозки воздушника
#ifdef DEFROST
void HeatPump::defrost()
{
      if (get_State()==pOFF_HP) return;                                    // если ТН не работает то выходим
      
      #ifdef RTRV            // Нет четырехходового - нет режима охлаждения
        if(dRelay[RTRV].get_Relay()==true) return;                           // режим охлаждения - размораживать не надо
      #endif
         
      if (sInput[SFROZEN].get_Input()==SFROZEN_OFF) {startDefrost=0;return;  }    // размораживать не надо - датчик говорит что все ок
      
      // организация задержки перед включением
      if (startDefrost==0) startDefrost=xTaskGetTickCount();               // первое срабатывание датчика - запоминаем время (тики)
      if (xTaskGetTickCount()-startDefrost<delayDefrostOn*1000)  return; //  Еще рано размораживать
      // придется размораживать
       journal.jprintf("Start defrost\n"); 
       #ifdef RTRV
         if ((COMPRESSOR_IS_ON)&&(dRelay[RTRV].get_Relay()==false)) ChangesPauseTRV();    // Компрессор рабатает и 4-х ходовой стоит на тепле то хитро переключаем 4-х ходовой в положение холод
         dRelay[RTRV].set_ON();                                              // охлаждение
         _delay(2*1000);                               // Задержка на 2 сек
       #endif
       
       compressorON(pCOOL);                                                 // включить компрессор на холод
      
      while (sInput[SFROZEN].get_Input()!=SFROZEN_OFF)                     // ждем оттаивания
      {
      _delay(10*1000);                              // Задержка на 10 сек
        journal.jprintf(" Wait process defrost . . .\n"); 
        if((get_State()==pOFF_HP)||(get_State()==pSTARTING_HP)||(get_State()==pSTOPING_HP)) break;     // ТН выключен или включается или выключается выходим из разморозки
      }
      journal.jprintf(" Finish defrost, wait delayDefrostOff min.\n"); 
      _delay(delayDefrostOff*1000);               // Задержка перед выключением
      compressorOFF();                                                     // выключить компрессор
      journal.jprintf("Finish defrost\n"); 
      // выходим ТН сам определит что надо делать
}
#endif

// ОБРАБОТЧИК КОМАНД УПРАВЛЕНИЯ ТН
// Послать команду на управление ТН
void HeatPump::sendCommand(TYPE_COMMAND c)
{
  if (c==command) return;      // Игнорируем повторы
  if (command!=pEMPTY)         // Если команда выполняется (не pEMPTY), то следюющую, что послана игнорируем
     {

       journal.jprintf("Performance command: ");  
           switch(command)
            {
             case pEMPTY:     journal.jprintf("EMPTY");    break;    // Не должно быть!
             case pSTART:     journal.jprintf("START");    break;
             case pAUTOSTART: journal.jprintf("AUTOSTART");break;             
             case pSTOP:      journal.jprintf("STOP");     break;
             case pRESET:     journal.jprintf("RESET");    break;
             case pRESTART:   journal.jprintf("RESTART");  break;
             case pREPEAT:    journal.jprintf("REPEAT");   break;
             case pNETWORK:   journal.jprintf("NETWORK");  break;
             //case pJFORMAT:   journal.jprintf("JFORMAT");  break;
             case pSFORMAT:   journal.jprintf("SFORMAT");  break;                    
             case pSAVE:      journal.jprintf("SAVE");     break;                    
             case pWAIT:      journal.jprintf("WAIT");     break;                    
             case pRESUME:    journal.jprintf("RESUME");   break;                    
             
             default:         journal.jprintf("UNKNOW");   break;    // Не должно быть!
            }
       journal.jprintf(", ignore command: "); 
           switch(c)
            {
             case pEMPTY:     journal.jprintf("EMPTY\n");    break;    // Не должно быть!
             case pSTART:     journal.jprintf("START\n");    break;
             case pAUTOSTART: journal.jprintf("AUTOSTART\n");break;         
             case pSTOP:      journal.jprintf("STOP\n");     break;
             case pRESET:     journal.jprintf("RESET\n");    break;
             case pRESTART:   journal.jprintf("RESTART\n");  break;
             case pREPEAT:    journal.jprintf("REPEAT\n");   break;
             case pNETWORK:   journal.jprintf("NETWORK\n");  break; 
             //case pJFORMAT:   journal.jprintf("JFORMAT\n");  break;
             case pSFORMAT:   journal.jprintf("SFORMAT\n");  break;                    
             case pSAVE:      journal.jprintf("SAVE\n");     break;      
             case pWAIT:      journal.jprintf("WAIT\n");     break;                    
             case pRESUME:    journal.jprintf("RESUME\n");   break;                    
    
             default:         journal.jprintf("UNKNOW\n");   break;    // Не должно быть!
            }            
      
       return;  
      }
  if ((c==pSTART)&&(get_State()==pSTOPING_HP)) return;     // Пришла команда на старт а насос останавливается ничего не делаем игнорируем
  command=c;
  vTaskResume(xHandleUpdateCommand);                    // Запустить выполнение команды
}  
// Выполнить команду по управлению ТН true-команда выполнена
int8_t HeatPump::runCommand()
{
  uint16_t i;

       journal.jprintf("Run command: ");  
       switch(command)
        {
         case pEMPTY:     journal.jprintf("EMPTY\n");    break;    // Не должно быть!
         case pSTART:     journal.jprintf("START\n");    break;
         case pAUTOSTART: journal.jprintf("AUTOSTART\n");break;         
         case pSTOP:      journal.jprintf("STOP\n");     break;
         case pRESET:     journal.jprintf("RESET\n");    break;
         case pRESTART:   journal.jprintf("RESTART\n");  break;
         case pREPEAT:    journal.jprintf("REPEAT\n");   break;
         case pNETWORK:   journal.jprintf("NETWORK\n");  break;
         //case pJFORMAT:   journal.jprintf("JFORMAT\n");  break;
         case pSFORMAT:   journal.jprintf("SFORMAT\n");  break;                    
         case pSAVE:      journal.jprintf("SAVE\n");     break;                    
         case pWAIT:      journal.jprintf("WAIT\n");     break;                    
         case pRESUME:    journal.jprintf("RESUME\n");   break;        
                         
         default:         journal.jprintf("UNKNOW\n");   break;    // Не должно быть!
        }

       HP.PauseStart=true;                                // Необходимость начать задачу xHandlePauseStart с начала
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
                      journal.jprintf("$SOFTWARE RESET control . . .\r\n"); 
                      journal.jprintf("");
                      _delay(500);            // задержка что бы вывести сообщение в консоль
                      Software_Reset() ;      // Сброс
                      break;
        case pREPEAT:
                      StopWait(_stop);                                // Попытка запустит ТН (по числу пусков)
                      num_repeat++;                                  // увеличить счетчик повторов пуска ТН
                      journal.jprintf("Repeat start %s (attempts remaining %d) . . .\r\n",(char*)nameHeatPump,get_nStart()-num_repeat); 
              //        HP.PauseStart=true;                                // Необходимость начать задачу xHandlePauseStart с начала
                      vTaskResume(xHandlePauseStart);                    // Запустить выполнение отложенного старта
                      break;  
        case pRESTART:
                     // Stop();                                          // пуск Тн после сброса - есть задержка
                      journal.jprintf("Restart %s . . .\r\n",(char*)nameHeatPump);
      //              HP.PauseStart=true;                                // Необходимость начать задачу xHandlePauseStart с начала
                      vTaskResume(xHandlePauseStart);                    // Запустить выполнение отложенного старта
                      break;                 
        case pNETWORK:
                      journal.jprintf("Update network setting . . .\r\n");
                      _delay(1000);               						// задержка что бы вывести сообщение в консоль и на веб морду
                      if(SemaphoreTake(xWebThreadSemaphore,(W5200_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE) {journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexWebThreadBuzy); command=pEMPTY; return 0;} // Захват мютекса потока или ОЖИДАНИНЕ W5200_TIME_WAIT
                      initW5200(true);                                  // Инициализация сети с выводом инфы в консоль
                      for (i=0;i<W5200_THREARD;i++) SETBIT1(Socket[i].flags,fABORT_SOCK);                                 // Признак инициализации сокета, надо прерывать передачу в сервере
                      SemaphoreGive(xWebThreadSemaphore);                                                                // Мютекс потока отдать
                      break;                 
//        case pJFORMAT:                                                   // Форматировать журнал в I2C памяти
//                      #ifdef I2C_EEPROM_64KB
//                       _delay(2000);           						   // задержка что бы вывести сообщение в консоль и на веб морду
//                       journal.Format();                                 // Послать команду форматирование журнала
//                      #else                                              // Этого не может быть, но на всякий случай
//                       journal.Init();                                   // Очистить журнал в оперативке
//                      #endif
//                      break;
        case pSFORMAT:                                                   // Форматировать журнал в I2C памяти
                      #ifdef I2C_EEPROM_64KB 
                       _delay(2000);              						           // задержка что бы вывести сообщение в консоль и на веб морду
                       Stat.Format();                                    // Послать команду форматирование статистики
                      #endif 
                      break;                    
        case pSAVE:                                                      // Сохранить настройки
                      _delay(2000);              				             		 // задержка что бы вывести сообщение в консоль и на веб морду
                      save();                                            // сохранить настройки
                      break;  
        case pWAIT:   // Перевод в состяние ожидания  - особенность возможна блокировка задач - используем семафор
                      if(SemaphoreTake(xCommandSemaphore,(60*1000/portTICK_PERIOD_MS))==pdPASS)    // Cемафор  захвачен ОЖИДАНИНЕ ДА 60 сек 
                      {
                      vTaskSuspend(xHandleUpdate);                      // Перевод в состояние ожидания ТН
                      StopWait(_wait);                                  // Ожидание
                      SemaphoreGive(xCommandSemaphore);                 // Семафор отдать
                      vTaskResume(xHandleUpdate);
                      }
                      else  journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexCommandBuzy);
                      break;   
        case pRESUME:   // Восстановление работы после ожиданияя -особенность возможна блокировка задач - используем семафор
                      if(SemaphoreTake(xCommandSemaphore,(60*1000/portTICK_PERIOD_MS))==pdPASS)    // Cемафор  захвачен ОЖИДАНИНЕ ДА 60 сек
                      {
                      vTaskSuspend(xHandleUpdate);                      // Перевод в состояние ожидания ТН                                               
                      StartResume(_resume);                             // восстановление ТН
                      SemaphoreGive(xCommandSemaphore);                 // Семафор отдать
                      vTaskResume(xHandleUpdate);
                      }
                      else  journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexCommandBuzy);
                      break;                         
                                           
        default:                                                         // Не известная команда
                      journal.jprintf("Unknow command????"); 
                      break;
        }
      command=pEMPTY;   // Сбросить команду      
      return error;  
}

// --------------------------Строковые функции ----------------------------
const char *strRusPause={"Пауза"};
const char *strEngPause={"Pause"};
// Получить строку состояния ТН в виде строки
char *HeatPump::StateToStr()
{
switch ((int)get_State())  //TYPE_STATE_HP  
  {
  case pOFF_HP:     return (char*)"Выключен";  break;                     // 0 ТН выключен
  case pSTARTING_HP:return (char*)"Пуск...";   break;                     // 1 Стартует
  case pSTOPING_HP: return (char*)"Останов...";break;                     // 2 Останавливается
  case pWORK_HP:                                                          // 3 Работает
         switch ((int)get_modWork())                                      // MODE_HP
         {
         case  pOFF: return (char*)strRusPause;      break;                // 0 Пауза
         case  pHEAT: return (char*)"Нагрев+";        break;               // 1 Включить отопление
         case  pCOOL: return (char*)"Заморозка+";     break;               // 2 Включить охлаждение
         case  pBOILER: return (char*)"Нагрев ГВС+"; break;                // 3 Включить бойлер
         case  pNONE_H: if (!(COMPRESSOR_IS_ON)) return (char*)strRusPause; else return (char*)"Отопление";   break;  // 4 Продолжаем греть отопление
         case  pNONE_C: if (!(COMPRESSOR_IS_ON)) return (char*)strRusPause; else return (char*)"Охлаждение";  break;  // 5 Продолжаем охлаждение
         case  pNONE_B: if (!(COMPRESSOR_IS_ON)) return (char*)strRusPause; else return (char*)"ГВС";         break;  // 6 Продолжаем греть бойлер
         default: return (char*)"Error state";          break; 
         }
        break;   
  case pWAIT_HP:    return (char*)"Ожидание";  break;                     // 4 Ожидание
  case pERROR_HP:   return (char*)"Ошибка#";    break;                    // 5 Ошибка ТН
  default:          return (char*)"Вн.Ошибка"; break;                     // 6 - Эта ошибка возникать не должна!
  }
  
}
// Получить строку состояния ТН в виде строки АНГЛИСКИЕ буквы
char *HeatPump::StateToStrEN()
{
switch ((int)get_State())  //TYPE_STATE_HP  
  {
  case pOFF_HP:     return (char*)"Off";  break;                         // 0 ТН выключен
  case pSTARTING_HP:return (char*)"Start...";   break;                   // 1 Стартует
  case pSTOPING_HP: return (char*)"Stop...";break;                       // 2 Останавливается
  case pWORK_HP:                                                         // 3 Работает
         switch ((int)get_modWork())                                     // MODE_HP
         {
         case  pOFF: return (char*)strEngPause;        break;            // 0 Выключить
         case  pHEAT: return (char*)"Pre-heat";        break;            // 1 Включить отопление
         case  pCOOL: return (char*)"Pre-cool";        break;            // 2 Включить охлаждение
         case  pBOILER: return (char*)"Pre-boiler.";       break;        // 3 Включить бойлер
         case  pNONE_H: if (!(COMPRESSOR_IS_ON)) return (char*)strEngPause; else return (char*)"Heating";   break;                   // 4 Продолжаем греть отопление
         case  pNONE_C: if (!(COMPRESSOR_IS_ON)) return (char*)strEngPause; else return (char*)"Cooling";   break;                   // 5 Продолжаем охлаждение
         case  pNONE_B: if (!(COMPRESSOR_IS_ON)) return (char*)strEngPause; else return (char*)"Boiler";    break;                   // 6 Продолжаем греть бойлер
         default:       return (char*)"Error state";          break; 
         }
        break;   
  case pWAIT_HP:    return (char*)"Wait";    break;                     // 4 Выключить
  case pERROR_HP:   return (char*)cError;    break;                     // 5 Ошибка ТН
  default:          return (char*)"cInError";    break;                     // 6 - Эта ошибка возникать не должна!
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
  if(f)  // вывод в журнал
      {
        journal.jprintf(" modWork:%d[%s]",(int)Status.modWork,codeRet[Status.ret]); 
        if(!(dFC.get_present())) journal.printf(" RCOMP:%d",dRelay[RCOMP].get_Relay());  
        #ifdef RPUMPI
        journal.jprintf(" RPUMPI:%d",dRelay[RPUMPI].get_Relay()); 
        #endif
        journal.jprintf(" RPUMPO:%d",dRelay[RPUMPO].get_Relay());
        #ifdef RTRV  
        if (dRelay[RTRV].get_present())     journal.jprintf(" RTRV:%d",dRelay[RTRV].get_Relay());  
        #endif 
        #ifdef R3WAY 
        if (dRelay[R3WAY].get_present())    journal.jprintf(" R3WAY:%d",dRelay[R3WAY].get_Relay()); 
        #endif
        #ifdef RBOILER 
        if (dRelay[RBOILER].get_present())  journal.jprintf(" RBOILER:%d",dRelay[RBOILER].get_Relay());
        #endif
        #ifdef RHEAT
        if (HP.dRelay[RHEAT].get_present()) journal.jprintf(" RHEAT:%d",dRelay[RHEAT].get_Relay());  
        #endif
        #ifdef REVI
        if (dRelay[REVI].get_present())     journal.jprintf(" REVI:%d",dRelay[REVI].get_Relay());   
        #endif
        #ifdef RPUMPB
        if (dRelay[RPUMPB].get_present())   journal.jprintf(" RPUMPB:%d",dRelay[RPUMPB].get_Relay());   
        #endif     
        #ifdef RPUMPBH
        if (dRelay[RPUMPBH].get_present())   journal.jprintf(" RPUMPBH:%d",dRelay[RPUMPBH].get_Relay());   
        #endif     
        #ifdef RPUMPFL
        if (dRelay[RPUMPFL].get_present())   journal.jprintf(" RPUMPFL:%d",dRelay[RPUMPFL].get_Relay());   
        #endif
         
        if(dFC.get_present())               journal.jprintf(" freqFC:%.2f",dFC.get_freqFC()/100.0);  
        if(dFC.get_present())               journal.jprintf(" Power:%.2f",dFC.get_power()/10.0);  
        #ifdef EEV_DEF
        if (dEEV.get_present())             journal.jprintf(" EEV:%d",dEEV.get_EEV());
        #endif
         journal.jprintf(cStrEnd);
         // Доп инфо
        for(i=0;i<TNUMBER;i++)   // Информация по  датчикам температуры
             if (sTemp[i].get_present())      journal.jprintf(" %s:%.2f",sTemp[i].get_name(),sTemp[i].get_Temp()/100.0);
        if (sADC[PEVA].get_present())         journal.jprintf(" PEVA:%.2f",sADC[PEVA].get_Press()/100.0); 
        if (sADC[PCON].get_present())         journal.jprintf(" PCON:%.2f",sADC[PCON].get_Press()/100.0);  
        journal.jprintf(cStrEnd);
      }
   else
     {
        journal.printf(" modWork:%d[%s]",(int)Status.modWork,codeRet[Status.ret]); 
        if(!(dFC.get_present())) journal.printf(" RCOMP:%d",dRelay[RCOMP].get_Relay());  
        #ifdef RPUMPI
        journal.printf(" RPUMPI:%d",dRelay[RPUMPI].get_Relay()); 
        #endif
        journal.printf(" RPUMPO:%d",dRelay[RPUMPO].get_Relay()); 
        #ifdef RTRV 
        if (dRelay[RTRV].get_present())           journal.printf(" RTRV:%d",dRelay[RTRV].get_Relay());  
        #endif 
        #ifdef R3WAY 
        if (dRelay[R3WAY].get_present())          journal.printf(" R3WAY:%d",dRelay[R3WAY].get_Relay());  
        #endif
        #ifdef RBOILER 
        if (dRelay[RBOILER].get_present())        journal.printf(" RBOILER:%d",dRelay[RBOILER].get_Relay());
        #endif
        #ifdef RHEAT
        if (HP.dRelay[RHEAT].get_present())       journal.printf(" RHEAT:%d",dRelay[RHEAT].get_Relay());  
        #endif
        #ifdef REVI
        if (dRelay[REVI].get_present())           journal.printf(" REVI:%d",dRelay[REVI].get_Relay());   
        #endif
        #ifdef RPUMPB
        if (dRelay[RPUMPB].get_present())         journal.printf(" RPUMPB:%d",dRelay[RPUMPB].get_Relay());   
        #endif
        #ifdef RPUMPBH
        if (dRelay[RPUMPBH].get_present())        journal.printf(" RPUMPBH:%d",dRelay[RPUMPBH].get_Relay());   
        #endif 
        #ifdef RPUMPFL
        if (dRelay[RPUMPFL].get_present())        journal.printf(" RPUMPFL:%d",dRelay[RPUMPFL].get_Relay());   
        #endif
 //      Serial.print(" dEEV.stepperEEV.isBuzy():");  Serial.print(dEEV.stepperEEV.isBuzy());
 //      Serial.print(" dEEV.setZero: ");  Serial.print(dEEV.setZero);  
        if(dFC.get_present()) journal.printf(" freqFC:%.2f",dFC.get_freqFC()/100.0);  
        if(dFC.get_present()) journal.printf(" Power:%.2f",dFC.get_power()/10.0);
        #ifdef EEV_DEF
        journal.printf(" EEV:%d",dEEV.get_EEV()); 
        #endif
        journal.printf(cStrEnd);
                 
     }
  return OK;
}
#ifdef I2C_EEPROM_64KB 
// Функция вызываемая для первого часа для инициализации первичных счетчиков Только при старте ТН
void HeatPump::InitStatistics()
{
   Stat.updateCO(motoHour.P1); 
   Stat.updateEO(motoHour.E1); 
   Stat.updatemoto(motoHour.C1); 
}
    
void HeatPump::UpdateStatistics()
{
  uint32_t tt=rtcSAM3X8.unixtime();
  uint8_t h=(tt-Stat.get_dateUpdate())/(60*60);  //Получить целое число часов после последнего обновления
  if (h>0)  // прошел час и более надо обновлять
  {
   Stat.updateHour(h);
   Stat.updateTin(h*sTemp[TIN].get_Temp());
   Stat.updateTout(h*sTemp[TOUT].get_Temp());
   Stat.updateTbol(h*sTemp[TBOILER].get_Temp());
   // счетчики обновлять не надо
  }
  
  if(tt-Stat.get_date()>=24*60*60)                                   // Прошло больше суток с начала накопления надо закрывать
    {
       Stat.writeOneDay(motoHour.P1,motoHour.E1,motoHour.C1);        // записать
    }

}
#endif // I2C_EEPROM_64KB 

int16_t HeatPump::get_temp_condensing(void)
{
	if(HP.sADC[PCON].get_present()) {
		return PressToTemp(HP.sADC[PCON].get_Press(), HP.dEEV.get_typeFreon());
	} else {
		return sTemp[TCONOUTG].get_Temp() + 200; // +2C
	}
}

// пока только для режима отопления!
int16_t HeatPump::get_overcool(void)
{
	return get_temp_condensing() - HP.sTemp[TCONOUT].get_Temp();
}

// Возвращает 0 - Нет ошибок или ни одного активного датчика, 1 - ошибка, 2 - превышен предел ошибок
int8_t	 HeatPump::Prepare_Temp(uint8_t bus)
{
	int8_t i, ret = 0;
#ifdef ONEWIRE_DS2482_SECOND
	if((i = bus ? OneWireBus2.PrepareTemp() : OneWireBus.PrepareTemp())) {
#else
	if((i = OneWireBus.PrepareTemp())) {
#endif
		for(uint8_t j = 0; j < TNUMBER; j++) {
			if(sTemp[j].get_fAddress() && sTemp[j].get_bus() == bus) {
				if(HP.sTemp[j].inc_error()) {
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
