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
// Описание базовых классов для работы c "железом"
// (датчики и исполнительные устройства) зависит от контроллера
// --------------------------------------------------------------------------------
#ifndef Hardware_h
#define Hardware_h
#include "Constant.h"                       // Вся конфигурация и константы проекта Должен быть первым !!!!
#include <rtc_clock.h>                      // работа со встроенными часами  Это основные часы!!!
#include <ModbusMaster.h>                   // Используется МОДИФИЦИРОВАННАЯ либа ModbusMaster https://github.com/4-20ma/ModbusMaster
#include "StepMotor.h" 
#include "Information.h"
#include "devOneWire.h" 

// Флаги датчиков (единые для всех датчиков!!!!!)
#define fPresent      0               // флаг наличие датчика
#define fTest         1               // флаг режим теста
#define fFull         2               // флаг полного буфера для усреднения
#define fAddress      3               // флаг правильного адреса для температурного датчика
#define fcheckRange	  4				  // флаг Проверка граничного значения
#define fsensModbus	  5				  // флаг дистанционного датчика по Modbus

extern RTC_clock rtcSAM3X8;
extern int8_t set_Error(int8_t err, char *nam);

// Структура для хранения "сырых"данных с аналогового датчика.
struct type_rawADC
{
 uint32_t sum;                          // сумма
 uint16_t p[FILTER_SIZE];               // массив накопленных значений
 //uint16_t *p;                           // указатель на массив накопленных значений, не забыть память выделить
 uint16_t last;                         // текущий индекс
 boolean flagFull;                      // буфер полный
 uint16_t lastVal;                      // последнее считанное значение
 //uint32_t err_read;                     // счетчик ошибкок чтения
 uint8_t  error;                        // Последняя ошибка чтения датчика
};
// ------------------------------------------------------------------------------------------
// Д А Т Ч И К И  ---------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------
#include "DigitalTemp.h"

// ------------------------------------------------------------------------------------------
// Аналоговые датчики давления --------------------------------------------------------------
// Давление харится в сотых бара
#define PEVA        0                                    // Датчик давления испарителя.
#define PCON        1                                    // Датчик давления нагнетания.

// Класс аналоговый датчик управление токовая петля + нагрузочный резистор + делитель
// Может быть несколько датчиков
// Давление хранится в СОТЫХ БАРА
class sensorADC
{
  public:
    void initSensorADC(int sensor,int pinA);             // Инициализация датчика  порядковый номер датчика и нога он куда прикреплен
    int8_t  Read();                                      // чтение данных c аналогового датчика давления (АЦП) возвращает код ошибки, делает все преобразования
    //int16_t Test();                                      // полный цикл получения данных возвращает значение давления, только тестирование!! никакие переменные класса не трогает!!
    __attribute__((always_inline)) inline int16_t get_minPress(){return cfg.minPress;}     // Минимальная давление датчика - нижняя граница диапазона, при выходе из него ошибка
    __attribute__((always_inline)) inline int16_t get_maxPress(){return cfg.maxPress;}     // Максимальная давление датчика - верхняя граница диапазона, при выходе из него ошибка
    int16_t get_zeroPress(){return cfg.zeroPress;}           // Выход датчика (отсчеты ацп)  соответсвующий 0
    int8_t  set_zeroPress(int16_t p);                    // Установка Выход датчика (отсчеты ацп)  соответсвующий 0
    int16_t get_lastPress(){return lastPress;}           // Последнее считанное значение датчика - НЕ обработанное (без усреднения)
    int16_t get_lastADC(){ return lastADC; }             // Последнее считанное значение датчика в отсчетах с фильтром
    int16_t get_Press();                                 // Получить значение давления датчика - это то что используется
    float get_transADC(){return cfg.transADC;}               // Получить значение коэффициента преобразования напряжение-температура
    int8_t set_transADC(float p);                        // Установить значение коэффициента преобразования напряжение-температура
    __attribute__((always_inline)) inline boolean get_present(){return GETBIT(flags,fPresent);} // Наличие датчика в текущей конфигурации
    __attribute__((always_inline)) inline boolean get_fmodbus(){return GETBIT(flags,fsensModbus);} // Подключен по Modbus
    int8_t  get_lastErr(){return err;}                   // Получить последнюю ошибку
    inline int8_t  get_pinA(){return pin;}               // Получить канал АЦП (нумерация SAM3X) куда прицеплен датчик
    int16_t get_testPress(){return cfg.testPress;}           // Получить значение давления датчика в режиме теста
    int8_t  set_testPress(int16_t p);                    // Установить значение давления датчика в режиме теста
    int8_t  set_minPress(int16_t p) { cfg.minPress = p; return OK; }
    int8_t  set_maxPress(int16_t p)  { cfg.maxPress = p; return OK; }
    TEST_MODE get_testMode(){return testMode;}           // Получить текущий режим работы
    void    set_testMode(TEST_MODE t){testMode=t;}       // Установить значение текущий режим работы
     
    char*   get_note(){return note;}                     // Получить наименование датчика
    char*   get_name(){return name;}                     // Получить имя датчика
    uint8_t *get_save_addr(void) { return (uint8_t *)&cfg; } // Адрес структуры сохранения
    uint16_t get_save_size(void) { return sizeof(cfg); } // Размер структуры сохранения
   
    statChart Chart;                                      // График по датчику
    type_rawADC adc;                                      // структура для хранения сырых данных с АЦП
    
  private:
    int16_t lastPress;                                   // последнее считанное давление с датчика
    int16_t Press;                                       // давление датчика (обработанная)
    struct {     // Save GROUP, firth number
   	uint8_t number;										 // Номер
    int16_t zeroPress;                                   // отсчеты АЦП при нуле датчика
    float   transADC;                                    // коэффициент пересчета АЦП в давление
    int16_t testPress;                                   // давление датчика в режиме тестирования
    int16_t minPress;                                    // минимально разрешенное давление
    int16_t maxPress;                                    // максимально разрешенное давление
    } __attribute__((packed)) cfg;// Save Group end
    TEST_MODE testMode;                                  // Значение режима тестирования
    uint16_t lastADC;                                    // Последние значение отсчета ацп
       
    uint8_t pin;                                         // Ножка куда прицеплен датчик
    int8_t  err;                                         // ошибка датчика (работа) при ошибке останов ТН
    byte 	flags;                                       // флаги  датчика
    // Кольцевой буфер для усреднения
    void clearBuffer();                                  // очистить буфер
    int16_t p[P_NUMSAMLES];                              // буфер для усреднения показаний давления
    int32_t sum;                                         // Накопленная сумма
    uint8_t last;                                        // указатель на последнее (самое старое) значение в буфере диапазон от 0 до P_NUMSAMLES-1
   
    char *note;                                          // Описание датчика
    char *name;                                          // Имя датчика
};

