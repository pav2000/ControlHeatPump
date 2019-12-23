 /*
 * Copyright (c) 2016-2019 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
 * &                       by Vadim Kulakov vad7@yahoo.com, vad711
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
// Цифровые датчики температуры -------------------------------------------------------------

#ifndef DigitalTemp_h
#define DigitalTemp_h
#define STARTTEMP 32767  // Значение инициализации датчика температуры, по нему определяется первая итерация (сотые градуса)

enum TEMP_SETUP_FLAGS { // bit #
	fDS2482_bus = 0,		// 0..3 - шина DS2482
	fTEMP_ignory_errors = 2,// 2: игнорировать ошибки датчика - не будет останавливаться ТН
	fTEMP_dont_log_errors,	// 3: не логировать ошибки
	fTEMP_ignory_CRC,		// 4: Ошибки CRC игнорируются - неверные показания отбрасываеются через GAP_TEMP_VAL_CRC
	fTEMP_as_TIN_average,	// 5: Используется для расчета TIN в качестве средней температуры, между всеми датчиками с таким флагом
	fTEMP_as_TIN_min,		// 6: Используется для расчета TIN в качестве минимальной температуры, между всеми датчиками с таким и "average" флагами
	fTEMP_HeatFloor			// 7: Используется для управления реле теплого пола (RPUMPFL), если температура любого датчика с этим флагом меньше целевой - реле вкл, иначе - реле выкл.
};
#define fDS2482_bus_mask	3

// Удаленные датчики температуры -------------------------------------------------------------
#ifdef SENSOR_IP
//  Работа с отдельными флагами sensorIP
#define fUse        0               // флаг разрешить использование датчика
#define fRule       1               // флаг Правило использования датчика 0 - использование удаленного датчика 1- усреднение с основным
class sensorIP
{
  public:
    void initIP(uint8_t sensor);                             // Инициализация датчика
    boolean set_DataIP(int16_t a,int16_t b,int16_t c,int16_t d,uint32_t e,IPAddress f);  // Запомнить данные (обновление данных)
    char* get_sensorIP(char *var, char *ret);                    // Получить параметр в виде строки
    __attribute__((always_inline)) inline int16_t get_Temp(){return Temp;}                           // Получение последний температуры датчика  если данные не достоверны или датчик не рабоает возврат -10000 (-100 градусов)
    __attribute__((always_inline)) inline uint32_t get_update(){return rtcSAM3X8.unixtime()-stime;}  // Получение времени прошедшего с последнего пакета
    __attribute__((always_inline)) inline boolean get_fUse(){return GETBIT(flags,fUse);}             // Получить  флаг разрешить использование датчика
    __attribute__((always_inline)) inline boolean get_fRule(){return GETBIT(flags,fRule);}           // Получить  флаг Правило использования датчика 0 - использование удаленного датчика 1- усреднение с основным
    __attribute__((always_inline)) inline int8_t get_link(){return link;}                            // Получение номера датчика (из массива датиков температуры),c которым связан удаленный датчик
    uint8_t get_num(){return num;}                             // Получение номера датчика, по нему осуществляется идентификация датачика о 0 до MAX_SENSOR_IP-1
    int8_t get_RSSI(){return RSSI;}                            // Получение Уровень сигнала датчика (-дБ)
    uint16_t get_VCC(){return VCC;}                            // Получение Уровень напряжениея питания датчика (мВ)
    uint32_t get_count(){return count;}                        // Получение Кольцевой счетчик пакетов с момента включения контрола
    uint32_t get_stime(){return stime;}                        // Получение Время получения последнего пакета
    IPAddress get_ip(){return ip;}                             // Адрес датчика
    void set_fUse(boolean b)  {if (b) SETBIT1(flags,fUse); else SETBIT0(flags,fUse); };   // Установить флаг использования
    void set_fRule(boolean b) {if (b) SETBIT1(flags,fRule); else SETBIT0(flags,fRule);}   // Установить флаг учреднения
    void set_link(int8_t b) {link=b;}                          // Установить флаг усреднения
    uint8_t *get_save_addr(void) { return (uint8_t *)&number; } // Адрес структуры сохранения
    uint16_t get_save_size(void) { return (byte*)&link - (byte*)&number + sizeof(link); } // Размер структуры сохранения
    void  	 after_load();                         				// Инициализация после загрузки

  private:
    int16_t Temp;                                              // Последние показания датчика
    int8_t num;                                                // Номер датчика (-1 датчик отсутсвует), по нему осуществляется идентификация датачика о 0 до MAX_SENSOR_IP-1
    int16_t RSSI;                                              // Уровень сигнала датчика (-дБ)
    uint16_t VCC;                                              // Уровень напряжениея питания датчика (мВ)
    uint32_t count;                                            // Кольцевой счетчик пакетов с момента включения контрола
    uint32_t stime;                                            // Время получения последнего пакета
    struct { // Save GROUP, firth number
    uint8_t number;												// номер
    uint8_t flags;                                             // флаги
    int8_t  link;                                               // привязка датчика (номер в массиве температурных датчиков, -1 не привязан)
    } __attribute__((packed));// Save Group end, last link
    IPAddress ip;                                              // Адрес датчика
};
#endif

#ifdef RADIO_SENSORS
#define DEBUG_RADIO
enum {
	RS_WAIT_HEADER = 0,
	RS_WAIT_DATA,
	RS_SEND_RESPONSE
};
struct radio_received_type {
	uint32_t serial_num;
	int16_t  Temp;		// Температура в сотых градуса
	uint16_t timecnt;	// Время чтения *TIME_READ_SENSOR
	uint8_t  battery;	// в десятых вольта
	uint8_t  RSSI;		// уровень сигнала = -(-120..-10)dBm, 255 - lost
};
uint8_t rs_serial_flag = RS_WAIT_HEADER; // enum
radio_received_type radio_received[RADIO_SENSORS_MAX];
uint8_t radio_received_num = 0;
uint32_t radio_hub_serial = 0;
uint16_t radio_timecnt = 0; // *TIME_READ_SENSOR
char Radio_RSSI_to_Level(uint8_t RSSI);
#endif

// класс датчик DS18B20 Температура хранится в сотых градуса в целых значениях
class sensorTemp
{
  public:
 //   sensorTemp();                                     // Конструктор
    void     initTemp(int sensor);                      // Инициализация на входе номер датчика
    int8_t   PrepareTemp();                             // запуск преобразования
    int8_t   Read();                                    // чтение данных, возвращает код ошибки, делает все преобразования
#ifdef TNTC
    int16_t  Read_NTC(uint16_t val);
#endif
    int16_t  get_minTemp(){return minTemp;}             // Минимальная темература датчика - нижняя граница диапазона, при выходе из него ошибка
    int16_t  get_maxTemp(){return maxTemp;}             // Максимальная темература датчика - верхняя граница диапазона, при выходе из него ошибка
    int16_t  set_maxTemp(int16_t t){return maxTemp=t;}  // Установить максимальную температуру датчика - верхняя граница диапазона, при выходе из него ошибка (изменение нужно для сальмонеллы)
    int16_t  get_errTemp(){ return errTemp;}            // Значение систематической ошибки датчика
    int8_t   set_errTemp(int16_t t);                    // Установить значение систематической ошибки датчика (поправка)
    int16_t  get_lastTemp(){return lastTemp;}           // Последнее считанное значение датчика - НЕ обработанное (без коррекции ошибки и усреднения)
    int16_t  get_Temp();                                // Получить значение температуры датчика с учетом удаленных датчиков!!! - это то что используется
    int16_t  get_rawTemp();                             // Получить значение температуры ПРОВОДНОГО датчика
    void	 set_Temp(int16_t t) { Temp = t; } 			// Установить температуру датчика
    int16_t  get_testTemp(){return testTemp;}           // Получить значение температуры датчика - в режиме теста
    int8_t   set_testTemp(int16_t t);                   // Установить значение температуры датчика - в режиме теста
    TEST_MODE get_testMode(){return  testMode;}         // Получить текущий режим работы
    void     set_testMode(TEST_MODE t){testMode=t;}     // Установить значение текущий режим работы
    
    void     set_address(byte *addr, byte bus);    		// Привязать адрес и номер шины
    uint8_t* get_address(){return address;}  			// Получить адрес датчика
    __attribute__((always_inline)) inline boolean get_present(){return GETBIT(flags,fPresent);} // Наличие датчика в текущей конфигурации
    uint8_t get_cfg_flags() { return SENSORTEMP[number]; } // Вернуть биты конфигурации (наличие, особенности отображения на веб страницах,)
    __attribute__((always_inline)) inline boolean get_fAddress(){ return GETBIT(flags,fAddress); } // Датчик привязан
    uint8_t get_bus(void);									// Шина
    __attribute__((always_inline)) inline boolean get_setup_flag(uint8_t bit){ return GETBIT(setup_flags, bit); }
    __attribute__((always_inline)) inline uint8_t get_setup_flags(void){ return setup_flags; }
    inline void set_setup_flag(uint8_t bit, uint8_t value){ setup_flags = (setup_flags & ~(1<<bit)) | ((value!=0)<<bit); }
    int8_t   get_radio_received_idx();					// Индекс массива полученных данных
    int8_t   get_lastErr(){return err;}                 // Получить последнюю ошибку
    uint32_t get_sumErrorRead(){return sumErrorRead;}   // Получить число ошибок чтения датчика с момента сброса НК
    void     Reset_Errors() { sumErrorRead = 0; }		// Сброс счетчика ошибок
    char*    get_note(){return note;}                   // Получить оисание датчика
    char*    get_name(){return name;}                   // Получить имя датчика
    uint8_t *get_save_addr(void) { return (uint8_t *)&number; } // Адрес структуры сохранения
    uint16_t get_save_size(void) { return (byte*)&setup_flags - (byte*)&number + sizeof(setup_flags); } // Размер структуры сохранения
    void  	 after_load();                         		// Инициализация после загрузки
    int8_t   inc_error(void);				   		    // Увеличить счетчик ошибок
    statChart Chart;                                    // Статистика по датчику
    
    #ifdef SENSOR_IP                                    // Удаленные устройства  #ifdef SENSOR_IP
     sensorIP *devIP;                                   // Ссылка на привязаный датчик (класс) если NULL привявязки нет
    #endif
    
  private:
   int16_t minTemp;                                     // минимальная разрешенная температура
   int16_t maxTemp;                                     // максимальная разрешенная температура
   int16_t lastTemp;                                    // последняя считанная температура с датчика
   int16_t Temp;                                        // температура датчика в сотых градуса (обработанная)
   TEST_MODE testMode;                                  // Значение режима тестирования
   int8_t  err;                                         // ошибка датчика (работа) при ошибке останов ТН
   uint8_t numErrorRead;                                // Счечик ошибок чтения датчика подряд если оно больше NUM_READ_TEMP_ERR генерация ошибки датчика
   uint32_t sumErrorRead;                               // Cуммарный счечик ошибок чтения датчика - число ошибок датчика с момента перегрузки
   uint8_t nGap;                                        // Счечик "разорванных" данных  - требуется для фильтрации помехи
   // Кольцевой буфер для усреднения
#if T_NUMSAMLES > 1
   int32_t sum;                                         // Накопленная сумма
   uint8_t last;                                        // указатель на последнее (самое старое) значение в буфере диапазон от 0 до T_NUMSAMLES-1
   int16_t t[T_NUMSAMLES];                              // буфер для усреднения показаний температуры
#endif
   byte    flags;                                       // флаги  датчика
   struct { // Save GROUP, firth number
   uint8_t number;  									// Номер датчика
   int16_t errTemp;                                     // статическая ошибка датчика
   int16_t testTemp;                                    // температура датчика в режиме тестирования
   byte    address[8];                                  // текущий адрес датчика
   	   	   	   	   	   	   	   	   	   	   	   	   	    // для радиодатчика, байты: 1 = tRadio, 4 - адрес
   uint8_t setup_flags;							    	// флаги настройки (TEMP_SETUP_FLAGS)
   } __attribute__((packed));// Save Group end, setup_flags
   char    *note;                                       // Описание датчика
   char    *name;                                        // Имя датчика

   void set_onewire_bus_type();							// Устанавливает по флагам тип шины
   deviceOneWire *busOneWire;                           // указатель на используемую шину
};

#endif

