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
// Константы проекта которые описывают специфику КОНФИГУРАЦИИ ТН
// --------------------------------------------------------------------------------
#ifndef Constant_h
#define Constant_h
#include "Config.h"                         // Цепляем сразу конфигурацию


// Макросы работы с битами байта, используется для флагов
#define GETBIT(b,f)   ((b&(1<<(f)))?true:false)              // получить состяние бита
#define SETBIT1(b,f)  (b|=(1<<(f)))                          // установка бита в 1
#define SETBIT0(b,f)  (b&=~(1<<(f)))                         // установка бита в 0


// СЕТЕВЫЕ НАСТРОЙКИ --------------------------------------------------------------
// По умолчанию и в демо режиме (действуют там и там)
// В рабочем режиме настройки берутся из ЕЕПРОМ, если прочитать не удалось, то действуют настройки по умолчанию
byte defaultMAC[] = { 0xDE, 0xA1, 0x1E, 0x01, 0x02, 0x03 };// не менять
const uint16_t  defaultPort=80;

// ОПЦИИ КОМПИЛЯЦИИ ПРОЕКТА -------------------------------------------------------
#define VERSION         "0.958 beta"        // Версия прошивки
#ifndef UART_SPEED
#define UART_SPEED       115200             // Скорость отладочного порта
#endif
#define DEBUG                               // В последовательный порт шлет сообщения в первую очередь ошибки
//#define LOG                               // В последовательный порт шлет лог веб сервера (логируются запросы)
#define FAST_LIB                            // использование допиленной библиотеки езернета Обычно используется
#define TIME_ZONE         3                 // поправка на часовой пояс по ДЕФОЛТУ
#define NTP_SERVER        "time.nist.gov"   // NTP сервер для синхронизации времени по ДЕФОЛТУ
#define NTP_SERVER_LEN    60                // максимальная длиная адреса NTP сервера
#define NTP_PORT		  123
#define NTP_LOCAL_PORT    8888              // локальный порт, который будет прослушиваться на предмет UDP-пакетов NTP сервера
#define NTP_REPEAT        3                 // Число попыток запросов NTP сервера
#define NTP_REPEAT_TIME   1000              // (мсек) Время между повторами ntp пакетов
#define PING_SERVER       "8.8.8.8"          // ping сервер по ДЕФОЛТУ
#define WDT_TIME          10                // период Watchdog таймера секунды но не более 16 секунд!!! ЕСЛИ установить 0 то Watchdog будет отключен!!!
#define INDEX_FILE        "index.html"       // стартовый файл по умолчанию для большой морды
#define INDEX_MOB_FILE    INDEX_FILE         // стартовый файл по умолчанию для мобильной морды
#define MOB_PATH          "/mob/"            // Путь к мобильной морде
#define VER_SAVE          126               // Версия формата сохраняемых данных
#define HEADER_BIN        "HP-SAVE-DATA"    // Заголовок (начало) файла при сохранении настроек. Необходим для поиска данных в буфере данных при восстановлении из файла
#define MAX_LEN_PM        250               // максимальная длина строкового параметра в запросе (расписание бойлера 175 байт) кодирование описания профиля 40 букв одна буква 6 байт (двойное кодирование)
#define CHART_POINT       300               // Максимальное число точек графика //300 - рабоатет

// ------------------- SPI ----------------------------------
// Карта памяти
#define SD_REPEAT         3                 // Число попыток чтения карты, открытия файлов, при неудаче переход на работу без карты
#define SD_SPI_SPEED      SPI_RATE          // ЭТО ДЕЛИТЕЛЬ (SPI_RATE определен в w5100.h)!!! Частота SPI SD = 84/SD_SPI_SPEED т.е. 2-42МГц 3-28МГц 4-21МГц 6-14МГц Диапазон 2-6
#define SD_LOW_SPEED                        // Если этот дефайн то скорость для КАРТЫ понижается вдвое

// чип W5200 (точнее любой используемый чип)
#define W5200_THREARD     3                 // Число потоков для сетевого чипа w5200 допустимо 1-4 потока, на 4 скорее всего не хватить места в оперативке
#define W5200_NUM_PING    4                 // Число попыток пинга до определения потери связи
#define W5200_TIME_PING   1000              // мсек Время между попытками пинга (если не удача)
#define W5200_MAX_LEN     2048              // Максимальная длина буфера, определяется W5200 не более 2048 байт
#define W5200_NUM_LINK    2                 // Число попыток сброса чипа w5500 и проверки появления связи (кабель воткнут) используется для инициализаци чипа
#define W5200_TIME_LINK   4000              // Максимальное время ожидания устанoвления связи (поднятие Link) кабель воткнут  используется для инициализаци чипа (мсек)
#define W5200_TIME_WAIT   3000              // Время ожидания захвата мютекса (переключение потоков) мсек
#define W5200_STACK_SIZE  230               // Размер стека (слова!!! - 4 байта) до обрезки стеков было 340 - работает
#define W5200_SPI_SPEED   SPI_RATE          // ЭТО ДЕЛИТЕЛЬ (SPI_RATE определен в w5100.h)!!! Частота SPI w5200 = 84/W5200_SPI_SPEED т.е. 2-42МГц 3-28МГц 4-21МГц 6-14МГц Диапазон 2-6
#define W5200_SOCK_SYS    (MAX_SOCK_NUM-1)  // Номер системного сокета который не использутся в вебсервере, это последний сокет, НЕ МЕНЯТЬ
#define W5200_RTR         (2*0x07D0)          // время таймаута в 100 мкс интервалах  (по умолчанию 200ms(100us X 2000(0x07D0))) актуально для комманд CONNECT, DISCON, CLOSE, SEND, SEND_MAC, SEND_KEEP
#define W5200_RCR         (0x08)            // число повторов передачи  (по умолчанию 0x08 раз))

// ------------------- SERIAL --------------------------------
//Nextion дисплей
//#define NEXTION_DEBUG                     // Выводить информацию в отладочный порт с дисплея
#define NEXTION_PORT      Serial1            // Аппаратный порт куда прицеплен дисплей
#define NEXTION_UPDATE    5000               // Время обновления информации на дисплее Nextion (мсек)
#define NEXTION_READ      20                 // Время опроса дисплея Nextion (мсек) разбор входной очереди

// Конфигурирование Modbus для инвертора и счетчика SDM
#ifndef MODBUS_PORT_NUM
#define MODBUS_PORT_NUM      Serial2        // Аппаратный порт куда прицеплен Modbus
#define MODBUS_PORT_SPEED    9600           // Скорость порта куда прицеплен частотник и счетчик
#define MODBUS_PORT_CONFIG   SERIAL_8N1     // Конфигурация порта куда прицеплен частотник и счетчик
#define MODBUS_TIME_WAIT     2000           // Время ожидания захвата мютекса для modbus мсек
#define MODBUS_TIME_TRANSMISION 4           // Пауза (msec) между запросом и ответом по модбас было 4
#endif
//#define MODBUS_FREERTOS                     // Настроить либу на многозадачность определить надо в либе.
#if RADIO_SENSORS_PORT == 2
	#define RADIO_SENSORS_SERIAL	Serial2			// Аппаратный порт
#elif RADIO_SENSORS_PORT == 3
	#define RADIO_SENSORS_SERIAL	Serial3			// Аппаратный порт
#endif

// Глобальные параметры инвертора инвертора на модбасе зависят от компрессора!!!!!!!!!
#define FC_MODBUS_ADR      1             // Адрес частотного преобразователя на шине не должно совпадать SMD_MODBUS_ADR
#define FC_TIME_READ      (8*1000)       // Время опроса инвертора в мск (было 6)
#define FC_NUM_READ        5             // Число попыток чтения инвертора (подряд) по модбас до его останова ТН по ошибке
#define FC_DELAY_REPEAT    50            // мсек Время между ПОВТОРНЫМИ попытками чтения было 100
#define FC_DELAY_READ      5            // мсек Время между последовательными запросами было 20

// Глобальные переменные счетчика SDM120 на модбасе
// настройки связи со счетчиком по умолчанию (из коробки см инструкцию) требуется для его программирования для работы
#define DEFAULT_SDM_SPEED       2400      // Скорость в бодах для счетчика по умолчанию
#define DEFAULT_SDM_MODBUS_ADR  1         // Адрес счетчика на шине не должно совпадать с FC_MODBUS_ADR при программировании инвертор ОКЛЮЧИТЬ (адрес 1)
// Требуемые настройки связи (после программирования)
#define SDM_SPEED           2              // скорость счетчика в константах 0 = 2400 bps. 1 = 4800 bps. 2 = 9600 bps 5=1200 bps  Скорости обмена должны совпадать см MODBUS_PORT_SPEED
#define SDM_MODBUS_ADR      2              // Адрес счетчика на шине не должно совпадать с FC_MODBUS_ADR
#define SDM_TIME_READ       (10*1000)      // Время опроса счетчика в мск
#define SDM_NUM_READ        8              // Число попыток чтения счетчика (подряд) по модбас до его отключения (ошибка не генерится)
#define SDM_DELAY_REPEAD    50             // мсек Время между ПОВТОРНЫМИ попытками чтения                      было 150
#define SDM_DELAY_READ      5             // Время между последовательными запросами на чтение счетчика (мсек) было 20
 
// ------------------- TIME & DELAY ----------------------------------
// Времена и задержки
#define cDELAY_DS1820      750             // мсек. Задержка для чтения DS1820 (время преобразования)
#ifndef TIME_READ_SENSOR 
#define TIME_READ_SENSOR  4000		      	 // мсек. Период опроса датчиков, к нему добавляется время DELAY_DS1820 и 1мсек * TNUMBER
#endif
#define TIME_WEB_SERVER   5                // мсек. Период опроса web servera было 5
#define TIME_CONTROL      (10*1000)        // мсек. Период управления тепловым насосом (цикл управления в режиме Гистерезис)
#define TIME_EEV          (4*1000)         // мсек. Период управления ЭРВ (цикл управления)
#define TIME_COMMAND      500              // мсек. Период разбора команд управления ТН (скорее пауза перед обработкой команды)
#define TIME_I2C_UPDATE   (60*60)          // секунды Время обновления внутренних часов по I2С часам (если конечно нужно) каждый час 60*60
#define TIME_MESSAGE_TEMP  300			   // 1/10 секунды, Проверка граничных температур для уведомлений
#define TIME_LED_OK       1500             // Период мигания светодиода при ОК (мсек)
#define TIME_LED_ERR      200              // Период мигания светодиода при ошибке (мсек).
#define cDELAY_START_MESSAGE 60            // Задержка (сек) после старта на отправку сообщений

// ------------------- I2C ----------------------------------
// Устройства i2c I2C_EEPROM_64KB и I2C_FRAM_MEMORY   Размер и тип памяти, определен в config.h т.к. он часто меняется
#define I2C_SPEED         twiClock400kHz // Частота работы шины I2C
#define I2C_NUM_INIT         3           // Число попыток инициализации шины
#define I2C_TIME_WAIT        2000        // Время ожидания захвата мютекса шины I2C мсек
#define I2C_ADR_RTC          0x68        // Адрес чипа rtc на шине I2C
#define I2C_ADR_DS2482       0x18        // Адрес чипа OneWire на шине I2C 3-х проводная
#define I2C_ADR_DS2482_2     0x19        // Адрес чипа OneWire на 2-ой шине I2C
#define I2C_ADR_DS2482_3     0x1A        // Адрес чипа OneWire на 3-ей шине I2C
#define I2C_ADR_DS2482_4     0x1B        // Адрес чипа OneWire на 4-ой шине I2C

