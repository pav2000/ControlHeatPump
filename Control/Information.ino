/*
 * Copyright (c) 2016-2019 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
 * &                       by Vadim Kulakov vad7@yahoo.com, vad711
 *
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

//  описание вспомогательных Kлассов данных, предназначенных для получения информации о ТН
#include "Information.h"

#define bufI2C Socket[0].outBuf

// --------------------------------------------------------------------------------------------------------------- 
//  Класс системный журнал пишет в консоль и в память ------------------------------------------------------------
//  Место размещения (озу ли флеш) определяется дефайном #define I2C_EEPROM_64KB
// --------------------------------------------------------------------------------------------------------------- 
// Инициализация
void Journal::Init()
{
	bufferTail = 0;
	bufferHead = 0;
	full = false;                   // Буфер не полный
	err = OK;
#ifdef DEBUG
#ifndef DEBUG_NATIVE_USB
	SerialDbg.begin(UART_SPEED);                   // Если надо инициализировать отладочный порт
#endif
#endif

#ifndef I2C_EEPROM_64KB     // журнал в памяти
	memset(_data, 0, JOURNAL_LEN);
	jprintf("\nSTART ----------------------\n");
	jprintf("Init RAM journal, size %d . . .\n", JOURNAL_LEN);
	return;
#else                      // журнал во флеше

	uint8_t eepStatus=0;
	uint16_t i;
	char *ptr;

	if ((eepStatus=eepromI2C.begin(I2C_SPEED)!=0))  // Инициализация памяти
	{
#ifdef DEBUG
		SerialDbg.println("$ERROR - open I2C journal, check I2C chip!");   // ошибка открытия чипа
#endif
		err=ERR_OPEN_I2C_JOURNAL;
		return;
	}

	if (checkREADY()==false) // Проверка наличия журнал
	{
#ifdef DEBUG
		SerialDbg.print("I2C journal not found! ");
#endif
		Format(bufI2C);
	}

	for (i=0;i<JOURNAL_LEN/W5200_MAX_LEN;i++)   // поиск журнала начала и конца, для ускорения читаем по W5200_MAX_LEN байт
	{
		WDT_Restart(WDT);
#ifdef DEBUG
		SerialDbg.print(".");
#endif
		if (readEEPROM_I2C(I2C_JOURNAL_START+i*W5200_MAX_LEN, (byte*)&bufI2C,W5200_MAX_LEN))
		{	err=ERR_READ_I2C_JOURNAL;
#ifdef DEBUG
		SerialDbg.print(errorReadI2C);
#endif
		break;};
		if ((ptr=(char*)memchr(bufI2C,I2C_JOURNAL_HEAD,W5200_MAX_LEN))!=NULL) {bufferHead=i*W5200_MAX_LEN+(ptr-bufI2C);}
		if ((ptr=(char*)memchr(bufI2C,I2C_JOURNAL_TAIL,W5200_MAX_LEN))!=NULL) {bufferTail=i*W5200_MAX_LEN+(ptr-bufI2C);}
		if ((bufferTail!=0)&&(bufferHead!=0)) break;
	}
	if (bufferTail<bufferHead) full=true;                   // Буфер полный
	jprintf("\nSTART ----------------------\n");
	jprintf("Found I2C journal: size %d bytes, head=0x%x, tail=0x%x\n",JOURNAL_LEN,bufferHead,bufferTail);
#endif //  #ifndef I2C_EEPROM_64KB     // журнал в памяти
}

  
#ifdef I2C_EEPROM_64KB  // функции долько для I2C журнала
// Записать признак "форматирования" журнала - журналом можно пользоваться
void Journal::writeREADY()
{  
    uint16_t  w=I2C_JOURNAL_READY; 
    if (writeEEPROM_I2C(I2C_JOURNAL_START-2, (byte*)&w,sizeof(w))) 
       { err=ERR_WRITE_I2C_JOURNAL; 
         #ifdef DEBUG
         SerialDbg.println(errorWriteI2C);
         #endif
        }
}
// Проверить наличие журнала
boolean Journal::checkREADY()
{  
    uint16_t  w=0x0; 
    if (readEEPROM_I2C(I2C_JOURNAL_START-2, (byte*)&w,sizeof(w))) 
       { err=ERR_READ_I2C_JOURNAL; 
         #ifdef DEBUG
         SerialDbg.print(errorReadI2C);
         #endif
        }
    if (w!=I2C_JOURNAL_READY) return false; else return true;
}

// Форматирование журнала (инициализация I2C памяти уже проведена), sizeof(buf)=W5200_MAX_LEN
void Journal::Format(char *buf)
{
	uint16_t i;
	err = OK;
	memset(buf, I2C_JOURNAL_FORMAT, W5200_MAX_LEN);
	#ifdef DEBUG
	SerialDbg.print("Formating I2C journal ");
	#endif
	for(i = 0; i < JOURNAL_LEN / W5200_MAX_LEN; i++) {
		#ifdef DEBUG
		SerialDbg.print("*");
		#endif
		if(i == 0) {
			buf[0] = I2C_JOURNAL_HEAD;
			buf[1] = I2C_JOURNAL_TAIL;
		} else {
			buf[0] = I2C_JOURNAL_FORMAT;
			buf[1] = I2C_JOURNAL_FORMAT;
		}
		if(writeEEPROM_I2C(I2C_JOURNAL_START + i * W5200_MAX_LEN, (byte*)&buf, W5200_MAX_LEN)) {
			err = ERR_WRITE_I2C_JOURNAL;
			#ifdef DEBUG
			SerialDbg.println(errorWriteI2C);
			#endif
			break;
		};
		WDT_Restart(WDT);
	}
	full = 0;                   // Буфер не полный
	bufferHead = 0;
	bufferTail = 1;
	if(err == OK) {
		writeREADY();                 // было форматирование
		jprintf("\nFormat I2C journal (size %d bytes) - Ok\n", JOURNAL_LEN);
	}
}
#endif
    
// Печать только в консоль
void Journal::printf(const char *format, ...)             
{
#ifdef DEBUG
	va_list ap;
	va_start(ap, format);
	m_vsnprintf(pbuf, PRINTF_BUF, format, ap);
	va_end(ap);
	SerialDbg.print(pbuf);
#endif
}

// Печать в консоль и журнал возвращает число записанных байт
void Journal::jprintf(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	m_vsnprintf(pbuf, PRINTF_BUF, format, ap);
	va_end(ap);
#ifdef DEBUG
	SerialDbg.print(pbuf);
#endif
	// добавить строку в журнал
	_write(pbuf);
}

//type_promt вставляется в начале журнала а далее печать в консоль и журнал возвращает число записанных байт с типом промта
void Journal::jprintf(type_promt pr,const char *format, ...)
{
	switch (pr)
	{
	case  pP_NONE: break;
	case  pP_TIME: jprintf((char*)"%s ",NowTimeToStr()); break;                       // время
	case  pP_DATE: jprintf((char*)"%s %s ",NowDateToStr(),NowTimeToStr()); break;     // дата и время
	case  pP_USER: jprintf((char*)promtUser); break;                                  // константа определяемая пользователем
	}
	va_list ap;
	va_start(ap, format);
	m_vsnprintf(pbuf, PRINTF_BUF, format, ap);
	va_end(ap);
#ifdef DEBUG
	SerialDbg.print(pbuf);
#endif
	_write(pbuf);   // добавить строку в журнал
}   

// Печать ТОЛЬКО в журнал возвращает число записанных байт для использования в критических секциях кода
void Journal::jprintf_only(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	m_vsnprintf(pbuf, PRINTF_BUF, format, ap);
	va_end(ap);
	_write(pbuf);
}

// отдать журнал в сеть клиенту  Возвращает число записанных байт
int32_t Journal::send_Data(uint8_t thread)
{
	int32_t num, len, sum = 0;
#ifdef I2C_EEPROM_64KB // чтение еепром
	num = bufferHead + 1;                     // Начинаем с начала журнала, num позиция в буфере пропуская символ начала
	for(uint16_t i = 0; i < (JOURNAL_LEN / W5200_MAX_LEN + 1); i++) // Передаем пакетами по W5200_MAX_LEN байт, может быть два неполных пакета!!
	{
		__asm__ volatile ("" ::: "memory");
		if((num > bufferTail))                                        // Текущая позиция больше хвоста (начало передачи)
		{
			if(JOURNAL_LEN - num >= W5200_MAX_LEN) len = W5200_MAX_LEN;
			else len = JOURNAL_LEN - num;   // Контроль достижения границы буфера
		} else {                                                        // Текущая позиция меньше хвоста (конец передачи)
			if(bufferTail - num >= W5200_MAX_LEN) len = W5200_MAX_LEN;
			else len = bufferTail - num;     // Контроль достижения хвоста журнала
		}
		if(readEEPROM_I2C(I2C_JOURNAL_START + num, (byte*) Socket[thread].outBuf, len))         // чтение из памяти
		{
			err = ERR_READ_I2C_JOURNAL;
#ifdef DEBUG
			SerialDbg.print(errorReadI2C);
#endif
			return 0;
		}
		if(sendPacketRTOS(thread, (byte*) Socket[thread].outBuf, len, 0) == 0) return 0;        // передать пакет, при ошибке выйти
		_delay(2);
		sum = sum + len;                                                                        // сколько байт передано
		if(sum >= available()) break;                                                           // Все передано уходим
		num = num + len;                                                                        // Указатель на переданные данные
		if(num >= JOURNAL_LEN) num = 0;                                                         // переходим на начало
	}  // for
#else
	num=bufferHead;                                                   // Начинаем с начала журнала, num позиция в буфере
	for(uint16_t i=0;i<(JOURNAL_LEN/W5200_MAX_LEN+1);i++)// Передаем пакетами по W5200_MAX_LEN байт, может быть два неполных пакета!!
	{
		if((num>bufferTail))                              // Текущая позиция больше хвоста (начало передачи)
		{
			if (JOURNAL_LEN-num>=W5200_MAX_LEN) len=W5200_MAX_LEN; else len=JOURNAL_LEN-num; // Контроль достижения границы буфера
		} else {                                                           // Текущая позиция меньше хвоста (конец передачи)
			if (bufferTail-num>=W5200_MAX_LEN) len=W5200_MAX_LEN; else len=bufferTail-num; // Контроль достижения хвоста журнала
		}
		if(sendPacketRTOS(thread,(byte*)_data+num,len,0)==0) return 0;          // передать пакет, при ошибке выйти
		_delay(2);
		sum=sum+len;// сколько байт передано
		if (sum>=available()) break;// Все передано уходим
		num=num+len;// Указатель на переданные данные
		if (num>=JOURNAL_LEN) num=0;// переходим на начало
	}  // for
#endif
	return sum;
}

// Возвращает размер журнала
int32_t Journal::available(void)
{ 
  #ifdef I2C_EEPROM_64KB
    if (full) return JOURNAL_LEN; else return bufferTail-1;
  #else   
     if (full) return JOURNAL_LEN; else return bufferTail;
  #endif
}    
                 
// чтобы print рабоtал для это класса
size_t Journal::write (uint8_t c)
  {
  SerialDbg.print(char(c));
  return 1;   // one byte output
  }  // end of myOutputtingClass::write
         
// Записать строку в журнал
void Journal::_write(char *dataPtr)
{
	int32_t numBytes;
	if(dataPtr == NULL || (numBytes = strlen(dataPtr)) == 0) return;  // Записывать нечего
#ifdef I2C_EEPROM_64KB // запись в еепром
	if(numBytes > JOURNAL_LEN - 2) numBytes = JOURNAL_LEN - 2; // Ограничиваем размером журнала JOURNAL_LEN не забываем про два служебных символа
	// Запись в I2C память
	if(SemaphoreTake(xI2CSemaphore, I2C_TIME_WAIT / portTICK_PERIOD_MS) == pdFALSE) {  // Если шедулер запущен то захватываем семафор
		journal.printf((char*) cErrorMutex, __FUNCTION__, MutexI2CBuzy);
		return;
	}
	__asm__ volatile ("" ::: "memory");
	dataPtr[numBytes] = I2C_JOURNAL_TAIL;
	if(full) dataPtr[numBytes + 1] = I2C_JOURNAL_HEAD;
	if(bufferTail + numBytes + 2 > JOURNAL_LEN) { //  Запись в два приема если число записываемых бит больше чем место от конца очереди до конца буфера ( помним про символ начала)
		int32_t n;
		if(eepromI2C.write(I2C_JOURNAL_START + bufferTail, (byte*) dataPtr, n = JOURNAL_LEN - bufferTail)) {
			#ifdef DEBUG
				if(err != ERR_WRITE_I2C_JOURNAL) SerialDbg.print(errorWriteI2C);
			#endif
			err = ERR_WRITE_I2C_JOURNAL;
		} else {
			dataPtr += n;
			numBytes -= n;
			full = 1;
			if(eepromI2C.write(I2C_JOURNAL_START, (byte*) dataPtr, numBytes + 2)) {
				err = ERR_WRITE_I2C_JOURNAL;
				#ifdef DEBUG
					SerialDbg.print(errorWriteI2C);
				#endif
			} else {
				bufferTail = numBytes;
				bufferHead = bufferTail + 1;
				err = OK;
			}
		}
	} else {  // Запись в один прием Буфер не полный
		if(eepromI2C.write(I2C_JOURNAL_START + bufferTail, (byte*) dataPtr, numBytes + 1 + full)) {
			#ifdef DEBUG
				if(err != ERR_WRITE_I2C_JOURNAL) SerialDbg.print(errorWriteI2C);
			#endif
			err = ERR_WRITE_I2C_JOURNAL;
		} else {
			bufferTail += numBytes;
			if(full) bufferHead = bufferTail + 1;
		}
	}
	SemaphoreGive(xI2CSemaphore);
#else   // Запись в память
	// SerialDbg.print(">"); SerialDbg.print(numBytes); SerialDbg.println("<");

	if( numBytes >= JOURNAL_LEN ) numBytes = JOURNAL_LEN;// Ограничиваем размером журнала
	// Запись в журнал
	if(numBytes > JOURNAL_LEN - bufferTail)//  Запись в два приема если число записываемых бит больше чем место от конца очереди до конца буфера
	{
		int len = JOURNAL_LEN - bufferTail;             // сколько можно записать в конец
		memcpy(_data+bufferTail,dataPtr,len);// Пишем с конца очереди но до конца журнала
		memcpy(_data, dataPtr+len, numBytes-len);// Пишем в конец буфера с начала
		bufferTail = numBytes-len;// Хвост начинает рости с начала буфера
		bufferHead=bufferTail +1;// Буфер полный по этому начало стоит сразу за концом (затирание данных)
		full=true;// буфер полный
	} else   // Запись в один прием Буфер
	{
		memcpy(_data+bufferTail, dataPtr, numBytes);     // Пишем с конца очереди
		bufferTail = bufferTail + numBytes;// Хвост вырос
		if (full) bufferHead=bufferTail+1;// голова изменяется только при полном буфере (затирание данных)
		else bufferHead=0;
	}
#endif
}

    
// ---------------------------------------------------------------------------------
//  Класс ГРАФИКИ    ------------------------------------------------------------
// ---------------------------------------------------------------------------------

 // Инициализация
void statChart::init(boolean pres)
{  
  err=OK; 
  present=pres;                                 // наличие статистики - зависит от конфигурации
  pos=0;                                        // текущая позиция для записи
  num=0;                                        // число накопленных точек
  flagFULL=false;                               // false в буфере менее CHART_POINT точек
  if (pres)                                     // отводим память если используем статистику
  { 
    data=(int16_t*)malloc(sizeof(int16_t)*CHART_POINT);
    if (data==NULL) {err=ERR_OUT_OF_MEMORY; set_Error(err,(char*)__FUNCTION__);return;}  // ОШИБКА если память не выделена
    for(int i=0;i<CHART_POINT;i++) data[i]=0;     // обнуление
  }  
}

// Очистить статистику
void statChart::clear()
{   
  pos=0;                                        // текущая позиция для записи
  num=0;                                        // число накопленных точек
  flagFULL=false;                               // false в буфере менее CHART_POINT точек
  for(int i=0;i<CHART_POINT;i++) data[i]=0;     // обнуление
}

 // добавить точку в массиве
void statChart::addPoint(int16_t y)
{
 if (!present) return; 
 data[pos]=y;
 if (pos<CHART_POINT-1) pos++; else { pos=0; flagFULL=true; }
 if (!flagFULL) num++ ;   // буфер пока не полный
}

// получить точку нумерация 0-самая новая CHART_POINT-1 - самая старая, (работает кольцевой буфер)
inline int16_t statChart::get_Point(uint16_t x)
{
 if (!present) return 0; 
 if (!flagFULL) return data[x];
 else 
 {
    if ((pos+x)<CHART_POINT) return data[pos+x];else return data[pos+x-CHART_POINT];
 }
}

// БИНАРНЫЕ данные по маске: получить точку нумерация 0-самая старая CHART_POINT - самая новая, (работает кольцевой буфер)
boolean statChart::get_boolPoint(uint16_t x,uint16_t mask)  
{ 
 if (!present) return 0; 
 if (!flagFULL) return data[x]&mask?true:false;
 else 
 {
    if ((pos+x)<CHART_POINT) return data[pos+x]&mask?true:false; 
    else                     return data[pos+x-CHART_POINT]&mask?true:false;
 }
}

// получить строку в которой перечислены все точки в строковом виде через; при этом значения делятся на m
// строка не обнуляется перед записью
void statChart::get_PointsStr(uint16_t m, char *&b)
{ 
  if ((!present)||(num==0)) {
	  //strcat(b, ";");
	  return;
  }
  b += m_strlen(b);
  for(uint16_t i = 0; i < num; i++) {
    b += _ftoa(b, (float)get_Point(i)/m, 2);
    *b++ = ';'; *b = '\0';
  }
}

void statChart::get_PointsStrSub(uint16_t m, char *&b, statChart *sChart)
{
  if (!present || num == 0 || !sChart->get_present() || sChart->get_num() == 0) {
	  //strcat(b, ";");
	  return;
  }
  b += m_strlen(b);
  for(uint16_t i = 0; i < num; i++) {
    b += _ftoa(b, (float)(get_Point(i) - sChart->get_Point(i))/m, 2);
    *b++ = ';'; *b = '\0';
  }
}
// Расчитать мощность на лету используется для графика потока, передаются указатели на графики температуры + теплоемкость
void statChart::get_PointsStrPower(uint16_t m, char *&b, statChart *inChart,statChart *outChart, float kfCapacity)
{
  if (!present || num == 0 || !inChart->get_present() || inChart->get_num()==0 || !outChart->get_present() || outChart->get_num()== 0) {
	  //strcat(b, ";");
	  return;
  }
  b += m_strlen(b);
  for(uint16_t i = 0; i < num; i++) {
    b += _ftoa(b, float(abs(outChart->get_Point(i)-inChart->get_Point(i))*get_Point(i))/kfCapacity/m, 2);
    *b++ = ';'; *b = '\0';
  }
}

// ---------------------------------------------------------------------------------
//  Класс Профиль ТН    ------------------------------------------------------------
// ---------------------------------------------------------------------------------
// Класс предназначен для работы с настройками ТН. в пямяти хранится текущий профиль, ав еепром можно хранить до 10 профилей, и загружать их по одному в пямять
// также есть функции по сохраннию и удалению из еепром
// номер текущего профиля хранится в структуре type_SaveON
// инициализация профиля
void Profile::initProfile()
{
  err=OK;
  magic=0xaa;
  crc16=0;
  strcpy(dataProfile.name,"unknow");
  strcpy(dataProfile.note,"default profile");
  dataProfile.flags=0x00;
  dataProfile.len=get_sizeProfile();
  dataProfile.id=0;
  
  // Состояние ТН структура SaveON
  SaveON.magic=0x55;                   // признак данных, должно быть  0x55
  SETBIT0(SaveON.flags,fBoilerON);     // Бойлер выключен
  SaveON.startTime=0;                  // нет времени включения
  SaveON.mode=pOFF;                    // выключено
  
  // Охлаждение
  Cool.Rule=pHYSTERESIS,               // алгоритм гистерезис, интервальный режим
  Cool.Temp1=2000;                     // Целевая температура дома
  Cool.Temp2=1200;                     // Целевая температура Обратки
  SETBIT0(Cool.flags,fTarget);         // Что является целью ПИД - значения true (температура в доме), false (температура обратки).
  SETBIT0(Cool.flags,fWeather);        // флаг Погодозависмости
  Cool.dTemp=200;                      // Гистерезис целевой температуры
  Cool.dTempDay=200;                   // Гистерезис целевой температуры дневной тариф
  Cool.pid_time=90;                    // Постоянная интегрирования времени в секундах ПИД ТН
  Cool.pid.Kp=1;                      // Пропорциональная составляющая ПИД ТН
  Cool.pid.Ki=0;                       // Интегральная составляющая ПИД ТН
  Cool.pid.Kd=3;                       // Дифференциальная составляющая ПИД ТН
  Cool.tempPID=2200;                // Целевая температура ПИД
 
 // Защиты
  Cool.tempIn=1000;                    // Tемпература подачи (минимальная)
  Cool.tempOut=3500;                   // Tемпература обратки (макс)
  Cool.dt=1500;                        // Максимальная разность температур конденсатора.
  Cool.kWeather=10;                    // Коэффициент погодозависимости в СОТЫХ градуса на градус
  
 // Cool.P1=0;
  
// Отопление
  Heat.Rule=pPID,              		 // алгоритм гистерезис, интервальный режим
  Heat.Temp1=2200;                     // Целевая температура дома
  Heat.Temp2=3500;                     // Целевая температура Обратки
  SETBIT1(Heat.flags,fTarget);         // Что является целью ПИД - значения true (температура в доме), false (температура обратки).
  SETBIT1(Heat.flags,fWeather);        // флаг Погодозависмости
  Heat.dTemp=050;                      // Гистерезис целевой температуры
  Heat.dTempDay=100;                   // Гистерезис целевой температуры дневной тариф
  Heat.pid_time=60;                    // Постоянная интегрирования времени в секундах ПИД ТН
  Heat.pid.Kp=100;                      // Пропорциональная составляющая ПИД ТН
  Heat.pid.Ki=480;                       // Интегральная составляющая ПИД ТН
  Heat.pid.Kd=10;                       // Дифференциальная составляющая ПИД ТН
  Heat.tempPID=3200;                // Целевая температура ПИД
  Heat.add_delta_temp = 150;	 	   // Добавка температуры к установке бойлера, в градусах
  Heat.add_delta_hour = 5;		   	   // Начальный Час добавки температуры к установке бойлера
  Heat.add_delta_end_hour = 6;         // Конечный Час добавки температуры к установке
 // Защиты
  Heat.tempIn=4700;                    // Tемпература подачи (макс)
  Heat.tempOut=-5;                      // Tемпература обратки (минимальная)
  Heat.dt=1500;                        // Максимальная разность температур конденсатора.
  Heat.kWeather=10;                    // Коэффициент погодозависимости в СОТЫХ градуса на градус
  
 // Heat.P1=0;
 
 // Бойлер
  SETBIT1(Boiler.flags,fSchedule);      // !save! флаг Использование расписания выключено
  SETBIT0(Boiler.flags,fTurboBoiler);    // !save! флаг использование ТЭН для нагрева  выключено
  SETBIT0(Boiler.flags,fSalmonella);    // !save! флаг Сальмонела раз внеделю греть бойлер  выключено
  SETBIT0(Boiler.flags,fCirculation);   // !save! флагУправления циркуляционным насосом ГВС  выключено
  SETBIT1(Boiler.flags,fAddHeating);    // флаг флаг догрева ГВС ТЭНом
  SETBIT1(Boiler.flags,fScheduleAddHeat);
  SETBIT0(Boiler.flags,fResetHeat);     // флаг Сброса лишнего тепла в СО
  Boiler.TempTarget=5000;               // !save! Целевая температура бойлера
  Boiler.dTemp=500;                     // !save! гистерезис целевой температуры
  Boiler.tempIn=5400;                   // !save! Tемпература подачи максимальная
  for (uint8_t i=0;i<7; i++) Boiler.Schedule[i]=0;             // !save! Расписание бойлера
  Boiler.Circul_Work=60*3;              // Время  работы насоса ГВС секунды (fCirculation)
  Boiler.Circul_Pause=60*10;            // Пауза в работе насоса ГВС  секунды (fCirculation)
  Boiler.Reset_Time=30;                 // время сброса излишков тепла в секундах (fResetHeat)
  Boiler.pid_time=20;                   // Постоянная интегрирования времени в секундах ПИД ГВС
  Boiler.pid.Kp=1;                      // Пропорциональная составляющая ПИД ГВС
  Boiler.pid.Ki=0;                      // Интегральная составляющая ПИД ГВС
  Boiler.pid.Kd=3;                      // Дифференциальная составляющая ПИД ГВС
  Boiler.tempPID=3800;                  // Целевая температура ПИД ГВС
  Boiler.tempRBOILER=3500;              // Температура ГВС при котором включается бойлер и отключатся ТН
  Boiler.dAddHeat = HYSTERESIS_BoilerAddHeat;
  Boiler.add_delta_temp = 1800;		    // Добавка температуры к установке бойлера, в градусах
  Boiler.add_delta_hour = 6;		    // Начальный Час добавки температуры к установке бойлера
  Boiler.add_delta_end_hour = 6;        // Конечный Час добавки температуры к установке
}

// Охлаждение Установить параметры ТН из числа (float)
boolean Profile::set_paramCoolHP(char *var, float x)
{ 
 if(strcmp(var,hp_RULE)==0)  {  switch ((int)x)
				                   {
				                    case 0: Cool.Rule=pHYSTERESIS; break;
				                    case 1: Cool.Rule=pPID;        break;
				                    case 2: Cool.Rule=pHYBRID;     break;
				                    default:Cool.Rule=pHYSTERESIS; break;
				                    }
 	 	 	 	 	 	 	 	 HP.resetPID(); return true; } else
 if(strcmp(var,hp_TEMP1)==0) {   if ((x>=0)&&(x<=30))  {Cool.Temp1=rd(x, 100); return true;} else return false;   }else             // целевая температура в доме
 if(strcmp(var,hp_TEMP2)==0) {   if ((x>=10)&&(x<=50))  {Cool.Temp2=rd(x, 100); return true;} else return false;  }else             // целевая температура обратки
 if(strcmp(var,hp_TARGET)==0) {  if (x==0) {SETBIT0(Cool.flags,fTarget); return true;} else if (x==1.0) {SETBIT1(Cool.flags,fTarget); return true;} else return false; }else // что является целью значения  0 (температура в доме), 1 (температура обратки).
 if(strcmp(var,hp_DTEMP)==0) {   if ((x>=0)&&(x<=12))  {Cool.dTemp=rd(x, 100); return true;} else return false;   }else             // гистерезис целевой температуры
 if(strcmp(var,hp_HP_TIME)==0) { if ((x>=10)&&(x<=1000)) {UpdatePIDbyTime(x, Cool.pid_time, Cool.pid); Cool.pid_time=x; return true;} else return false;                                             }else             // Постоянная интегрирования времени в секундах ПИД ТН !
 if(strcmp(var,hp_HP_PRO)==0) {  if ((x>=0)&&(x<=32)) {Cool.pid.Kp=rd(x, 1000); return true;} else return false;    }else             // Пропорциональная составляющая ПИД ТН
#ifdef PID_FORMULA2
 if(strcmp(var,hp_HP_IN)==0) {   if ((x>=0)&&(x<=32))  {x *= Cool.pid_time; if(x>32.7) x=32.7; Cool.pid.Ki=rd(x, 1000); return true;} else return false; }else             // Интегральная составляющая ПИД ТН
 if(strcmp(var,hp_HP_DIF)==0) {  if ((x>=0)&&(x<=32))  {Cool.pid.Kd=rd(x / Cool.pid_time, 1000); return true;} else return false; }else             // Дифференциальная составляющая ПИД ТН
#else
 if(strcmp(var,hp_HP_IN)==0) {   if ((x>=0)&&(x<=32))  {Cool.pid.Ki=rd(x, 1000); return true;} else return false;   }else             // Интегральная составляющая ПИД ТН
 if(strcmp(var,hp_HP_DIF)==0) {  if ((x>=0)&&(x<=32))  {Cool.pid.Kd=rd(x, 1000); return true;} else return false;   }else             // Дифференциальная составляющая ПИД ТН
#endif
 if(strcmp(var,hp_TEMP_IN)==0) { if ((x>=0)&&(x<=30))  {Cool.tempIn=rd(x, 100); return true;} else return false;  }else             // температура подачи (минимальная)
 if(strcmp(var,hp_TEMP_OUT)==0){ if ((x>=0)&&(x<=35))  {Cool.tempOut=rd(x, 100); return true;} else return false; }else             // температура обратки (максимальная)
 if(strcmp(var,hp_D_TEMP)==0) {  if ((x>=0)&&(x<=40))  {Cool.dt=rd(x, 100); return true;} else return false;      }else             // максимальная разность температур конденсатора.
 if(strcmp(var,hp_TEMP_PID)==0){ if ((x>=0)&&(x<=30))  {Cool.tempPID=rd(x, 100); return true;} else return false; }else             // Целевая темпеартура ПИД
 if(strcmp(var,hp_WEATHER)==0) { Cool.flags = (Cool.flags & ~(1<<fWeather)) | ((x!=0)<<fWeather); return true; }else     // Использование погодозависимости
 if(strcmp(var,hp_HEAT_FLOOR)==0) { Cool.flags = (Cool.flags & ~(1<<fHeatFloor)) | ((x!=0)<<fHeatFloor); return true; }else
 if(strcmp(var,hp_SUN)==0) { Cool.flags = (Cool.flags & ~(1<<fUseSun)) | ((x!=0)<<fUseSun); return true; }else
 if(strcmp(var,hp_K_WEATHER)==0){ Cool.kWeather=rd(x, 1000); return true; }             // Коэффициент погодозависимости
 return false; 
}

//Охлаждение Получить параметр в виде строки  второй параметр - наличие частотника
char* Profile::get_paramCoolHP(char *var, char *ret, boolean fc)  
{
   if(strcmp(var,hp_RULE)==0)     {if (fc)   // Есть частотник
	 								return web_fill_tag_select(ret, "HYSTERESIS:0;PID:0;HYBRID:0;", Cool.Rule);
				                  else {Cool.Rule=pHYSTERESIS;return strcat(ret,(char*)"HYSTERESIS:1;");}} else             // частотника нет единсвенный алгоритм гистрезис
   if(strcmp(var,hp_TEMP1)==0)    {_ftoa(ret,(float)Cool.Temp1/100,1); return ret;               } else             // целевая температура в доме
   if(strcmp(var,hp_TEMP2)==0)    {_ftoa(ret,(float)Cool.Temp2/100,1); return ret;               } else             // целевая температура обратки
   if(strcmp(var,hp_TARGET)==0)   {if (!(GETBIT(Cool.flags,fTarget))) return strcat(ret,(char*)"Дом:1;Обратка:0;");
                                  else return strcat(ret,(char*)"Дом:0;Обратка:1;");           } else             // что является целью значения  0 (температура в доме), 1 (температура обратки).
   if(strcmp(var,hp_DTEMP)==0)    {_ftoa(ret,(float)Cool.dTemp/100,1); return ret;               } else             // гистерезис целевой температуры
   if(strcmp(var,hp_HP_TIME)==0)  {return  _itoa(Cool.pid_time,ret);                               } else             // Постоянная интегрирования времени в секундах ПИД ТН
   if(strcmp(var,hp_HP_PRO)==0)   {_ftoa(ret,(float)Cool.pid.Kp/1000,3); return ret;              } else             // Пропорциональная составляющая ПИД ТН
#ifdef PID_FORMULA2
   if(strcmp(var,hp_HP_IN)==0)    {_ftoa(ret,(float)Cool.pid.Ki/Cool.pid_time/1000,3); return ret;} else             // Интегральная составляющая ПИД ТН
   if(strcmp(var,hp_HP_DIF)==0)   {_ftoa(ret,(float)Cool.pid.Kd*Cool.pid_time/1000,3); return ret;} else             // Дифференциальная составляющая ПИД ТН
#else
   if(strcmp(var,hp_HP_IN)==0)    {_ftoa(ret,(float)Cool.pid.Ki/1000,3); return ret;              } else             // Интегральная составляющая ПИД ТН
   if(strcmp(var,hp_HP_DIF)==0)   {_ftoa(ret,(float)Cool.pid.Kd/1000,3); return ret;              } else             // Дифференциальная составляющая ПИД ТН
#endif
   if(strcmp(var,hp_TEMP_IN)==0)  {_ftoa(ret,(float)Cool.tempIn/100,1); return ret;              } else             // температура подачи (минимальная)
   if(strcmp(var,hp_TEMP_OUT)==0) {_ftoa(ret,(float)Cool.tempOut/100,1); return ret;             } else             // температура обратки (максимальная)
   if(strcmp(var,hp_D_TEMP)==0)   {_ftoa(ret,(float)Cool.dt/100,1); return ret;                  } else             // максимальная разность температур конденсатора.
   if(strcmp(var,hp_TEMP_PID)==0) {_ftoa(ret,(float)Cool.tempPID/100,1); return ret;          } else             // Целевая темпеартура ПИД
   if(strcmp(var,hp_WEATHER)==0)  { if(GETBIT(Cool.flags,fWeather)) return strcat(ret,(char*)cOne);else return strcat(ret,(char*)cZero);} else // Использование погодозависимости
   if(strcmp(var,hp_HEAT_FLOOR)==0)  { if(GETBIT(Cool.flags,fHeatFloor)) return strcat(ret,(char*)cOne);else return strcat(ret,(char*)cZero);} else
   if(strcmp(var,hp_SUN)==0)      { if(GETBIT(Cool.flags,fUseSun)) return strcat(ret,(char*)cOne);else return strcat(ret,(char*)cZero);} else
   if(strcmp(var,hp_targetPID)==0){_ftoa(ret,(float)HP.CalcTargetPID(Cool)/100,2); return ret;      } else
   if(strcmp(var,hp_K_WEATHER)==0){_ftoa(ret,(float)Cool.kWeather/1000.0,3); return ret;            }                 // Коэффициент погодозависимости
 return  strcat(ret,(char*)cInvalid);   
}

// Отопление Установить параметры ТН из числа (float)
boolean Profile::set_paramHeatHP(char *var, float x)
{ 
if(strcmp(var,hp_RULE)==0) {  switch ((int)x)
				              {
				                 case 0: Heat.Rule=pHYSTERESIS; break;
				                 case 1: Heat.Rule=pPID;        break;
				                 case 2: Heat.Rule=pHYBRID;     break;
				                 default:Heat.Rule=pHYSTERESIS; break;
				              }
							  HP.resetPID(); return true; } else
 if(strcmp(var,hp_TEMP1)==0) {   if ((x>=0)&&(x<=40))   {Heat.Temp1=rd(x, 100); return true;} else return false;  }else             // целевая температура в доме
 if(strcmp(var,ADD_DELTA_TEMP)==0){ if ((x>=-30)&&(x<=50))  {Heat.add_delta_temp=rd(x, 100); return true;}else return false; }else      // Добавка к целевой температуры ВНИМАНИЕ здесь еденица измерения ГРАДУСЫ
 if(strcmp(var,ADD_DELTA_HOUR)==0){ if ((x>=0)&&(x<=23))    {Heat.add_delta_hour=x; return true;} else return false; }else
 if(strcmp(var,ADD_DELTA_END_HOUR)==0){ if ((x>=0)&&(x<=23)){Heat.add_delta_end_hour=x; return true;} else return false; }else
 if(strcmp(var,hp_TEMP2)==0) {   if ((x>=10)&&(x<=50))  {Heat.Temp2=rd(x, 100); return true;} else return false;  }else             // целевая температура обратки
 if(strcmp(var,hp_TARGET)==0) {  if (x==0) {SETBIT0(Heat.flags,fTarget); return true;} else if (x==1.0) {SETBIT1(Heat.flags,fTarget); return true;} else return false; }else // что является целью значения  0 (температура в доме), 1 (температура обратки).
 if(strcmp(var,hp_DTEMP)==0) {   if ((x>=0)&&(x<=12))  {Heat.dTemp=rd(x, 100); return true;} else return false;   }else             // гистерезис целевой температуры
 if(strcmp(var,hp_HP_TIME)==0) { if ((x>=10)&&(x<=1000)) {UpdatePIDbyTime(x, Heat.pid_time, Heat.pid); Heat.pid_time=x; return true;} else return false; }else             // Постоянная интегрирования времени в секундах ПИД ТН !
 if(strcmp(var,hp_HP_PRO)==0) {  if ((x>=0)&&(x<=32)) {Heat.pid.Kp=rd(x, 1000); return true;} else return false;   }else             // Пропорциональная составляющая ПИД ТН
#ifdef PID_FORMULA2
 if(strcmp(var,hp_HP_IN)==0) {   if ((x>=0)&&(x<=32))  {x *= Heat.pid_time; if(x>32.7) x=32.7; Heat.pid.Ki=rd(x, 1000); return true;} else return false; }else  // Интегральная составляющая ПИД ТН
 if(strcmp(var,hp_HP_DIF)==0) {  if ((x>=0)&&(x<=32))  {Heat.pid.Kd=rd(x / Heat.pid_time, 1000); return true;} else return false; }else  // Дифференциальная составляющая ПИД ТН
#else
 if(strcmp(var,hp_HP_IN)==0) {   if ((x>=0)&&(x<=32))  {Heat.pid.Ki=rd(x, 1000); return true;} else return false;   }else             // Интегральная составляющая ПИД ТН
 if(strcmp(var,hp_HP_DIF)==0) {  if ((x>=0)&&(x<=32))  {Heat.pid.Kd=rd(x, 1000); return true;} else return false;   }else             // Дифференциальная составляющая ПИД ТН
#endif
 if(strcmp(var,hp_TEMP_IN)==0) { if ((x>=0)&&(x<=70))  {Heat.tempIn=rd(x, 100); return true;} else return false;     }else             // температура подачи (минимальная)
 if(strcmp(var,hp_TEMP_OUT)==0){ if ((x>=-10)&&(x<=70)) {Heat.tempOut=rd(x, 100); return true;} else return false;    }else             // температура обратки (максимальная)
 if(strcmp(var,hp_D_TEMP)==0) {  if ((x>=0)&&(x<=40))  {Heat.dt=rd(x, 100); return true;} else return false;  }else                  // максимальная разность температур конденсатора.
 if(strcmp(var,hp_TEMP_PID)==0){ if ((x>=10)&&(x<=50))  {Heat.tempPID=rd(x, 100); return true;} else return false;  }else             // Целевая темпеартура ПИД
 if(strcmp(var,hp_WEATHER)==0) { Heat.flags = (Heat.flags & ~(1<<fWeather)) | ((x!=0)<<fWeather); return true; }else                     // Использование погодозависимости
 if(strcmp(var,hp_HEAT_FLOOR)==0) { Heat.flags = (Heat.flags & ~(1<<fHeatFloor)) | ((x!=0)<<fHeatFloor); return true; }else
 if(strcmp(var,hp_SUN)==0) { Heat.flags = (Heat.flags & ~(1<<fUseSun)) | ((x!=0)<<fUseSun); return true; }else
 if(strcmp(var,hp_K_WEATHER)==0){ Heat.kWeather=rd(x, 1000); return true; }             // Коэффициент погодозависимости
 return false; 
}

// Отопление Получить параметр в виде строки  второй параметр - наличие частотника
char* Profile::get_paramHeatHP(char *var,char *ret, boolean fc)
{
  if(strcmp(var,hp_RULE)==0)     {if (fc)   // Есть частотник
	  	  	  	  	  	  	  	  	  return web_fill_tag_select(ret, "HYSTERESIS:0;PID:0;HYBRID:0;", Heat.Rule);
				                  else {Heat.Rule=pHYSTERESIS;return strcat(ret,(char*)"HYSTERESIS:1;");}} else             // частотника нет единсвенный алгоритм гистрезис
   if(strcmp(var,hp_TEMP1)==0)    {_ftoa(ret,(float)Heat.Temp1/100,1); return ret;                } else             // целевая температура в доме
   if(strcmp(var,ADD_DELTA_TEMP)==0) 	{  _ftoa(ret,(float)Heat.add_delta_temp/100, 1); return ret;}else
   if(strcmp(var,ADD_DELTA_HOUR)==0) 	{  _itoa(Heat.add_delta_hour, ret); return ret;         }else
   if(strcmp(var,ADD_DELTA_END_HOUR)==0){  _itoa(Heat.add_delta_end_hour, ret); return ret;    	}else
   if(strcmp(var,hp_TEMP2)==0)    {_ftoa(ret,(float)Heat.Temp2/100,1); return ret;                } else            // целевая температура обратки
   if(strcmp(var,hp_TARGET)==0)   {if (!(GETBIT(Heat.flags,fTarget))) return strcat(ret,(char*)"Дом:1;Обратка:0;");
                                  else return strcat(ret,(char*)"Дом:0;Обратка:1;");           } else             // что является целью значения  0 (температура в доме), 1 (температура обратки).
   if(strcmp(var,hp_DTEMP)==0)    {_ftoa(ret,(float)Heat.dTemp/100,1); return ret;                } else             // гистерезис целевой температуры
   if(strcmp(var,hp_HP_TIME)==0)  {return  _itoa(Heat.pid_time,ret);                                } else             // Постоянная интегрирования времени в секундах ПИД ТН
   if(strcmp(var,hp_HP_PRO)==0)   {_ftoa(ret,(float)Heat.pid.Kp/1000,3); return ret;               } else             // Пропорциональная составляющая ПИД ТН
#ifdef PID_FORMULA2
   if(strcmp(var,hp_HP_IN)==0)    {_ftoa(ret,(float)Heat.pid.Ki/Heat.pid_time/1000,3); return ret;} else             // Интегральная составляющая ПИД ТН
   if(strcmp(var,hp_HP_DIF)==0)   {_ftoa(ret,(float)Heat.pid.Kd*Heat.pid_time/1000,3); return ret;} else             // Дифференциальная составляющая ПИД ТН
#else
   if(strcmp(var,hp_HP_IN)==0)    {_ftoa(ret,(float)Heat.pid.Ki/1000,3); return ret;               } else             // Интегральная составляющая ПИД ТН
   if(strcmp(var,hp_HP_DIF)==0)   {_ftoa(ret,(float)Heat.pid.Kd/1000,3); return ret;               } else             // Дифференциальная составляющая ПИД ТН
#endif
   if(strcmp(var,hp_TEMP_IN)==0)  {_ftoa(ret,(float)Heat.tempIn/100,1); return ret;               } else             // температура подачи (минимальная)
   if(strcmp(var,hp_TEMP_OUT)==0) {_ftoa(ret,(float)Heat.tempOut/100,1); return ret;              } else             // температура обратки (максимальная)
   if(strcmp(var,hp_D_TEMP)==0)   {_ftoa(ret,(float)Heat.dt/100,1); return ret;                   } else             // максимальная разность температур конденсатора.
   if(strcmp(var,hp_TEMP_PID)==0) {_ftoa(ret,(float)Heat.tempPID/100,1); return ret;              } else             // Целевая темпеартура ПИД
   if(strcmp(var,hp_WEATHER)==0)  { if(GETBIT(Heat.flags,fWeather)) return strcat(ret,(char*)cOne);else return strcat(ret,(char*)cZero);} else // Использование погодозависимости
   if(strcmp(var,hp_HEAT_FLOOR)==0)  { if(GETBIT(Heat.flags,fHeatFloor)) return strcat(ret,(char*)cOne);else return strcat(ret,(char*)cZero);} else
   if(strcmp(var,hp_SUN)==0)      { if(GETBIT(Heat.flags,fUseSun)) return strcat(ret,(char*)cOne);else return strcat(ret,(char*)cZero);} else
   if(strcmp(var,hp_targetPID)==0){_ftoa(ret,(float)HP.CalcTargetPID(Heat)/100,2); return ret;    } else
   if(strcmp(var,hp_K_WEATHER)==0){_ftoa(ret,(float)Heat.kWeather/1000,3); return ret;            }                 // Коэффициент погодозависимости
 return  strcat(ret,(char*)cInvalid);  
}

// Настройка бойлера --------------------------------------------------
//Установить параметр из строки
boolean Profile::set_boiler(char *var, char *c)
{ 
	if(strcmp(var,boil_SCHEDULER)==0)		{ return set_Schedule(c,Boiler.Schedule); }  // разбор строки расписания
	float x=my_atof(c);
	if(x == ATOF_ERROR) return false;   // Ошибка преобразования короме расписания - это не число
	if(strcmp(var,boil_BOILER_ON)==0)		{ if(x) SETBIT1(SaveON.flags,fBoilerON); else SETBIT0(SaveON.flags,fBoilerON); return true;} else
	if(strcmp(var,boil_SCHEDULER_ON)==0)	{ if(x) SETBIT1(Boiler.flags,fSchedule); else { SETBIT0(Boiler.flags,fSchedule); SETBIT0(Boiler.flags,fScheduleAddHeat); } return true;} else
	if(strcmp(var,boil_SCHEDULER_ADDHEAT)==0){if(x) {SETBIT1(Boiler.flags,fScheduleAddHeat); SETBIT1(Boiler.flags,fSchedule); } else SETBIT0(Boiler.flags,fScheduleAddHeat); return true;} else
	if(strcmp(var,boil_TOGETHER_HEAT)==0)	{ if(x) SETBIT1(Boiler.flags,fBoilerTogetherHeat); else {
												SETBIT0(Boiler.flags,fBoilerTogetherHeat);
#ifdef RPUMPBH
												if((HP.get_modWork() & pHEAT)) HP.dRelay[RPUMPBH].set_OFF();   // насос ГВС - выключить
												SETBIT0(HP.flags, fHP_BoilerTogetherHeat);
#endif
											} return true;} else
	if(strcmp(var,boil_fBoilerPID)==0)	    { if(x) SETBIT1(Boiler.flags,fBoilerPID); else SETBIT0(Boiler.flags,fBoilerPID); return true;} else
	if(strcmp(var,boil_TURBO_BOILER)==0)	{ if(x) SETBIT1(Boiler.flags,fTurboBoiler); else SETBIT0(Boiler.flags,fTurboBoiler); return true;} else
	if(strcmp(var,boil_SALLMONELA)==0)		{ if(x) { SETBIT1(Boiler.flags,fSalmonella); HP.sTemp[TBOILER].set_maxTemp(SALLMONELA_TEMP+300); }
												else { SETBIT0(Boiler.flags,fSalmonella); HP.sTemp[TBOILER].set_maxTemp(MAXTEMP[TBOILER]); } return true;} else // Изменение максимальной температуры при включенном режиме сальмонелла
	if(strcmp(var,boil_CIRCULATION)==0)		{ if(x) SETBIT1(Boiler.flags,fCirculation); else SETBIT0(Boiler.flags,fCirculation); return true;} else
	if(strcmp(var,boil_TEMP_TARGET)==0)		{ if((x>=5)&&(x<=95)) {Boiler.TempTarget=rd(x, 100); return true;} else return false; } else  // Целевая температура бойлера
	if(strcmp(var,ADD_DELTA_TEMP)==0)		{ if((x>=-50)&&(x<=50)) {Boiler.add_delta_temp=rd(x, 100); return true;}else return false; } else  // Добавка к целевой температуры ВНИМАНИЕ здесь еденица измерения ГРАДУСЫ
	if(strcmp(var,ADD_DELTA_HOUR)==0)		{ if((x>=0)&&(x<=23)) {Boiler.add_delta_hour=x; return true;} else return false; } else      // Начальный Час добавки температуры к установке бойлера
	if(strcmp(var,ADD_DELTA_END_HOUR)==0)	{ if((x>=0)&&(x<=23)){Boiler.add_delta_end_hour=x; return true;} else return false; } else   // Конечный Час добавки температуры к установке
	if(strcmp(var,boil_DTARGET)==0)			{ if((x>=1)&&(x<=20)) {Boiler.dTemp=rd(x, 100); return true;} else return false; } else      // гистерезис целевой температуры
	if(strcmp(var,boil_TEMP_MAX)==0)		{ if((x>=20)&&(x<=70)) {Boiler.tempIn=rd(x, 100); return true;} else return false; } else    // Tемпература подачи максимальная
	if(strcmp(var,boil_CIRCUL_WORK)==0) 	{ if((x>=0)&&(x<=60)){Boiler.Circul_Work=60*x; return true;} else return false;} else         // Время  работы насоса ГВС секунды (fCirculation)
	if(strcmp(var,boil_CIRCUL_PAUSE)==0)	{ if((x>=0)&&(x<=60)){Boiler.Circul_Pause=60*x; return true;} else return false;} else        // Пауза в работе насоса ГВС  секунды (fCirculation)
	if(strcmp(var,boil_RESET_HEAT)==0)		{ if(x) SETBIT1(Boiler.flags,fResetHeat); else SETBIT0(Boiler.flags,fResetHeat); return true;} else // флаг Сброса лишнего тепла в СО
	if(strcmp(var,boil_RESET_TIME)==0)		{ if((x>=1)&&(x<=10000)) {Boiler.Reset_Time=x; return true;} else return false; } else        // время сброса излишков тепла в секундах (fResetHeat)
	if(strcmp(var,boil_BOIL_TIME)==0)		{ if((x>=2)&&(x<=1000)) {UpdatePIDbyTime(x, Boiler.pid_time, Boiler.pid); Boiler.pid_time=x; return true;} else return false; } else             // Постоянная интегрирования времени в секундах ПИД ГВС
	if(strcmp(var,boil_BOIL_PRO)==0)		{ if((x>=0.0)&&(x<=32)) {Boiler.pid.Kp=rd(x, 1000); return true;} else return false; } else  // Пропорциональная составляющая ПИД ГВС
#ifdef PID_FORMULA2
	if(strcmp(var,boil_BOIL_IN)==0)			{ if((x>=0.0)&&(x<=32)) {x *= Boiler.pid_time; Boiler.pid.Ki=rd(x, 1000); return true;} else return false; } else   // Интегральная составляющая ПИД ГВС
	if(strcmp(var,boil_BOIL_DIF)==0)		{ if((x>=0.0)&&(x<=32)) {Boiler.pid.Kd=rd(x / Boiler.pid_time, 1000); return true;} else return false; } else   // Дифференциальная составляющая ПИД ГВС
#else
	if(strcmp(var,boil_BOIL_IN)==0)			{ if((x>=0.0)&&(x<=32)) {Boiler.pid.Ki=rd(x, 1000); return true;} else return false; } else   // Интегральная составляющая ПИД ГВС
	if(strcmp(var,boil_BOIL_DIF)==0)		{ if((x>=0.0)&&(x<=32)) {Boiler.pid.Kd=rd(x, 1000); return true;} else return false; } else   // Дифференциальная составляющая ПИД ГВС
#endif
	if(strcmp(var,boil_BOIL_TEMP)==0)		{ if((x>=30.0)&&(x<=70)) {Boiler.tempPID=rd(x, 100); return true;} else return false; } else   // Целевая темпеартура ПИД ГВС
	if(strcmp(var,boil_ADD_HEATING)==0)		{ if(x) SETBIT1(Boiler.flags,fAddHeating); else SETBIT0(Boiler.flags,fAddHeating); return true;} else  // флаг использования тена для догрева ГВС
	if(strcmp(var,boil_fAddHeatingForce)==0){ if(x) SETBIT1(Boiler.flags,fAddHeatingForce); else SETBIT0(Boiler.flags,fAddHeatingForce); return true;} else
	if(strcmp(var,boil_HeatUrgently)==0)    { HP.set_HeatBoilerUrgently(x); return true;} else
	if(strcmp(var,hp_SUN)==0) 				{ Boiler.flags = (Boiler.flags & ~(1<<fBoilerUseSun)) | ((x!=0)<<fBoilerUseSun); return true; }else
	if(strcmp(var,boil_TEMP_RBOILER)==0)	{ if((x>=0.0)&&(x<=60.0))  {Boiler.tempRBOILER=rd(x, 100); return true;} else return false;} else   // температура включения догрева бойлера
	if(strcmp(var,boil_dAddHeat)==0)	    { Boiler.dAddHeat = rd(x, 100); return true;} else
	return false;
}

// Получить параметр из строки по имени var, результат ДОБАВЛЯЕТСЯ в строку ret
char* Profile::get_boiler(char *var, char *ret)
{ 
 if(strcmp(var,boil_BOILER_ON)==0){       if (GETBIT(SaveON.flags,fBoilerON))   return  strcat(ret,(char*)cOne); else return  strcat(ret,(char*)cZero); }else
 if(strcmp(var,boil_SCHEDULER_ON)==0){    if (GETBIT(Boiler.flags,fSchedule))   return  strcat(ret,(char*)cOne); else return  strcat(ret,(char*)cZero); }else
 if(strcmp(var,boil_SCHEDULER_ADDHEAT)==0){ if (GETBIT(Boiler.flags,fScheduleAddHeat)) return  strcat(ret,(char*)cOne); else return  strcat(ret,(char*)cZero); }else
 if(strcmp(var,boil_TOGETHER_HEAT)==0){ if (GETBIT(Boiler.flags,fBoilerTogetherHeat)) return  strcat(ret,(char*)cOne); else return  strcat(ret,(char*)cZero); }else
 if(strcmp(var,boil_fBoilerPID)==0){ if (GETBIT(Boiler.flags,fBoilerPID)) return  strcat(ret,(char*)cOne); else return  strcat(ret,(char*)cZero); }else
 if(strcmp(var,boil_TURBO_BOILER)==0){    if (GETBIT(Boiler.flags,fTurboBoiler))return  strcat(ret,(char*)cOne); else return  strcat(ret,(char*)cZero); }else
 if(strcmp(var,boil_SALLMONELA)==0){      if (GETBIT(Boiler.flags,fSalmonella)) return  strcat(ret,(char*)cOne); else return  strcat(ret,(char*)cZero); }else
 if(strcmp(var,boil_CIRCULATION)==0){     if (GETBIT(Boiler.flags,fCirculation))return  strcat(ret,(char*)cOne); else return  strcat(ret,(char*)cZero); }else
 if(strcmp(var,boil_TEMP_TARGET)==0){     _ftoa(ret,(float)Boiler.TempTarget/100,1); return ret;    }else
 if(strcmp(var,ADD_DELTA_TEMP)==0) 		{ _ftoa(ret,(float)Boiler.add_delta_temp/100, 1); return ret; }else
 if(strcmp(var,ADD_DELTA_HOUR)==0) 		{ _itoa(Boiler.add_delta_hour, ret); return ret;           }else
 if(strcmp(var,ADD_DELTA_END_HOUR)==0) 	{ _itoa(Boiler.add_delta_end_hour, ret); return ret;    	}else
 if(strcmp(var,boil_DTARGET)==0){         _ftoa(ret,(float)Boiler.dTemp/100,1); return ret;        }else
 if(strcmp(var,boil_TEMP_MAX)==0){        _ftoa(ret,(float)Boiler.tempIn/100,1); return ret;       }else
 if(strcmp(var,boil_SCHEDULER)==0){       return strcat(ret,get_Schedule(Boiler.Schedule));          }else  
 if(strcmp(var,boil_CIRCUL_WORK)==0){     return _itoa(Boiler.Circul_Work/60,ret);                   }else                            // Время  работы насоса ГВС секунды (fCirculation)
 if(strcmp(var,boil_CIRCUL_PAUSE)==0){    return _itoa(Boiler.Circul_Pause/60,ret);                  }else                            // Пауза в работе насоса ГВС  секунды (fCirculation)
 if(strcmp(var,boil_RESET_HEAT)==0){      if (GETBIT(Boiler.flags,fResetHeat))   return  strcat(ret,(char*)cOne); else return  strcat(ret,(char*)cZero); }else       // флаг Сброса лишнего тепла в СО
 if(strcmp(var,boil_RESET_TIME)==0){      return  _itoa(Boiler.Reset_Time,ret);                      }else                            // время сброса излишков тепла в секундах (fResetHeat)
 if(strcmp(var,boil_BOIL_TIME)==0){       return  _itoa(Boiler.pid_time,ret);                        }else                            // Постоянная интегрирования времени в секундах ПИД ГВС
 if(strcmp(var,boil_BOIL_PRO)==0){        _ftoa(ret,(float)Boiler.pid.Kp/1000,3); return ret;        }else                            // Пропорциональная составляющая ПИД ГВС
#ifdef PID_FORMULA2
 if(strcmp(var,boil_BOIL_IN)==0){         _ftoa(ret,(float)Boiler.pid.Ki/Boiler.pid_time/1000,3); return ret;} else             // Интегральная составляющая ПИД ТН
 if(strcmp(var,boil_BOIL_DIF)==0){        _ftoa(ret,(float)Boiler.pid.Kd*Boiler.pid_time/1000,3); return ret;} else             // Дифференциальная составляющая ПИД ТН
#else
 if(strcmp(var,boil_BOIL_IN)==0){         _ftoa(ret,(float)Boiler.pid.Ki/1000,3); return ret;       }else                            // Интегральная составляющая ПИД ГВС
 if(strcmp(var,boil_BOIL_DIF)==0){        _ftoa(ret,(float)Boiler.pid.Kd/1000,3); return ret;       }else                            // Дифференциальная составляющая ПИД ГВС
#endif
 if(strcmp(var,boil_BOIL_TEMP)==0){       _ftoa(ret,(float)Boiler.tempPID/100,1); return ret;       }else                            // Целевая темпеартура ПИД ГВС
 if(strcmp(var,boil_ADD_HEATING)==0){     if(GETBIT(Boiler.flags,fAddHeating)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero); }else   // флаг использования тена для догрева ГВС
 if(strcmp(var,boil_fAddHeatingForce)==0){if(GETBIT(Boiler.flags,fAddHeatingForce)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero); }else
 if(strcmp(var,hp_SUN)==0) { if(GETBIT(Boiler.flags,fBoilerUseSun)) return strcat(ret,(char*)cOne);else return strcat(ret,(char*)cZero);} else
 if(strcmp(var,boil_TEMP_RBOILER)==0){    _ftoa(ret,(float)Boiler.tempRBOILER/100,1); return ret;    }else                            // температура включения догрева бойлера
 if(strcmp(var,boil_dAddHeat)==0){        _ftoa(ret,(float)Boiler.dAddHeat/100,1); return ret;       }else
 if(strcmp(var,boil_HeatUrgently)==0){if(HP.HeatBoilerUrgently) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero); }else
 return strcat(ret,(char*)cInvalid);
}

// Порядок записи профиля
/*
 * sizeof(magic) + \
   sizeof(crc16) + \
   // данные контрольная сумма считается с этого места
   sizeof(dataProfile) + \
   sizeof(SaveON) + \
   sizeof(Cool)  + \
   sizeof(Heat)  + \
   sizeof(Boiler)+ \
 */