// ------------------------------------------------------------------------------------------
// Цифровые контактные датчики (есть 2 состяния 0 и 1) --------------------------------------
class sensorDiditalInput
{
public:
  void initInput(int sensor);                            // Инициализация контактного датчика
  int8_t  Read();                                        // Чтение датчика возвращает ошибку или ОК
  __attribute__((always_inline)) inline boolean get_Input(){return Input;}   // Получить значение датчика при последнем чтении
  __attribute__((always_inline)) inline boolean get_present(){return GETBIT(flags,fPresent);} // Наличие датчика в текущей конфигурации
  int8_t  get_lastErr(){return err;}                     // Получить последнюю ошибку
  char*   get_note(){return note;}                       // Получить наименование датчика
  char*   get_name(){return name;}                       // Получить имя датчика
  boolean get_testInput(){return testInput;}             // Получить Состояние датчика в режиме теста
  int8_t  set_testInput(int16_t i);                      // Установить Состояние датчика в режиме теста
  TEST_MODE get_testMode(){return testMode;}             // Получить текущий режим работы
  void  set_testMode(TEST_MODE t){testMode=t;}           // Установить значение текущий режим работы
  boolean get_alarmInput(){return alarmInput;}           // Состояние аварии датчика
  int8_t  set_alarmInput(int16_t i);                     // Установить Состояние аварии датчика
  inline int8_t  get_pinD(){return pin;}                 // Получить ногу куда прицеплен датчик
  TYPE_SENSOR get_typeInput(){return type;}              // Получить тип датчика
  uint8_t *get_save_addr(void) { return (uint8_t *)&number; } // Адрес структуры сохранения
  uint16_t get_save_size(void) { return (byte*)&alarmInput - (byte*)&number + sizeof(alarmInput); } // Размер структуры сохранения
    
private:
   boolean Input;                                        // Состояние датчика
   struct { // Save GROUP, firth number
   uint8_t number;										 // номер
   boolean testInput;                                    // !save! Состояние датчика в режиме теста
   boolean alarmInput;                                   // !save! Состояние датчика в режиме аварии
   } __attribute__((packed));// Save Group end, last alarmInput
   TEST_MODE testMode;                                   // Значение режима тестирования
   TYPE_SENSOR type;                                     // Тип датчика
   int8_t err;                                           // ошибка датчика (работа) при ошибке останов ТН
   byte flags;                                           // флаги  датчика
   uint8_t  pin;                                         // Ножка куда прицеплен датчик
   char *note;                                           // наименование датчика
   char *name;                                           // Имя датчика
};
// ------------------------------------------------------------------------------------------
// Цифровые частотные датчики (значение кодируется в выходной частоте) ----------------------
// основное назначение - датчики потока
// Частота кодируется в тысячных герца
// Число импульсов рассчитывается за базовый период (BASE_TIME_READ), т.к частоты малы период надо савить не менее 5 сек
// Выходная величина кодируется в тысячных от целого

class sensorFrequency
{
public:
  void initFrequency(int sensor);                   // Инициализация частотного датчика
  void reset(void);									// Сброс счетчика
  __attribute__((always_inline)) inline void InterruptHandler(){count++;} // обработчик перываний
  int8_t  Read();                                         // Чтение датчика (точнее расчет значения) возвращает ошибку или ОК
  __attribute__((always_inline)) inline uint32_t get_Frequency(){return Frequency;}   // Получить ЧАСТОТУ датчика при последнем чтении
  __attribute__((always_inline)) inline uint16_t get_Value(){return Value;}           // Получить Значение датчика при последнем чтении, литры в час
  __attribute__((always_inline)) inline boolean get_present(){return GETBIT(flags,fPresent);} // Наличие датчика в текущей конфигурации
  __attribute__((always_inline)) inline uint16_t get_minValue(){return minValue * 100;}     // Получить минимальное значение датчика, литры в час
  void set_minValue(float f){ minValue = f*10+0.05; }     			// Установить минимальное значение датчика
  __attribute__((always_inline)) inline float get_kfCapacity(){return 3600*100/Capacity;}   // Получить Коэффициент пересчета для определениея мощности  (3600 секунды в часе) в СОТЫХ!!!
  __attribute__((always_inline)) inline boolean get_checkFlow(){return GETBIT(flags,fcheckRange);}// Проверка граничного значения
  void set_checkFlow(boolean f) { flags = (flags & ~(1<<fcheckRange)) | (f<<fcheckRange); }
  int8_t  get_lastErr(){return err;}                     // Получить последнюю ошибку
  char*   get_note(){return note;}                       // Получить описание датчика
  char*   get_name(){return name;}                       // Получить имя датчика
  uint16_t get_testValue(){return testValue;}            // Получить Состояние датчика в режиме теста
  int8_t  set_testValue(int16_t i);                      // Установить Состояние датчика в режиме теста
  float   get_kfValue(){return kfValue;}                 // Получить коэффициент пересчета
  void    set_kfValue(uint16_t f) { kfValue=f; }         // Установить коэффициент пересчета
  uint16_t get_Capacity(){return Capacity;}              // Получить теплоемкость
  int8_t set_Capacity(uint16_t c);                       // Установить теплоемкость больше 5000 не устанавливается
  TEST_MODE get_testMode(){return testMode;}             // Получить текущий режим работы
  void  set_testMode(TEST_MODE t){testMode=t;}           // Установить значение текущий режим работы
  inline int8_t  get_pinF(){return pin;}                 // Получить ногу куда прицеплен датчик
  uint8_t *get_save_addr(void) { return (uint8_t *)&number; } // Адрес структуры сохранения
  uint16_t get_save_size(void) { return (byte*)&Capacity - (byte*)&number + sizeof(Capacity); } // Размер структуры сохранения
  statChart Chart;                                       // Статистика по датчику
    
private:
   uint32_t Frequency;                                   // значение частоты в тысячных герца
   uint16_t Value;                                       // значение датчика ЛИТРЫ В ЧАС (ИЛИ ТЫСЯЧНЫЕ КУБА) 
   struct { // SAVE GROUP, number the first
   uint8_t  number;										 // номер
   uint16_t testValue;                                   // !save! Состояние датчика в режиме теста
   uint16_t kfValue; 								 	 // коэффициент пересчета частоты в значение, сотые
   uint8_t  flags;                                       // флаги  датчика
   uint8_t  minValue;							     	 // десятые m3/h (0..25,5)
   uint16_t Capacity;                                    // значение теплоемкости теплоносителя в конутре где установлен датчик [Cp, Дж/(кг·град)]
   } __attribute__((packed));// END SAVE GROUP, Capacity the last
   TEST_MODE testMode;                                   // Значение режима тестирования
   volatile uint16_t count;                              // число импульсов за базовый период (то что меняется в прерывании)
   uint32_t sTime;                                       // время начала базового периода в тиках
   int8_t err;                                           // ошибка датчика (работа) при ошибке останов ТН
   uint8_t  pin;                                         // Ножка куда прицеплен датчик
   char *note;                                           // наименование датчика
   char *name;                                           // Имя датчика
};




// ------------------------------------------------------------------------------------------
// И С П О Л Н И Т Е Л Ь Н Ы Е   У С Т Р О Й С Т В А   --------------------------------------
// ------------------------------------------------------------------------------------------

