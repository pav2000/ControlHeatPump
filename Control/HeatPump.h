/*
 * Copyright (c) 2016-2018 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav; by vad711 (vad7@yahoo.com)
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
extern char *MAC2String(byte* mac);
 
/*/ Структура для хранения заголовка при сохранении настроек EEPROM
struct type_headerEEPROM    // РАЗМЕР 1+1+2+2=6 байт
{
//  byte magic;               // признак данных, должно быть  0xaa
//  byte zero;                // признак данных, должно быть  0x00
  uint16_t ver;             // номер версии для сохранения
  uint16_t len;             // длина данных, сколько записано байт в еепром
 };
*/

// Структура описывающая текущее состояние ТН (хранится только в памяти)
struct type_status
{
   MODE_HP modWork;         // Что делает ТН если включен (7 стадий)  0 Пауза 1 Включить отопление  2 Включить охлаждение  3 Включить бойлер   4 Продолжаем греть отопление 5 Продолжаем охлаждение 6 Продолжаем греть бойлер
   TYPE_STATE_HP State;     // Состояние теплового насоса  0 ТН выключен   1 Стартует 2 Останавливается  3 Работает 4 Ожидание ТН (расписание - пустое место) 5 Ошибка ТН  6 - Эта ошибка возникать не должна!
   int8_t ret;              // Точка выхода из алгоритма регулирования
}; 
// Структура для хранения различных счетчиков (максимальный размер 128-1 байт!!!!!)
#define fHP_ON    0         // флаг Включения ТН (пишется внутрь счетчиков flags)