// --------------------------------------------------------------------------------------------------------------------------
// КАРТА ПАМЯТИ в чипе i2c объемом 64 кбайта
// 0х0000 - I2C_COUNT_EEPROM хранение счетчиков, максимальный размер 0x79 (127) байт. Сейчас используется 52 байта
// 0х0080 - I2C_SETTING_EEPROM хранение настроек ТН  максимальный размер 0х580 (1408) байт. Сейчас используется 1074 байта
// 0х0600 - I2C_PROFILE_EEPROM хранение профилей, максимальный размер 0x9ff(2559) байт. Число профилей 8 шт. Размер профиля сейчас  267 байт (8*259=2072)
// 0x0E60 - I2C_SCHEDULER_EEPROM хранения расписаний, максимум 415 байт (сейчас 377 байта)
// 0x0fff ----- конец 4 кбайтного чипа, дальше идет распределение если определено I2C_EEPROM_64KB
// 0х0fff - I2C_JOURNAL_EEPROM хранение журнала размер журнала область должна быть кратна W5200_MAX_LEN
// 0х9fff - I2C_STAT_EEPROM хранение статистики теплового насоса. Размер данных одного дня 26 байта хранить надо максимум 1 год 365 дней  или 9620 байт отведем 10 кб
// 0xffff ---- конец 64 кбайтного чипа
#define I2C_PROFIL_NUM			8             // Максимальное число сохряняемых профилей
#define I2C_COUNT_EEPROM		0x00          // Адрес внутри чипа eeprom от куда пишется счетчики с начала чипа 0
#define I2C_SETTING_EEPROM		0x080         // Адрес внутри чипа eeprom от куда пишутся настройки ТН  а перед ним пишется счетчики
#define I2C_PROFILE_EEPROM		0x600         // Адрес внутри чипа eeprom от куда профили (адрес первого профиля)
#define I2C_SCHEDULER_EEPROM	0xE60		  // Адрес внутри чипа eeprom для Расписаний
#ifdef  I2C_EEPROM_64KB                  // Стартовые адреса -----------------------------------------------------
  #define I2C_JOURNAL_EEPROM (0x0fff)    // Адрес с которого начинается журнал в памяти i2c ВНИМАНИЕ в смещении -2 байт лежит признак форматирования журнала. Длина журнала JOURNAL_LEN
  #define I2C_STAT_EEPROM    (0xd7ff)    // Адрес с которого начинается статисткика ТН в памяти i2c ВНИМАНИЕ в начале  6 байт лежит заголовок статистики
#endif
#ifdef I2C_EEPROM_64KB                                                                                   // Длины -----------------------------------------------------
  // Журнал
  #define I2C_JOURNAL_START (I2C_JOURNAL_EEPROM)                                                         // Адрес с которого начинается ДАННЫЕ журнал в памяти i2c ВНИМАНИЕ - 2 байт лежит признак форматирования журнала
 // #define JOURNAL_LEN (((int)(I2C_SIZE_EEPROM*1024/8-I2C_JOURNAL_START)/W5200_MAX_LEN)*W5200_MAX_LEN)  // Размер журнала - округление на целое число страниц W5200_MAX_LEN
  #define JOURNAL_LEN (((int)(I2C_STAT_EEPROM-I2C_JOURNAL_START)/W5200_MAX_LEN)*W5200_MAX_LEN)           // Размер журнала - округление на целое число страниц W5200_MAX_LEN
 // #define JOURNAL_LEN (2*W5200_MAX_LEN)                                                                // ОТЛАДКА Размер журнала - 2* W5200_MAX_LEN
  #define I2C_JOURNAL_HEAD   (0x01)                                                                      // Признак головы журнала
  #define I2C_JOURNAL_TAIL   (0x02)                                                                      // Признак хвоста журнала
  #define I2C_JOURNAL_FORMAT (0xff)                                                                      // Символ которым заполняется журнал при форматировании
  #define I2C_JOURNAL_READY  (0x55aa)                                                                    // Признак создания журнала - если его нет по адресу I2C_JOURNAL_START-2 то надо форматировать журнал (первичная инициализация)
 // Статситика
  #define I2C_STAT_START (I2C_STAT_EEPROM+6)                                                             // Адрес с которого начинается ДАННЫЕ статистики в памяти i2c ВНИМАНИЕ - 6 байт в начале лежит признак форматирования журнала, и адрес первого и последнего элемента
  #define STAT_LEN (((int)(0xffff-I2C_STAT_EEPROM)/W5200_MAX_LEN)*W5200_MAX_LEN)                         // Размер статистки - округление на целое число страниц W5200_MAX_LEN в байтах
  #define STAT_POINT  ((int)(STAT_LEN/sizeof(type_OneDay)-1))                                            // Максимальное число хранимых точек
  #define I2C_STAT_FORMAT (0xff)                                                                         // Символ которым заполняется статистика при форматировании
  #define I2C_STAT_READY  (0xaa55)                                                                       // Признак создания журнала - если его нет по адресу I2C_JOURNAL_START-2 то надо форматировать журнал (первичная инициализация)
#else  
  #define JOURNAL_LEN       (2*W5200_MAX_LEN)                                                            // Размер системного журнала ДОЛЖНО БЫТЬ кратно W5200_MAX_LEN, Увеличивать аккуратно, может не хвать памяти - виснет при загрузке
#endif
// Тип записи сохранения, 16bit
#define SAVE_TYPE_END			0
#define SAVE_TYPE_sTemp			-1
#define SAVE_TYPE_sADC			-2
#define SAVE_TYPE_sInput		-3
#define SAVE_TYPE_sFrequency	-4
#define SAVE_TYPE_sIP			-5
#define SAVE_TYPE_dEEV			-6
#define SAVE_TYPE_dSDM			-7
#define SAVE_TYPE_clMQTT		-8
#define SAVE_TYPE_LIMIT			-100

// ------------------- EEV ----------------------------------
// Константы фаз движения ЭРВ, три варианта (константы вариантов не менять!)
    #define PHASE_4       0              // 4 фазы, шаг
    #define PHASE_8       1              // 8 фаз,  шаг
    #define PHASE_8s      2              // 8 фаз, полушаг основной вариант (pav2000)

	#ifndef DEFAULT_RULE_EEV
		#define DEFAULT_RULE_EEV   0			  // Формула по умолчанию
		#define DEFAULT_FREON_TYPE 0 			  // Типа фрона по умолчанию
		#define DEFAULT_OVERHEAT   400			  // Перегрев по умолчанию (сотые градуса)
	#endif

// ----------------------- EVI ------------------------------
#define EVI_TEMP_CON      4000           // Температура кондесатора для включения EVI
#define EVI_TEMP_EVA      300            // Температура испарителя для включения EVI

// ------------------- ОБЩИЕ НАСТРОЙКИ ----------------------------------
#define TEMP_WEATHER      0              // Температура при которой устанавливается целевая температура подачи для отопления (погодозависимость)
#define MIN_WEATHER       (12*100)       // Минимальная температура подачи при погодозависимости
#define MAX_WEATHER       (42*100)       // Максимальная температура подачи при погодозависимости
#define HYSTERESIS_RHEAD  20             // Гистерезис работы дополнительного тена отопления (вычитается из целевой) в сотых градуса
#define HYSTERESIS_RBOILER 30            // Гистерезис работы дополнительного тена ГВС догрева (вычитается из целевой) в сотых градуса
#define SALLMONELA_DAY    3              // День когда включается алгоритм обеззараживания воды (Понедельник 1 воскресенье 7)
#define SALLMONELA_HOUR   1              // Час когда включается алгоритм обеззараживания воды (должно быть 0 минут)
#define SALLMONELA_TEMP   (70*100)       // Температура которая поддерживается для обеззараживания (сотые градуса)
#define SALLMONELA_TIME   (120*60)       // Максимальная продолжительность цикла (сек), что бы цикл не длился бесконечно при не возможности достижения SALLMONELA_TEMP
//#define SALLMONELA_HARD                  // Если определено то работает поддержание температуры SALLMONELA_TEMP до окончания времени SALLMONELA_TIME, если НЕ ОПРЕДЕЛЕНО то выключение сразу по достижению SALLMONELA_TEMP но цикл не более SALLMONELA_TIME 
#define NIGHT_START_HOUR  23
#define NIGHT_END_HOUR	  7
//#define SALLMONELA_HARD                // Если определено то работает поддержание температуры SALLMONELA_TEMP до окончания времени SALLMONELA_TIME, если НЕ ОПРЕДЕЛЕНО то выключение сразу по достижению SALLMONELA_TEMP но цикл не более SALLMONELA_TIME 
//#define COP_ALL_CALC                     // Проводить расчет КОП всегда, если дефайна нет то КОП считается ТОЛЬКО при работающем компрессоре, в паузах ставится 0

// ------------------- SENSOR PRESS----------------------------------
#define P_NUMSAMLES       5              // Число значений для усреднения показаний давления
#define PRESS_FREQ        200            // Частота опроса аналоговых датчиков
#define FILTER_SIZE       128            // Длина фильтра для датчика давления

// ------------------- SENSOR TEMP----------------------------------
#ifndef T_NUMSAMLES
#define T_NUMSAMLES       1              // Число значений для усреднения показаний температуры
#define GAP_TEMP_VAL      300      		 // Допустимая разница (в сотых C) показаний между двумя считываниями (борьба с помехами) - при привышении ошибка не возникает, но данные пропускаются.
#define GAP_NUMBER        3  		     // Максимальное число идущих подряд показаний превышающих GAP_TEMP_VAL, после этого эти показания выдаются за действительные
#define GAP_TEMP_VAL_CRC  200     	     // Датчики с флагом игнорировать CRC. Допустимая разница (в сотых C) показаний между двумя считываниями (борьба с помехами) - при привышении ошибка не возникает, но данные пропускаются.
#define GAP_NUMBER_CRC    5       	     // Датчики с флагом игнорировать CRC. Максимальное число идущих подряд показаний превышающих на GAP_TEMP_VAL, после этого эти показания выдаются за действительные
#endif
#define MAX_TEMP_ERR      700            // Максимальная систематическая ошибка датчика температуры (сотые градуса)
#define NUM_READ_TEMP_ERR 10             // Число ошибок подряд чтения датчика температуры после которого считается что датчик не исправен
#define RES_ONEWIRE_ERR   3              // Число попыток сброса датчиков температуры перед генерацией ошибки шины

#define BASE_TIME_READ    10 //20        // Частотные датчики - время (сек) на котором измеряется число импульсов, в конце идет пересчет в частоту

#define UPDATE_IP         120            // Время с получения последнего пакета от удаленного датчика (в сек) после которого считается что датчик не активен и не используетс в расчетах

// ------------------- MQTT ----------------------------------
#define MQTT                             // признак использования MQTT при неиспользовании необходимо закоментировать
#define MQTT_REPEAT                      // Делать попытку повторного соединения с сервером
#define MQTT_NUM_ERR_OFF   8             // Число ошибок отправки подряд при котором отключается сервис отправки MQTT (флаг сбрасывается)

#define DEFAULT_PORT_MQTT 1883           // Адрес порта сервера MQTT
#define DEFAULT_TIME_MQTT (3*60)         // период отправки на сервер в сек. 10...60000
#define DEFAULT_ADR_MQTT "mqtt.thingspeak.com"   // Адрес MQTT сервера по умолчанию
#define DEFAULT_ADR_NARMON "narodmon.ru" // Адрес сервера народного мониторинга
#define TIME_NARMON       (5*60)         // (сек) Период отправки на народный монитоинг (константа) меньше 5 минут не ставить


// ------------------- HEAP ----------------------------------
#define PASS_LEN          10             // Максимальная длина пароля для входа на контроллер
#define NAME_USER         "user"         // имя пользователя
#define NAME_ADMIN        "admin"        // имя администратора
#define FILE_CHART        "chart_sd.csv" // имя файла графиков при записи на карту памяти
#define HOUR_SIGNAL_LIFE  12             // Час когда генерится сигнал жизни

#define ATOF_ERROR       -9876543.00     // Код ошибки преобразования строки во флоат
#define K_VCC_POWER       338.2          // Коэффициент пересчета ацп в вольты для контроля питания (учет опоры) (UT71E результататы ЗИП 284.02 ТН 338.2)
#define HEAT_CAPACITY     4174           // теплоемкость жидкости в конутре по дефолту при 30 градусах [Cp, Дж/(кг·град)]

