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
//  Описание вспомогательных лассов данных, предназначенных для получения информации о ТН
#ifndef Information_h
#define Information_h
#include "Constant.h"                       // Вся конфигурация и константы проекта Должен быть первым !!!!
#include "utility/w5100.h"
#include "utility/socket.h"
// --------------------------------------------------------------------------------------------------------------- 
//  Класс системный журнал пишет в консоль и в память ------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------- 
//  Необходим для получения лога контроллера
//  http://arduino.ru/forum/programmirovanie/etyudy-dlya-nachinayushchikh-interfeis-printable
//  Для записи ТОЛЬКО в консоль использовать функции printf
//  Для записи в консоль И в память (журнал) использовать jprintf
//  По умолчанию журнал пишется в RAM размер JOURNAL_LEN
//  Если включена опция I2C_EEPROM_64KB то журнал пишется в I2C память. Должен быть устанвлен чип размером более 4кБ, адрес начала записи 0x0fff до I2C_STAT_EEPROM
//  Размер журнала I2C_STAT_EEPROM-I2C_JOURNAL_START

enum type_promt //  Перечисляемый тип - что идет в начале строки при выводе в журнал
{
    pP_NONE,        // ничего
    pP_TIME,        // время
    pP_DATE,        // дата и время
    pP_USER         // константа определяемая пользователем
};

#define PRINTF_BUF 256                           // размер буфера для одной строки - большаяя длина нужна при отправке уведомлений, там длинные строки (видел 178)

extern uint16_t sendPacketRTOS(uint8_t thread, const uint8_t * buf, uint16_t len,uint16_t pause);
const char *MessageLongString = { "Jornal: Input string too long, skip string!"};  
const char *errorReadI2C =    {"$ERROR - read I2C memory\n"};
const char *errorWriteI2C =   {"$ERROR - write I2C memory\n"};
const char *promtUser={"> "};   

class Journal :public Print
{
public:
 // SemaphoreHandle_t xJournalSemaphore;                    // Семафор Journal
  void Init();                                            // Инициализация
  void printf(const char *format, ...) ;                  // Печать только в консоль
  void jprintf(const char *format, ...);                  // Печать в консоль и журнал возвращает число записанных байт
  void jprintf(type_promt pr,const char *format, ...);    // Печать в консоль и журнал возвращает число записанных байт с типом промта
  void jprintf_only(const char *format, ...);             // Печать ТОЛЬКО в журнал возвращает число записанных байт для использования в критических секциях кода
  int32_t send_Data(uint8_t thread);                     // отдать журнал в сеть клиенту  Возвращает число записанных байт
  int32_t available(void);                               // Возвращает размер журнала
  int8_t   get_err(void) { return err; };
  virtual size_t write (uint8_t c);                       // чтобы print работал для это класса
  #ifdef I2C_EEPROM_64KB                                  // Если журнал находится в i2c
  void Format(char * buf);                               // форматирование журнала в еепром
  #else
  void Clear(){bufferTail=0;bufferHead=0;full=false;err=OK;} // очистка журнала в памяти
  #endif
private:
  int8_t err;                                             // ошибка журнала
  int32_t bufferHead, bufferTail;                        // Начало и конец
  uint8_t full;                                           // признак полного буфера
  void _write(char *dataPtr);                            // Записать строку в журнал
   // Переменные
  char pbuf[PRINTF_BUF+2];                                // Буфер для одной строки + маркеры
  #ifndef I2C_EEPROM_64KB                                 // Если журнал находится в памяти
    char _data[JOURNAL_LEN+1];                            // Буфер журнала
  #else
    void writeTAIL();                                     // Записать символ "конец" значение bufferTail должно быть установлено
    void writeHEAD();                                     // Записать символ "начало" значение bufferHead должно быть установлено
    void writeREADY();                                    // Записать признак "форматирования" журнала - журналом можно пользоваться
    boolean checkREADY();                                 // Проверить наличие журнала
  #endif
 };

