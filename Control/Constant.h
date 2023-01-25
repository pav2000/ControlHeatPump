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
// --------------------------------------------------------------------------------
// Константы проекта которые описывают специфику КОНФИГУРАЦИИ ТН
// --------------------------------------------------------------------------------
#ifndef Constant_h
#define Constant_h
#include "Config.h"                         // Цепляем сразу конфигурацию
#include "Util.h"

// ОПЦИИ КОМПИЛЯЦИИ ПРОЕКТА -------------------------------------------------------
#define VERSION			"1.099"				// Версия прошивки
#define VER_SAVE		147					// Версия формата сохраняемых данных в I2C память
#ifndef UART_SPEED
#define UART_SPEED		115200				// Скорость отладочного порта
#endif
#ifdef DEBUG_NATIVE_USB                    // Куда выводить отладку
#define SerialDbg	SerialUSB
#else
#define SerialDbg	Serial
#endif
//#define LOG                               // В последовательный порт шлет лог веб сервера (логируются запросы)
#define FAST_LIB                            // использование допиленной библиотеки езернета Обычно используется
#define TIME_ZONE         3                 // поправка на часовой пояс по ДЕФОЛТУ
#define NTP_SERVER        "time.nist.gov"   // NTP сервер для синхронизации времени по ДЕФОЛТУ (готовимся к отключению буржуинского инета - "ntp2.stratum2.ru")
#define NTP_SERVER_LEN    60                // максимальная длиная адреса NTP сервера
#define NTP_PORT		  123
#define NTP_LOCAL_PORT    8888              // локальный порт, который будет прослушиваться на предмет UDP-пакетов NTP сервера
#define NTP_REPEAT        3                 // Число попыток запросов NTP сервера
#define NTP_REPEAT_TIME   1000              // (мсек) Время между повторами ntp пакетов
#define PING_SERVER       "192.168.0.1"     // ping сервер по ДЕФОЛТУ
#define WDT_TIME          10                // период Watchdog таймера секунды но не более 16 секунд!!! ЕСЛИ установить 0 то Watchdog будет отключен!!!
#ifndef INDEX_FILE
#define INDEX_FILE        "index.html"       // стартовый файл по умолчанию для большой морды
#endif
#define INDEX_MOB_FILE    INDEX_FILE         // стартовый файл по умолчанию для мобильной морды
#define MOB_PATH          "/mob/"            // Путь к мобильной морде
const char HEADER_BIN[] = "HP-SAVE-DATA";   // Заголовок (начало) файла при сохранении настроек. Необходим для поиска данных в буфере данных при восстановлении из файла
#define MAX_LEN_PM        250               // максимальная длина строкового параметра в запросе (расписание бойлера 175 байт) кодирование описания профиля 40 букв одна буква 6 байт (двойное кодирование)
#ifndef CHART_POINT                         // Можно определить себе спецальный размер графика в своем конфиге 
#define CHART_POINT       300               // Максимальное число точек графика, одна точка это 2 байта * число графиков  //300 - работает
#endif
#ifndef ADC_PRESCAL
#define ADC_PRESCAL       2					// ADCClock = MCK / ( (PRESCAL+1) * 2 )
#endif

// СЕТЕВЫЕ НАСТРОЙКИ --------------------------------------------------------------
// По умолчанию и в демо режиме (действуют там и там)
// В рабочем режиме настройки берутся из ЕЕПРОМ, если прочитать не удалось, то действуют настройки по умолчанию
byte defaultMAC[] = { 0xDE, 0xA1, 0x1E, 0x01, 0x02, 0x03 };// не менять
const uint16_t  defaultPort=80;

// Макросы работы с битами байта, используется для флагов
#define GETBIT(b,f)   ((b&(1<<(f)))?true:false)              // получить состяние бита
#define SETBIT1(b,f)  (b|=(1<<(f)))                          // установка бита в 1
#define SETBIT0(b,f)  (b&=~(1<<(f)))                         // установка бита в 0

// ------------------- SPI ----------------------------------
// Карта памяти
#define SD_REPEAT         3                 // Число попыток чтения карты, открытия файлов, при неудаче переход на работу без карты

// чип W5200 (точнее любой используемый чип)
#ifndef W5200_THREAD
#define W5200_THREAD      3                 // Число потоков для сетевого чипа w5200 допустимо 1-4 потока, на 4 скорее всего не хватить места в оперативке
#endif
#define W5200_NUM_PING    4                 // Число попыток пинга до определения потери связи
#define W5200_TIME_PING   1000              // мсек Время между попытками пинга (если не удача)
#define W5200_MAX_LEN     2048 //=W5100.SSIZE // Максимальная длина буфера, определяется W5200 не более 2048 байт
#define W5200_NUM_LINK    2                 // Число попыток сброса чипа w5500 и проверки появления связи (кабель воткнут) используется для инициализаци чипа
#define W5200_TIME_LINK   4000              // Максимальное время ожидания устанoвления связи (поднятие Link) кабель воткнут  используется для инициализаци чипа (мсек)
#define W5200_TIME_WAIT   3000              // Время ожидания захвата мютекса (переключение потоков) мсек
//#define W5200_STACK_SIZE  230             переименована и переехало в конфиг  // Размер стека (слова!!! - 4 байта) до обрезки стеков было 340 - работает
#define W5200_SPI_SPEED   SPI_RATE          // ЭТО ДЕЛИТЕЛЬ (SPI_RATE определен в w5100.h)!!! Частота SPI w5200 = 84/W5200_SPI_SPEED т.е. 2-42МГц 3-28МГц 4-21МГц 6-14МГц Диапазон 2-6
#define W5200_SOCK_SYS    (MAX_SOCK_NUM-1)  // Номер системного сокета который не использутся в вебсервере, это последний сокет, НЕ МЕНЯТЬ
#define W5200_RTR         (2*0x07D0)        // время таймаута в 100 мкс интервалах  (по умолчанию 200ms(100us X 2000(0x07D0))) актуально для комманд CONNECT, DISCON, CLOSE, SEND, SEND_MAC, SEND_KEEP
#define W5200_RCR         (0x04)            // число повторов передачи  (по умолчанию 0x08 раз))
#define MAIN_WEB_TASK      0                // какой поток вебсервера является основным (поток в котором идет отправка MQTT и уведомлений) обычно 0

// ------------------- SERIAL --------------------------------
//Nextion дисплей
//#define NEXTION_DEBUG                     // Выводить информацию в отладочный порт с дисплея
#define NEXTION_PORT         Serial1        // Аппаратный порт куда прицеплен дисплей
#define NEXTION_PORT_SPEED   9600           // Скорость порта, бод
#define NEXTION_UPDATE       5000           // Время обновления информации на дисплее Nextion (мсек)
#define NEXTION_BOOT_TIME    300            // Время для загрузки дисплея, если при сбросе дисплей не находится надо увеличить (мсек)
#define NEXTION_READ         50             // Время опроса дисплея Nextion (мсек) разбор входной очереди

// Конфигурирование Modbus для инвертора и счетчика SDM
#ifndef MODBUS_PORT_NUM
#define MODBUS_PORT_NUM      Serial2        // Аппаратный порт куда прицеплен Modbus
#define MODBUS_PORT_SPEED    9600           // Скорость порта куда прицеплен частотник и счетчик
#define MODBUS_PORT_CONFIG   SERIAL_8N1     // Конфигурация порта куда прицеплен частотник и счетчик
#define MODBUS_TIME_WAIT     2000           // Время ожидания захвата мютекса для modbus мсек
#ifndef MODBUS_TIME_TRANSMISION
#define MODBUS_TIME_TRANSMISION 4           // Пауза (msec) между запросом и ответом по модбас было 4
#endif
#endif
//#define MODBUS_FREERTOS                     // Настроить либу на многозадачность определить надо в либе.
#if RADIO_SENSORS_PORT == 2
	#define RADIO_SENSORS_SERIAL	Serial2	// Аппаратный порт
#elif RADIO_SENSORS_PORT == 3
	#define RADIO_SENSORS_SERIAL	Serial3	// Аппаратный порт
#endif
#define RADIO_LOST_TIMEOUT 30*60*1000		// через сколько считать, что связь потеряна с датчиком, мсек

// Глобальные параметры инвертора инвертора на модбасе зависят от компрессора!!!!!!!!!
#define FC_MODBUS_ADR      1             // Адрес частотного преобразователя на шине не должно совпадать SMD_MODBUS_ADR
#define FC_TIME_READ      (8*1000)       // Время опроса инвертора в мск (было 6)
#define FC_NUM_READ        3             // Число попыток чтения инвертора (подряд) по модбас до его останова ТН по ошибке
#define FC_DELAY_REPEAT    40            // мсек Время между ПОВТОРНЫМИ попытками чтения было 100
#define FC_DELAY_READ      5             // мсек Время между последовательными запросами было 20
#define FC_WRITE_READ      10            // мсек Время между последовательной записью

