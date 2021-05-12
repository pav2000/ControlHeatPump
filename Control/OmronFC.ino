/*
 * Copyright (c) 2016-2020 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
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
#include "OmronFC.h"

#ifndef FC_VACON
// ------------------------------------------------------------------------------------------
// ЧАСТОТНЫЙ ПРЕОБРАЗОВАТЕЛЬ ТОЛЬКО ОДНА ШТУКА ВСЕГДА (не массив) ---------------------------
// Инициализация класса                                                   
int8_t devOmronMX2::initFC()
{ 

  err=OK;                           // ошибка частотника (работа) при ошибке останов ТН
  numErr=0;                         // число ошибок чтение по модбасу для статистики
  number_err=0;                     // Число ошибок связи при превышении FC_NUM_READ блокировка инвертора
  FC=0;                             // Целевая частота частотика
  freqFC=0;                         // текущая частота инвертора
  power=0;                          // Тееущая мощность частотника
  current=0;                        // Текуший ток частотника
  startCompressor=0;                // время старта компрессора
  state=ERR_LINK_FC;                // Состояние - нет связи с частотником
  dac=0;                            // Текущее значение ЦАП
  testMode=NORMAL;                                 // Значение режима тестирования
  name=(char*)nameOmron;                           // Имя
  note=(char*)noteFC_NONE;                         // Описание инвертора   типа нет его
  // Настройки по умолчанию
  _data.Uptime=DEF_FC_UPTIME;				       // Время обновления алгоритма пид регулятора (мсек) Основной цикл управления
  _data.PidFreqStep=DEF_FC_PID_FREQ_STEP;          // Максимальный шаг (на увеличение) изменения частоты при ПИД регулировании в 0.01 Гц Необходимо что бы ЭРВ успевал
  _data.PidStop=DEF_FC_PID_STOP;				   // Проценты от уровня защит (мощность, ток, давление, темпеартура) при которой происходит блокировка роста частоты пидом
  _data.dtCompTemp=DEF_FC_DT_COMP_TEMP;    		   // Защита по температуре компрессора - сколько градусов не доходит до максимальной (TCOMP) и при этом происходит уменьшение частоты
  _data.startFreq=DEF_FC_START_FREQ;               // Стартовая скорость инвертора (см компрессор) в 0.01
  _data.startFreqBoiler=DEF_FC_START_FREQ_BOILER;  // Стартовая скорость инвертора (см компрессор) в 0.01 ГВС
  _data.minFreq=DEF_FC_MIN_FREQ;                   // Минимальная  скорость инвертора (см компрессор) в 0.01
  _data.minFreqCool=DEF_FC_MIN_FREQ_COOL;          // Минимальная  скорость инвертора при охлаждении в 0.01
  _data.minFreqBoiler=DEF_FC_MIN_FREQ_BOILER;      // Минимальная  скорость инвертора при нагреве ГВС в 0.01
  _data.minFreqUser=DEF_FC_MIN_FREQ_USER;          // Минимальная  скорость инвертора РУЧНОЙ РЕЖИМ (см компрессор) в 0.01
  _data.maxFreq=DEF_FC_MAX_FREQ;                   // Максимальная скорость инвертора (см компрессор) в 0.01
  _data.maxFreqCool=DEF_FC_MAX_FREQ_COOL;          // Максимальная скорость инвертора в режиме охлаждения  в 0.01
  _data.maxFreqBoiler=DEF_FC_MAX_FREQ_BOILER;      // Максимальная скорость инвертора в режиме ГВС в 0.01 Гц поглощение бойлера обычно меньше чем СО
  _data.maxFreqUser=DEF_FC_MAX_FREQ_USER;          // Максимальная скорость инвертора РУЧНОЙ РЕЖИМ (см компрессор) в 0.01
  _data.stepFreq=DEF_FC_STEP_FREQ ;                // Шаг уменьшения инвертора при достижении максимальной температуры, мощности и тока (см компрессор) в 0.01
  _data.stepFreqBoiler=DEF_FC_STEP_FREQ_BOILER;    // Шаг уменьшения инвертора при достижении максимальной температуры, мощности и тока ГВС в 0.01
  _data.dtTemp=DEF_FC_DT_TEMP;                     // Привышение температуры от уставок (подача) при которой срабатыват защита (уменьшается частота) в сотых градуса
  _data.dtTempBoiler=DEF_FC_DT_TEMP_BOILER;        // Привышение температуры от уставок (подача) при которой срабатыват защита ГВС в сотых градуса
 #ifdef FC_ANALOG_CONTROL
  _data.level0=0;                                  // Отсчеты ЦАП соответсвующие 0   мощности
  _data.level100=4096;                             // Отсчеты ЦАП соответсвующие 100 мощности
  _data.levelOff=10;                               // Минимальная мощность при котором частотник отключается (ограничение минимальной мощности)
  #endif
 _data.PidMaxStep = 1000;
 flags=0x00;                               		 // флаги  0 - наличие FC
  _data.setup_flags=0x00;                                // флаги
  if(!Modbus.get_present())                        // modbus отсутствует
      {
       SETBIT0(flags,fFC);          // Инвертор не рабоатет
       journal.jprintf("%s, modbus not found, block.\n",name); 
       err=ERR_NO_MODBUS;
       return err; 
      }
  else if (DEVICEFC==true) SETBIT1(flags,fFC); // наличие частотника в текушей конфигурации
               
  if(get_present())  journal.jprintf("Invertor %s: present config\r\n",name); 
  else {journal.jprintf("Invertor %s: none config\r\n",name);return err;  }  // выходим если нет инвертора

  note=(char*)noteFC_OK;             // Описание инвертора есть
  #ifdef FC_ANALOG_CONTROL                      // Аналоговое управление графики не нужны
  	  pin = PIN_DEVICE_FC;                // Ножка куда прицеплено FC
  	  analogWriteResolution(12);        // разрешение ЦАП 12 бит;
  	  analogWrite(pin,dac);
  	  ChartPower.init(false);                 // инициалазация графика
  #else									// НЕ Аналоговое управление
      err=Modbus.LinkTestOmronMX2();     // проверка связи с инвертором  xModbusSemaphore не используем так как в один поток
      check_blockFC();   
      if (err!=OK)  return err;          // связи нет выходим
      journal.jprintf("Test link Modbus %s: OK\r\n",name);   // Тест пройден

      // Если частотник работает то остановить его
      get_readState(); //  Получить состояние частотника
      switch (state)   // В зависимости от состояния
      {
        case 0:
        case 2: break;                                                         // ОСТАНОВКА ничего не делаем
        case 3: stop_FC();                                                      // ВРАЩЕНИЕ Послать команду стоп и ждать остановки
                while ((state!=2)||(state!=4)) { get_readState();journal.jprintf("Wait stop %s . . .\r\n",name); _delay(3000); } 
                break;
        case 4:                                                                // ОСТАНОВКА С ВЫБЕГОМ  ждать остановки
                break;
        case 5:stop_FC();                                                      // ТОЛЧОВЫЙ ХОД Послать команду стоп и ждать остановки
                while ((state!=2)||(state!=4)) { get_readState();journal.jprintf("Wait stop %s . . .\r\n",name); _delay(3000); } 
                break;
        case 6:                                                                // ТОРМОЖЕНИЕ ПОСТОЯННЫМ ТОКОМ
        case 7: err=ERR_MODBUS_STATE;set_Error(err,name);  break;             // ВЫПОЛНЕНИЕ ПОВТОРНОЙ ПОПЫТКИ Подъем ошибки на верх и останов ТН
        case 8: break;                                                         // АВАРИЙНОЕ ОТКЛЮЧЕНИЕ
        case 9: break;                                                         // ПОНИЖЕНОЕ ПИТАНИЕ
        case -1:break;
        default:err=ERR_MODBUS_STATE;set_Error(err,name);  break;             // Подъем ошибки на верх и останов ТН
      }
      if (err!=OK) return err;
   #endif  // #ifndef FC_ANALOG_CONTROL
  // Установить стартовую частоту
  set_target(_data.startFreq,true,_data.minFreqUser ,_data.maxFreqUser);       // режим н знаем по этому границы развигаем
  return err;                       
}

#define progOK  " Register %s to %d\r\n"  // Строка вывода сообщения о удачном программировании регистра
#define progErr " Error setting register %s\r\n"  // Строка вывода сообщения о не удачном программировании регистра

#ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
// Программирование отдельного регистра инвертора
// adrReg - адрес регистра
// nameReg - имя регистра
// valReg - значение регистра
int8_t devOmronMX2::progReg16(uint16_t adrReg, char* nameReg, uint16_t valReg)   
{ 
	_delay(50);	
	if ((err=write_0x06_16(adrReg, valReg))==OK)  journal.jprintf(progOK,nameReg,valReg);
	else                                          journal.jprintf(progErr,nameReg);
	return err;
}
int8_t devOmronMX2::progReg32(uint16_t adrReg, char* nameReg, uint32_t valReg)   
{ 
	_delay(50);	
	if ((err=write_0x10_32(adrReg, valReg))==OK)  journal.jprintf(progOK,nameReg,valReg);
	else                                          journal.jprintf(progErr,nameReg);
	return err;
}
#endif // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ	

// Программирование инвертора под конкретный компрессор
int8_t devOmronMX2::progFC() 
{
#ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ	
	journal.jprintf("Programming %s . . .\r\n",name);
	// Настройка инвертора под конкретный компрессор Регистры Hxxx Двигатель с постоянными магнитами (PM-двигатель)
	progReg16(MX2_b171, (char*)"b171",0x03);   // b171 Выбор режима ПЧ b171 чт./зап. 0 (выключено), 1 (режим IM), 2 (режим высокой частоты), 3 (режим PM) =03 
	progReg16(MX2_b180,(char*)"b180",0x01);   // b180 Запуск процесса инициализации
    //	while(read_0x03_16(MX2_H102)==1) _delay(100); // Задержка на инициализацию инвертора - ждем пока появится регистр Р102
	journal.jprintf("Wait initialization . . .\r\n"); 
    _delay(7000);
	progReg16(MX2_H102,(char*)"H102",valH102);      // H102 Установка кода PM-двигателя  00 (стандартные данные Omron) 01 (данные автонастройки) = 1
	progReg16(MX2_H103,(char*)"H103",valH103);      // H103 Мощность PM-двигателя (0,1/0,2/0,4/0,55/0,75/1,1/1,5/2,2/3,0/3,7/4,0/5,5/7,5/11,0/15,0/18,5) = 7
	progReg16(MX2_H104,(char*)"H104",valH104);      // H104 Установка числа полюсов PM двигателя = 4
	progReg16(MX2_H105,(char*)"H105",valH105);      // H105 Номинальный ток PM-двигателя = 1000 (это 11А)
	progReg16(MX2_H106,(char*)"H106",valH106);      // H106 Константа R PM-двигателя От 0,001 до 65,535 Ом =0.55 *1000
	progReg16(MX2_H107,(char*)"H107",valH107);      // H107 Константа Ld PM-двигателя От 0,01 до 655,35 мГн =2.31*100
	progReg16(MX2_H108,(char*)"H108",valH108);      // H108 Константа Lq PM-двигателя От 0,01 до 655,35 мГн =2.7*100
	progReg16(MX2_H109,(char*)"H109",valH109);      // H109 Константа Ke PM-двигателя 0,0001...6,5535 Вмакс./(рад/с) =750 надо подбирать влияет на потребление и шум
    progReg32(MX2_H110,(char*)"H110",valH110);      // H110 Константа J PM-двигателя От 0,001 до 9999,000 кг/мІ =0.01
	progReg16(MX2_H119,(char*)"H119",valH119);      // H119 Постоянная стабилизации PM двигателя От 0 до 120% с =100
	progReg16(MX2_H121,(char*)"H121",valH121);      // H121 Минимальная частота PM двигателя От 0,0 до 25,5%  =10 (default)
	progReg16(MX2_H122,(char*)"H122",valH122);      // H122 Ток холостого хода PM двигателя От 0,00 до 100,00%   =50 (default)
    progReg16(MX2_C001,(char*)"C001",valC001);      // C001 Функция входа [1] 0 (FW: ход вперед) =0
    progReg16(MX2_C004,(char*)"C004",valC004);      // C004 Функция входа [4] 18 (RS: сброс) =18
	#ifndef DEMO   // для демо не надо термозащиты настраивать иначе вечная ошибка Е35.1
	progReg16(MX2_C005,(char*)"C005",valC005);      // C005 Функция входа [5] [также вход «PTC»]   = 19 PTC Термистор с положительным ТКС для тепловой защиты (только C005)
	#endif
    progReg16(MX2_C026,(char*)"C026",valC026);      // C026  Функция релейного выхода 5 (AL: сигнал ошибки) =05
	progReg16(MX2_b091,(char*)"b091",valb091);      // b091 Выбор способа остановки 0 (торможение до полной остановки),1 (остановка выбегом)=1
	progReg16(MX2_b021,(char*)"b021",valb021);      // b021 Режим работы при ограничении перегрузки =1
	progReg16(MX2_b022,(char*)"b022",valb022);      // b022 Уровень ограничения перегрузки  200...2000 (0.1%) =                                      
	progReg16(MX2_b023,(char*)"b023",valb023);      // b023  Время торможения при ограничении перегрузки (0.1 сек)=10
	progReg16(MX2_A001,(char*)"A001",valA001);      // A001 Источник задания частоты =03
	progReg16(MX2_A002,(char*)"A002",valA002);      // A002 Источник команды «Ход» 
	progReg16(MX2_A003,(char*)"A003",FC_BASE_FREQ/10);    // A003 основная частота
    progReg16(MX2_A004,(char*)"A004",DEF_FC_MAX_FREQ/10); // A004 установка максимальной частоты
    progReg32(MX2_F002,(char*)"F002",FC_ACCEL_TIME);      // F002 Время разгона
    progReg32(MX2_F002,(char*)"F003",FC_DEACCEL_TIME);    // F003 Торможения разгона
    journal.jprintf(". . . OK\r\n");
#else
    journal.jprintf("Analog control - no support programm Omron MX2\r\n");
#endif // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ	    
	return err;
 }

// Установить целевую частоту
// параметр показывать сообщение сообщение или нет, два оставшихся параметра границы
int8_t  devOmronMX2::set_target(int16_t x,boolean show, int16_t _min, int16_t _max)
{ 
  err=OK;
  #ifdef DEMO
    if ((x>=_min)&&(x<=_max))                     // Проверка диапазона разрешенных частот
    {FC=x; if(show) journal.jprintf(" Set %s: %.2f [Hz]\r\n",name,FC/100.0);   return err;} // установка частоты OK  - вывод сообщения если надо
     else { journal.jprintf("%s: Wrong frequency %.2f\n",name,x/100.0); return WARNING_VALUE; } 
  #else   // Боевой вариант
  uint8_t i;
  if ((!get_present())||(GETBIT(flags,fErrFC))) return err;    // выходим если нет инвертора или он заблокирован по ошибке
  if(GETBIT(HP.Option.flags, fBackupPower) && x > _data.maxFreqGen) x = _data.maxFreqGen;
  if ((x>=_min)&&(x<=_max))                     // Проверка диапазона разрешенных частот
   {
  #ifndef FC_ANALOG_CONTROL                                    // Не аналоговое управление
	  	  if(testMode == NORMAL || testMode == HARD_TEST) {
			  // Запись в регистры инвертора установленной частоты
			  for(i=0;i<FC_NUM_READ;i++)  // Делаем FC_NUM_READ попыток
				{
				  err=write_0x10_32(MX2_TARGET_FR,x);
				  if (err==OK) break;                     // Команда выполнена
				  _delay(100);
				  journal.jprintf("%s: repeat set frequency\n",name);  // Выводим сообщение о повторной команде
				}
	  	  }
          if(err==OK)  { FC=x; if(show) journal.jprintf(" Set %s: %.2f [Hz]\r\n",name,FC/100.0);return err;} // установка частоты OK  - вывод сообщения если надо
          else {err=ERR_LINK_FC; SETBIT1(flags,fErrFC); set_Error(err,name);return err;}                 // генерация ошибки
          // Проверка на установку частоты и адекватность инвертора
          if(testMode == NORMAL || testMode == HARD_TEST) {
        	  if( x!=(int16_t)read_0x03_32(MX2_TARGET_FR)) {err=ERR_FC_ERROR; SETBIT1(flags,fErrFC); set_Error(err,name);return err;}
          }
   #else  // Аналоговое управление
         FC=x;
         dac=((level100-level0)*FC-0*level100)/(100-0);
         switch (testMode)  // РЕАЛЬНЫЕ Действия в зависимости от режима
             {
              case NORMAL:     analogWrite(pin,dac);  break; //  Режим работа не тест, все включаем
              case SAFE_TEST:                         break; //  Ничего не включаем
              case TEST:                              break; //  Включаем все кроме компрессора
              case HARD_TEST:  analogWrite(pin,dac);  break; //  Все включаем и компрессор тоже
             }
          if(show) journal.jprintf(" Set %s: %.2f [Hz]\r\n",name,FC/100.0); // установка частоты OK  - вывод сообщения если надо
   #endif 
    return err;
   }  // if ((x>=_min)&&(x<=_max)) 
   else { journal.jprintf("%s: Wrong frequency %.2f\n",name,x/100.0); return WARNING_VALUE; }  
  #endif  // DEMO
}

// Установить Отсчеты ЦАП соответсвующие 0   мощности
int8_t devOmronMX2::set_level0(int16_t x)
{
   if ((x>=0)&&(x<=4096)) { level0=x; return OK;} // Только правильные значения
   return WARNING_VALUE;
}
// Установить Отсчеты ЦАП соответсвующие 100 мощности
int8_t devOmronMX2::set_level100(int16_t x)
{
  if ((x>=0)&&(x<=4096)) { level100=x; return OK;} // Только правильные значения
  return WARNING_VALUE;
}
// Установить Минимальная мощность при котором частотник отключается (ограничение минимальной мощности)
int8_t devOmronMX2::set_levelOff(int16_t x)
{
  if ((x>=0)&&(x<=100))  { levelOff=x;  return OK;} // Только правильные значения
  return WARNING_VALUE;
}

// Установить запрет на использование инвертора если лимит ошибок исчерпан
void  devOmronMX2::check_blockFC() 
{
   #ifndef FC_ANALOG_CONTROL                                    // Не аналоговое управление
    if((xTaskGetSchedulerState()==taskSCHEDULER_NOT_STARTED)&&(err!=OK))   // если не запущена free rtos то блокируем с первого раза
       {
        SETBIT1(flags,fErrFC);                                                  // Установить флаг
        note=(char*)noteFC_NO;
        set_Error(err,(char*)name);                                        // Подъем ошибки на верх и останов ТН
        return; 
       }
        
    if (err!=OK) number_err++;else { number_err=0; return;}       // Увеличить счетчик ошибок
    if (number_err>FC_NUM_READ)                                // если привышено число ошибок то блокировка
      {
       SemaphoreGive(xModbusSemaphore); // разблокировать семафор
       SETBIT1(flags,fErrFC);                                         // Установить флаг
       note=(char*)noteFC_NO;
       set_Error(err,(char*)name);                        // Подъем ошибки на верх и останов ТН
      }
    #endif 
} 

// Прочитать (внутренние переменные обновляются) состояние Инвертора, возвращает или ОК или ошибку
// Вызывается из задачи чтения датчиков период FC_TIME_READ
int8_t devOmronMX2::get_readState()
{
uint8_t i;
if(testMode != NORMAL && testMode != HARD_TEST) {
	SETBIT0(flags, fErrFC);
	state = 0;
	return err = OK;
}
if ((!get_present())||(GETBIT(flags,fErrFC))) return err;  // выходим если нет инвертора или он заблокирован по ошибке
err=OK;
#ifndef FC_ANALOG_CONTROL                                    // Не аналоговое управление
  // Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
  for(i=0;i<FC_NUM_READ;i++)   // делаем FC_NUM_READ попыток чтения
      { 
        state=read_0x03_16(MX2_STATE);                     // прочитать состояние
        err=Modbus.get_err();                              // Скопировать ошибку
        if (err==OK)                                       // Прочитано верно
         {
          if ((GETBIT(flags,fOnOff))&&(state!=3)) continue; else break;  // ТН включил компрессор а инвертор не имеет правильного состяния пытаемся прочитать еще один раз в проитвном случае все ок выходим
         } 
        _delay(FC_DELAY_REPEAT); 
        journal.jprintf(cErrorRS485,name,__FUNCTION__,err);// Выводим сообщение о повторном чтении
        numErr++;                                          // число ошибок чтение по модбасу
 //       journal.jprintf_time(cErrorRS485,name,err);     // Вывод кода ошибки в журнал
      }
  if (err!=OK)                                              // Ошибка модбаса
      {
       state=ERR_LINK_FC;                                  // признак потери связи с инвертором
       SETBIT1(flags,fErrFC);                              // Блок инвертора
       set_Error(err,name);                                 // генерация ошибки
       return err;                                          // Возврат
      }
//  else  if ((testMode==NORMAL)||(testMode==HARD_TEST))     //   Режим работа и хард тест, анализируем состояние,
//        if ((GETBIT(flags,fOnOff))&&(state!=3))                  // Не верное состояние
//         {
//         err=ERR_MODBUS_STATE;                            // Ошибка не верное состояние инвертора
//         journal.jprintf(" %s:Compressor ON and wrong read state: %d \n",name,state); 
//         set_Error(err,name);  
//         return err;                                        // Возврат
//         }
    // Состояние прочитали и оно правильное все остальное читаем
    _delay(FC_DELAY_READ);
    freqFC=read_0x03_32(MX2_CURRENT_FR);               // прочитать текущую частоту
    err=Modbus.get_err();                             // Скопировать ошибку
    if (err!=OK) {state=ERR_LINK_FC; }               // Ошибка выходим
    
    _delay(FC_DELAY_READ);
    power=read_0x03_16(MX2_POWER);                    // прочитать мощность
    err=Modbus.get_err();                             // Скопировать ошибку
    if (err!=OK) {state=ERR_LINK_FC;}                // Ошибка выходим
    
    _delay(FC_DELAY_READ);
    current=read_0x03_16(MX2_AMPERAGE);               // прочитать ток
    err=Modbus.get_err();                             // Скопировать ошибку
    if (err!=OK) {state=ERR_LINK_FC;}                // Ошибка выходим
#else // Аналоговое управление
    freqFC=FC;
    power=0;
    current=0;
#endif
return err;                            
}

// Команда ход на инвертор (целевая частота НЕ ВЫСТАВЛЯЕТСЯ)
// Может быть подана команда через реле и через модбас в зависимости от ключей компиляции
int8_t devOmronMX2::start_FC()
{
if (((testMode==NORMAL)||(testMode==HARD_TEST))&&(((FC<_data.minFreq)||(FC>_data.maxFreq)))) {journal.jprintf(" %s: Wrong frequency, ignore start\n",name);  return err;} // проверка частоты не в режиме теста
err=OK;
 #ifndef FC_ANALOG_CONTROL                                    // Не аналоговое управление
      #ifdef DEMO
             #ifdef FC_USE_RCOMP   // Использовать отдельный провод для команды ход/стоп
                 HP.dRelay[RCOMP].set_ON();                // ПЛОХО через глобальную переменную
             #endif // FC_USE_RCOMP   
            if (err==OK) {SETBIT1(flags,fOnOff);startCompressor=rtcSAM3X8.unixtime(); journal.jprintf(" %s ON\n",name);}
            else {state=ERR_LINK_FC; SETBIT1(flags,fErrFC); set_Error(err,name);}               // генерация ошибки
      #else // DEMO
             // Боевая часть
            if (((testMode==NORMAL)||(testMode==HARD_TEST))&&(((!get_present())||(GETBIT(flags,fErrFC))))) return err;         // выходим если нет инвертора или он заблокирован по ошибке
          
           // set_target(startFreq,true);  // Запись в регистр инвертора стартовой частоты  НЕ всегда частота стартовая - супербойлер
           
            err=OK;
            if ((testMode==NORMAL)||(testMode==HARD_TEST))  //   Режим работа и хард тест, все включаем,
            {  
            #ifdef FC_USE_RCOMP   // Использовать отдельный провод для команды ход/стоп
                HP.dRelay[RCOMP].set_ON();                // ПЛОХО через глобальную переменную
            #else                 // подать команду ход/стоп через модбас
                err= write_0x05_bit(MX2_START, true);   // Команда Ход
            #endif    
            }
            if (err==OK) {SETBIT1(flags,fOnOff);startCompressor=rtcSAM3X8.unixtime(); journal.jprintf(" %s ON\n",name);}
            else {state=ERR_LINK_FC; SETBIT1(flags,fErrFC); set_Error(err,name);}               // генерация ошибки
      #endif
  #else  //  FC_ANALOG_CONTROL
       #ifdef DEMO
            #ifdef FC_USE_RCOMP   // Использовать отдельный провод для команды ход/стоп
                 HP.dRelay[RCOMP].set_ON();                // ПЛОХО через глобальную переменную
            #endif // FC_USE_RCOMP   
            SETBIT1(flags,fOnOff);startCompressor=rtcSAM3X8.unixtime(); journal.jprintf(" %s ON\n",name);
        #else // DEMO
             // Боевая часть
            if (((testMode==NORMAL)||(testMode==HARD_TEST))&&(((!get_present())||(GETBIT(flags,fErrFC))))) return err;   // выходим если нет инвертора или он заблокирован по ошибке
            err=OK;
            if ((testMode==NORMAL)||(testMode==HARD_TEST))  //   Режим работа и хард тест, все включаем,
            {  
            #ifdef FC_USE_RCOMP   // Использовать отдельный провод для команды ход/стоп
                HP.dRelay[RCOMP].set_ON();                // ПЛОХО через глобальную переменную
            #else               
                state=ERR_LINK_FC; err=ERR_FC_CONF_ANALOG; SETBIT1(flags,fErrFC); set_Error(err,name);// Ошибка конфигурации
            #endif    
            }
            SETBIT1(flags,fOnOff);startCompressor=rtcSAM3X8.unixtime(); journal.jprintf(" %s ON\n",name);
      #endif 
  #endif    
return err;
}

// Команда стоп на инвертор Обратно код ошибки
int8_t devOmronMX2::stop_FC()
{
//uint8_t i;	
err=OK;     
 #ifndef FC_ANALOG_CONTROL                                    // Не аналоговое управление
      #ifdef DEMO
            #ifdef FC_USE_RCOMP   // Использовать отдельный провод для команды ход/стоп
               HP.dRelay[RCOMP].set_OFF();                // ПЛОХО через глобальную переменную
            #endif // FC_USE_RCOMP   
          if (err==OK) {SETBIT0(flags,fOnOff);startCompressor=0; journal.jprintf(" %s OFF\n",name);}
          else {state=ERR_LINK_FC; SETBIT1(flags,fErrFC); set_Error(err,name);}               // генерация ошибки
      #else // не DEMO
          if (!get_present()) return err; // если инвертора нет выходим
          // if  (((testMode==NORMAL)||(testMode==HARD_TEST))&&(((!get_present())||(GETBIT(flags,fErrFC))))) return err;// выходим если нет инвертора или он заблокирован по ошибке
          err=OK;   
          if ((testMode==NORMAL)||(testMode==HARD_TEST))      // Режим работа и хард тест, все включаем,
          {  
          #ifdef FC_USE_RCOMP   // Использовать отдельный провод для команды ход/стоп с проверкой выполнения
              HP.dRelay[RCOMP].set_OFF();            // ПЛОХО через глобальную переменную
               vTaskDelay(1000/ portTICK_PERIOD_MS); // задержка на прохождение команды
               state=read_0x03_16(MX2_STATE);        // 0:Начальное состояние, 2:Остановка 3:Вращение 4:Остановка с выбегом 5:Толчковый ход 6:Торможение  постоянным током 7:Выполнение  повторной попытки 8:Аварийное  отключение 9:Пониженное напряжение -1:Блокировка]
 /*             if ((state!=4)||(state!=2)||(state!=7)) { // если не тормозим то плохо, надо по модбасу рулить
              	 err=write_0x05_bit(MX2_START, false);   // подать команду ход/стоп через модбас
                  _delay(100);
              	 err=write_0x05_bit(MX2_START, false);   // дубль подать команду ход/стоп через модбас
              	 SETBIT1(flags,fErrFC);                  // Установить флаг блокировки
              	 err=ERR_FC_RCOMP;
                 set_Error(err,(char*)name);             // Подъем ошибки на верх и останов ТН
              	 journal.jprintf("$ERROR: it is not possible to stop the inverter via RCOMP, the inverter is blocked\n"); 
              	} */
      /*         for(i=0;i<FC_NUM_READ;i++)  // установить целевую частоту в 0
		            {
		              err=write_0x10_32(MX2_TARGET_FR,0);
		              if (err==OK) break;             // Команда выполнена
		              _delay(100);
		              journal.jprintf("%s: repeat set frequency 0.0 Hz\n",name);  // Выводим сообщение о повторной команде
		            }
	*/	            
          #else                  // подать команду ход/стоп через модбас
              err=write_0x05_bit(MX2_START, false);   // Команда стоп
          #endif   
           }
          if (err==OK) {SETBIT0(flags,fOnOff);startCompressor=0; journal.jprintf(" %s OFF\n",name);}
          else {state=ERR_LINK_FC; SETBIT1(flags,fErrFC); set_Error(err,name);}                          // генерация ошибки
      #endif
 #else  // FC_ANALOG_CONTROL 
      #ifdef DEMO
            #ifdef FC_USE_RCOMP   // Использовать отдельный провод для команды ход/стоп
               HP.dRelay[RCOMP].set_OFF();                // ПЛОХО через глобальную переменную
            #endif // FC_USE_RCOMP   
            SETBIT0(flags,fOnOff);startCompressor=0; journal.jprintf(" %s OFF\n",name);
      #else // не DEMO
          if  (((testMode==NORMAL)||(testMode==HARD_TEST))&&(((!get_present())||(GETBIT(flags,fErrFC))))) return err;    // выходим если нет инвертора или он заблокирован по ошибке
          if ((testMode==NORMAL)||(testMode==HARD_TEST))      // Режим работа и хард тест, все включаем,
          {  
          #ifdef FC_USE_RCOMP   // Использовать отдельный провод для команды ход/стоп
              HP.dRelay[RCOMP].set_OFF();                    // ПЛОХО через глобальную переменную
          #else                  // подать команду ход/стоп через модбас
              state=ERR_LINK_FC; err=ERR_FC_CONF_ANALOG; SETBIT1(flags,fErrFC); set_Error(err,name);// Ошибка конфигурации
          #endif   
           }
          SETBIT0(flags,fOnOff);startCompressor=0; journal.jprintf(" %s OFF\n",name);
      #endif 
 #endif // FC_ANALOG_CONTROL 