// ---------------------------------------------------------------------------------
//  Класс Графики работы   ------------------------------------------------------------
// ---------------------------------------------------------------------------------
// Бинарные данные могут хранится в упакованном виде до 16 значений в одной точке.
// запихиваем уже упакованное значение
// читаем get_boolPointsStr
class statChart                                         // организован кольцевой буфер
{
 public: 
  void init(boolean pres);                              // инициализация класса
  void clear();                                         // очистить статистику
  void addPoint(int16_t y);                             // добавить точку в массиве (для бинарных данных добавляем все точки сразу)
  inline int16_t get_Point(uint16_t x);                 // получить точку нумерация 0-самая старая CHART_POINT - самая новая, (работает кольцевой буфер)
  boolean get_boolPoint(uint16_t x,uint16_t mask);      // БИНАРНЫЕ данные: получить точку нумерация 0-самая старая CHART_POINT - самая новая, (работает кольцевой буфер)
  
  void get_PointsStr(uint16_t m, char *b);             // получить строку в которой перечислены все точки в строковом виде через ";" при этом значения делятся на m
  void get_PointsStrSub(uint16_t m, char *b, statChart *sChart); // получить строку, вычесть точки sChart
  void get_PointsStrPower(uint16_t m, char *b, statChart *inChart,statChart *outChart, float kfCapacity); // Расчитать мощность на лету используется для графика потока, передаются указатели на температуры
  
  inline boolean get_present() {return present;}        // Наличие датчика в текущей конфигурации
  inline uint16_t get_num()  {return num;}              // Получить число накопленных точек
 private:
  int8_t err;                                            // Ошибка
  int16_t *data;                                        // указатель на массив для накопления данных
  int16_t pos;                                          // текущая позиция для записи
  int16_t num;                                          // число накопленных точек
  boolean present;                                      // наличие статистики
  boolean flagFULL;                                     // false в буфере менее CHART_POINT точек
};

// ---------------------------------------------------------------------------------
//  Класс Профиль ТН    ------------------------------------------------------------
// ---------------------------------------------------------------------------------
// Класс предназначен для работы с настройками ТН. в пямяти хранится текущий профиль, ав еепром можно хранить до 10 профилей, и загружать их по одному в пямять
// также есть функции по сохраннию и удалению из еепром
// номер текущего профиля хранится в структуре type_SaveON
#define PROFILE_EMPTY 0x55     // Признак пустого профиля

// --------- внутренние структуры --------------
// Структура для хранения статуса работы ТН EEPROM  - часть профиля!!!
//  Определение флагов в  type_SaveON
#define fBoilerON 1         // флаг Включения бойлера  true - включено
struct type_SaveON
{
  byte magic;               // признак данных, должно быть  0x55
  uint8_t  flags;           // флаги состояния ТН
  MODE_HP mode;             // режим работы отопление|охлаждение
  uint32_t startTime;       // дата и время включения ТН
  uint32_t P1;              // Резервный параметр 1
};

// Структуры для хранения настроек бойлера
//  Определение флагов в type_boilerHP
#define fScheduleAddHeat 0                // флаг Расписание только для ТЭНа
#define fSchedule        1                // флаг Использование расписания
#define fSalmonella      2                // флаг Сальмонела раз внеделю греть бойлер
#define fCirculation     3                // флаг Управления циркуляционным насосом ГВС
#define fResetHeat       4                // флаг Сброса лишнего тепла в СО
#define fTurboBoiler     5                // флаг ТУРБО ГВС нагрев (нагрев=ТН+ТЭН)
#define fAddHeating      6                // флаг ДОГРЕВА ГВС ТЭНом

struct type_boilerHP
{
 uint8_t flags;                        // Флаги
 int16_t TempTarget;                   // Целевая температура бойлера
 int16_t dTemp;                        // гистерезис целевой температуры ночью
 int16_t tempIn;                       // Tемпература подачи максимальная/минимальная если охлаждение
 uint16_t pause;                       // Минимальное время простоя компрессора в секундах
 uint32_t Schedule[7];                 // Расписание бойлера
 uint16_t Circul_Work;                 // Время  работы насоса ГВС секунды (fCirculation)
 uint16_t Circul_Pause;                // Пауза в работе насоса ГВС  секунды (fCirculation)
 uint16_t Reset_Time;                  // время сброса излишков тепла в секундах (fResetHeat)
 uint16_t time;                        // Постоянная интегрирования времени в секундах ПИД ТН
 int16_t Kp;                           // Пропорциональная составляющая ПИД ТН
 int16_t Ki;                           // Интегральная составляющая ПИД ТН
 int16_t Kd;                           // Дифференциальная составляющая ПИД ТН
 int16_t tempPID;                      // Целевая темпеартура ПИД
 int16_t tempRBOILER;                  // Температура догрева бойлера
 int16_t P1;                           // Резервный параметр 1
 int16_t P2;                           // Резервный параметр 2
};