// static uint16_t crc= 0xFFFF;  // рабочее значение
 uint16_t  Profile::get_crc16_mem()  // Расчитать контрольную сумму
 {
  uint16_t i;
  uint16_t crc= 0xFFFF;
  for(i=0;i<sizeof(dataProfile);i++) crc=_crc16(crc,*((byte*)&dataProfile+i));           // CRC16 структуры  dataProfile
  for(i=0;i<sizeof(SaveON);i++) crc=_crc16(crc,*((byte*)&SaveON+i));                     // CRC16 структуры  SaveON
  for(i=0;i<sizeof(Cool);i++) crc=_crc16(crc,*((byte*)&Cool+i));                         // CRC16 структуры  Cool
  for(i=0;i<sizeof(Heat);i++) crc=_crc16(crc,*((byte*)&Heat+i));                         // CRC16 структуры  Heat
  for(i=0;i<sizeof(Boiler);i++) crc=_crc16(crc,*((byte*)&Boiler+i));                     // CRC16 структуры  Boiler
  for(i=0;i<sizeof(DailySwitch);i++) crc=_crc16(crc,*((byte*)&DailySwitch+i));           // CRC16 структуры  DailySwitch
  return crc;   
 }

// Проверить контрольную сумму ПРОФИЛЯ в EEPROM для данных на выходе ошибка, длина определяется из заголовка
int8_t Profile::check_crc16_eeprom(int8_t num)
{
  uint16_t i, crc16tmp;
  byte x;
  uint16_t crc= 0xFFFF;
  int32_t adr=I2C_PROFILE_EEPROM+dataProfile.len*num;     // вычислить адрес начала данных
  
  if (readEEPROM_I2C(adr, (byte*)&x, sizeof(x))) { set_Error(ERR_LOAD_PROFILE,(char*)nameHeatPump); return ERR_SAVE_PROFILE;}  adr=adr+sizeof(x);              // прочитать заголовок
  if (x==!0xaa)   return OK;                                                                                                                              // профиль пустой, или его вообще нет, контрольная сумма не актуальна
//  if (x!=0xaa)  { set_Error(ERR_HEADER_PROFILE,(char*)nameHeatPump); return ERR_HEADER_PROFILE;}                                                       // Заголовок не верен, данных нет
  if (readEEPROM_I2C(adr, (byte*)&crc16tmp, sizeof(crc16tmp))) { set_Error(ERR_LOAD_PROFILE,(char*)nameHeatPump); return ERR_SAVE_PROFILE;}  adr=adr+sizeof(crc16tmp);        // прочитать crc16
  
  for (i=0;i<dataProfile.len-1-2;i++) {if (readEEPROM_I2C(adr+i, (byte*)&x, sizeof(x))) { set_Error(ERR_LOAD_PROFILE,(char*)nameHeatPump); return ERR_LOAD_PROFILE;}  crc=_crc16(crc,x);} // расчет - записанной в еепром сумму за вычетом заголовка
  if (crc==crc16tmp) return OK; 
  else            return err=ERR_CRC16_PROFILE;
}
// Проверить контрольную сумму в буфере для данных на выходе ошибка, длина определяется из заголовка
int8_t  Profile::check_crc16_buf(int32_t adr, byte* buf)   
{
  uint16_t i, readCRC, crc=0xFFFF;
  byte x;
  memcpy((byte*)&x,buf+adr,sizeof(x)); adr=adr+sizeof(x);                                              // заголовок
  if (x==!0xaa)   return OK;                                                                           // профиль пустой, или его вообще нет, контрольная сумма не актуальна
 // if (x!=0xaa)   { set_Error(ERR_HEADER_PROFILE,(char*)nameHeatPump); return ERR_HEADER_PROFILE;}    // Заголовок не верен, данных нет
  memcpy((byte*)&readCRC,buf+adr,sizeof(readCRC)); adr=adr+sizeof(readCRC);                            // прочитать crc16
  // Расчет контрольной суммы
  for (i=0;i<dataProfile.len-1-2;i++) crc=_crc16(crc,buf[adr+i]);                                      // расчет -2 за вычетом CRC16 из длины и заголовка один байт
  if (crc==readCRC) return OK; 
  else              return err=ERR_CRC16_EEPROM;
}