// Глобальные переменные счетчика SDMxxx на модбасе
// настройки связи со счетчиком по умолчанию (из коробки см инструкцию) требуется для его программирования для работы
#define DEFAULT_SDM_SPEED       2400      // Скорость в бодах для счетчика по умолчанию
#define DEFAULT_SDM_MODBUS_ADR  1         // Адрес счетчика на шине не должно совпадать с FC_MODBUS_ADR при программировании инвертор ОКЛЮЧИТЬ (адрес 1)
// Требуемые настройки связи (после программирования)
#define SDM_SPEED           2              // скорость счетчика в константах 0 = 2400 bps. 1 = 4800 bps. 2 = 9600 bps 5=1200 bps  Скорости обмена должны совпадать см MODBUS_PORT_SPEED
#define SDM_MODBUS_ADR      2              // Адрес счетчика на шине не должно совпадать с FC_MODBUS_ADR
#ifndef SDM_READ_PERIOD
#define SDM_READ_PERIOD     (10*1000)      // Время опроса счетчика в мск
#endif
#ifndef SDM_NUM_READ
#define SDM_NUM_READ        4              // Число попыток чтения счетчика (подряд) по модбас до его отключения (ошибка не генерится)
#endif
#define SDM_DELAY_REPEAD    100            // мсек Время между ПОВТОРНЫМИ попытками чтения  было 40 (меньше не имеет смысла - счетчик может не успеть)
//#define SDM_BLOCK                        // Блокировать чтение счетчика при потере связи, в противном случае будут периодически делаться попытки восстановить связь
    
// ------------------- TIME & DELAY ----------------------------------
// Времена и задержки
#define cDELAY_DS1820     750              // мсек. Задержка для чтения DS1820 (время преобразования)
#ifndef TIME_READ_SENSOR 
#define TIME_READ_SENSOR  4000		       // мсек. Период опроса датчиков
#endif
#define TIME_WEB_SERVER   2                // мсек. Период опроса web servera было 5
#define TIME_CONTROL      (10*1000)        // мсек. Период управления тепловым насосом (цикл управления в режиме Гистерезис)
#define TIME_EEV          (1*1000)         // мсек. Период задачи vUpdateEEV в переходных состояниях ТН
#define TIME_EEV_BEFORE_PID (4*1000)       // мсек.
#define TIME_COMMAND      500              // мсек. Период разбора команд управления ТН (скорее пауза перед обработкой команды)
#define TIME_I2C_UPDATE   (60*60)*1000     // мсек. Время обновления внутренних часов по I2С часам (если конечно нужно)
#define TIME_MESSAGE_TEMP 300			   // 1/10 секунды, Проверка граничных температур для уведомлений
#define TIME_LED_OK       1500             // Период мигания светодиода при ОК (мсек)
#define TIME_LED_ERR      200              // Период мигания светодиода при ошибке (мсек).
#define TIME_BEEP_ERR     1000             // Период звукового сигнала при ошибке, мсек
#define cDELAY_START_MESSAGE 60            // Задержка (сек) после старта на отправку сообщений
#define UPDATE_HP_WAIT_PERIOD 5000			// Период вызова vUpdate во время ожидания или ошибки, мсек
#define NO_POWER_ON_DELAY_CNT 10			// Задержка включения после появления питани, *TIME_READ_SENSOR
#define HTTP_REQ_TIMEOUT   2000				// ms
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
#ifdef  I2C_EEPROM_64KB
// Стартовые адреса -----------------------------------------------------
// КАРТА ПАМЯТИ в чипе i2c объемом 64 кбайта
// 0х0000 - I2C_COUNT_EEPROM хранение счетчиков, максимальный размер 0x79 (127) байт. Сейчас используется 52 байта
// 0х0080 - I2C_SETTING_EEPROM хранение настроек ТН  максимальный размер 0х980 (2432) байт. Сейчас используется 1343 байт
// 0x0A00 - I2C_PROFILE_EEPROM хранение профилей, максимальный размер 0x1000 (4096) байт. Число профилей 10 шт. Размер профиля сейчас 294 байт (2940)
// 0x1A00 - I2C_SCHEDULER_EEPROM хранения расписаний, максимум 0x5FE (1534) байт (сейчас 350 байта)
// 0х1FFE - I2C_JOURNAL_EEPROM хранение журнала размер журнала область должна быть кратна W5200_MAX_LEN
	#define I2C_PROFIL_NUM			10           // Максимальное число сохряняемых профилей
	#define I2C_COUNT_EEPROM		0x0000      // Адрес внутри чипа eeprom от куда пишется счетчики с начала чипа 0
	#define I2C_SETTING_EEPROM		0x0080      // Адрес внутри чипа eeprom от куда пишутся настройки ТН  а перед ним пишется счетчики
	#define I2C_PROFILE_EEPROM		0x0A00      // Адрес внутри чипа eeprom от куда профили (адрес первого профиля)
	#define I2C_SCHEDULER_EEPROM	0x1A00		// Адрес внутри чипа eeprom для Расписаний
	#define MAX_CALENDARS			9   		// максимум 9
	#define TIMETABLES_MAXSIZE		500 		// bytes
	#define DAILY_SWITCH_MAX		10			// Максимальное количество записей ежедневного включения устройств (DailySwitch). MAX=10
	#define I2C_JOURNAL_EEPROM 		0x1FFE      // Адрес с которого начинается журнал в памяти i2c, внечале лежит признак форматирования журнала. Длина журнала JOURNAL_LEN
	#define I2C_JOURNAL_START 		(I2C_JOURNAL_EEPROM + 2)      // Адрес с которого начинается ДАННЫЕ журнал в памяти i2c ВНИМАНИЕ - 2 байт лежит признак форматирования журнала
	#define I2C_JOURNAL_EEPROM_NEXT (I2C_MEMORY_TOTAL * 1024 / 8) // Адрес после журнала = размер EEPROM
	// Журнал
	#define JOURNAL_LEN 			((I2C_JOURNAL_EEPROM_NEXT-I2C_JOURNAL_START)/W5200_MAX_LEN*W5200_MAX_LEN)// Размер журнала - округление на целое число страниц W5200_MAX_LEN
	#define I2C_JOURNAL_HEAD   		(0x01)                                                                  // Признак головы журнала
	#define I2C_JOURNAL_TAIL   		(0x02)                                                                  // Признак хвоста журнала
	#define I2C_JOURNAL_FORMAT 		(0xff)                                                                  // Символ которым заполняется журнал при форматировании
	#define I2C_JOURNAL_READY  		(0x55aa)                                                                // Признак создания журнала - если его нет по адресу I2C_JOURNAL_START-2 то надо форматировать журнал (первичная инициализация)
#else // 4KB eeprom
// 0х0000 - I2C_COUNT_EEPROM хранение счетчиков, максимальный размер 0x79 (127) байт. Сейчас используется 52 байта
// 0х0080 - I2C_SETTING_EEPROM хранение настроек ТН  максимальный размер 0х580 (1408) байт.
// 0х0600 - I2C_PROFILE_EEPROM хранение профилей, максимальный размер 0x860 (2144) байт.
// 0x0E60 - I2C_SCHEDULER_EEPROM хранения расписаний, максимум 0x1A0 (416) байт
	#define I2C_PROFIL_NUM			6           // Максимальное число сохряняемых профилей
	#define I2C_COUNT_EEPROM		0x00        // Адрес внутри чипа eeprom от куда пишется счетчики с начала чипа 0
	#define I2C_SETTING_EEPROM		0x080       // Адрес внутри чипа eeprom от куда пишутся настройки ТН  а перед ним пишется счетчики
	#define I2C_PROFILE_EEPROM		0x600       // Адрес внутри чипа eeprom от куда профили (адрес первого профиля)
	#define I2C_SCHEDULER_EEPROM	0xE60		// Адрес внутри чипа eeprom для Расписаний
	#define MAX_CALENDARS			4   		// максимум 9
	#define TIMETABLES_MAXSIZE		214 		// bytes
	#define DAILY_SWITCH_MAX		5			// Максимальное количество записей ежедневного включения устройств (DailySwitch)
	#define JOURNAL_LEN       		(2*W5200_MAX_LEN)   // Размер системного журнала ДОЛЖНО БЫТЬ кратно W5200_MAX_LEN, Увеличивать аккуратно, может не хвать памяти - виснет при загрузке
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
#define SAVE_TYPE_PwrCorr		-9
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
#define EEV_MAX_INT_PID		    1             // Максимальный вклад интегральной составляющей в шагах

#define EEV_START_POS_LOW_TEMP	1000		// Нижняя граница температура для установки позиции при старте, в стотых
#define EEV_START_POS_HIGH_TEMP	4500		// Верхняя граница температура для установки позиции при старте, в стотых
#define EEV_STAT_ARRAY_SIZE		4
#ifndef EEV_SET_ZERO_OVERRIDE
#define EEV_SET_ZERO_OVERRIDE	40			// Добавка к полному закрытию при установке нуля, шаги
#endif
#ifndef EEV_OVERHEAT2_CRITICAL
#define EEV_OVERHEAT2_CRITICAL 30           // Критическое значение перегрева 2, сотые градуса
#endif