// Структуры для хранения настроек Отопления и Охлаждения (одинаковые)
// Работа с отдельными флагами type_settingHP
#define fTarget      0                // Что является целью  - 0 (температура в доме), 1 (температура обратки).
#define fWeather     1                // флаг Погодозависмости
#define fHeatFloor   2				  // флаг использования теплого пола
struct type_settingHP
{
 uint8_t flags;                       // Флаги опций до 8 флагов
 RULE_HP Rule;                        // алгоритм работы отопления.
 int16_t Temp1;                       // Целевая температура дома
 int16_t Temp2;                       // Целевая температура обратки
 int16_t dTemp;                       // гистерезис целевой температуры ночью
 uint16_t time;                       // Постоянная интегрирования времени в секундах ПИД ТН
 int16_t Kp;                          // Пропорциональная составляющая ПИД ТН
 int16_t Ki;                          // Интегральная составляющая ПИД ТН
 int16_t Kd;                          // Дифференциальная составляющая ПИД ТН
 // Защиты
 int16_t tempIn;                      // Tемпература подачи (минимальная для охлажления или максимальная для нагрева)
 int16_t tempOut;                     // Tемпература обратки (максимальная для охлажления или минимальная для нагрева)
 uint16_t pause;                      // Минимальное время простоя компрессора в секундах
 int16_t dt;                          // Максимальная разность температур конденсатора.
 int16_t tempPID;                     // Целевая темпеартура ПИД
 int16_t kWeather;                    // Коэффициент погодозависимости в ТЫСЯЧНЫХ градуса на градус (проблемы с преобраованием - округление)
 int16_t dTempDay;                    // гистерезис целевой температуры днем
 int16_t P1;                          // Резервный параметр 1
 int16_t P2;                          // Резервный параметр 2
};

#define LEN_PROFILE_NAME       25     // Длина имени профиля
#define fEnabled               0      // Разрешение данного профайла использоваться в коротком списке
struct type_dataProfile               // Хранение общих данных
{
  int8_t id;                          // Номер профайла
  uint8_t flags;                      // Флаги профайла
  uint16_t len;                       // Длинна данных
  uint32_t saveTime;                  // Время сохранения профиля
  char name[LEN_PROFILE_NAME];        // Имя профайла
  char note[85];                      // Описание профайла кодирование описания профиля 40 русских букв (одна буква 6 байт (двойное кодирование) это входная строка)  описание переводится и хранится в utf8 по 2 байта символ
};


class Profile                         // Класс профиль
{
  public: 
    type_SaveON SaveON;                                     // Структура для хранения состяния ТН в ЕЕПРОМ
   // Настройки для отопления и охлаждения бойлера
    type_settingHP Cool;                                    // Настройки для режима охлаждение
    type_settingHP Heat;                                    // Настройки для режима отопления
    type_boilerHP Boiler;                                   // Настройка бойлера
    
     // Функции работы с профилем
    void initProfile();                                     // инициализация профиля
    char *get_list(char *c /*,int8_t num*/);                // ДОБАВЛЯЕТ к строке с - список возможных конфигураций num - текущий профиль
    int8_t set_list(int8_t num);                            // Устанавливает текущий профиль из номера списка, ДОБАВЛЯЕТ к строке с
    int8_t update_list(int8_t num);                         // обновить список имен профилей

