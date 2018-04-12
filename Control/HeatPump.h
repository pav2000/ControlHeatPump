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
// Описание базовых классов для работы Теплового Насоса
// --------------------------------------------------------------------------------
#ifndef HeatPump_h
#define HeatPump_h
#include "Constant.h"                       // Вся конфигурация и константы проекта Должен быть первым !!!!
#include "Hardware.h"
#include "Message.h"
#include "Information.h"
#include "VaconFC.h" 
#include "Scheduler.h"

// Структура для хранения заголовка при сохранении настроек EEPROM
struct type_headerEEPROM    // РАЗМЕР 1+1+2+2=6 байт
{
  byte magic;               // признак данных, должно быть  0xaa
  byte zero;                // признак данных, должно быть  0x00
  uint16_t ver;             // номер версии для сохранения
  uint16_t len;             // длина данных, сколько записано байт в еепром
 };

// Структура описывающая текущее состояние ТН
struct type_status
{
   MODE_HP modWork;         // Что делает ТН если включен (7 стадий)  0 Пауза 1 Включить отопление  2 Включить охлаждение  3 Включить бойлер   4 Продолжаем греть отопление 5 Продолжаем охлаждение 6 Продолжаем греть бойлер
   TYPE_STATE_HP State;     // Состояние теплового насоса  0 ТН выключен   1 Стартует 2 Останавливается  3 Работает  5 Ошибка ТН  6 - Эта ошибка возникать не должна!
   int8_t ret;              // Точка выхода из алгоритма регулирования
}; 
// Структура для хранения различных счетчиков (максимальный размер 128-1 байт!!!!!)
struct type_motoHour
{
  byte magic;       // признак данных, должно быть  0xaa  пока не используется!!!!!!!!!!
  uint8_t  flags;   // флаги                  пока не используется!!!!!!!!!!
  uint32_t H1;      // моточасы ТН ВСЕГО
  uint32_t H2;      // моточасы ТН сбрасываемый счетчик (сезон)
  uint32_t C1;      // моточасы компрессора ВСЕГО
  uint32_t C2;      // моточасы компрессора сбрасываемый счетчик (сезон)
  float    E1;      // Значение потребленный энергии в момент пуска  актуально при использовании счетчика SDM120 (вычитаем текущее и получам итого)
  float    E2;      // Значение потребленный энергии в начале сезона актуально при использовании счетчика SDM120 (вычитаем текущее и получам итого)
  uint32_t D1;      // Дата сброса общих счетчиков
  uint32_t D2;      // дата сброса сезонных счетчиков
  uint32_t P1;      // выработанное тепло  ВСЕГО
  uint32_t P2;      // выработанное тепло  сбрасываемый счетчик (сезон)
 // uint32_t Q1;      // Объем прочаченного теплоносителя в СО ВСЕГО
 // uint32_t Q2;      // Объем прокаченного теплоносителя в СО (сезон)
  uint32_t Z1;      // Резервный параметр 1
  uint32_t Z2;      // Резервный параметр 2
  
};

//  Работа с отдельными флагами type_optionHP
#define fAddHeat           0                // флаг Использование дополнительного тена при нагреве
#define fBeep              1                // флаг Использование звука
#define fNextion           2                // флаг Использование nextion дисплея
#define fEEV_close         3                // флаг Закрытие ЭРВ при выключении компрессора
#define fSD_card           4                // флаг записи статистики на карту памяти
#define fSaveON            5                // флаг записи в EEPROM включения ТН
#define fEEV_start         6                // флаг Всегда начинать работу ЭРВ со стратовой позиции
#define fEEV_light_start   7                // флаг Облегчение пуска компрессора при старте ЭРВ открывается после запуска уходит на рабочуюю позицию
#define fTypeRHEAT         8                // флаг как используется доболнительный ТЭН для нагрева 0-резерв 1-бивалент
#define fAddBoiler         9                // флаг догрева ГВС ТЭНом
 