// ----------------------- EVI ------------------------------
#define EVI_TEMP_CON      4000           // Температура кондесатора для включения EVI
#define EVI_TEMP_EVA      300            // Температура испарителя для включения EVI

// ------------------- ОБЩИЕ НАСТРОЙКИ ----------------------------------
#define TEMP_WEATHER      0              // Температура при которой устанавливается целевая температура подачи для отопления (погодозависимость)
#define MIN_WEATHER       (7*100)        // Минимальная температура подачи при погодозависимости
#define MAX_WEATHER       (50*100)       // Максимальная температура подачи при погодозависимости
#define HYSTERESIS_RHEAD				20	// Гистерезис работы дополнительного тена отопления (вычитается из целевой) в сотых градуса
#define HYSTERESIS_RBOILER				30	// Гистерезис работы дополнительного тена ГВС догрева (вычитается из целевой) в сотых градуса
#define HYSTERESIS_BoilerTogetherHeatSt	500	// Гистерезис совместного нагрева бойлера с отоплением в сотых градуса
#define HYSTERESIS_BoilerTogetherHeatEn	200	// Гистерезис совместного нагрева бойлера с отоплением в сотых градуса
#define HYSTERESIS_BoilerAddHeat      	300	// Гистерезис нагрева бойлера до температуры догрева, в сотых градуса
#define HYSTERESIS_HeatFloor		 	30	// Гистерезис раздельного управления реле теплого пола
#define SALMONELLA_DAY    3              // День когда включается алгоритм обеззараживания воды (Понедельник 1 воскресенье 7)
#define SALMONELLA_HOUR   1              // Час когда включается алгоритм обеззараживания воды (должно быть 0 минут)
#define SALMONELLA_TEMP   (70*100)       // Температура которая поддерживается для обеззараживания (сотые градуса)
#define SALMONELLA_TIME   (240*60)       // Максимальная продолжительность цикла (сек), что бы цикл не длился бесконечно при не возможности достижения SALMONELLA_TEMP
//#define SALMONELLA_HARD                // Если определено то работает поддержание температуры SALMONELLA_TEMP до окончания времени SALMONELLA_TIME, если НЕ ОПРЕДЕЛЕНО то выключение сразу по достижению SALMONELLA_TEMP но цикл не более SALMONELLA_TIME 
//#define NIGHT_START_HOUR  23           // Начало ночного тарифа
//#define NIGHT_END_HOUR	  7              // Окончание точного тарифа
// Сброс тепла при нагреве ГВС
#ifndef BOILER_TEMP_COMP_RESET
#define BOILER_TEMP_COMP_RESET  500      // На сколько температура нагнетания (TCOMP) меньше максимальной при нагреве ГВС, при котрой происходит сброс тепла в систему отопления
#endif
#define WEB0_OTHER_JOB_PERIOD   10*1000  // Периодичность других функций внутри задачи WEB0, мс
#define HTTP_REQUEST_ERR_REPEAT 120      // Повтор информирования при ошибке через n секунд
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
#define MQTT_REPEAT                      // Делать попытку повторного соединения с сервером
#define MQTT_NUM_ERR_OFF   8             // Число ошибок отправки подряд при котором отключается сервис отправки MQTT (флаг сбрасывается)

#define DEFAULT_PORT_MQTT 1883           // Адрес порта сервера MQTT по умолчанию
#define DEFAULT_TIME_MQTT (3*60)         // период отправки на сервер в сек. 10...60000
#define DEFAULT_ADR_MQTT "mqtt.thingspeak.com"   // Адрес MQTT сервера по умолчанию
#define DEFAULT_ADR_NARMON "narodmon.ru" // Адрес сервера народного мониторинга
#define TIME_NARMON       (5*60)         // (сек) Период отправки на народный монитоинг (константа) меньше 5 минут не ставить

// ------------------- HEAP ----------------------------------
#define PASS_LEN          10             // Максимальная длина пароля для входа на контроллер
#define NAME_USER         "user"         // имя пользователя
#define NAME_ADMIN        "admin"        // имя администратора
//#define FILE_STATISTIC    "statistic.csv"// имя файла статистики за ТЕКУШИЙ период

#define HOUR_SIGNAL_LIFE  12             // Час когда генерится сигнал жизни

#define ATOF_ERROR       -9876543.00     // Код ошибки преобразования строки во флоат
#define K_VCC_POWER       338.2          // Коэффициент пересчета ацп в вольты для контроля питания (учет опоры) (UT71E результататы ЗИП 284.02 ТН 338.2)
#define HEAT_CAPACITY     4174           // теплоемкость жидкости в конутре по дефолту при 30 градусах [Cp, Дж/(кг·град)]

#define HTTP_REQ_BUFFER_SIZE 256

//----------------------- WEB ----------------------------
const char WEB_HEADER_OK_CT[] 			= "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: ";
const char WEB_HEADER_TEXT_ATTACH[] 	= "text/plain\r\nContent-Disposition: attachment; filename=\"";
const char WEB_HEADER_BIN_ATTACH[] 		= "application/x-binary\r\nContent-Disposition: attachment; filename=\"";
const char WEB_HEADER_TXT_KEEP[] 		= "text/html\r\nConnection: keep-alive";
const char WEB_HEADER_END[]				= "\r\n\r\n";
const char* HEADER_FILE_NOT_FOUND = {"HTTP/1.1 404 Not Found\r\n\r\n<html>\r\n<head><title>404 NOT FOUND</title><meta charset=\"utf-8\" /></head>\r\n<body><h1>404 NOT FOUND</h1></body>\r\n</html>\r\n\r\n"};
//const char* HEADER_FILE_WEB       = {"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: keep-alive\r\n\r\n"}; // КЕШ НЕ ИСПОЛЬЗУЕМ
const char* HEADER_FILE_WEB       = {"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nCache-Control: max-age=3600, must-revalidate\r\n\r\n"}; // КЕШ ИСПОЛЬЗУЕМ
const char* HEADER_FILE_CSS       = {"HTTP/1.1 200 OK\r\nContent-Type: text/css\r\nConnection: keep-alive\r\nCache-Control: max-age=3600, must-revalidate\r\n\r\n"}; // КЕШ ИСПОЛЬЗУЕМ
const char* HEADER_ANSWER         = {"HTTP/1.1 200 OK\r\nContent-Type: text/ajax\r\nAccess-Control-Allow-Origin: *\r\n\r\n"};  // начало ответа на запрос
static uint8_t  fWebUploadingFilesTo = 0;                // Куда грузим файлы: 1 - SPI flash, 2 - SD card

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
#define   SUPC_SMMR_SMRSTEN (0x1u << 12) /**< \brief (SUPC_SMMR) Supply Monitor Reset Enable */
#define   SUPC_SMMR_SMRSTEN_NOT_ENABLE (0x0u << 12) /**< \brief (SUPC_SMMR) the core reset signal "vddcore_nreset" is not affected when a supply monitor detection occurs. */
#define   SUPC_SMMR_SMRSTEN_ENABLE (0x1u << 12) /**< \brief (SUPC_SMMR) the core reset signal, vddcore_nreset is asserted when a supply monitor detection occurs. */
#define   SUPC_SMMR_SMIEN (0x1u << 13) /**< \brief (SUPC_SMMR) Supply Monitor Interrupt Enable */
#define   SUPC_SMMR_SMIEN_NOT_ENABLE (0x0u << 13) /**< \brief (SUPC_SMMR) the SUPC interrupt signal is not affected when a supply monitor detection occurs. */
#define   SUPC_SMMR_SMIEN_ENABLE (0x1u << 13) /**< \brief (SUPC_SMMR) the SUPC interrupt signal is asserted when a supply monitor detection occurs. */
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

#ifndef FORMAT_DATE_STR_CUSTOM
const char *FORMAT_DATE_STR	 = { "%02d/%02d/%04d" };
#endif

// --------------------------------------------------------------------------------------------------------------------------------
// Строковые константы многократно используемые по всем файлам --------------------------------------------------------------------
const char *cYes= {"Да" };
const char *cNo = {"Нет"};
const char *cOne= {"1"  };
const char *cZero={"0"  };
const char *cOk=  {"Ok" };
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
#define WEBDELIM	"\x7f" // ALT+127 разделитель строки в вебе
const char SendMessageTitle[]	= "Народный контроллер теплового насоса";
const char SendSMSTitle[] 		= "Control";


// Многозадачность, деление аппартных ресурсов
const char *nameFREERTOS =     {"FreeRTOS"};           // Имя источника ошибки (нужно для передачи в функцию) - операционная система
const char *nameHeatPump =     {"Heat Pump"};           // Имя теплового насоса (для лога ошибок) Здесь можно его поменять
const char *MutexI2CBuzy =     {"I2C"}; 
const char *MutexModbusBuzy=   {"Modbus"}; 
const char *MutexWebThreadBuzy={"WebThread"}; 
const char *MutexSPIBuzy=      {"SPI"}; 
const char *MutexCommandBuzy = {"Command"}; 