struct type_motoHour
{
  byte magic;       // признак данных, должно быть  0xaa  пока не используется!!!!!!!!!!
  uint8_t  flags;   // флаги
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

// Рабочие флаги ТН
#define fHP_SunActive 		0				// Солнечный контур активен

//  Работа с отдельными флагами type_optionHP
#define fAddHeat            0               // флаг Использование дополнительного тена при нагреве
#define fBeep               1               // флаг Использование звука
#define fNextion            2               // флаг Использование nextion дисплея
#define fSD_card            3               // флаг записи статистики на карту памяти
#define fSaveON             4               // флаг записи в EEPROM включения ТН
#define fTypeRHEAT          5               // флаг как используется дополнительный ТЭН для нагрева 0-резерв 1-бивалент
#define fSDMLogErrors  		6               // флаг писать в лог нерегулярные ошибки счетчика SDM
#define f1Wire2TSngl		7				// На 2-ой шине 1-Wire(DS2482) только один датчик
#define f1Wire3TSngl		8				// На 3-ей шине 1-Wire(DS2482) только один датчик
#define f1Wire4TSngl		9				// На 4-ей шине 1-Wire(DS2482) только один датчик
#define fSunRegenerateGeo	10				// Использовать солнечный коллектор для регенерации геоконтура в простое
#define fNextionOnWhileWork	11				// Включать дисплей, когда ТН работает
#define fWebStoreOnSPIFlash 12				// флаг, что веб морда лежит на SPI Flash, иначе на SD карте
 
// Структура для хранения опций теплового насоса.
struct type_optionHP
{
 uint8_t ver;             			   // номер версии для сохранения
 int8_t numProf;                       //  Текущий номер профиля 0 стоит по дефолту
 uint16_t flags;                       //  Флаги опций до 16 флагов
 uint8_t nStart;                       //  Число попыток пуска компрессора
 uint8_t sleep;                        //  Время засыпания дисплея минуты
 uint8_t dim;                          //  Яркость дисплея %
 uint16_t tChart;                      //  период сбора статистики в секундах!!
 int16_t tempRHEAT;                    //  Значение температуры для управления дополнительным ТЭН для нагрева СО
 uint16_t pausePump;                   //  Время паузы  насоса при выключенном компрессоре СЕКУНДЫ
 uint16_t workPump;                    //  Время работы  насоса при выключенном компрессоре СЕКУНДЫ
 // Временные задержки
 uint16_t delayOnPump;   		       // Задержка включения компрессора после включения насосов (сек).
 uint16_t delayOffPump;                // Задержка выключения насосов после выключения компрессора (сек).
 uint16_t delayStartRes;               // Задержка включения ТН после внезапного сброса контроллера (сек.)
 uint16_t delayRepeadStart;            // Задержка перед повторным включениме ТН при ошибке (попытки пуска) секунды
 uint16_t delayDefrostOn;              // ДЛЯ ВОЗДУШНОГО ТН Задержка после срабатывания датчика перед включением разморозки (секунды)
 uint16_t delayDefrostOff;             // ДЛЯ ВОЗДУШНОГО ТН Задержка перед выключением разморозки (секунды)
 uint16_t delayTRV;                    // Задержка между переключением 4-х ходового клапана и включением компрессора, для выравнивания давлений (сек). Если включены эти опции (переключение тепло-холод)
 uint16_t delayBoilerSW;               // Пауза (сек) после переключение ГВС - выравниваем температуру в контуре отопления/ГВС что бы сразу защиты не сработали
 uint16_t delayBoilerOff;              // Время (сек) на сколько блокируются защиты при переходе с ГВС на отопление и охлаждение слишком горяче после ГВС
 int16_t  SunRegGeoTemp;			   // Температура начала регенерации геоконтура с помощью СК, в сотых градуса
};// __attribute__((packed));


//  Работа с отдельными флагами type_DateTimeHP
#define fUpdateNTP     0                // флаг Обновление часов по NTP при старте
#define fUpdateI2C     1                // флаг Обновление часов раз в час с I2C  часами
#define fUpdateByHTTP  2                // флаг Обновление по HTTP - спец страница: define HTTP_TIME_REQUEST
// Структура для хранения настроек времени, для удобного сохранения.
struct type_DateTimeHP
{
    uint8_t flags;                        //  Флаги опций до 8 флагов
    int8_t timeZone;                      //  Часовой пояс
    char serverNTP[NTP_SERVER_LEN+1];     //  Адрес NTP сервера
    uint32_t saveTime;                    //  дата и время сохранения настроек в eeprom
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
    // Информационные функции определяющие состояние ТН
     __attribute__((always_inline)) inline MODE_HP get_modWork()     {return Status.modWork;}  // (переменная) Получить что делает сейчас ТН [0-Пауза 1-Включить отопление 2-Включить охлаждение 3-Включить бойлер 4-Продолжаем греть отопление 5-Продолжаем охлаждение 6-Продолжаем греть бойлер]
     __attribute__((always_inline)) inline TYPE_STATE_HP get_State() {return Status.State;}    // (переменная) Получить состяние теплового насоса [1 Стартует 2 Останавливается  3 Работает 4 Ожидание ТН (расписание - пустое место) 5 Ошибка ТН 6 - Эта ошибка возникать не должна!]
     __attribute__((always_inline)) inline int8_t get_ret()          {return Status.ret;}      // (переменная) Точка выхода из алгоритма регулирования (причина (условие) нахождения в текущем положении modWork)
    __attribute__((always_inline)) inline  MODE_HP get_modeHouse()   {return Prof.SaveON.mode;}// (настройка) Получить режим работы ДОМА (охлаждение/отопление/выключено) ЭТО НАСТРОЙКА через веб морду!
    boolean IsWorkingNow() { return !(get_State() == pOFF_HP && PauseStart == 0); }				// Включен ТН или нет
    boolean CheckAvailableWork();							// проверить есть ли работа для ТН
  
    void vUpdate();                                          // Итерация по управлению всем ТН - старт-стоп
    void calculatePower();                                   // Вычисление мощностей контуров и КОП
    void eraseError();                                       // стереть последнюю ошибку