return err;
}

// Получить параметр инвертора в виде строки, результат ДОБАВЛЯЕТСЯ в ret
void devOmronMX2::get_paramFC(char *var,char *ret)
{
    if(strcmp(var,fc_ON_OFF)==0)                { if (GETBIT(flags,fOnOff))  strcat(ret,(char*)cOne);else  strcat(ret,(char*)cZero); } else
    if(strcmp(var,fc_INFO)==0)                  {
    	                                        #ifndef FC_ANALOG_CONTROL  
    	                                        get_infoFC(ret);
    	                                        #else
                                                 strcat(ret, "|Данные не доступны, работа через аналоговый вход|;") ;
                                                #endif              
    	                                        } else
    if(strcmp(var,fc_NAME)==0)                  {  strcat(ret,name);             } else
    if(strcmp(var,fc_NOTE)==0)                  {  strcat(ret,note);             } else
    if(strcmp(var,fc_PIN)==0)                   {  _itoa(pin,ret);     } else
    if(strcmp(var,fc_PRESENT)==0)               { if (GETBIT(flags,fFC))  strcat(ret,(char*)cOne);else  strcat(ret,(char*)cZero); } else
    if(strcmp(var,fc_STATE)==0)                 {  _itoa(state,ret);   } else
    if(strcmp(var,fc_FC)==0)                    {  _ftoa(ret,(float)FC/100.0,2); } else
    if(strcmp(var,fc_cFC)==0)                   {  _ftoa(ret,(float)freqFC/100.0,2); } else
    if(strcmp(var,fc_cPOWER)==0)                {  _ftoa(ret,(float)power/10.0,1); } else
    if(strcmp(var,fc_INFO1)==0)                 {  _ftoa(ret,(float)power/10.0,1); strcat(ret, " кВт"); } else
    if(strcmp(var,fc_cCURRENT)==0)              {  _ftoa(ret,(float)current/100.0,2); } else
    if(strcmp(var,fc_AUTO_RESET_FAULT)==0)      {  strcat(ret,(char*)(GETBIT(_data.setup_flags,fAutoResetFault) ? cOne : cZero)); } else
    if(strcmp(var,fc_LogWork)==0)      			{  strcat(ret,(char*)(GETBIT(_data.setup_flags,fLogWork) ? cOne : cZero)); } else
    if(strcmp(var,fc_ANALOG)==0)                { // Флаг аналогового управления
		                                        #ifdef FC_ANALOG_CONTROL                                                    
		                                         strcat(ret,(char*)cOne);
		                                        #else
		                                         strcat(ret,(char*)cZero);
		                                        #endif
                                                } else
    if(strcmp(var,fc_DAC)==0)                   {  _itoa(dac,ret);          } else
    #ifdef FC_ANALOG_CONTROL
    if(strcmp(var,fc_LEVEL0)==0)                {  _itoa(level0,ret);       } else
    if(strcmp(var,fc_LEVEL100)==0)              {  _itoa(level100,ret);     } else
    if(strcmp(var,fc_LEVELOFF)==0)              {  _itoa(levelOff,ret);     } else
    #endif
    if(strcmp(var,fc_BLOCK)==0)                 { if (GETBIT(flags,fErrFC))  strcat(ret,(char*)cOne);else  strcat(ret,(char*)cZero); } else
    if(strcmp(var,fc_ERROR)==0)                 {  _itoa(err,ret);          } else
    if(strcmp(var,fc_UPTIME)==0)                {  _itoa(_data.Uptime,ret); } else   // вывод в секундах
    if(strcmp(var,fc_PID_STOP)==0)              {  _itoa(_data.PidStop,ret);          } else
    if(strcmp(var,fc_DT_COMP_TEMP)==0)          {  _ftoa(ret,(float)_data.dtCompTemp/100.0,2); } else // градусы
	if(strcmp(var,fc_PID_FREQ_STEP)==0)         {  _ftoa(ret,(float)_data.PidFreqStep/100.0,2); } else // Гц
	if(strcmp(var,fc_START_FREQ)==0)            {  _ftoa(ret,(float)_data.startFreq/100.0,2); } else // Гц
	if(strcmp(var,fc_START_FREQ_BOILER)==0)     {  _ftoa(ret,(float)_data.startFreqBoiler/100.0,2); } else // Гц
	if(strcmp(var,fc_MIN_FREQ)==0)              {  _ftoa(ret,(float)_data.minFreq/100.0,2); } else // Гц
	if(strcmp(var,fc_MIN_FREQ_COOL)==0)         {  _ftoa(ret,(float)_data.minFreqCool/100.0,2); } else // Гц
	if(strcmp(var,fc_MIN_FREQ_BOILER)==0)       {  _ftoa(ret,(float)_data.minFreqBoiler/100.0,2); } else // Гц
	if(strcmp(var,fc_MIN_FREQ_USER)==0)         {  _ftoa(ret,(float)_data.minFreqUser/100.0,2); } else // Гц
	if(strcmp(var,fc_MAX_FREQ)==0)              {  _ftoa(ret,(float)_data.maxFreq/100.0,2); } else // Гц
	if(strcmp(var,fc_MAX_FREQ_COOL)==0)         {  _ftoa(ret,(float)_data.maxFreqCool/100.0,2); } else // Гц
	if(strcmp(var,fc_MAX_FREQ_BOILER)==0)       {  _ftoa(ret,(float)_data.maxFreqBoiler/100.0,2); } else // Гц
	if(strcmp(var,fc_MAX_FREQ_USER)==0)         {  _ftoa(ret,(float)_data.maxFreqUser/100.0,2); } else // Гц
	if(strcmp(var,fc_MAX_FREQ_GEN)==0)          {  _dtoa(ret, _data.maxFreqGen, 2); } else
	if(strcmp(var,fc_STEP_FREQ)==0)             {  _ftoa(ret,(float)_data.stepFreq/100.0,2); } else // Гц
	if(strcmp(var,fc_STEP_FREQ_BOILER)==0)      {  _ftoa(ret,(float)_data.stepFreqBoiler/100.0,2); } else // Гц
    if(strcmp(var,fc_DT_TEMP)==0)               {  _ftoa(ret,(float)_data.dtTemp/100.0,2); } else // градусы
    if(strcmp(var,fc_DT_TEMP_BOILER)==0)        {  _ftoa(ret,(float)_data.dtTempBoiler/100.0,2); } else // градусы
    if(strcmp(var,fc_MB_ERR)==0)        		{  _itoa(numErr, ret); } else
    if(strcmp(var,fc_FC_RETOIL_FREQ)==0)   		{ 	strcat(ret, "-"); } else
   	if(strcmp(var, fc_PidMaxStep)==0)   		{  _dtoa(ret, _data.PidMaxStep, 2); } else
  	if(strcmp(var,fc_FC_TIME_READ)==0)   		{  _itoa(FC_TIME_READ, ret); } else
   		strcat(ret,(char*)cInvalid);
}
   