// Описание имен параметров ЭРВ для функций get_pEEV set_pEEV
const char *eev_POS           =  {"POS"};           // Положение ЭРВ шаги
const char *eev_POSp          =  {"POSp"};          // Положение ЭРВ %
const char *eev_POSpp         =  {"POSpp"};         // Положение ЭРВ шаги+%
const char *eev_OVERHEAT      =  {"OH"};            // Текущий перегрев ЭРВ, если
const char *eev_ERROR         =  {"ERROR"};         // Ошибка ЭРВ
const char *eev_MIN           =  {"MIN"};           // Минимум ЭРВ
const char *eev_MAX           =  {"MAX"};           // Максимум ЭРВ
const char *eev_TIME          =  {"TIME"};          // ПИД время в секундах ЭРВ СЕКУНДЫ
const char *eev_TARGET        =  {"TRG"};        	// Перегрев ЦЕЛЬ (сотые градуса)
const char *eev_tOverheatTCOMP=  {"TRG2"};          // Перегрев2 цель (сотые градуса)
const char *eev_tOverheat2_low = {"T2L"};
const char *eev_tOverheat2_low_hyst = {"T2H"};
const char *eev_tOverheatTCOMP_delta= {"TRG2D"};    // Перегрев2 дельта цель (сотые градуса)
const char *eev_KP            =  {"KP"};            // ПИД Коэф пропорц.   В ТЫСЯЧНЫХ!!!
const char *eev_KI            =  {"KI"};            // ПИД Коэф интегр.  для настройки Ki=0   В ТЫСЯЧНЫХ!!!
const char *eev_KD            =  {"KD"};            // ПИД Коэф дифф.    В ТЫСЯЧНЫХ!!!
const char *eev_KP2           =  {"KP2"};           // ПИД Коэф пропорц.   В ТЫСЯЧНЫХ!!!
const char *eev_KI2           =  {"KI2"};           // ПИД Коэф интегр.  для настройки Ki=0   В ТЫСЯЧНЫХ!!!
const char *eev_KD2           =  {"KD2"};           // ПИД Коэф дифф.    В ТЫСЯЧНЫХ!!!
const char *eev_PID2_delta	  =  {"P2D"};           // Дельта для консервативных вычислений ПИДа (Для формулы 1  это ошибка при которой начинает уменьшаться пропорциональная)
const char *eev_PID_MAX       =  {"PMAX"};          // ограничение ПИД в шагах ЭРВ
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
const char *eev_cDELTA        =  {"cD"};            // TDIS_TCON: Температура нагнетания - конденсации (сотые градуса)
const char *eev_cDELTA_Thr    =  {"cDT"};           // Порог, после превышения которого начинаем менять перегрев, в сотых градуса
const char *eev_cOH_cDELTA_MAX =  {"cDM"};     	    // верхняя граница для пропорционального увеличения перегрева, % от OHCor_TDIS_TCON
const char *eev_cOH_MIN       =  {"cOH_MIN"};       // Минимальный перегрев (сотые градуса)
const char *eev_cOH_START     =  {"cOH_START"};     // Стартовый перегрев (сотые градуса)
const char *eev_cOH_MAX       =  {"cOH_MAX"};       // Максимальный перегрев (сотые градуса)
const char *eev_cOH_TDELTA    =  {"cTDELTA"};     	// Расчитанная целевая дельта Нагнетание-Конденсации
#ifndef PID_FORMULA2
const char *eev_ERR_KP        =  {"ERR_KP"};        // Ошибка (в сотых градуса) при которой происходит уменьшение пропорциональной составляющей ПИД ЭРВ
#endif
const char *eev_SPEED         =  {"SPEED"};         // Скорость шагового двигателя ЭРВ (импульсы в сек.)
const char *eev_PRE_START_POS =  {"PSP"};           // ПУСКОВАЯ позиция ЭРВ (ТО что при старте компрессора ПРИ РАСКРУТКЕ)
const char *eev_START_POS     =  {"SP"};            // СТАРТОВАЯ позиция ЭРВ после раскрутки компрессора т.е. ПОЗИЦИЯ С КОТОРОЙ НАЧИНАЕТСЯ РАБОТА проходит DelayStartPos сек
const char *eev_DELAY_ON_PID  =  {"DOP"};           // Задержка включения EEV после включения компрессора (сек).  Точнее после выхода на рабочую позицию Общее время =delayOnPid+DelayStartPos
const char *eev_DELAY_START_POS= {"DSP"};           // Время после старта компрессора когда EEV выходит на стартовую позицию - облегчение пуска вначале ЭРВ
const char *eev_DELAY_OFF     =  {"DOFF"};          // Задержка закрытия EEV после выключения насосов (сек). Время от команды стоп компрессора до закрытия ЭРВ = delayOffPump+delayOff
const char *eev_DELAY_ON      =  {"DON"};           // Задержка между открытием (для старта) ЭРВ и включением компрессора, для выравнивания давлений (сек). Если ЭРВ закрывлось при остановке
const char *eev_HOLD_MOTOR    =  {"HM"};    		// Флаг удержания мотора
const char *eev_PRESENT       =  {"PRESENT"};       // Флаг наличия ЭРВ в ТН
const char *eev_SEEK_ZERO     =  {"ZERO"};          // Флаг однократного поиска "0" ЭРВ (только при первом включении ТН)
const char *eev_CLOSE         =  {"CLOSE"};         // Флаг закрытие ЭРВ при выключении компрессора
const char *eev_LIGHT_START   =  {"LST"};           // флаг Облегчение старта компрессора   приоткрытие ЭРВ в момент пуска компрессора
const char *eev_START         =  {"START"};         // флаг Всегда начинать работу ЭРВ со стратовой позици
const char *eev_PID_P_ON_M    =  {"POM"};           // флаг ПИД пропорционально измерению
const char *eev_fEEVStartPosByTemp = {"SPT"};		// флаг fEEVStartPosByTemp
const char *eev_PosAtHighTemp =  {"PHT"};			// PosAtHighTemp
const char *eev_fEEV_DirectAlgorithm = {"DIR"};		// флаг fEEV_DirectAlgorithm
const char *eev_trend_threshold ={"TTH"};
const char *eev_trend_mul_threshold = {"TMT"};
const char *eev_DebugToLog    = {"DBG"};

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

// Описание имен параметров SDM счетчика для функций get_paramSDM ("get_SDM"), set_paramSDM ("set_SDM")
const char *sdm_NAME        = {"NAME"};               // Имя счетчика
const char *sdm_NOTE        = {"NOTE"};               // Описание счетчика
const char *sdm_MAX_VOLTAGE = {"MAXV"};        // Контроль напряжения максимум
const char *sdm_MIN_VOLTAGE = {"MINV"};        // Контроль напряжения минимум
const char *sdm_MAX_POWER   = {"MAXP"};          // Контроль мощности максимум
const char *sdm_VOLTAGE     = {"VOLT"};            // Напряжение
const char *sdm_CURRENT     = {"CURRENT"};            // Ток
const char *sdm_REPOWER     = {"REPOWER"};            // Реактивная мощность
const char *sdm_ACPOWER     = {"ACPOWER"};            // Активная мощность
const char *sdm_POWER       = {"POWER"};              // Полная мощность
const char *sdm_POW_FACTOR  = {"PF"};         // Коэффициент мощности
const char *sdm_PHASE       = {"PHASE"};              // Угол фазы (градусы)
const char *sdm_FREQ        = {"FREQ"};               // Частота
const char *sdm_ACENERGY    = {"ACENERGY"};           // Суммарная активная энергия
const char *sdm_LINK        = {"LINK"};               // Cостояние связи со счетчиком
const char *sdm_ERRORS  	= {"ERR"};                    // Ошибок чтения Modbus

// Описание имен параметров профиля для функций get_paramProfile set_paramProfile	
const char *prof_NAME_PROFILE   = {"NAME"};       // Имя профиля до 10 русских букв
const char *prof_ENABLE_PROFILE = {"ENABLE"};     // разрешение использовать в списке
const char *prof_ID_PROFILE     = {"ID"};         // номер профиля, нумерация c 1
const char *prof_NOTE_PROFILE   = {"NOTE"};       // описание профиля
const char *prof_DATE_PROFILE   = {"DATE"};       // дата профиля
const char *prof_CRC16_PROFILE  = {"CRC16"};      // контрольная сумма профиля
const char *prof_NUM_PROFILE    = {"NUM"};        // максимальное число профилей
const char *prof_SEL_PROFILE    = {"SEL"};        // список профилей (пока не используется)
const char prof_DailySwitch[] 	= "DS";
const char prof_DailySwitchDevice = 'D';		// DSD
const char prof_DailySwitchOn  	= 'S';			// DSS
const char prof_DailySwitchOff 	= 'E';			// DSE

