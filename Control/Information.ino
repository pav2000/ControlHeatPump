/*
 * Copyright (c) 2016-2018 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav; by vad711 (vad7@yahoo.com)
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
    full=false;                   // Буфер не полный
    err=OK;
    #ifdef DEBUG
      Serial.begin(UART_SPEED);                   // Если надо инициализировать отладочный порт
    #endif
        
    #ifndef I2C_EEPROM_64KB     // журнал в памяти
     memset(_data,0,JOURNAL_LEN); 
     jprintf("\nSTART ----------------------\n"); 
     jprintf("Init RAM journal, size %d . . .\n",JOURNAL_LEN);  
     return;  
    #else                      // журнал во флеше

    #ifdef DEBUG
     Serial.println("\nInit I2C journal . . .");
    #endif  
     uint8_t eepStatus=0;
     uint16_t i;
     char *ptr;
     
     if ((eepStatus=eepromI2C.begin(I2C_SPEED)!=0))  // Инициализация памяти
     {
      #ifdef DEBUG
       Serial.println("$ERROR - open I2C journal, check I2C chip!");   // ошибка открытия чипа
      #endif
      err=ERR_OPEN_I2C_JOURNAL; 
      return;
     }
     
     if (checkREADY()==false) // Проверка наличия журнал
     { 
      #ifdef DEBUG
        Serial.print("I2C journal not found\n"); 
       #endif  
      Format(bufI2C);
     }   
     #ifdef DEBUG  
       else Serial.print("I2C journal is ready for use\n");
       Serial.print("Scan I2C journal ");
     #endif 
     
     for (i=0;i<JOURNAL_LEN/W5200_MAX_LEN;i++)   // поиск журнала начала и конца, для ускорения читаем по W5200_MAX_LEN байт
       {
        #ifdef DEBUG  
        Serial.print(".");
        #endif
        if (readEEPROM_I2C(I2C_JOURNAL_START+i*W5200_MAX_LEN, (byte*)&bufI2C,W5200_MAX_LEN))   
           { err=ERR_READ_I2C_JOURNAL; 
             #ifdef DEBUG
             Serial.print(errorReadI2C); 
             #endif
            break; };
        if ((ptr=(char*)memchr(bufI2C,I2C_JOURNAL_HEAD,W5200_MAX_LEN))!=NULL)       { bufferHead=i*W5200_MAX_LEN+(ptr-bufI2C); }
        if ((ptr=(char*)memchr(bufI2C,I2C_JOURNAL_TAIL,W5200_MAX_LEN))!=NULL)       { bufferTail=i*W5200_MAX_LEN+(ptr-bufI2C); }
        if ((bufferTail!=0)&&(bufferHead!=0))  break;
       }
      if  (bufferTail<bufferHead) full=true;                   // Буфер полный
      jprintf("\nSTART ----------------------\n"); 
      jprintf("Found journal I2C: total size %d bytes, head=0x%x, tail=0x%x \n",JOURNAL_LEN,bufferHead,bufferTail);
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
         Serial.println(errorWriteI2C);
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
         Serial.print(errorReadI2C);
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
	Serial.print("Formating journal I2C ");
	#endif
	for(i = 0; i < JOURNAL_LEN / W5200_MAX_LEN; i++) {
		#ifdef DEBUG
		Serial.print("*");
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
			Serial.println(errorWriteI2C);
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
//    if (m_strlen(format)>PRINTF_BUF-10) strcpy(pbuf,MessageLongString);   // Слишком длинная строка
    {
    va_start(ap, format);
    m_vsnprintf(pbuf, PRINTF_BUF, format, ap);
    va_end(ap);
    }
    Serial.print(pbuf);
  #endif 
  }
  
// Печать в консоль и журнал возвращает число записанных байт
void Journal::jprintf(const char *format, ...)
  {
      va_list ap;
//      if (m_strlen(format)>PRINTF_BUF-10) strcpy(pbuf,MessageLongString);   // Слишком длинная строка
//      else
      {
      va_start(ap, format);
      m_vsnprintf(pbuf, PRINTF_BUF, format, ap);
      va_end(ap);
      }
      #ifdef DEBUG
   //     Serial.print(full); Serial.print(">"); Serial.print(bufferHead);Serial.print(":"); Serial.print(bufferTail); Serial.print(" ");  
        Serial.print(pbuf);
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
//      if (m_strlen(format)>PRINTF_BUF-10) strcpy(pbuf,MessageLongString);   // Слишком длинная строка
//      else
      {
      va_start(ap, format);
      m_vsnprintf(pbuf, PRINTF_BUF, format, ap);
      va_end(ap);
      }
      #ifdef DEBUG
       Serial.print(pbuf);
      #endif 
    _write(pbuf);   // добавить строку в журнал
}   
  
// Печать ТОЛЬКО в журнал возвращает число записанных байт для использования в критических секциях кода
void Journal::jprintf_only(const char *format, ...)
{
  va_list ap;
//    if (m_strlen(format)>PRINTF_BUF-10) strcpy(pbuf,MessageLongString);   // Слишком длинная строка
    {
    va_start(ap, format);
    m_vsnprintf(pbuf, PRINTF_BUF, format, ap);
    va_end(ap);
    }
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
			Serial.print(errorReadI2C);
#endif
			return 0;
		}
		if(sendPacketRTOS(thread, (byte*) Socket[thread].outBuf, len, 0) == 0) return 0;        // передать пакет, при ошибке выйти
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
  Serial.print(char(c));  
  return 1;   // one byte output
  }  // end of myOutputtingClass::write
         
// Записать строку в журнал
void Journal::_write(char *dataPtr)
{
	int32_t numBytes;
	if(dataPtr == NULL || (numBytes = m_strlen(dataPtr)) == 0) return;  // Записывать нечего
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
				if(err != ERR_WRITE_I2C_JOURNAL) Serial.print(errorWriteI2C);
			#endif
			err = ERR_WRITE_I2C_JOURNAL;
		} else {
			dataPtr += n;
			numBytes -= n;
			full = 1;
			if(eepromI2C.write(I2C_JOURNAL_START, (byte*) dataPtr, numBytes + 2)) {
				err = ERR_WRITE_I2C_JOURNAL;
				#ifdef DEBUG
					Serial.print(errorWriteI2C);
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
				if(err != ERR_WRITE_I2C_JOURNAL) Serial.print(errorWriteI2C);
			#endif
			err = ERR_WRITE_I2C_JOURNAL;
		} else {
			bufferTail += numBytes;
			if(full) bufferHead = bufferTail + 1;
		}
	}
	SemaphoreGive(xI2CSemaphore);
#else   // Запись в память
	// Serial.print(">"); Serial.print(numBytes); Serial.println("<");

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
void statChart::get_PointsStr(uint16_t m, char *b)
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

void statChart::get_PointsStrSub(uint16_t m, char *b, statChart *sChart)
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
void statChart::get_PointsStrPower(uint16_t m, char *b, statChart *inChart,statChart *outChart, float kfCapacity)
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
  Heat.tempIn=5000;                    // Tемпература подачи (макс)
  Heat.tempOut=0;                      // Tемпература обратки (минимальная)
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
  Boiler.tempPID=3800;               // Целевая температура ПИД ГВС
  Boiler.tempRBOILER=3500;              // Темпеартура ГВС при котором включается бойлер и отключатся ТН
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
 if(strcmp(var,hp_TEMP1)==0) {   if ((x>=0.0)&&(x<=30.0))  {Cool.Temp1=rd(x, 100); return true;} else return false;   }else             // целевая температура в доме
 if(strcmp(var,hp_TEMP2)==0) {   if ((x>=10.0)&&(x<=50.0))  {Cool.Temp2=rd(x, 100); return true;} else return false;  }else             // целевая температура обратки
 if(strcmp(var,hp_TARGET)==0) {  if (x==0.0) {SETBIT0(Cool.flags,fTarget); return true;} else if (x==1.0) {SETBIT1(Cool.flags,fTarget); return true;} else return false; }else // что является целью значения  0 (температура в доме), 1 (температура обратки).
 if(strcmp(var,hp_DTEMP)==0) {   if ((x>=0.0)&&(x<=12.0))  {Cool.dTemp=rd(x, 100); return true;} else return false;   }else             // гистерезис целевой температуры
 if(strcmp(var,hp_HP_TIME)==0) { if ((x>=10)&&(x<=1000)) {UpdatePIDbyTime(x, Cool.pid_time, Cool.pid); Cool.pid_time=x; return true;} else return false;                                             }else             // Постоянная интегрирования времени в секундах ПИД ТН !
 if(strcmp(var,hp_HP_PRO)==0) {  if ((x>=0.0)&&(x<=32)) {Cool.pid.Kp=rd(x, 1000); return true;} else return false;    }else             // Пропорциональная составляющая ПИД ТН
#ifdef PID_FORMULA2
 if(strcmp(var,hp_HP_IN)==0) {   if ((x>=0.0)&&(x<=32))  {x *= Cool.pid_time; if(x>32.7) x=32.7; Cool.pid.Ki=rd(x, 1000); return true;} else return false; }else             // Интегральная составляющая ПИД ТН
 if(strcmp(var,hp_HP_DIF)==0) {  if ((x>=0.0)&&(x<=32))  {Cool.pid.Kd=rd(x / Cool.pid_time, 1000); return true;} else return false; }else             // Дифференциальная составляющая ПИД ТН
#else
 if(strcmp(var,hp_HP_IN)==0) {   if ((x>=0.0)&&(x<=32))  {Cool.pid.Ki=rd(x, 1000); return true;} else return false;   }else             // Интегральная составляющая ПИД ТН
 if(strcmp(var,hp_HP_DIF)==0) {  if ((x>=0.0)&&(x<=32))  {Cool.pid.Kd=rd(x, 1000); return true;} else return false;   }else             // Дифференциальная составляющая ПИД ТН
#endif
 if(strcmp(var,hp_TEMP_IN)==0) { if ((x>=0.0)&&(x<=30.0))  {Cool.tempIn=rd(x, 100); return true;} else return false;  }else             // температура подачи (минимальная)
 if(strcmp(var,hp_TEMP_OUT)==0){ if ((x>=0.0)&&(x<=35.0))  {Cool.tempOut=rd(x, 100); return true;} else return false; }else             // температура обратки (максимальная)
 if(strcmp(var,hp_D_TEMP)==0) {  if ((x>=0.0)&&(x<=40.0))  {Cool.dt=rd(x, 100); return true;} else return false;      }else             // максимальная разность температур конденсатора.
 if(strcmp(var,hp_TEMP_PID)==0){ if ((x>=0.0)&&(x<=30.0))  {Cool.tempPID=rd(x, 100); return true;} else return false; }else             // Целевая темпеартура ПИД
 if(strcmp(var,hp_WEATHER)==0) { Cool.flags = (Cool.flags & ~(1<<fWeather)) | ((x!=0)<<fWeather); return true; }else     // Использование погодозависимости
 if(strcmp(var,hp_HEAT_FLOOR)==0) { Cool.flags = (Cool.flags & ~(1<<fHeatFloor)) | ((x!=0)<<fHeatFloor); return true; }else
 if(strcmp(var,hp_SUN)==0) { Cool.flags = (Cool.flags & ~(1<<fUseSun)) | ((x!=0)<<fUseSun); return true; }else
 if(strcmp(var,hp_K_WEATHER)==0){ if ((x>=0.0)&&(x<=1.0)) {Cool.kWeather=rd(x, 1000); return true;} else return false;                             }             // Коэффициент погодозависимости
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
   if(strcmp(var,hp_K_WEATHER)==0){_ftoa(ret,(float)Cool.kWeather/1000.0,2); return ret;            }                 // Коэффициент погодозависимости
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
 if(strcmp(var,hp_TEMP1)==0) {   if ((x>=0.0)&&(x<=30.0))   {Heat.Temp1=rd(x, 100); return true;} else return false;  }else             // целевая температура в доме
 if(strcmp(var,ADD_DELTA_TEMP)==0){ if ((x>=-30)&&(x<=50))  {Heat.add_delta_temp=rd(x, 100); return true;}else return false; }else      // Добавка к целевой температуры ВНИМАНИЕ здесь еденица измерения ГРАДУСЫ
 if(strcmp(var,ADD_DELTA_HOUR)==0){ if ((x>=0)&&(x<=23))    {Heat.add_delta_hour=x; return true;} else return false; }else
 if(strcmp(var,ADD_DELTA_END_HOUR)==0){ if ((x>=0)&&(x<=23)){Heat.add_delta_end_hour=x; return true;} else return false; }else
 if(strcmp(var,hp_TEMP2)==0) {   if ((x>=10.0)&&(x<=50.0))  {Heat.Temp2=rd(x, 100); return true;} else return false;  }else             // целевая температура обратки
 if(strcmp(var,hp_TARGET)==0) {  if (x==0.0) {SETBIT0(Heat.flags,fTarget); return true;} else if (x==1.0) {SETBIT1(Heat.flags,fTarget); return true;} else return false; }else // что является целью значения  0 (температура в доме), 1 (температура обратки).
 if(strcmp(var,hp_DTEMP)==0) {   if ((x>=0.0)&&(x<=12.0))  {Heat.dTemp=rd(x, 100); return true;} else return false;   }else             // гистерезис целевой температуры
 if(strcmp(var,hp_HP_TIME)==0) { if ((x>=10)&&(x<=1000))   {UpdatePIDbyTime(x, Heat.pid_time, Heat.pid); Heat.pid_time=x; return true;} else return false; }else             // Постоянная интегрирования времени в секундах ПИД ТН !
 if(strcmp(var,hp_HP_PRO)==0) {  if ((x>=0.0)&&(x<=32.0)) {Heat.pid.Kp=rd(x, 1000); return true;} else return false;   }else             // Пропорциональная составляющая ПИД ТН
#ifdef PID_FORMULA2
 if(strcmp(var,hp_HP_IN)==0) {   if ((x>=0.0)&&(x<=32))  {x *= Heat.pid_time; if(x>32.7) x=32.7; Heat.pid.Ki=rd(x, 1000); return true;} else return false; }else  // Интегральная составляющая ПИД ТН
 if(strcmp(var,hp_HP_DIF)==0) {  if ((x>=0.0)&&(x<=32))  {Heat.pid.Kd=rd(x / Heat.pid_time, 1000); return true;} else return false; }else  // Дифференциальная составляющая ПИД ТН
#else
 if(strcmp(var,hp_HP_IN)==0) {   if ((x>=0.0)&&(x<=32.0))  {Heat.pid.Ki=rd(x, 1000); return true;} else return false;   }else             // Интегральная составляющая ПИД ТН
 if(strcmp(var,hp_HP_DIF)==0) {  if ((x>=0.0)&&(x<=32.0))  {Heat.pid.Kd=rd(x, 1000); return true;} else return false;   }else             // Дифференциальная составляющая ПИД ТН
#endif
 if(strcmp(var,hp_TEMP_IN)==0) { if ((x>=0.0)&&(x<=70.0))  {Heat.tempIn=rd(x, 100); return true;} else return false;     }else             // температура подачи (минимальная)
 if(strcmp(var,hp_TEMP_OUT)==0){ if ((x>=-10.0)&&(x<=70.0)) {Heat.tempOut=rd(x, 100); return true;} else return false;    }else             // температура обратки (максимальная)
 if(strcmp(var,hp_D_TEMP)==0) {  if ((x>=0.0)&&(x<=40.0))  {Heat.dt=rd(x, 100); return true;} else return false;  }else                  // максимальная разность температур конденсатора.
 if(strcmp(var,hp_TEMP_PID)==0){ if ((x>=10.0)&&(x<=50.0))  {Heat.tempPID=rd(x, 100); return true;} else return false;  }else             // Целевая темпеартура ПИД
 if(strcmp(var,hp_WEATHER)==0) { Heat.flags = (Heat.flags & ~(1<<fWeather)) | ((x!=0)<<fWeather); return true; }else                     // Использование погодозависимости
 if(strcmp(var,hp_HEAT_FLOOR)==0) { Heat.flags = (Heat.flags & ~(1<<fHeatFloor)) | ((x!=0)<<fHeatFloor); return true; }else
 if(strcmp(var,hp_SUN)==0) { Heat.flags = (Heat.flags & ~(1<<fUseSun)) | ((x!=0)<<fUseSun); return true; }else
 if(strcmp(var,hp_K_WEATHER)==0){ if ((x>=0.0)&&(x<=1.0)) {Heat.kWeather=rd(x, 1000); return true;} else return false;  }             // Коэффициент погодозависимости
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
   if(strcmp(var,hp_K_WEATHER)==0){_ftoa(ret,(float)Heat.kWeather/1000,2); return ret;            }                 // Коэффициент погодозависимости
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
	if(strcmp(var,boil_TOGETHER_HEAT)==0)	{ if(x) SETBIT1(Boiler.flags,fBoilerTogetherHeat); else SETBIT0(Boiler.flags,fBoilerTogetherHeat); return true;} else
	if(strcmp(var,boil_fBoilerPID)==0)	    { if(x) SETBIT1(Boiler.flags,fBoilerPID); else SETBIT0(Boiler.flags,fBoilerPID); return true;} else
	if(strcmp(var,boil_TURBO_BOILER)==0)	{ if(x) SETBIT1(Boiler.flags,fTurboBoiler); else SETBIT0(Boiler.flags,fTurboBoiler); return true;} else
	if(strcmp(var,boil_SALLMONELA)==0)		{ if(x) { SETBIT1(Boiler.flags,fSalmonella); HP.sTemp[TBOILER].set_maxTemp(SALLMONELA_TEMP+300); }
												else { SETBIT0(Boiler.flags,fSalmonella); HP.sTemp[TBOILER].set_maxTemp(MAXTEMP[TBOILER]); } return true;} else // Изменение максимальной температуры при включенном режиме сальмонелла
	if(strcmp(var,boil_CIRCULATION)==0)		{ if(x) SETBIT1(Boiler.flags,fCirculation); else SETBIT0(Boiler.flags,fCirculation); return true;} else
	if(strcmp(var,boil_TEMP_TARGET)==0)		{ if((x>=5)&&(x<=90)) {Boiler.TempTarget=rd(x, 100); return true;} else return false; } else  // Целевая температура бойлера
	if(strcmp(var,ADD_DELTA_TEMP)==0)		{ if((x>=-50)&&(x<=50)) {Boiler.add_delta_temp=rd(x, 100); return true;}else return false; } else  // Добавка к целевой температуры ВНИМАНИЕ здесь еденица измерения ГРАДУСЫ
	if(strcmp(var,ADD_DELTA_HOUR)==0)		{ if((x>=0)&&(x<=23)) {Boiler.add_delta_hour=x; return true;} else return false; } else      // Начальный Час добавки температуры к установке бойлера
	if(strcmp(var,ADD_DELTA_END_HOUR)==0)	{ if((x>=0)&&(x<=23)){Boiler.add_delta_end_hour=x; return true;} else return false; } else   // Конечный Час добавки температуры к установке
	if(strcmp(var,boil_DTARGET)==0)			{ if((x>=1)&&(x<=20)) {Boiler.dTemp=rd(x, 100); return true;} else return false; } else      // гистерезис целевой температуры
	if(strcmp(var,boil_TEMP_MAX)==0)		{ if((x>=20)&&(x<=70)) {Boiler.tempIn=rd(x, 100); return true;} else return false; } else    // Tемпература подачи максимальная
	if(strcmp(var,boil_CIRCUL_WORK)==0) 	{ if((x>=0)&&(x<=60)){Boiler.Circul_Work=60*x; return true;} else return false;} else         // Время  работы насоса ГВС секунды (fCirculation)
	if(strcmp(var,boil_CIRCUL_PAUSE)==0)	{ if((x>=0)&&(x<=60)){Boiler.Circul_Pause=60*x; return true;} else return false;} else        // Пауза в работе насоса ГВС  секунды (fCirculation)
	if(strcmp(var,boil_RESET_HEAT)==0)		{ if(x) SETBIT1(Boiler.flags,fResetHeat); else SETBIT0(Boiler.flags,fResetHeat); return true;} else // флаг Сброса лишнего тепла в СО
	if(strcmp(var,boil_RESET_TIME)==0)		{ if((x>=1)&&(x<=10000)) {Boiler.Reset_Time=x; return true;} else return false; } else        // время сброса излишков тепла в секундах (fResetHeat)
	if(strcmp(var,boil_BOIL_TIME)==0)		{ if((x>=2)&&(x<=1000)) {Boiler.pid_time=x; return true;} else return false; } else             // Постоянная интегрирования времени в секундах ПИД ГВС
	if(strcmp(var,boil_BOIL_PRO)==0)		{ if((x>=0.0)&&(x<=32)) {Boiler.pid.Kp=rd(x, 1000); return true;} else return false; } else  // Пропорциональная составляющая ПИД ГВС
	if(strcmp(var,boil_BOIL_IN)==0)			{ if((x>=0.0)&&(x<=32)) {Boiler.pid.Ki=rd(x, 1000); return true;} else return false; } else   // Интегральная составляющая ПИД ГВС
	if(strcmp(var,boil_BOIL_DIF)==0)		{ if((x>=0.0)&&(x<=32)) {Boiler.pid.Kd=rd(x, 1000); return true;} else return false; } else   // Дифференциальная составляющая ПИД ГВС
	if(strcmp(var,boil_BOIL_TEMP)==0)		{ if((x>=30.0)&&(x<=70)) {Boiler.tempPID=rd(x, 100); return true;} else return false; } else   // Целевая темпеартура ПИД ГВС
	if(strcmp(var,boil_ADD_HEATING)==0)		{ if(x) SETBIT1(Boiler.flags,fAddHeating); else SETBIT0(Boiler.flags,fAddHeating); return true;} else  // флаг использования тена для догрева ГВС
	if(strcmp(var,boil_TEMP_RBOILER)==0)	{ if((x>=0.0)&&(x<=60.0))  {Boiler.tempRBOILER=rd(x, 100); return true;} else return false;} else   // температура включения догрева бойлера
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
 if(strcmp(var,boil_BOIL_PRO)==0){        _ftoa(ret,(float)Boiler.pid.Kp/1000,3); return ret;       }else                            // Пропорциональная составляющая ПИД ГВС
 if(strcmp(var,boil_BOIL_IN)==0){         _ftoa(ret,(float)Boiler.pid.Ki/1000,3); return ret;       }else                            // Интегральная составляющая ПИД ГВС
 if(strcmp(var,boil_BOIL_DIF)==0){        _ftoa(ret,(float)Boiler.pid.Kd/1000,3); return ret;       }else                            // Дифференциальная составляющая ПИД ГВС
 if(strcmp(var,boil_BOIL_TEMP)==0){       _ftoa(ret,(float)Boiler.tempPID/100,1); return ret;   }else                            // Целевая темпеартура ПИД ГВС
 if(strcmp(var,boil_ADD_HEATING)==0){     if(GETBIT(Boiler.flags,fAddHeating)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero); }else   // флаг использования тена для догрева ГВС
 if(strcmp(var,boil_TEMP_RBOILER)==0){    _ftoa(ret,(float)Boiler.tempRBOILER/100,1); return ret;       }else                            // температура включения догрева бойлера
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
  
  // читаем основные данные
  if (readEEPROM_I2C(adr, (byte*)&SaveON, sizeof(SaveON))) { set_Error(ERR_LOAD_PROFILE,(char*)nameHeatPump); return ERR_LOAD_PROFILE;}  adr=adr+sizeof(SaveON);     // прочитать состояние ТН
  if (readEEPROM_I2C(adr, (byte*)&Cool, sizeof(Cool))) { set_Error(ERR_LOAD_PROFILE,(char*)nameHeatPump); return ERR_LOAD_PROFILE;}  adr=adr+sizeof(Cool);           // прочитать настройки охлаждения
  if (readEEPROM_I2C(adr, (byte*)&Heat, sizeof(Heat))) { set_Error(ERR_LOAD_PROFILE,(char*)nameHeatPump); return ERR_LOAD_PROFILE;}  adr=adr+sizeof(Heat);           // прочитать настройки отопления
  if (readEEPROM_I2C(adr, (byte*)&Boiler, sizeof(Boiler))) { set_Error(ERR_LOAD_PROFILE,(char*)nameHeatPump); return ERR_LOAD_PROFILE;}  adr=adr+sizeof(Boiler);     // прочитать настройки ГВС
 
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
	uint8_t x;
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
		x = atoi(c);
		if(x == 0 || x > I2C_PROFIL_NUM) return false; // не верный номер профиля
		dataProfile.id = x - 1;
		return true;
	} else if(strcmp(var, prof_NOTE_PROFILE) == 0) {
		urldecode(dataProfile.note, c, sizeof(dataProfile.note));
		return true;
	} else if(strcmp(var, prof_DATE_PROFILE) == 0) {
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
int8_t Profile::set_list( int8_t num)
 {
  uint8_t xx, i,j=0;
  int32_t adr;
  
  for (i=0;i<I2C_PROFIL_NUM;i++)                                                                // перебор по всем профилям
  {
    adr=I2C_PROFILE_EEPROM+ get_sizeProfile()*i;                                                // вычислить адрес начала профиля
    if (readEEPROM_I2C(adr, (byte*)&xx, sizeof(magic))) { continue; }                                 // прочитать заголовок
    if (xx!=0xaa)  {  continue; }                                                               // Заголовок не верен, данных нет, пропускаем чтение профиля это не ошибка
    //Serial.print("xx==0xaa ");Serial.println(i);
    adr=adr+sizeof(magic)+sizeof(crc16);                                                        // вычислить адрес начала данных
    if (readEEPROM_I2C(adr, (byte*)&temp_prof, sizeof(temp_prof))) { continue; }                          // прочитать данные
    if ((GETBIT(temp_prof.flags,fEnabled))&&(temp_prof.id==i))                                            // Если разрешено использовать профиль  в  списке, и считанный номер совпадает с текущим (это должно быть всегда)
     {
      if (num==j) {load(i);  break; }                                                           // надо проверить не выбран ли он , и если совпало загрузить профиль и выход
      else j++;                                                                                 // увеличить счетчик - двигаемся по списку
     } 
   }
//update_list(num); // <-- вызывается в load()!                                                 // обновить список
return dataProfile.id;                                                                          // вернуть текущий профиль
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

// ---------------------------------------------------------------------------------
//  MQTT клиент ТН -----------------------------------------------------------------
// ---------------------------------------------------------------------------------
#ifdef MQTT
// инициализация MQTT
void clientMQTT::initMQTT()
{
  
 IPAddress zeroIP(0,0,0,0);   
 mqttSettintg.flags=0x00;                                 // Бинарные флага настроек
 SETBIT0(mqttSettintg.flags,fMqttUse);                    // флаг использования MQTT
 SETBIT0(mqttSettintg.flags,fTSUse);                      // флаг использования ThingSpeak
 SETBIT0(mqttSettintg.flags,fMqttBig);                    // флаг отправки ДОПОЛНИТЕЛЬНЫХ данных на MQTT
 SETBIT0(mqttSettintg.flags,fMqttSDM120);                 // флаг отправки данных электросчетчика на MQTT
 SETBIT0(mqttSettintg.flags,fMqttFC);                     // флаг отправки данных инвертора на MQTT
 SETBIT0(mqttSettintg.flags,fMqttCOP);                    // флаг отправки данных COP на MQTT
 SETBIT0(mqttSettintg.flags,fNarodMonUse);                // флаг отправки данных на народный мониторинг
 SETBIT0(mqttSettintg.flags,fNarodMonBig);                // флаг отправки данных на народный мониторинг расширенную версию
 mqttSettintg.ttime=DEFAULT_TIME_MQTT;                    // период отправки на сервер в сек. 10...60000
 strcpy(mqttSettintg.mqtt_server,DEFAULT_ADR_MQTT);       // Адрес сервера
 mqttSettintg.mqtt_serverIP=zeroIP;                       // IP Адрес сервера
  
 mqttSettintg.mqtt_port=DEFAULT_PORT_MQTT;                // Адрес порта сервера
 strcpy(mqttSettintg.mqtt_login,"admin");                 // логин сервера
 strcpy(mqttSettintg.mqtt_password,"admin");              // пароль сервера
 strcpy(mqttSettintg.mqtt_id,"HeatPump");                 // Идентификатор клиента на MQTT сервере
 // NARMON
 strcpy(mqttSettintg.narodMon_server,DEFAULT_ADR_NARMON); // Адрес сервера народного мониторинга
 mqttSettintg.narodMon_serverIP=zeroIP;                   // IP Адрес сервера народного мониторинга
 mqttSettintg.narodMon_port=DEFAULT_PORT_MQTT;            // Адрес порта сервера народного мониторинга
 strcpy(mqttSettintg.narodMon_login,"login");             // логин сервера народного мониторинга
 strcpy(mqttSettintg.narodMon_password,"1234");           // пароль сервера народного мониторинга
 strcpy(mqttSettintg.narodMon_id,"HeatPump");             // Идентификатор клиента на MQTT сервере
  
 dnsUpadateMQTT=false;                                     // Флаг необходимости обновления через dns IP адреса для MQTT
 dnsUpadateNARMON=false;                                   // Флаг необходимости обновления через dns IP адреса для NARMON
 w5200_MQTT.setSock(W5200_SOCK_SYS);                       // Установить сокет с которым рабоатем
// w5200_MQTT.setCallback(callback);  
}

// Установить параметр Уведомления из строки
 boolean clientMQTT::set_paramMQTT(char *var, char *c)         
{
float x;	
   	if(strcmp(var, mqtt_USE_TS)==0){
          if (strcmp(c,cZero)==0)      { SETBIT0(mqttSettintg.flags,fTSUse); return true;}          // флаг использования ThingSpeak
          else if (strcmp(c,cOne)==0) { SETBIT1(mqttSettintg.flags,fTSUse);  return true;}
          else return false;  
   	} else if(strcmp(var, mqtt_USE_MQTT)==0){
   	      if (strcmp(c,cZero)==0)      { SETBIT0(mqttSettintg.flags,fMqttUse); return true;}          // флаг использования MQTT
          else if (strcmp(c,cOne)==0) { SETBIT1(mqttSettintg.flags,fMqttUse);  return true;}
          else return false;  
  	} else if(strcmp(var, mqtt_BIG_MQTT)==0){
          if (strcmp(c,cZero)==0)      { SETBIT0(mqttSettintg.flags,fMqttBig); return true;}          // флаг отправки ДОПОЛНИТЕЛЬНЫХ данных на MQTT
          else if (strcmp(c,cOne)==0) { SETBIT1(mqttSettintg.flags,fMqttBig);  return true;}
          else return false;  
   	} else if(strcmp(var, mqtt_SDM_MQTT)==0){
   		  if (strcmp(c,cZero)==0)      { SETBIT0(mqttSettintg.flags,fMqttSDM120); return true;}       //  флаг отправки данных электросчетчика на MQTT
          else if (strcmp(c,cOne)==0) { SETBIT1(mqttSettintg.flags,fMqttSDM120);  return true;}
          else return false;  
   	} else if(strcmp(var, mqtt_FC_MQTT)==0){
          if (strcmp(c,cZero)==0)       { SETBIT0(mqttSettintg.flags,fMqttFC); return true;}           //  флаг отправки данных инвертора на MQTT
          else if (strcmp(c,cOne)==0) { SETBIT1(mqttSettintg.flags,fMqttFC);  return true;}
          else return false;  
    } else if(strcmp(var, mqtt_COP_MQTT)==0){
          if (strcmp(c,cZero)==0)      { SETBIT0(mqttSettintg.flags,fMqttCOP); return true;}          // флаг отправки данных COP на MQTT
          else if (strcmp(c,cOne)==0) { SETBIT1(mqttSettintg.flags,fMqttCOP);  return true;}
          else return false;  
    } else if(strcmp(var, mqtt_TIME_MQTT)==0){
          x=my_atof(c);                                                                             // ПРИХОДЯТ МИНУТЫ храним СЕКУНДЫ период отправки на сервер в сек. 10...60000
          if (x==ATOF_ERROR) return   false;
          else if((x<1)||(x>=1000)) return   false;    
          else mqttSettintg.ttime=(int)x*60; return true;
    } else if(strcmp(var, mqtt_ADR_MQTT)==0){
          if(m_strlen(c)==0) return false;                                                            // Адрес сервера  пустая строка
          if(m_strlen(c)>sizeof(mqttSettintg.mqtt_server)-1) return false;                            // слишком длиная строка
           else // ок сохраняем
            {  
            strcpy(mqttSettintg.mqtt_server,c); 
            dnsUpadateMQTT=true;
            return true;  
            }                        
    } else if(strcmp(var, mqtt_IP_MQTT)==0){
           return true;                                                                        // IP Адрес сервера,  Только на чтение. описание первого параметра для отправки смс
    } else if(strcmp(var, mqtt_PORT_MQTT)==0){
           x=my_atof(c);                                                                             // Порт сервера
	       if (x==ATOF_ERROR) return   false;
	       else if((x<=1)||(x>=65535-1)) return   false;   
	       else mqttSettintg.mqtt_port=(int)x; return true;
    } else if(strcmp(var, mqtt_LOGIN_MQTT)==0){
           if(m_strlen(c)==0) return false;                                                            // логин сервера
           if(m_strlen(c)>sizeof(mqttSettintg.mqtt_login)-1) return false;
           else { strcpy(mqttSettintg.mqtt_login,c); return true;  }                         
    } else if(strcmp(var, mqtt_PASSWORD_MQTT)==0){
           if(m_strlen(c)==0) return false;                                                            // пароль сервера
	       if(m_strlen(c)>sizeof(mqttSettintg.mqtt_password)-1) return false;
	       else { strcpy(mqttSettintg.mqtt_password,c); return true;  }                         
	 } else if(strcmp(var, mqtt_ID_MQTT)==0){
	       if(m_strlen(c)==0) return false;                                                            // дентификатор клиента на MQTT сервере
           if(m_strlen(c)>sizeof(mqttSettintg.mqtt_id)-1) return false;
           else { strcpy(mqttSettintg.mqtt_id,c); return true;  }                                      // --------------------- NARMON -------------------------            
    } else if(strcmp(var, mqtt_USE_NARMON)==0){
           if (strcmp(c,cZero)==0)      { SETBIT0(mqttSettintg.flags,fNarodMonUse); return true;}     // флаг отправки данных на народный мониторинг
           else if (strcmp(c,cOne)==0) { SETBIT1(mqttSettintg.flags,fNarodMonUse);  return true;}
           else return false; 
    } else if(strcmp(var, mqtt_BIG_NARMON)==0){  if (strcmp(c,cZero)==0)       { SETBIT0(mqttSettintg.flags,fNarodMonBig); return true;}    // флаг отправки данных на народный мониторинг расширенная версия
           else if (strcmp(c,cOne)==0) { SETBIT1(mqttSettintg.flags,fNarodMonBig);  return true;}
           else return false;  
    } else if(strcmp(var, mqtt_ADR_NARMON)==0){  
       	   if(m_strlen(c)==0) return false;                                                             // Адрес сервера  пустая строка
	       if(m_strlen(c)>sizeof(mqttSettintg.narodMon_server)-1) return false;                         // слишком длиная строка
	       else // ок сохраняем
	            {  
	            strcpy(mqttSettintg.narodMon_server,c); 
	            dnsUpadateNARMON=true;
	            return true;  
	            }                        
    } else if(strcmp(var, mqtt_IP_NARMON)==0){  
           return true; 
    } else if(strcmp(var, mqtt_PORT_NARMON)==0){  
           x=my_atof(c);                                                                             // Порт сервера
           if (x==ATOF_ERROR) return   false;
           else if((x<=1)||(x>=65535-1)) return   false;   
           else mqttSettintg.narodMon_port=(int)x; return true;
     } else if(strcmp(var, mqtt_LOGIN_NARMON)==0){
           if(m_strlen(c)==0) return false;                                                            // логин сервера
           if(m_strlen(c)>sizeof(mqttSettintg.narodMon_login)-1) return false;
           else { strcpy(mqttSettintg.narodMon_login,c); return true;  }                         
      } else if(strcmp(var, mqtt_PASSWORD_NARMON)==0){  if(m_strlen(c)==0) return false;                                                          // пароль сервера
           if(m_strlen(c)>sizeof(mqttSettintg.narodMon_password)-1) return false;
           else { strcpy(mqttSettintg.narodMon_password,c); return true;  }                         
      } else if(strcmp(var, mqtt_ID_NARMON)==0){
         	if(m_strlen(c)==0) return false;                                                            // дентификатор клиента на MQTT сервере
            if(m_strlen(c)>sizeof(mqttSettintg.narodMon_id)-1) return false;
            else { strcpy(mqttSettintg.narodMon_id,c); return true;  }   
       }                          
  return false;                        
}

 // Получить параметр из строки по имени var, результат ДОБАВЛЯЕТСЯ в строку ret
char*   clientMQTT::get_paramMQTT(char *var, char *ret)
{
    if(strcmp(var, mqtt_USE_TS)==0){if (GETBIT(mqttSettintg.flags,fTSUse))      return  strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero);} else     // флаг использования ThingSpeak
    if(strcmp(var, mqtt_USE_MQTT)==0){  if (GETBIT(mqttSettintg.flags,fMqttUse))    return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero);} else       // флаг использования MQTT
    if(strcmp(var, mqtt_BIG_MQTT)==0){  if (GETBIT(mqttSettintg.flags,fMqttBig))    return strcat(ret,(char*)cOne); else return  strcat(ret,(char*)cZero);} else      // флаг отправки ДОПОЛНИТЕЛЬНЫХ данных на MQTT
    if(strcmp(var, mqtt_SDM_MQTT)==0){   if (GETBIT(mqttSettintg.flags,fMqttSDM120)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero);} else     // флаг отправки данных электросчетчика на MQTT
    if(strcmp(var, mqtt_FC_MQTT)==0){   if (GETBIT(mqttSettintg.flags,fMqttFC))     return  strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero);} else      // флаг отправки данных инвертора на MQTT
    if(strcmp(var, mqtt_COP_MQTT)==0){    if (GETBIT(mqttSettintg.flags,fMqttCOP))    return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero);} else     // флаг отправки данных COP на MQTT
    if(strcmp(var, mqtt_TIME_MQTT)==0){  return _itoa(mqttSettintg.ttime/60,ret);                                                    } else     // ПРИХОДЯТ МИНУТЫ храним СЕКУНДЫ период отправки на сервер в сек. 10...60000
    if(strcmp(var, mqtt_ADR_MQTT)==0){  return strcat(ret,mqttSettintg.mqtt_server);                                                          } else     // Адрес сервера
    if(strcmp(var, mqtt_IP_MQTT)==0){    return strcat(ret,IPAddress2String(mqttSettintg.mqtt_serverIP));                                      } else      // IP Адрес сервера,  Только на чтение. описание первого параметра для отправки смс
    if(strcmp(var, mqtt_PORT_MQTT)==0){    return _itoa(mqttSettintg.mqtt_port,ret);                                                  } else      // Порт сервера
    if(strcmp(var, mqtt_LOGIN_MQTT)==0){  return strcat(ret,mqttSettintg.mqtt_login);                                                           } else     // логин сервера
    if(strcmp(var, mqtt_PASSWORD_MQTT)==0){  return strcat(ret,mqttSettintg.mqtt_password);                                                     } else     // пароль сервера
    if(strcmp(var, mqtt_ID_MQTT)==0){      return strcat(ret,mqttSettintg.mqtt_id);                                                         } else     // дентификатор клиента на MQTT сервере
    // ----------------------NARMON -------------------------
    if(strcmp(var, mqtt_USE_NARMON)==0){if (GETBIT(mqttSettintg.flags,fNarodMonUse)) return  strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero);} else      // флаг отправки данных на народный мониторинг
    if(strcmp(var, mqtt_BIG_NARMON)==0){ if (GETBIT(mqttSettintg.flags,fNarodMonBig)) return strcat(ret,(char*)cOne); else return strcat(ret,(char*)cZero); } else      // флаг отправки данных на народный мониторинг  расширенная версия
    if(strcmp(var, mqtt_ADR_NARMON)==0){ return  strcat(ret,mqttSettintg.narodMon_server);                                                      } else   // Адрес сервера народного мониторинга
    if(strcmp(var, mqtt_IP_NARMON)==0){return strcat(ret,IPAddress2String(mqttSettintg.narodMon_serverIP));                                 } else    // IP Адрес сервера народного мониторинга,
    if(strcmp(var, mqtt_PORT_NARMON)==0){ return _itoa(mqttSettintg.narodMon_port,ret);                                               } else   // Порт сервера народного мониторинга
    if(strcmp(var, mqtt_LOGIN_NARMON)==0){ return strcat(ret,mqttSettintg.narodMon_login);                                                      } else      // логин сервера народного мониторинга
    if(strcmp(var, mqtt_PASSWORD_NARMON)==0){return strcat(ret,mqttSettintg.narodMon_password);                                                  } else      // пароль сервера  народного мониторинга
    if(strcmp(var, mqtt_ID_NARMON)==0){ return strcat(ret,mqttSettintg.narodMon_id);                                                          } else     // дентификатор клиента на MQTT сервере народного мониторинга
    return (char*)cError;                        
}


// Обновление IP адресов MQTT через dns
// возвращает true обновление не было false - прошло обновление или ошибка
boolean clientMQTT::dnsUpdate()
{ boolean ret=true;
  if  (dnsUpadateMQTT) //надо обновлятся
  {
     dnsUpadateMQTT=false;   // сбросить флаг
     if(SemaphoreTake(xWebThreadSemaphore,(W5200_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE)  {return false;}  // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
     ret=check_address(mqttSettintg.mqtt_server,mqttSettintg.mqtt_serverIP);                                  // Получить адрес IP через DNS
     SemaphoreGive(xWebThreadSemaphore);
     _delay(20);
     ret=false;
  }
  if  (dnsUpadateNARMON) //надо обновлятся
  {
     dnsUpadateNARMON=false;   // сбросить флаг
     if(SemaphoreTake(xWebThreadSemaphore,(W5200_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE)  {return false;}  // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
     ret=check_address(mqttSettintg.narodMon_server,mqttSettintg.narodMon_serverIP);                          // Получить адрес IP через DNS
     SemaphoreGive(xWebThreadSemaphore);
     _delay(20);
     ret=false;
   }
  return ret; 
}
// Обновление IP адресов серверов через dns при СТАРТЕ!!!  вачдог сбрасывается т.к. может сеть не рабоать
boolean clientMQTT::dnsUpdateStart()
{    boolean ret=false;
     IPAddress zeroIP(0,0,0,0);  
     dnsUpadateMQTT=false;
     WDT_Restart(WDT);                                                               // Сбросить вачдог
     if(mqttSettintg.mqtt_serverIP==zeroIP) ret=check_address(mqttSettintg.mqtt_server,mqttSettintg.mqtt_serverIP);     // если адрес нулевой Получить адрес IP через DNS
     dnsUpadateNARMON=false;
     WDT_Restart(WDT);                                                              // Сбросить вачдог
     if(mqttSettintg.narodMon_serverIP==zeroIP) ret=check_address(mqttSettintg.narodMon_server,mqttSettintg.narodMon_serverIP);              //  если адрес нулевой Получить адрес IP через DNS
     return ret;                                               
} 

// Записать настройки в eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t clientMQTT::save(int32_t adr)    
{
 if (writeEEPROM_I2C(adr, (byte*)&mqttSettintg, sizeof(mqttSettintg))) { set_Error(ERR_SAVE_EEPROM,(char*)nameHeatPump); return ERR_SAVE_EEPROM;}  adr=adr+sizeof(mqttSettintg);           // записать параметры MQTT
 return adr;   
}
// Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t clientMQTT::load(int32_t adr)
{
  if (readEEPROM_I2C(adr, (byte*)&mqttSettintg, sizeof(mqttSettintg))) { set_Error(ERR_LOAD_EEPROM,(char*)nameHeatPump); return ERR_LOAD_EEPROM;}  adr=adr+sizeof(mqttSettintg);           // прочитать параметры MQTT
  return adr;                                       
}
// Считать настройки из буфера на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t clientMQTT::loadFromBuf(int32_t adr, byte *buf)
{
 memcpy((byte*)&mqttSettintg,buf+adr,sizeof(mqttSettintg)); adr=adr+sizeof(mqttSettintg); 
 return adr;                         
}
// Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
uint16_t clientMQTT::get_crc16(uint16_t crc)
{
  uint16_t i;
  for(i=0;i<sizeof(mqttSettintg);i++) crc=_crc16(crc,*((byte*)&mqttSettintg+i));   // CRC16 настройки MQTT
  return crc;                                    
}

// Еще одна ошибка отправки на MQTT параметр тип сервера true - народный мониторинг false MQTT
uint16_t clientMQTT::updateErrMQTT(boolean NM)                
{
 if (NM)
     {
     numErrNARMON++;
     if (numErrNARMON>=MQTT_NUM_ERR_OFF)
         {
         SETBIT0(mqttSettintg.flags,fNarodMonUse);
         journal.jprintf(pP_TIME,(char*)BlockService,get_narodMon_server());   
         numErrNARMON=0;// сбросить счетчик
         }
      return numErrNARMON;
     }
 else 
     {
     numErrMQTT++;
     if (numErrMQTT>=MQTT_NUM_ERR_OFF)
         {
         SETBIT0(mqttSettintg.flags,fMqttUse);
         journal.jprintf(pP_TIME,(char*)BlockService,get_mqtt_server());    
         numErrMQTT=0;// сбросить счетчик
         }
      return numErrMQTT;
     }
return 0;
}

/*
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
} */