    char *get_info(char *c,int8_t num);                     // ДОБАВЛЯЕТ к строке с - описание профиля num
    int16_t save(int8_t num);                               // Записать профайл в еепром под номерм num
    int32_t load(int8_t num);                               // загрузить профайл num из еепром память
    int8_t  loadFromBuf(int32_t adr,byte *buf);             // Считать настройки из буфера на входе адрес с какого, на выходе код ошибки (меньше нуля)
    int32_t erase(int8_t num);                              // стереть профайл num из еепром  (ставится признак пусто)
    boolean set_paramProfile(char *var,char *c);            // Профиль Установить параметры ТН из строки
    char*   get_paramProfile(char *var,char *ver);          // профиль Получить параметр второй параметр - наличие частотника
    int16_t get_lenProfile(){return dataProfile.len;}       // получить длину профиля при записи
    int8_t  get_idProfile(){return dataProfile.id;}         // получить номер текущего профиля
      
    // Установка параметров
    boolean set_paramCoolHP(char *var, float x);           // Охлаждение Установить параметры ТН из числа (float)
    char*   get_paramCoolHP(char *var, char *ret, boolean fc);// Охлаждение Получить параметр второй параметр - наличие частотника
    boolean set_paramHeatHP(char *var, float x);           // Отопление  Установить параметры ТН из числа (float)
    char*   get_paramHeatHP(char *var, char *ret,boolean fc);// отопление  Получить параметр  второй параметр - наличие частотника
    boolean set_boiler(char *var, char *c);                 // Установить параметр из строки
    char*   get_boiler(char *var, char *ret);               // Получить параметр из строки по имени var, результат ДОБАВЛЯЕТСЯ в строку ret
    int8_t  load_from_EEPROM_SaveON(type_SaveON *_SaveOn);	// Прочитать из EEPROM структуру: режим работы ТН (SaveON)
 
 private:
  int8_t err;                                               // Ошибка
  char list[I2C_PROFIL_NUM*(LEN_PROFILE_NAME+2)+1];         // текущий список конфигураций, не забывем про :1 (список)
 
  // заголовок
  byte magic;                                               // признак данных, должно быть  0xaa
  uint16_t crc16;                                           // Контрольная сумма
  type_dataProfile dataProfile;                             // данные профиля
  inline int32_t get_sizeProfile()  // определить длину данных
   {
    return sizeof(magic) + \
           sizeof(crc16) + \
           // данные контрольная сумма считается с этого места
           sizeof(dataProfile) + \
           sizeof(SaveON) + \
           sizeof(Cool)  + \
           sizeof(Heat)  + \
           sizeof(Boiler);
   };
   uint16_t get_crc16_mem();                                 // Расчитать контрольную сумму
   int8_t   check_crc16_eeprom(int8_t num);   // Проверить контрольную сумму ПРОФИЛЯ в EEPROM для данных на выходе ошибка, длина определяется из заголовка
   int8_t   check_crc16_buf(int32_t adr, byte* buf);         // Проверить контрольную сумму в буфере для данных на выходе ошибка, длина определяется из заголовка
};
// ---------------------------------------------------------------------------------
//  Класс Статистика ТН ------------------------------------------------------------
//  ------------- только для 64 кбайтного чипа памяти ------------------------------
// ---------------------------------------------------------------------------------
#ifdef I2C_EEPROM_64KB  
//  #define I2C_STAT_START (I2C_STAT_EEPROM+6)                                                          // Адрес с которого начинается ДАННЫЕ статистики в памяти i2c ВНИМАНИЕ - 6 байт в начале лежит признак форматирования журнала, и адрес первого и последнего элемента
//  #define STAT_LEN (((int)(0xffff-I2C_STAT_EEPROM)/W5200_MAX_LEN)*W5200_MAX_LEN)                      // Размер статистки - округление на целое число страниц W5200_MAX_LEN в байтах
//  #define STAT_POINT  ((int)(STAT_LEN/sizeof(type_OneDay)-1))                                         // Максимальное число хранимых точек
//  #define I2C_STAT_FORMAT (0xff)                                                                      // Символ которым заполняется статистика при форматировании
//  #define I2C_STAT_READY  (0xaa55)                                                                    // Признак создания журнала - если его нет по адресу I2C_JOURNAL_START-2 то надо форматировать журнал (первичная инициализация)
// Работа с отдельными флагами type_OneDay
#define fStatFull         0          // флаг полного дня  все 24 часа