class devRelay
{
public:
  void initRelay(int sensor);                            // Инициализация реле
  __attribute__((always_inline)) inline int8_t  set_ON() {return set_Relay((boolean)true);}    // Включить реле
  __attribute__((always_inline)) inline int8_t  set_OFF(){return set_Relay((boolean)false);}   // Выключить реле
  int8_t  set_Relay(boolean r);                          // Установить реле в состояние r
  int8_t  set_Relay(int16_t r);                          // Установить реле в состояние r
  __attribute__((always_inline)) inline boolean get_Relay(){return Relay;}                    // Прочитать состояние реле
  int8_t  get_pinD(){return pin;}                        // Получить ногу куда прицеплено реле
  char*   get_note(){return note;}                       // Получить наименование реле
  char*   get_name(){return name;}                       // Получить имя реле
  __attribute__((always_inline)) inline boolean get_present(){return GETBIT(flags,fPresent);} // Наличие датчика в текущей конфигурации
  TEST_MODE get_testMode(){return testMode;}             // Получить текущий режим работы
  void set_testMode(TEST_MODE t){testMode=t;}            // Установить значение текущий режим работы
private:
   int8_t  err;                                          // ошибка реле
   boolean Relay;                                        // Состояние реле
   TEST_MODE testMode;                                   // Значение режима тестирования
   byte flags;                                           // флаги  0 - наличие реле,
   uint8_t  pin;                                         // Ножка куда прицеплено реле
   char *note;                                           // наименование реле
   char *name;                                           // Имя реле
};  

// ЭРВ ТОЛЬКО ОДНА ШТУКА ВСЕГДА (не массив) ---------------------------------------- --------------------------------------
#define fEnterInPercent  2		// Ввод на веб-странице в %, иначе в шагах
#define fCorrectOverHeat 3		// Включен режим корректировки перегрева
#define fHoldMotor       4		// Режим "удержания" шагового двигателя ЭРВ

class devEEV
{
public:
  void initEEV();                                        // Инициализация ЭРВ
  int8_t Start();                                        // Запуск ЭРВ - начало алгоритма отслеживания - параметр текущее время
  void Pause(){fPause=true;}                             // Пауза ЭРВ - останов отслеживания
  void Resume(uint16_t pos);                             // Пауза ЭРВ - возобновление отслеживания
  int8_t Update(int16_t teva, int16_t tcon);             // Обновление ЭРВ - одна итерация алгоритма отслеживания на входе
  boolean isWork();                                      // Получить состояние ЭРВ
  int16_t get_Overheat(){return Overheat;}               // Получить текущий перегрев
  int16_t set_Overheat(int16_t rto,int16_t out, int16_t in, int16_t p); // Вычислить текущий перегрев, вычисляется каждое измерение (проводится в опросе датчиков)
  void   CorrectOverheat(void);							 // Корректировка перегрева
  void 	 CorrectOverheatInit(void);						 // Перед стартом компрессора

  // Движение ЭРВ
  __attribute__((always_inline)) inline int16_t get_EEV() {return  EEV;} // Прочитать МГНОВЕННУЮ!! позицию шагового двигателя ЭРВ двигатель может двигаться
  int16_t  get_EEV_percent(void) { return EEV > 0 ? (int32_t) EEV * 10000 / maxEEV : 0; } //  % открытия ЭРВ
  int8_t  set_EEV(int x);                                // Перейти на позицию абсолютную  возвращает код ошибки
  int8_t  set_zero();                                    // Гарантированно (шагов больше чем диапазон) закрыть ЭРВ возвращает код ошибки

  char* get_paramEEV(char *var, char *ret);              // Получить параметр ЭРВ в виде строки
  boolean set_paramEEV(char *var,float x);               // Установить параметр ЭРВ из строки

  // Функции чтения настроек ЭРВ в бинарном виде  
  int16_t get_tOverheat(){return  _data.tOverheat;}      // Получить целевой перегрев
  int16_t get_timeIn(){ return  _data.timeIn;}           // Получить ЗАДАННУЮ постоянную интегрирования времени в секундах ЭРВ
  int16_t get_delay() {return tmpTime;}                  // Получить ТЕКУЩУЮ постоянную интегрирования времени в секундах ЭРВ
  int16_t get_Kpro()  {return  _data.Kp;}                // Пропорциональная составляющая  В СОТЫХ!!!
  int16_t get_Kint(){return  _data.Ki;}                  // Интегральная составляющая  В СОТЫХ!!!
  int16_t get_Kdif(){return  _data.Kd;}                  // Дифференциальная составляющая  В СОТЫХ!!!
  int16_t get_Correction(){return _data.Correction;}     // Получить поправку в градусах для правила работы ЭРВ «TEVAOUT-TEVAIN».  СОТЫЕ ГРАДУСА
  int16_t get_manualStep(){return _data.manualStep;}     // Получить число шагов открытия ЭРВ для правила работы ЭРВ «Manual»
  TYPEFREON get_typeFreon(){return _data.typeFreon;}     // Получить тип фреона
  RULE_EEV get_ruleEEV(){return _data.ruleEEV;}          // Получить тип фреона
  boolean get_HoldMotor(){return GETBIT(_data.flags,fHoldMotor);}
  uint8_t get_minSteps(){return _data.minSteps;}
  uint8_t get_delayOnPid(){return _data.delayOnPid;}
  uint8_t get_delayOff(){return _data.delayOff;}
  uint8_t get_delayOn(){return _data.delayOn;}
  uint8_t get_DelayStartPos() {return _data.DelayStartPos;}
  uint16_t get_StartPos(){return _data.StartPos;}
  uint16_t get_preStartPos(){return _data.preStartPos;}
  char*   get_note(){ return note;}                      // Прочитать описание ЭРВ
  char*   get_name(){ return name;}                      // Прочитать имя ЭРВ 
  int8_t  get_lastErr(){return err;}                     // Получить последнюю ошибку
  int16_t get_maxEEV(){return  maxEEV;}                  // Максимальное число шагов ЭРВ (диапазон)
  int16_t get_minEEV(){return  _data.minSteps;}          // Число шагов при котором ЭРВ начинает открываться (холостой ход) может быть равным 0
  __attribute__((always_inline)) inline boolean get_present(){return GETBIT(_data.flags,fPresent);} // Наличие EEV в текущей конфигурации
  
  TEST_MODE get_testMode(){return testMode;}             // Получить текущий режим работы
  void set_testMode(TEST_MODE t){testMode=t;}            // Установить значение текущий режим работы
  
  // Сохранение
  uint8_t *get_save_addr(void) { return (uint8_t *)&_data; } // Адрес структуры сохранения
  uint16_t get_save_size(void) { return sizeof(_data); } // Размер структуры сохранения
  int32_t save(int32_t adr);                             // Записать настройки в eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
  int32_t load(int32_t adr);                             // Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
  int32_t loadFromBuf(int32_t adr,byte *buf);            // Считать настройки из буфера на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
  uint16_t get_crc16(uint16_t crc);                      // Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
 