// Описание имен параметров уведомлений для функций set_messageSetting get_messageSetting
const char *mess_MAIL         = {"MAIL"};                // флаг уведомления скидывать на почту
const char *mess_MAIL_AUTH    = {"MAIL_AUTH"};           // флаг необходимости авторизации на почтовом сервере
const char *mess_MAIL_INFO    = {"MAIL_INFO"};           // флаг необходимости добавления в письмо информации о состоянии ТН
const char *mess_SMS          = {"SMS"};                 // флаг уведомления скидывать на СМС
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
const char *mess_SMS_SERVICE  = {"SMS_list"};            // сервис отправки смс
const char *mess_SMS_IP       = {"SMS_IP"};              // IP Адрес сервера для отправки смс
const char *mess_SMS_PHONE    = {"SMS_PHONE"};           // телефон куда отправляется смс
const char *mess_SMS_P1       = {"SMS_P1"};              // первый параметр для отправки смс
const char *mess_SMS_P2       = {"SMS_P2"};              // второй параметр для отправки смс
const char *mess_SMS_NAMEP1   = {"SMS_NAMEP1"};          // описание первого параметра для отправки смс
const char *mess_SMS_NAMEP2   = {"SMS_NAMEP2"};          // описание второго параметра для отправки смс
const char *mess_MESS_TIN     = {"MESS_TIN"};            // Критическая температура в доме (если меньше то генерится уведомление)
const char *mess_MESS_TBOILER = {"MESS_TBOILER"};        // Критическая температура бойлера (если меньше то генерится уведомление)
const char *mess_MESS_TCOMP   = {"MESS_TCOMP"};          // Критическая температура компрессора (если больше то генериться уведомление)
const char *mess_MAIL_RET     = {"scan_MAIL"};           // Ответ на тестовую почту
const char *mess_SMS_RET      = {"scan_SMS"};            // Ответ на тестовую  sms

// Описание имен параметров бойлера для функций set_Boiler get_Boiler
const char *boil_BOILER_ON    = {"ON"};                  // флаг Включения бойлера
const char *boil_SCHEDULER_ON = {"SCH_ON"};        		 // флаг Использование расписания
const char *boil_SCHEDULER_ADDHEAT = {"SCH_AH"};         // флаг Использование расписания только для ТЭНа
const char *boil_TURBO_BOILER = {"TURBO"};               // флаг ТУРБО ГВС нагрев (нагрев=ТН+ТЭН)
const char *boil_SALMONELLA   = {"SLMN"};                // флаг Сальмонела раз в неделю греть бойлер
const char *boil_CIRCULATION  = {"CIRC"};  		         // флаг Управления циркуляционным насосом ГВС
const char *boil_TEMP_TARGET  = {"TRG"};                 // Целевая температура бойлера
const char *boil_DTARGET      = {"DTRG"};                // гистерезис целевой температуры
const char *boil_TEMP_MAX     = {"MAX"};                 // Tемпература подачи максимальная
const char *boil_SCHEDULER    = {"SCHEDULER"};           // Расписание
const char *boil_CIRCUL_WORK  = {"CIRCW"};               // Время  работы насоса ГВС секунды (fCirculation)
const char *boil_CIRCUL_PAUSE = {"CIRCP"};               // Пауза в работе насоса ГВС  секунды (fCirculation)
const char *boil_RESET_HEAT   = {"RESH"};                // флаг Сброса лишнего тепла в СО
const char *boil_RESET_TIME   = {"RESHT"};               // время сброса излишков тепла в СО в секундах (fResetHeat)
const char *boil_BOIL_TIME    = {"PT"};                  // Постоянная интегрирования времени в секундах ПИД ТН
const char *boil_BOIL_PRO     = {"PP"};                  // Пропорциональная составляющая ПИД ГВС
const char *boil_BOIL_IN      = {"PI"};                  // Интегральная составляющая ПИД ГВС
const char *boil_BOIL_DIF     = {"PD"};                  // Дифференциальная составляющая ПИД ГВС
const char *boil_BOIL_TEMP    = {"TEMP"};                // Целевая температура ПИД ГВС
const char *boil_ADD_HEATING  = {"ADDH"};                // флаг ДОГРЕВА ГВС ТЭНом
const char *boil_fAddHeatingForce={"AHF"};               // флаг Включать догрев, если компрессор не нагрел бойлер до температуры догрева
const char *boil_TEMP_RBOILER = {"TEMPR"};               // температура включения догрева бойлера
const char *boil_TOGETHER_HEAT= {"TGHEAT"};              // флаг Греть совместно с отоплением, если ТН работает на отопление
const char *boil_fBoilerPID   = {"PID"};                 // флаг ПИД вкл/выкл
const char *boil_dAddHeat     = {"dAH"};				 // Гистерезис нагрева бойлера до температуры догрева, в сотых градуса
const char *boil_HeatUrgently = {"URG"};				 // флаг Срочно нужно ГВС
const char *boil_DischargeDelta={"DD"};                  // Сброс тепла в отопление при приближении подачи к максимальной/догреву на °С
const char *boil_fWorkOnGenerator={"WG"};                // флаг Греть ГВС без ТЭНа при работе от генератора

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
const char *net_RES_SOCKET = {"NSLS"};             // Время сброса зависших сокетов
const char *net_RES_W5200  = {"NSLR"};             // Время регулярного сброса сетевого чипа
const char *net_PASS       = {"PASS"};             // Использование паролей (флаг)
const char *net_PASSUSER   = {"PASSUSER"};         // Пароль пользователя 
const char *net_PASSADMIN  = {"PASSADMIN"};        // Пароль администратора  
const char *net_SIZE_PACKET= {"SIZE"};             // размер пакета
const char *net_INIT_W5200 = {"INIT"};             // Ежеминутный контроль SPI для сетевого чипа
const char *net_PORT       = {"PORT"};             // Port веб сервера
const char *net_NO_ACK     = {"NO_ACK"};           // Не ожидать ответа ack
const char *net_DELAY_ACK  = {"DELAY_ACK"};        // Задержка перед посылкой следующего пакета
const char *net_PING_ADR   = {"PING"};             // адрес для пинга
const char *net_PING_TIME  = {"NSLP"};             // время пинга в секундах
const char *net_NO_PING    = {"NO_PING"};          // запрет пинга контроллера
const char *net_fWebLogError={"WLOG"};             // логировать ошибки веб запросов
const char *net_fWebFullLog= {"WFLOG"};            // выводить полный лог