// Данные об эффективности работы ТН  по дням
struct type_OneDay                   // Хранение данных одной точки во флеше
{
uint8_t  flags;                      // 1 Флаги точки
uint8_t  Hour;                       // 1 Сколько часов в точке
uint32_t date;                       // 4 Дата точки (начала)
int16_t  tin;                        // 2 средняя температура в доме
int16_t  tout;                       // 2 средняя температура на улице
int16_t  tbol;                       // 2 средняя температура в бойлере
uint16_t eCO;                        // 2 Выработаная энергия
uint16_t eEn;                        // 2 Затраченная энергия 220
uint16_t moto;                       // 2 Моточасы компрессора
uint32_t P1;                         // 4 резерв 1
uint32_t P2;                         // 4 резерв 2
};  // итого 24 байт

class Statistics                      // Класс статистика
 {
 public:  
  void Init();                                            // Инициализация статистики
  uint16_t available();                                   // Возвращает размер статистики днях (записях)
  void Format();                                          // форматирование статистики
  uint32_t get_dateUpdate() {return dateUpdate;}          // получить время последнего обновления статистики
  uint32_t get_date() {return OneDay.date;}               // получить время начала накопления дня
  uint8_t get_Hour() {return Hour;}                       // Получить сколько часов в точке
  void  updateHour(uint8_t x){Hour=Hour+x;}               // добавить часов
  void  updateTin(uint16_t x){tin=tin+x;}                 // добавить температура в доме
  void  updateTout(uint16_t x){tout=tout+x;}              // добавить температура на улице
  void  updateTbol(uint16_t x){tbol=tbol+x;}              // добавить температура в бойлере
  void  updateCO(uint32_t x){eCO=x;}                      // добавить Выработаная энергия
  void  updateEO(uint32_t x){eEn=x;}                      // добавить Затраченная энергия 220
  void  updatemoto(uint32_t x){moto=x;}                   // добавить Моточасы компрессора
  uint8_t writeOneDay(uint32_t energyCO,uint32_t energy220,uint32_t motoH); // Записать один день в еепром
  void generate_TestData(uint16_t num);                   // Сгенерировать тестовые данные в памяти i2c на входе число точек
  char *get_listStat(char* str, boolean cat);             // получить список доступой статистики кладется в str
  char *get_Stat(char* var,char* str, boolean cat);       // получить данные статистики по одному типу данных в виде строки
  char *get_OneDay(char* str,uint16_t index,boolean cat); // получить данные статистики по одному дню в виде строки
  char *get_Info(char* str, boolean cat);                 // получить информацию о накопленной стаитистики в виде строки
  
private:
  int8_t error;                                           // ошибка
  uint16_t pos, num;                                      // позиция для записи и число точек
  char bufferI2C[256];                                    // буфер для работы с i2c памятью для ускорения работы
  boolean full;                                           // признак полного буфера
  uint32_t dateUpdate;                                    // Время поледнего обновления статистики
  uint8_t Hour=0;                                         // 1 Сколько часов в точке
  int32_t tin=0;                                          // 4 средняя температура в доме
  int32_t tout;                                           // 4 средняя температура на улице
  int32_t tbol=0;                                         // 4 средняя температура в бойлере
  uint32_t eCO=0;                                         // 4 Выработаная энергия
  uint32_t eEn=0;                                         // 4 Затраченная энергия 220
  uint32_t moto=0;                                        // 4 Моточасы компрессора
  type_OneDay  OneDay;                                    // буффер для записи
  type_OneDay  ReadDay;                                   // буффер для чтения
  inline int32_t get_sizeOneDay(){return sizeof(type_OneDay);}// определить длину одного дня структура type_OneDay
  void clear_OneDay();                                     // очистка данных текущего дня
  void writePOS();                                         // Позиция для записи
  void writeNUM();                                         // Число точек
  void writeREADY();                                       // Записать признак "форматирования" статистки - статистикой можно пользоваться
  boolean checkREADY();                                    // Проверить наличие статистики
  uint16_t readDBYTE(uint16_t adr);                        // прочитать uint16_t
 };