// Установить параметр инвертора из строки
boolean devOmronMX2::set_paramFC(char *var, float x)
{
    if(strcmp(var,fc_ON_OFF)==0)                { if (x==0) stop_FC();else start_FC();return true;  } else 
    if(strcmp(var,fc_FC)==0)                    { if((x*100>=_data.minFreqUser)&&(x*100<=_data.maxFreqUser)){set_target(x*100,true, _data.minFreqUser, _data.maxFreqUser); return true; }else return false; } else
    if(strcmp(var,fc_AUTO_RESET_FAULT)==0)      { if (x==0) SETBIT0(_data.setup_flags,fAutoResetFault);else SETBIT1(_data.setup_flags,fAutoResetFault);return true;  } else // для Омрона код не написан
    if(strcmp(var,fc_LogWork)==0)               { _data.setup_flags = (_data.setup_flags & ~(1<<fLogWork)) | ((x!=0)<<fLogWork); return true;  } else

    #ifdef FC_ANALOG_CONTROL
    if(strcmp(var,fc_LEVEL0)==0)                { if ((x>=0)&&(x<=4096)) { level0=x; return true;} else return false;      } else 
    if(strcmp(var,fc_LEVEL100)==0)              { if ((x>=0)&&(x<=4096)) { level100=x; return true;} else return false;    } else 
    if(strcmp(var,fc_LEVELOFF)==0)              { if ((x>=0)&&(x<=4096)) { levelOff=x; return true;} else return false;    } else 
    #endif
    if(strcmp(var,fc_BLOCK)==0)                 { SemaphoreGive(xModbusSemaphore); // отдать семафор ВСЕГДА
                                                if (x==0) { SETBIT0(flags,fErrFC); note=(char*)noteFC_OK; }
                                                else      { SETBIT1(flags,fErrFC); note=(char*)noteFC_NO; }
                                                return true;            
                                                } else  
    if(strcmp(var,fc_UPTIME)==0)                { if((x>=3)&&(x<600)){_data.Uptime=x;return true; } else return false; } else   // хранение в сек
    if(strcmp(var,fc_PID_STOP)==0)              { if((x>=50)&&(x<=100)){_data.PidStop=x;return true; } else return false;  } else // % от цели
    if(strcmp(var,fc_DT_COMP_TEMP)==0)          { if((x>=1)&&(x<=25)){_data.dtCompTemp=x*100;return true; } else return false; } else // градусы

	if(strcmp(var,fc_PID_FREQ_STEP)==0)         { if((x>0)&&(x*100<=_data.PidMaxStep)){_data.PidFreqStep=x*100;return true; } else return false; } else // Гц
	if(strcmp(var,fc_START_FREQ)==0)            { if((x>=20)&&(x<=120)){_data.startFreq=x*100;return true; } else return false; } else // Гц
	if(strcmp(var,fc_START_FREQ_BOILER)==0)     { if((x>=20)&&(x<=150)){_data.startFreqBoiler=x*100;return true; } else return false; } else // Гц
	if(strcmp(var,fc_MIN_FREQ)==0)              { if((x>=20)&&(x<=80)){_data.minFreq=x*100;return true; } else return false; } else // Гц
	if(strcmp(var,fc_MIN_FREQ_COOL)==0)         { if((x>=20)&&(x<=80)){_data.minFreqCool=x*100;return true; } else return false; } else // Гц
	if(strcmp(var,fc_MIN_FREQ_BOILER)==0)       { if((x>=20)&&(x<=80)){_data.minFreqBoiler=x*100;return true; } else return false; } else // Гц
	if(strcmp(var,fc_MIN_FREQ_USER)==0)         { if((x>=20)&&(x<=80)){_data.minFreqUser=x*100;return true; } else return false; } else // Гц
	if(strcmp(var,fc_MAX_FREQ)==0)              { if((x>=40)&&(x<=240)){_data.maxFreq=x*100;return true; } else return false; } else // Гц
	if(strcmp(var,fc_MAX_FREQ_COOL)==0)         { if((x>=40)&&(x<=240)){_data.maxFreqCool=x*100;return true; } else return false; } else // Гц
	if(strcmp(var,fc_MAX_FREQ_BOILER)==0)       { if((x>=40)&&(x<=240)){_data.maxFreqBoiler=x*100;return true; } else return false; } else // Гц
	if(strcmp(var,fc_MAX_FREQ_USER)==0)         { if((x>=40)&&(x<=240)){_data.maxFreqUser=x*100;return true; } else return false; } else // Гц
	if(strcmp(var,fc_MAX_FREQ_GEN)==0)          { if((x>=40)&&(x<=240)){_data.maxFreqGen=x*100;return true; } } else
	if(strcmp(var,fc_STEP_FREQ)==0)             { if((x>=0.2)&&(x<=10)){_data.stepFreq=x*100;return true; } else return false; } else // Гц
	if(strcmp(var,fc_STEP_FREQ_BOILER)==0)      { if((x>=0.2)&&(x<=10)){_data.stepFreqBoiler=x*100;return true; } else return false; } // Гц

	if(strcmp(var,fc_DT_TEMP)==0)               { if((x>0)&&(x<10)){_data.dtTemp=x*100;return true; } else return false; } else // градусы
    if(strcmp(var,fc_DT_TEMP_BOILER)==0)        { if((x>0)&&(x<10)){_data.dtTempBoiler=x*100;return true; } else return false; } else // градусы
	if(strcmp(var,fc_PidMaxStep)==0)            { if(x>=0 && x<10000){_data.PidMaxStep=x; return true; } else return false; } else // %
    return false;
}

	
 
