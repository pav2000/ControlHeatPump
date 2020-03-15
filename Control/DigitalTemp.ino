 /*
 * Copyright (c) 2016-2020 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
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
// ------------------------------------------------------------------------------------------
// Цифровые датчики температуры -------------------------------------------------------------
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
      memset(address, 0, sizeof(address));	   // обнуление адресс датчика
      busOneWire = NULL;
      testMode = NORMAL;                         // Значение режима тестирования
#if T_NUMSAMLES > 1
      memset(t, 0, sizeof(t));	   			   // обнуление буффера значений
      sum=0;
      last=0;
#endif
      nGap=0;                                  // Счечик "разорванных" данных  - требуется для фильтрации помехи
      note=(char*)noteTemp[sensor];            // присвоить наименование датчика
      name=(char*)nameTemp[sensor];            // присвоить имя датчика

     // Удаленные устройства  #ifdef SENSOR_IP
     #ifdef SENSOR_IP
      devIP=NULL;                             // Ссылка на привязаный датчик (класс) если NULL привявязки нет
     #endif
 }

#ifdef TNTC
int16_t sensorTemp::Read_NTC(uint16_t val)
{
	uint8_t r = sizeof(NTC_table) / sizeof(NTC_table[0]) - 1;
	uint8_t l = 0;
	while((r - l) > 1) {
		uint8_t m = (l + r) >> 1;
		if(val > NTC_table[m]) r = m; else l = m;
	}
	uint16_t vl = NTC_table[l];
	int16_t Temp;
	if(val > vl) Temp = l * TEMP_TABLE_STEP + TEMP_TABLE_START - TEMP_TABLE_STEP;
	else {
		uint16_t vr = NTC_table[r];
		if(val < vr) Temp = r * TEMP_TABLE_STEP + TEMP_TABLE_START + TEMP_TABLE_STEP;
		else {
			uint16_t vd = vl - vr;
			int16_t res = TEMP_TABLE_START + r * TEMP_TABLE_STEP;
			if(vd) res -= ((TEMP_TABLE_STEP * (int32_t) (val - vr) + (vd >> 1)) / vd);
			Temp = res;
		}
	}
	return Temp;
}
#endif

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
			} else if(number == TIN) return OK; // Этот может получать значения от других датчиков
			else lastTemp = testTemp; // Если датчик не привязан, то присвоить значение теста
		} else {
#ifdef RADIO_SENSORS
			if(*address == tRadio) {
				err = OK;
				int8_t i = get_radio_received_idx();
				if(i >= 0) {
					lastTemp = radio_received[i].Temp;
					if(radio_timecnt - radio_received[i].timecnt > RADIO_LOST_TIMEOUT/TIME_READ_SENSOR) radio_received[i].RSSI = 255;
				} else return err;
			} else
#endif
#ifdef TNTC
			if(*address == tADC) {
				lastTemp = Read_NTC(TNTC_Value[address[1] - '0']);
			} else
#endif
#ifdef TNTC_EXT
			if(*address == tADS1115) {
				//...
			} else
#endif
			{
				int16_t ttemp;
				err = busOneWire->Read(address, ttemp);
				if(err != OK) {
					sumErrorRead++;
					if(!(err == ERR_ONEWIRE_CRC && get_setup_flag(fTEMP_ignory_CRC))) {
						if(!get_setup_flag(fTEMP_dont_log_errors)) {
							journal.jprintf_time("%s: Error ", name);
							if(err == ERR_ONEWIRE_CRC || err >= 0x40) { // Ошибка CRC или ошибка чтения, но успели прочитать температуру
								journal.jprintf("%s (%d). t=%.2d, prev=%.2d\n", err == ERR_ONEWIRE_CRC ? "CRC" : "read", err >= 0x40 ? err - 0x40 : err, ttemp, lastTemp);
							} else journal.jprintf("%s (%d)\n", err == ERR_ONEWIRE ? "RESET" : "read", err);
							//err = ERR_READ_TEMP;
						}
						if(++numErrorRead == 0) numErrorRead--;
						if(numErrorRead > NUM_READ_TEMP_ERR && !get_setup_flag(fTEMP_ignory_errors)) set_Error(err, name); // Слишком много ошибок чтения подряд - ошибка!
						return err;
					}
				}
				numErrorRead = 0; // Сброс счетчика ошибок
				//SerialDbg.print(rtcSAM3X8.get_seconds()); SerialDbg.print('.'); SerialDbg.print(name); SerialDbg.print(':'); SerialDbg.println(ttemp);
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
					   journal.jprintf_time("GAP %s t=%.2d, %s\n", name, ttemp, nGap == 0 ? "accept" : "skip");
				}
			}
		}
#endif
	}

	// Усреднение значений
  #if T_NUMSAMLES == 1         // При 1 - без усреднения
	Temp = lastTemp + errTemp;
  #else                        // буфер может быть не полным
	sum = sum-t[last];           // Убрать самое старое значение из суммы
	t[last] = lastTemp;          // Запомить новое значение
	sum = sum + lastTemp;          // Добавить новое значение

	if (last<(T_NUMSAMLES-1)) last++; else { last=0; SETBIT1(flags,fFull);}  // Установить признак буфер полный при T_NUMSAMLES=1 буфер всегда полный (придупреждение компилятора)
	if(GETBIT(flags,fFull))   Temp=sum/T_NUMSAMLES+errTemp;   // буфер полный
	else                      Temp=sum/last+errTemp;
  #endif

	// Проверка на ошибки именно здесь обрабатывются ошибки и передаются на верх
	if(Temp<minTemp) { set_Error(err = ERR_MINTEMP, name); return err; }
	if(Temp>maxTemp) { set_Error(err = ERR_MAXTEMP, name); return err; }
	// дошли до сюда значит ошибок нет
	return (err = OK);                                        // Новый цикл новые ошибки!! СБРОС ОШИБКИ
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
  else return Temp;                                     // датчик не привязан
 #else                                                  // При отсутствии удаленныхдатчиков сразу выводим проводной датчик
   return Temp;
 #endif
}

// Получить значение температуры ПРОВОДНОГО датчика
int16_t sensorTemp::get_rawTemp()
{
	return Temp;
}

// Установить значение температуры датчика в режиме теста
int8_t sensorTemp::set_testTemp(int16_t t)            
{
 if((t>=minTemp)&&(t<=maxTemp)) { testTemp=t; return OK;} else return WARNING_VALUE;
}

// Шина
uint8_t sensorTemp::get_bus()
{
	return (*address < tRadio ? setup_flags & fDS2482_bus_mask : *address == tRadio ? tRadio_Bus : *address == tADC ? tADC_Bus: tADS1115_Bus );
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
	if(get_bus() == 0) busOneWire = &OneWireBus;	// 1 шина
}

// Установить адрес на шине датчика, bus
void sensorTemp::set_address(byte *addr, byte bus)
{
	uint8_t i;
	err = OK;
	setup_flags &= ~fDS2482_bus_mask;
	if(addr == NULL) // сброс адреса
	{
		memset(address, 0, sizeof(address));	   // обнуление адресс датчика
		SETBIT0(flags, fAddress);                  // Поставить флаг, что адрес не установлен
		return;
	}
	for(i = 0; i < sizeof(address); i++) address[i] = addr[i];   // Скопировать адрес
	setup_flags |= bus & fDS2482_bus_mask;
	after_load();
}
    
// Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, если число меньше 0 это код ошибки
void sensorTemp::after_load()
{
	if(address[0] || address[1]) // Если адрес не пустой то Установить адрес датчика
	{
		SETBIT1(flags, fAddress);  // Поставить флаг что адрес установлен
		if(HP.Option.ver <= 137) {
			if(address[0] == 0x03) address[0] = tRadio;
		}
		if(address[0] >= tRadio) {
			busOneWire = NULL;
		} else set_onewire_bus_type();
	}
}

// Возвращает 1, если превышен предел ошибок
int8_t sensorTemp::inc_error(void)
{
	if(++numErrorRead == 0) numErrorRead--;
	return numErrorRead > NUM_READ_TEMP_ERR;
}

int8_t sensorTemp::get_radio_received_idx()
{
#ifdef RADIO_SENSORS
	for(uint8_t i = 0; i < radio_received_num; i++) if(memcmp(&radio_received[i].serial_num, &address[1], sizeof(radio_received[0].serial_num)) == 0) {
		return i;
	}
#endif
	return -1;
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

#ifdef RADIO_SENSORS

uint8_t rs_serial_buf[96];
uint8_t rs_serial_idx = 0;
const uint8_t rs_serial_header[] = { 0x02, 'M', 'l', 0x02 }; // <addr to><addr from><Len><' '><'#'><cmd>
#define rs_serial_full_header_size 7
#define rs_serial_addr_idx	5
#define rs_addr	0x05

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

uint8_t get_next_byte_from_string(char **ptr)
{
	if(*ptr != NULL) {
		char *p = strchr(*ptr, ' ');
		if(p) {
			*p = '\0';
			uint8_t ret = atoi(*ptr);
			*ptr = p + 1;
			return ret;
		}
	}
	*ptr = NULL;
	return 0;
}


void radio_transmit(void)
{
	RADIO_SENSORS_SERIAL._pUart->UART_CR = US_CR_RXDIS; // Disables USART RX
	RADIO_SENSORS_SERIAL.write(rs_serial_buf, rs_serial_idx);
	// Пустой буфер: SerialDbg.availableForWrite() == SERIAL_BUFFER_SIZE - 1
	while(RADIO_SENSORS_SERIAL.availableForWrite() < SERIAL_BUFFER_SIZE - 1 || (RADIO_SENSORS_SERIAL._pUart->UART_SR & (UART_SR_TXEMPTY | UART_SR_TXRDY)) != (UART_SR_TXEMPTY | UART_SR_TXRDY))
		_delay(1); // Ждем отправки
	RADIO_SENSORS_SERIAL._pUart->UART_CR =  US_CR_RXEN; // Enables USART RX
	#ifdef DEBUG_RADIO
	if(GETBIT(HP.Option.flags, fLogWirelessSensors)) journal.jprintf("RA=%s\n", rs_serial_buf + rs_serial_full_header_size);
	#endif
	rs_serial_idx = 0;
	rs_serial_flag = RS_WAIT_HEADER;
}

// Новые данные в порту от радиодатчиков, вызывать с паузой
void check_radio_sensors(void)
{
	if(rs_serial_flag == RS_SEND_RESPONSE) {
		radio_transmit();
		return;
	}
	while(RADIO_SENSORS_SERIAL.available())
	{
		rs_serial_buf[rs_serial_idx++] = RADIO_SENSORS_SERIAL.read();
		if(rs_serial_flag == RS_WAIT_HEADER) {
			if(memcmp(rs_serial_buf, rs_serial_header, rs_serial_idx < sizeof(rs_serial_header) ? rs_serial_idx : sizeof(rs_serial_header)) == 0) {
				if(rs_serial_idx >= sizeof(rs_serial_header)) rs_serial_flag = RS_WAIT_DATA;
			} else {
				rs_serial_idx = 0;
			}
		}
		if(rs_serial_flag == RS_WAIT_DATA) {
			uint8_t len;
			if(rs_serial_idx >= rs_serial_full_header_size && rs_serial_idx >= rs_serial_full_header_size + (len = rs_serial_buf[rs_serial_full_header_size-1]) + 2) {
				if(RS_SUM_CRC(rs_serial_buf + sizeof(rs_serial_header), len + rs_serial_full_header_size - sizeof(rs_serial_header)) != *(uint16_t *)(rs_serial_buf + rs_serial_full_header_size + len)) {
					rs_serial_buf[rs_serial_full_header_size + len] = '\0';
					if(GETBIT(HP.Option.flags, fLogWirelessSensors)) journal.jprintf("RS CRC error=%s\n", rs_serial_buf + rs_serial_full_header_size);
					rs_serial_flag = RS_WAIT_HEADER;
				} else {
					rs_serial_buf[rs_serial_full_header_size + len] = '\0';
					//#ifdef DEBUG_RADIO
					if(GETBIT(HP.Option.flags, fLogWirelessSensors)) journal.jprintf("RS=%s\n", rs_serial_buf + rs_serial_full_header_size);
					//#endif
					if(rs_serial_buf[rs_serial_full_header_size + 1] == '#') {
						uint8_t c = rs_serial_buf[rs_serial_full_header_size + 2];
						char *pdata = strchr((char *)&rs_serial_buf[rs_serial_full_header_size + 2], ':');
						if((c >= 'a' && c <= 'z')) { // echo - skip
							rs_serial_idx = 0;
							return;
						}
						if(pdata) {
							if(c == 'A') { //    // Новый датчик ждем в течении 2-х минуты после старта НК (нажать кнопку датчика пока не загорится его светодиод)
								if(HP.get_uptime() < 120) {
									journal.jprintf("New radio sensor: %s\n", (char *)rs_serial_buf + rs_serial_full_header_size);
									pdata = strchr(pdata, ' ');
									if(pdata) {
										*(pdata+1) = '1'; // Разрешение регистрации
										*(pdata+2) = '\0';
									}
								}
							} else {
								*pdata = '\0';
								pdata++;
								if(c == 'I') { // Присутствие
									if(pdata) {
										radio_hub_serial = atoi((char *)&rs_serial_buf[rs_serial_full_header_size + 3]);
										if(radio_hub_serial) journal.jprintf("Radio module found: #%d\n", radio_hub_serial);
									}
								} else if(c == 'D') { // Данные
									char *p = pdata;
									if(p) p = strchr(p, 'R');
									if(p) {
										char *p2 = strchr(p, ' ');
										if(p2) {
											*p2 = '\0';
											uint32_t ser = atoi(p+1);
											p = p2 + 1;
											uint8_t i = 0;
											for(; i < radio_received_num; i++) if(radio_received[i].serial_num == ser) break;
											if(i < RADIO_SENSORS_MAX && ser) {
												if(i == radio_received_num) { // new
													memset(&radio_received[i], 0, sizeof(radio_received[0]));
													radio_received_num++;
												}
												radio_received[i].serial_num = ser;
												while(p) {
													c = get_next_byte_from_string(&p);
													if(p == NULL || c == 0) break;
													if(c == 0xC0) { // Температура 2b
														radio_received[i].Temp = (get_next_byte_from_string(&p) + get_next_byte_from_string(&p) * 256 - 2730) * 10;
														radio_received[i].timecnt = radio_timecnt;
													} else if(c == 0xB4) { // Питание 1b
														radio_received[i].battery = ((uint32_t) get_next_byte_from_string(&p) * 125 * 3 / 128 + 5) / 10;
													} else if(c == 0xBF) { // Test 1b
														get_next_byte_from_string(&p);
													} else if(c == 0xB0) { // RSSI 1b
														c = get_next_byte_from_string(&p);
														radio_received[i].RSSI = abs((c >= 128 ? ((int16_t)c - 256) : (int16_t)c) / 2 - 73);
													} else if(c >= 0x80 && c <= 0x8F) { // состояние батареи 0b
													}
												}
											}
										}
									}
								}
							}
						}
						// Send response
						rs_serial_buf[rs_serial_addr_idx] = rs_addr;
						rs_serial_buf[rs_serial_full_header_size + 2] |= 0x20; // В нижний регистр cmd
						uint8_t len = strlen((char *)rs_serial_buf + rs_serial_full_header_size) + 1;
						rs_serial_buf[rs_serial_full_header_size - 1] = len;
						*(uint16_t *)pdata = RS_SUM_CRC((uint8_t *)rs_serial_buf + sizeof(rs_serial_header), len + rs_serial_full_header_size - sizeof(rs_serial_header));
						rs_serial_idx = pdata + 2 - (char *)rs_serial_buf;
						rs_serial_flag = RS_SEND_RESPONSE;
						return;
					}
				}
				rs_serial_idx = 0;
			}
			if(rs_serial_idx >= sizeof(rs_serial_buf)) rs_serial_idx = 0;  // кривые данные
		}
	}
}

void radio_sensor_send(char *cmd)
{
	journal.jprintf("Radio cmd: %s", cmd);
	while(rs_serial_idx && rs_serial_idx) _delay(1); // Ждем принятия сообщения
	memcpy(rs_serial_buf, rs_serial_header, sizeof(rs_serial_header));
	rs_serial_buf[sizeof(rs_serial_header)] = 0; // адрес приёмника 1 байт (0 – широковещат кадр)
	rs_serial_buf[sizeof(rs_serial_header) + 1] = rs_addr; // адрес источника 1 байт
	uint8_t len = strlen(cmd) + 2; // +sizeof(" \0")
	rs_serial_buf[sizeof(rs_serial_header) + 2] = len; // длина данных
	rs_serial_buf[sizeof(rs_serial_header) + 3] = ' '; // Тип текст
	strcpy((char *)&rs_serial_buf[sizeof(rs_serial_header) + 4], cmd);
	len += 3; // sizeof(Adr_dest + Adr_source + Len_data)
	*(uint16_t *)(rs_serial_buf + sizeof(rs_serial_header) + len) = RS_SUM_CRC((uint8_t *)rs_serial_buf + sizeof(rs_serial_header), len);
	rs_serial_idx = sizeof(rs_serial_header) + len + 2;
	radio_transmit();
}

char Radio_RSSI_to_Level(uint8_t RSSI)
{
	return RSSI == 255 ? '-' : '0' + (RSSI>100?0:(100-RSSI)/7); // 0..9
}

#endif