// Константы регистров контроллера питания SOPC SAM3x ---------------------------------------
// Регистр SMMR
// Уровень контроля питания ядра
#define   SUPC_SMMR_SMTH_1_9V (0x0u << 0) 
#define   SUPC_SMMR_SMTH_2_0V (0x1u << 0) 
#define   SUPC_SMMR_SMTH_2_1V (0x2u << 0) 
#define   SUPC_SMMR_SMTH_2_2V (0x3u << 0) 
#define   SUPC_SMMR_SMTH_2_3V (0x4u << 0) 
#define   SUPC_SMMR_SMTH_2_4V (0x5u << 0) 
#define   SUPC_SMMR_SMTH_2_5V (0x6u << 0) 
#define   SUPC_SMMR_SMTH_2_6V (0x7u << 0) 
#define   SUPC_SMMR_SMTH_2_7V (0x8u << 0) 
#define   SUPC_SMMR_SMTH_2_8V (0x9u << 0) 
#define   SUPC_SMMR_SMTH_2_9V (0xAu << 0) 
#define   SUPC_SMMR_SMTH_3_0V (0xBu << 0) 
#define   SUPC_SMMR_SMTH_3_1V (0xCu << 0) 
#define   SUPC_SMMR_SMTH_3_2V (0xDu << 0) 
#define   SUPC_SMMR_SMTH_3_3V (0xEu << 0) 
#define   SUPC_SMMR_SMTH_3_4V (0xFu << 0) 
// Время контроля питания
#define   SUPC_SMMR_SMSMPL_SMD (0x0u << 8)       // запрещено
#define   SUPC_SMMR_SMSMPL_CSM (0x1u << 8)       // непрерывно
#define   SUPC_SMMR_SMSMPL_32SLCK (0x2u << 8) 
#define   SUPC_SMMR_SMSMPL_256SLCK (0x3u << 8) 
#define   SUPC_SMMR_SMSMPL_2048SLCK (0x4u << 8) 
// Регистр MR
#define SUPC_MR_KEY_Pos 24
#define SUPC_MR_KEY_Msk (0xffu << SUPC_MR_KEY_Pos) // Ключ для записи!!!
#define SUPC_MR_KEY(value) ((SUPC_MR_KEY_Msk & ((value) << SUPC_MR_KEY_Pos)))
#define SUPC_MR_BODRSTEN (0x1u << 12) /**< \brief (SUPC_MR) Brownout Detector Reset Enable */
#define SUPC_MR_BODRSTEN_NOT_ENABLE (0x0u << 12) /**< \brief (SUPC_MR) the core reset signal "vddcore_nreset" is not affected when a brownout detection occurs. */
#define SUPC_MR_BODRSTEN_ENABLE (0x1u << 12) /**< \brief (SUPC_MR) the core reset signal, vddcore_nreset is asserted when a brownout detection occurs. */
#define SUPC_MR_BODDIS (0x1u << 13)     // Вот это /**< \brief (SUPC_MR) Brownout Detector Disable */
#define SUPC_MR_BODDIS_ENABLE (0x0u << 13) /**< \brief (SUPC_MR) the core brownout detector is enabled. */
#define SUPC_MR_BODDIS_DISABLE (0x1u << 13) /**< \brief (SUPC_MR) the core brownout detector is disabled. */

// --------------------------------------------------------------------------------
// ОШИБКИ едины для всего - сквозной список
// --------------------------------------------------------------------------------
#define OK                  0          // Ошибок нет
#define ERR_MINTEMP        -1          // Выход за нижнюю границу температурного датчика
#define ERR_MAXTEMP        -2          // Выход за верхнюю границу температурного датчика
#define ERR_MINPRESS       -3          // Выход за нижнюю границу  датчика давления
#define ERR_MAXPRESS       -4          // Выход за верхнюю границу датчика давления
#define ERR_SENSOR         -5          // Датчик запрещен в текущей конфигурации
#define ERR_ADDRESS        -6          // Адрес датчика температруры не установлен
#define ERR_DINPUT         -7          // Срабатывания контактного датчика -  авария
#define ERR_MAXERV         -8          // Выход за диапазон (по шагам) в верху
#define ERR_MINERV         -9          // Выход за диапазон (по шагам) в внизу
#define ERR_DEVICE         -10         // Устройство запрещено в текущей конфигурации
#define ERR_ONEWIRE        -11         // Ошибка сброса на OneWire шине (обрыв или замыкание)
#define ERR_OVERHEAT       -12         // ЭРВ получило отрицательный перегрев
#define ERR_MEM_FREERTOS   -13         // Free RTOS не может создать задачу - мало пямяти
#define ERR_PEVA_EEV       -14         // Отсутвует датчик давления, и выбран алгоритм ЭРВ который его использует
#define ERR_SAVE_EEPROM    -15         // Ошибка записи настроек в eeprom I2C
#define ERR_LOAD_EEPROM    -16         // Ошибка чтения настроек из eeprom I2C
#define ERR_CRC16_EEPROM   -17         // Ошибка контрольной суммы для настроек
#define ERR_BAD_LEN_EEPROM -18         // Не совпадение размера данных при чтении настроек
#define ERR_HEADER_EEPROM  -19         // Данные настроек не найдены  в eeprom I2C
#define ERR_SAVE1_EEPROM   -20         // Ошибка записи состояния в eeprom I2C
#define ERR_LOAD1_EEPROM   -21         // Ошибка чтения состояния из eeprom I2C
#define ERR_HEADER1_EEPROM -22         // Данные состояния не найдены в eeprom I2C
#define ERR_SAVE2_EEPROM   -23         // Ошибка записи счетчиков в eeprom I2C
#define ERR_LOAD2_EEPROM   -24         // Ошибка чтения счетчиков из eeprom I2C
#define ERR_VER_EEPROM     -25         // Не совпадение версий данных в eeprom I2C
#define ERR_DTEMP_CON      -26         // Привышена разность температур на кондесаторе
#define ERR_DTEMP_EVA      -27         // Привышена разность температур на испарителе
#define ERR_PUMP_CON       -28         // Отсутствует насос на конденсаторе, проверьте конфигурацию
#define ERR_PUMP_EVA       -29         // Отсутствует насос на испарителе, проверьте конфигурацию
#define ERR_READ_PRESS     -30         // Ошибка чтения датчика давления (данные не готовы)
#define ERR_NO_COMPRESS    -31         // Отсутствует компрессор, проверьте конфигурацию
#define ERR_NO_WORK        -32         // Все выключено а ТН включается
#define ERR_COMP_ERR       -33         // Попытка включить компрессор при ошибке (обратитесь к разработчику)
#define ERR_CONFIG         -34         // Сбой внутренней конфигурации (обратитесь к разработчику)
#define ERR_SD_INIT        -35         // Ошибка инициализации SD карты
#define ERR_SD_INDEX       -36         // Файл index.xxx не найден на SD карте
#define ERR_SD_READ        -37         // Ошибка чтения файла с SD карты
#define ERR_TYPE_OVERHEAT  -38         // Правило вычисления перегрева не соответствует датчикам (обратитесь к разработчику)
#define ERR_485_INIT       -39         // Инвертор на шине Modbus не найден (работа инвертора запрещена)
#define ERR_485_BUZY       -40         // При обращении к 485 порту привышено время ожидания его освобождения
// Ошибки описаные в протоколе modbus
#define ERR_MODBUS_0x01      -41         // Modbus 0x01 protocol illegal function exception
#define ERR_MODBUS_0x02      -42         // Modbus 0x02 protocol illegal data address exception
#define ERR_MODBUS_0x03      -43         // Modbus 0x03 protocol illegal data value exception
#define ERR_MODBUS_0x04      -44         // Modbus 0х04 protocol slave device failure exception
#define ERR_MODBUS_0xe0      -45         // Modbus 0xe0 Master invalid response slave ID exception
#define ERR_MODBUS_0xe1      -46         // Modbus 0xe1 Master invalid response function exception
#define ERR_MODBUS_0xe2      -47         // Modbus 0xe2 Master response timed out exception
#define ERR_MODBUS_0xe3      -48         // Modbus 0xe3 Master invalid response CRC exception
#ifdef FC_VACON // Спицифические ошибки Vocon 10
  #define ERR_MODBUS_VACON_0x05 -49       // Ведомое устройство приняло запрос и обрабатывает его, но это требует много времени. Этот ответ предохраняет ведущее устройство от генерации ошибки тайм-аута.
  #define ERR_MODBUS_VACON_0x06 -50       // Ведомое устройство занято обработкой команды. Ведущее устройство должно повторить сообщение позже, когда ведомое освободится.
  #define ERR_MODBUS_VACON_0x07 -52       // Ведомое устройство не может выполнить программную функцию, заданную в запросе.
  #define ERR_MODBUS_VACON_0x08 -52       // Ведомое устройство при чтении расширенной памяти обнаружило ошибку контроля четности
  #define ERR_MODBUS_VACON_0000 -53       // пусто для сохранения нумерации
  #define ERR_MODBUS_VACON_0001 -54       // пусто для сохранения нумерации
  #define ERR_MODBUS_VACON_0002 -55       // пусто для сохранения нумерации
#else           // Спицифические ошибки OMRON
  #define ERR_MODBUS_MX2_0x01  -49        // Omron mx2 Код исключения 0x01 Указанная функция не поддерживается
  #define ERR_MODBUS_MX2_0x02  -50        // Omron mx2 Код исключения 0x02 Указанная функция не обнаружена.
  #define ERR_MODBUS_MX2_0x03  -52        // Omron mx2 Код исключения 0x03 Неприемлемый формат указанных данных
  #define ERR_MODBUS_MX2_0x05  -52        // Omron mx2 ошибка связи по Modbus (функция проверка связи 0x08 Omron mx2)
  #define ERR_MODBUS_MX2_0x21  -53        // Omron mx2 Код исключения 0x21 Данные, записываемые в регистр хранения, находятся за пределами ПЧ
  #define ERR_MODBUS_MX2_0x22  -54        // Omron mx2 Код исключения 0x22 Указанные функции не доступны для ПЧ
  #define ERR_MODBUS_MX2_0x23  -55        // Omron mx2 Код исключения 0x23 Регистр (бит), в который должно быть записано значение, доступен только для чтения