int8_t  Profile::convert_to_new_version(void)
{
	/*
	  char checker(int);
	  char checkSizeOfInt1[sizeof(dataProfile)]={checker(&checkSizeOfInt1)};
	  char checkSizeOfInt2[sizeof(SaveON)]={checker(&checkSizeOfInt2)};
	  char checkSizeOfInt3[sizeof(Heat)]={checker(&checkSizeOfInt3)};
	  char checkSizeOfInt4[sizeof(Boiler)]={checker(&checkSizeOfInt4)};
	  char checkSizeOfInt5[sizeof(DailySwitch)]={checker(&checkSizeOfInt5)};
	//*/
	// v.135
	#define CNVPROF_SIZE_dataProfile	120
	#define CNVPROF_SIZE_SaveON		 	12
	#define CNVPROF_SIZE_HeatCool		50
	#define CNVPROF_SIZE_Boiler			80
	#define CNVPROF_SIZE_ALL (sizeof(magic) + sizeof(crc16) + CNVPROF_SIZE_dataProfile + CNVPROF_SIZE_SaveON + CNVPROF_SIZE_HeatCool + CNVPROF_SIZE_HeatCool + CNVPROF_SIZE_Boiler)
	if(HP.Option.ver <= 135) {
		journal.jprintf("Converting Profiles to new version...\n");
		if(readEEPROM_I2C(I2C_PROFILE_EEPROM, (byte*)&Socket[0].outBuf, CNVPROF_SIZE_ALL * I2C_PROFIL_NUM)) return ERR_LOAD_EEPROM;
		for(uint8_t i = 0; i < I2C_PROFIL_NUM; i++) {
			uint8_t *addr = (uint8_t*)&Socket[0].outBuf + CNVPROF_SIZE_ALL * i;
			if(*addr != 0xaa) continue;
			addr += sizeof(magic) + sizeof(crc16);
			memcpy(&dataProfile, addr, CNVPROF_SIZE_dataProfile <= sizeof(dataProfile) ? CNVPROF_SIZE_dataProfile : sizeof(dataProfile));
			addr += CNVPROF_SIZE_dataProfile;
			memcpy(&SaveON, addr, CNVPROF_SIZE_SaveON <= sizeof(SaveON) ? CNVPROF_SIZE_SaveON : sizeof(SaveON));
			addr += CNVPROF_SIZE_SaveON;
			memcpy(&Cool, addr, CNVPROF_SIZE_HeatCool <= sizeof(Cool) ? CNVPROF_SIZE_HeatCool : sizeof(Cool));
			addr += CNVPROF_SIZE_HeatCool;
			memcpy(&Heat, addr, CNVPROF_SIZE_HeatCool <= sizeof(Heat) ? CNVPROF_SIZE_HeatCool : sizeof(Heat));
			addr += CNVPROF_SIZE_HeatCool;
			memcpy(&Boiler, addr, CNVPROF_SIZE_Boiler <= sizeof(Boiler) ? CNVPROF_SIZE_Boiler : sizeof(Boiler));
			if(save(i) < 0) return ERR_SAVE_EEPROM;
		}
		if(HP.save() < 0) return ERR_SAVE_EEPROM;
	}
	return OK;
}