  StepMotor stepperEEV;                                  // Шаговый двигатель ЭРВ
  statChart Chart;                                       // График по ЭРВ
  boolean setZero;                                       // признак обнуления EEV;
  int16_t EEV;                                           // Текущая  АБСОЛЮТНАЯ позиция

private:
  boolean fPause;                                        // пауза алгоритма отслеживания true
  boolean fStart;                                        // Значение true при первом заходе в ПИД или после сброса, Убрать первое рысканье
  __attribute__((always_inline)) inline int8_t jamp(int x){return set_EEV(EEV+x);} // Перейти на позицию относительную возвращает код ошибки
  int8_t stepDown();                                     // 1 Шаг в минус возвращает код ошибки
  int8_t stepUp();                                       // 1 Шаг в плюс возвращает код ошибки
  void resetPID();                                       // Сброс пид регулятора
  // переменные пид регулятора
  #ifdef EEV_INT_PID                                     // использование ПИДА в целочисленной арифметике
    int32_t   temp_int ;                                 // Служебная переменная интегрирования в 1000 (тысячных)
    int16_t   errPID;                                    // текущая ошибка ПИД регулятора  в сотых градуса
    int16_t   pre_errPID;                                // Предыдущая ошибка ПИД регулятора в сотых градуса
  #else
    float   temp_int ;                                     // Служебная переменная интегрирования
    float   errPID;                                        // текущая ошибка ПИД регулятора
    float   pre_errPID;                                    // Предыдущая ошибка ПИД регулятора
  #endif

  uint16_t Pid_start;                                    // откуда стартует ПИД регулятор обновление в функции Resume
  int16_t Overheat;                                      // Перегрев текущий (сотые градуса)
  int16_t OHCor_tdelta;									 // Расчитанная целевая дельта Нагнетание-Конденсации
  int16_t tmpTime;                                       // ТЕКУЩАЯ постоянная интегрирования времени в секундах ЭРВ (она по алгоритму может быть меньше timeIn)
  TEST_MODE testMode;                                    // Значение режима тестирования
  int16_t maxEEV;                                        // Максимальное число шагов ЭРВ (диапазон)
  int8_t  err;                                           // ошибка ЭРВ (работа) при ошибке останов ТН
  char *note;                                            // Описание
  char *name;                                            // Имя
  
 struct {                                                // Структура для сохранения настроек! Первая переменная => timeIn
		  int16_t timeIn;                                        // Постоянная интегрирования времени в секундах ЭРВ СЕКУНДЫ
		  int16_t tOverheat;                                     // Перегрев ЦЕЛЬ (сотые градуса)
		  int16_t Kp ;                                           // ПИД Коэф пропорц.  В СОТЫХ!!!
		  int16_t Ki ;                                           // ПИД Коэф интегр.  для настройки Ki=0  В СОТЫХ!!!
		  int16_t Kd ;                                           // ПИД Коэф дифф.   В СОТЫХ!!!
		  int16_t Correction;                                    // 0.855 ПЕРЕДЕЛАНО  зона не чуствительности перегрева в "плюсе" в этой зоне на каждом шаге эрв закрывается на 1 шаг
		  int16_t manualStep;                                    // Число шагов открытия ЭРВ для правила работы ЭРВ «Manual»
		  TYPEFREON typeFreon;                                   // Тип фреона
		  RULE_EEV ruleEEV;                                      // правило работы ЭРВ

		  uint16_t OHCor_Delay;									 // Задержка после старта компрессора, сек
		  uint16_t OHCor_Period;								 // Период в циклах ЭРВ, сколько пропустить
		  int16_t OHCor_TDIS_TCON;							     // Температура нагнетания - конденсации (/0.01) при конденсации 20 градусов
		  uint8_t OHCor_TDIS_TCON_Thr;							 // Порог (/0.1), после превышения TDIS_TCON + TDIS_TCON_Thr начинаем менять перегрев
		  uint8_t OHCor_TDIS_ADD;								 // Корректировка (/0.1) в + для TDIS_TCON на каждые 10 градусов выше 20.
		  int16_t OHCor_K;										 // Коэффициент (/0.001): перегрев += дельта * K
		  int16_t OHCor_OverHeatMin;							 // Минимальный перегрев (сотые градуса)
		  int16_t OHCor_OverHeatMax;							 // Максимальный перегрев (сотые градуса)

		  uint16_t errKp;                                        // Ошибка (в сотых градуса) при которой происходит уменьшение пропорциональной составляющей ПИД ЭРВ
		  uint8_t  speedEEV;                                     // Скорость шагового двигателя ЭРВ (импульсы в сек.)
		  uint8_t  minSteps;                                     // Минимальное число шагов открытия ЭРВ
		  uint16_t preStartPos;                                  // ПУСКОВАЯ позиция ЭРВ (ТО что при старте компрессора ПРИ РАСКРУТКЕ)
		  uint16_t StartPos;                                     // СТАРТОВАЯ позиция ЭРВ после раскрутки компрессора т.е. ПОЗИЦИЯ С КОТОРОЙ НАЧИНАЕТСЯ РАБОТА проходит DelayStartPos сек
		  // ЭРВ Времена и задержки
		  uint8_t  delayOnPid;                                   // Задержка включения EEV после включения компрессора (сек).  Точнее после выхода на рабочую позицию Общее время =delayOnPid+DelayStartPos
		  uint8_t  DelayStartPos;                                // Время после старта компрессора когда EEV выходит на стартовую позицию - облегчение пуска вначале ЭРВ
		  uint8_t  delayOff;                                     // Задержка закрытия EEV после выключения насосов (сек). Время от команды стоп компрессора до закрытия ЭРВ = delayOffPump+delayOff
		  uint8_t  delayOn;                                      // Задержка между открытием (для старта) ЭРВ и включением компрессора, для выравнивания давлений (сек). Если ЭРВ закрывлось при остановке
		  byte flags;                                            // флаги ЭРВ,
		  int16_t OHCor_OverHeatStart;							 // Начальный перегрев (сотые градуса)
  } _data;                                               // Конец структуры для сохранения настроек - последняя переменная => flags  
};

// Частотный преобразователь ТОЛЬКО ОДНА ШТУКА ВСЕГДА (не массив) ------------------------------------------------------------------------------
// Флаги Инвертора
#define fFC         	0        // флаг наличие инвертора
#define fAuto       	1        // флаг режим автоматического регулирования частоты ( 0 - старт-стоп через инвертор 1 - ПИД)
#define fPower      	2        // флаг режим ограничения мощности (резерв - сейчас ограничение всегда)
#define fOnOff      	3        // флаг включения-выключения частотника
#define fErrFC      	4        // флаг глобальная ошибка инвертора - работа инвертора запрещена
#define fAutoResetFault	5        // флаг Автосброс не критичного сбоя инвертора
#define fLogWork		6		 // флаг логировать параметры во время работы
#define FC_SAVED_FLAGS 	((1<<fAuto) | (1<<fAutoResetFault) | (1<<fLogWork))

const char *noteFC_OK   = {" связь по Modbus установлена" };                     // Все впорядке
const char *noteFC_NO   = {" связь по Modbus потеряна, инвертор заблокирован" };
const char *noteFC_NONE = {" отсутствует в данной конфигурации" };

#ifndef FC_VACON

// КОНКРЕТНЫЙ ИНВЕРТОР OMRON MX2 --------------------------------------------------------------------------------------------------------------
// Управление идет частотой в герцах. Внутри частота хранится в сотых герца  максимально возможная частота 650 Гц!!
// Мощность хранится в 0.1 кВт
// Напряжение в 0.1 В
const char *nameOmron    = {"Omron MX2"}; //  Имя, марка инвертора
// Регистры Omron MX2
#define MX2_STATE         0x0003       // (2 байта) Состояние ПЧ
#define MX2_TARGET_FR     0x0001       // (4 байта) Источник (задание) задания частоты (0,01 [Гц])
#define MX2_ACCEL_TIME    0x1103       // (4 байта) Время разгона (см компрессор) в 0.01 сек
#define MX2_DEACCEL_TIME  0x1105       // (4 байта) Время торможения (см компрессор) в 0.01 сек