#endif
#define ERR_MODBUS_UNKNOW    -56         // Modbus не известная ошибка (сбой протокола)
#define ERR_MODBUS_STATE     -57         // Запрещенное (не верное) состояние инвертора
#define ERR_MODBUS_BLOCK    -58         // Попытка включения ТН при заблокированном инверторе
#define ERR_PID_FEED       -59         // Алгоритм ПИД - достижение максимальной температуры подачи (защита) Подача целевая функция, защита выше, и этого не должно быть
#define ERR_OUT_OF_MEMORY  -60         // Не хватает памяти для выделения массивов
#define ERR_SAVE_PROFILE   -61         // Ошибка записи профиля в eeprom I2C
#define ERR_LOAD_PROFILE   -62         // Ошибка чтения профиля из eeprom I2C
#define ERR_CRC16_PROFILE  -63         // Ошибка контрольной суммы для профиля
#define ERR_BAD_LEN_PROFILE -64        // Не совпадение размера данных при чтении профиля
#define ERR_DS2482_NOT_FOUND -65       // Мастер DS2482 не найден на шине, возможно ошибка шины I2C
#define ERR_DS2482_ONEWIRE -66         // Мастер DS2482 не может сбросить шину OneWire бит PPD равен 0
#define ERR_I2C_BUZY       -67         // При обращении к I2C шине привышено время ожидания ее освобождения
#define ERR_DRV_EEV        -68         // Отказ драйвера L9333 ЭРВ (сработала защита драйвера)
#define ERR_HEADER2_EEPROM -69         // Ошибка заголовка счетчиков в eeprom I2C
#define ERR_OPEN_I2C_JOURNAL -70       // Ошибка открытия журнала в I2C памяти (инициализация чипа)
#define ERR_READ_I2C_JOURNAL -71       // Ошибка чтения журнала в I2C памяти
#define ERR_WRITE_I2C_JOURNAL -72      // Ошибка записи журнала в I2C памяти
#define ERR_NUM_FREQUENCY   -73        // Указано FNUMBER (количество частотных датчиков) более трех штук
#define ERR_MIN_FLOW        -74        // Поток в ПТО ниже установленного уровня
#define ERR_MAX_VOLTAGE     -75        // Слишком большое напряжение сети
#define ERR_MAX_POWER       -76        // Слишком большая портебляемая мощность
#define ERR_NO_MODBUS       -77        // Modbus требуется но отсутвует в конфигурации
#define ERR_RESET_FC        -78         // Не удалось сбросить инвертор после ошибки
#define ERR_SEVA_FLOW       -79        // Отсутствует проток в испарителе (срабатывание SEVA)
#define ERR_OPEN_I2C_STAT   -80        // Ошибка открытия статистики в  I2C памяти (инициализация чипа)
#define ERR_READ_I2C_STAT   -81        // Ошибка чтения статистики из I2C памяти
#define ERR_WRITE_I2C_STAT  -82        // Ошибка записи статистики в I2C память
#define ERR_FC_CONF_ANALOG  -83        // Ошибка использования аналогового управления инвертором без выхода ХОД
#define ERR_READ_TEMP       -84        // Ошибка чтения температурного датчика (лимит чтения исчерпан)
#define ERR_CONFIG_TEMP     -85        // Ошибка конфигурации температурного датчика (привязка к отсутвующей шине OneWire)
#define ERR_ONEWIRE_CRC     -86		   // ошибка CRC во время чтения OneWire
#define ERR_ONEWIRE_RW      -87		   // ошибка во время чтения/записи OneWire
#define ERR_FC_FAULT		-88			// сбой инвертора
#define ERR_FC_ERROR		-89			// ошибка программы управления инвертором

// Предупреждения
#define WARNING_VALUE        1         // Попытка установить значение за границами диапазона запрос типа SET

#ifndef FORMAT_DATE_STR_CUSTOM
const char *FORMAT_DATE_STR	 = { "%02d/%02d/%04d" };
#endif
// --------------------------------------------------------------------------------------------------------------------------------
// Строковые константы многократно используемые по всем файлам --------------------------------------------------------------------
const char *cYes= {"Да" };
const char *cNo = {"Нет"};
const char *cOne= {"1"  };
const char *cZero={"0"  };
const char *cError={"error"};
const char *cInvalid={"invalid"};
const char *cStrEnd={"\n"};
const char *cErrorRS485={"%s: Read error %s, code=%d repeat . . .\n"};  // имя, функция, код
const char *cErrorMutex={"Function %s: %s, mutex is buzy\n"};           // функция, мютекс
const char *cAddHeat = {"+"};                                           // Значек нагрева ГВС ТЭНом
const char http_get_str1[] = "GET ";
const char http_get_str2[] = " HTTP/1.0\r\nHost: ";
const char http_get_str3[] = "\r\nAccept: text/html\r\n\r\n";
const char http_key_ok1[] = "HTTP/"; // "1.1"
const char http_key_ok2[] = " 200 OK\r\n";
const uint8_t save_end_marker[1] = { 0 };

// Многозадачность, деление аппартных ресурсов
const char *nameFREERTOS =     {"Free RTOS"};           // Имя источника ошибки (нужно для передачи в функцию) - операционная система
const char *nameHeatPump =     {"Heat Pump"};           // Имя теплового насоса (для лога ошибок) Здесь можно его поменять
const char *MutexI2CBuzy =     {"I2C"}; 
const char *MutexModbusBuzy=   {"Modbus"}; 
const char *MutexWebThreadBuzy={"WebThread"}; 
const char *MutexSPIBuzy=      {"SPI"}; 
const char *MutexCommandBuzy = {"Command"}; 

// Описание имен параметров ЭРВ для функций get_paramEEV set_paramEEV
const char *eev_POS           =  {"POS"};           // Положение ЭРВ шаги
const char *eev_POSp          =  {"POSp"};          // Положение ЭРВ %
const char *eev_POSpp         =  {"POSpp"};         // Положение ЭРВ шаги+%
const char *eev_OVERHEAT      =  {"OVERHEAT"};      // Текущий перегрев ЭРВ
//const char *eev_OVERCOOL =  {"OVERCOOL"};    // Переохлаждение
const char *eev_ERROR         =  {"ERROR"};         // Ошибка ЭРВ
const char *eev_MIN           =  {"MIN"};           // Минимум ЭРВ
const char *eev_MAX           =  {"MAX"};           // Максимум ЭРВ
const char *eev_TIME          =  {"TIME"};          // Постоянная интегрирования времени в секундах ЭРВ СЕКУНДЫ
const char *eev_TARGET        =  {"TARGET"};        // Перегрев ЦЕЛЬ (сотые градуса)
const char *eev_KP            =  {"KP"};            // ПИД Коэф пропорц.  В СОТЫХ!!!
const char *eev_KI            =  {"KI"};            // ПИД Коэф интегр.  для настройки Ki=0  В СОТЫХ!!!
const char *eev_KD            =  {"KD"};            // ПИД Коэф дифф.   В СОТЫХ!!!
const char *eev_CONST         =  {"CONST"};         // Корректировка перегрева (постоянная ошибка)
const char *eev_MANUAL        =  {"MANUAL"};        // Число шагов открытия ЭРВ для правила работы ЭРВ «Manual»
const char *eev_FREON         =  {"FREON"};         // Тип фреона
const char *eev_RULE          =  {"RULE"};          // Правило работы ЭРВ
const char *eev_NAME          =  {"NAME"};          // Имя ЭРВ
const char *eev_NOTE          =  {"NOTE"};          // Описание ЭРВ
const char *eev_REMARK        =  {"REMARK"};        // Описание алгоритма ЭРВ
const char *eev_PINS          =  {"PINS"};          // Перечисление ног куда привязана ЭРВ
const char *eev_cCORRECT      =  {"cCORRECT"};      // Флаг включения корректировки перегерва от разности температур конденсатора и испраителя
const char *eev_cDELAY        =  {"cDELAY"};        // Задержка после старта компрессора, сек
const char *eev_cPERIOD       =  {"cPERIOD"};       // Период в циклах ЭРВ, сколько пропустить
const char *eev_cDELTA        =  {"cDELTA"};        // TDIS_TCON: Температура нагнетания - конденсации (сотые градуса)
const char *eev_cDELTAT       =  {"cDELTAT"};       // Порог, после превышения TDIS_TCON + TDIS_TCON_Thr начинаем менять перегрев
const char *eev_cDELTAC       =  {"cDELTAC"};		// Корректировка в + для TDIS_TCON на каждые 10 градусов выше 20.
const char *eev_cKF           =  {"cKF"};           // Коэффициент (/0.001): перегрев += дельта * K
const char *eev_cOH_MAX       =  {"cOH_MAX"};       // Максимальный перегрев (сотые градуса)
const char *eev_cOH_MIN       =  {"cOH_MIN"};       // Минимальный перегрев (сотые градуса)
const char *eev_cOH_START     =  {"cOH_START"};     // Стартовый перегрев (сотые градуса)
const char *eev_cOH_TDELTA    =  {"cTDELTA"};     	// Расчитанная целевая дельта Нагнетание-Конденсации
const char *eev_ERR_KP        =  {"ERR_KP"};        // Ошибка (в сотых градуса) при которой происходит уменьшение пропорциональной составляющей ПИД ЭРВ
const char *eev_SPEED         =  {"SPEED"};         // Скорость шагового двигателя ЭРВ (импульсы в сек.)
const char *eev_PRE_START_POS =  {"PRE_START_POS"}; // ПУСКОВАЯ позиция ЭРВ (ТО что при старте компрессора ПРИ РАСКРУТКЕ)
const char *eev_START_POS     =  {"START_POS"};     // СТАРТОВАЯ позиция ЭРВ после раскрутки компрессора т.е. ПОЗИЦИЯ С КОТОРОЙ НАЧИНАЕТСЯ РАБОТА проходит DelayStartPos сек
const char *eev_DELAY_ON_PID  =  {"DELAY_ON_PID"};  // Задержка включения EEV после включения компрессора (сек).  Точнее после выхода на рабочую позицию Общее время =delayOnPid+DelayStartPos
const char *eev_DELAY_START_POS= {"DELAY_START_POS"};// Время после старта компрессора когда EEV выходит на стартовую позицию - облегчение пуска вначале ЭРВ
const char *eev_DELAY_OFF     =  {"DELAY_OFF"};     // Задержка закрытия EEV после выключения насосов (сек). Время от команды стоп компрессора до закрытия ЭРВ = delayOffPump+delayOff
const char *eev_DELAY_ON      =  {"DELAY_ON"};      // Задержка между открытием (для старта) ЭРВ и включением компрессора, для выравнивания давлений (сек). Если ЭРВ закрывлось при остановке
const char *eev_HOLD_MOTOR    =  {"HOLD_MOTOR"};    // Флаг удержания мотора
const char *eev_PRESENT       =  {"PRESENT"};       // Флаг наличия ЭРВ в ТН

// Описание имен параметров MQTT для функций get_paramMQTT set_paramMQTT
const char *mqtt_USE_TS           =  {"USE_TS"};         // флаг использования ThingSpeak - формат передачи для клиента
const char *mqtt_USE_MQTT         =  {"USE_MQTT"};       // флаг использования MQTT
const char *mqtt_BIG_MQTT         =  {"BIG_MQTT"};       // флаг отправки ДОПОЛНИТЕЛЬНЫХ данных на MQTT
const char *mqtt_SDM_MQTT         =  {"SDM_MQTT"};       // флаг отправки данных электросчетчика на MQTT
const char *mqtt_FC_MQTT          =  {"FC_MQTT"};        // флаг отправки данных инвертора на MQTT
const char *mqtt_COP_MQTT         =  {"COP_MQTT"};       // флаг отправки данных COP на MQTT
const char *mqtt_TIME_MQTT        =  {"TIME_MQTT"};      // период отправки на сервер в сек. 10...60000
const char *mqtt_ADR_MQTT         =  {"ADR_MQTT"};       // Адрес сервера
const char *mqtt_IP_MQTT          =  {"IP_MQTT"};        // IP Адрес сервера
const char *mqtt_PORT_MQTT        =  {"PORT_MQTT"};      // Адрес порта сервера
const char *mqtt_LOGIN_MQTT       =  {"LOGIN_MQTT"};     // логин сервера
const char *mqtt_PASSWORD_MQTT    =  {"PASSWORD_MQTT"};  // пароль сервера
const char *mqtt_ID_MQTT          =  {"ID_MQTT"};        // Идентификатор клиента на MQTT сервере
   // народный мониторинг
const char *mqtt_USE_NARMON       =  {"USE_NARMON"};     // флаг отправки данных на народный мониторинг
const char *mqtt_BIG_NARMON       =  {"BIG_NARMON"};     // флаг отправки данных на народный мониторинг ,большую версию
const char *mqtt_ADR_NARMON       =  {"ADR_NARMON"};     // Адрес сервера народного мониторинга
const char *mqtt_IP_NARMON        =  {"IP_NARMON"};      // IP Адрес сервера народного мониторинга
const char *mqtt_PORT_NARMON      =  {"PORT_NARMON"};    // Адрес порта сервера  народного мониторинга
const char *mqtt_LOGIN_NARMON     =  {"LOGIN_NARMON"};   // логин сервера народного мониторинга
const char *mqtt_PASSWORD_NARMON  =  {"PASSWORD_NARMON"};// пароль сервера народного мониторинга
const char *mqtt_ID_NARMON        =  {"ID_NARMON"};      // Идентификатор клиента на MQTT сервере