// Записать профайл в еепром под номерм num
// Возвращает число записанных байт или ошибку
int16_t  Profile::save(int8_t num)
{
  magic=0xaa;                                   // Обновить заголовок
  dataProfile.len=get_sizeProfile();            // вычислить адрес начала данных
  dataProfile.saveTime=rtcSAM3X8.unixtime();    // запомнить время сохранения профиля

  int32_t adr=I2C_PROFILE_EEPROM+dataProfile.len*num;
  
  if (writeEEPROM_I2C(adr, (byte*)&magic, sizeof(magic))) { set_Error(ERR_SAVE_PROFILE,(char*)nameHeatPump); return ERR_SAVE_PROFILE;}  adr=adr+sizeof(magic);       // записать заголовок
  int32_t adrCRC16=adr;                                                                                                                                              // Запомнить адрес куда писать контрольную сумму
  adr=adr+sizeof(crc16);                                                                                                                                             // пропуск записи контрольной суммы
  if (writeEEPROM_I2C(adr, (byte*)&dataProfile, sizeof(dataProfile))) { set_Error(ERR_SAVE_PROFILE,(char*)nameHeatPump); return ERR_SAVE_PROFILE;}  adr=adr+sizeof(dataProfile);// записать данные
  if (writeEEPROM_I2C(adr, (byte*)&SaveON, sizeof(SaveON))) { set_Error(ERR_SAVE_PROFILE,(char*)nameHeatPump); return ERR_SAVE_PROFILE;}  adr=adr+sizeof(SaveON);    // записать состояние ТН
  if (writeEEPROM_I2C(adr, (byte*)&Cool, sizeof(Cool))) { set_Error(ERR_SAVE_PROFILE,(char*)nameHeatPump); return ERR_SAVE_PROFILE;}  adr=adr+sizeof(Cool);          // записать настройки охлаждения
  if (writeEEPROM_I2C(adr, (byte*)&Heat, sizeof(Heat))) { set_Error(ERR_SAVE_PROFILE,(char*)nameHeatPump); return ERR_SAVE_PROFILE;}  adr=adr+sizeof(Heat);          // записать настройи отопления
  if (writeEEPROM_I2C(adr, (byte*)&Boiler, sizeof(Boiler))) { set_Error(ERR_SAVE_PROFILE,(char*)nameHeatPump); return ERR_SAVE_PROFILE;}  adr=adr+sizeof(Boiler);    // записать настройки ГВС
  if (writeEEPROM_I2C(adr, (byte*)&DailySwitch, sizeof(DailySwitch))) { set_Error(ERR_SAVE_PROFILE,(char*)nameHeatPump); return ERR_SAVE_PROFILE;}  adr=adr+sizeof(DailySwitch);    // записать настройки DailySwitch
  // запись контрольной суммы
  crc16=get_crc16_mem();
  if (writeEEPROM_I2C(adrCRC16, (byte*)&crc16, sizeof(crc16))) {set_Error(ERR_SAVE_PROFILE,(char*)nameHeatPump); return err=ERR_SAVE_PROFILE;} 

  if ((err=check_crc16_eeprom(num))!=OK) { journal.jprintf("- Verify Error!\n"); return (int16_t) err;}                            // ВЕРИФИКАЦИЯ Контрольные суммы не совпали
  journal.jprintf(" Save profile #%d OK, wrote: %d bytes, crc: %04x\n",num,dataProfile.len,crc16);                                                        // дошли до конца значит ошибок нет
  update_list(num);                                                                                                                                                  // обновить список
  return dataProfile.len;
}

