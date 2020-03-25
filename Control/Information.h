/*
 * Copyright (c) 2016-2020 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
 * &                       by Vadim Kulakov vad7@yahoo.com, vad711
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
  void jprintf_only(const char *format, ...);             // Печать ТОЛЬКО в журнал возвращает число записанных байт для использования в критических секциях кода
  void jprintf_time(const char *format, ...);			// +Time, далее печать в консоль и журнал
  void jprintf_date(const char *format, ...);			// +DateTime, далее печать в консоль и журнал
  int32_t send_Data(uint8_t thread);                     // отдать журнал в сеть клиенту  Возвращает число записанных байт
  int32_t available(void);                               // Возвращает размер журнала
  int8_t   get_err(void) { return err; };
  virtual size_t write (uint8_t c);                       // чтобы print работал для это класса
  #ifdef I2C_EEPROM_64KB                                  // Если журнал находится в i2c
  void Format(void);                           		    // форматирование журнала в еепром
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
struct type_SaveON {
	byte magic;               // признак данных, должно быть  0x55
	uint8_t flags;           // флаги состояния ТН
	MODE_HP mode;             // режим работы отопление|охлаждение
	uint32_t startTime;       // дата и время включения ТН
	uint32_t P1;              // Резервный параметр 1
};

// Структуры для хранения настроек бойлера
//  Определение флагов в type_boilerHP
#define fScheduleAddHeat 	0           // флаг Расписание только для ТЭНа
#define fSchedule        	1           // флаг Использование расписания
#define fSalmonella      	2           // флаг Сальмонела раз внеделю греть бойлер
#define fCirculation     	3           // флаг Управления циркуляционным насосом ГВС
#define fResetHeat       	4           // флаг Сброса лишнего тепла в СО
#define fTurboBoiler     	5           // флаг Ускоренный нагрев бойлера (ТН + ТЭН)
#define fAddHeating      	6           // флаг ДОГРЕВА ГВС ТЭНом
#define fBoilerTogetherHeat	7			// флаг одновременного нагрева бойлера с отоплением до температуры догрева
#define fBoilerPID			8			// ПИД
#define fBoilerUseSun		9		  	// флаг использования солнечного коллектора
#define fAddHeatingForce	10			// флаг Включать догрев, если компрессор не нагрел бойлер до температуры догрева

struct type_boilerHP {
	uint8_t reserved1;                 // Резерв
	int16_t TempTarget;                // Целевая температура бойлера
	int16_t dTemp;                     // гистерезис целевой температуры
	int16_t tempIn;                    // Tемпература подачи максимальная/минимальная если охлаждение ЗАЩИТА
	uint16_t flags;                    // Флаги
	uint32_t Schedule[7];              // Расписание бойлера
	uint16_t Circul_Work;              // Время  работы насоса ГВС секунды (fCirculation)
	uint16_t Circul_Pause;             // Пауза в работе насоса ГВС  секунды (fCirculation)
	uint16_t Reset_Time;               // время сброса излишков тепла в секундах (fResetHeat)
	uint16_t pid_time; 				// Период расчета ПИД в секундах
	PID_STRUCT pid;                    // Настройки и переменные ПИД регулятора
	int16_t dAddHeat;                  // Гистерезис нагрева бойлера до температуры догрева, в сотых градуса
	int16_t tempRBOILER;               // Температура догрева бойлера
	int16_t add_delta_temp; 	        // Добавка температуры к установке бойлера, в сотых градусах
	uint8_t add_delta_hour;	        // Начальный Час добавки температуры к установке бойлера
	uint8_t add_delta_end_hour; 	 	// Конечный Час добавки температуры к установке
	int16_t tempPID;                   // Целевая температура ПИД
};

// Структуры для хранения настроек Отопления и Охлаждения (одинаковые)
// Работа с отдельными флагами type_settingHP
#define fTarget      0                // Что является целью  - 0 (температура в доме), 1 (температура обратки).
#define fWeather     1                // флаг Погодозависмости
#define fHeatFloor   2				  // флаг использования теплого пола
#define fUseSun      3				  // флаг использования солнечного коллектора

struct type_DailySwitch {
	uint8_t Device;					// Реле; 0 - нет и конец массива
	uint8_t TimeOn;					// Время включения hh:m0
	uint8_t TimeOff;				// Время выключения hh:m0
} __attribute__((packed));

struct type_settingHP {
	uint8_t flags;                       // Флаги опций до 8 флагов
	RULE_HP Rule;                        // алгоритм работы отопления.
	int16_t Temp1;                       // Целевая температура дома
	int16_t Temp2;                       // Целевая температура обратки
	int16_t dTemp;                       // гистерезис целевой температуры
	uint16_t pid_time; 				  // Период расчета ПИД в секундах
	PID_STRUCT pid;                      // Настройки и переменные ПИД регулятора
	uint16_t reserved1;
	// Защиты
	int16_t tempIn;                      // Tемпература подачи (минимальная для охлажления или максимальная для нагрева)
	int16_t tempOut;                     // Tемпература обратки (максимальная для охлажления или минимальная для нагрева)
	uint16_t reserved2;                  // Резерв
	int16_t dt;                          // Максимальная разность температур конденсатора.
	int16_t kWeather;                    // Коэффициент погодозависимости в ТЫСЯЧНЫХ градуса на градус (проблемы с преобраованием - округление)
	int16_t dTempDay;                    // гистерезис целевой температуры днем
	int16_t add_delta_temp;		         // Добавка температуры к установке отопления, в сотых градусах
	uint8_t add_delta_hour;		         // Начальный Час добавки температуры к установке
	uint8_t add_delta_end_hour; 		 // Конечный Час добавки температуры к установке
	int16_t tempPID;                     // Целевая темперaтура ПИД
};

#define LEN_PROFILE_NAME       25     // Длина имени профиля
#define fEnabled               0      // Разрешение данного профайла использоваться в коротком списке
struct type_dataProfile               // Хранение общих данных
{
	int8_t id;                          // Номер профайла 0..I2C_PROFIL_NUM-1 (1 элемент структуры!)
	uint8_t flags;                      // Флаги профайла (2 элемент структуры!)
	uint16_t len;                       // Длина данных
	uint32_t saveTime;                  // Время сохранения профиля
	char name[LEN_PROFILE_NAME];        // Имя профайла
	char note[85]; // Описание профайла кодирование описания профиля 40 русских букв (одна буква 6 байт (двойное кодирование) это входная строка)  описание переводится и хранится в utf8 по 2 байта символ
};


class Profile                         // Класс профиль
{
  public: 
    type_SaveON SaveON;                                     // Структура для хранения состяния ТН в ЕЕПРОМ
   // Настройки для отопления и охлаждения бойлера
    type_settingHP Cool;                                    // Настройки для режима охлаждение
    type_settingHP Heat;                                    // Настройки для режима отопления
    type_boilerHP Boiler;                                   // Настройка бойлера
	type_DailySwitch DailySwitch[DAILY_SWITCH_MAX]; 		// дневное периодическое включение
    
     // Функции работы с профилем
    void initProfile();                                     // инициализация профиля
    char *get_list(char *c /*,int8_t num*/);                // ДОБАВЛЯЕТ к строке с - список возможных конфигураций num - текущий профиль
    int8_t set_list(int8_t num);                            // Устанавливает текущий профиль из номера списка, ДОБАВЛЯЕТ к строке с
    int8_t update_list(int8_t num);                         // обновить список имен профилей

    char *get_info(char *c,int8_t num);                     // ДОБАВЛЯЕТ к строке с - описание профиля num
    int16_t save(int8_t num);                               // Записать профайл в еепром под номерм num
    int32_t load(int8_t num);                               // загрузить профайл num из еепром память
    int8_t  loadFromBuf(int32_t adr,byte *buf);             // Считать настройки из буфера на входе адрес с какого, на выходе код ошибки (меньше нуля)
    int8_t  convert_to_new_version(void);
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
    int32_t load_from_EEPROM_SaveON_mode(int8_t id);	    // Прочитать из EEPROM режим работы ТН (SaveON)
 
    char list[I2C_PROFIL_NUM*(LEN_PROFILE_NAME+2)+1];         // текущий список конфигураций, не забывем про :1 (список)
    inline int32_t get_sizeProfile() { // определить длину данных
    	return sizeof(magic) + sizeof(crc16) +
				// данные контрольная сумма считается с этого места
				sizeof(dataProfile) + sizeof(SaveON) + sizeof(Cool) + sizeof(Heat) + sizeof(Boiler)	+ sizeof(DailySwitch);
    };
 private:
  int8_t err;                                               // Ошибка
 
  // заголовок
  byte magic;                                               // признак данных, должно быть  0xaa
  uint16_t crc16;                                           // Контрольная сумма
  type_dataProfile dataProfile;                             // данные профиля
   uint16_t get_crc16_mem();                                 // Расчитать контрольную сумму
   int8_t   check_crc16_eeprom(int8_t num);   // Проверить контрольную сумму ПРОФИЛЯ в EEPROM для данных на выходе ошибка, длина определяется из заголовка
   int8_t   check_crc16_buf(int32_t adr, byte* buf);         // Проверить контрольную сумму в буфере для данных на выходе ошибка, длина определяется из заголовка
};

#endif