#define MX2_CURRENT_FR    0x1001       // (4 байта) Контроль выходной частоты (0,01 [Гц])
#define MX2_AMPERAGE      0x1003       // (2 байта) Контроль выходного тока (0,01 [A])
#define MX2_VOLTAGE       0x1011       // (2 байта) Контроль выходного  напряжения 0,1 [В]
#define MX2_POWER         0x1012       // (2 байта) Контроль мощности 0,1 [кВт]
#define MX2_POWER_HOUR    0x1013       // (4 байта) Контроль ватт-часов 0,1 [кВт/ч]
#define MX2_HOUR          0x1015       // (4 байта) Контроль времени наработки в режиме "Ход" 1 [ч]
#define MX2_HOUR1         0x1017       // (4 байта) Контроль времени наработки при включенном питании 1 [ч]
#define MX2_TEMP          0x1019       // (2 байта) Контроль температуры радиатора (0.1 градус) -200...1500
#define MX2_VOLTAGE_DC    0x1026       // (2 байта) Контроль напряжения  постоянного тока (P-N) 0,1 [В]
#define MX2_NUM_ERR       0x0011       // (2 байта) Счетчик аварийных отключений 0...65530
#define MX2_ERROR1        0x0012       // (20 байт) Описалово 1 отключения  остальные 5 лежат последовательно за первой ошибкой адреса вычисляются MX2_ERROR1+i*0x0a
#define MX2_INIT_DEF      0x1357       // (2 байта) Задать режим инициализации 0 (ничего),1 (очистка истории отключений),2 (инициализация данных), 3 (очистка истории отключений и инициализация данных), 4 (очистка истории отключений, инициализация данных ипрограммы EzSQ)
#define MX2_INIT_RUN      0x13b7       // (2 байта) Запуск инициализации 0 (выключено), 1 (включено)

#define MX2_SOURCE_FR     0x1201       // (2 байта) Источник задания частоты
#define MX2_SOURCE_CMD    0x1202       // (2 байта) Источник команды
#define MX2_BASE_FR       0x1203       // (2 байта) Основная частота 300...«максимальная частота»  0.1 Гц
#define MX2_MAX_FR        0x1204       // (2 байта) Максимальная частота 300...4000 (10000) 0.1 Гц
#define MX2_DC_BRAKING    0x1245       // (2 байта) Разрешение торможения постоянным током
#define MX2_STOP_MODE     0x134e       // (2 байта) Выбор способа остановки  B091=01
#define MX2_MODE          0x13ae       // (2 байта) Выбор режима ПЧ b171=03

// Биты Omron MX2
#define MX2_START         0x0001       // (бит) Команда «Ход» 1: Ход, 0: Стоп (действительно при A002 = 03)
#define MX2_SET_DIR       0x0002       // (бит) Команда направления вращения 1: Обратное вращение, 0: Вращение в  прямом направлении (действительно при  A002 = 03)
#define MX2_RESET         0x0004       // (бит) Сброс аварийного отключения (RS) 1: Сброс
#define MX2_READY         0x0011       // (бит) Готовность ПЧ 1: Готов, 0: Не готов
#define MX2_DIRECTION     0x0010       // (бит) Направление вращения 1: Обратное вращение, 0: Вращение в  прямом направлении (взаимоблокировка с  "d003")

#define TEST_NUMBER       1234         // Проверочный код для функции 0x08

struct type_errorMX2       // структура ошибки
{
  uint16_t code;           // причина
  uint16_t status;         // Состояние ПЧ при отключении
  uint16_t noUse;          // Не используется
  uint16_t fr;             // Частота ПЧ при отключении
  uint16_t cur;            // Ток ПЧ при отключении
  uint16_t vol;            // Напряжение ПЧ при отключении
  uint32_t time1;          // Общее время наработки в режиме ХОД при отключении
  uint32_t time2;          // Общее время работы ПЧ при включенном питании в момент отключения
};

union union_errorFC
{
  type_errorMX2 MX2;
  uint16_t  inputBuf[10]; 
};

#define ERR_LINK_FC 0xFF  	    // Состояние инертора - нет связи.

class devOmronMX2   // Класс частотный преобразователь Omron MX2
{
public:
  int8_t initFC();                                 // Инициализация Частотника
  __attribute__((always_inline)) inline boolean get_present(){return GETBIT(flags,fFC);} // Наличие ПЧ в текущей конфигурации
  int8_t get_err(){return err;}                    // Получить последню ошибку частотника
  uint16_t get_numErr(){return numErr;}            // Получить число ошибок чтения
  void get_paramFC(char *var, char *ret);         // Получить параметр инвертора в виде строки
  boolean set_paramFC(char *var, float x);         // Установить параметр инвертора из строки

  // Получение отдельных значений 
  uint16_t get_Uptime(){return _data.Uptime;}				     // Время обновления алгоритма пид регулятора (сек) Основной цикл управления
  uint16_t get_PidFreqStep(){return _data.PidFreqStep;}          // Максимальный шаг (на увеличение) изменения частоты при ПИД регулировании в 0.01 Гц Необходимо что бы ЭРВ успевал
  uint16_t get_PidStop(){return _data.PidStop;}				     // Проценты от уровня защит (мощность, ток, давление, темпеартура) при которой происходит блокировка роста частоты пидом
  uint16_t get_dtCompTemp(){return _data.dtCompTemp;}    		 // Защита по температуре компрессора - сколько градусов не доходит до максимальной (TCOMP) и при этом происходит уменьшение частоты
  uint16_t get_startFreq(){return _data.startFreq;}              // Стартовая частота инвертора (см компрессор) в 0.01 
  uint16_t get_startFreqBoiler(){return _data.startFreqBoiler;}  // Стартовая частота инвертора (см компрессор) в 0.01  ГВС
  uint16_t get_minFreq(){return _data.minFreq;}                  // Минимальная  частота инвертора (см компрессор) в 0.01 
  uint16_t get_minFreqCool(){return _data.minFreqCool;}          // Минимальная  частота инвертора при охлаждении в 0.01 
  uint16_t get_minFreqBoiler(){return _data.minFreqBoiler;}      // Минимальная  частота инвертора при нагреве ГВС в 0.01
  uint16_t get_minFreqUser(){return _data.minFreqUser;}          // Минимальная  частота инвертора РУЧНОЙ РЕЖИМ (см компрессор) в 0.01 
  uint16_t get_maxFreq(){return _data.maxFreq;}                  // Максимальная частота инвертора (см компрессор) в 0.01 
  uint16_t get_maxFreqCool(){return _data.maxFreqCool;}          // Максимальная частота инвертора в режиме охлаждения  в 0.01 
  uint16_t get_maxFreqBoiler(){return _data.maxFreqBoiler;}      // Максимальная частота инвертора в режиме ГВС в 0.01 Гц поглощение бойлера обычно меньше чем СО
  uint16_t get_maxFreqUser(){return _data.maxFreqUser;}          // Максимальная частота инвертора РУЧНОЙ РЕЖИМ (см компрессор) в 0.01 
  uint16_t get_stepFreq(){return _data.stepFreq;}                // Шаг уменьшения инвертора при достижении максимальной температуры, мощности и тока (см компрессор) в 0.01 
  uint16_t get_stepFreqBoiler(){return _data.stepFreqBoiler;}    // Шаг уменьшения инвертора при достижении максимальной температуры, мощности и тока ГВС в 0.01 
  uint16_t get_dtTemp(){return _data.dtTemp;}                    // Привышение температуры от уставок (подача) при которой срабатыват защита (уменьшается частота) в сотых градуса
  uint16_t get_dtTempBoiler(){return _data.dtTempBoiler;}        // Привышение температуры от уставок (подача) при которой срабатыват защита ГВС в сотых градуса
    