// загрузить профайл num из еепром память
int32_t Profile::load(int8_t num)
{
  byte x;
  int32_t adr=I2C_PROFILE_EEPROM+dataProfile.len*num;     // вычислить адрес начала данных
   
  if (readEEPROM_I2C(adr, (byte*)&x, sizeof(magic))) { set_Error(ERR_LOAD_PROFILE,(char*)nameHeatPump); return err=ERR_LOAD_PROFILE;}  adr=adr+sizeof(magic);         // прочитать заголовок
 
  if (x==PROFILE_EMPTY) {journal.jprintf(" Profile #%d is empty\n",num); return OK;}                                                                                  // профиль пустой, загружать нечего, выходим
  if (x==!0xaa)  {journal.jprintf(" Profile #%d is bad format\n",num); return OK; }                                                                                   // профиль битый, читать нечего выходим

  #ifdef LOAD_VERIFICATION
    if ((err=check_crc16_eeprom(num))!=OK) { journal.jprintf(" Error load profile #%d, CRC16 is wrong!\n",num); return err;}                           // проверка контрольной суммы перед чтением
  #endif
  
  if (readEEPROM_I2C(adr, (byte*)&crc16, sizeof(crc16))) { set_Error(ERR_LOAD_PROFILE,(char*)nameHeatPump); return ERR_LOAD_PROFILE;}  adr=adr+sizeof(crc16);                   // прочитать crc16
  if (readEEPROM_I2C(adr, (byte*)&dataProfile, sizeof(dataProfile))) { set_Error(ERR_LOAD_PROFILE,(char*)nameHeatPump); return ERR_LOAD_PROFILE;}  adr=adr+sizeof(dataProfile); // прочитать данные
  
  #ifdef LOAD_VERIFICATION
  if (dataProfile.len!=get_sizeProfile())  { set_Error(ERR_BAD_LEN_PROFILE,(char*)nameHeatPump); return err=ERR_BAD_LEN_PROFILE;}                                    // длины не совпали
  #endif
  
  x = TaskSuspendAll(); // Запрет других задач
  // читаем основные данные
  if(readEEPROM_I2C(adr, (byte*) &SaveON, sizeof(SaveON))) err = ERR_LOAD_PROFILE; // прочитать состояние ТН
  else if(readEEPROM_I2C(adr += sizeof(SaveON), (byte*) &Cool, sizeof(Cool))) err = ERR_LOAD_PROFILE; // прочитать настройки охлаждения
  else if(readEEPROM_I2C(adr += sizeof(Cool), (byte*) &Heat, sizeof(Heat))) err = ERR_LOAD_PROFILE; // прочитать настройки отопления
  else if(readEEPROM_I2C(adr += sizeof(Heat), (byte*) &Boiler, sizeof(Boiler))) err = ERR_LOAD_PROFILE; // прочитать настройки ГВС
  else if(readEEPROM_I2C(adr += sizeof(DailySwitch), (byte*) &DailySwitch, sizeof(DailySwitch))) err = ERR_LOAD_PROFILE; // прочитать настройки DailySwitch

  if(x) xTaskResumeAll(); // Разрешение других задач
  if(err) {
	  set_Error(err, (char*) nameHeatPump);
	  return err;
  }
  adr += sizeof(Boiler);     // прочитать настройки ГВС
 
// ВСЕ ОК
   #ifdef LOAD_VERIFICATION
  // проверка контрольной суммы
  if(crc16!=get_crc16_mem()) { set_Error(ERR_CRC16_PROFILE,(char*)nameHeatPump); return err=ERR_CRC16_PROFILE;}                                                           // прочитать crc16
  if (dataProfile.len!=adr-(I2C_PROFILE_EEPROM+dataProfile.len*num))  {err=ERR_BAD_LEN_EEPROM;set_Error(ERR_BAD_LEN_EEPROM,(char*)nameHeatPump); return err;} // Проверка длины
    journal.jprintf(" Load profile #%d OK, read: %d bytes, crc: %04x\n",num,adr-(I2C_PROFILE_EEPROM+dataProfile.len*num),crc16);
  #else
    journal.jprintf(" Load profile #%d OK, read: %d bytes VERIFICATION OFF!\n",num,adr-(I2C_PROFILE_EEPROM+dataProfile.len*num));
  #endif
  update_list(num); 
   
  #ifdef TBOILER // Изменение максимальной температуры при включенном режиме сальмонелла
  if (GETBIT(HP.Prof.Boiler.flags,fSalmonella)) {HP.sTemp[TBOILER].set_maxTemp(SALLMONELA_TEMP+300);journal.jprintf(" Set boiler max t=%.2f for salmonella\n",(float)(HP.sTemp[TBOILER].get_maxTemp()/100.0));} 
  else HP.sTemp[TBOILER].set_maxTemp(MAXTEMP[TBOILER]);
  #endif
  // Обнуляем ПИД errKp
  return adr;
 }