// Структура для хранения опций теплового насоса.
struct type_optionHP
{
 int8_t numProf;                       //  Текущий номер профиля 0 стоит по дефолту
 uint16_t flags;                       //  Флаги опций до 16 флагов
 uint8_t nStart;                       //  Число попыток пуска компрессора
 uint8_t sleep;                        //  Время засыпания дисплея секунды
 uint8_t dim;                          //  Якрость дисплея %
 uint16_t tChart;                      //  период сбора статистики в секундах!!
 int16_t tempRHEAT;                    //  Значение температуры для управления дополнительным ТЭН для нагрева СО
 uint16_t pausePump;                   //  Время паузы  насоса при выключенном компрессоре МИНУТЫ
 uint16_t workPump;                    //  Время работы  насоса при выключенном компрессоре МИНУТЫ
 uint16_t tempRBOILER;                 //  Темпеартура ГВС при котором включается бойлер и отключатся ТН
 uint16_t P1;                          //  резерв два байта
 uint16_t P2;                          //  резерв два байта
};


//  Работа с отдельными флагами type_DateTimeHP
#define fUpdateNTP     0                // флаг Обновление часов по NTP при старте
#define fUpdateI2C     1                // флаг Обновление часов раз в час с I2C  часами
// Структура для хранения настроек времени, для удобного сохранения.
struct type_DateTimeHP
{
    uint8_t flags;                        //  Флаги опций до 8 флагов
    int8_t timeZone;                      //  Часовой пояс
    char serverNTP[NTP_SERVER_LEN+1];     //  Адрес NTP сервера
    uint32_t saveTime;                    //  дата и время сохранения настроек в eeprom
    uint32_t P1;                          //  резерв 4 байта
};

//  Работа с отдельными флагами type_NetworkHP
#define fDHCP         0                  // флаг Использование DHCP
#define fPass         1                  // флаг Использование паролей
#define fInitW5200    2                  // флаг Ежеминутный контроль SPI для сетевого чипа
#define fNoAck        4                  // флаг Не ожидать ответа ACK
#define fNoPing       5                  // флаг Запрет пинга контроллера

// Структура для хранения сетевых настроек, для удобного сохранения.
struct type_NetworkHP
{
    uint16_t flags;                       // !save! Флаги
    IPAddress ip;                         // !save! ip адрес
    IPAddress sdns;                       // !save! сервер dns
    IPAddress gateway;                    // !save! шлюз
    IPAddress subnet;                     // !save!подсеть
    byte mac[6];                          // !save! mac адрес
    uint16_t resSocket;                   // !save! Время очистки сокетов секунды
    uint32_t resW5200;                    // !save! Время сброса чипа     секунды
    char passUser[PASS_LEN+1];            // !save! Пароль пользователя
    char passAdmin[PASS_LEN+1];           // !save! Пароль администратора
    uint16_t sizePacket;                  // !save! Размер пакета для отправки в байтах
    uint16_t port;                        // !save! порт веб сервера
    uint8_t delayAck;                     // !save! задержка мсек перед отправкой пакета
    char pingAdr[40];                     // !save! адрес для пинга, может быть в любом виде
    uint16_t pingTime;                    // !save! время пинга в секундах
    uint16_t P0;                          // !save! резерв два байта
    uint16_t P1;                          // !save! резерв два байта
    uint16_t P2;                          // !save! резерв два байта
};

// Структура для хранения переменных для паролей
struct type_SecurityHP
{
  char hashUser[80];                      // Хеш для пользоваетля
  uint16_t hashUserLen;                   // Длина хеша пользователя
  char hashAdmin[80];                     // Хеш для администратора
  uint16_t hashAdminLen;                  // Длина хеша администратора
};

// Структура для хранения состояния ТН в момент работы.
struct type_statusHP
{
 uint32_t comp_ON;                        // Время включения компрессора
 uint32_t comp_OFF;                       // Время выключения компрессора
 uint32_t pumpCO_ON;                      // Время включения насоса системы отопления
 uint32_t pumpCO_OFF;                     // Время выключения насоса системы отопления
};


