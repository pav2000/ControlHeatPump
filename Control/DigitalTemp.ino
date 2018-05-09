 /*
 * Copyright (c) 2016-2018 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav,
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
// ------------------------------------------------------------------------------------------
// Цифровые датчики температуры -------------------------------------------------------------
#ifndef OVERRIDE_TNUMBERS   // Если определено, берется из Config.h
// Имена датчиков
const char *nameTemp[] = {"TOUT",             // Температура улицы
                          "TIN",              // Температура в доме
                          "TEVAIN",           // Температура на входе испарителя по фреону
                          "TEVAOUT",          // Температура на выходе испарителя по фреону
                          "TCONIN",           // Температура на входе конденсатора по фреону
                          "TCONOUT",          // Температура на выходе конденсатора по фреону
                          "TBOILER",          // Температура в бойлере ГВС
                          "TACCUM",           // Температура на выходе теплоаккмулятора
                          "TRTOOUT",          // Температура на выходе RTO (по фреону)
                          "TCOMP",            // Температура нагнетания компрессора
                          "TEVAING",          // Температура на входе испарителя по гликолю
                          "TEVAOUTG",         // Температура на выходе испарителя по гликолю
                          "TCONING",          // Температура на входе конденсатора по гликолю
                          "TCONOUTG"          // Температура на выходе конденсатора по гликолю
                           };           

// Описание датчиков
const char *noteTemp[] = {"Температура улицы",
                          "Температура в доме",
                          "Температура на входе испарителя по фреону",
                          "Температура на выходе испарителя по фреону",
                          "Температура на входе конденсатора по фреону",
                          "Температура на выходе конденсатора по фреону",
                          "Температура в бойлере ГВС",
                          "Температура на выходе теплоаккумулятора",
                          "Температура на выходе РТО (по фреону)",
                          "Температура нагнетания компрессора",
                          "Температура на входе испарителя по гликолю",
                          "Температура на выходе испарителя по гликолю",
                          "Температура на входе конденсатора по гликолю",
                          "Температура на выходе конденсатора по гликолю"
                          };
#endif
                         
 // Инициализация на входе номер датчика
void sensorTemp::initTemp(int sensor)
    { 
      err=OK;                                  // ошибка датчика (работа)
      numErrorRead=0;                          // Ошибок нет
      sumErrorRead=0;                          // ошибок нет
      minTemp=MINTEMP[sensor];                 // минимальная разрешенная температура
      maxTemp=MAXTEMP[sensor];                 // максимальная разрешенная температура
      errTemp=ERRTEMP[sensor];                 // статическая ошибка датчика
      testTemp=TESTTEMP[sensor];               // Значение при тестировании
      lastTemp=STARTTEMP;                      // последняя считанная температура с датчика по умолчанию абсолютный ноль
      Temp=0;                                  // температура датчика (обработанная)
      flags=0x00;                              // Сбросить все флаги

      if(SENSORTEMP[sensor]) SETBIT1(flags, fPresent); // наличие датчика в текушей конфигурации
      SETBIT0(flags,fFull);                    // буфер не полный
      Chart.init(SENSORTEMP[sensor]);          // инициалазация статистики
      memset(address, 0, sizeof(address));	   // обнуление адресс датчика
      memset(t, 0, sizeof(t));	   			   // обнуление буффера значений
      busOneWire = NULL;
      testMode=NORMAL;                         // Значение режима тестирования
      sum=0;
      last=0;
      nGap=0;                                  // Счечик "разорванных" данных  - требуется для фильтрации помехи
      note=(char*)noteTemp[sensor];            // присвоить наименование датчика
      name=(char*)nameTemp[sensor];            // присвоить имя датчика

     // Удаленные устройства  #ifdef SENSOR_IP
     #ifdef SENSOR_IP
      devIP=NULL;                             // Ссылка на привязаный датчик (класс) если NULL привявязки нет
     #endif
 }

// Чтение датчиков температуры, возвращает код ошибки, делает все преобразования
int8_t sensorTemp::Read() 
{  
	if(!(GETBIT(flags, fPresent))) return err;          // датчик запрещен в конфигурации ничего не делаем
	if(testMode!=NORMAL) lastTemp=testTemp;             // В режиме теста присвоить значение теста
	else                                                 // Чтение датчиков
	{
#ifdef DEMO
		if (strcmp(name,"TBOILER")==0) lastTemp=4500;       // В демо бойлер всегда 45 градусов нужно для отладки
		else lastTemp=random(101,1190);                     // В демо режиме генерим значения
#else   // чтение датчика
		if(!(GETBIT(flags,fAddress))) { // Адрес не установлен
			err = ERR_ADDRESS; set_Error(err,name);
			return err;
		}
		int16_t ttemp;
		err = busOneWire->Read(address, ttemp);
		if(err) {
            journal.jprintf(" %s: Error %s (%d)\n", name, err == ERR_ONEWIRE ? "RESET" : err == ERR_ONEWIRE_CRC ? "CRC" : "read", err);
            numErrorRead++;
            sumErrorRead++;
            err = ERR_READ_TEMP;
            if(numErrorRead > NUM_READ_TEMP_ERR) set_Error(err, name); // Слишком много ошибок чтения подряд - ошибка!
            return err;
		} else {
			numErrorRead = 0; // Сброс счетчика ошибок
			//Serial.print(rtcSAM3X8.get_seconds()); Serial.print('.'); Serial.print(name); Serial.print(':'); Serial.println(ttemp);
			// Защита от скачков
			if ((lastTemp==STARTTEMP)||(abs(lastTemp-ttemp)<GAP_TEMP_VAL)) {
				lastTemp=ttemp; nGap=0; // Первая итерация или нет скачка Штатная ситуация
			} else { // Данные сильно отличаются от предыдущих "СКАЧЕК"
			   nGap++;
			   if (nGap>GAP_NUMBER) { // Больше максимальной длительности данные используем, счетчик сбрасываем
				   nGap=0;
				   lastTemp=ttemp;
			   } else {  // Пропуск данных
				   journal.jprintf("WARNING: Gap DS1820: %s t=%.2f, skip\r\n",name,(float)ttemp/100.0);
			   }
			}
		}
#endif
	}

	// Усреднение значений
	sum = sum-t[last];           // Убрать самое старое значение из суммы
	t[last] = lastTemp;          // Запомить новое значение
	sum = sum + lastTemp;          // Добавить новое значение

	#if T_NUMSAMLES == 1         // При объеме буфера 1 - буфер всегда полный
	last = 0; SETBIT1(flags,fFull);
	#else                        // буфер может быть не полным
	if (last<(T_NUMSAMLES-1)) last++; else { last=0; SETBIT1(flags,fFull);}  // Установить признак буфер полный при T_NUMSAMLES=1 буфер всегда полный (придупреждение компилятора)
	#endif

	// Serial.print("sum="); Serial.print(sum);Serial.print(" lastTemp="); Serial.print(lastTemp); Serial.print(" last="); Serial.print(last);  Serial.print(" fFull="); Serial.println(GETBIT(fFull));

	if(GETBIT(flags,fFull))   Temp=sum/T_NUMSAMLES+errTemp;   // буфер полный
	else                      Temp=sum/last+errTemp;

	// Проверка на ошибки именно здесь обрабатывются ошибки и передаются на верх
	if(Temp<minTemp) { err=ERR_MINTEMP; set_Error(err, name); return err; }
	if(Temp>maxTemp) { err=ERR_MAXTEMP; set_Error(err, name); return err; }
	// дошли до сюда значит ошибок нет
	err = OK;                                        // Новый цикл новые ошибки!! СБРОС ОШИБКИ
	return err;
}

// полный цикл получения данных возвращает значение температуры, только тестирование!! никакие переменные класса не трогает!!
int16_t sensorTemp::Test() 
{  
#ifdef DEMO
	return random(-3000,3000);      // Если демо вернуть случайное число
#else
	int16_t ttemp = STARTTEMP;
	busOneWire->Read(address, ttemp);
	return ttemp;
#endif
} 

// установить значение систематической ошибки датчика диапазон +10 -10 не более
int8_t sensorTemp::set_errTemp(int16_t t)     
{
  if (abs(t)<MAX_TEMP_ERR) { errTemp=t; return OK;} else return WARNING_VALUE;
}

// Получить значение температуры датчика (n.nn) с учетом удаленных датчиков!!! - это то что используется в работе ТН
int16_t sensorTemp::get_Temp()            
{
 #ifdef SENSOR_IP                                       // использутся удаленные датчики
 int16_t t;
 if (!(GETBIT(flags,fPresent))) return 0;               // проводной датчик запрещен в конфигурации ничего не делаем
 if( (devIP->get_fUse())&&(devIP->get_link()>-1))       // Удаленный датчик привязан к данному проводному датчику надо использовать
   {
    if (devIP->get_update()>UPDATE_IP)  return Temp;    // Время просрочено, удаленный датчик не используем
    t=devIP->get_Temp();                                // Получить значение температуры удаленного датчика
    if ((devIP->get_fRule())) return (t+Temp)/2;          // Усреднение датчиков
    else return t;                                      // Использование только удаленного датчика
   }
  else  return Temp;                                    // датчик не привязан
 #else                                                  // При отсутствии удаленныхдатчиков сразу выводим проводной датчик
   if (!(GETBIT(flags,fPresent))) return 0;             // проводной датчик запрещен в конфигурации ничего не делаем
   return Temp;  
 #endif
}

// Получить значение температуры ПРОВОДНОГО датчика
int16_t sensorTemp::get_rawTemp()
{
 if (!(GETBIT(flags,fPresent))) return 0;                // датчик запрещен в конфигурации ничего не делаем
 return Temp;  
}

// Установить значение температуры датчика в режиме теста
int8_t sensorTemp::set_testTemp(int16_t t)            
{
 if((t>=minTemp)&&(t<=maxTemp)) { testTemp=t; return OK;} else return WARNING_VALUE;
}

void sensorTemp::set_onewire_bus_type()
{
	// Привязка шины к датчику
#ifdef ONEWIRE_DS2482_SECOND
	if(GETBIT(setup_flags, fDS2482_second)) busOneWire = &OneWireBus2; 	// второй
	else
#endif
		busOneWire = &OneWireBus;		                   				// первый
}

// Установить адрес на шине датчикаbus_type
void sensorTemp::set_address(byte *addr, byte bus_type)
{
	uint8_t i;
	setup_flags &= ~(1<<fDS2482_second);
	if (addr == NULL) //сброс адреса
	{
		for(i=0;i<8;i++) address[i]=0;           // обнуление адресс датчика
		SETBIT0(flags,fAddress);                 // Поставить флаг что адрес не установлен
		return;
	}
	for (i=0;i<8;i++) address[i]=addr[i];  		   // Скопировать адрес
	SETBIT1(flags, fAddress);                      // Поставить флаг что адрес установлен, в противном случае будет возвращать ошибку
	err = 0;
	if(bus_type) setup_flags |= (1<<fDS2482_second);
	set_onewire_bus_type();
	busOneWire->SetResolution(address, DS18B20_p12BIT);
}
    
// Записать настройки в eeprom i2c на входе адрес с какого, на выходе конечный адрес, если число меньше 0 это код ошибки
int32_t sensorTemp::save(int32_t adr)
{
	if(writeEEPROM_I2C(adr, (byte*)&setup_flags, sizeof(setup_flags)))
		{ set_Error(ERR_SAVE_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(setup_flags);     // записать поправку датчитка и флаг шины
	if(writeEEPROM_I2C(adr, (byte*)&errTemp, sizeof(errTemp)))
		{ set_Error(ERR_SAVE_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(errTemp);     // записать поправку датчитка и флаг шины
	if(writeEEPROM_I2C(adr, (byte*)&testTemp, sizeof(testTemp)))
		{ set_Error(ERR_SAVE_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(testTemp);    // записать температуру датчика в режиме тестирования
	if(writeEEPROM_I2C(adr, (byte*)address, sizeof(address)))
		{ set_Error(ERR_SAVE_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(address);     // записать адрес датчика
	return adr;
}

// Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, если число меньше 0 это код ошибки
int32_t sensorTemp::load(int32_t adr)
{
	if(readEEPROM_I2C(adr, (byte*)&setup_flags, sizeof(setup_flags)))
    	{ set_Error(ERR_LOAD_EEPROM,name); return ERR_LOAD_EEPROM; } adr=adr+sizeof(setup_flags);     // прочитать поправку датчитка и флаг шины
	if(readEEPROM_I2C(adr, (byte*)&errTemp, sizeof(errTemp)))
    	{ set_Error(ERR_LOAD_EEPROM,name); return ERR_LOAD_EEPROM; } adr=adr+sizeof(errTemp);     // прочитать поправку датчитка и флаг шины
    if(readEEPROM_I2C(adr, (byte*)&testTemp, sizeof(testTemp)))
    	{ set_Error(ERR_LOAD_EEPROM,name); return ERR_LOAD_EEPROM; } adr=adr+sizeof(testTemp);    // прочитать температуру датчика в режиме тестирования
    if(readEEPROM_I2C(adr, (byte*)address, sizeof(address)))
    	{ set_Error(ERR_LOAD_EEPROM,name); return ERR_LOAD_EEPROM; } adr=adr+sizeof(address);     // прочитать адрес датчика
    if((address[0]|address[1]|address[3]|address[4]|address[5]|address[6]|address[7])>0)  // Если адрес не 00000000 Установить адрес датчика
    {
		SETBIT1(flags, fAddress);                           // Поставить флаг что адрес установлен, в противном случае будет возвращать ошибку -6
		set_onewire_bus_type();
    }
    return adr;
}
// Считать настройки из буфера на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t sensorTemp::loadFromBuf(int32_t adr,byte *buf)
{
	setup_flags = buf[adr++];
    memcpy((byte*)&errTemp,buf+adr,sizeof(errTemp)); adr=adr+sizeof(errTemp);     // прочитать поправку датчитка и флаг шины
    memcpy((byte*)&testTemp,buf+adr,sizeof(testTemp)); adr=adr+sizeof(testTemp);  // прочитать температуру датчика в режиме тестирования
    memcpy((byte*)&address,buf+adr,sizeof(address)); adr=adr+sizeof(address);      // прочитать адрес датчика
    if((address[0]|address[1]|address[3]|address[4]|address[5]|address[6]|address[7])>0)  // Если адрес не 00000000 Установить адрес датчика
    {
		SETBIT1(flags,fAddress);                           // Поставить флаг что адрес установлен, в противном случае будет возвращать ошибку -6
		set_onewire_bus_type();
    }
    return adr;  
}

 // Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
uint16_t sensorTemp::get_crc16(uint16_t crc)
{
	crc=_crc16(crc, setup_flags);
	crc=_crc16(crc,lowByte(errTemp));  crc=_crc16(crc,highByte(errTemp));   // поправка датчитка
	crc=_crc16(crc,lowByte(testTemp)); crc=_crc16(crc,highByte(testTemp));  // температура датчика в режиме тестирования
	for(uint8_t i=0; i<8; i++) crc=_crc16(crc,address[i]);
	return crc;
}

// Возвращает 1, если превышен предел ошибок
int8_t   sensorTemp::inc_error(void)
{
	if(++numErrorRead <= 0) numErrorRead--;
	return numErrorRead > NUM_READ_TEMP_ERR;
}

// Удаленные датчики температуры ---------------------------------------------------------------------------------------
#ifdef SENSOR_IP
// Инициализация датчика
void sensorIP::initIP()
{
    Temp=-12345;                 // Последние показания датчика
    num=-1;                      // Номер датчика, по нему осуществляется идентификация датачика о 0 до MAX_SENSOR_IP-1
    RSSI=-321;                   // Уровень сигнала датчика (-дБ)
    VCC=0;                       // Уровень напряжениея питания датчика (мВ)
    count=0;                     // Кольцевой счетчик пакетов с момента включения контрола
    stime=0;                     // Время получения последнего пакета
    flags=0;                     // флаги
    link=-1;                     // датчик не привязан
}

// Запомнить данные (обновление данных)
boolean sensorIP::set_DataIP(int16_t a,int16_t b,int16_t c,int16_t d,uint32_t e, IPAddress f)
{
num=a;                         // Номер датчика -1  датчик отсутсвует
Temp=b;                        // Последние показания датчика
RSSI=c;                        // Уровень сигнала датчика (-дБ)
VCC=d;                         // Уровень напряжениея питания датчика (мВ)
count=e;                       // Кольцевой счетчик пакетов с момента включения контрола
stime=rtcSAM3X8.unixtime();    // Время получения последнего пакета
ip=f;   
return true;      
}

// Получить параметр в виде строки
char* sensorIP::get_sensorIP(TYPE_SENSOR_IP  p)
{
  char static temp[12];
  switch (p)
   {
    case pSENSOR_TEMP:          return ftoa(temp,(float)Temp/100.0,2);       break;  
    case pSENSOR_NUMBER:        return int2str(num);                         break;  
    case pRSSI:                 return ftoa(temp,(float)RSSI/10.0,1);        break;                
    case pVCC:                  return ftoa(temp,(float)VCC/1000.0,2);       break;
    case pSENSOR_USE:           if (GETBIT(flags,fUse)) return (char*)cOne; else    return  (char*)cZero;  break;  
    case pSENSOR_RULE:          if (GETBIT(flags,fRule)) return (char*)cOne; else   return  (char*)cZero;  break;     
    case pSENSOR_IP:            return IPAddress2String(ip);                 break;  
    case pSENSOR_COUNT:         return int2str(count);                       break;     
    case pSTIME:                if(stime==0) return (char*)"none"; else return  int2str(rtcSAM3X8.unixtime()-stime);  break;  
    default:                    return (char*)cInvalid;                     break;   
   }
 return (char*)cInvalid;    
}

// Записать настройки в eeprom i2c на входе адрес с какого, на выходе конечный адрес, если число меньше 0 это код ошибки
int32_t sensorIP::save(int32_t adr)
{
if (writeEEPROM_I2C(adr, (byte*)&flags, sizeof(flags)))    { set_Error(ERR_SAVE_EEPROM,IPAddress2String(ip)); return ERR_SAVE_EEPROM; } adr=adr+sizeof(flags);   // записать флаги удаленного датчика
if (writeEEPROM_I2C(adr, (byte*)&link, sizeof(link)))      { set_Error(ERR_SAVE_EEPROM,IPAddress2String(ip)); return ERR_SAVE_EEPROM; } adr=adr+sizeof(link);    // записать привязку удаленного датчика
return adr;   
}

// Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, если число меньше 0 это код ошибки
int32_t sensorIP::load(int32_t adr)
{
if (readEEPROM_I2C(adr, (byte*)&flags, sizeof(flags)))     { set_Error(ERR_LOAD_EEPROM,IPAddress2String(ip)); return ERR_LOAD_EEPROM; } adr=adr+sizeof(flags);    // прочитать флаги удаленного датчика
if (readEEPROM_I2C(adr, (byte*)&link, sizeof(link)))       { set_Error(ERR_LOAD_EEPROM,IPAddress2String(ip)); return ERR_LOAD_EEPROM; } adr=adr+sizeof(link);     // прочитать привязку удаленного датчика
if ((link<-1)||(link>=TNUMBER)) { link=-1; num=-1;}    // если бредовое значение привязки отвязываем датчик
return adr;
}
// Считать настройки из буфера на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t sensorIP::loadFromBuf(int32_t adr,byte *buf)
{
  memcpy((byte*)&flags,buf+adr,sizeof(flags));  adr=adr+sizeof(flags);    // прочитать флаги удаленного датчика
  memcpy((byte*)&link,buf+adr,sizeof(link));    adr=adr+sizeof(link);     // прочитать привязку удаленного датчика
  if ((link<-1)||(link>=TNUMBER)) { link=-1; num=-1;}                     // если бредовое значение привязки отвязываем датчик
  return adr;  
}
// Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
uint16_t sensorIP::get_crc16(uint16_t crc)
{
  crc=_crc16(crc,flags);
  crc=_crc16(crc,link);
  return crc;              
}

#endif  