// Считать настройки из буфера на входе адрес с какого, на выходе код ошибки (меньше нуля)
int8_t Profile::loadFromBuf(int32_t adr,byte *buf)  
{
  uint16_t i;
  byte x;
  uint32_t aStart=adr;
   
  // Прочитать заголовок
  memcpy((byte*)&x,buf+adr,sizeof(magic)); adr=adr+sizeof(magic);                                                       // заголовок
  if (x==PROFILE_EMPTY) {journal.jprintf(" Profile of memory is empty\n"); return OK;}                                  // профиль пустой, загружать нечего, выходим
  if (x==!0xaa)  {journal.jprintf(" Profile of memory is bad format\n"); return OK; }                                   // профиль битый, читать нечего выходим

  // проверка контрольной суммы
  #ifdef LOAD_VERIFICATION 
  if ((err=check_crc16_buf(aStart,buf)!=OK)) {journal.jprintf(" Error load profile from file, crc is wrong!\n"); return err;}
  #endif 
  
  memcpy((byte*)&i,buf+adr,sizeof(i)); adr=adr+sizeof(i);                                                             // прочитать crc16
  
  // Прочитать переменные ТН
   memcpy((byte*)&dataProfile,buf+adr,sizeof(dataProfile)); adr=adr+sizeof(dataProfile);                              // прочитать структуры  dataProfile
   memcpy((byte*)&SaveON,buf+adr,sizeof(SaveON)); adr=adr+sizeof(SaveON);                                             // прочитать SaveON
   memcpy((byte*)&Cool,buf+adr,sizeof(Cool)); adr=adr+sizeof(Cool);                                                   // прочитать параметры охлаждения
   memcpy((byte*)&Heat,buf+adr,sizeof(Heat)); adr=adr+sizeof(Heat);                                                   // прочитать параметры отопления
   memcpy((byte*)&Boiler,buf+adr,sizeof(Boiler)); adr=adr+sizeof(Boiler);                                             // прочитать параметры бойлера
   memcpy((byte*)&DailySwitch,buf+adr,sizeof(DailySwitch)); adr=adr+sizeof(DailySwitch);                              // прочитать параметры DailySwitch

   #ifdef LOAD_VERIFICATION
    if (dataProfile.len!=adr-aStart)  {err=ERR_BAD_LEN_EEPROM;set_Error(ERR_BAD_LEN_EEPROM,(char*)nameHeatPump); return err;}    // Проверка длины
    journal.jprintf(" Load profile from file OK, read: %d bytes, crc: %04x\n",adr-aStart,i);                                                                    // ВСЕ ОК
  #else
    journal.jprintf(" Load setting from file OK, read: %d bytes VERIFICATION OFF!\n",adr-aStart);
  #endif
  
  #ifdef TBOILER // Изменение максимальной температуры при включенном режиме сальмонелла
  if (GETBIT(HP.Prof.Boiler.flags,fSalmonella)) {HP.sTemp[TBOILER].set_maxTemp(SALLMONELA_TEMP+300);journal.jprintf(" Set boiler max t=%.2f for salmonella\n",(float)(HP.sTemp[TBOILER].get_maxTemp()/100.0));} 
  else HP.sTemp[TBOILER].set_maxTemp(MAXTEMP[TBOILER]);
  #endif

  return OK;       
}

