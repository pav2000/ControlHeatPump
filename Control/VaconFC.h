/*
 * Частотный преобразователь Vacon 10
 * Адаптировано vad711, vad7@yahoo.com
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
//
#ifndef _VaconFC_h
#define _VaconFC_h

#define FC_VACON_NAME "Vacon 10"

// Регистры Vacon 10
// Чтение
#define FC_FREQ_OUT		1			// Выходная частота, поступающая на двигатель
#define FC_FREQ_REF		2			// Опорная частота для управления двигателем
#define FC_TEMP			8			// Температуры радиатора частотника, C
#define FC_TEMP_MOTOR	9			// Расчетная температура двигателя, %
#define FC_COMM_STATUS	808			// Состояние связи по шине Modbus. Формат: xx.yyy где xx = 0 – 64 (число сообщений об ошибках), yyy = 0 - 999 (число положительных сообщений)
#define FC_POWER_DAYS	828			// Наработка, дней
#define FC_POWER_HOURS	829			// Наработка, часов
#define FC_RUN_DAYS		840			// Счетчик работы двигателя, дней
#define FC_RUN_HOURS	841			// Счетчик работы двигателя, часов
#define FC_NUM_FAULTS	842			// Счетчик отказов

// Чтение / запись
#define FC_REMOTE_CTRL	172			// Выбор источника сигналов управления: 0 = Клемма ввода/вывода, 1 = Шина Fieldbus
#define FC_REMORE_REF	117			// Выбор опорной частоты: 1 = Предустановленная скорость 0-7, 2 = Клавиатура, 3 = Шина Fieldbus, 4 = AI1, 5 = AI2, 6 = ПИ-регулятор
#define FC_FREQ_MIN		101			// Мин. частота 0.01 Гц
#define FC_FREQ_MAX		102			// Макс. частота 0.01 Гц
#define FC_MOTOR_CTRL	600			// Режим управления двигателем: 0 = Управление частотой, 1 = Управление скоростью с разомкнутым контуром
#define FC_MOTOR_Uf		108			// Вид кривой U/f: 0 = Линейная, 1 = Квадратичная, 2 = Программируемая
#define FC_TORQUE_BOOST	109			// Форсировка момента: 0 = Запрещено, 1 = Разрешено
#define FC_SW_FREQ		601			// Частота коммутации (ШИМ), 0.1 кГц
#define FC_COMM_ST_RESET 815		// 1 = Сброс состояния связи FC_COMM_STATUS

// Системные:
// Чтение
#define FC_STATUS		2101		// Слово состояния FB
#define FC_SPEED		2103		// Фактическая скорость FB, 0.01 %
#define FC_FREQ			2104		// Выходная частота, +/- 0.01 Гц
#define FC_RPM			2105		// Скорость двигателя, +/- об/мин
#define FC_CURRENT		2106		// Ток двигателя, 0.01 A
#define FC_TORQUE		2107		// Крутящий момент двигателя от номинального, +/- 0.1 %
#define FC_POWER		2108		// Мощность двигателя от номинального, +/- 0.1 %
#define FC_VOLTAGE		2109		// Напряжение двигателя, 0.1 В
#define FC_VOLTATE_DC	2110		// Напряжение шины постоянного тока, 1 В
#define FC_ERROR		2111		// Код активного отказа

// Запись
#define FC_CONTROL		2001		// Слово управления FB
#define FC_SET_SPEED	2003		// Задание скорости FB, 0.01 %
#define FC_SOURCE		2004		// источник уставки, если P15.1=3
#define FC_FEEDBACK		2005		// источник обратной связи, если P15.4=2

// Биты
// FC_STATE
#define FC_S_RDY		0x01	// Привод готов
const char FC_S_RDY_str[]			= "Готов,";
#define FC_S_RUN		0x02	// Привод работает
const char FC_S_RUN_str[]			= "Работает,";
#define FC_S_DIR		0x04	// 0 - По часовой стрелке, 1 - Против часовой стрелки
const char FC_S_DIR_str[]			= "Против час.ст.,";
#define FC_S_FLT		0x08	// Действующий отказ
const char FC_S_FLT_str[]			= "Ошибка,";
#define FC_S_W			0x10	// Сигнал тревоги
const char FC_S_W_str[]				= "Тревога,";
#define FC_S_AREF		0x20	// 0 - Линейное изменение скорости, 1 - Задание скорости достигнуто
const char FC_S_AREF_str0[]			= "Изменение скорости,";
#define FC_S_Z			0x40	// 1 - Привод работает на нулевой скорости
const char FC_S_Z_str[]				= "Остановлен,";
// FC_CONTROL
#define FC_C_RUN		0x01	// 0 - Останов, 1 - Выполнение
#define FC_C_STOP		0
#define FC_C_DIR		0x02	// 0 - По часовой стрелке, 1 - Против часовой стрелки
#define FC_C_RST		0x04	// Сброс отказа

const uint8_t FC_Faults_code[] = {
	0,
	1,  // FC_ERR_Overcurrent
	2,  // FC_ERR_Overvoltage
	3,  // FC_ERR_Earth_fault
	8,  // FC_ERR_System_fault
	9,  // FC_ERR_Undervoltage
	11, // FC_ERR_Output_phase_fault
	13, // FC_ERR_FC_Undertemperature
	14, // FC_ERR_FC_Overtemperature
	15, // FC_ERR_Motor_stalled
	16, // FC_ERR_Motor_overtemperature
	17, // FC_ERR_Motor_underload
	22, // FC_ERR_EEPROM_checksum_fault
	25, // FC_ERR_MC_watchdog_fault
	27, // FC_ERR_Back_EMF_protection
	34, // FC_ERR_Internal_bus_comm
	35, // FC_ERR_Application_fault
	41, // FC_ERR_IGBT_Overtemperature
	50, // FC_ERR_Analog_input_wrong
	51, // FC_ERR_External_fault
	53, // FC_ERR_Fieldbus_fault
	55, // FC_ERR_Wrong_run_faul
	57  // FC_ERR_Idenfication_fault
};

const char *FC_Faults_str[] = {	"Ok", // нет ошибки
								"Overcurrent",
								"Overvoltage",
								"Earth fault",
								"System fault",
								"Undervoltage",
								"Output phase fault",
								"FC Undertemperature",
								"FC Overtemperature",
								"Motor stalled",
								"Motor overtemperature",
								"Motor underload",
								"EEPROM checksum fault",
								"MC watchdog fault",
								"Back EMF protection",
								"Internal bus comm",
								"Application fault",
								"IGBT Overtemperature",
								"Analog input wrong",
								"External fault",
								"Fieldbus fault",
								"Wrong run faul",
								"Idenfication fault",

								"Unknown"}; // sizeof(FC_Faults_code)+1

class devVaconFC
{
public:
  int8_t	initFC();                                 // Инициализация Частотника
  __attribute__((always_inline)) inline boolean get_present(){return GETBIT(flags,fFC);} // Наличие датчика в текущей конфигурации
  int8_t	get_err(){return err;}                    // Получить последню ошибку частотника
  uint16_t	get_numErr(){return numErr;}            // Получить число ошибок чтения
  char*		get_paramFC(TYPE_PARAM_FC p);              // Получить параметр инвертора в виде строки
  boolean	set_paramFC(TYPE_PARAM_FC p, float x);   // Установить параметр инвертора из строки
  
  // Управление по модбас Общее для всех частотников
  int16_t	get_targetFreq() {return FC;}                    // Получить целевую скорость в %
  int8_t	set_targetFreq(int16_t x,boolean show, int16_t _min, int16_t _max);// Установить целевую скорость в %, show - выводить сообщение или нет + границы
  uint16_t	get_power(){return power;}              // Получить текущую мощность в 0.1 кВт. В 100 ваттах еденица измерения
  uint16_t	get_current(){return current;}          // Получить текущий ток в 0.01А
  void		get_infoFC(char *buf);                   // Получить информацию о частотнике
  void		get_infoFC_status(char *buffer, uint16_t st); // Вывести в buffer строковый статус.
  boolean	reset_errorFC();                        // Сброс ошибок инвертора
  boolean	reset_FC();               		      // Сброс состояния связи модбас
  int16_t	CheckLinkStatus(void);				   // Получить Слово состояния FB, ERR_LINK_FC - ошибка связи
  int16_t	read_stateFC();                        // Текущее состояние инвертора
  int16_t	read_tempFC();                         // Tемпература радиатора
   
  int16_t	get_freqFC() {return FC_curr;}            // Получить текущую скорость в 0.01 %
  uint32_t	get_startTime(){return startCompressor;}// Получить время старта компрессора
  int8_t	get_readState();                          // Прочитать (внутренние переменные обновляются) состояние Инвертора, возвращает или ОК или ошибку
  int8_t	start_FC();                                // Команда ход на инвертор (целевая скорость выставляется)
  int8_t	stop_FC();                                 // Команда стоп на инвертор
  boolean	isfAuto(){return GETBIT(flags,fAuto);}   // проверка на режим старт-стопа false стоит флаг стартстопа
  boolean	isfOnOff(){return GETBIT(flags,fOnOff);} // получить состояние инвертора вкл или выкл
 
  void		check_blockFC();                          // Установить запрет на использование инвертора
  boolean	get_blockFC(); 						    // Получить флаг блокировки инвертора

  const char *get_fault_str(uint8_t fault); // Возвращает название ошибки
  
  // Аналоговое управление
  int16_t get_DAC(){return dac;};                  // Получить установленное значеие ЦАП
  int16_t get_level0(){return level0;}             // Получить Отсчеты ЦАП соответсвующие 0   скорости
  int16_t get_level100(){return level100;}         // Получить Отсчеты ЦАП соответсвующие максимальной скорости
  int16_t get_levelOff(){return levelOff;}         // Получить Минимальная скорость при котором частотник отключается (ограничение минимальной мощности)
  int8_t  set_level0(int16_t x);                    // Установить Отсчеты ЦАП соответсвующие 0   мощности
  int8_t  set_level100(int16_t x);                  // Установить Отсчеты ЦАП соответсвующие 100 мощности
  int8_t  set_levelOff(int16_t x);                  // Установить Минимальная мощность при котором частотник отключается (ограничение минимальной скорости)
  uint8_t get_pinA(){return  pin;}                 // Ножка куда прицеплено FC
   
  // Сервис
  TEST_MODE	get_testMode() {return testMode;}      // Получить текущий режим работы
  void		set_testMode(TEST_MODE t);			   // Установить значение текущий режим работы
  char *	get_note(){return  note;}                // Получить описание
  char *	get_name(){return  name;}                // Получить имя
  int32_t	save(int32_t adr);                       // Записать настройки в eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
  int32_t	load(int32_t adr);                       // Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
  int32_t	loadFromBuf(int32_t adr, byte *buf);      // Считать настройки из буфера на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
  uint16_t	get_crc16(uint16_t crc);                // Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая

  statChart ChartFC;                               // График по скорости
  statChart ChartPower;                            // График по мощности
  statChart ChartCurrent;                          // График по току инвертора
 
private:
  int8_t  err;                                     // ошибка частотника (работа) при ошибке останов ТН
  uint16_t numErr;                                 // число ошибок чтение по модбасу
  uint8_t number_err;                              // Число ошибок связи при превышении FC_NUM_READ блокировка инвертора
  byte flags;                                      // флаги  0 - наличие Частоника,
   // Управление по 485
  uint16_t FC;                                     // Целевая скорость инвертора в 0.01 %
  uint16_t FC_curr;                                // Чтение: текущая скорость инвертора в 0.01 %
  uint16_t power;                                  // Чтение: Текущая мощность инвертора  в 100 ватных единицах (3 это 300 вт)
  uint16_t current;                                // Чтение: Текущий ток инвертора  в 0.01 Ампера единицах
  
  int16_t state;                                   // Чтение: Состояние ПЧ регистр FC_STATUS
  uint16_t minFC;                                  // Минимальная скорость инвертора в 0.01 %
  uint16_t maxFC;                                  // Максимальная скорость инвертора в 0.01 %
  uint32_t startCompressor;                        // время старта компрессора
        
  // Аналоговое управление
  int16_t dac;                                     // Текущее значение ЦАП
  int16_t level0;                                  // !save! Отсчеты ЦАП соответсвующие 0   скорость
  int16_t level100;                                // !save! Отсчеты ЦАП соответсвующие максимальной скорости
  int16_t levelOff;                                // !save! Минимальная мощность при котором частотник отключается (ограничение минимальной мощности)
  uint8_t pin;                                     // Ножка куда прицеплено FC
  
  TEST_MODE testMode;                              // Значение режима тестирования
  char *note;                                      // Описание
  char *name;                                      // Имя

  // Функции работы с Modbus
#ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
  int16_t  read_0x03_16(uint16_t cmd);             // Функция Modbus 0х03 прочитать 2 байта
  uint32_t read_0x03_32(uint16_t cmd);             // Функция Modbus 0х03 прочитать 4 байта
  int8_t   write_0x06_16(uint16_t cmd, uint16_t data);// Запись данных (2 байта) в регистр cmd возвращает код ошибки

#endif
 };

#endif