// Внутренная функция послать один топик, возврат удачно или нет послан топик, при не удаче запись в журнал
// t - название топика
// p - значение топика
// NM - true народный мониторинг, false обычный сервер
// debug - выводить отладочные сообщения
// link_close  = true - по завершению закрывать связь  false по завершению не закрывать связь (отсылка нескольких топиков)
boolean clientMQTT::sendTopic(char * t,char *p,boolean NM, boolean debug, boolean link_close)
{
  TYPE_STATE_MQTT state; 
  IPAddress tempIP;
  uint8_t i;
  if(SemaphoreTake(xWebThreadSemaphore,(W5200_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE) {journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexWebThreadBuzy);return false;} // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
 if (NM) state=stateNARMON; else state=stateMQTT;
 
 if (!linkStatusWiznet(false)) {journal.jprintf((char*)"sendTopic:no link.\n");  SemaphoreGive(xWebThreadSemaphore); return false;}// Нет связи выходим
 
 //debug=true;    // выводить отладку
 if (!w5200_MQTT.connected())   // если нет соединения то возможно требуется реанимация
  {
     switch (state) // Реанимация варианты
     {
          case pMQTT_OK:  break;  // Предыдущая отправка была удачна
          case pMQTT_SOCK:  // Попытка реанимировать сокет
                          HP.num_resMQTT++;   
                          if(closeSockSys(true))  journal.jprintf((char*)ResSock);  
                          break;         
          case pMQTT_DNS: // Предыдущая отправка была НЕ удачна
                          HP.num_resMQTT++;  
                          if(closeSockSys(true))   // НЕУДАЧА Пытаемся реанимировать сокет
                          { 
                           journal.jprintf((char*)ResDHCP);
                          _delay(50);
                          // Пытаемся еще раз узнать адрес через ДНС, возмлжно он поменялось, если поменялся то меняем не сохраняем настройки
                          if (NM) {check_address(get_narodMon_server(),tempIP);if (tempIP!=get_narodMon_serverIP()){ set_narodMon_serverIP(tempIP);journal.jprintf((char*)ChangeIP,get_narodMon_server());}}
                          else    {check_address(get_mqtt_server(),tempIP);  if (tempIP!=get_mqtt_serverIP())    { set_mqtt_serverIP(tempIP);    journal.jprintf((char*)ChangeIP,get_mqtt_server());}}
                           _delay(50);
                          }
                          break;
          case pMQTT_RES: // Предудущая отправка - был сброс ДНС адреса теперь НАДО ЧИП СБРОСИТЬ
                         HP.num_resMQTT++; 
                         journal.jprintf((char*)ResChip);  
                         initW5200(false);                                  // Инициализация сети без вывода инфы в консоль
                         for (i=0;i<W5200_THREARD;i++) SETBIT1(Socket[i].flags,fABORT_SOCK);  // Признак инициализации сокета, надо прерывать передачу в сервере
                          _delay(50);
                         break;
     }
 } // if (!connect(NM)) 
// Попытка соедиения - и определение что делаем дальше
//if (debug){journal.jprintf((char*)">"); ShowSockRegisters(W5200_SOCK_SYS);}// выводим состояние регистров ЕСЛИ ОТЛАДКА
         if (connect(NM))                                        // Попытка соедиениея
          {
           w5200_MQTT.publish(t,p);                              // Посылка данных топика
           if (link_close) w5200_MQTT.disconnect();              // отсоединение если надо
           if (debug) ShowSockRegisters(W5200_SOCK_SYS);         // выводим состояние регистров ЕСЛИ ОТЛАДКА
         
 //          if(NM)    // Чтение информации
 //          {
 //            w5200_MQTT.subscribe("pav2000/HeatPump/");  //  pav2000/HeatPump/
 //            w5200_MQTT.loop(); 
 //          }
           SemaphoreGive(xWebThreadSemaphore);
           resetErrMQTT(NM);                                     // Удачно - сбрасываем счетчик ошибок
           updateState(NM,pMQTT_OK);   
           return true;      
          }
          else                                  // соединениe не установлено
          { 
          ShowSockRegisters(W5200_SOCK_SYS);    // выводим состояние регистров ВСЕГДА
          closeSockSys(true);
          SemaphoreGive(xWebThreadSemaphore);
          journal.jprintf(" %s\n",get_stateMQTT()); 
          switch(state)  // устанавливаем метод реанимации
               {
               case pMQTT_OK:   updateState(NM,pMQTT_SOCK); break;        
               case pMQTT_SOCK: updateState(NM,pMQTT_DNS);  break;   
               case pMQTT_DNS:  updateState(NM,pMQTT_RES);  break;   
               case pMQTT_RES:  updateState(NM,pMQTT_OK);   break;        
               }
           updateErrMQTT(NM);                             // Добавляем ошибку
           return false; 
          }   
         SemaphoreGive(xWebThreadSemaphore);
 return false; 
}   
 // Попытаться соеденится с сервером МЮТЕКС уже захвачен!!
boolean clientMQTT::connect(boolean NM)
{
if (w5200_MQTT.connected()) return true;  // соеденино выходим
w5200_MQTT.setSock(W5200_SOCK_SYS);       // Установить сокет с которым рабоатем
if (NM) // В зависимости от того с кем надо соединяться
         {
         w5200_MQTT.setServer(mqttSettintg.narodMon_serverIP,mqttSettintg.narodMon_port);                       // установить параметры Народного мониторинга
         w5200_MQTT.connect(HP.get_netMAC(), mqttSettintg.narodMon_login,mqttSettintg.narodMon_password);       // Соедиенение с народным мониторингом
         #ifdef MQTT_REPEAT            // разрешен повтор соединения
         if (!w5200_MQTT.connected()) { _delay(20); ShowSockRegisters(W5200_SOCK_SYS); w5200_MQTT.connect(HP.get_netMAC(), mqttSettintg.narodMon_login,mqttSettintg.narodMon_password); } // вторая попытка
         #endif
         }
         else 
         {
          if(HP.clMQTT.get_TSUse()) 
               {
                w5200_MQTT.setServer(mqttSettintg.mqtt_serverIP,mqttSettintg.mqtt_port);
                w5200_MQTT.connect(mqttSettintg.mqtt_id);   //  установить параметры сервера (используется обращение через ДНС) Соедиенение с ThingSpeak
                #ifdef MQTT_REPEAT            // разрешен повтор соединения
                if (!w5200_MQTT.connected()) { _delay(20); ShowSockRegisters(W5200_SOCK_SYS); w5200_MQTT.connect(mqttSettintg.mqtt_id); }  // вторая попытка
                #endif
               }       
               else  
               {
                w5200_MQTT.setServer(mqttSettintg.mqtt_serverIP,mqttSettintg.mqtt_port);
                w5200_MQTT.connect(mqttSettintg.mqtt_id,mqttSettintg.mqtt_login,mqttSettintg.mqtt_password); //  установить параметры сервера Соедиенение с сервером MQTT
                #ifdef MQTT_REPEAT            // разрешен повтор соединения
                if (!w5200_MQTT.connected()) { _delay(20); ShowSockRegisters(W5200_SOCK_SYS); w5200_MQTT.connect(mqttSettintg.mqtt_id,mqttSettintg.mqtt_login,mqttSettintg.mqtt_password);}   // вторая попытка
                #endif
               }
         }
return  w5200_MQTT.connected();                  
}

// Попытаться сбросить системный сокет МЮТЕКС уже захвачен!!
const char* NoClosed={"Problem: W5200_SOCK_SYS sock is not closed, SR=%s\n"};
boolean clientMQTT::closeSockSys(boolean debug)
{
uint8_t x, sr;
sr = W5100.readSnSR(W5200_SOCK_SYS);                                                   // Прочитать статус сокета
if(sr==SnSR::CLOSED) return true;                                                      // Сокет уже закрыт выходим
if ((sr==SnSR::ESTABLISHED)||(sr==SnSR::CLOSE_WAIT)) { W5100.execCmdSn(W5200_SOCK_SYS, Sock_DISCON);_delay(20);}// если надо разорать соедиение

W5100.execCmdSn(W5200_SOCK_SYS, Sock_CLOSE);       // закрыть сокет
W5100.writeSnIR(W5200_SOCK_SYS, 0xFF);             // сбросить прерывания
_delay(20);
if((x=W5100.readSnSR(W5200_SOCK_SYS))!=SnSR::CLOSED) { if(debug) journal.jprintf((char*)NoClosed,byteToHex(x)); return false;}   // проверка на закрытие сокета
return true; 
}

#endif