// Профиль Установить параметры ТН из числа (float)
boolean Profile::set_paramProfile(char *var, char *c)
{
	uint32_t x = atoi(c);
	if(strcmp(var, prof_NAME_PROFILE) == 0) {
		urldecode(dataProfile.name, c, sizeof(dataProfile.name));
		return true;
	} else if(strcmp(var, prof_ENABLE_PROFILE) == 0) {
		if(strcmp(c, cZero) == 0) {
			SETBIT0(dataProfile.flags, fEnabled);
			return true;
		} else if(strcmp(c, cOne) == 0) {
			SETBIT1(dataProfile.flags, fEnabled);
			return true;
		}
	} else if(strcmp(var, prof_ID_PROFILE) == 0) {
		if(x == 0 || x > I2C_PROFIL_NUM) return false; // не верный номер профиля
		dataProfile.id = x - 1;
		return true;
	} else if(strcmp(var, prof_NOTE_PROFILE) == 0) {
		urldecode(dataProfile.note, c, sizeof(dataProfile.note));
		return true;
	} else if(strcmp(var, prof_DATE_PROFILE) == 0) {
		return true;
	} else if(strncmp(var, prof_DailySwitch, sizeof(prof_DailySwitch)-1) == 0) {
		var += sizeof(prof_DailySwitch)-1;
		uint32_t i = *(var + 1) - '0';
		if(i >= DAILY_SWITCH_MAX) return false;
		if(*var == prof_DailySwitchDevice) {
			DailySwitch[i].Device = x;
		} else {
			uint32_t h = x / 10;
			if(h > 23) h = 23;
			uint32_t m = x % 10;
			if(m > 5) m = 5;
			x = h * 10 + m;
			if(*var == prof_DailySwitchOn) {
				DailySwitch[i].TimeOn = x;
			} else if(*var == prof_DailySwitchOff) {
				DailySwitch[i].TimeOff = x;
			}
		}
		return true;
	} else // параметры только чтение
	if(strcmp(var, prof_CRC16_PROFILE) == 0) {
		return true;
	} else if(strcmp(var, prof_NUM_PROFILE) == 0) {
		return true;
	} 
	return false;
}
 // профиль Получить параметры по имени var, результат ДОБАВЛЯЕТСЯ в строку ret
