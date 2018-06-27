 /*
 * Copyright (c) 2016-2018 by vad711 (vad7@yahoo.com); Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
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
      number = sensor;
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
	if(!(GETBIT(flags, fPresent))) return OK;          // датчик запрещен в конфигурации ничего не делаем
	if(testMode!=NORMAL) lastTemp=testTemp;             // В режиме теста присвоить значение теста
	else {                                              // Чтение датчиков
#ifdef DEMO
		if (strcmp(name,"TBOILER")==0) lastTemp=4500;       // В демо бойлер всегда 45 градусов нужно для отладки
		else lastTemp=random(101,1190);                     // В демо режиме генерим значения
#else   // чтение датчика
		if(!(GETBIT(flags,fAddress))) { // Адрес не установлен
			if(number == TCOMP) { // эти датчики должны быть привязаны
				set_Error(err = ERR_ADDRESS, name);
				return err;
			} else lastTemp = testTemp; // Если датчик не привязан, то присвоить значение теста
		} else {
			int16_t ttemp;
			err = busOneWire->Read(address, ttemp);
			if(err != OK) {
				sumErrorRead++;
				if(!(err == ERR_ONEWIRE_CRC && get_setup_flag(fTEMP_ignory_CRC))) {
					if(!get_setup_flag(fTEMP_dont_log_errors)) {
						journal.jprintf(pP_TIME, "%s: Error ", name);
						if(err == ERR_ONEWIRE_CRC || err >= 0x40) { // Ошибка CRC или ошибка чтения, но успели прочитать температуру
							journal.jprintf("%s (%d). t=%.2f, prev=%.2f\n", err == ERR_ONEWIRE_CRC ? "CRC" : "read", err >= 0x40 ? err - 0x40 : err, (float)ttemp/100.0, (float)lastTemp/100.0);
						} else journal.jprintf("%s (%d)\n", err == ERR_ONEWIRE ? "RESET" : "read", err);
						//err = ERR_READ_TEMP;
					}
					if(++numErrorRead == 0) numErrorRead--;
					if(numErrorRead > NUM_READ_TEMP_ERR && !get_setup_flag(fTEMP_ignory_errors)) set_Error(err, name); // Слишком много ошибок чтения подряд - ошибка!
					return err;
				}
			}
			numErrorRead = 0; // Сброс счетчика ошибок
			//Serial.print(rtcSAM3X8.get_seconds()); Serial.print('.'); Serial.print(name); Serial.print(':'); Serial.println(ttemp);
			// Защита от скачков
			if ((lastTemp==STARTTEMP)||(abs(lastTemp-ttemp) < (get_setup_flag(fTEMP_ignory_CRC) ? GAP_TEMP_VAL_CRC : GAP_TEMP_VAL))) {
				lastTemp=ttemp; nGap=0; // Первая итерация или нет скачка Штатная ситуация
			} else { // Данные сильно отличаются от предыдущих "СКАЧЕК"
			   nGap++;
			   if (nGap > (get_setup_flag(fTEMP_ignory_CRC) ? GAP_NUMBER_CRC : GAP_NUMBER)) { // Больше максимальной длительности данные используем, счетчик сбрасываем
				   nGap = 0;
				   lastTemp = ttemp;
			   }
			   if(nGap == 0 || !get_setup_flag(fTEMP_dont_log_errors))
				   journal.jprintf(pP_TIME, "GAP %s t=%.2f, %s\n", name, (float)ttemp/100.0, nGap == 0 ? "accept" : "skip");
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
	if(Temp<minTemp) { set_Error(err = ERR_MINTEMP, name); return err; }
	if(Temp>maxTemp) { set_Error(err = ERR_MAXTEMP, name); return err; }
	// дошли до сюда значит ошибок нет
	return (err = OK);                                        // Новый цикл новые ошибки!! СБРОС ОШИБКИ
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

// установить значение систематической ошибки датчика диапазон +-MAX_TEMP_ERR не более
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
	if(get_bus() == 1) busOneWire = &OneWireBus2; 	// 2 шина
	else
#endif
#ifdef ONEWIRE_DS2482_THIRD
	if(get_bus() == 2) busOneWire = &OneWireBus3; 	// 3 шина
	else
#endif
#ifdef ONEWIRE_DS2482_FOURTH
	if(get_bus() == 3) busOneWire = &OneWireBus4; 	// 4 шина
	else
#endif
		busOneWire = &OneWireBus;					// 1 шина
}

// Установить адрес на шине датчика, bus
void sensorTemp::set_address(byte *addr, byte bus)
{
	uint8_t i;
	setup_flags &= ~fDS2482_bus_mask;
	if (addr == NULL) //сброс адреса
	{
		for(i=0;i<8;i++) address[i]=0;           // обнуление адресс датчика
		SETBIT0(flags,fAddress);                 // Поставить флаг что адрес не установлен
		return;
	}
	for (i=0;i<8;i++) address[i]=addr[i];  		   // Скопировать адрес
	SETBIT1(flags, fAddress);                      // Поставить флаг что адрес установлен, в противном случае будет возвращать ошибку
	err = OK;
	setup_flags |= bus & fDS2482_bus_mask;
	set_onewire_bus_type();
#ifndef ONEWIRE_DONT_CHG_RES
	busOneWire->SetResolution(address, DS18B20_p12BIT);
#endif
}
    
// Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, если число меньше 0 это код ошибки
void sensorTemp::after_load()
{
	if((address[0] | address[1] | address[3] | address[4] | address[5] | address[6] | address[7]) > 0) // Если адрес не 00000000 Установить адрес датчика
	{
		SETBIT1(flags, fAddress);  // Поставить флаг что адрес установлен, в противном случае будет возвращать ошибку -6
		set_onewire_bus_type();
	}
}

// Возвращает 1, если превышен предел ошибок
int8_t   sensorTemp::inc_error(void)
{
	if(++numErrorRead == 0) numErrorRead--;
	return numErrorRead > NUM_READ_TEMP_ERR;
}

// Удаленные датчики температуры ---------------------------------------------------------------------------------------
#ifdef SENSOR_IP
// Инициализация датчика
void sensorIP::initIP(uint8_t sensor)
{
    number = sensor;
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
char* sensorIP::get_sensorIP(char *var, char *ret)
{
if(strcmp(var,ip_SENSOR_TEMP)==0)     {_ftoa(ret,(float)Temp/100.0,2); return ret;     } else
if(strcmp(var,ip_SENSOR_NUMBER)==0)   {return _itoa(num,ret);                      } else  
if(strcmp(var,ip_RSSI)==0)            {_ftoa(ret,(float)RSSI/10.0,1); return ret;      } else
if(strcmp(var,ip_VCC)==0)             {_ftoa(ret,(float)VCC/1000.0,2); return ret;     } else
if(strcmp(var,ip_SENSOR_USE)==0)      {if (GETBIT(flags,fUse)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero);} else  
if(strcmp(var,ip_SENSOR_RULE)==0)     {if (GETBIT(flags,fRule)) return strcat(ret,(char*)cOne); else return  strcat(ret,(char*)cZero);} else     
if(strcmp(var,ip_SENSOR_IP)==0)       {return strcat(ret,IPAddress2String(ip));    } else  
if(strcmp(var,ip_SENSOR_COUNT)==0)    {return _itoa(count,ret);                    } else     
if(strcmp(var,ip_STIME)==0)           {if(stime==0) return strcat(ret,(char*)"none"); else return _itoa(rtcSAM3X8.unixtime()-stime,ret);} else  
if(strcmp(var,ip_SENSOR)==0)          {return strcat(ret,(char*)"----");           } else   
return strcat(ret,(char*)"E26");    
}

void sensorIP::after_load()
{
	if((link<-1)||(link >= TNUMBER)) { link=-1; num=-1; }    // если бредовое значение привязки отвязываем датчик
}

#endif  

uint8_t rs_serial_buf[128];
uint8_t rs_serial_idx = 0;
uint8_t rs_serial_flag = 0; // 0 - ждем заголовок, 1 - ждем данные
const uint8_t rs_serial_header[] = { 0x02, 'M', 'l', 0x02 };
#define rs_serial_full_header_size 7
#define rs_serial_addr_idx	5
#define rs_addr	1

unsigned short RS_SUM_CRC(unsigned char *Address, unsigned char Lenght)
{
	unsigned char N;
	unsigned short WCRC = 0;
	for(N = 1; N <= Lenght; N++, Address++) {
		WCRC += (*Address) ^ 255;
		WCRC += ((*Address) * 256);
	}
	return WCRC;
}

void RS_send_response(void)
{
	uint8_t *p = (uint8_t *)strchr((char *)rs_serial_buf + rs_serial_full_header_size, ':');
	if(p) {
		*p = '\0';
		rs_serial_buf[rs_serial_addr_idx] = rs_addr;
		p++;
		*(uint16_t *)p = RS_SUM_CRC((uint8_t *)rs_serial_buf + rs_serial_full_header_size, p - ((uint8_t *)rs_serial_buf + rs_serial_full_header_size));
		RADIO_SENSORS_SERIAL.write(rs_serial_buf, p + 2 - (uint8_t *)rs_serial_buf);
	}
}

// Новые данные в порту от радиодатчиков
#if RADIO_SENSORS_PORT == 2
void serialEvent2()
#elif RADIO_SENSORS_PORT == 3
void serialEvent3()
#endif
{
	if(rs_serial_idx < sizeof(rs_serial_buf)) {
		rs_serial_buf[rs_serial_idx++] = RADIO_SENSORS_SERIAL.read();
		if(rs_serial_flag == 0) { // ждем заголовок
			if(memcmp(rs_serial_buf, rs_serial_header, rs_serial_idx < sizeof(rs_serial_header) ? rs_serial_idx : sizeof(rs_serial_header)) == 0) {
				if(rs_serial_idx >= sizeof(rs_serial_header)) rs_serial_flag = 1;
			} else {
				rs_serial_idx = 0;
			}
		} else if(rs_serial_flag == 1) { // ждем данные
			uint8_t len = rs_serial_buf[rs_serial_full_header_size-1];
			if(rs_serial_idx >= rs_serial_full_header_size && rs_serial_idx >= rs_serial_full_header_size + len + 2) {
				if(RS_SUM_CRC(rs_serial_buf + rs_serial_full_header_size, len) != *(uint16_t *)(rs_serial_buf + rs_serial_full_header_size + len)) {
					journal.jprintf("RS CRC error!\n");
				} else {
					rs_serial_buf[rs_serial_full_header_size + len] = '\0';
					journal.jprintf("RS:%s\n", rs_serial_buf + rs_serial_full_header_size);
					if(rs_serial_buf[rs_serial_full_header_size + 1] == '#') {
						uint8_t c = rs_serial_buf[rs_serial_full_header_size + 2];
						if(c == 'I') { // Присутствие

						} else if(c == 'D') { // Данные

						}
						RS_send_response();
					}
				}
				rs_serial_idx = 0;
			}
			if(rs_serial_idx >= sizeof(rs_serial_buf)) rs_serial_idx = 0;  // кривые данные
		}
	}
}