    __attribute__((always_inline)) inline int8_t get_errcode(){return error;} // Получить код последней ошибки
    char    *get_lastErr(){return note_error;} // Получить описание последней ошибки, которая вызвала останов ТН, при удачном запуске обнуляется
    void     scan_OneWire(char *result_str); // Сканирование шины OneWire на предмет датчиков
    TEST_MODE get_testMode(){return testMode;} // Получить текущий режим работы
    void     set_testMode(TEST_MODE t);    // Установить значение текущий режим работы
    boolean  get_onBoiler(){return onBoiler;} // Получить состояние трехходового точнее если true то идет нагрев бойлера
    boolean  get_fSD() {return fSD;}     // Получить флаг наличия РАБОТАЮЩЕЙ СД карты
    void     set_fSD(boolean f) {fSD=f;}    // Установить флаг наличия РАБОТАЮЩЕЙ СД карты
    boolean  get_fSPIFlash() {return fSPIFlash;}     // Получить флаг наличия РАБОТАЮЩЕГО флеш диска
    void     set_fSPIFlash(boolean f) {fSPIFlash=f;}    // Установить флаг наличия РАБОТАЮЩЕГО флеш диска
    TYPE_SOURSE_WEB get_SourceWeb()                    // Получить источник загрузки веб морды
              { if (get_WebStoreOnSPIFlash()){if(get_fSPIFlash()) return pFLASH_WEB; else {if (get_fSD()) return pSD_WEB; else return pMIN_WEB;}}
                 else {if (get_fSD()) return pSD_WEB; else return pMIN_WEB;} return pERR_WEB;}
      
     uint32_t get_errorReadDS18B20();    // Получить число ошибок чтения датчиков темпеартуры

    void     sendCommand(TYPE_COMMAND c);   // Послать команду на управление ТН
    __attribute__((always_inline)) inline TYPE_COMMAND isCommand()  {return command;}  // Получить текущую команду выполняемую ТН
    int8_t   runCommand();               // Выполнить команду по управлению ТН
    char *get_command_name(TYPE_COMMAND c) { return (char*)hp_commands_names[c < pEND14 ? c : pEND14]; }
    boolean is_next_command_stop() { return next_command == pSTOP || next_command == pREPEAT; }
    uint8_t is_pause();					// Возвращает 1, если ТН в паузе
	inline boolean is_compressor_on() { return dRelay[RCOMP].get_Relay() || dFC.isfOnOff(); }    // Проверка работает ли компрессор



    // Строковые функции
    char *StateToStr();                 // Получить состояние ТН в виде строки
    char *StateToStrEN();               // Получить состояние ТН в виде английской строки
    char *TestToStr();                  // Получить режим тестирования
    int8_t save_DumpJournal(boolean f); // Записать состояние теплового насоса в журнал
    
    int32_t save(void); 		        // Записать настройки в eeprom i2c на входе адрес с какого, на выходе код ошибки (меньше нуля) или количество записанных  байт
    int32_t load(uint8_t *buffer, uint8_t from_RAM); // Считать настройки из i2c или RAM, на выходе код ошибки (меньше нуля)
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
   Scheduler Schdlr;					// Расписание

   #ifdef MQTT
      clientMQTT clMQTT;                // MQTT клиент
   #endif
  // Сетевые настройки
    boolean set_network(char *var, char *c);        // Установить параметр из строки
    char*   get_network(char *var,char *ret);       // Получить параметр из строки
  //  inline uint16_t get_sizePacket() {return Network.sizePacket;} // Получить размер пакета при передаче
    inline uint16_t get_sizePacket() {return 2048;} // Получить размер пакета при передаче
    
    uint8_t set_hashUser();                               // расчитать хеш для пользователя возвращает длину хеша
    uint8_t set_hashAdmin();                              // расчитать хеш для администратора возвращает длину хеша
    
   // Дата время
    boolean set_datetime(char *var, char *c);              //  Установить параметр дата и время из строки
    char*   get_datetime(char *var,char *ret);             //  Получить параметр дата и время из строки
    IPAddress get_ip() { return Network.ip;}               //  Получить ip адрес
    IPAddress get_sdns() { return Network.sdns;}           //  Получить sdns адрес
    IPAddress get_subnet() { return Network.subnet;}       //  Получить subnet адрес
    IPAddress get_gateway() { return Network.gateway;}     //  Получить gateway адрес
    void set_ip(IPAddress ip) {Network.ip=ip;}             //  Установить ip адрес
    void set_sdns(IPAddress sdns) {Network.sdns=sdns;}     //  Установит sdns адрес
    void set_subnet(IPAddress subnet) {Network.subnet=subnet;}  //  Установит subnet адрес
    void set_gateway(IPAddress gateway) {Network.gateway=gateway;}//  Установит gateway адрес
    uint16_t get_port() {return Network.port;}             //  получить порт вебсервера
    boolean get_NoAck() { return GETBIT(Network.flags,fNoAck);}  //  Получить флаг Не ожидать ответа ACK
    uint8_t get_delayAck() {return Network.delayAck;}      //  получить задержку перед отсылкой следующего пакета
    uint16_t get_pingTime() {return Network.pingTime;}     //  получить вермя пингования сервера, 0 если не надо
    char *  get_pingAdr() {return Network.pingAdr;}         //  получить адрес сервера для пингования
    boolean get_NoPing() { return GETBIT(Network.flags,fNoPing);} //  Получить флаг блокировки пинга
    char *  get_netMAC() {return MAC2String(Network.mac);}  //  получить мас адрес контроллера
        