// Описание имен параметров SDM счетчика для функций get_paramSDM set_paramSDM
const char *sdm_NAME        = {"NAME_SDM"};               // Имя счетчика
const char *sdm_NOTE        = {"NOTE_SDM"};               // Описание счетчика
const char *sdm_MAX_VOLTAGE = {"MAX_VOLTAGE_SDM"};        // Контроль напряжения максимум
const char *sdm_MIN_VOLTAGE = {"MIN_VOLTAGE_SDM"};        // Контроль напряжения минимум
const char *sdm_MAX_POWER   = {"MAX_POWER_SDM"};          // Контроль мощности максимум
const char *sdm_VOLTAGE     = {"VOLTAGE_SDM"};            // Напряжение
const char *sdm_CURRENT     = {"CURRENT_SDM"};            // Ток
const char *sdm_REPOWER     = {"REPOWER_SDM"};            // Реактивная мощность
const char *sdm_ACPOWER     = {"ACPOWER_SDM"};            // Активная мощность
const char *sdm_POWER       = {"POWER_SDM"};              // Полная мощность
const char *sdm_POW_FACTOR  = {"POW_FACTOR_SDM"};         // Коэффициент мощности
const char *sdm_PHASE       = {"PHASE_SDM"};              // Угол фазы (градусы)
const char *sdm_FREQ        = {"FREQ_SDM"};               // Частота
const char *sdm_ACENERGY    = {"ACENERGY_SDM"};           // Суммарная активная энергия
const char *sdm_LINK        = {"LINK_SDM"};               // Cостояние связи со счетчиком
const char *sdm_ERRORS  	= {"ERR"};                    // Ошибок чтения Modbus

// Описание имен параметров профиля для функций get_paramProfile set_paramProfile	
const char *prof_NAME_PROFILE   = {"NAME_PROFILE"};       // Имя профиля до 10 русских букв
const char *prof_ENABLE_PROFILE = {"ENABLE_PROFILE"};     // разрешение использовать в списке
const char *prof_ID_PROFILE     = {"ID_PROFILE"};         // номер профиля
const char *prof_NOTE_PROFILE   = {"NOTE_PROFILE"};       // описание профиля
const char *prof_DATE_PROFILE   = {"DATE_PROFILE"};       // дата профиля
const char *prof_CRC16_PROFILE  = {"CRC16_PROFILE"};      // контрольная сумма профиля
const char *prof_NUM_PROFILE    = {"NUM_PROFILE"};        // максимальное число профилей
const char *prof_SEL_PROFILE    = {"SEL_PROFILE"};        // список профилей (пока не используется)

// Описание имен параметров уведомлений для функций set_messageSetting get_messageSetting
const char *mess_MAIL         = {"MAIL"};                // флаг уведомления скидывать на почту
const char *mess_MAIL_AUTH    = {"MAIL_AUTH"};           // флаг необходимости авторизации на почтовом сервере
const char *mess_MAIL_INFO    = {"MAIL_INFO"};           // флаг необходимости добавления в письмо информации о состоянии ТН
const char *mess_SMS          = {"SMS"};                 // флаг уведомления скидывать на СМС (пока не реализовано)
const char *mess_MESS_RESET   = {"MESS_RESET"};          // флаг уведомления Сброс
const char *mess_MESS_ERROR   = {"MESS_ERROR"};          // флаг уведомления Ошибка
const char *mess_MESS_LIFE    = {"MESS_LIFE"};           // флаг уведомления Сигнал жизни
const char *mess_MESS_TEMP    = {"MESS_TEMP"};           // флаг уведомления Достижение граничной температуры
const char *mess_MESS_SD      = {"MESS_SD"};             // флаг уведомления "Проблемы с sd картой"
const char *mess_MESS_WARNING = {"MESS_WARNING"};        // флаг уведомления "Прочие уведомления"
const char *mess_SMTP_SERVER  = {"SMTP_SERVER"};         // Адрес сервера
const char *mess_SMTP_IP      = {"SMTP_IP"};             // IP Адрес сервера
const char *mess_SMTP_PORT    = {"SMTP_PORT"};           // Адрес порта сервера
const char *mess_SMTP_LOGIN   = {"SMTP_LOGIN"};          // логин сервера если включена авторизация
const char *mess_SMTP_PASS    = {"SMTP_PASS"};           // пароль сервера если включена авторизация
const char *mess_SMTP_MAILTO  = {"SMTP_MAILTO"};         // адрес отправителя
const char *mess_SMTP_RCPTTO  = {"SMTP_RCPTTO"};         // адрес получателя
const char *mess_SMS_SERVICE  = {"SMS_SERVICE"};         // сервис отправки смс
const char *mess_SMS_IP       = {"SMS_IP"};              // IP Адрес сервера для отправки смс
const char *mess_SMS_PHONE    = {"SMS_PHONE"};           // телефон куда отправляется смс
const char *mess_SMS_P1       = {"SMS_P1"};              // первый параметр для отправки смс
const char *mess_SMS_P2       = {"SMS_P2"};              // второй параметр для отправки смс
const char *mess_SMS_NAMEP1   = {"SMS_NAMEP1"};          // описание первого параметра для отправки смс
const char *mess_SMS_NAMEP2   = {"SMS_NAMEP2"};          // описание второго параметра для отправки смс
const char *mess_MESS_TIN     = {"MESS_TIN"};            // Критическая температура в доме (если меньше то генерится уведомление)
const char *mess_MESS_TBOILER = {"MESS_TBOILER"};        // Критическая температура бойлера (если меньше то генерится уведомление)
const char *mess_MESS_TCOMP   = {"MESS_TCOMP"};          // Критическая температура компрессора (если больше то генериться уведомление)
const char *mess_MAIL_RET     = {"MAIL_RET"};            // Ответ на тестовую почту
const char *mess_SMS_RET      = {"SMS_RET"};             // Ответ на тестовую  sms

// Описание имен параметров бойлера для функций set_Boiler get_Boiler
const char *boil_BOILER_ON    = {"BOILER_ON"};           // флаг Включения бойлера
const char *boil_SCHEDULER_ON = {"SCH_ON"};        		// флаг Использование расписания
const char *boil_SCHEDULER_ADDHEAT = {"SCH_AH"};        // флаг Использование расписания только для ТЭНа
const char *boil_TURBO_BOILER = {"TURBO_BOILER"};        // флаг ТУРБО ГВС нагрев (нагрев=ТН+ТЭН)
const char *boil_SALLMONELA   = {"SALLMONELA"};          // флаг Сальмонела раз в неделю греть бойлер
const char *boil_CIRCULATION  = {"CIRCULATION"};         // флаг Управления циркуляционным насосом ГВС
const char *boil_TEMP_TARGET  = {"TEMP_TARGET"};         // Целевая температура бойлера
const char *boil_DTARGET      = {"DTARGET"};             // гистерезис целевой температуры
const char *boil_TEMP_MAX     = {"TEMP_MAX"};            // Tемпература подачи максимальная
const char *boil_PAUSE1       = {"PAUSE1"};              // Минимальное время простоя компрессора в секундах
const char *boil_SCHEDULER    = {"SCHEDULER"};           // Расписание
const char *boil_CIRCUL_WORK  = {"CIRCUL_WORK"};         // Время  работы насоса ГВС секунды (fCirculation)
const char *boil_CIRCUL_PAUSE = {"CIRCUL_PAUSE"};        // Пауза в работе насоса ГВС  секунды (fCirculation)
const char *boil_RESET_HEAT   = {"RESET_HEAT"};          // флаг Сброса лишнего тепла в СО
const char *boil_RESET_TIME   = {"RESET_TIME"};          // время сброса излишков тепла в СО в секундах (fResetHeat)
const char *boil_BOIL_TIME    = {"BOIL_TIME"};           // Постоянная интегрирования времени в секундах ПИД ТН
const char *boil_BOIL_PRO     = {"BOIL_PRO"};            // Пропорциональная составляющая ПИД ГВС
const char *boil_BOIL_IN      = {"BOIL_IN"};             // Интегральная составляющая ПИД ГВС
const char *boil_BOIL_DIF     = {"BOIL_DIF"};            // Дифференциальная составляющая ПИД ГВС
const char *boil_BOIL_TEMP    = {"BOIL_TEMP"};           // Целевая температура ПИД ГВС
const char *boil_ADD_HEATING  = {"ADD_HEATING"};         // флаг ДОГРЕВА ГВС ТЭНом
const char *boil_TEMP_RBOILER = {"TEMP_RBOILER"};        // температура включения догрева бойлера

// Дата время
const char *time_TIME       = {"TIME"};         // текущее время  12:45 без секунд
const char *time_DATE       = {"DATE"};         // текушая дата типа  12/04/2016
const char *time_NTP        = {"NTP"};          // адрес NTP сервера строка до 60 символов.
const char *time_UPDATE     = {"UPDATE"};       // Время синхронизации с NTP сервером.
const char *time_TIMEZONE   = {"TIMEZONE"};     // Часовой пояс
const char *time_UPDATE_I2C = {"UPDATE_I2C"};   // Синхронизация времени раз в час с i2c часами

// Сеть
const char *net_IP         = {"IP"};               // Адрес 
const char *net_DNS        = {"DNS"};              // DNS
const char *net_GATEWAY    = {"GATEWAY"};          // Шлюз
const char *net_SUBNET     = {"SUBNET"};           // Маска подсети
const char *net_DHCP       = {"DHCP"};             // Флаг использования DHCP
const char *net_MAC        = {"MAC"};              // МАС адрес чипа
const char *net_RES_SOCKET = {"RES_SOCKET"};       // Время сброса зависших сокетов
const char *net_RES_W5200  = {"RES_W5200"};        // Время регулярного сброса сетевого чипа
const char *net_PASS       = {"PASS"};             // Использование паролей (флаг)
const char *net_PASSUSER   = {"PASSUSER"};         // Пароль пользователя 
const char *net_PASSADMIN  = {"PASSADMIN"};        // Пароль администратора  
const char *net_SIZE_PACKET= {"SIZE_PACKET"};      // размер пакета
const char *net_INIT_W5200 = {"INIT_W5200"};       // Ежеминутный контроль SPI для сетевого чипа
const char *net_PORT       = {"PORT"};             // Port веб сервера
const char *net_NO_ACK     = {"NO_ACK"};           // Не ожидать ответа ack
const char *net_DELAY_ACK  = {"DELAY_ACK"};        // Задержка перед посылкой следующего пакета
const char *net_PING_ADR   = {"PING_ADR"};         // адрес для пинга
const char *net_PING_TIME  = {"PING_TIME"};        // время пинга в секундах
const char *net_NO_PING    = {"NO_PING"};          // запрет пинга контроллера

// Статистика
const char *stat_NONE      = {"none"};
const char *stat_TIN       = {"Tin"};              // средняя темпеартура дома
const char *stat_TOUT      = {"Tout"};             // средняя темпеартура улицы
const char *stat_TBOILER   = {"Tboiler"};          // средняя температура бойлера
const char *stat_HOUR      = {"Hour"};             // число накопленных часов должно быть 24
const char *stat_HMOTO     = {"Hmoto"};            // моточасы за сутки
const char *stat_ENERGYCO  = {"EnergyCO"};         // выработанная энергия
const char *stat_ENERGY220 = {"Energy220"};        // потраченная энергия
const char *stat_COP       = {"COP"};              // КОП
const char *stat_POWERCO   = {"PowerCO"};          // средння мощность СО
const char *stat_POWER220  = {"Power220"};         // средняя потребляемая мощность