// Описание имен параметров инвертора  для функций get_paramFC ("get_pFC") set_paramFC ("set_pFC")
const char *fc_ON_OFF            = {"ON_OFF"};            // Флаг включения выключения (управление частотником)
const char *fc_INFO              = {"INFO"};              // Получить информацию из инвертора (таблица !!)
const char *fc_NAME              = {"NAME"};              // Имя инвертора 
const char *fc_NOTE              = {"NOTE"};              // Получение описания частотного преобразователя. Строка 80+1
const char *fc_PIN               = {"PIN"};               // Получение номера пина куда прицеплен analog FC
const char *fc_PRESENT           = {"PRESENT"};           // Наличие FC в конфигурации. 
const char *fc_STATE             = {"STATE"};             // Состояние ПЧ (чтение)
const char *fc_FC                = {"FC"};                // Целевая частота инвертора в 0.01
const char *fc_cFC               = {"cFC"};               // Текущая частота ПЧ (чтение)
const char *fc_cPOWER            = {"cPOWER"};            // Текущая мощность (чтение)
const char *fc_INFO1             = {"INFO1"};             // Первая строка под картинкой инвертора на схеме
const char *fc_cCURRENT          = {"cCURRENT"};          // Текущий ток (чтение)
const char *fc_AUTO_RESET_FAULT  = {"ARSTFLT"};           // Флаг автоматического сброса не критичной ошибки инвертора
const char *fc_LogWork		     = {"LOGW"};              // Флаг логировать во время работы
const char *fc_ANALOG            = {"AN"};                // Флаг аналогового управления
const char *fc_DAC               = {"DAC"};               // Получение текущего значения ЦАП
const char *fc_LEVEL0            = {"L0"};                // Уровень частоты 0 в отсчетах ЦАП
const char *fc_LEVEL100          = {"L100"};              // Уровень частоты 100% в  отсчетах ЦАП
const char *fc_LEVELOFF          = {"LOFF"};              // Минимальная мощность при котором частотник отключается (ограничение минимальной мощности)
const char *fc_BLOCK             = {"BLOCK"};             // флаг глобальная ошибка инвертора - работа инвертора запрещена блокировку можно сбросить установив в 0
const char *fc_ERROR             = {"ERROR"};             // Получить код ошибки
const char *fc_UPTIME            = {"UPTIME"};            // Время обновления алгоритма пид регулятора (мсек) Основной цикл управления
const char *fc_PID_STOP          = {"PID_STOP"};          // Проценты от уровня защит (мощность, ток, давление, температура) при которой происходит блокировка роста частоты пидом
const char *fc_PID_FREQ_STEP     = {"PID_STEP"};          // Максимальный шаг (на увеличение) изменения частоты при ПИД регулировании в 0.01 Гц Необходимо что бы ЭРВ успевал
const char *fc_START_FREQ        = {"FRQ"};               // Стартовая частота инвертора (см компрессор) в 0.01 ГЦ
const char *fc_START_FREQ_BOILER = {"FRQB"};			  // Стартовая частота инвертора (см компрессор) в 0.01 ГЦ ГВС
const char *fc_MIN_FREQ          = {"MIN"};         	  // Минимальная  частота инвертора (см компрессор) в 0.01 Гц
const char *fc_MIN_FREQ_COOL     = {"MINC"};    	      // Минимальная  частота инвертора при охлаждении в 0.01 Гц
const char *fc_MIN_FREQ_BOILER   = {"MINB"};  	          // Минимальная  частота инвертора при нагреве ГВС в 0.01 Гц
const char *fc_MIN_FREQ_USER     = {"MINU"};    	      // Минимальная  частота инвертора РУЧНОЙ РЕЖИМ (см компрессор) в 0.01 Гц
const char *fc_MAX_FREQ          = {"MAX"};          	  // Максимальная частота инвертора (см компрессор) в 0.01 Гц
const char *fc_MAX_FREQ_COOL     = {"MAXC"};          	  // Максимальная частота инвертора в режиме охлаждения  в 0.01 Гц
const char *fc_MAX_FREQ_BOILER   = {"MAXB"};  	          // Максимальная частота инвертора в режиме ГВС в 0.01 Гц поглощение бойлера обычно меньше чем СО
const char *fc_MAX_FREQ_USER     = {"MAXU"};   	 	      // Максимальная частота инвертора РУЧНОЙ РЕЖИМ (см компрессор) в 0.01 Гц
const char *fc_MAX_FREQ_GEN      = {"MAXG"};   	 	      // Максимальная частота инвертора на генераторе в 0.01 Гц
const char *fc_STEP_FREQ         = {"STEP"};        	  // Шаг уменьшения инвертора при достижении максимальной температуры, мощности и тока (см компрессор) в 0.01 Гц
const char *fc_STEP_FREQ_BOILER  = {"STEPB"};		 	  // Шаг уменьшения инвертора при достижении максимальной температуры, мощности и тока ГВС в 0.01 Гц
const char *fc_DT_COMP_TEMP      = {"DTC"};   			  // Защита по температуре компрессора - сколько градусов не доходит до максимальной (TCOMP) и при этом происходит уменьшение частоты
const char *fc_DT_TEMP           = {"DT"};           	  // Превышение температуры от уставок (подача) при которой срабатыват защита (уменьшается частота) в сотых градуса
const char *fc_DT_TEMP_BOILER    = {"DTB"};    			  // Превышение температуры от уставок (подача) при которой срабатыват защита ГВС в сотых градуса
const char *fc_MB_ERR		     = {"MB_ERR"};			  // Ошибок Modbus
const char *fc_FC_TIME_READ      = {"TR"};				  // Время опроса
const char *fc_fFC_RetOil	     = {"FRO"};               // Флаг возврат масла
const char *fc_FC_RETOIL_FREQ	 = {"FRF"};				  // Частота
const char *fc_ReturnOilPeriod   = {"ROP"};               // Время возварта масла
const char *fc_ReturnOilPerDivHz = {"ROPH"};              // Частота при которой возвращается масло
const char *fc_ReturnOilEEV      = {"ROE"};               // Шаги ЭРВ при котором возвращается масло

// Описание имен параметров опций ТН  для функций get_optionHP ("get_oHP") set_optionHP ("set_oHP")
const char *option_ADD_HEAT           = {"HEAT_list"};              // использование дополнительного нагревателя (значения 1 и 0)
const char *option_TEMP_RHEAT         = {"TEMP_RHEAT"};         // температура для управления RHEAT (градусы)
const char *option_PUMP_WORK          = {"PUMP_WORK"};          // работа насоса конденсатора при выключенном компрессоре секунды
const char *option_PUMP_PAUSE         = {"PUMP_PAUSE"};         // пауза между работой насоса конденсатора при выключенном компрессоре (секунды)
const char *option_ATTEMPT            = {"ATTEMPT"};            // число попыток пуска
const char *option_TIME_CHART         = {"TIME_CHART"};         // период сбора статистики
const char *option_BEEP               = {"BEEP"};               // включение звука
const char *option_NEXTION            = {"NXT"};                // использование дисплея nextion
const char *option_NEXTION_WORK       = {"NXTW"};               // Включать дисплей, когда ТН работает
const char *option_History            = {"HIST"};               // запись истории на SD карту
const char *option_SDM_LOG_ERR        = {"SDM_LOGER"};          // флаг писать в лог нерегулярные ошибки счетчика SDM
const char *option_SAVE_ON            = {"SAVE_ON"};            // флаг записи в EEPROM включения ТН (восстановление работы после перезагрузки)
const char *option_NEXT_SLEEP         = {"NXTS"};               // Время засыпания секунды NEXTION
const char *option_NEXT_DIM           = {"NXTD"};               // Якрость % NEXTION
const char option_SGL1W[]             = "SGL1W_";			    // SGLOW_n, На шине n (1-Wire, DS2482) только один датчик
const char *option_DELAY_ON_PUMP      = {"DLONP"};      		// Задержка включения компрессора после включения насосов (сек).
const char *option_DELAY_OFF_PUMP     = {"DLOFP"};     			// Задержка выключения насосов после выключения компрессора (сек).
const char *option_DELAY_START_RES    = {"DLSR"};    			// Задержка включения ТН после внезапного сброса контроллера (сек.)
const char *option_DELAY_REPEAD_START = {"DLRS"};				// Задержка перед повторным включениме ТН при ошибке (попытки пуска) секунды
const char *option_DELAY_DEFROST_ON   = {"DLDON"};  			// ДЛЯ ВОЗДУШНОГО ТН Задержка после срабатывания датчика перед включением разморозки (секунды)
const char *option_DELAY_DEFROST_OFF  = {"DLDOFF"};  			// ДЛЯ ВОЗДУШНОГО ТН Задержка перед выключением разморозки (секунды)
const char *option_DELAY_R4WAY        = {"DLTRV"};        		// Задержка между переключением 4-х ходового клапана и включением компрессора, для выравнивания давлений (сек). Если включены эти опции (переключение тепло-холод)
const char *option_DELAY_BOILER_SW    = {"DLBSW"};    			// Пауза (сек) после переключение ГВС - выравниваем температуру в контуре отопления/ГВС что бы сразу защиты не сработали
const char *option_DELAY_BOILER_OFF   = {"DLBOFF"};   			// Время (сек) на сколько блокируются защиты при переходе с ГВС на отопление и охлаждение слишком горяче после ГВС
const char *option_SunTempOn		  = {"STO"};				// Температура выше которой открывается СК
const char *option_SunTempOff		  = {"STF"};				// Температура ниже которой закрывается СК
const char *option_SunRegGeo		  = {"SCG"};				// Использовать солнечный коллектор для регенерации геоконтура в простое
const char *option_SunRegGeoTemp	  = {"SCGT"};				// Температура начала регенерации геоконтура с помощью СК, в градусах
const char *option_SunRegGeoTempGOff  = {"SCGO"};				// Температура жидкости конца регенерации геоконтура с помощью СК, в градусах
const char *option_SunTDelta		  = {"STD"};				// Дельта температур для включения, сотые градуса
const char *option_SunGTDelta		  = {"SGD"};				// Дельта температур жидкости для выключения, сотые градуса
const char *option_SunMinWorktime	  = {"SW"};
const char *option_SunMinPause		  = {"SP"};
const char *option_WebOnSPIFlash      = {"WSPIF"};              // флаг, что веб морда лежит на SPI Flash, иначе на SD карте
const char *option_LogWirelessSensors = {"LOGWS"};              // Логировать обмен между беспроводными датчиками
const char *option_PAUSE              = {"PAUSE"};              // минимальное время простоя компрессора
const char *option_MinCompressorOn    = {"MCON"};               // Минимальное время работы компрессора в секундах
const char *option_Charts_when_comp_on= {"CWCO"};				// Графики в памяти только во время работы компрессора
const char *option_fBackupPower       = {"BPOW"};				// флаг Использование резервного питания от генератора (ограничение мощности) 
const char *option_fBackupPowerInfo   = {"BP"};					// Работа от генератора
const char *option_maxBackupPower     = {"MAXPOW"};				// Максимальная мощность при питании от генератора
const char *option_fBackupPowerAuto	  = {"BPA"};