    boolean get_DHCP() { return GETBIT(Network.flags,fDHCP);}    //  Получить использование DHCP
    byte *get_mac() { return Network.mac;}                 //  Получить mac адрес
    uint32_t socketRes() {return countResSocket;}          //  Получить число сбросов сокетов
    void add_socketRes() {countResSocket++;}               //  Добавить 1 к счетчику число сбросов сокетов
    uint32_t time_socketRes() {return Network.resSocket;}  //  Получить период сбросов сокетов
    uint32_t time_resW5200() {return Network.resW5200;}    //  Получить период сбросов W5200
    boolean get_fPass() { return GETBIT(Network.flags,fPass);}   //  Получить флаг необходимости идентификации
    boolean get_fInitW5200() { return GETBIT(Network.flags,fInitW5200);}  //  Получить флаг Контроля w5200

  // Параметры ТН
   boolean set_optionHP(char *var, float x);                // Установить опции ТН из числа (float)
   char*   get_optionHP(char *var, char *ret);              // Получить опции ТН
   uint16_t get_delayRepeadStart(){return Option.delayRepeadStart;} // Получить время между повторными попытками старта
   void set_mode(MODE_HP b) {Prof.SaveON.mode=b;}           // Установить режим работы отопления
   void set_nextMode();                                     // Переключение на следующий режим работы отопления (последовательный перебор режимов)
   void set_profile();										// Установить рабочий профиль по текущему Prof

   RULE_HP get_ruleCool(){return Prof.Cool.Rule;}           // Получить алгоритм охлаждения
   RULE_HP get_ruleHeat(){return Prof.Heat.Rule;}           // Получить алгоритм отопления
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
   uint16_t get_pausePump() {return Option.pausePump;};                // Время паузы  насоса при выключенном компрессоре, секунды
   uint16_t get_workPump() {return Option.workPump;};                  // Время работы  насоса при выключенном компрессоре, секунды
   uint8_t  get_Beep() {return GETBIT(Option.flags,fBeep);};           // подача звуковых сигналов
   uint8_t  get_SaveON() {return GETBIT(Option.flags,fSaveON);}        // получить флаг записи состояния
   uint8_t  get_WebStoreOnSPIFlash() {return GETBIT(Option.flags,fWebStoreOnSPIFlash);}// получить флаг хранения веб морды на флеш диске
   
   uint8_t  get_nStart() {return Option.nStart;};                      // получить максимальное число попыток пуска ТН
   uint8_t  get_sleep() {return Option.sleep;}                         //
   uint16_t get_flags() { return Option.flags; }					  // Все флаги
   void     updateNextion();                                          // Обновить настройки дисплея
  
   void set_HP_error_state() { Status.State = pERROR_HP; }
   inline void  set_HP_OFF(){SETBIT0(motoHour.flags,fHP_ON);Status.State=pOFF_HP;}// Сброс флага включения ТН
   inline void  set_HP_ON(){SETBIT1(motoHour.flags,fHP_ON);Status.State=pWORK_HP;}// Установка флага включения ТН
   inline boolean  get_HP_ON() {return GETBIT(motoHour.flags,fHP_ON);}           // Получить значение флага включения ТН

   void     set_BoilerOFF(){SETBIT0(Prof.SaveON.flags,fBoilerON);}          // Выключить бойлер
   void     set_BoilerON() {SETBIT1(Prof.SaveON.flags,fBoilerON);}          // Включить бойлер
   boolean  get_BoilerON() {return GETBIT(Prof.SaveON.flags,fBoilerON);}    // Получить флаг включения бойлера
      
   void     set_startTime(uint32_t t){Prof.SaveON.startTime = t;}           // Запомить время включения ТН
   