// Получить информацию о частотнике, информация добавляется в buf
char * devOmronMX2::get_infoFC(char *buf)
{
  if(testMode != NORMAL && testMode != HARD_TEST) {
	  strcat(buf, "|TEST MODE|;");
	  return buf;
  }
#ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
  if(!HP.dFC.get_present()) { strcat(buf,"|Данные не доступны (нет инвертора)|;"); return buf;}          // Инвертора нет в конфигурации
  if(HP.dFC.get_blockFC())  { strcat(buf,"|Данные не доступны (нет связи по Modbus, инвертор заблокирован)|;"); return buf;}  // Инвертор заблокирован
  int8_t i;  
       strcat(buf,"-|Состояние инвертора [0:Начальное состояние, 2:Остановка 3:Вращение 4:Остановка с выбегом 5:Толчковый ход 6:Торможение  постоянным током ");strcat(buf,"7:Выполнение  повторной попытки 8:Аварийное  отключение 9:Пониженное напряжение -1:Блокировка]|");_itoa(read_0x03_16(MX2_STATE),buf);strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d001|Контроль выходной частоты (Гц)|");_ftoa(buf,(float)read_0x03_32(MX2_CURRENT_FR)/100.0,2);strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d003|Контроль выходного тока (А)|");_ftoa(buf,(float)read_0x03_16(MX2_AMPERAGE)/100.0,2);strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d014|Контроль мощности (кВт)|");_ftoa(buf,(float)read_0x03_16(MX2_POWER)/10.0,1);strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d013|Контроль выходного напряжения (В)|");_ftoa(buf,(float)read_0x03_16(MX2_VOLTAGE)/10.0,1);strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d015|Контроль ватт-часов (кВт/ч)|");_ftoa(buf,(float)read_0x03_32(MX2_POWER_HOUR)/10.0,1);strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d016|Контроль времени наработки в режиме \"Ход\" (ч)|");_itoa(read_0x03_32(MX2_HOUR),buf);strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d017|Контроль времени наработки при включенном питании (ч)|");_itoa(read_0x03_32(MX2_HOUR1),buf);strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d018|Контроль температуры радиатора (°С)|");_ftoa(buf,(float)read_0x03_16(MX2_TEMP)/10.0,2);strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d102|Контроль напряжения  постоянного тока (В)|");_ftoa(buf,(float)read_0x03_16(MX2_VOLTAGE_DC)/10.0,1);strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d080|Счетчик аварийных отключений (Шт)|");_itoa(read_0x03_16(MX2_NUM_ERR),buf);strcat(buf,";");
       for(i=0;i<6;i++)  // Скан по ошибкам
          {
          strcat(buf,"d0");_itoa(81+i,buf);strcat(buf,"|Состояние в момент ошибки ");
          read_0x03_error(MX2_ERROR1+i*0x0a);
          // Формирование ответа в строке
          strcat(buf,"[F:");  _ftoa(buf,(float)error.MX2.fr/100.0,2);
          strcat(buf," I:");  _ftoa(buf,(float)error.MX2.cur/100.0,2);
          strcat(buf," V:");  _ftoa(buf,(float)error.MX2.vol/10.0,2);
          strcat(buf," T1:"); _itoa(error.MX2.time1,buf);
          strcat(buf," T2:"); _itoa(error.MX2.time2,buf);
          strcat(buf,"] Код ошибки:|");
          if(error.MX2.code<10) strcat(buf,"E0"); else strcat(buf,"E");
          _itoa(error.MX2.code,buf);strcat(buf,"."); _itoa(error.MX2.status,buf);
          strcat(buf,";");   
          }  