// Отопление/охлаждение параметры
const char *hp_RULE      = {"RULE"};             // алгоритм работы
const char *hp_TEMP1     = {"TEMP1"};            // целевая температура в доме
const char *hp_TEMP2     = {"TEMP2"};            // целевая температура обратки
const char *hp_TARGET    = {"TARGET"};           // что является целью ПИД - значения  0 (температура в доме), 1 (температура обратки).
const char *hp_DTEMP     = {"DTEMP"};            // гистерезис целевой температуры
const char *hp_dTempGen  = {"DTG"};              // гистерезис целевой температуры на генераторе
const char *hp_HP_TIME   = {"HP_TIME"};          // Постоянная интегрирования времени в секундах ПИД ТН
const char *hp_HP_PRO    = {"HP_PRO"};           // Пропорциональная составляющая ПИД ТН
const char *hp_HP_IN     = {"HP_IN"};            // Интегральная составляющая ПИД ТН
const char *hp_HP_DIF    = {"HP_DIF"};           // Дифференциальная составляющая ПИД ТН
const char *hp_TEMP_IN   = {"TEMP_IN"};          // температура подачи (минимальная для охлажления или максимальная для нагрева)
const char *hp_TEMP_OUT  = {"TEMP_OUT"};         // температура обратки (максимальная для охлажления или минимальная для нагрева)
const char *hp_D_TEMP    = {"D_TEMP"};           // максимальная разность температур конденсатора.
const char *hp_TEMP_PID  = {"TEMP_PID"};         // Целевая температура ПИД
const char *hp_WEATHER   = {"W"};          		// Использование погодозависимости
const char *hp_HEAT_FLOOR = {"HFL"};          	 // Использование теплого пола
const char *hp_SUN 		 = {"SUN"};          	 // Использование солнечного коллектора
const char *hp_targetPID = {"TP"};       	 // Цель PID
const char *hp_K_WEATHER = {"KW"}; 		        // Коэффициент погодозависимости PID
const char *hp_kWeatherTarget = {"KWT"};		// Коэффициент погодозависимости
const char *hp_WeatherBase = {"KWB"};
const char *hp_WeatherTargetRange = {"KWR"};

// Действуют для отопления и ГВС
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
const char *noteFreon[]    =   {"R22","R410A","R600","R134a","R407C","R12","R290","R404A","R717"};
// Названия правил работы ЭРВ для веба
const char noteRuleEEV[]   =	"TEVAOUT-T[PEVA]:0;"
	#ifdef TCOMPIN
								"TCOMPIN-T[PEVA]:0;"
		#ifdef TEVAIN
								"TEVAOUT-TEVAIN:0;"
								"TCOMPIN-TEVAIN:0;"
								"TABLE[EVA,CON]:0;"
		#endif
	#endif
								"MANUAL:0;";

// Описание правила работы ЭРВ
const char *noteRemarkEEV[] = {	"Перегрев равен: температура на выходе испарителя - температура по давлению на выходе испарителя.",
#ifdef TCOMPIN
								"Перегрев равен: температура на входе компрессора - температура по давлению на выходе испарителя.",
	#ifdef TEVAIN
								"Перегрев равен: температура на выходе испарителя - температура на входе испарителя.",
								"Перегрев равен: температура на входе компрессора - температура на входе испарителя.",
								"Перегрев не вычисляется. ЭРВ открывается по значению шага из таблицы температур испарителя и конденсатора.",
	#endif
#endif
								"Перегрев не вычисляется. Ручной режим, ЭРВ открывается на заданное число шагов."};

// Предупреждения
#define WARNING_VALUE        1         // Попытка установить значение за границами диапазона запрос типа SET

// GPBR  - восемь 32x битных регистров не обнуляющиеся при сбросе, Энергозависимы!
// адреса - 0x90-0xDC General Purpose Backup Register GPBR
// The System Controller embeds Eight general-purpose backup registers.
// Карта использования GPBR в НК
// GPBR->SYS_GPBR[0] текущую задача (сдвиг на 8 влево) +  номера ошибки RTOS
// GPBR->SYS_GPBR[1] причина сброса контроллера
// GPBR->SYS_GPBR[4] последня точка отладки перед сбросом максимальнаое значение точки отладки сейчас 56 (обновляем!)
#define STORE_DEBUG_INFO(s) GPBR->SYS_GPBR[4] = s  // Сохранение номера точки отладки в энергозависимой памяти sam3
#define WEB_STORE_DEBUG_INFO(s) GPBR->SYS_GPBR[5] = s  // Сохранение номера точки отладки в энергозависимой памяти sam3

// --------------------------------------------------------------------------------
// ОШИБКИ едины для всего - сквозной список
// --------------------------------------------------------------------------------
#define OK                  0          // Ошибок нет
#define ERR_MINTEMP        -1          // Выход за нижнюю границу температурного датчика
#define ERR_MAXTEMP        -2          // Выход за верхнюю границу температурного датчика
#define ERR_ANALOG_MIN       -3          // Выход за нижнюю границу  датчика давления
#define ERR_ANALOG_MAX       -4          // Выход за верхнюю границу датчика давления
#define ERR_SENSOR         -5          // Датчик запрещен в текущей конфигурации
#define ERR_ADDRESS        -6          // Адрес датчика температруры не установлен
#define ERR_DINPUT         -7          // Срабатывания контактного датчика -  авария
#define ERR_MAX_EEV        -8          // Выход за диапазон (по шагам) в верху
#define ERR_MIN_EEV        -9          // Выход за диапазон (по шагам) в внизу
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
#define ERR_WRONG_HARD_STATE -25       // Неверное состояние ТН
#define ERR_DTEMP_CON      -26         // Привышена разность температур на конденсаторе
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
#define ERR_I2C_BUZY       -67         // При обращении к I2C шине превышено время ожидания ее освобождения
#define ERR_DRV_EEV        -68         // Отказ драйвера L9333 ЭРВ (сработала защита драйвера)
#define ERR_HEADER2_EEPROM -69         // Ошибка заголовка счетчиков в eeprom I2C
#define ERR_OPEN_I2C_JOURNAL -70       // Ошибка открытия журнала в I2C памяти (инициализация чипа)
#define ERR_READ_I2C_JOURNAL -71       // Ошибка чтения журнала в I2C памяти
#define ERR_WRITE_I2C_JOURNAL -72      // Ошибка записи журнала в I2C памяти
//#define ERR_   			-73        //
#define ERR_MIN_FLOW        -74        // Поток в ПТО ниже установленного уровня
#define ERR_MAX_VOLTAGE     -75        // Слишком большое напряжение сети (данные электросчетчика)
#define ERR_MAX_POWER       -76        // Слишком большая портебляемая мощность (данные электросчетчика)
#define ERR_NO_MODBUS       -77        // Modbus требуется, но отсутвует в конфигурации
#define ERR_RESET_FC        -78        // Не удалось сбросить инвертор после ошибки
#define ERR_SEVA_FLOW       -79        // Отсутствует проток в испарителе (срабатывание SEVA)
#define ERR_COMP_NO_PUMP    -80        // Попытка включить компрессор при неработающих насосах контуров.
#define ERR_DEFROST_R4WAY   -81        // Ошибочная конфигурация - попытка разморозки при не возможности переключения на охлаждение (нет R4WAY)
#define ERR_DEFROST         -82        // Требуется разморозка (есть условия) при охлаждении
#define ERR_FC_CONF_ANALOG  -83        // Ошибка использования аналогового управления инвертором без выхода ХОД
#define ERR_READ_TEMP       -84        // Ошибка чтения температурного датчика (лимит чтения исчерпан)
//#define ERR_			     -85        //
#define ERR_ONEWIRE_CRC     -86		   // ошибка CRC во время чтения OneWire
#define ERR_ONEWIRE_RW      -87		   // ошибка во время чтения/записи OneWire
#define ERR_FC_FAULT		-88			// сбой инвертора
#define ERR_FC_ERROR		-89			// ошибка программы управления инвертором
#define ERR_SD_WRITE		-90			// ошибка записи на SD карту
#define ERR_FC_RCOMP		-91			// Не возможно остановить инвертор с помошью RCOMP
#define ERR_FC_NO_LINK		-92			// Нет связи с инвертором по MODBUS

#define ERR_ERRMAX			-92 		// Последняя ошибка

#ifdef NOT_RESTART_ON_CRITICAL_ERRORS
const int8_t CRITICAL_ERRORS[] = { ERR_COMP_ERR };
#endif