   // Бойлер ТН
    int16_t get_boilerTempTarget();					          // Получить целевую температуру бойлера с учетом корректировки
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
   
   __attribute__((always_inline)) inline uint32_t get_uptime() {return rtcSAM3X8.unixtime()-timeON;} // Получить время с последенй перезагрузки в секундах
   uint32_t get_startDT(){return timeON;}                  // Получить дату и время последеней перезагрузки
   inline uint32_t get_startCompressor(){return startCompressor;} // Получить дату и время пуска компрессора! нужно для старта ЭРВ
   inline uint32_t get_stopCompressor(){return stopCompressor;} // Получить дату и время останова компрессора!
   uint32_t get_startTime(){return Prof.SaveON.startTime;} // Получить дату и время пуска ТН (не компрессора!)
   inline uint32_t get_command_completed(){ return command_completed; } // Время выполнения команды
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
   
   void set_uptime(unsigned long ttime){timeON=ttime;}     // Установить текущее время как начало старта контроллера
  
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
    
    uint8_t PauseStart;                                    // 1 - ТН в отложенном запуске, 0 - нет, начать отсчет времени с начала при отложенном старте
       
    boolean startPump;                                     // Признак запуска задачи насос false - останов задачи true запуск
    type_SecurityHP Security;                              // хеш паролей
    boolean safeNetwork;                                   // Режим работы safeNetwork (сеть по умолчанию, паролей нет)
      
   
    // функции для работой с графикками
    uint16_t get_tChart(){return Option.tChart;}           // Получить время накопления ститистики в секундах
    void updateChart();                                     // обновить статистику
    void startChart();                                      // Запуститьь статистику
    char * get_listChart(char* str);				          // получить список доступных графиков
    char * get_Chart(char *var,char* str);   				 // получить данные графика
  
    // графики не по датчикам (по датчикам она хранится внутри датчика)
    statChart ChartRCOMP;                                   // Статистика по включению компрессора
    
    #ifdef EEV_DEF
    statChart ChartOVERHEAT;                                // перегрев
    statChart ChartTPEVA;                                   // температура расчитанная из давления испариения
    statChart ChartTPCON;                                   // температура расчитанная из давления Конденсации
    #endif
    statChart ChartCOP;                                     // Коэффициент преобразования
    statChart ChartFullCOP;                                 // ПОЛНЫЙ Коэффициент преобразования
    
    float powerCO;                                          // Мощность системы отопления
    float powerGEO;                                         // Мощность системы GEO
    float power220;                                         // Мощность системы 220
    int16_t fullCOP;                                        // Полный СОР сотые
    int16_t COP;                                            // Чистый COP сотые
    
    // Удаленные датчики
    #ifdef SENSOR_IP
    boolean updateLinkIP();                    // Обновить ВСЕ привязки удаленных датчиков
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
 
    void Pumps(boolean b, uint16_t d);    // Включение/выключение насосов, задержка после включения msec
    void Pump_HeatFloor(boolean On);	  // Включить/выключить насос ТП
    void Sun_OFF(void);					  // Выключить СК
    uint8_t flags;						  // Рабочие флаги ТН

    int16_t get_temp_condensing(void);	    // Расчитать температуру конденсации
    int16_t get_temp_evaporating(void);		// Получить температуру кипения
    int16_t get_overcool(void);			    // Расчитать переохлаждение
    int8_t	Prepare_Temp(uint8_t bus);		// Запуск преобразования температуры
    // Настройки опций
    type_optionHP Option;                  // Опции теплового насоса

    uint32_t time_Sun_ON;                 // тики включения солнечного коллектора
    uint32_t time_Sun_OFF;                // тики выключения солнечного коллектора
    uint8_t  NO_Power;					  // Нет питания основных узлов
    uint8_t  NO_Power_delay;

  private:
    int8_t StartResume(boolean start);    // Функция Запуска/Продолжения работы ТН - возвращает ок или код ошибки
    int8_t StopWait(boolean stop);        // Функция Останова/Ожидания ТН  - возвращает код ошибки
    int8_t ResetFC();                     // Сброс инвертора если он стоит в ошибке, провиряется наличие инвертора и прорверка ошибки