// Описание имен параметров Графиков для функций get_Chart
const char *chart_NONE      = {"NONE"};                    // 0 ничего не показываем
const char *chart_TOUT      = {"TOUT"};                     // 1 Температура улицы
const char *chart_TIN       = {"TIN"};                      // 2 Температура в доме
const char *chart_TEVAIN    = {"TEVAIN"};                   // 3 Температура на входе испарителя (по фреону)
const char *chart_TEVAOUT   = {"TEVAOUT"};                  // 4 Температура на выходе испарителя (по фреону)
const char *chart_TCONIN    = {"TCONIN"};                   // 5 Температура на входе конденсатора (по фреону)
const char *chart_TCONOUT   = {"TCONOUT"};                  // 6 Температура на выходе конденсатора (по фреону)
const char *chart_TBOILER   = {"TBOILER"};                  // 7 Температура в бойлере ГВС
const char *chart_TACCUM    = {"TACCUM"};                    // 8 Температура на выходе теплоаккмулятора
const char *chart_TRTOOUT   = {"TRTOOUT"};                   // 9 Температура на выходе RTO (по фреону)
const char *chart_TCOMP     = {"TCOMP"};                     // 10 Температура нагнетания компрессора
const char *chart_TEVAING   = {"TEVAING"};                   // 11 Температура на входе испарителя (по гликолю)
const char *chart_TEVAOUTG  = {"TEVAOUTG"};                  // 12 Температура на выходе испарителя (по гликолю)
const char *chart_TCONING   = {"TCONING"};                   // 13 Температура на входе конденсатора (по гликолю)
const char *chart_TCONOUTG  = {"TCONOUTG"};                  // 14 Температура на выходе конденсатора (по гликолю)
const char *chart_PEVA      = {"PEVA"};                      // 15 Давление
const char *chart_PCON      = {"PCON"};                      // 16 Давление нагнетания
const char *chart_FLOWCON   = {"FLOWCON"};                   // 17 Датчик потока по кондесатору
const char *chart_FLOWEVA   = {"FLOWEVA"};                   // 18 Датчик потока по испарителю
const char *chart_FLOWPCON  = {"FLOWPCON"};                  // 19 Датчик протока по предконденсатору
const char *chart_posEEV    = {"posEEV"};                    // 20 позиция ЭРВ
const char *chart_freqFC    = {"freqFC"};                    // 21 Частота инвертора
const char *chart_powerFC   = {"powerFC"};                   // 22 Мощность инвертора
const char *chart_currentFC = {"currentFC"};                 // 23 Ток компрессора
const char *chart_RCOMP     = {"RCOMP"};                     // 24 включение компрессора
const char *chart_OVERHEAT  = {"OVERHEAT"};                  // 25 перегрев
const char *chart_dCO       = {"dCO"};                       // 26 дельта СО
const char *chart_dGEO      = {"dGEO"};                      // 27 дельта геоконтура
const char *chart_TPEVA     = {"T[PEVA]"};                     // 28 температура расчитанная из давления Испариенифя
const char *chart_TPCON     = {"T[PCON]"};                     // 29 температура расчитанная из давления Конденсации
const char *chart_PowerCO   = {"PowerCO"};                   // 30 Мощность выходная теплового насоса
const char *chart_PowerGEO  = {"PowerGEO"};                  // 31 Мощность контура
const char *chart_COP       = {"COP"};                       // 32 Коэффициент преобразования Холодильной машины (без насосов)
const char *chart_VOLTAGE   = {"VOLTAGE"};                   // 33 Статистика по напряжению
const char *chart_CURRENT   = {"CURRENT"};                   // 34 Статистика по току
//const char *chart_acPOWER   = {"acPOWER"};                   // 34 Статистика по активная мощность
//const char *chart_rePOWER   = {"rePOWER"};                   // 36 Статистика по Реактивная мощность
const char *chart_fullPOWER = {"fullPOWER"};                 // 37 Статистика по Полная мощность
//const char *chart_kPOWER    = {"kPOWER"};                    // 38 Статистика по Коэффициент мощности
const char *chart_fullCOP   = {"fullCOP"};                   // 39 Полный COP

// Описание имен параметров инвертора  для функций get_paramFC set_paramFC
const char *fc_ON_OFF            = {"ON_OFF"};            // Флаг включения выключения (управление частотником)
const char *fc_INFO              = {"INFO"};              // Получить информацию из инвертора (таблица !!)
const char *fc_NAME              = {"NAME"};              // Имя инвертора 
const char *fc_NOTE              = {"NOTE"};              // Получение описания частотного преобразователя. Строка 80+1
const char *fc_PIN               = {"PIN"};               // Получение номера пина куда прицеплен analog FC
const char *fc_PRESENT           = {"PRESENT"};           // Наличие FC в конфигурации. 
const char *fc_STATE             = {"STATE"};             // Состояние ПЧ (чтение)
const char *fc_FC                = {"FC"};                // Целевая частота инвертора в 0.01 герцах
const char *fc_cFC               = {"cFC"};               // Текущая частота ПЧ (чтение)
const char *fc_cPOWER            = {"cPOWER"};            // Текущая мощность (чтение)
const char *fc_INFO1             = {"INFO1"};             // Первая строка под картинкой инвертора на схеме
const char *fc_cCURRENT          = {"cCURRENT"};          // Текущий ток (чтение)
const char *fc_AUTO              = {"AUTO"};              // Флаг автоматического подбора частоты
const char *fc_AUTO_RESET_FAULT  = {"ARSTFLT"};           // Флаг автоматического сброса не критичной ошибки инвертора
const char *fc_LogWork		       = {"LOGW"};              // Флаг логировать во время работы
const char *fc_ANALOG            = {"ANALOG"};            // Флаг аналогового управления
const char *fc_DAC               = {"DAC"};               // Получение текущего значения ЦАП
const char *fc_LEVEL0            = {"LEVEL0"};            // Уровень частоты 0 в отсчетах ЦАП
const char *fc_LEVEL100          = {"LEVEL100"};          // Уровень частоты 100% в  отсчетах ЦАП
const char *fc_LEVELOFF          = {"LEVELOFF"};          // Уровень частоты в % при отключении
const char *fc_BLOCK             = {"BLOCK"};             // флаг глобальная ошибка инвертора - работа инвертора запрещена блокировку можно сбросить установив в 0
const char *fc_ERROR             = {"ERROR"};             // Получить код ошибки
const char *fc_UPTIME            = {"UPTIME"};            // Время обновления алгоритма пид регулятора (мсек) Основной цикл управления
const char *fc_PID_STOP          = {"PID_STOP"};          // Проценты от уровня защит (мощность, ток, давление, температура) при которой происходит блокировка роста частоты пидом
const char *fc_DT_COMP_TEMP      = {"DT_COMP_TEMP"};      // Защита по температуре компрессора - сколько градусов не доходит до максимальной (TCOMP) и при этом происходит уменьшение частоты
const char fc_FREQ[]			 = "FREQ_";
	const char *fc_PID_FREQ_STEP     = {"PID_STEP"};      	// Максимальный шаг (на увеличение) изменения частоты при ПИД регулировании в 0.01 Гц Необходимо что бы ЭРВ успевал
	const char *fc_START_FREQ        = {"START"};       	// Стартовая частота инвертора (см компрессор) в 0.01 ГЦ
	const char *fc_START_FREQ_BOILER = {"START_BOILER"};	// Стартовая частота инвертора (см компрессор) в 0.01 ГЦ ГВС
	const char *fc_MIN_FREQ          = {"MIN"};         	// Минимальная  частота инвертора (см компрессор) в 0.01 Гц
	const char *fc_MIN_FREQ_COOL     = {"MIN_COOL"};    	// Минимальная  частота инвертора при охлаждении в 0.01 Гц
	const char *fc_MIN_FREQ_BOILER   = {"MIN_BOILER"};  	// Минимальная  частота инвертора при нагреве ГВС в 0.01 Гц
	const char *fc_MIN_FREQ_USER     = {"MIN_USER"};    	// Минимальная  частота инвертора РУЧНОЙ РЕЖИМ (см компрессор) в 0.01 Гц
	const char *fc_MAX_FREQ          = {"MAX"};         	// Максимальная частота инвертора (см компрессор) в 0.01 Гц
	const char *fc_MAX_FREQ_COOL     = {"MAX_COOL"};    	// Максимальная частота инвертора в режиме охлаждения  в 0.01 Гц
	const char *fc_MAX_FREQ_BOILER   = {"MAX_BOILER"};  	// Максимальная частота инвертора в режиме ГВС в 0.01 Гц поглощение бойлера обычно меньше чем СО
	const char *fc_MAX_FREQ_USER     = {"MAX_USER"};   	 	// Максимальная частота инвертора РУЧНОЙ РЕЖИМ (см компрессор) в 0.01 Гц
	const char *fc_STEP_FREQ         = {"STEP"};        	// Шаг уменьшения инвертора при достижении максимальной температуры, мощности и тока (см компрессор) в 0.01 Гц
	const char *fc_STEP_FREQ_BOILER  = {"STEP_BOILER"}; 	// Шаг уменьшения инвертора при достижении максимальной температуры, мощности и тока ГВС в 0.01 Гц
const char *fc_DT_TEMP           = {"DT_TEMP"};           // Превышение температуры от уставок (подача) при которой срабатыват защита (уменьшается частота) в сотых градуса
const char *fc_DT_TEMP_BOILER    = {"DT_TEMP_BOILER"};    // Превышение температуры от уставок (подача) при которой срабатыват защита ГВС в сотых градуса
const char *fc_MB_ERR		     = {"MB_ERR"};			  // Ошибок Modbus

// Описание имен параметров опций ТН  для функций get_optionHP set_optionHP
const char *option_ADD_HEAT           = {"ADD_HEAT"};           // использование дополнительного нагревателя (значения 1 и 0)
const char *option_TEMP_RHEAT         = {"TEMP_RHEAT"};         // температура для управления RHEAT (градусы)
const char *option_PUMP_WORK          = {"PUMP_WORK"};          // работа насоса конденсатора при выключенном компрессоре секунды
const char *option_PUMP_PAUSE         = {"PUMP_PAUSE"};         // пауза между работой насоса конденсатора при выключенном компрессоре (секунды)
const char *option_ATTEMPT            = {"ATTEMPT"};            // число попыток пуска
const char *option_TIME_CHART         = {"TIME_CHART"};         // период сбора статистики
const char *option_BEEP               = {"BEEP"};               // включение звука
const char *option_NEXTION            = {"NEXTION"};            // использование дисплея nextion
const char *option_EEV_CLOSE          = {"EEV_CLOSE"};          // закрытие ЭРВ при выключении компрессора
const char *option_EEV_LIGHT_START    = {"EEV_LIGHT_START"};    // флаг Облегчение старта компрессора   приоткрытие ЭРВ в момент пуска компрессора
const char *option_EEV_START_POS      = {"EEV_START"};          // флаг Всегда начинать работу ЭРВ со стратовой позици
const char *option_SD_CARD            = {"SD_CARD"};            // запись статистики на карточку
const char *option_SDM_LOG_ERR        = {"SDM_LOGER"};          // флаг писать в лог нерегулярные ошибки счетчика SDM
const char *option_SAVE_ON            = {"SAVE_ON"};            // флаг записи в EEPROM включения ТН (восстановление работы после перезагрузки)
const char *option_NEXT_SLEEP         = {"NEXT_SLEEP"};         // Время засыпания секунды NEXTION
const char *option_NEXT_DIM           = {"NEXT_DIM"};           // Якрость % NEXTION
const char option_SGL1W[]             = "SGL1W_";			    // SGLOW_n, На шине n (1-Wire, DS2482) только один датчик
const char *option_DELAY_ON_PUMP      = {"DELAY_ON_PUMP"};      // Задержка включения компрессора после включения насосов (сек).
const char *option_DELAY_OFF_PUMP     = {"DELAY_OFF_PUMP"};     // Задержка выключения насосов после выключения компрессора (сек).
const char *option_DELAY_START_RES    = {"DELAY_START_RES"};    // Задержка включения ТН после внезапного сброса контроллера (сек.)
const char *option_DELAY_REPEAD_START = {"DELAY_REPEAD_START"}; // Задержка перед повторным включениме ТН при ошибке (попытки пуска) секунды
const char *option_DELAY_DEFROST_ON   = {"DELAY_DEFROST_ON"};   // ДЛЯ ВОЗДУШНОГО ТН Задержка после срабатывания датчика перед включением разморозки (секунды)
const char *option_DELAY_DEFROST_OFF  = {"DELAY_DEFROST_OFF"};  // ДЛЯ ВОЗДУШНОГО ТН Задержка перед выключением разморозки (секунды)
const char *option_DELAY_TRV          = {"DELAY_TRV"};          // Задержка между переключением 4-х ходового клапана и включением компрессора, для выравнивания давлений (сек). Если включены эти опции (переключение тепло-холод)
const char *option_DELAY_BOILER_SW    = {"DELAY_BOILER_SW"};    // Пауза (сек) после переключение ГВС - выравниваем температуру в контуре отопления/ГВС что бы сразу защиты не сработали
const char *option_DELAY_BOILER_OFF   = {"DELAY_BOILER_OFF"};   // Время (сек) на сколько блокируются защиты при переходе с ГВС на отопление и охлаждение слишком горяче после ГВС