#endif

// ---------------------------------------------------------------------------------
//  Структура MQTT клиент ТН -------------------------------------------------------
// ---------------------------------------------------------------------------------
// Работа с отдельными флагами type_mqtt
#define fMqttUse         0                   // флаг использования MQTT
#define fMqttBig         1                   // флаг отправки ДОПОЛНИТЕЛЬНЫХ данных на MQTT
#define fMqttSDM120      2                   // флаг отправки данных электросчетчика на MQTT
#define fMqttFC          3                   // флаг отправки данных инвертора на MQTT
#define fMqttCOP         4                   // флаг отправки данных COP на MQTT
#define fNarodMonUse     5                   // флаг отправки данных на народный мониторинг
#define fNarodMonBig     6                   // флаг отправки данных на народный мониторингбольшая версия
#define fTSUse           7                   // флаг использования ThingSpeak
enum TYPE_STATE_MQTT
{
 pMQTT_OK,             // Последняя передача была удачна
 pMQTT_SOCK,           // Попытка реанимировать сокет
 pMQTT_DNS,            // Надо проверить имя
 pMQTT_RES             // надо сбросить чип
};

const char* BlockService={"Too many sending errors, sending %s is off . . .\n"};
//const char* MQTTnoconnect={"MQTT server: %s no connect, state: %s\n"};
//const char* RepeatConnect={"Repeat connect %s state: %s\n"};
//const char* DNSConnect={"Update IP address for %s \n"};
const char* ChangeIP={"\n Change IP address %s\n"};
const char* ResSock ={"\n$WARNING: Reset W5200_SOCK_SYS sock\n"};
const char* ResDHCP ={"\n$WARNING: Update server IP\n"};
const char* ResChip ={"\n$WARNING: Reset W5XXX chip\n"};

// Настройки относящиеся к MQTT
struct type_mqtt                             // настройки MQTT клиента
{
 uint16_t flags;                             // Бинарные флага настроек
 // Сервер MQTT
 uint16_t ttime;                             // период отправки на сервер в сек. 10...60000
 char mqtt_server[32];                       // Адрес сервера
 IPAddress mqtt_serverIP;                    // IP Адрес сервера
 uint16_t mqtt_port;                         // Адрес порта сервера
 char mqtt_login[20];                        // логин сервера
 char mqtt_password[20];                     // пароль сервера
 char mqtt_id[16];                           // Идентификатор клиента на MQTT сервере
 // Народный мониторинг
 char narodMon_server[32];                    // Адрес сервера народного мониторинга
 IPAddress narodMon_serverIP;                 // IP Адрес сервера народного мониторинга
 uint16_t narodMon_port;                      // Адрес порта сервера  народного мониторинга
 char narodMon_login[16];                     // логин сервера народного мониторинга
 char narodMon_password[16];                  // пароль сервера народного мониторинга
 char narodMon_id[16];                        // Идентификатор клиента на народного мониторинга
};