// Описание ВСЕХ Ошибок длина описания не более 160 байт (ограничение основной класс note_error[160+1])
const char *noteError[] = {"Ok",                                                  //  0
                           "Выход за нижнюю границу температурного датчика",      // -1
                           "Выход за верхнюю границу температурного датчика",     // -2
                           "Выход за нижнюю границу  датчика давления",           // -3
                           "Выход за верхнюю границу датчика давления",           // -4
                           "Датчик запрещен в текущей конфигурации",              // -5
                           "Адрес датчика температруры не установлен",            // -6
                           "Срабатывания контактного датчика - авария",           // -7
                           "ЭРВ выход за диапазон (по шагам) вверх (отказ ЭРВ?)", // -8
                           "ЭРВ выход за диапазон (по шагам) вниз (отказ ЭРВ?)",  // -9
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
                           "Неверное состояние ТН",     		                  // -25
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
                           "Правило вычисления перегрева не соответствует датчикам",				//-38
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
                           "При обращении к I2C шине превышено время ожидания ее освобождения",                 //-67
                           "Отказ драйвера L9333 ЭРВ (сработала защита драйвера)",                              //-68
                           "Ошибка заголовка счетчиков в I2C",                          		                //-69
                           "Ошибка открытия журнала в I2C (инициализация чипа)", 		                        //-70
                           "Ошибка чтения журнала из I2C памяти",                                               //-71
                           "Ошибка записи журнала в I2C память",                                                //-72
                           "",                   //-73
                           "Поток в ПТО ниже установленного уровня (проблема - насос, фильтр)",                 //-74
                           "Слишком большое напряжение сети (данные SDM счетчика)",                             //-75
                           "Слишком большая потребляемая мощность (данные SDM счетчика)",                       //-76
                           "Modbus требуется, но отсутвует в конфигурации",                                     //-77
                           "Не удалось сбросить инвертор после ошибки",                               			//-78
                           "Отсутствует проток в испарителе (срабатывание SEVA)",                               //-79
                           "Попытка включить компрессор при неработающем насосе контура",                       //-80
                           "Попытка разморозки при не возможности переключения на охлаждение (нет R4WAY)",      //-81
                           "Требуется разморозка (есть условия по температурам) при охлаждении",                //-82
                           "Ошибка использования аналогового управления инвертором без выхода ХОД",             //-83
                           "Ошибка чтения температурного датчика (лимит попыток чтения исчерпан)",              //-84
                           "",  		//-85
						   "Ошибка CRC чтения OneWire",															//-86
						   "Ошибка чтения/записи OneWire",														//-87
						   "Сбой инвертора",																	//-88
						   "Ошибка программы управления инвертором",											//-89
						   "Ошибка записи на SD карту",															//-90
                           "Не возможно остановить инвертор с помошью RCOMP",                                   //-91
						   "Нет связи с инвертором по MODBUS",													//-92
                           
                           "NULL"
                           };
// --------------------------------- ПЕРЕЧИСЛЯЕМЫЕ ТИПЫ ---------------------------------------------
//  Перечисляемый тип - источник загрузки для веб морды
enum TYPE_SOURSE_WEB
{
 pMIN_WEB,                 // Надо грузить минимальную морду
 pSD_WEB,                  // Надо грузить морду с карты 
 pFLASH_WEB,               // Надо грузить морду с флеш диска
 pERR_WEB                  // Внутренняя ошибка? 
};


//  Перечисляемый тип - Типы ответов POST запросов
enum TYPE_RET_POST
{
 pSETTINGS_OK,                // Настройки из выбранного файла восстановлены, CRC16 OK 
 pSETTINGS_ERR,               // Ошибка восстановления настроек из файла (см. журнал) 
 pNULL,                       // "" - пустая строка
 pLOAD_OK,                    // Файлы загружены, подробности смотри в журнале 
 pLOAD_ERR,                   // Ошибка загрузки файла, подробности смотри в журнале      
 pPOST_ERR,                   // Внутреняя ошибка парсера post запросов 
 pNO_DISK,                    // Нет флеш диска
 pSETTINGS_MEM                // Настройки не влезают во внутренний буфер
};

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
// Источник: B-бойлер, H-отопление, C-охлаждение
// Алгоритм: h - гистерeзис, p - ПИД
// Код алгоритма
// 1 - выключение по подаче
// 2 - включение по гистерезису
// 3 - выключение по гистерезису
// 4 - внутри гистерезиса (продолжение: нагрев или охлаждение)
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
// 28 - Ограничение мощности при работе от резервного источника питания (сброс частоты)
// 29 - Выключение режима ТН при достижении минимальной частоты при работе от резервного источника питания.
// 30 - Ограничение при работе от генератора


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
  pBp28,
  pBp29, 
  pBdis,  // Сброс тепла
  pBgen,  // Запрет на генераторе
 
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
  pHp28,
  pHp29,   

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
  pCp28,
  pCp29,      
  
  pEND18                            // Обязательно должен быть последним, добавляем ПЕРЕД!!!
};
//  Для вывода кодов
const char *codeRet[]={ "none","MinPause","Bh1","Bh2","Bh3","Bh4","Bh5","Bh22","Bp3","Bp1","Bp2","Bp6","Bp7","Bp8","Bp9","Bp5","Bp10","Bp11","Bp12","Bp14","Bp16","Bp17","Bp18","Bp19","Bp20","Bp21","Bp22", "Bp23","Bp24","Bp25","Bp26","Bp27","Bp28","Bp29","Bdis","Bgen",\
                       "Hh3","Hh1","Hh2","Hh13","Hh4","Hp3","Hp1","Hp2","Hp6","Hp7","Hp8","Hp9","Hp5","Hp10","Hp11","Hp12","Hp15","Hp16","Hp17","Hp18","Hp19","Hp20","Hp21","Hp23","Hp24","Hp25","Hp26","Hp27","Hp28","Hp29",\
                       "Ch3","Ch1","Ch2","Ch13","Ch4","Cp3","Cp1","Cp2","Cp6","Cp7","Cp8","Cp9","Cp5","Cp10","Cp11","Cp12","Cp15","Cp16","Cp17","Cp18","Cp19","Cp20","Cp21","Cp23","Cp24","Cp25","Cp26","Cp27","Cp28","Cp29","null"};           

//  Перечисляемый тип - действия над компрессором
enum MODE_COMP          
{
  pCOMP_OFF,                     // компрессор был выключен
  pCOMP_ON,                      // компрессор был включен
  pCOMP_NONE,                    // компрессор ничего не делаем - не меняем стутус
  pEND9                          // Обязательно должен быть последним, добавляем ПЕРЕД!!!
};

//  Перечисляемый тип - что надо делать или какой режим отопления выбран (первые три)
#define MODE_HP uint8_t
#define pOFF		0             // Выключить
#define pHEAT		0x01          // Отопление
#define pCOOL		0x02          // Охлаждение
#define pBOILER		0x04          // Бойлер
#define pDEFROST	0x08          // Разморозка воздушника
#define pCONTINUE	0x80          // флаг Продолжения

const char *MODE_HP_STR[] = { "Off", "Heat", "Cool", "Boil", "Defrost", "C" };
const char MODE_HOUSE_WEBSTR[] = "Выключено:0;Отопление:0;Охлаждение:0;";

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
  pPROG_FC,                      // 13 Первоначальное программирование частотного преобразователя
  pEND14                         // Обязательно должен быть последним, добавляем ПЕРЕД!!!
};

const char *hp_commands_names[] = {"EMPTY", "START", "AUTOSTART", "STOP", "RESET", "RESTART", "REPEAT", "NETWORK", "JFORMAT", "SFORMAT", "SAVE", "WAIT", "RESUME", "PROG_FC", "UNKNOWN"};

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
  pSMSC_UA,                      // Сервис smsc.ua
  pSMSCLUB                    // Сервис smsclub.mobi
};

const char ADR_SMS_RU[]  = "sms.ru";
const char ADR_SMSC_RU[] = "smsc.ru";
const char ADR_SMSC_UA[] = "smsc.ua";
const char ADR_SMSCLUB[] = "gate.smsclub.mobi";
const char SMS_SERVICE_WEB_SELECT[] = "sms.ru:0;smsc.ru:0;smsc.ua:0;smsclub.mobi:0;";

//  Перечисляемый тип - тип фреона
//enum TYPEFREON
//{
#define     R22		0
#define     R410A	1
#define     R600A	2
#define     R134A	3
#define     R407C	4
#define     R12		5
#define     R290	6
#define     R404A	7
#define     R717	8            // Обязательно должен быть последним, добавляем ПЕРЕД!!!
//};

//  Перечисляемый тип - правило работы ЭРВ пять вариантов выводятся в зависимости от наличия датчиков
enum RULE_EEV           
{
   TEVAOUT_PEVA,
#ifdef TCOMPIN
   TCOMPIN_PEVA,
#endif
#ifdef TEVAIN
   TEVAOUT_TEVAIN, 
   TCOMPIN_TEVAIN,
   TABLE,
#endif
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

struct PID_STRUCT {   		// Настройки ПИД регулятора
	  int16_t Kp;           // ПИД Коэф. пропорц, в тысячных (отриц)
	  int16_t Ki;           // ПИД Коэф. интегральный, в тысячных (отриц)
	  int16_t Kd;           // ПИД Коэф. дифференциальный,в тысячных (отриц)
} __attribute__((packed));


#define trOH_default	2
#define trOH_TCOMP		3

struct PID_WORK_STRUCT {	// Переменные ПИД регулятора
	union {
		int32_t sum;		// сумма
		int16_t pre_err2[2];// i=0
		int8_t  trend[4];	// i=trOH_*
	};
	int16_t pre_err;		// предыдущая ошибка для дифференцирования
	int32_t max;
#ifdef PID_FORMULA2
	union {
		int32_t min;
		uint8_t hyst[4];	// i=0..1
	};
	boolean PropOnMeasure;	// ПИД пропорционально измерению, иначе пропорционально ошибке
#else
	union {
		int16_t Kp_dmin;	// Разница (в сотых градуса) при которой происходит уменьшение пропорциональной составляющей ПИД ЭРВ
		uint8_t hyst[2];	// i=0..1
	};
#endif
};

#endif