  // Управление по модбас Общее для всех частотников
  int16_t get_targetFreq() {return FC;}            // Получить целевую частоту в 0.01 герцах
  int8_t  set_targetFreq(int16_t x,boolean show, int16_t _min, int16_t _max);// Установить целевую частоту в 0.01 герцах show - выводить сообщение или нет + границы
  __attribute__((always_inline)) inline uint16_t get_power(){return power * 100;}              // Получить текущую мощность. В ваттах еденица измерения
  __attribute__((always_inline)) inline uint16_t get_current(){return current;}          // Получить текущий ток в 0.01А
  char * get_infoFC(char *bufn);                   // Получить информацию о частотнике
  boolean reset_errorFC();                         // Сброс ошибок инвертора
  boolean reset_FC();                              // Сброс инвертора через модбас
  int16_t read_stateFC();                          // Текущее состояние инвертора
  int16_t read_tempFC();                           // Tемпература радиатора
   
  int16_t get_freqFC() {return freqFC;}            // Получить текущую частоту в 0.01 герцах
  uint32_t get_startTime(){return startCompressor;}// Получить время старта компрессора
  int8_t get_readState();                          // Прочитать (внутренние переменные обновляются) состояние Инвертора, возвращает или ОК или ошибку
  int8_t start_FC();                               // Команда ход на инвертор (целевая частота выставляется)
  int8_t stop_FC();                                // Команда стоп на инвертор
  boolean isfAuto(){return GETBIT(_data.setup_flags,fAuto);}   // проверка на режим старт-стопа false стоит флаг стартстопа
  boolean isfOnOff(){return GETBIT(flags,fOnOff);} // получить состояние инвертора вкл или выкл
 
  void check_blockFC();                            // Установить запрет на использование инвертора
  boolean get_blockFC(){return GETBIT(flags,fErrFC);}// Получить флаг блокировки инвертора
  union_errorFC get_errorFC(){return error;}       // Получить структуру с ошибкой
  
  // Аналоговое управление
  int16_t get_DAC(){return dac;};                  // Получить установленное значеие ЦАП
  int16_t get_level0(){return level0;}             // Получить Отсчеты ЦАП соответсвующие 0   частоте
  int16_t get_level100(){return level100;}         // Получить Отсчеты ЦАП соответсвующие максимальной частоте
  int16_t get_levelOff(){return levelOff;}         // Получить Минимальная частота при котором частотник отключается (ограничение минимальной мощности)
  int8_t set_level0(int16_t x);                    // Установить Отсчеты ЦАП соответсвующие 0   мощности
  int8_t set_level100(int16_t x);                  // Установить Отсчеты ЦАП соответсвующие 100 мощности
  int8_t set_levelOff(int16_t x);                  // Установить Минимальная мощность при котором частотник отключается (ограничение минимальной частоты)
  uint8_t get_pinA(){return  pin;}                 // Ножка куда прицеплено FC
   
  // Сервис
  TEST_MODE get_testMode() {return testMode;};     // Получить текущий режим работы
  void  set_testMode(TEST_MODE t) {testMode=t;};   // Установить значение текущий режим работы
  char*   get_note(){return  note;}                // Получить описание
  char*   get_name(){return  name;}                // Получить имя
  uint8_t *get_save_addr(void) { return (uint8_t *)&_data; } // Адрес структуры сохранения
  uint16_t get_save_size(void) { return sizeof(_data); } // Размер структуры сохранения

  statChart ChartFC;                               // График по частоте
  statChart ChartPower;                            // График по мощности
  statChart ChartCurrent;                          // График по току инвертора
 
private:
  int8_t  err;                                     // ошибка частотника (работа) при ошибке останов ТН
  uint16_t numErr;                                 // число ошибок чтение по модбасу
  uint8_t number_err;                              // Число ошибок связи при превышении FC_NUM_READ блокировка инвертора
   // Управление по 485
  uint16_t FC;                                     // Целевая частота инвертора в 0.01 герцах
  uint16_t freqFC;                                 // Чтение: текущая частота инвертора в 0.01 герцах
  uint16_t power;                                  // Чтение: Текущая мощность инвертора  в 100 ватных единицах (3 это 300 вт)
  uint16_t current;                                // Чтение: Текущий ток инвертора  в 0.01 Ампера единицах
  
  int16_t state;                                   // Чтение: Состояние ПЧ регистр MX2_STATE
//  uint16_t minFC;                                  // Минимальная частотота инвертора в 0.01 герцах
//  uint16_t maxFC;                                  // Максимальная частота инвертора в 0.01 герцах
  uint32_t startCompressor;                        // время старта компрессора
  union_errorFC error;                             // Структура для дешефрации ошибки инвертора
        
  // Аналоговое управление
  int16_t dac;                                     // Текущее значение ЦАП
  int16_t level0;                                  // Отсчеты ЦАП соответсвующие 0   частота
  int16_t level100;                                // Отсчеты ЦАП соответсвующие максимальной частотет
  int16_t levelOff;                                // Минимальная мощность при котором частотник отключается (ограничение минимальной мощности)
  uint8_t pin;                                     // Ножка куда прицеплено FC
  