// Отопление/охлаждение параметры
const char *hp_RULE      = {"RULE"};             // алгоритм работы
const char *hp_TEMP1     = {"TEMP1"};            // целевая температура в доме
const char *hp_TEMP2     = {"TEMP2"};            // целевая температура обратки
const char *hp_TARGET    = {"TARGET"};           // что является целью ПИД - значения  0 (температура в доме), 1 (температура обратки).
const char *hp_DTEMP     = {"DTEMP"};            // гистерезис целевой температуры
const char *hp_HP_TIME   = {"HP_TIME"};          // Постоянная интегрирования времени в секундах ПИД ТН
const char *hp_HP_PRO    = {"HP_PRO"};           // Пропорциональная составляющая ПИД ТН
const char *hp_HP_IN     = {"HP_IN"};            // Интегральная составляющая ПИД ТН
const char *hp_HP_DIF    = {"HP_DIF"};           // Дифференциальная составляющая ПИД ТН
const char *hp_TEMP_IN   = {"TEMP_IN"};          // температура подачи (минимальная для охлажления или максимальная для нагрева)
const char *hp_TEMP_OUT  = {"TEMP_OUT"};         // температура обратки (максимальная для охлажления или минимальная для нагрева)
const char *hp_PAUSE     = {"PAUSE"};            // минимальное время простоя компрессора
const char *hp_D_TEMP    = {"D_TEMP"};           // максимальная разность температур конденсатора.
const char *hp_TEMP_PID  = {"TEMP_PID"};         // Целевая температура ПИД
const char *hp_WEATHER   = {"WEATHER"};          // Использование погодозависимости
const char *hp_HEAT_FLOOR = {"HFL"};          	 // Использование теплого пола
const char *hp_SUN 		 = {"SUN"};          	 // Использование солнечного коллектора
const char *hp_K_WEATHER = {"K_WEATHER"};        // Коэффициент погодозависимости

const char *ADD_DELTA_TEMP     = {"ADT"};		// Добавка температуры к установке, в градусах
const char *ADD_DELTA_HOUR     = {"ADH"};		// Начальный Час добавки температуры к установке
const char *ADD_DELTA_END_HOUR = {"ADEH"};		// Конечный Час добавки температуры к установке


#ifdef SENSOR_IP // параметры удаленного датчика get_sensorIP
const char *ip_SENSOR_TEMP     = {"SENSOR_TEMP"};   // Темпеартура
const char *ip_SENSOR_NUMBER   = {"SENSOR_NUMBER"}; // Номер
const char *ip_RSSI            = {"RSSI"};          // Уровень сигнала
const char *ip_VCC             = {"VCC"};           // Напряжение питания
const char *ip_SENSOR_USE      = {"SENSOR_USE"};    // Использование
const char *ip_SENSOR_RULE     = {"SENSOR_RULE"};   // Правило использования
const char *ip_SENSOR_IP       = {"SENSOR_IP"};     // Адрес
const char *ip_SENSOR_COUNT    = {"SENSOR_COUNT"};  // Счетчик
const char *ip_STIME           = {"STIME"};         // время с последнего считывания датчика
const char *ip_SENSOR          = {"SENSOR"};        // -------
#endif


// Названия типы фреонов
const char *noteFreon[]    =   {"R22","R410A","R600","R134A","R407C","R12","R290","R404A","R717"};
// Названия правило работы ЭРВ
const char *noteRuleEEV[]   =  {"TEVAOUT-TEVAIN","TRTOOUT-TEVAIN","TEVAOUT-T[PEVA]","TRTOOUT-T[PEVA]","Table[EVA CON]","Manual"};
// Описание правила работы ЭРВ
const char *noteRemarkEEV[] = {"Перегрев равен температуре на выходе испарителя - температура на входе испарителя. Есть возможность введения поправки (добавляется).",
                               "Перегрев равен температуре на выходе РТО - температура на входе испарителя. Есть возможность введения поправки (добавляется).",
                               "Перегрев равен температура на выходе испарителя - температура пересчитанной из давления на выходе испарителя. Есть выбор фреона и поправка (добавляется).",
                               "Перегрев равен температура на выходе РТО - температура пересчитанной из давления на выходе испарителя. Есть выбор фреона и поправка (добавляется).",
                               "Перегрев не вычисляется. ЭРВ открывается по значению из таблицы, которая увязывает температуры испарителя и конденсатора с шагами открытия ЭРВ.",
                               "Перегрев не вычисляется. Ручной режим, ЭРВ открывается на заданное число шагов."};


// Описание ВСЕХ Ошибок длина описания не более 160 байт (ограничение основной класс note_error[160+1])
const char *noteError[] = {"Ok",                                                  //  0
                           "Выход за нижнюю границу температурного датчика",      // -1
                           "Выход за верхнюю границу температурного датчика",     // -2
                           "Выход за нижнюю границу  датчика давления",           // -3
                           "Выход за верхнюю границу датчика давления",           // -4
                           "Датчик запрещен в текущей конфигурации",              // -5
                           "Адрес датчика температруры не установлен",            // -6
                           "Срабатывания контактного датчика - авария",           // -7
                           "ЭРВ выход за диапазон (по шагам) вверх",              // -8
                           "ЭРВ выход за диапазон (по шагам) вниз",               // -9
                           "Устройство запрещено в текущей конфигурации",         // -10
                           "Ошибка сброса на OneWire шине (обрыв или замыкание)", // -11
                           "ЭРВ получило отрицательный перегрев",                 // -12
                           "Free RTOS не может создать задачу, не хватает пямяти",// -13
                           "Отсутствует датчик давления, и выбран алгоритм ЭРВ который его использует",// -14 Длина 132 байта
                           "Ошибка записи настроек", 			        		  // -15
                           "Ошибка чтения настроек",					          // -16
                           "Ошибка контрольной суммы для настроек в I2C",         // -17
                           "Не совпадение размера данных при чтении настроек",    // -18
                           "Данные настроек не найдены в I2C",			          // -19
                           "Ошибка записи состояния в I2C",			              // -20
                           "Ошибка чтения состояния в I2C",			               // -21
                           "Данные состояния не найдены в I2C",			           // -22
                           "Ошибка записи счетчиков в I2C",                			// -23
                           "Ошибка чтения счетчиков из I2C", 		              // -24
                           "Не совпадение версий данных в I2C",     		       // -25
                           "Привышена разность температур на кондесаторе",        // -26
                           "Привышена разность температур на испарителе",         // -27
                           "Отсутствует насос на конденсаторе, проверьте конфигурацию",  // -28
                           "Отсутствует насос на испарителе, проверьте конфигурацию",    // -29
                           "Ошибка чтения датчика давления (данные не готовы)",          // -30
                           "Отсутствует компрессор, проверьте конфигурацию (RCOMP и FC)",// -31
                           "Нет работы для ТН, проверьте настройки ГВС и отопления",     // -32
                           "Попытка включить компрессор при ошибке (обратитесь к разработчику)", // -33
                           "Сбой внутренней конфигурации (обратитесь к разработчику)",           // -34
                           "Ошибка инициализации SD карты",                                      // -35
                           "Файл index.xxx не найден на SD карте",                               // -36
                           "Ошибка чтения файла с SD карты",                                     // -37
                           "Правило вычисления перегрева не соответствует датчикам (обратитесь к разработчику)",//-38
                           "Инвертор на шине Modbus не найден (работа инвертора запрещена)",                    //-39
                           "При обращении к 485 порту привышено время ожидания его освобождения",               //-40
                           "Modbus error 0x01 protocol illegal function exception",                             //-41
                           "Modbus error 0x02 protocol illegal data address exception",                         //-42
                           "Modbus error 0x03 protocol illegal data value exception",                           //-43
                           "Modbus error 0х04 protocol slave device failure exception",                         //-44
                           "Modbus error 0xe0 Master invalid response slave ID exception",                      //-45
                           "Modbus error 0xe1 Master invalid response function exception",                      //-46
                           "Modbus error 0xe2 Master response timed out exception",                             //-47
                           "Modbus error 0xe3 Master invalid response CRC exception",                           //-48
                           #ifdef FC_VACON // Спицифические ошибки Vocon 10
                              "Ведомое устройство приняло запрос и обрабатывает его (0x05)",                    //-49
                              "Ведомое устройство занято обработкой команды (0x06)",                            //-50
                              "Ведомое устройство не может выполнить программную функцию (0x07)",               //-51
                              "Ведомое устройство обнаружило ошибку контроля четности (0x08)",                  //-52
                              "",                                                                               //-53
                              "",                                                                               //-54
                              "",                                                                               //-55
                            #else  // Спицифические ошибки OMRON
                           "Код исключения 0x01 Указанная функция не поддерживается",                           //-49
                           "Код исключения 0x02 Указанная функция не обнаружена",                               //-50
                           "Код исключения 0x03 Неприемлемый формат указанных данных",                          //-51
                           "Omron mx2 0х05 ошибка связи по Modbus (функция проверка связи 0x08)",               //-52
                           "Код исключения 0x21 Данные, записываемые в регистр, находятся за пределами ПЧ",     //-53
                           "Код исключения 0x22 Указанные функции не доступны для ПЧ",                          //-54
                           "Код исключения 0x23 Регистр (бит), доступен только для чтения",                     //-55
                           #endif
                           "Modbus не известная ошибка (сбой протокола)",                                       //-56
                           "Запрещенное (не верное) состояние инвертора",                                       //-57
                           "Попытка включения ТН при заблокированном инверторе",                                //-58
                           "Алгоритм ПИД - достижение максимальной температуры подачи (защита)",                //-59
                           "Не хватает памяти для выделения под данные (см. конфигурацию).",                    //-60
                           "Ошибка записи профиля в I2C",                                       		        //-61
                           "Ошибка чтения профиля из I2C",                                              		//-62
                           "Ошибка контрольной суммы для профиля",                                              //-63
                           "Не совпадение размера данных при чтении профиля",                                   //-64
                           "Мастер DS2482 не найден на шине, возможно ошибка шины I2C",                         //-65
                           "Мастер DS2482 не может сбросить шину OneWire бит PPD равен 0",                      //-66
                           "При обращении к I2C шины привышено время ожидания ее освобождения",                 //-67
                           "Отказ драйвера L9333 ЭРВ (сработала защита драйвера)",                              //-68
                           "Ошибка заголовка счетчиков в I2C",                          		                //-69
                           "Ошибка открытия журнала в I2C (инициализация чипа)", 		                        //-70
                           "Ошибка чтения журнала из I2C памяти",                                               //-71
                           "Ошибка записи журнала в I2C память",                                                //-72
                           "Указано FNUMBER (количество частотных датчиков) более трех штук",                   //-73
                           "Поток в ПТО ниже установленного уровня (проблема - насос, фильтр)",                 //-74
                           "Слишком большое напряжение сети",                                                   //-75
                           "Слишком большая портебляемая мощность",                                             //-76
                           "Modbus требуется но отсутвует в конфигурации",                                      //-77
                           "Не удалось сбросить инвертор Omron mx2 после ошибки",                               //-78
                           "Отсутствует проток в испарителе (срабатывание SEVA)",                               //-79
                           "Ошибка открытия статистики в  I2C памяти (инициализация чипа)",                     //-80
                           "Ошибка чтения статистики из I2C памяти",                                            //-81
                           "Ошибка записи статистики в I2C память",                                             //-82
                           "Ошибка использования аналогового управления инвертором без выхода ХОД",             //-83
                           "Ошибка чтения температурного датчика (лимит попыток чтения исчерпан)",              //-84
                           "Ошибка конфигурации температурного датчика (привязка к отсутвующей шине OneWire)",  //-85
						   "Ошибка CRC чтения OneWire",															//-86
						   "Ошибка чтения/записи OneWire",														//-87
						   "Сбой инвертора",																	//-88
						   "Ошибка программы управления инвертором",											//-89

                           "NULL"
                           };