#endif          
return buf;                              
}          
// Сброс ошибок инвертора по модбасу
boolean devOmronMX2::reset_errorFC()                   
{
#ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
  write_0x06_16(MX2_INIT_DEF, 0x01);       // задать режим иннициализации - стирание ошибок
  _delay(FC_DELAY_READ);
  if((read_0x03_16(MX2_INIT_DEF)==0x01)&&(err=OK))   // подать команду на инициализацию, если записано стирание только ошибок
    {
    write_0x06_16(MX2_INIT_RUN, 0x01);       
    journal.jprintf("Reset error %s\r\n",name);
    }
  else journal.jprintf("$WARNING: bad read from MX2_INIT_DEF, no reset error\r\n");
#endif
if (err==OK) return true;  else return false;   
}

// Сброс инвертора через модбас
boolean devOmronMX2::reset_FC() 
{
#ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
  write_0x05_bit(MX2_RESET, true);               // подать команду на сброс по модбас
//  journal.jprintf("Reset %s use Modbus\r\n",name);  
#endif
if (err==OK) return true;  else return false;                          
}

// Текущее состояние инвертора
// 
int16_t devOmronMX2::read_stateFC()
{
#ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
  state=read_0x03_16(MX2_STATE);  // прочитать состояние
  if(GETBIT(_data.setup_flags,fLogWork) && GETBIT(flags, fOnOff)) {
			journal.jprintf_time("FC: %Xh, %.2fHz, %.2fA, %.2fkW\n", state, (float)freqFC/100.0, (float)current/100.0, (float)get_power()/1000.0);}
  return state;
#else
  return 0;
#endif  
} 