  TEST_MODE testMode;                              // Значение режима тестирования
  char *note;                                      // Описание
  char *name;                                      // Имя

// Структура для сохранения настроек, Uptime всегда первый
  struct {
	  uint16_t Uptime;				  // Время обновления алгоритма пид регулятора (сек) Основной цикл управления
	  uint16_t PidFreqStep;           // Максимальный шаг (на увеличение) изменения частоты при ПИД регулировании в 0.01 Гц Необходимо что бы ЭРВ успевал
	  uint16_t PidStop;				  // Проценты от уровня защит (мощность, ток, давление, темпеартура) при которой происходит блокировка роста частоты пидом
	  uint16_t dtCompTemp;    		  // Защита по температуре компрессора - сколько градусов не доходит до максимальной (TCOMP) и при этом происходит уменьшение частоты
	  int16_t startFreq;              // Стартовая частота инвертора (см компрессор) в 0.01 
	  int16_t startFreqBoiler;        // Стартовая частота инвертора (см компрессор) в 0.01  ГВС
	  int16_t minFreq;                // Минимальная  частота инвертора (см компрессор) в 0.01 
	  int16_t minFreqCool;            // Минимальная  частота инвертора при охлаждении в 0.01 
	  int16_t minFreqBoiler;          // Минимальная  частота инвертора при нагреве ГВС в 0.01
	  int16_t minFreqUser;            // Минимальная  частота инвертора РУЧНОЙ РЕЖИМ (см компрессор) в 0.01 
	  int16_t maxFreq;                // Максимальная частота инвертора (см компрессор) в 0.01 
	  int16_t maxFreqCool;            // Максимальная частота инвертора в режиме охлаждения  в 0.01 
	  int16_t maxFreqBoiler;          // Максимальная частота инвертора в режиме ГВС в 0.01 Гц поглощение бойлера обычно меньше чем СО
	  int16_t maxFreqUser;            // Максимальная частота инвертора РУЧНОЙ РЕЖИМ (см компрессор) в 0.01 
	  int16_t stepFreq;               // Шаг уменьшения инвертора при достижении максимальной температуры, мощности и тока (см компрессор) в 0.01 
	  int16_t stepFreqBoiler;         // Шаг уменьшения инвертора при достижении максимальной температуры, мощности и тока ГВС в 0.01 
	  uint16_t dtTemp;                // Привышение температуры от уставок (подача) при которой срабатыват защита (уменьшается частота) в сотых градуса
	  uint16_t dtTempBoiler;          // Привышение температуры от уставок (подача) при которой срабатыват защита ГВС в сотых градуса
	#ifdef FC_ANALOG_CONTROL
	  int16_t  level0;                // Отсчеты ЦАП соответсвующие 0   частота
	  int16_t  level100;              // Отсчеты ЦАП соответсвующие максимальной частота
	  int16_t  levelOff;              // Минимальная мощность при котором частотник отключается (ограничение минимальной мощности)
	#endif
	  uint8_t  setup_flags;           // флаги настройки
   } _data;  // Структура для сохранения настроек, setup_flags всегда последний!
	  uint8_t  flags;                 // флаги настройки
     
  // Функции работы с OMRON MX2  Чтение регистров
  #ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
  int16_t  read_0x03_16(uint16_t cmd);             // Функция Modbus 0х03 прочитать 2 байта
  uint32_t read_0x03_32(uint16_t cmd);             // Функция Modbus 0х03 прочитать 4 байта
  int16_t  read_0x03_error(uint16_t cmd);          // Функция Modbus 0х03 описание ошибки num
  int8_t   write_0x05_bit(uint16_t cmd, boolean f);// Запись отдельного бита в регистр cmd возвращает код ошибки
  boolean  read_0x01_bit(uint16_t cmd);            // Чтение отдельного бита в регистр cmd возвращает код ошибки
  int8_t write_0x06_16(uint16_t cmd, uint16_t data);// Запись данных (2 байта) в регистр cmd возвращает код ошибки
  int8_t write_0x10_32(uint16_t cmd, uint32_t data);// Запись данных (4 байта) в регистр cmd возвращает код ошибки
  #endif
 };

#endif

// Класс Электрический счетчик SDM -----------------------------------------------------------------------------------------------
#ifdef USE_SDM630
const char *nameSDM = {"SDM630"};                               // Имя счетчика
#else
const char *nameSDM = {"SDM120"};                               // Имя счетчика
#endif
const char *noteSDM = {"Электрический счетчик с modbus"};       // Описание счетчика
const char *noteSDM_NONE = {"Отсутствует в конфигурации"};      //

// Флаги Электросчетчика
#define fSDM           0              // флаг наличие счетчика
#define fSDMLink       1              //  флаг связь установлена

// Структура для хранения настроек счетчика
struct type_settingSDM
{
uint16_t maxVoltage;                    // максимальное напряжение (вольты) иначе ошибка если 0 то не работает
uint16_t minVoltage;                    // минимальное напряжение (вольты) иначе ПРЕДУПРЕЖДЕНИЕ если 0 то не работает
uint16_t maxPower;                      // максимальная мощность (ватты) напряжение иначе ошибка если 0 то не работает
};
// Input register Function code 04 to read input parameters:
#ifdef USE_SDM630    // Регистры 3-х фазного счетчика SDM630.
	// Адрес уже уменьшен на 1
    #define SDM_VOLTAGE     42 
    #define SDM_CURRENT     48
    #define SDM_AC_POWER    52
    #define SDM_POWER       56
    #define SDM_RE_POWER    60
    #define SDM_POW_FACTOR  62
    #define SDM_PHASE       66
#else                                      // Регистры однофазного счетчика sdm120
    #define SDM_VOLTAGE          0x0000    // (2 слова 4 байта формат float) Line to neutral volts.  Напряжение
    #define SDM_CURRENT          0x0006    // (2 слова 4 байта формат float) Current.  Ток
    #define SDM_AC_POWER         0x000c    // (2 слова 4 байта формат float) Active power.  Активная мощность
    #define SDM_POWER            0x0012    // (2 слова 4 байта формат float) Apparent power.  Полная мощность
    #define SDM_RE_POWER         0x0018    // (2 слова 4 байта формат float) Reactive power.  Реактивная мощность
    #define SDM_POW_FACTOR       0x001e    // (2 слова 4 байта формат float) Power factor. Коэффициент мощности
    #define SDM_PHASE            0x0024    // (2 слова 4 байта формат float) Phase angle Угол фазы
#endif
// Общие регистры
#define SDM_FREQUENCY        0x0046        // (2 слова 4 байта формат float) Frequency частота
#define SDM_IMPORT_AC_ENERGY 0x0048        // (2 слова 4 байта формат float) Import active energy Потребленная активная энергия
#define SDM_EXPORT_AC_ENERGY 0x004a        // (2 слова 4 байта формат float) Export active energy Переданная активная энергия
#define SDM_IMPORT_RE_ENERGY 0x004c        // (2 слова 4 байта формат float) Import reactive energy Потребленная реактивная энергия
#define SDM_EXPORT_RE_ENERGY 0x004e        // (2 слова 4 байта формат float) Export reactive energy Переданная реактивная энергия
#define SDM_ENERGY           0x0156        // (2 слова 4 байта формат float) Общая энергия

#define SDM_AC_ENERGY        0x0156        // (2 слова 4 байта формат float) Total active energy Суммараная активная энергия
#define SDM_RE_ENERGY        0x0158        // (2 слова 4 байта формат float) Total reactive energy Суммараная реактивная энергия