// --------------------------------- ПЕРЕЧИСЛЯЕМЫЕ ТИПЫ ---------------------------------------------

//  Перечисляемый тип - Состояния теплового насоса
enum TYPE_STATE_HP         
{
  pOFF_HP,                          // 0 ТН выключен
  pSTARTING_HP,                     // 1 Стартует
  pSTOPING_HP,                      // 2 Останавливается
  pWORK_HP,                         // 3 Работает
  pWAIT_HP,                         // 4 Ожидание ТН (расписание - пустое место)
  pERROR_HP,                        // 5 Ошибка ТН
  pERROR_CODE,                      // 6 - Эта ошибка возникать не должна!
  pEND15                            // Обязательно должен быть последним, добавляем ПЕРЕД!!!
};

// Перечисляемый тип - точка возврата из алгоритма управления (расшифровка состояния)
// Расшифровка кода (пример Bp12  Бойлер - Алгоритм ПИД - изменение частоты ПИДом)
// Код имеет три поля <источник><алгоритм><код_алгоритма>
// Источник: B-бойлер H-отопление C-охлаждение
// Алгоритм: h - гистрерзис p - ПИД
// Код алгоритма
// 1 - выключение по подаче
// 2 - включение по гистерезису
// 3 - выключение по гистерезису
// 4 - внутри гистерезиса (ПРОДОЛЖЕНИЕ! нагрев или охлаждение)
// 5 - внутри гистерезиса пауза
// 6 - сброс частоты по подаче
// 7 - сброс частоты по мощности
// 8 - сброс частоты по температуре компрессора
// 9 - сброс частоты по давлению
// 10 - разгон, пид не работает
// 11 - время пида не пришло
// 12 - дошли до ПИДа, регулируем
// 13 - включение по обратке достигнута минимальная температура обратки
// 14 - работа супербойлера ПИД ГВС (заход в бойлер)
// 15 - Бойлер греется от предкондесатора (заход в отопление)
// 16 - сброс частоты по току инвертора
// 17 - блокировка роста частоты ПИДом при подходе к уровням защиты ПОДАЧА
// 18 - блокировка роста частоты ПИДом при подходе к уровням защиты МОЩНОСТЬ
// 19 - блокировка роста частоты ПИДом при подходе к уровням защиты ТОК
// 20 - блокировка роста частоты ПИДом при подходе к уровням защиты ТCOMP
// 21 - блокировка роста частоты ПИДом при подходе к уровням защиты ДАВЛЕНИЮ
// 22 - Выключение нагрева бойлера ТН для перехода в режим ДОГРЕВА его ТЭНом
// 23 - Выключение режима ТН при достижении уровня защиты по подаче (достижение границы)
// 24 - Выключение режима ТН при достижении уровня защиты по мощности (достижение границы)
// 25 - Выключение режима ТН при достижении уровня защиты по температуре компрессора (достижение границы)
// 26 - Выключение режима ТН при достижении уровня защиты по давлению (достижение границы)
// 27 - Выключение режима ТН при достижении уровня защиты по току (достижение границы)

enum TYPE_RET_HP
{
  pNone,              // В начале
  pMinPauseOn,        // Обеспечение минимальной паузы между включениями
  // Бойлер
  pBh1,
  pBh2,
  pBh3,
  pBh4,
  pBh5,
  pBh22,
  
  pBp3,
  pBp1,
  pBp2,
  pBp6,
  pBp7,
  pBp8,
  pBp9,
  pBp5,
  pBp10,
  pBp11,
  pBp12,
  pBp14,
  pBp16,
  
  pBp17,  // ограничения пида
  pBp18,
  pBp19,
  pBp20,
  pBp21,
  pBp22,
  pBp23,
  pBp24,
  pBp25,
  pBp26,
  pBp27,  
 
  // Отопление
  pHh3,
  pHh1,
  pHh2,
  pHh13,
  pHh4,

  pHp3,
  pHp1,
  pHp2,
  pHp6,
  pHp7,
  pHp8,
  pHp9,
  pHp5,
  pHp10,
  pHp11,
  pHp12,
  pHp15,
  pHp16,

  pHp17,  // ограничения пида
  pHp18,
  pHp19,
  pHp20,
  pHp21,
  pHp23,
  pHp24,
  pHp25,
  pHp26,
  pHp27,   

  // Охлаждение
  pCh3,
  pCh1,
  pCh2,
  pCh13,
  pCh4,

  pCp3,
  pCp1,
  pCp2,
  pCp6,
  pCp7,
  pCp8,
  pCp9,
  pCp5,
  pCp10,
  pCp11,
  pCp12,
  pCp15, 
  pCp16,

  pCp17,  // ограничения пида
  pCp18,
  pCp19,
  pCp20,
  pCp21,
  pCp23,
  pCp24,
  pCp25,
  pCp26,
  pCp27,   
  
  pEND18                            // Обязательно должен быть последним, добавляем ПЕРЕД!!!
};
//  Для вывода кодов
const char *codeRet[]={ "none","MinPause","Bh1","Bh2","Bh3","Bh4","Bh5","Bh22","Bp3","Bp1","Bp2","Bp6","Bp7","Bp8","Bp9","Bp5","Bp10","Bp11","Bp12","Bp14","Bp16","Bp17","Bp18","Bp19","Bp20","Bp21","Bp22", "Bp23","Bp24","Bp25","Bp26","Bp27",\
                       "Hh3","Hh1","Hh2","Hh13","Hh4","Hp3","Hp1","Hp2","Hp6","Hp7","Hp8","Hp9","Hp5","Hp10","Hp11","Hp12","Hp15","Hp16","Hp17","Hp18","Hp19","Hp20","Hp21","Hp23","Hp24","Hp25","Hp26","Hp27",\
                       "Ch3","Ch1","Ch2","Ch13","Ch4","Cp3","Cp1","Cp2","Cp6","Cp7","Cp8","Cp9","Cp5","Cp10","Cp11","Cp12","Cp15","Cp16","Cp17","Cp18","Cp19","Cp20","Cp21","Cp23","Cp24","Cp25","Cp26","Cp27","null"};           

//  Перечисляемый тип - действия над компрессором
enum MODE_COMP          
{
  pCOMP_OFF,                     // компрессор был выключен
  pCOMP_ON,                      // компрессор был включен
  pCOMP_NONE,                    // компрессор ничего не делаем - не меняем стутус
  pEND9                          // Обязательно должен быть последним, добавляем ПЕРЕД!!!
};

//  Перечисляемый тип - что надо делать или какой режим отопления выбран (первые три)
enum MODE_HP          
{
  pOFF,                          // 0 Выключить
  pHEAT,                         // 1 Включить отопление
  pCOOL,                         // 2 Включить охлаждение
  pBOILER,                       // 3 Включить бойлер
  pNONE_H,                       // 4 Продолжаем греть отопление
  pNONE_C,                       // 5 Продолжаем охлаждение
  pNONE_B,                       // 6 Продолжаем греть бойлер
  pEND8                          // Обязательно должен быть последним, добавляем ПЕРЕД!!!
};

const char *MODE_HP_STR[] = { "Off", "Heat", "Cool", "Boil", "HeatC", "CoolC", "BoilC" };

//  Перечисляемый тип - Команды управления ТН
enum TYPE_COMMAND         
{
  pEMPTY,                        // 0 Команд нет
  pSTART,                        // 1 Пуск теплового насоса (РУКАМИ)
  pAUTOSTART,                    // 2 Пуск теплового насоса (автоматический pRESTART pREPEAT)
  pSTOP,                         // 3 Стоп теплового насоса
  pRESET,                        // 4 Сброс контроллера
  pRESTART,                      // 5 Пуск ТН после сброса контроллера (отличается от pREPEAT только задержкой)
  pREPEAT,                       // 6 Повторный пуск ТН (определяется числом попыток повторного пуска)
  pNETWORK,                      // 7 перегрузить сетевой контроллер
  pJFORMAT,                      // 8 форматирование журнала в I2C флеше
  pSFORMAT,                      // 9 форматирование статистики в I2C флеше
  pSAVE,                         // 10 запись настроек ТН
  pWAIT,                         // 11 перевод в режим ожидания ТН (пустое расписание)
  pRESUME,                       // 12 Восстановление работы из режима ожидания
  pEND14                         // Обязательно должен быть последним, добавляем ПЕРЕД!!!
};

const char *hp_commands_names[] = { "EMPTY", "START", "AUTOSTART", "STOP", "RESET", "RESTART", "REPEAT", "NETWORK",
		"JFORMAT", "SFORMAT", "SAVE", "WAIT", "RESUME",  "UNKNOWN"};

//  Перечисляемый тип -ТИПЫ уведомлений
enum MESSAGE          
{
  pMESSAGE_NONE,                 // 0 Нет уведомлений
  pMESSAGE_TESTMAIL,             // 1 Тестовое уведомление по почте +
  pMESSAGE_TESTSMS,              // 2 Тестовое уведомление по SMS +
  pMESSAGE_RESET,                // 3 Уведомление Сброс +
  pMESSAGE_ERROR,                // 4 Уведомление Ошибка +
  pMESSAGE_LIFE,                 // 5 Уведомление Сигнал жизни +
  pMESSAGE_TEMP,                 // 6 Уведомление Достижение граничной температуры +
  pMESSAGE_SD,                   // 7 Уведомление "Проблемы с sd картой" +
  pMESSAGE_WARNING,              // 8 Уведомление "Прочие уведомления"
  pEND10                         // Обязательно должен быть последним, добавляем ПЕРЕД!!!
};

//  Перечисляемый тип - сервис для отправки смс
enum SMS_SERVICE 
{ 
  pSMS_RU,                       // Сервис sms.ru
  pSMSC_RU,                      // Сервис smsc.ru
  pEND12
};

//  Перечисляемый тип - тип фреона
enum TYPEFREON           
{
    R22,        
    R410A,
    R600A,
    R134A,
    R407C,
    R12,
    R290,
    R404A,
    R717            // Обязательно должен быть последним, добавляем ПЕРЕД!!!
};

//  Перечисляемый тип - правило работы ЭРВ пять вариантов выводятся в зависимости от наличия датчиков
enum RULE_EEV           
{
   TEVAOUT_TEVAIN, 
   TRTOOUT_TEVAIN, 
   TEVAOUT_PEVA,
   TRTOOUT_PEVA,
   TABLE,
   MANUAL           // Обязательно должен быть последним, добавляем ПЕРЕД!!!
};

//  Перечисляемый тип - режим тестирования ТН
enum TEST_MODE          
{
   NORMAL=0,        
   SAFE_TEST,
   TEST,
   HARD_TEST           // Обязательно должен быть последним, добавляем ПЕРЕД!!!
};

//  Перечисляемый тип - Время сброса сокетов
enum TIME_RES_SOCKET       
{
    pNONE1,           // нет сброса
    p30sec,           // сброс раз в 30 секунд
    p300sec,          // сброс раз в 300 секунд
    pEND4             // Обязательно должен быть последним, добавляем ПЕРЕД!!!
};

//  Перечисляемый тип - алгоритмы отопления
enum RULE_HP       
{
    pHYSTERESIS,      // алгоритм гистерезис, интервальный режим
    pPID,             // алгоритм использование ПИД регулятора
    pHYBRID,          // алгоритм смешаный алгоритм, предложил  Ljutik
    pEND1             // Обязательно должен быть последним, добавляем ПЕРЕД!!!
};

 #endif