// Tемпература радиатора
int16_t devOmronMX2::read_tempFC()
{
#ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
  return read_0x03_16(MX2_TEMP);
#else
  return 0;
#endif  
} 
// Функции общения по модбас инвертора  Чтение регистров
#ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
    // Чтение отдельного бита в регистр cmd возвращает код ,  ошибка обновляется
    // Реалезовано FC_NUM_READ попыток чтения/записи в инвертор
    boolean devOmronMX2::read_0x01_bit(uint16_t cmd)
    { uint8_t i;
      boolean result;
      err=OK;  
      if ((!get_present())||(GETBIT(flags,fErrFC))) return false;             // выходим если нет инвертора или он заблокирован по ошибке
      for(i=0;i<FC_NUM_READ;i++)   // делаем FC_NUM_READ попыток чтения Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
         { 
         err=Modbus.readCoil(FC_MODBUS_ADR,cmd-1, &result);              // послать запрос, Нумерация регистров MX2 с НУЛЯ!!!!
         if (err==OK) break;                                                // Прочитали удачно
         _delay(FC_DELAY_REPEAT);
         journal.jprintf(cErrorRS485,name,__FUNCTION__,err);                // Выводим сообщение о повторном чтении
         numErr++;                                                          // число ошибок чтение по модбасу
 //        journal.jprintf_time(cErrorRS485,name,err);                     // Вывод кода ошибки в журнал
         }
    
      check_blockFC();                                                     // проверить необходимость блокировки
      return result;  
    }
    // Функция 0х03 прочитать 2 байта, возвращает значение, ошибка обновляется
    // Реализовано FC_NUM_READ попыток чтения/записи в инвертор
    int16_t  devOmronMX2::read_0x03_16(uint16_t cmd)
    {   uint8_t i;
        uint16_t result;  
        err=OK;
        if ((!get_present())||(GETBIT(flags,fErrFC))) return 0;                  // выходим если нет инвертора или он заблокирован по ошибке
    
        for(i=0;i<FC_NUM_READ;i++)   // делаем FC_NUM_READ попыток чтения Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
            { 
            err=Modbus.readHoldingRegisters16(FC_MODBUS_ADR,cmd-1,&result);  // Послать запрос, Нумерация регистров MX2 с НУЛЯ!!!!
            if (err==OK) break;                                                 // Прочитали удачно
            _delay(FC_DELAY_REPEAT);
             journal.jprintf(cErrorRS485,name,__FUNCTION__,err);                // Выводим сообщение о повторном чтении
             numErr++;                                                          // число ошибок чтение по модбасу
   //         journal.jprintf_time(cErrorRS485,name,err);                     // Вывод кода ошибки в журнал
            }
            check_blockFC();                                                   // проверить необходимость блокировки
            return result;
             
    }
    
    // Функция 0х03 прочитать 4 байта
    // Реализовано FC_NUM_READ попыток чтения/записи в инвертор
    uint32_t devOmronMX2::read_0x03_32(uint16_t cmd)
    {
        uint8_t i;
        uint32_t result;  
        err=OK;
        if ((!get_present())||(GETBIT(flags,fErrFC))) return 0;            // выходим если нет инвертора или он заблокирован по ошибке
        for(i=0;i<FC_NUM_READ;i++)   // делаем FC_NUM_READ попыток чтения Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
          { 
           err=Modbus.readHoldingRegisters32(FC_MODBUS_ADR,cmd-1,&result);  // послать запрос, Нумерация регистров MX2 с НУЛЯ!!!!
           if (err==OK) break;                                                 // Прочитали удачно
          _delay(FC_DELAY_REPEAT);
          journal.jprintf(cErrorRS485,name,__FUNCTION__,err);                 // Выводим сообщение о повторном чтении
          numErr++;                                                           // число ошибок чтение по модбасу
    //       journal.jprintf_time(cErrorRS485,name,err);                   // Вывод кода ошибки в журнал
          }
        check_blockFC();                                                      // проверить необходимость блокировки
        return result;
    }
    
    // Функция Modbus 0х03 описание ошибки num НУМЕРАЦИЯ с 0 (общая длина данных 10 слов по 2 байта)
    // Возвращает код ошибки и в буфер кладет описание
    // Реализовано FC_NUM_READ попыток чтения/записи в инвертор
    int16_t devOmronMX2::read_0x03_error(uint16_t cmd)  
    { uint8_t i;
      uint16_t tmp;
      err=OK;
      if ((!get_present())||(GETBIT(flags,fErrFC))) return err;              // выходим если нет инвертора или он заблокирован по ошибке
      for(i=0;i<0x0a;i++) error.inputBuf[i]=0;
      for(i=0;i<FC_NUM_READ;i++)   // делаем FC_NUM_READ попыток чтения Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
         { 
         err = Modbus.readHoldingRegistersNN(FC_MODBUS_ADR,cmd-1,0x0a,error.inputBuf);  // послать запрос, Нумерация регистров с НУЛЯ!!!!
         if (err==OK) break;                                                 // Прочитали удачно
         _delay(FC_DELAY_REPEAT);
         journal.jprintf(cErrorRS485,name,__FUNCTION__,err);                 // Выводим сообщение о повторном чтении
          numErr++;                                                          // число ошибок чтение по модбасу
  //        journal.jprintf_time(cErrorRS485,name,err);                     // Вывод кода ошибки в журнал
         }
      if (err==OK) // Для времен переставить местами слова (2 байта) т.е сначала идет старшие 2 байта потом младшие
      {
        tmp=error.inputBuf[6]; error.inputBuf[6]=error.inputBuf[7]; error.inputBuf[7]=tmp; // Общее время наработки в режиме «Ход» к моменту отключения
        tmp=error.inputBuf[8]; error.inputBuf[8]=error.inputBuf[9]; error.inputBuf[9]=tmp; // Общее время работы ПЧ при включенном питании в момент отключения выхода
      }
      check_blockFC();                                                   // проверить необходимость блокировки
      return err;
    }
    
    // Запись отдельного бита в регистр cmd возвращает код ошибки
    // Реализовано FC_NUM_READ попыток чтения/записи в инвертор
    int8_t devOmronMX2::write_0x05_bit(uint16_t cmd, boolean f)
    { uint8_t i;
      err=OK;
      if ((!get_present())||(GETBIT(flags,fErrFC))) return err;     // выходим если нет инвертора или он заблокирован по ошибке
      for(i=0;i<FC_NUM_READ;i++)   // делаем FC_NUM_READ попыток записи
         {   
            if (f) err=Modbus.writeSingleCoil(FC_MODBUS_ADR,cmd-1,1);   // послать запрос, Нумерация регистров с НУЛЯ!!!!
            else   err=Modbus.writeSingleCoil(FC_MODBUS_ADR,cmd-1,0);
            if (err==OK) break;                                            // Записали удачно
            _delay(FC_DELAY_REPEAT);
           journal.jprintf(cErrorRS485,name,__FUNCTION__,err);             // Выводим сообщение о повторном чтении
           numErr++;                                                       // число ошибок чтение по модбасу
  //         journal.jprintf_time(cErrorRS485,name,err);                  // Вывод кода ошибки в журнал
         }
      check_blockFC();                                                    // проверить необходимость блокировки
      return err;
    }
    // Запись данных (2 байта) в регистр cmd возвращает код ошибки
    // Реализовано FC_NUM_READ попыток чтения/записи в инвертор
    int8_t devOmronMX2::write_0x06_16(uint16_t cmd, uint16_t data)
    { uint8_t i;
      err=OK;
      if ((!get_present())||(GETBIT(flags,fErrFC))) return err;              // выходим если нет инвертора или он заблокирован по ошибке
       for(i=0;i<FC_NUM_READ;i++)                                          // делаем FC_NUM_READ попыток записи
         {
           err=Modbus.writeHoldingRegisters16(FC_MODBUS_ADR,cmd-1,data);  // послать запрос, Нумерация регистров с НУЛЯ!!!!
           if (err==OK) break;                                               // Записали удачно
           _delay(FC_DELAY_REPEAT);
           journal.jprintf(cErrorRS485,name,__FUNCTION__,err);                // Выводим сообщение о повторном чтении
           numErr++;                                                          // число ошибок чтение по модбасу
   //        journal.jprintf_time(cErrorRS485,name,err);                     // Вывод кода ошибки в журнал
         }
      check_blockFC();                                                      // проверить необходимость блокировки
      return err;
      
    }
    // Запись данных (4 байта) в регистр cmd возвращает код ошибки
    int8_t devOmronMX2::write_0x10_32(uint16_t cmd, uint32_t data)
    { uint8_t i;
      err=OK;
      if ((!get_present())||(GETBIT(flags,fErrFC))) return err;             // выходим если нет инвертора или он заблокирован по ошибке
      for(i=0;i<FC_NUM_READ;i++)                                          // делаем FC_NUM_READ попыток записи
         {  
           err=Modbus.writeHoldingRegisters32(FC_MODBUS_ADR, cmd-1, data);// послать запрос, Нумерация регистров с НУЛЯ!!!!
           if (err==OK) break;                                               // Записали удачно
           _delay(FC_DELAY_REPEAT);
           journal.jprintf(cErrorRS485,name,__FUNCTION__,err);                // Выводим сообщение о повторном чтении
           numErr++;                                                          // число ошибок чтение по модбасу
  //         journal.jprintf_time(cErrorRS485,name,err);                     // Вывод кода ошибки в журнал
         }
      check_blockFC();                                                       // проверить необходимость блокировки
      return err; 
    }
#endif  // FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ

#endif // НЕ FC_VACON