    MODE_HP get_Work();                   // Что надо делать
    void configHP(MODE_HP conf);          // Концигурация 4-х, 3-х ходового крана и включение насосов
    void ChangesPauseTRV();               // "Интелектуальная пауза" для перекидывания на "ходу" 4-х ходового
    void defrost();                       // Все что касается разморозки воздушника

    void resetSettingHP();                // Функция сброса настроек охлаждения и отопления
    void relayAllOFF();                   // Все реле выключить
    #ifdef  RTRV
    void set_RTRV(uint16_t d);            // Поставить 4х ходовой в нужное положение для работы в заваисимости от Prof.SaveON.mode параметр задержка после включения mсек.
    #endif
    boolean boilerAddHeat();              // Проверка на необходимость греть бойлер дополнительным теном (true - надо греть)
    boolean switchBoiler(boolean b);      // Переключение на нагрев бойлера ТН true-бойлер false-отопление/охлаждение
    boolean checkEVI();                   // Проверка и если надо включение EVI если надо то выключение возвращает состояние реле
    MODE_COMP UpdateHeat();               // Итерация нагрев  вход true - делаем, false - ТОЛЬКО проверяем выход что сделано или надо сделать
    MODE_COMP UpdateCool();               // Итерация охлаждение вход true - делаем, false - ТОЛЬКО проверяем выход что сделано или надо сделать
    MODE_COMP UpdateBoiler();             // Итерация управления бойлером возвращает что делать компрессору
    void compressorON(MODE_HP mod);       // попытка включить компрессор  с учетом всех защит
    void compressorOFF();                 // попытка выключить компрессор  с учетом всех защит
    boolean check_compressor_pause(MODE_HP mod); // проверка на паузу между включениями
    int8_t check_crc16_eeprom(int32_t addr, uint16_t size);// Проверить контрольную сумму в EEPROM для данных на выходе ошибка, длина определяется из заголовка
    boolean setState(TYPE_STATE_HP st);   // установить состояние теплового насоса
          
    type_motoHour motoHour;               // Структура для хранения счетчиков запись каждый час
    TEST_MODE testMode;                   // Значение режима тестирования
    TYPE_COMMAND command;                 // Текущая команда управления ТН
    TYPE_COMMAND next_command;            // Следующая команда управления ТН
    type_status Status;                   // Описание состояния ТН

    // Ошибки и описания
    int8_t error;                         // Код ошибки
    char   source_error[16];              // источник ошибки
    char   note_error[160+1];             // Строка c описанием ошибки формат "время источник:описание"
    boolean fSD;                          // Признак наличия РАБОТАЮЩЕЙ SD карты
    boolean fSPIFlash;                    // Признак наличия (физического) spi диска
    boolean startWait;                    // Начало работы с ожидания
     
    // Различные времена
    type_DateTimeHP DateTime;             // структура где хранится все что касается времени и даты
    uint32_t timeON;                      // время включения контроллера для вычисления UPTIME
    uint32_t countNTP;                    // число секунд с последнего обновления по NTP
    uint32_t startCompressor;             // время пуска компрессора (для обеспечения минимального времени работы)
    uint32_t stopCompressor;              // время останова компрессора (для опеспечения паузы)
    uint32_t offBoiler;                   // время выключения нагрева ГВС ТН (необходимо для переключения на другие режимы на ходу)
    uint32_t startDefrost;                // время срабатывания датчика разморозки
    uint32_t timeBoilerOff;               // Время переключения (находу) с ГВС на отопление или охлаждения (нужно для временной блокировки защит) если 0 то переключения не было
    uint32_t startSallmonela;             // время начала обеззараживания
    uint32_t command_completed;			  // Время отработки команды
       
    // Сетевые настройки
    type_NetworkHP Network;                 // Структура для хранения сетевых настроек
    uint32_t countResSocket;                // Число сбросов сокетов

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
    boolean flagRBOILER;                  // true - идет цикл догрева бойлера
    boolean onBoiler;                     // Если true то идет нагрев бойлера ТН (не ТЭНом)
    boolean onSallmonela;                 // Если true то идет Обеззараживание
    
    SdFile  statFile;                       // файл для записи статистики

    friend int8_t set_Error(int8_t err, char *nam );// Установка критической ошибки для класса ТН
  };

#endif