char*   Profile::get_paramProfile(char *var, char *ret)
{
	if(strcmp(var,prof_NAME_PROFILE)==0)   { return strcat(ret,dataProfile.name);                             }else
	if(strcmp(var,prof_ENABLE_PROFILE)==0) { if (GETBIT(dataProfile.flags,fEnabled)) return  strcat(ret,(char*)cOne);
											 else                                    return  strcat(ret,(char*)cZero);}else
	if(strcmp(var,prof_ID_PROFILE)==0)     { return _itoa(dataProfile.id + 1,ret);                            }else
	if(strcmp(var,prof_NOTE_PROFILE)==0)   { return strcat(ret,dataProfile.note);                             }else
	if(strcmp(var,prof_DATE_PROFILE)==0)   { return DecodeTimeDate(dataProfile.saveTime,ret);                 }else// параметры только чтение
	if(strcmp(var,prof_CRC16_PROFILE)==0)  { return strcat(ret,uint16ToHex(crc16));                           }else
	if(strcmp(var,prof_NUM_PROFILE)==0)    { return _itoa(I2C_PROFIL_NUM,ret);                                }else
	if(strncmp(var, prof_DailySwitch, sizeof(prof_DailySwitch)-1) == 0) {
		var += sizeof(prof_DailySwitch)-1;
		uint8_t i = *(var + 1) - '0';
		if(i >= DAILY_SWITCH_MAX) return false;
		if(*var == prof_DailySwitchDevice) {
		 _itoa(DailySwitch[i].Device, ret);
		} else if(*var == prof_DailySwitchOn) {
		 m_snprintf(ret + m_strlen(ret), 32, "%02d:%d0", DailySwitch[i].TimeOn / 10, DailySwitch[i].TimeOn % 10);
		} else if(*var == prof_DailySwitchOff) {
		 m_snprintf(ret + m_strlen(ret), 32, "%02d:%d0", DailySwitch[i].TimeOff / 10, DailySwitch[i].TimeOff % 10);
		}
		return ret;
	} else
		return  strcat(ret,(char*)cInvalid);
}

// Временные данные для профиля
static type_dataProfile temp_prof;

// ДОБАВЛЯЕТ к строке с - описание профиля num
char *Profile::get_info(char *c,int8_t num)  
{
  byte xx;
  uint16_t crc16temp;
  int32_t adr=I2C_PROFILE_EEPROM+dataProfile.len*num;                                            // вычислить адрес начала профиля
  if (readEEPROM_I2C(adr, (byte*)&xx, sizeof(magic))) {strcat(c,"Error read profile"); return c; }     // прочитать заголовок
  
  if (xx==PROFILE_EMPTY)  {strcat(c,"Empty profile"); return c;}                                 // Данных нет
  if (xx!=0xaa)  {strcat(c,"Bad format profile"); return c;}                                     // Заголовок не верен, данных нет
    
  adr=adr+sizeof(magic); 
  if (readEEPROM_I2C(adr, (byte*)&crc16temp,sizeof(crc16))) {strcat(c,"Error read profile");return c;} // прочитать crc16
  adr=adr+sizeof(crc16);                                                                         // вычислить адрес начала данных

    if (readEEPROM_I2C(adr, (byte*)&temp_prof, sizeof(temp_prof))) {strcat(c,"Error read profile");return c;}    // прочитать данные
    // прочли формируем строку с описанием
    if(GETBIT(temp_prof.flags, fEnabled)) strcat(c,"* ");                         // отметка об использовании в списке
    strcat(c,temp_prof.name);
    strcat(c,":  ");
    strcat(c,temp_prof.note);
    strcat(c," [");
    DecodeTimeDate(temp_prof.saveTime,c);
//    strcat(c," ");
//    strcat(c,uint16ToHex(crc16temp));
    strcat(c,"]");
    return c;
}

// стереть профайл num из еепром  (ставится признак пусто, данные не стираются)
int32_t Profile::erase(int8_t num)
{
   int32_t adr=I2C_PROFILE_EEPROM+dataProfile.len*num;                                             // вычислить адрес начала профиля
   byte xx=PROFILE_EMPTY;                                                                          // признак стертого профайла
   if (writeEEPROM_I2C(adr, (byte*)&xx, sizeof(xx))) { set_Error(ERR_SAVE_PROFILE,(char*)nameHeatPump); return ERR_SAVE_PROFILE;}  // записать заголовок
   return dataProfile.len;                                                                         // вернуть длину профиля
}

// ДОБАВЛЯЕТ к строке с - список возможных конфигураций num - текущий профиль,
char *Profile::get_list(char *c/*,int8_t num*/)
{
  return strcat(c,list);
}

// Устанавливает текущий профиль из номера списка, новый профиль;
int8_t Profile::set_list(int8_t num)
{
	if(num != dataProfile.id) { // new
		if(load(num) < 0) journal.jprintf(" Profile #%d not selected!\n", num);
	}
	return dataProfile.id;
}

// обновить список имен профилей, зопоминается в строке list
// Возможно что будет отсутвовать выбранный элемент - это нормально
// такое будет догда когда текущйий профиль не отмечен что учасвует в списке
int8_t Profile::update_list(int8_t num)
{
  byte xx;
  uint8_t i;
  int32_t adr;
  strcpy(list,"");                                                                              // стереть список
  char *p = list;
  for (i=0;i<I2C_PROFIL_NUM;i++)                                                                // перебор по всем профилям
  {
    adr=I2C_PROFILE_EEPROM+ get_sizeProfile()*i;                                                // вычислить адрес начала профиля
    if (readEEPROM_I2C(adr, (byte*)&xx, sizeof(xx))) { continue; }                              // прочитать заголовок
    if (xx!=0xaa)   continue;                                                                   // Заголовок не верен, данных нет, пропускаем чтение профиля это не ошибка
    adr=adr+sizeof(magic)+sizeof(crc16);                                                        // вычислить адрес начала данных
    if (readEEPROM_I2C(adr, (byte*)&temp_prof, sizeof(temp_prof))) { continue; }                          // прочитать данные
    if ((GETBIT(temp_prof.flags,fEnabled))||(i==num))                                                // Если разрешено использовать или ТЕКУЩИЙ профиль
   // if (GETBIT(temp.flags,fEnabled)))                                                         // Если разрешено использовать
     { 
    	p = list + m_strlen(list);
    	_itoa(i + 1, p);
    	strcat(p, ". ");
    	strcat(p, temp_prof.name);
    	if (i==num) strcat(p,":1;"); else strcat(p,":0;");
     }                    
  }
 return OK;
}

// Прочитать из EEPROM структуру: режим работы ТН (SaveON), возврат OK - успешно
int8_t  Profile::load_from_EEPROM_SaveON(type_SaveON *_SaveOn)
{
	return readEEPROM_I2C(I2C_PROFILE_EEPROM + dataProfile.len * dataProfile.id + sizeof(magic) + sizeof(crc16) + dataProfile.len, (byte*)_SaveOn, sizeof(SaveON)) ? ERR_LOAD_PROFILE : OK;
}