// Modbus Protocol Holding Registers and Digital meter set up
// Function code 10 to set holding parameter ,function code 03 to read holding parameter
#define SDM_WIDTH_RELAY      0x000c        // (2 слова 4 байта формат float) Write relay on period in Width milliseconds: 60, 100 or 200, Ширина импульса на импульстом выходе
#define SDM_PARITY_STOP      0x0012        // (2 слова 4 байта формат float) Write the network port parity/stop bits for MODBUS Protocol, where: 0 = One stop bit and no parity, Network Parity Stop default. 1 = One stop bit and even parity. 2 = One stop bit and odd parity.3 = Two stop bits and no parity. Requires a restart to become effective.
#define SDM_ADR_MODBUS       0x0014        // (2 слова 4 байта формат float) Адрес на шине модбас
#define SDM_BAUD_RATE        0x001C        // (2 слова 4 байта формат float) Write the network port baud rate for MODBUS Protocol, where:0 = 2400 bps. 1 = 4800 bps.2 = 9600 bps( default) 5=1200 bps Requires a restart to become effective Data Format:float
#define SDM_CT_CURRENT       0x0032        // (2 слова 4 байта формат float) CT Primary current Ranges from 1-2000, Default ID is 5
#define SDM_RELAY_MODE       0x0056        // (2 слова 4 байта формат HEX) 0001: Import active energy, 0002: Import + export active energy, 0004: Export active energy, (default). 0005: Import reactive energy, 0006: Import + export reactive energy, 0008: Export reactive energy,
#define SDM_TIME_SCROLL      0xf500        // (2 слова 4 байта формат BCD) Time of scroll display Data Format: BCD 0-30s Default 0:does not display in turns
#define SDM_RELAY_CONST      0xf910        // (2 слова 4 байта формат HEX) Pulse constant Data Format:Hex 0000: 0.001kwh(kvarh)/imp (default) 0001: 0.01kwh(kvarh)/imp 0002: 0.1kwh(kvarh)/imp 0003: 1kwh(kvarh)/imp
#define SDM_MEASURE_MODE     0xf920        // (2 слова 4 байта формат HEX) Measurement mode Data Format:Hex 0001:mode 1(total = import) 0002:mode 2 (total = import + export)(default) 0003:mode 3 (total = import - export)

class devSDM
{
   public:  
       int8_t initSDM();                               // Инициализация счетчика и проверка и если надо программирование
       __attribute__((always_inline)) inline boolean get_present(){return GETBIT(flags,fSDM);} // Наличие счетчика в текущей конфигурации
      int8_t  get_readState(uint8_t group);            // Прочитать инфо с счетчика
      int8_t  get_lastErr(){return err;}               // Получить последнюю ошибку счетчика
      uint16_t get_numErr(){return numErr;}            // Получить число ошибок чтения счетчика
      char*   get_note(){return note;}                 // Получить описание датчика
      char*   get_name(){return name;}                 // Получить имя датчика
      float   get_power(){return AcPower;}
       __attribute__((always_inline)) inline float get_Voltage(){return Voltage;}          // Напряжение
       __attribute__((always_inline)) inline float get_Power(){return AcPower;}            // Aктивная мощность
       __attribute__((always_inline)) inline float get_Current(){return Current;}          // Ток
       __attribute__((always_inline)) inline float get_Energy(){return AcEnergy;}          // Активная энергия
         
      boolean uplinkSDM();                             // Проверить связь со счетчиком
      boolean progConnect();                           // перепрограммировать счетчик на требуемые параметры связи SDM_SPEED SDM_MODBUS_ADR c DEFAULT_SDM_SPEED DEFAULT_SDM_MODBUS_ADR
      char* get_paramSDM(char *var, char *ret);        // Получить параметр SDM в виде строки
      boolean set_paramSDM(char *var,char *c);         // Установить параметр SDM из строки
      
      uint8_t  *get_save_addr(void) { return (uint8_t *)&settingSDM; } // Адрес структуры сохранения
      uint16_t  get_save_size(void) { return sizeof(settingSDM); } // Размер структуры сохранения
      int32_t save(int32_t adr);                       // Записать настройки в eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
      int32_t load(int32_t adr);                       // Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
      int32_t loadFromBuf(int32_t adr,byte *buf);      // Считать настройки из буфера на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
      uint16_t get_crc16(uint16_t crc);                // Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
       // Графики из счетчика
      statChart ChartVoltage;                              // Статистика по напряжению
      statChart ChartCurrent;                              // Статистика по току
      statChart ChartPower;                            // Статистика по Полная мощность
  private:
      int8_t  err;                                     // ошибка стесчика (работа)
      uint16_t numErr;                                 // число ошибок чтение по модбасу
      byte flags;                                      // флаги  0 - наличие счетчика,
       // Управление по 485
      float Voltage;                                   // Напряжение
      float Current;								   // Ток
      float AcPower;                                   // активная мощность
      float AcEnergy;                                  // Суммарная активная энергия
      
      type_settingSDM  settingSDM;                     // Настройки
      char *note;                                      // Описание
      char *name;                                      // Имя
};


// Класс устройство Модбас  -----------------------------------------------------------------------------------------------
#define fModbus    			0               // флаг наличие modbus
class devModbus
  {
  public:  
    int8_t initModbus();                                                                // Инициализация Modbus и проверка связи возвращает ошибку
     __attribute__((always_inline)) inline boolean get_present(){return GETBIT(flags,fModbus);} // Наличие Modbus в текущей конфигурации
    int8_t readInputRegistersFloat(uint8_t id, uint16_t cmd, float *ret);                  // Получить значение 2-x (Modbus function 0x04 Read Input Registers) регистров (4 байта) в виде float возвращает код ошибки данные кладутся в ret
    int8_t readHoldingRegisters16(uint8_t id, uint16_t cmd, uint16_t *ret);                 // Получить значение регистра (2 байта) МХ2 в виде целого  числа возвращает код ошибки данные кладутся в ret
    int8_t readHoldingRegisters32(uint8_t id, uint16_t cmd, uint32_t *ret);                // Получить значение 2-x регистров (4 байта) МХ2 в виде целого числа возвращает код ошибки данные кладутся в ret
    int8_t readHoldingRegistersFloat(uint8_t id, uint16_t cmd, float *ret);                // Получить значение 2-x регистров (4 байта) в виде float возвращает код ошибки данные кладутся в ret
    int8_t readHoldingRegistersNN(uint8_t id, uint16_t cmd, uint16_t num,uint16_t *buf);   // Получить значение N регистров (2*N байта) МХ2 (положить в buf) возвращает код ошибки
    int8_t writeSingleCoil(uint8_t id,uint16_t cmd, uint8_t u8State);                      // установить битовый вход, возвращает код ошибки Modbus function 0x05 Write Single Coil.
    int8_t readCoil(uint8_t id,uint16_t cmd, boolean *ret);                                // прочитать отдельный бит, возвращает ошибку Modbus function 0x01 Read Coils.
    int8_t writeHoldingRegisters16(uint8_t id, uint16_t cmd, uint16_t data);               // Установить значение регистра (2 байта) МХ2 в виде целого  числа возвращает код ошибки данные data
    int8_t writeHoldingRegistersFloat(uint8_t id, uint16_t cmd, float dat);                // Записать float как 2 регистра числа возвращает код ошибки данные data
    #ifndef FC_VACON
    int8_t LinkTestOmronMX2();                                                             // Тестирование связи c МХ2 (актуально только с omronom) возвращает код ошибки
    #endif
    int8_t writeHoldingRegisters32(uint8_t id,uint16_t cmd, uint32_t data); // Записать 2 регистра подряд возвращает код ошибки
    int8_t get_err() {return err;}                                                         // Получить код ошибки
private:
    // Переменные
    int8_t flags;                           // Флаги
    int8_t err;                             // Ошибки модбас
    ModbusMaster RS485;                     // Класс модбас 485
    int8_t translateErr(uint8_t result);    // Перевод ошибки протокола Модбас в ошибки ТН
  }; // End class

 #endif