// ------------------------- ОСНОВНОЙ КЛАСС --------------------------------------
class HeatPump
  {
  public:
    void initHeatPump();                                     // Конструктор
     __attribute__((always_inline)) inline MODE_HP get_modWork()   {return Status.modWork;}  // Получить что делает ТН ?????? надо порядок навести
     __attribute__((always_inline)) inline TYPE_STATE_HP get_State() {return Status.State;}  // Получить состяние теплового насоса
     __attribute__((always_inline)) inline int8_t get_ret() {return Status.ret;}             // Точка выхода из алгоритма регулирования
    void vUpdate();                                          // Итерация по управлению всем ТН - старт-стоп
    void eraseError();                                       // стереть последнюю ошибку

    __attribute__((always_inline)) inline int8_t get_errcode(){return error;} // Получить код последней ошибки
    char *get_lastErr();                // Получить описание последней ошибки, которая вызвала останов ТН, при удачном запуске обнуляется
    void scan_OneWire(char *result_str); // Сканирование шины OneWire на предмет датчиков
    TEST_MODE get_testMode();           // Получить текущий режим работы
    void  set_testMode(TEST_MODE t);    // Установить значение текущий режим работы
    boolean get_relay3Way(){return relay3Way;} // Получить состояние трехходового точнее если true то идет нагрев бойлера
    boolean get_fSD() {return fSD;}     // Получить флаг наличия РАБОТАЮЩЕЙ СД карты
    void set_fSD(boolean f) {fSD=f;}    // Установить флаг наличия РАБОТАЮЩЕЙ СД карты
    uint32_t get_errorReadDS18B20();    // Получить число ошибок чтения датчиков темпеартуры

    void sendCommand(TYPE_COMMAND c);   // Послать команду на управление ТН
    __attribute__((always_inline)) inline TYPE_COMMAND isCommand()  {return command;}  // Получить текущую команду выполняемую ТН
    int8_t  runCommand();               // Выполнить команду по управлению ТН

    // Строковые функции
    char *StateToStr();                 // Получить состояние ТН в виде строки
    char *StateToStrEN();               // Получить состояние ТН в виде английской строки
    char *TestToStr();                  // Получить режим тестирования
    int8_t save_DumpJournal(boolean f); // Записать состояние теплового насоса в журнал
    
    int32_t save();                     // Записать настройки в eeprom i2c на входе адрес с какого, на выходе код ошибки (меньше нуля) или количество записанных  байт
    int8_t load();                      // Считать настройки из eeprom i2c на входе адрес с какого, на выходе код ошибки (меньше нуля) или количество прочитанных байт
    int8_t loadFromBuf(int32_t adr,byte *buf);// Считать настройки из буфера на входе адрес с какого, на выходе код ошибки (меньше нуля)
    int8_t save_motoHour();             // запись счетчиков теплового насоса в ЕЕПРОМ
    int8_t load_motoHour();             // чтение счетчиков теплового насоса в ЕЕПРОМ
    
  //  ===================  К Л А С С Ы  ===========================
  // Датчики
    sensorTemp sTemp[TNUMBER];          // Датчики температуры
    #ifdef SENSOR_IP                    // Получение данных удаленного датчика
      sensorIP sIP[IPNUMBER];           // Массив удаленных датчиков
    #endif   
    sensorADC sADC[ANUMBER];            // Датчик аналоговый
    sensorDiditalInput sInput[INUMBER]; // Контактные датчики
    sensorFrequency sFrequency[FNUMBER]; // Частотные датчики
    
  // Устройства  исполнительные
    devRelay dRelay[RNUMBER];           // Реле
    #ifdef EEV_DEF
    devEEV dEEV;                        // ЭРВ
    #endif
    #ifdef FC_VACON
        devVaconFC dFC;                // Частотник VACON
    #else
        devOmronMX2 dFC;               // Частотник OMRON
    #endif
    #ifdef USE_ELECTROMETER_SDM
      devSDM dSDM;                      // Счетчик
    #endif

   // Сервис
   Message message;                     // Класс уведомления
   Profile Prof;                        // Текуший профиль
   #ifdef USE_SCHEDULER
     Scheduler Schdlr;					// Расписание
   #endif

   #ifdef MQTT
      clientMQTT clMQTT;                // MQTT клиент
   #endif
   #ifdef I2C_EEPROM_64KB  
      Statistics Stat;                 // Статистика работы теплового насоса
   #endif
  // Сетевые настройки
    boolean set_network(PARAM_NETWORK p, char *c);        // Установить параметр из строки
    char*   get_network(PARAM_NETWORK p);                 // Получить параметр из строки
  //  inline uint16_t get_sizePacket() {return Network.sizePacket;} // Получить размер пакета при передаче
    inline uint16_t get_sizePacket() {return 2048;} // Получить размер пакета при передаче
    
    uint8_t set_hashUser();                               // расчитать хеш для пользователя возвращает длину хеша
    uint8_t set_hashAdmin();                              // расчитать хеш для администратора возвращает длину хеша
    
   // Дата время
    boolean set_datetime(DATE_TIME p, char *c);            //  Установить параметр дата и время из строки
    char*   get_datetime(DATE_TIME p);                     //  Получить параметр дата и время из строки
    IPAddress get_ip() { return Network.ip;}               //  Получить ip адрес
    IPAddress get_sdns() { return Network.sdns;}           //  Получить sdns адрес
    IPAddress get_subnet() { return Network.subnet;}       //  Получить subnet адрес
    IPAddress get_gateway() { return Network.gateway;}     //  Получить gateway адрес
    uint16_t get_port() {return Network.port;}             //  получить порт вебсервера
    boolean get_NoAck() { return GETBIT(Network.flags,fNoAck);}  //  Получить флаг Не ожидать ответа ACK
    uint8_t get_delayAck() {return Network.delayAck;}      //  получить задержку перед отсылкой следующего пакета
    uint16_t get_pingTime() {return Network.pingTime;}     //  получить вермя пингования сервера, 0 если не надо
    char * get_pingAdr() {return Network.pingAdr;}         //  получить адрес сервера для пингования
    boolean get_NoPing() { return GETBIT(Network.flags,fNoPing);} //  Получить флаг блокировки пинга
        
    boolean get_DHCP() { return GETBIT(Network.flags,fDHCP);}    //  Получить использование DHCP
    byte *get_mac() { return Network.mac;}                 //  Получить mac адрес
    uint32_t socketRes() {return countResSocket;}          //  Получить число сбросов сокетов
    void add_socketRes() {countResSocket++;}               //  Добавить 1 к счетчику число сбросов сокетов
    uint32_t time_socketRes() {return Network.resSocket;}  //  Получить период сбросов сокетов
    uint32_t time_resW5200() {return Network.resW5200;}    //  Получить период сбросов W5200
    boolean get_fPass() { return GETBIT(Network.flags,fPass);}   //  Получить флаг необходимости идентификации
    boolean get_fInitW5200() { return GETBIT(Network.flags,fInitW5200);}  //  Получить флаг Контроля w5200

  // Параметры ТН
   boolean set_optionHP(OPTION_HP p, float x);             // Установить опции ТН из числа (float)
   char*   get_optionHP(OPTION_HP p);                      // Получить опции ТН
   void set_mode(MODE_HP b) {Prof.SaveON.mode=b;}          // Установить режим работы отопления
   void set_nextMode();                                    // Переключение на следующий режим работы отопления (последовательный перебор режимов)
   void set_profile();										// Установить рабочий профиль по текущему Prof

   MODE_HP get_mode() {return Prof.SaveON.mode;}                // Получить режим работы отопления
   RULE_HP get_ruleCool(){return Prof.Cool.Rule;}               // Получить алгоритм охлаждения
   RULE_HP get_ruleHeat(){return Prof.Heat.Rule;}               // Получить алгоритм отопления
   boolean get_TargetCool(){return GETBIT(Prof.Cool.flags,fTarget);}  // Получить цель 0 - Дом 1 - Обратка
   boolean get_TargetHeat(){return GETBIT(Prof.Heat.flags,fTarget);}  // Получить цель 0 - Дом 1 - Обратка
   uint16_t get_timeCool(){return Prof.Cool.time;}              // Получить время интегрирования охлаждения
   uint16_t get_timeHeat(){return Prof.Heat.time;}              // Получить время интегрирования отопления
   uint16_t get_timeBoiler(){return Prof.Boiler.time;}          // Получить время интегрирования ГВС
   
   int16_t get_targetTempCool();                           // Получить целевую температуру Охлаждения
   int16_t get_targetTempHeat();                           // Получить целевую температуру Отопления
   int16_t setTargetTemp(int16_t dt);                      // ИЗМЕНИТЬ целевую температуру
   int16_t setTempTargetBoiler(int16_t dt);                // ИЗМЕНИТЬ целевую температуру бойлера
   boolean scheduleBoiler();                               // Проверить расписание бойлера true - нужно греть false - греть не надо

   // Опции ТН
   uint16_t get_pausePump() {return Option.pausePump;};                // !save! Время паузы  насоса при выключенном компрессоре МИНУТЫ
   uint16_t get_workPump() {return Option.workPump;};                  // !save! Время работы  насоса при выключенном компрессоре МИНУТЫ
   uint8_t  get_Beep() {return GETBIT(Option.flags,fBeep);};           // !save! подача звуковых сигналов
   uint8_t  get_SaveON() {return GETBIT(Option.flags,fSaveON);}        // !save! получить флаг записи состояния
   uint8_t  get_nStart() {return Option.nStart;};                      // получить максимальное число попыток пуска ТН
   void     updateNextion();                                           // Обновить настройки дисплея
   uint8_t  get_sleep() {return Option.sleep;};                        // 
   
   // Структура состояния ТН Prof.SaveON.
   inline void  set_HP_OFF()                                                // Сброс флага включения ТН
    { SETBIT0(Prof.SaveON.flags,fHP_ON); Status.State=pOFF_HP;}
   inline void  set_HP_ON()                                                 // Установка флага включения ТН
    { SETBIT1(Prof.SaveON.flags,fHP_ON); Status.State=pWORK_HP;}
    
   boolean  get_HP_ON() {return GETBIT(Prof.SaveON.flags,fHP_ON);}          // Получить значение флага включения ТН

   void     set_BoilerOFF(){SETBIT0(Prof.SaveON.flags,fBoilerON);}          // Выключить бойлер
   void     set_BoilerON() {SETBIT1(Prof.SaveON.flags,fBoilerON);}          // Включить бойлер
   boolean  get_BoilerON() {return GETBIT(Prof.SaveON.flags,fBoilerON);}    // Получить флаг включения бойлера
      
   void     set_startTime(uint32_t t){Prof.SaveON.startTime = t;}           // Запомить время включения ТН
   
   // Бойлер ТН
    int16_t get_boilerTempTarget(){return Prof.Boiler.TempTarget;}          // Получить целевую температуру бойлера
    boolean get_Circulation(){return GETBIT(Prof.Boiler.flags,fCirculation);} // Нужно ли управлять циркуляционным насосом болйлера
    uint16_t get_CirculWork(){ return Prof.Boiler.Circul_Work; }            // Время  работы насоса ГВС секунды (fCirculation)
    uint16_t get_CirculPause(){ return Prof.Boiler.Circul_Pause;}           // Пауза в работе насоса ГВС  секунды (fCirculation)

      
   // Времена
   void set_countNTP(uint32_t b) {countNTP=b;}             // Установить текущее время обновления по NTP, (секундах)
   uint32_t get_countNTP()  {return countNTP;}             // Получить время последнего обновления по NTP (секундах)
   void set_updateNTP(boolean b);                          // Установить синхронизацию по NTP
   boolean get_updateNTP();                                // Получить флаг возможности синхронизации по NTP
   unsigned long get_saveTime(){return  DateTime.saveTime;}// Получить время сохранения текущих настроек
   char* get_serverNTP() {return DateTime.serverNTP;}      // Получить адрес сервера
   void updateDateTime(int32_t  dTime);                    // После любого изменения часов необходимо пересчитать все времна которые используются
   boolean  get_updateI2C(){return GETBIT(DateTime.flags,fUpdateI2C);}// Получить необходимость обновления часов I2C
   unsigned long timeNTP;                                  // Время обновления по NTP в тиках (0-сразу обновляемся)
   
   __attribute__((always_inline)) inline uint32_t get_uptime();// Получить время с последенй перезагрузки в секундах
   uint32_t get_startDT();                                 // Получить дату и время последеней перезагрузки
   uint32_t get_startCompressor(){return startCompressor;} // Получить дату и время пуска компрессора! нужно для старта ЭРВ
   uint32_t get_startTime(){return Prof.SaveON.startTime;} // Получить дату и время пуска ТН (не компрессора!)
   uint32_t get_motoHourH1(){return motoHour.H1;}          // Получить моточасы ТН ВСЕГО
   uint32_t get_motoHourH2(){return motoHour.H2;}          // Получить моточасы ТН сбрасываемый счетчик (сезон)
   uint32_t get_motoHourC1(){return motoHour.C1;}          // моточасы компрессора ВСЕГО
   uint32_t get_motoHourC2(){return motoHour.C2;}          // моточасы компрессора сбрасываемый счетчик (сезон)
   float    get_motoHourE1(){return motoHour.E1;}          // Значение потреленный энергии в момент пуска  актуально при использовании счетчика SDM120
   float    get_motoHourE2(){return motoHour.E2;}          // Значение потреленный энергии в начале сезона актуально при использовании счетчика SDM120
   uint32_t get_motoHourD2(){return motoHour.D2;}          // дата сброса сезонных счетчиков
   uint32_t get_motoHourD1(){return motoHour.D1;}          // Дата сброса общих счетчиков
   uint32_t get_motoHourP2(){return motoHour.P2;}          // Выработанная энергия в вт/часах сезон
   uint32_t get_motoHourP1(){return motoHour.P1;}          // Выработанная энергия в вт/часах общая
   
   void resetCount(boolean full);                          // Сборос сезонного счетчика моточасов
   void updateCount();                                     // Обновление счетчиков моточасов
   
   void set_uptime(unsigned long ttime);                   // Установить текущее время как начало старта контроллера
  
    // Переменные
    uint8_t CPU_IDLE;                                      // загрузка CPU
    uint32_t mRTOS;                                        // Память занимаемая задачами
    uint32_t startRAM;                                     // Свободная память при старте FREE Rtos - пытаемся определить свободную память при работе
       
    int16_t  lastEEV;                                      // + значение шагов ЭРВ перед выключением  -1 - первое включение
    uint16_t num_repeat;                                   // + текущее число повторов пуска ТН
    uint16_t num_resW5200;                                 // + текущее число сброса сетевого чипа
    uint16_t num_resMutexSPI;                              // + текущее число сброса митекса SPI
    uint16_t num_resMutexI2C;                              // + текущее число сброса митекса I2C
    uint16_t num_resMQTT;                                  // + число повторных инициализация MQTT клиента
    uint16_t num_resPing;                                  // + число не прошедших пингов
    
    uint16_t AdcVcc;                                       // напряжение питания
  //  uint16_t AdcTempSAM3x;                                 // температура чипа
    
    boolean PauseStart;                                    // True начать отсчет времени с начала при отложенном старте
       
    boolean startPump;                                     // Признак запуска задачи насос false - останов задачи true запуск
    type_headerEEPROM headerEEPROM;                        // Заголовок записи настроек
    type_SecurityHP Security;                              // хеш паролей
    boolean safeNetwork;                                   // Режим работы safeNetwork (сеть по умолчанию, паролей нет)
   
    // функции для работой с графикками
    uint16_t get_tChart(){return Option.tChart;}           // Получить время накопления ститистики в секундах
    void updateChart();                                     // обновить статистику
    void startChart();                                      // Запуститьь статистику
    char * get_listChart(char* str, boolean cat);          // получить список доступных графиков
    char * get_Chart(TYPE_CHART t,char* str, boolean cat); // получить данные графика
  
    // графики не по датчикам (по датчикам она хранится внутри датчика)
    statChart ChartRCOMP;                                   // Статистика по включению компрессора
    statChart ChartdCO;                                     // дельта СО
    statChart ChartdGEO;                                    // дельта геоконтура
    #ifdef EEV_DEF
    statChart ChartOVERHEAT;                                // перегрев
    statChart ChartTPEVA;                                   // температура расчитанная из давления испариения
    statChart ChartTPCON;                                   // температура расчитанная из давления Конденсации
    #endif
    statChart ChartPowerCO;                                 // выходная мощность насоса
    statChart ChartPowerGEO;                                // Мощность геоконтура
    statChart ChartCOP;                                     // Коэффициент преобразования
    statChart ChartFullCOP;                                 // ПОЛНЫЙ Коэффициент преобразования
    
    float powerCO;                                         // Мощность системы отопления
    float powerGEO;                                        // Мощность системы GEO
    float power220;                                        // Мощность системы 220
    
    #ifdef I2C_EEPROM_64KB   // Статистика ----------------------------------------------------------------------
    void InitStatistics();    // Функция вызываемая для первого часа для инициализации первичных счетчиков
    void UpdateStatistics();  // обновление статистики дня, если нужно то пишет в память полный день и открывает новый (вызывать надо раз в час)
    #endif
    
    // Удаленные датчики
    #ifdef SENSOR_IP
    boolean updateLinkIP();                                // Обновить ВСЕ привязки удаленных датчиков
    #endif        

   
    TaskHandle_t xHandleUpdate;                            // Заголовок задачи "Обновление ТН"
    #ifdef EEV_DEF
    TaskHandle_t xHandleUpdateEEV;                         // Заголовок задачи "Обновление ЭРВ"
    #endif
    TaskHandle_t xHandleUpdateCommand;                     // Разбор очереди команд
    TaskHandle_t xHandleReadSensor;                        // Заголовок задачи "Чтение датчиков"
    TaskHandle_t xHandleUpdateStat;                        // Заголовок задачи "Обновление ститистики"
    TaskHandle_t xHandleUpdatePump;                        // Заголовок задачи "Работа насоса при выключенном компрессоре"
    TaskHandle_t xHandleUpdateWeb0;                        // Заголовок задачи "Веб сервер"
    TaskHandle_t xHandleUpdateWeb1;                        // Заголовок задачи "Веб сервер"
    TaskHandle_t xHandleUpdateWeb2;                        // Заголовок задачи "Веб сервер"
    TaskHandle_t xHandleUpdateWeb3;                        // Заголовок задачи "Веб сервер"
    TaskHandle_t xHandleUpdateNextion;                     // заголовок задачи "Обновление дисплея nextion"
    TaskHandle_t xHandlePauseStart;                        // заголовок задачи "Отложенный старт"

    SemaphoreHandle_t xCommandSemaphore;                   // Семафор команды
    
  private:
    int8_t Start();                       // Запустить ТН - возвращает ок или код ошибки
    int8_t Stop();                        // Остановить ТН
    int8_t ResetFC();                     // Сброс инвертора если он стоит в ошибке, провиряется наличие инвертора и прорверка ошибки

    MODE_HP get_Work();                   // Что надо делать
    void configHP(MODE_HP conf);          // Концигурация 4-х, 3-х ходового крана и включение насосов
    void ChangesPauseTRV();               // "Интелектуальная пауза" для перекидывания на "ходу" 4-х ходового
    void defrost();                       // Все что касается разморозки воздушника
    void setStatusRet(TYPE_RET_HP ret,uint32_t t); // Установить КОД сосояния и вывести его в консоль о текущем состоянии + пауза t сек

    void resetSettingHP();                // Функция сброса настроек охлаждения и отопления
    void relayAllOFF();                   // Все реле выключить
    #ifdef  RTRV
    void set_RTRV(uint16_t d);            // Поставить 4х ходовой в нужное положение для работы в заваисимости от Prof.SaveON.mode параметр задержка после включения mсек.
    #endif
    boolean  switch3WAY(boolean b);       //  Переключение 3-х ходового крана с обеспечением паузы после переключения
    boolean checkEVI();                   // Проверка и если надо включение EVI если надо то выключение возвращает состояние реле
    void Pumps(boolean b,  uint16_t d);   // Включение/выключение насосов, задержка после включения msec
    MODE_COMP UpdateHeat();               // Итерация нагрев  вход true - делаем, false - ТОЛЬКО проверяем выход что сделано или надо сделать
    MODE_COMP UpdateCool();               // Итерация охлаждение вход true - делаем, false - ТОЛЬКО проверяем выход что сделано или надо сделать
    MODE_COMP UpdateBoiler();             // Итерация управления бойлером возвращает что делать компрессору
    void compressorON(MODE_HP mod);       // попытка включить компрессор  с учетом всех защит
    void compressorOFF();                 // попытка выключить компрессор  с учетом всех защит
    uint16_t get_crc16_mem();             // Рассчитать контрольную сумму в ПАМЯТИ для структуры дынных на выходе crc16
    int8_t check_crc16_eeprom(int32_t adr);// Проверить контрольную сумму в EEPROM для данных на выходе ошибка, длина определяется из заголовка
    int8_t check_crc16_buf(int32_t adr, byte* buf);// Проверить контрольную сумму в буфере для данных на выходе ошибка, длина определяется из заголовка
  
    // Использование ночного тарифа вычисление гистерезиса
    int16_t get_dTempBoiler();                              // Получить гистерезис в зависмости от времени суток БОЙЛЕР
    int16_t get_dTempHeat();                                // Получить гистерезис в зависмости от времени суток ОТОПЛЕНИЕ
    int16_t get_dTempCool();                                // Получить гистерезис в зависмости от времени суток ОХЛАЖДЕНИЕ
         
    type_motoHour motoHour;               // Структура для хранения счетчиков запись каждый час
    TEST_MODE testMode;                   // Значение режима тестирования
    TYPE_COMMAND command;                 // Текущая команда управления ТН
    type_status Status;                   // Описание состояния ТН
    boolean setState(TYPE_STATE_HP st);   // установить состояние теплового насоса
      
    // Ошибки
    int8_t error;                         // Код ошибки
    char   source_error[16];              // источник ошибки
    char   note_error[160+1];             // Строка c описанием ошибки формат "время источник:описание"
    boolean fSD;                          // Признак наличия РАБОТАЮЩЕЙ SD карты
    boolean relay3Way;                    // Cостояние трехходового точнее если true то идет нагрев бойлера
   
    // Различные времена
    type_DateTimeHP DateTime;             // структура где хранится все что касается времени и даты
    uint32_t timeON;                      // время включения контроллера для вычисления UPTIME
    uint32_t countNTP;                    // число секунд с последнего обновления по NTP
    uint32_t startCompressor;             // время пуска компрессора
    uint32_t stopCompressor;              // время останова компрессора
    uint32_t offBoiler;                   // время выключения нагрева ГВС ТН (необходимо для переключения на другие режимы на ходу)
    uint32_t startDefrost;                // время срабатывания датчика разморозки
    uint32_t timeBoilerOff;               // Время переключения (находу) с ГВС на отопление или охлаждения (нужно для временной блокировки защит) если 0 то переключения не было
   
    
    // Сетевые настройки
    type_NetworkHP Network;                 // !save! Структура для хранения сетевых настроек
    uint32_t countResSocket;                // Число сбросов сокетов

     // Настройки опций
    type_optionHP Option;                 // Опции теплового насоса
 
    // Переменные пид регулятора Отопление
    float temp_int;                       // Служебная переменная интегрирования
    float errPID;                         // Текущая ошибка ПИД регулятора
    float pre_errPID;                     // Предыдущая ошибка ПИД регулятора
    unsigned long updatePidTime;          // время обновления ПИДа отопления
    
    // Переменные пид регулятора ГВС
    float temp_intBoiler;                 // Служебная переменная интегрирования ГВС
    float errPIDBoiler;                   // Текущая ошибка ПИД регулятора
    float pre_errPIDBoiler;               // Предыдущая ошибка ПИД регулятора
    unsigned long updatePidBoiler;        // время обновления ПИДа ГВС
    boolean flagRBOILER;                  // true - идет цикл нагрева бойлера
    
    SdFile  statFile;                       // файл для записи статистики

    friend int8_t set_Error(int8_t err, char *nam );// Установка критической ошибки для класса ТН
  };

 
#endif