class clientMQTT                              // Класс клиент MQTT
 {
 public: 
    void initMQTT();                            // инициализация MQTT
    uint8_t *get_save_addr(void) { return (uint8_t *)&mqttSettintg; } // Адрес структуры сохранения
    uint16_t get_save_size(void) { return sizeof(mqttSettintg); } // Размер структуры сохранения
    int32_t save(int32_t adr);                  // Записать MQTT в еепром под номерм num
    int32_t load(int32_t adr);                  // загрузить MQTT num из еепром память
    int32_t loadFromBuf(int32_t adr,byte *buf); // Считать настройки из буфера на входе адрес с какого, на выходе код ошибки (меньше нуля)
    uint16_t get_crc16(uint16_t crc);           // Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
      
    boolean dnsUpdate();                        // Обновление IP адресов MQTT через dns
    boolean dnsUpdateStart();                   // Обновление IP адресов MQTT через dns при СТАРТЕ!!! семафоры не используются
    boolean sendTopic(char * t,char *p,boolean NM, boolean debug, boolean link_close);// Внутренная функция послать один топик
 
    uint16_t updateErrMQTT(boolean NM);         // Еще одна ошибка отправки на MQTT параметр тип сервера true - народный мониторинг false MQTT
    void resetErrMQTT(boolean NM){if(NM)numErrNARMON=0;else numErrMQTT=0;}  // Сброс числа ошибок отправки на MQTT параметр тип сервера true - народный мониторинг false MQTT
    void updateState(boolean NM,TYPE_STATE_MQTT state){if(NM) stateNARMON=state;else stateMQTT=state;}  // Установка состояния соединения MQTT
           
    // Установка параметров
    boolean set_paramMQTT(char *var, char *c);    // Установить параметр из строки
    char*   get_paramMQTT(char *var, char *ret);  // Получить параметр из строки по имени var, результат ДОБАВЛЯЕТСЯ в строку ret
     
    void set_mqtt_serverIP(IPAddress x){mqttSettintg.mqtt_serverIP=x; }           // Установить IP Адрес серверa MQTT
    void set_narodMon_serverIP(IPAddress x){mqttSettintg.narodMon_serverIP=x; }   // УстановитьIP Адрес сервера народного мониторинга
    // чтение настоек
    boolean get_MqttUse() {return GETBIT(mqttSettintg.flags,fMqttUse);}          // Получить флаг отправки на сервер MQTT
    boolean get_TSUse() {return GETBIT(mqttSettintg.flags,fTSUse);}              // Получить флаг отправки на  ThingSpeak
    boolean get_MqttBig() {return GETBIT(mqttSettintg.flags,fMqttBig);}          // Получить флаг отправки доп данных на сервер MQTT
    boolean get_MqttSDM120() {return GETBIT(mqttSettintg.flags,fMqttSDM120);}    // Получить флаг отправки данных электросчетчика на сервер MQTT
    boolean get_MqttFC() {return GETBIT(mqttSettintg.flags,fMqttFC);}            // Получить флаг отправки данных инвертора на MQTT
    boolean get_MqttCOP() {return GETBIT(mqttSettintg.flags,fMqttCOP);}          // Получить флаг отправки данных COP на MQTT
     
    IPAddress get_mqtt_serverIP(){return mqttSettintg.mqtt_serverIP; }           // IP Адрес серверa MQTT
    char*  get_mqtt_server(){return mqttSettintg.mqtt_server; }                  // Адрес серверa MQTT
    
    uint16_t get_mqtt_port(){return mqttSettintg.mqtt_port;}                     // Адрес порта серверa MQTT
    uint16_t get_ttime(){return mqttSettintg.ttime;}                             // Время отправки
    
    boolean get_NarodMonUse() {return GETBIT(mqttSettintg.flags,fNarodMonUse);}  // Получить флаг отправки на народный мониторинг
    boolean get_NarodMonBig() {return GETBIT(mqttSettintg.flags,fNarodMonBig);}  // Получить флаг отправки на народный мониторинг
    IPAddress get_narodMon_serverIP(){return mqttSettintg.narodMon_serverIP; }   // IP Адрес сервера народного мониторинга
    char*   get_narodMon_server(){return mqttSettintg.narodMon_server; }         // Адрес сервера народного мониторинга
    uint16_t get_narodMon_port(){return mqttSettintg.narodMon_port;}             // Адрес порта сервера  народного мониторинга
  
 private:
   boolean connect(boolean NM);                 // Попытаться соеденится с сервером МЮТЕКС уже захвачен!!
   boolean closeSockSys(boolean debug);         // Попытаться сбросить системный сокет МЮТЕКС уже захвачен!!
   boolean dnsUpadateMQTT;                      // Флаг необходимости обновления через dns IP адреса для MQTT
   boolean dnsUpadateNARMON;                    // Флаг необходимости обновления через dns IP адреса для NARMON
   uint16_t numErrNARMON;                       // число ошибок отправки на народный моиторинг
   uint16_t numErrMQTT;                         // число ошибок отправки на MQTT
   TYPE_STATE_MQTT stateNARMON;                 // Состояние передачи что надо делать
   TYPE_STATE_MQTT stateMQTT;                   // Состояние передачи что надо делать
   
   type_mqtt mqttSettintg;                      // настройки клиента
 } ;

#endif
