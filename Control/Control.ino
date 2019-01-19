/*
 * Copyright (c) 2016-2018 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav;
 * by vad711, vad7@yahoo.com
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
// Последние версии
// https://github.com/pav2000/ControlHeatPump проект на гитхабе (рабочие сборки)
// https://github.com/vad7/ControlHeatPump - последняя стабильная версия
// http://77.50.254.24:25402/ последняя версия демо
// http://77.50.254.24:25402/mob/index.html мобильная морда демо
// Архивные ссылки
// http://pumps.tk/webtn.zip  последняя вебморда
// http://pumps.tk/v09/ демо версия (старая)
// http://pumps.tk/v09/mob мобильная демо версия (старая)
// https://github.com/vad7/Arduino-DUE-WireSam  библиотеки доработанные vad711 при релизе добавляются в архив

#include "Arduino.h"
#include "Constant.h"                       // Должен быть первым !!!! кое что берется в стандартных либах для настройки либ, Config.h входит в него

#include <WireSam.h>                        // vad711 доработанная библиотека https://github.com/vad7/Arduino-DUE-WireSam
#include <DS3232.h>                         // vad711 Работа с часами i2c - используются при выключении питания. модифицированная https://github.com/vad7/Arduino-DUE-WireSam
#include <extEEPROM.h>                      // vad711 Работа с eeprom и fram i2c библиотека доработана для DUE!!! https://github.com/vad7/Arduino-DUE-WireSam

#include <SPI.h>                            // SPI - стандартная
#include "SdFat.h"                          // SdFat - модифицированная
#include "FreeRTOS_ARM.h"                   // модифицированная
#include <rtc_clock.h>                      // работа со встроенными часами  Это основные часы!!!
#include <ModbusMaster.h>                   // Используется МОДИФИЦИРОВАННАЯ для омрона мх2 либа ModbusMaster https://github.com/4-20ma/ModbusMaster

// Глубоко переделанные библиотеки
#include <Ethernet.h>                       // Ethernet - модифицированная
#include <Dns.h>                            // DNS - модифицированная
#include <SerialFlash.h>                    // Либа по работе с spi флешом как диском

#include "Hardware.h"
#include "HeatPump.h"
#include "Nextion.h"
#include "Message.h"
#include "Information.h"
#include "Statistics.h"

void vUpdateStepperEEV(void *);
#include "StepMotor.h"

// Мютексы блокираторы железа
SemaphoreHandle_t xModbusSemaphore;                 // Семафор Modbus, инвертор запас на счетчик
SemaphoreHandle_t xWebThreadSemaphore;              // Семафор потоки вебсервера,  деление сетевой карты
SemaphoreHandle_t xI2CSemaphore;                    // Семафор шины I2C, часы, память, мастер OneWire
SemaphoreHandle_t xSPISemaphore;                    // Семафор шины SPI  сетевая карта, память. SD карта // пока не используется
SemaphoreHandle_t xLoadingWebSemaphore;             // Семафор загрузки веб морды в spi память
uint16_t lastErrorFreeRtosCode;                     // код последней ошибки операционки нужен для отладки
uint32_t startSupcStatusReg;                        // Состояние при старте SUPC Supply Controller Status Register - проверяем что с питание


#if defined(W5500_ETHERNET_SHIELD)                  // Задание имени чипа для вывода сообщений
  const char nameWiznet[] ={"W5500"};
#elif defined(W5200_ETHERNET_SHIELD)
  const char nameWiznet[] ={"W5200"};
#else
  const char nameWiznet[] ={"W5100"};
#endif

// Глобальные переменные
extern boolean set_time_NTP(void);   
EthernetServer server1(80);                         // сервер
EthernetUDP Udp;                                    // Для NTP сервера
EthernetClient ethClient(W5200_SOCK_SYS);           // для MQTT

#ifdef RADIO_SENSORS
void check_radio_sensors(void);
void radio_sensor_send(char *cmd);
#endif

#ifdef MQTT                                 // признак использования MQTT
#include <PubSubClient.h>                   // передаланная под многозадачность  http://knolleary.net
PubSubClient w5200_MQTT(ethClient);  	    // клиент MQTT
#endif

// I2C eeprom Размер в килобитах, число чипов, страница в байтах, адрес на шине, тип памяти:
extEEPROM eepromI2C(I2C_SIZE_EEPROM,I2C_MEMORY_TOTAL/I2C_SIZE_EEPROM,I2C_PAGE_EEPROM,I2C_ADR_EEPROM,I2C_FRAM_MEMORY);
//RTC_clock rtcSAM3X8(RC);                                               // Внутренние часы, используется внутренний RC генератор
RTC_clock rtcSAM3X8(XTAL);                                               // Внутренние часы, используется часовой кварц
DS3232  rtcI2C;                                                          // Часы 3231 на шине I2C
static Journal  journal;                                                 // системный журнал, отдельно т.к. должен инициализоваться с начала старта
static HeatPump HP;                                                      // Класс тепловой насос (в констукторе плохо проходит инициализация пинов????)
static devModbus Modbus;                                                 // Класс модбас - управление инвертором
SdFat card;                                                              // Карта памяти
#ifdef NEXTION   
  Nextion myNextion;                                                     // Дисплей
#endif


// Структура для хранения одного сокета, нужна для организации многопотоковой обработки
#define fABORT_SOCK   0                     // флаг прекращения передачи (произошел сброс сети)
#define fUser         1                     // Признак идентификации как обычный пользователь (нужно подменить файл меню)

struct type_socketData
  {
    EthernetClient client;                  // Клиент для работы с потоком
    byte inBuf[W5200_MAX_LEN];              // буфер для работы с w5200 (входной запрос)
    char *inPtr;                            // указатель в входном буфере с именем файла или запросом (копирование не делаем)
    char outBuf[3*W5200_MAX_LEN];           // буфер для формирования ответа на запрос и чтение с карты
    int8_t sock;                            // сокет потока -1 свободный сокет
    uint8_t flags;                          // Флаги состояния потока
    uint16_t http_req_type;                 // Тип запроса
  };
static type_socketData Socket[W5200_THREARD];   // Требует много памяти 4*W5200_MAX_LEN*W5200_THREARD=24 кб

// Установка вачдога таймера вариант vad711 - адаптация для DUE 1.6.11
// WDT_TIME период Watchdog таймера секунды но не более 16 секунд!!! ЕСЛИ WDT_TIME=0 то Watchdog будет отключен!!!
volatile unsigned long ulHighFrequencyTimerTicks;   // vad711 - адаптация для DUE 1.6.11
void watchdogSetup(void)
{
#if WDT_TIME == 0
	watchdogDisable();
#else 
	watchdogEnable(WDT_TIME * 1000);
#endif
}

__attribute__((always_inline)) inline void _delay(int t) // Функция задержки (мсек) в зависимости от шедулера задач FreeRtos
{
  if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) vTaskDelay(t/portTICK_PERIOD_MS);
  else delay(t);
}

// Захватить семафор с проверкой, что шедуллер работает
BaseType_t SemaphoreTake(QueueHandle_t xSemaphore, TickType_t xBlockTime)
{
	if(xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) return pdTRUE;
	else {
		for(;;) {
			if(xSemaphoreTake(xSemaphore, 0) == pdTRUE) return pdTRUE;
			if(!xBlockTime--) break;
			vTaskDelay(1/portTICK_PERIOD_MS);
		}
		return pdFALSE;
	}
}

// Освободить семафор с проверкой, что шедуллер работает
inline void SemaphoreGive(QueueHandle_t xSemaphore)
{
	if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) xSemaphoreGive(xSemaphore);
}

// Остановить шедулер задач, возврат 1, если получилось
uint8_t TaskSuspendAll(void) {
	if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
		vTaskSuspendAll(); // запрет других задач
		return 1;
	}
	return 0;
}

// Установка критической ошибки для класса ТН вызывает останов ТН
// Возвращает ошибку останова ТН
int8_t set_Error(int8_t _err, char *nam)
{
	if(HP.dRelay[RCOMP].get_Relay() || HP.dFC.isfOnOff())    // СРАЗУ Если компрессор включен, выключить  ГЛАВНАЯ ЗАЩИТА
	{ // Выключить компрессор для обоих вариантов
		journal.jprintf("$Compressor protection: ");
		if(HP.dFC.get_present()) HP.dFC.stop_FC(); else HP.dRelay[RCOMP].set_OFF();
	}
	//   if ((HP.get_State()==pOFF_HP)&&(HP.error!=OK)) return HP.error;  // Если ТН НЕ работает, не стартует не останавливается и уже есть ошибка то останавливать нечего и выключать нечего выходим - ошибка не обновляется - важна ПЕРВАЯ ошибка

	if(HP.error != OK) return HP.error;                              // Ошибка уже есть выходим
	//   if((_err!=HP.error)||(strcmp(nam,HP.source_error)!=0))     // Если приходит ошибка отличная от предыдущей то запоминаем
	{
		HP.error = _err;
		strcpy(HP.source_error, nam);
		strcpy(HP.note_error, NowTimeToStr());       // Cтереть всю строку и поставить время
		strcat(HP.note_error, " ");
		strcat(HP.note_error, nam);                  // Имя кто сгенерировал ошибку
		strcat(HP.note_error, ": ");
		strcat(HP.note_error, noteError[abs(_err)]); // Описание ошибки
		journal.jprintf(pP_TIME, "$ERROR source: %s, code: %d\n", nam, _err); //journal.jprintf(", code: %d\n",_err);
		if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) HP.save_DumpJournal(true); // вывод отладочной информации для начала  если запущена freeRTOS
		HP.message.setMessage(pMESSAGE_ERROR, HP.note_error, 0);    // сформировать уведомление об ошибке
	}
	// Сюда ставить надо останов ТН !!!!!!!!!!!!!!!!!!!!!
	if(HP.get_State() != pOFF_HP)    // Насос не ВЫКЛЮЧЕН есть что выключать
	{
		if(HP.get_nStart() == 0) HP.sendCommand(pSTOP); // Послать команду на останов ТН  если нет попыток повторного пуск
		else { // сюда ставить повторные пуски ТН при ошибке.
			if(HP.num_repeat < HP.get_nStart())                    // есть еще попытки
			{
				HP.sendCommand(pREPEAT);                     // Повторный пуск ТН
			} else HP.sendCommand(pSTOP);                         // Послать команду на останов ТН  БЕЗ ПОПЫТОК ПУСКА
		}
		if(HP.get_State() == pSTARTING_HP) { // Ошибка во время старта
			HP.set_HP_error_state();
		}
	}
	return HP.error;
}

void setup() {
// 1. Инициализация SPI
// Баг разводки дуе (вероятность). Есть проблема с инициализацией spi.  Ручками прописываем
// https://groups.google.com/a/arduino.cc/forum/#!topic/developers/0PUzlnr7948
// http://forum.arduino.cc/index.php?topic=243778.0;nowap
  pinMode(PIN_SPI_SS0,INPUT_PULLUP);          // Eth Pin 77
  pinMode(PIN_SPI_SS1,INPUT_PULLUP);          // SD Pin  87
  pinMode(PIN_SPI_CS_SD,INPUT_PULLUP);        // сигнал CS управление SD картой
  pinMode(PIN_SPI_CS_W5XXX,INPUT_PULLUP);     // сигнал CS управление сетевым чипом

#ifdef SPI_FLASH
  pinMode(PIN_SPI_CS_FLASH,INPUT_PULLUP);     // сигнал CS управление чипом флеш памяти
  pinMode(PIN_SPI_CS_FLASH,OUTPUT);           // сигнал CS управление чипом флеш памяти (ВРЕМЕННО, пока нет реализации)
#endif
  SPI_switchAllOFF();                          // Выключить все устройства на SPI

  #ifdef POWER_CONTROL                       // Включение питания платы если необходимо НАДП здесь, иначе I2C память рабоать не будет
    pinMode(PIN_POWER_ON,OUTPUT);  
    digitalWriteDirect(PIN_POWER_ON, LOW);
  #endif
  
// Борьба с зависшими устройствами на шине  I2C (в первую очередь часы) неудачный сброс
  Recover_I2C_bus();
  
// 2. Инициализация журнала и в нем последовательный порт
  journal.Init();

  #ifdef DEMO
     journal.jprintf("DEMO - DEMO - DEMO - DEMO - DEMO - DEMO - DEMO\n"); 
  #endif 
  #ifdef TEST_BOARD
     journal.jprintf("\n---> TEST BOARD!!!\n\n");
  #endif
  journal.jprintf("Vesion firmware: %s\n",VERSION); 
  showID();                                                                  // информация о чипе
  journal.jprintf("Chip ID SAM3X8E: %s\n",getIDchip((char*)Socket[0].inBuf));// информация об серийном номере чипа
  if(GPBR->SYS_GPBR[0] & 0x80000000) GPBR->SYS_GPBR[0] = 0; else GPBR->SYS_GPBR[0] |= 0x80000000; // очистка старой причины
  lastErrorFreeRtosCode = GPBR->SYS_GPBR[0] & 0x7FFFFFFF;         // Сохранение кода ошибки при страте (причину перегрузки)
  journal.jprintf("Last reason for reset SAM3x: %s\n", ResetCause());
  journal.jprintf("Last FreeRTOS task + error: 0x%04x\n", lastErrorFreeRtosCode);

  #ifdef PIN_LED1                            // Включение (точнее индикация) питания платы если необходимо
    pinMode(PIN_LED1,OUTPUT);  
    digitalWriteDirect(PIN_LED1, HIGH);
  #endif
  #ifdef POWER_CONTROL
    journal.jprintf("Power +5V, +3.3V on board: ON\n"); 
  #endif

//  #ifdef POWER_CONTROL
  SupplyMonitorON(SUPC_SMMR_SMTH_3_0V);           // включение монитора питания
//  #endif
   
  #ifdef DRV_EEV_L9333                     // Контроль за работой драйвера ЭРВ
    pinMode(PIN_STEP_DIAG,INPUT_PULLUP); 
    journal.jprintf("Control EEV driver L9333: ON \n"); 
  #else
    journal.jprintf("Control EEV driver no support \n"); 
  #endif

#ifdef RADIO_SENSORS
  RADIO_SENSORS_SERIAL.begin(RADIO_SENSORS_PSPEED, RADIO_SENSORS_PCONFIG);
#endif

// 3. Инициализация и проверка шины i2c
   journal.jprintf("1. Setting and checking I2C devices . . .\n");
 
    uint8_t eepStatus=0;
	#ifdef I2C_EEPROM_64KB
	if(journal.get_err()) { // I2C память и журнал в ней уже пытались инициализировать
	#endif
	   Wire.begin();
	   for(uint8_t i=0; i<I2C_NUM_INIT; i++ )
	   {
		   if ((eepStatus=eepromI2C.begin(I2C_SPEED))>0)    // переходим на I2C_SPEED и пытаемся инициализировать
		   {
			   journal.jprintf("$ERROR - I2C mem failed, status = %d\n", eepStatus);
	        #ifdef POWER_CONTROL
			   digitalWriteDirect(PIN_POWER_ON, HIGH);
			   digitalWriteDirect(PIN_LED1, LOW);
			   _delay(2000);
			   digitalWriteDirect(PIN_POWER_ON, LOW);
			   digitalWriteDirect(PIN_LED1, HIGH);
			   _delay(500);
			   Wire.begin();
	        #else
		       Wire.begin();
		       _delay(500);
	        #endif
		       WDT_Restart(WDT);                       // Сбросить вачдог
		   }  else break;   // Все хорошо
	   } // for
	#ifdef I2C_EEPROM_64KB
	 }
	#endif
	if(eepStatus)  // если I2C память не инициализирована, делаем попытку запустится на малой частоте один раз!
	{
	   if((eepStatus=eepromI2C.begin(twiClock100kHz))>0) {
		   journal.jprintf("$ERROR - I2C mem init failed on speed %d kHz, status = %d\n",twiClock100kHz/1000,eepStatus);
		   eepromI2C.begin(I2C_SPEED);
		   goto x_I2C_init_std_message;
	   } else journal.jprintf("I2C bus low speed, init on %d kHz - OK\n",twiClock100kHz/1000);
	} else {
x_I2C_init_std_message:
	   journal.jprintf("I2C init on %d kHz - OK\n",I2C_SPEED/1000);
	}

     // Сканирование шины i2c
//    if (eepStatus==0)   // есть инициализация
    {
        byte error, address;
        const byte start_address = 8;       // lower addresses are reserved to prevent conflicts with other protocols
        const byte end_address = 119;       // higher addresses unlock other modes, like 10-bit addressing
      
        for(address = start_address; address < end_address; address++ )
         {
              #ifdef ONEWIRE_DS2482         // если есть мост
              if(address == I2C_ADR_DS2482) {
                  error = OneWireBus.Init();
			    #ifdef ONEWIRE_DS2482_SECOND
              } else if(address == I2C_ADR_DS2482_2) {
                  error = OneWireBus2.Init();
		        #endif
				#ifdef ONEWIRE_DS2482_THIRD
              } else if(address == I2C_ADR_DS2482_3) {
                  error = OneWireBus3.Init();
				#endif
				#ifdef ONEWIRE_DS2482_FOURTH
              } else if(address == I2C_ADR_DS2482_4) {
                  error = OneWireBus4.Init();
				#endif
              } else
			  #endif
              {
            	  Wire.beginTransmission(address); error = Wire.endTransmission();
              }
              if(error==0)
              {
               journal.jprintf("I2C device found at address %s",byteToHex(address));
               switch (address)
                    {
					#ifdef ONEWIRE_DS2482
               	   	case I2C_ADR_DS2482_4:
               	   	case I2C_ADR_DS2482_3:
               	   	case I2C_ADR_DS2482_2:
                    case I2C_ADR_DS2482:  		journal.jprintf(" - OneWire DS2482-100 bus: %d%s\n", address - I2C_ADR_DS2482 + 1, (ONEWIRE_2WAY & (1<<(address - I2C_ADR_DS2482))) ? " (2W)" : ""); break;
					#endif
                    #if I2C_FRAM_MEMORY == 1
                    	case I2C_ADR_EEPROM:	journal.jprintf(" - FRAM FM24V%02d\n", I2C_MEMORY_TOTAL*10/1024); break;
						#if I2C_MEMORY_TOTAL != I2C_SIZE_EEPROM
                    	case I2C_ADR_EEPROM+1:	journal.jprintf(" - FRAM second 64k page\n"); break;
						#endif
					#else
                    	case I2C_ADR_EEPROM:	journal.jprintf(" - EEPROM AT24C%d\n", I2C_SIZE_EEPROM);break; // 0x50 возможны варианты
						#if I2C_MEMORY_TOTAL != I2C_SIZE_EEPROM
                    	case I2C_ADR_EEPROM+1:	journal.jprintf(" - EEPROM second 64k page\n"); break;
						#endif
					#endif
                    case I2C_ADR_RTC   :		journal.jprintf(" - RTC DS3231\n"); break; // 0x68
                    default            :		journal.jprintf(" - Unknow\n"); break; // не определенный тип
                    }
              _delay(100);      
              }
          //    else journal.jprintf("I2C device bad endTransmission at address %s code %d",byteToHex(address), error);   
          } // for
    } //  (eepStatus==0) 

  #ifndef ONEWIRE_DS2482         // если нет моста
  if(OneWireBus.Init()) journal.jprintf("Error init 1-Wire: %d\n", OneWireBus.GetLastErr());
  else journal.jprintf("1-Wire init Ok\n");
  #endif

#ifdef RADIO_SENSORS
  check_radio_sensors();
  if(rs_serial_flag == RS_SEND_RESPONSE) {
	  _delay(5);
	  check_radio_sensors();
  }
#endif

// 4. Инициализировать основной класс
  journal.jprintf("2. Init %s main class . . .\n",(char*)nameHeatPump);
  HP.initHeatPump();                                               // Основной класс

// 5. Установка сервисных пинов
   journal.jprintf("3. Read safe Network key . . .\n");
   pinMode(PIN_KEY1, INPUT);               // Кнопка 1, Нажатие при включении - режим safeNetwork (настрока сети по умолчанию 192.168.0.177  шлюз 192.168.0.1, не спрашивает пароль на вход в веб морду)
   HP.safeNetwork=!digitalReadDirect(PIN_KEY1); 
   if (HP.safeNetwork)  journal.jprintf("Mode safeNetwork ON \n"); else journal.jprintf("Mode safeNetwork OFF \n"); 
  
   pinMode(PIN_BEEP, OUTPUT);              // Выход на пищалку
   pinMode(PIN_LED_OK, OUTPUT);            // Выход на светодиод мигает 0.5 герца - ОК  с частотой 2 герца ошибка
   digitalWriteDirect(PIN_BEEP,LOW);       // Выключить пищалку
   digitalWriteDirect(PIN_LED_OK,HIGH);    // Выключить светодиод

// 7. Инициализация СД карты и запоминание результата 3 попытки
   journal.jprintf("4. Init SD card . . .\n");
   HP.set_fSD(initSD());
   WDT_Restart(WDT);                          // Сбросить вачдог  иногда карта долго инициализируется
   digitalWriteDirect(PIN_LED_OK,LOW);        // Включить светодиод - признак того что сд карта инициализирована
   //_delay(100);

// 8. Инициализация spi флеш диска
#ifdef SPI_FLASH
  journal.jprintf("5. Init SPI flash disk . . .\n");
  HP.set_fSPIFlash(initSpiDisk(true));  // проверка диска с выводом инфо
#else
  journal.jprintf("5. No SPI flash in config.\n");
#endif

// 9. Чтение ЕЕПРОМ
   journal.jprintf("6. Load data from I2C memory . . .\n");
  if(HP.load_motoHour()==ERR_HEADER2_EEPROM)           // Загрузить счетчики ТН,
  {
	  journal.jprintf("I2C memory is empty, a default settings will be used!\n");
	  HP.save_motoHour();
  } else {
	  HP.load((uint8_t *)Socket[0].outBuf, 0);      // Загрузить настройки ТН
	  HP.Prof.load(HP.Option.numProf);				// Загрузка текущего профиля
  }

  HP.Schdlr.load();							// Загрузка настроек расписания

  start_ADC(); // после инициализации HP
  journal.jprintf("7. Start read ADC sensors\n");
  //journal.jprintf(" Mask ADC_IMR: 0x%08x\n",ADC->ADC_IMR);

  // обновить хеш для пользователей
  HP.set_hashUser();
  HP.set_hashAdmin();
  journal.jprintf(" Web interface source: ");
        switch (HP.get_SourceWeb())
        {
        case pMIN_WEB:   journal.jprintf("internal\n"); break;
        case pSD_WEB:    journal.jprintf("SD card\n"); break;
        case pFLASH_WEB: journal.jprintf("SPI Flash\n"); break;
        default:         journal.jprintf("unknown\n"); break;
        }

// 10. Сетевые настройки
   journal.jprintf("8. Setting Network . . .\n");
   if(initW5200(true)) {   // Инициализация сети с выводом инфы в консоль
	   W5100.getMACAddress((uint8_t *)Socket[0].outBuf);
   	   journal.jprintf(" MAC: %s\n", MAC2String((uint8_t *)Socket[0].outBuf));
   }
   digitalWriteDirect(PIN_BEEP,LOW);          // Выключить пищалку
 
// 11. Разбираемся со всеми часами и синхронизацией
   journal.jprintf("8. Setting time and clock . . .\n");
   set_time();        
   
 // 12. Инициалазация уведомлений
   journal.jprintf("10. Message update IP from DNS . . .\n");
   HP.message.dnsUpdateStart(); 
   
 // 13. Инициалазация MQTT
    #ifdef MQTT  
      journal.jprintf("11. Client MQTT update IP from DNS . . .\n");
      HP.clMQTT.dnsUpdateStart();
    #else
      journal.jprintf("11. Client MQTT disabled by config\n");
    #endif 

  // 14. Инициалазация Statistics
   journal.jprintf("12. Statistics ");
   if(HP.get_fSD()) {
	   journal.jprintf("writing on SD card\n");
	   Stats.Init();             // Инициализовать статистику
   } else journal.jprintf("not available\n");

   int8_t _profile = HP.Schdlr.calc_active_profile();
   if(_profile > SCHDLR_Profile_off && _profile != HP.Prof.get_idProfile()) {
	   HP.Prof.load(_profile);
	   HP.set_profile();
	   journal.jprintf("Profile changed to #%d\n", _profile);
   }

  if(HP.get_SaveON()==0)  HP.set_HP_OFF();    // Сбросить флаг включение ТН если стоит соответсвующий флаг в опциях
  journal.jprintf("13. Delayed start %s: ",(char*)nameHeatPump); if(HP.get_HP_ON()) journal.jprintf("YES\n"); else journal.jprintf("NO\n");

  #ifdef NEXTION   
    journal.jprintf("14. Nextion display - ");
    if(GETBIT(HP.Option.flags, fNextion)) {
    	if(myNextion.init()) journal.jprintf("Ok\n");
    } else {
    	journal.jprintf("Disabled\n");
    }
  #else
    journal.jprintf("14. Nextion display is absent in config\n");
  #endif

  #ifdef TEST_BOARD
  // Scan oneWire - TEST.
  //HP.scan_OneWire(Socket[0].outBuf);
  #endif

  // Создание задач FreeRTOS  ----------------------
    journal.jprintf("15. Create tasks FreeRTOS . . .\n");
HP.mRTOS=236;  //расчет памяти для задач 236 - размер данных шедуллера, каждая задача требует 64 байта+ стек (он в словах!!)
HP.mRTOS=HP.mRTOS+64+4*configMINIMAL_STACK_SIZE;  // задача бездействия
//HP.mRTOS=HP.mRTOS+4*configTIMER_TASK_STACK_DEPTH;  // программные таймера (их теперь нет)

// ПРИОРИТЕТ 4 Высший приоритет датчики читаются всегда и шаговик ЭРВ всегда шагает если нужно
if (xTaskCreate(vReadSensor,"ReadSensor",150,NULL,4,&HP.xHandleReadSensor)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)    set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS);
HP.mRTOS=HP.mRTOS+64+4*150;// 200, до обрезки стеков было 300

#ifdef EEV_DEF
  if (xTaskCreate(vUpdateStepperEEV,"StepperEEV",50,NULL,4,&HP.dEEV.stepperEEV.xHandleStepperEEV)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)  set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS);
  HP.mRTOS=HP.mRTOS+64+4*50; // 100, 150, до обрезки стеков было 200
  vTaskSuspend(HP.dEEV.stepperEEV.xHandleStepperEEV);                                 // Остановить задачу
  HP.dEEV.stepperEEV.xCommandQueue = xQueueCreate( EEV_QUEUE, sizeof( int ) );  // Создать очередь комманд для ЭРВ
#endif

// ПРИОРИТЕТ 3 Очень высокий приоритет Выполнение команд управления (разбор очереди комманд) - должен быть выше чем задачи обновления ТН и ЭРВ
if(xTaskCreate(vUpdateCommand,"CommandHP",STACK_vUpdateCommand,NULL,3,&HP.xHandleUpdateCommand)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)     set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS);
HP.mRTOS=HP.mRTOS+64+4*STACK_vUpdateCommand;// 200, до обрезки стеков было 300
vTaskSuspend(HP.xHandleUpdateCommand);      // Остановить задачу разбор очереди комнад


// ПРИОРИТЕТ 2 высокий - это управление ТН управление ЭРВ, сервис
if(xTaskCreate(vServiceHP, "ServiceHP", 200, NULL, 2, &HP.xHandleSericeHP)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS);
HP.mRTOS=HP.mRTOS+64+4*STACK_vUpdateCommand;// 200, до обрезки стеков было 300

vSemaphoreCreateBinary(HP.xCommandSemaphore);                       // Создание семафора
if (HP.xCommandSemaphore==NULL) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
                    
if (xTaskCreate(vUpdate,"UpdateHP",170,NULL,2,&HP.xHandleUpdate)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)    set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS);
HP.mRTOS=HP.mRTOS+64+4*170;// 200, до обрезки стеков было 350
vTaskSuspend(HP.xHandleUpdate);                                 // Остановить задачу обновление ТН
HP.Task_vUpdate_run = false;

#ifdef EEV_DEF
  if (xTaskCreate(vUpdateEEV,"UpdateEEV",120,NULL,2,&HP.xHandleUpdateEEV)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)     set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS);
  HP.mRTOS=HP.mRTOS+64+4*120;  //до обрезки стеков было 200
  vTaskSuspend(HP.xHandleUpdateEEV);                              // Остановить задачу обновление EEV
#endif  

// ПРИОРИТЕТ 1 средний - обслуживание вебморды в несколько потоков и дисплея Nextion
// ВНИМАНИЕ первый поток должен иметь больший стек для обработки фоновых сетевых задач
#if    W5200_THREARD < 2 
  if ( xTaskCreate(vWeb0,"Web0", STACK_vWebX+20,NULL,1,&HP.xHandleUpdateWeb0)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
  HP.mRTOS=HP.mRTOS+64+4*(STACK_vWebX+20);
#elif  W5200_THREARD < 3
  if ( xTaskCreate(vWeb0,"Web0", STACK_vWebX,NULL,1,&HP.xHandleUpdateWeb0)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS);
  HP.mRTOS=HP.mRTOS+64+4*STACK_vWebX;
  if ( xTaskCreate(vWeb1,"Web1", STACK_vWebX,NULL,1,&HP.xHandleUpdateWeb1)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
  HP.mRTOS=HP.mRTOS+64+4*STACK_vWebX;
#elif  W5200_THREARD < 4
  if ( xTaskCreate(vWeb0,"Web0", STACK_vWebX,NULL,1,&HP.xHandleUpdateWeb0)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS);
  HP.mRTOS=HP.mRTOS+64+4*STACK_vWebX;
  if ( xTaskCreate(vWeb1,"Web1", STACK_vWebX,NULL,1,&HP.xHandleUpdateWeb1)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
  HP.mRTOS=HP.mRTOS+64+4*STACK_vWebX;
  if ( xTaskCreate(vWeb2,"Web2", STACK_vWebX,NULL,1,&HP.xHandleUpdateWeb2)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
  HP.mRTOS=HP.mRTOS+64+4*STACK_vWebX;
#else
  if ( xTaskCreate(vWeb0,"Web0", STACK_vWebX,NULL,1,&HP.xHandleUpdateWeb0)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS);
  HP.mRTOS=HP.mRTOS+64+4*STACK_vWebX;
  if ( xTaskCreate(vWeb1,"Web1", STACK_vWebX,NULL,1,&HP.xHandleUpdateWeb1)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
  HP.mRTOS=HP.mRTOS+64+4*STACK_vWebX;
  if ( xTaskCreate(vWeb2,"Web2", STACK_vWebX,NULL,1,&HP.xHandleUpdateWeb2)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
  HP.mRTOS=HP.mRTOS+64+4*STACK_vWebX;
  if ( xTaskCreate(vWeb3,"Web3", STACK_vWebX,NULL,1,&HP.xHandleUpdateWeb3)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
  HP.mRTOS=HP.mRTOS+64+4*STACK_vWebX;
#endif
vSemaphoreCreateBinary(xLoadingWebSemaphore);           // Создание семафора загрузки веб морды в spi память
if (xLoadingWebSemaphore==NULL) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
//xLoadingWebMutex=xSemaphoreCreateMutex();

vSemaphoreCreateBinary(xWebThreadSemaphore);               // Создание мютекса
if (xWebThreadSemaphore==NULL) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
vSemaphoreCreateBinary(xI2CSemaphore);                     // Создание мютекса
if (xI2CSemaphore==NULL) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
//vSemaphoreCreateBinary(xSPISemaphore);                     // Создание мютекса
//if (xSPISemaphore==NULL) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
// Дополнительные семафоры (почему то именно здесь) Создается когда есть модбас
if(Modbus.get_present())
{  
 vSemaphoreCreateBinary(xModbusSemaphore);                       // Создание мютекса
 if (xModbusSemaphore==NULL) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS);
}

journal.jprintf(" Create tasks - OK, size %d bytes\n",HP.mRTOS);

if(HP.get_HP_ON()>0)  HP.sendCommand(pRESTART);  // если надо запустить ТН - отложенный старт

//journal.jprintf("16. Send a notification . . .\n");
//HP.message.setMessage(pMESSAGE_RESET,(char*)"Контроллер теплового насоса был сброшен",0);    // сформировать уведомление о сбросе контролла
journal.jprintf("17. Information:\n");
freeRamShow();
HP.startRAM=freeRam()-HP.mRTOS;   // оценка свободной памяти до пуска шедулера, поправка на 1054 байта
journal.jprintf("FREE MEMORY %d bytes\n",HP.startRAM); 
journal.jprintf("Temperature SAM3X8E: %.2f\n",temp_DUE()); 
journal.jprintf("Temperature DS2331: %.2f\n",getTemp_RtcI2C()); 
//HP.Stat.generate_TestData(STAT_POINT); // Сгенерировать статистику STAT_POINT точек только тестирование
journal.jprintf("Start FreeRTOS scheduler :-))\n");
journal.jprintf("READY ----------------------\n");
eepromI2C.use_RTOS_delay = 1;       //vad711
//
vTaskStartScheduler();              // СТАРТ !!
journal.jprintf("CRASH FreeRTOS!!!\n");
}


void loop() 
{
//web_server(); 
}

//  З А Д А Ч И -------------------------------------------------
// Это и есть поток с минимальным приоритетом измеряем простой процессора
//extern "C" 
//{
static unsigned long cpu_idle_max_count = 0; // 1566594 // максимальное значение счетчика, вычисляется при калибровке и соответствует 100% CPU idle

extern "C" void vApplicationIdleHook(void)  // FreeRTOS expects C linkage
{
	static boolean ledState = LOW;
	static unsigned long countLastTick = 0;
	static unsigned long countLED = 0;
	static unsigned long ulIdleCycleCount = 0;                                    // наш трудяга счетчик

	WDT_Restart(WDT);                                                            // Сбросить вачдог
	ulIdleCycleCount++;                                                          // приращение счетчика

	if(xTaskGetTickCount() - countLastTick >= 3000)		// мсек
	{
		countLastTick = xTaskGetTickCount();                            // расчет нагрузки
		if(ulIdleCycleCount > cpu_idle_max_count) cpu_idle_max_count = ulIdleCycleCount; // это калибровка запоминаем максимальные значения
		HP.CPU_IDLE = (100 * ulIdleCycleCount) / cpu_idle_max_count;              // вычисляем текущую загрузку
		ulIdleCycleCount = 0;
		//
//		static uint8_t xxxx = 0;
//		if(++xxxx > 3) {
//			Serial.println(cpu_idle_max_count);
//			Serial.println(HP.CPU_IDLE);
//			xxxx = 0;
//		}
	}

	// Светодиод мигание в зависимости от ошибки и подача звукового сигнала при ошибке
	if(xTaskGetTickCount() - countLED > TIME_LED_ERR) {
		if(HP.get_errcode() != OK) {          // Ошибка
			digitalWriteDirect(PIN_BEEP, HP.get_Beep() ? ledState : LOW); // звукового сигнала
			ledState = !ledState;
			digitalWriteDirect(PIN_LED_OK, ledState);
			countLED = xTaskGetTickCount();
		} else if(xTaskGetTickCount() - countLED > TIME_LED_OK)   // Ошибок нет и время пришло
		{
			digitalWriteDirect(PIN_BEEP, LOW);
			ledState = !ledState;       // ОК
			digitalWriteDirect(PIN_LED_OK, ledState);
			countLED = xTaskGetTickCount();
		}
	}
}

// --------------------------- W E B ------------------------
// Задача обслуживания web сервера
// Сюда надо пихать все что связано с сетью иначе конфликты не избежны
// Здесь также обслуживается посылка уведомлений MQTT
// Первый поток веб сервера - дополнительно нагружен различными сервисами
void vWeb0( void *)
{ //const char *pcTaskName = "Web server is running\r\n";
   static unsigned long timeResetW5200=0;
   static unsigned long thisTime=0;
   static unsigned long resW5200=0;
   static unsigned long iniW5200=0;
   static unsigned long pingt=0;
#ifdef MQTT
   static unsigned long narmont=0;
   static unsigned long mqttt=0;
#endif
   static boolean active=true;  // ФЛАГ Одно дополнительное действие за один цикл - распределяем нагрузку
   static boolean network_last_link = true;
   
   HP.timeNTP=xTaskGetTickCount();        // В первый момент не обновляем
    for( ;; )
    {
      web_server(0);
      active=true;                                                         // Можно работать в этом цикле (дополнительная нагрузка на поток)
      vTaskDelay(TIME_WEB_SERVER/portTICK_PERIOD_MS); // задержка чтения уменьшаем загрузку процессора
   
      // СЕРВИС: Этот поток работает на любых настройках, по этому сюда ставим работу с сетью
       HP.message.sendMessage();                                            // Отработать отсылку сообщений (внутри скрыта задержка после включения)
 
       active=HP.message.dnsUpdate();                                       // Обновить адреса через dns если надо Уведомления
       #ifdef MQTT 
         if (active) active=HP.clMQTT.dnsUpdate();                          // Обновить адреса через dns если надо MQTT
       #endif 
      if (thisTime>xTaskGetTickCount()) thisTime=0;                         // переполнение счетчика тиков
      if (xTaskGetTickCount()-thisTime>10*1000)                             // Делим частоту - период 10 сек
       {
         thisTime=xTaskGetTickCount();                                      // Запомнить тики
         // 1. Проверка захваченого семафора сети ожидаем  3 времен W5200_TIME_WAIT если мютекса не получаем то сбрасывае мютекс
         if (SemaphoreTake(xWebThreadSemaphore,(3*W5200_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE) { SemaphoreGive(xWebThreadSemaphore);  journal.jprintf(pP_TIME,"UNLOCK mutex xWebThread\n"); active=false; HP.num_resMutexSPI++;} // Захват мютекса SPI или ОЖИДАНИНЕ 2 времен W5200_TIME_WAIT и его освобождение
         else  SemaphoreGive(xWebThreadSemaphore);
       
         // Проверка и сброс митекса шины I2C  если мютекса не получаем то сбрасывае мютекс
         if (SemaphoreTake(xI2CSemaphore,(3*I2C_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE) { SemaphoreGive(xI2CSemaphore);journal.jprintf(pP_TIME,"UNLOCK mutex xI2CSemaphore\n");  HP.num_resMutexI2C++;} // Захват мютекса I2C или ОЖИДАНИНЕ 3 времен I2C_TIME_WAIT  и его освобождение
         else  SemaphoreGive(xI2CSemaphore);
 
         // 2. Чистка сокетов
         if (HP.time_socketRes()>0)    checkSockStatus();                   // Почистить старые сокеты  если эта позиция включена
                                            
          // 3. Сброс сетевого чипа по времени
          if ((HP.time_resW5200()>0)&&(active))                             // Сброс W5200 если включен и время подошло
               {
                    resW5200 = xTaskGetTickCount();
                    if (timeResetW5200==0) timeResetW5200 = resW5200;      // Первая итерация не должна быть сразу
                    if(resW5200 - timeResetW5200 > HP.time_resW5200()*1000UL) 
                     {
                       journal.jprintf(pP_TIME,"Resetting the chip %s by timer . . .\n", nameWiznet);
                       HP.sendCommand(pNETWORK);                          // Послать команду сброса и применения сетевых настроек
                       timeResetW5200 = resW5200;                         // Запомить время сброса
                       active=false;
                     }
               }
          // 4. Проверка связи с чипом
          if((HP.get_fInitW5200()) && (thisTime - iniW5200 > 60 * 1000UL) && (active)) // проверка связи с чипом сети раз в минуту
          {
        	  iniW5200 = thisTime;
        	  if(!HP.NO_Power) {
				  boolean lst = linkStatusWiznet(false);
				  if(!lst || !network_last_link) {
					  if(!lst) journal.jprintf(pP_TIME, "%s no link, resetting . . .\n", nameWiznet);
					  HP.sendCommand(pNETWORK);       // Если связь потеряна то подать команду на сброс сетевого чипа
					  HP.num_resW5200++;              // Добавить счетчик инициализаций
					  active = false;
				  }
				  network_last_link = lst;
          	  }
          }
          // 5.Обновление времени 1 раз в сутки или по запросу (HP.timeNTP==0)
          if((HP.timeNTP==0)||((HP.get_updateNTP())&&(thisTime-HP.timeNTP>60*60*24*1000UL)&&(active)))      // Обновление времени раз в день 60*60*24*1000 в тиках HP.timeNTP==0 признак принудительного обновления
          {
           HP.timeNTP=thisTime;
           set_time_NTP();                                                 // Обновить время
           active=false;
          }
         // 6. ping сервера если это необходимо
         if ((HP.get_pingTime()>0)&&(thisTime-pingt>HP.get_pingTime()*1000UL)&&(active))
           {
             pingt=thisTime;
             pingServer();
             active=false;
           }
        
         #ifdef MQTT                                     // признак использования MQTT
            // 7. Отправка нанародный мониторинг
           if ((HP.clMQTT.get_NarodMonUse())&&(thisTime-narmont>TIME_NARMON*1000UL)&&(active))       // если нужно & время отправки пришло
             {
                narmont=thisTime;
                sendNarodMon(false);                       // отладка выключена
                active=false;
             }  // if ((HP.clMQTT.get_NarodMonUse()))  
          
              // 8. Отправка на MQTT сервер
           if ((HP.clMQTT.get_MqttUse())&&(thisTime-mqttt>HP.clMQTT.get_ttime()*1000UL)&&(active))    // если нужно & время отправки пришло
             {
                mqttt=thisTime;
                if(HP.clMQTT.get_TSUse())  sendThingSpeak(false);              
                else                       sendMQTT(false);
                active=false;
             }    
         #endif   // MQTT
        taskYIELD();                       
      } // if (xTaskGetTickCount()-thisTime>10000)   
      
    } //for
  vTaskDelete( NULL );  
}

// Второй поток
void vWeb1(void *)
{ //const char *pcTaskName = "Web server is running\r\n";
	for(;;) {
		web_server(1);
		vTaskDelay(TIME_WEB_SERVER / portTICK_PERIOD_MS); // задержка чтения уменьшаем загрузку процессора

	}
	vTaskDelete( NULL);
}
// Третий поток
void vWeb2(void *)
{ //const char *pcTaskName = "Web server is running\r\n";
	for(;;) {
		web_server(2);
		vTaskDelay(TIME_WEB_SERVER / portTICK_PERIOD_MS); // задержка чтения уменьшаем загрузку процессора

	}
	vTaskDelete( NULL);
}
// Четвертый поток
void vWeb3(void *)
{ //const char *pcTaskName = "Web server is running\r\n";
	for(;;) {
		web_server(3);
		vTaskDelay(TIME_WEB_SERVER / portTICK_PERIOD_MS); // задержка чтения уменьшаем загрузку процессора
	}
	vTaskDelete( NULL);
}

//////////////////////////////////////////////////////////////////////////
// Задача чтения датчиков
void vReadSensor(void *)
{ //const char *pcTaskName = "ReadSensor\r\n";
	static unsigned long readFC = 0;
#ifdef USE_ELECTROMETER_SDM
	static unsigned long readSDM = 0;
#endif
	static uint32_t ttime;
	static uint32_t oldTime = millis();
	static uint8_t  prtemp = 0;
	
	for(;;) {
		int8_t i;
		WDT_Restart(WDT);

		ttime = millis();
#ifdef RADIO_SENSORS		
		radio_timecnt++;
#endif		
		if(OW_scan_flags == 0) {
#ifndef DEMO  // Если не демо
			prtemp = HP.Prepare_Temp(0);
#ifdef ONEWIRE_DS2482_SECOND
			prtemp |= HP.Prepare_Temp(1);
#endif
#ifdef ONEWIRE_DS2482_THIRD
			prtemp |= HP.Prepare_Temp(2);
#endif
#ifdef ONEWIRE_DS2482_FOURTH
			prtemp |= HP.Prepare_Temp(3);
#endif
#endif     // не DEMO
		}
		for(i = 0; i < ANUMBER; i++) HP.sADC[i].Read();                  // Прочитать данные с датчиков давления
#ifdef USE_ELECTROMETER_SDM   // Опрос состояния счетчика
	#ifdef USE_UPS
		if(!HP.NO_Power)
	#endif
		  HP.dSDM.get_readState(0); // Основная группа регистров
#endif
		for(i = 0; i < INUMBER; i++) HP.sInput[i].Read();                // Прочитать данные сухой контакт
		for(i = 0; i < FNUMBER; i++) HP.sFrequency[i].Read();			// Получить значения датчиков потока

		vReadSensor_delay8ms((cDELAY_DS1820 - (millis() - ttime)) / 8); 	// Ожидать время преобразования

		if(OW_scan_flags == 0) {
			uint8_t flags = 0;
			for(i = 0; i < TNUMBER; i++) {                                   // Прочитать данные с температурных датчиков
				if((prtemp & (1<<HP.sTemp[i].get_bus())) == 0) {
					if(HP.sTemp[i].Read() == OK) flags |= HP.sTemp[i].get_setup_flags();
				}
				_delay(1);     												// пауза
			}
			int32_t temp;
			if(GETBIT(flags, fTEMP_as_TIN_average)) { // Расчет средних датчиков для TIN
				temp = 0;
				uint8_t cnt = 0;
				for(i = 0; i < TNUMBER; i++) {
					if(HP.sTemp[i].get_setup_flag(fTEMP_as_TIN_average)) {
						temp += HP.sTemp[i].get_Temp();
						cnt++;
					}
				}
				if(cnt) temp /= cnt; else temp = 32767;
			} else temp = 32767;
			int16_t temp2 = temp;
			if(GETBIT(flags, fTEMP_as_TIN_min)) { // Выбор минимальной температуры для TIN
				for(i = 0; i < TNUMBER; i++) {
					if(HP.sTemp[i].get_setup_flag(fTEMP_as_TIN_min) && temp2 > HP.sTemp[i].get_Temp()) temp2 = HP.sTemp[i].get_Temp();
				}
			}
			if(temp2 != 32767) HP.sTemp[TIN].set_Temp(temp2);
		}

#ifdef USE_ELECTROMETER_SDM   // Опрос состояния счетчика
	#ifdef USE_UPS
		if(!HP.NO_Power)
	#endif
			if((HP.dSDM.get_present()) && (millis() - readSDM > SDM_READ_PERIOD)) {
				readSDM=millis();
				HP.dSDM.get_readState(2);     // Последняя группа регистров
			}
#endif

		HP.calculatePower();  // Расчет мощностей и СОР
		Stats.Update();

		vReadSensor_delay8ms((TIME_READ_SENSOR - (millis() - ttime)) / 2 / 8);     // 1. Ожидать время нужное для цикла чтения

		// Вычисление перегрева используются РАЗНЫЕ датчики при нагреве и охлаждении
		// Режим работы определяется по состоянию четырехходового клапана при его отсутвии только нагрев
#ifdef EEV_DEF
		HP.dEEV.set_Overheat(HP.get_modWork() != pCOOL && HP.get_modWork() != pNONE_C); // нагрев(1) или охлаждение(0)
#endif

		//  Опрос состояния инвертора
	#ifdef USE_UPS
		if(!HP.NO_Power)
	#endif
			if((HP.dFC.get_present()) && (millis() - readFC > FC_TIME_READ)) {
				readFC = millis();
				HP.dFC.get_readState();
			}

#ifdef DRV_EEV_L9333  // Опрос состяния драйвера ЭРВ
		if (digitalReadDirect(PIN_STEP_DIAG)) // Перечитываем два раза
		{
			vReadSensor_delay8ms(5);
			if (digitalReadDirect(PIN_STEP_DIAG)) set_Error(ERR_DRV_EEV,(char*)__FUNCTION__); // Контроль за работой драйвера ЭРВ
		}
#endif

		//  Синхронизация часов с I2C часами если стоит соответсвующий флаг
		if(HP.get_updateI2C())  // если надо обновить часы из I2c
		{
			if(millis() - oldTime > (uint32_t)TIME_I2C_UPDATE) // время пришло обновляться надо Период синхронизации внутренних часов с I2C часами (сек)
			{
				oldTime = rtcSAM3X8.unixtime();
				uint32_t t = TimeToUnixTime(getTime_RtcI2C());       // Прочитать время из часов i2c тут проблема
				rtcSAM3X8.set_clock(t);                		 // Установить внутренние часы по i2c
				HP.updateDateTime(t > oldTime ? t - oldTime : -(oldTime - t));  // Обновить переменные времени с новым значением часов
				journal.jprintf((const char*) "Sync from I2C RTC: %s %s\n", NowDateToStr(), NowTimeToStr());
				oldTime = millis();
			}
		}
		// Проверка и сброс митекса шины I2C
//       if (SemaphoreTake(xI2CSemaphore,(3*I2C_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE) { SemaphoreGive(xI2CSemaphore);journal.jprintf("UNLOCK mutex xI2CSemaphore\n");  HP.num_resMutexI2C++;} // Захват мютекса I2C или ОЖИДАНИНЕ 3 времен I2C_TIME_WAIT  и его освобождение
//       else  SemaphoreGive(xI2CSemaphore);
		// Проверки граничных температур для уведомлений, если разрешено!
		static uint16_t countTEMP = 0;        // Для проверки критических температур для рассылки уведомлений
		if(HP.message.get_fMessageTemp()) {
			if(countTEMP > TIME_MESSAGE_TEMP) {
				countTEMP = 0;
				if(HP.message.get_mTIN() > HP.sTemp[TIN].get_Temp()) HP.message.setMessage(pMESSAGE_TEMP,
						(char*) "Критическая температура в доме,", HP.sTemp[TIN].get_Temp());
				if(HP.message.get_mTBOILER() > HP.sTemp[TBOILER].get_Temp()) HP.message.setMessage(pMESSAGE_TEMP,
						(char*) "Критическая температура ГВС,", HP.sTemp[TBOILER].get_Temp());
				if(HP.message.get_mTCOMP() < HP.sTemp[TCOMP].get_Temp()) HP.message.setMessage(pMESSAGE_TEMP,
						(char*) "Критическая температура компрессора,", HP.sTemp[TCOMP].get_Temp());
			} else countTEMP += TIME_READ_SENSOR / 100; // в 0.1 сек
		}
		static uint8_t last_life_h = 255;
		if(HP.message.get_fMessageLife()) // Подача сигнала жизни если разрешено!
		{
			uint8_t hour = rtcSAM3X8.get_hours();
			if(hour == HOUR_SIGNAL_LIFE && hour != last_life_h) {
				HP.message.setMessage(pMESSAGE_LIFE, (char*) "Контроллер работает . . .", 0);
			}
			last_life_h = hour;
		}
		////
		vReadSensor_delay8ms((TIME_READ_SENSOR - (millis() - ttime)) / 8);     // Ожидать время нужное для цикла чтения
		ttime = TIME_READ_SENSOR - (millis() - ttime);
		if(ttime && ttime <= 8) vTaskDelay(ttime);

	}  // for
	vTaskDelete( NULL);
}

// Вызывается во время задержек в задаче чтения датчиков
void vReadSensor_delay8ms(int16_t ms8)
{
	do {
		if(ms8) vTaskDelay(8);
#ifdef  KEY_ON_OFF // Если надо проверяем кнопку включения ТН
		static boolean Key1_ON = HIGH;                              // кнопка вкл/вкл дребез подавление
		if ((!digitalReadDirect(PIN_KEY1))&&(Key1_ON)) {
			vTaskDelay(8*3);
			ms8 -= 3;
			if (!digitalReadDirect(PIN_KEY1)) {  // дребезг
				journal.jprintf("Press KEY_ON_OFF\n");
				if (HP.get_State()==pOFF_HP) HP.sendCommand(pSTART); else HP.sendCommand(pSTOP);
			}
		} else Key1_ON=digitalReadDirect(PIN_KEY1); // запоминаем состояние
#endif
#ifdef USE_UPS
		HP.sInput[SPOWER].Read(true);
		if(HP.sInput[SPOWER].is_alarm()) { // Электричество кончилось
			if(!HP.NO_Power) {
				HP.save_motoHour();
				Stats.SaveStats(0);
				Stats.SaveHistory(0);
				journal.jprintf(pP_DATE, "Power lost!\n");
				if(HP.get_State() == pSTARTING_HP || HP.get_State() == pWORK_HP) {
					HP.sendCommand(pWAIT);
					HP.NO_Power = 2;
				} else HP.NO_Power = 1;
			}
			HP.NO_Power_delay = NO_POWER_ON_DELAY_CNT;
		} else if(HP.NO_Power) { // Включаемся
			if(HP.NO_Power_delay) HP.NO_Power_delay--;
			else {
				journal.jprintf(pP_DATE, "Power restored!\n");
				if(!HP.Schdlr.IsShedulerOn()) {  // Расписание не активно, иначе включаемся через расписание
					if(HP.NO_Power == 2 && HP.get_State() == pWAIT_HP) {
						HP.NO_Power = 0;
						journal.jprintf("Resuming work\n");
						HP.sendCommand(pRESUME);
					}
				}
				HP.NO_Power = 0;
			}
		}
#endif
#ifdef RADIO_SENSORS
		check_radio_sensors();
#endif
	} while(--ms8 > 0);
}

//////////////////////////////////////////////////////////////////////////
// Задача Управление тепловым насосом (HP.xHandleUpdate)
 void vUpdate( void * )
{ //const char *pcTaskName = "HP_Update\r\n";
	#ifdef RPUMPB
	static uint32_t RPUMPBTick=0;
	#endif
	 for( ;; )
	 {
		 if(!HP.Task_vUpdate_run) {
			 vTaskSuspend(NULL);				// Stop vUpdate, HP.xHandleUpdate
			 continue;
		 }
		 if (HP.get_State()==pWORK_HP){ //Код обслуживания работы ТН выполняется только если состяние ТН - работа а вот расписание всегда выполняется
			 // 1. Обновится, В это время команды управления не выполняются!!!!!
			 if (SemaphoreTake(HP.xCommandSemaphore,0)==pdPASS)                                           // Cемафор  захвачен
			 {
				 if (HP.get_State()==pWORK_HP)  HP.vUpdate();                                               // ТН работает и идет процесс контроля
				 SemaphoreGive(HP.xCommandSemaphore);                                                       // Семафор отдан
			 }
			 // 2. Управление циркуляционным насосом ГВС
#ifdef RPUMPB
#ifdef SUPERBOILER
			 if (HP.scheduleBoiler())                         // Для супербойлера игнорироуем для циркуляции флаг включения бойлера только расписание
#else
				 if ((HP.scheduleBoiler())&&(HP.get_BoilerON()))  // если бойлер разрешен и разрешено греть бойлер согласно расписания или расписание выключено
#endif
				 {
					 if ((HP.get_modWork()==pBOILER)||(HP.get_modWork()==pNONE_B))           // Если включен нагрев ГВС всегда включать насос циркуляции
					 { HP.dRelay[RPUMPB].set_ON(); }
					 else
						 if (HP.get_Circulation())                                               // Циркуляция разрешена
						 {
							 if ((HP.dRelay[RCOMP].get_Relay()||HP.dFC.isfOnOff())&&(HP.get_onBoiler())) { HP.dRelay[RPUMPB].set_ON(); continue;} // идет нагрев ГВС включаем насос ГВС ВСЕГДА - улучшаем перемешивание
							 if (HP.get_CirculWork()==0) { HP.dRelay[RPUMPB].set_OFF(); continue;}   // В условиях стоит время работы 0 - выключаем насос ГВС
							 if (HP.get_CirculPause()==0) { HP.dRelay[RPUMPB].set_ON(); continue;}  // В условиях стоит время паузы 0 - включаем насос ГВС
							 if(HP.dRelay[RPUMPB].get_Relay())                                       // Насос включен Смотрим времена
							 {
								 if(((long)xTaskGetTickCount()-RPUMPBTick ) > HP.get_CirculWork()*configTICK_RATE_HZ)   // ждем время мсек
								 {
									 RPUMPBTick=xTaskGetTickCount();
									 HP.dRelay[RPUMPB].set_OFF();                                  // выключить насос
								 }
							 }
							 else                                                                 // Насос выключен
							 {
								 if(((long)xTaskGetTickCount()-RPUMPBTick ) >  HP.get_CirculPause()*configTICK_RATE_HZ)   // ждем время мсек
								 {
									 RPUMPBTick=xTaskGetTickCount();
									 HP.dRelay[RPUMPB].set_ON();                                    // включить насос
								 }
							 } // if(HP.dRealay[RPUMPB].get_Relay())
						 }  //  if (HP.get_Circulation())
						 else HP.dRelay[RPUMPB].set_OFF() ;                                      // if (HP.get_Circulation())        выключить насос если его управление запрещено
				 } //  if (HP.scheduleBoiler())
				 else  HP.dRelay[RPUMPB].set_OFF() ;                                       // По расписанию выключено или бойлер запрещен,  насос выключаем
#endif // #ifdef RPUMPB
		 } // НЕ РЕЖИМ ОЖИДАНИЕ if HP.get_State()==pWORK_HP)

		 if(!HP.Task_vUpdate_run) continue;

// 3. Расписание проверка всегда
		 int8_t _profile = HP.Schdlr.calc_active_profile(); // Какой профиль ДОЛЖЕН быть сейчас активен
		 if(_profile != SCHDLR_NotActive) {                 // Расписание активно
			 int8_t _curr_profile = HP.get_State() == pWORK_HP ? HP.Prof.get_idProfile() : SCHDLR_Profile_off;
			 if(_profile != _curr_profile && HP.isCommand() == pEMPTY) { // новый режим и ни чего не выполняется?
				 if(_profile == SCHDLR_Profile_off) {
					 HP.sendCommand(pWAIT);
				 } else if(HP.Prof.get_idProfile() != _profile) {
					 type_SaveON _son;
					 if(HP.Prof.load_from_EEPROM_SaveON(&_son) == OK) {
						 MODE_HP currmode = HP.get_modWork();
						 uint8_t frestart = currmode != pOFF && ((currmode == pCOOL) != (_son.mode == pCOOL)); // Если направление работы ТН разное
						 if(frestart) {
							 HP.sendCommand(pWAIT);
							 uint8_t i = 10; while(HP.isCommand()) {	_delay(1000); if(!--i) break; } // ждем отработки команды
							 if(!HP.Task_vUpdate_run) continue;
						 }
						 vTaskSuspendAll();	// без проверки
						 HP.Prof.load(_profile);
						 HP.set_profile();
						 xTaskResumeAll();
						 journal.jprintf(pP_TIME, "Profile changed to #%d\n", _profile);
						 if(frestart) HP.sendCommand(pRESUME);
					 }
				 } else if(HP.get_State() == pWAIT_HP && !HP.NO_Power) {
					 HP.sendCommand(pRESUME);
				 }
			 }
		 }

		 // 4. Отработка пауз всегда они разные в зависимости от состояния ТН!!
		 switch (HP.get_State())  // Состояние ТН 
		 {
		 case pOFF_HP:                          // 0 ТН выключен
		 case pSTOPING_HP:                      // 2 Останавливается
			 journal.jprintf((const char*)" Stop task update %s from vUpdate\n",(char*)nameHeatPump);
			 HP.Task_vUpdate_run = false;
			 break;
		 case pSTARTING_HP: _delay(10000); break; // 1 Стартует  - этого не должно быть в этом месте
		 case pWORK_HP:                           // 3 Работает   - анализ режима работы get_modWork()
			 switch(HP.get_modWork())              // Что делает ТН если включен (7 вариантов) 0 Пауза 1 Включить отопление 2 Включить охлаждение 3 Включить бойлер 4 Продолжаем греть отопление 5 Продолжаем охлаждение 6 Продолжаем греть бойлер
			 {
			 case pOFF:                          // 0 Пауза
				 vTaskDelay(TIME_CONTROL/portTICK_PERIOD_MS);    // Гистерезис
				 break;
			 case pHEAT:                         // 1 Включить отопление
			 case pNONE_H:                       // 4 Продолжаем греть отопление
				 if(HP.get_ruleHeat()==pHYSTERESIS)  vTaskDelay(TIME_CONTROL/portTICK_PERIOD_MS);    // Гистерезис
				 else
					 vTaskDelay(HP.dFC.get_Uptime()*1000/portTICK_PERIOD_MS);                                        // Время интегрирования ПИД  секунды
				 break;
			 case pCOOL:                         // 2 Включить охлаждение
			 case pNONE_C:                       // 5 Продолжаем охлаждение
				 if(HP.get_ruleCool()==pHYSTERESIS)  vTaskDelay(TIME_CONTROL/portTICK_PERIOD_MS);    // Гистерезис
				 else
					 vTaskDelay(HP.dFC.get_Uptime()*1000/portTICK_PERIOD_MS);                                         // Время интегрирования ПИД секунды
				 break;
			 case pBOILER:                       // 3 Включить бойлер
			 case pNONE_B:                       // 6 Продолжаем греть бойлер
				 vTaskDelay(HP.dFC.get_Uptime()*1000/portTICK_PERIOD_MS);                                    // Время интегрирования ПИД секунды
				 break;
			 default:
				 journal.jprintf((const char*)" $ERROR: Bad mode HP in function %s\n",(char*)__FUNCTION__);
			 }  // switch(HP.get_modeHouse() )
			 break;
		 case pWAIT_HP:                          // 4 Ожидание ТН (расписание - пустое место)   проверям раз в 5 сек
		 case pERROR_HP:_delay(UPDATE_HP_WAIT_PERIOD); break;     // 5 Ошибка ТН
		 case pERROR_CODE:                       // 6 - Эта ошибка возникать не должна!
		 default:
			 journal.jprintf((const char*)" $ERROR: Bad state HP in function %s\n",(char*)__FUNCTION__);
		 } //  switch (HP.get_State())

		 if(!HP.Task_vUpdate_run) continue;
		 // Солнечный коллектор
#ifdef USE_SUN_COLLECTOR
		boolean fregen = GETBIT(HP.get_flags(), fSunRegenerateGeo) && HP.is_pause();
		if(((HP.get_State() == pWORK_HP && !HP.is_pause()
				&& ((HP.get_modeHouse() == pHEAT && GETBIT(HP.Prof.Heat.flags, fUseSun)) || (HP.get_modeHouse() == pCOOL && GETBIT(HP.Prof.Cool.flags, fUseSun)) || (HP.get_modeHouse() == pBOILER && GETBIT(HP.Prof.Boiler.flags, fBoilerUseSun)))) || fregen)
				&& HP.get_State() != pERROR_HP && (HP.get_State() != pOFF_HP || HP.PauseStart != 0)) {
			if((HP.flags & (1<<fHP_SunActive))) {
				if(fregen) {
					if(HP.sTemp[TSUN].get_Temp() < HP.Option.SunRegGeoTemp) HP.Sun_OFF();
				} else if(HP.time_Sun_ON && millis() - HP.time_Sun_ON > SUN_MIN_WORKTIME && HP.sTemp[TSUNOUTG].get_Temp() < HP.sTemp[TEVAOUTG].get_Temp() + SUNG_TDELTA) HP.Sun_OFF();
			} else if(HP.sTemp[TSUN].get_Temp() >= (fregen ? HP.Option.SunRegGeoTemp : HP.sTemp[TEVAOUTG].get_Temp()) + SUN_TDELTA) {
				if(HP.time_Sun_OFF == 0 || millis() - HP.time_Sun_OFF > SUN_MIN_PAUSE) { // ON
					HP.flags |= (1<<fHP_SunActive);
					HP.dRelay[RSUN].set_Relay(fR_StatusSun);
					HP.dRelay[PUMP_IN].set_Relay(fR_StatusSun);
					HP.time_Sun_ON = millis();
					HP.time_Sun_OFF = 0;
				}
			}
		} else {
			HP.Sun_OFF();
			HP.time_Sun_OFF = 0;	// выключить задержку последующего включения
		}
#endif
	 }// for
	 vTaskDelete( NULL );
}

// Задача Управление ЭРВ
#ifdef EEV_DEF
void vUpdateEEV(void *)
{ //const char *pcTaskName = "HP_UpdateEEV\r\n";
	static int16_t cmd = 0;
	for(;;) {
		while(!(HP.get_startCompressor() && (rtcSAM3X8.unixtime() - HP.get_startCompressor() > HP.dEEV.get_delayOnPid() && HP.dEEV.get_delayOnPid() != 255))) { // ЭРВ контролирует если прошла задержка после включения компрессора (пауза перед началом работы ПИД) и задержка != 255
			vTaskDelay(TIME_EEV / portTICK_PERIOD_MS); // Период управления ЭРВ (цикл управления)
			if(GETBIT(HP.dEEV.get_flags(), fEEV_StartPosByTemp)) { // Скорректировать ЭРВ по температуре подачи
				HP.dEEV.set_EEV(HP.dEEV.get_StartPos());
			}
		}
		HP.dEEV.resetPID();
xContinue:
		// Для большей надежности если очередь заданий на шаговик пуста поставить флаг отсутвия движения
		// Если очередь пуста а флаг что есть движение - предупреждение потеря синхронизации ЭРВ  и сброс флага
		if((xQueuePeek(HP.dEEV.stepperEEV.xCommandQueue,&cmd,0) == errQUEUE_EMPTY) && (HP.dEEV.stepperEEV.isBuzy())) {
			//     journal.jprintf("$WARNING! Loss of sync EEV\n");
			HP.dEEV.stepperEEV.offBuzy();  // признак Мотор остановлен
		}

		if(!HP.is_compressor_on()) {
			switch((uint8_t)HP.get_State()) {
			case pOFF_HP:
			case pSTOPING_HP:
			case pWAIT_HP:
				// Если компрессор не работает, то остановить задачу Обновления ЭРВ
				journal.jprintf((const char*) " Stop task update EEV\n");
				vTaskSuspend(NULL);				// Stop vUpdateEEV
				continue; // продолжение задачи работы ЭРВ начитается с этого места, по этому сразу на начало цикла контроля
			}
		} else {
			HP.dEEV.CorrectOverheat();
			// Обновить и выполнить итерацию по контролю ЭРВ Для алгоритма таблица передаем СРЕДНИЕ (IN+OUT)/2 температуры
			HP.dEEV.Update();
			// штатная пауза (в зависимости от настроек)
			vTaskDelay(HP.dEEV. get_PID_time() * 1000 / portTICK_PERIOD_MS);  // ПИД
			goto xContinue;
		}
		vTaskDelay(TIME_EEV / portTICK_PERIOD_MS); // Период управления ЭРВ (цикл управления)
	} // for
	vTaskDelete( NULL);
}
#endif

#ifdef EEV_DEF
// Задача обеспечения движения шаговика EEV (StepperEEV)
void vUpdateStepperEEV(void *)
{ //const char *pcTaskName = "HP_UpdateStepperEEV\r\n";
  // Размер стека не позволяет использовать внутри jprintf.* !!!
	static int16_t cmd = 0;
	static int16_t steps_left = 0, step_number = 0, start_pos = 0, pos = 0;
	static boolean direction = true;

	for(;;) {
		// Полный цикл движения шаговика с разгребанием очереди команд,
		// В очереди лежат АБСОЛЮТНЫЕ координаты
		// При этом если очередь содержит более одной команды - просто суммируем все команды и двигаемся по итоговой сумме
		// Это значит что шаговик не успевает за темпом выдачи команд программой. Экономим время

		// 1. Чтение очереди команд, для выяснения все таки куда надо двигаться, переходим на относительные координаты
		pos = 0; // текущее суммарное движение - обнулится
		start_pos = HP.dEEV.get_EEV();  // получить текущее положение шаговика абсолютное в начале очереди
		//   step_number=HP.dEEV.EEV;
		//  Serial.print("1. step_number=");   Serial.print(step_number); Serial.print(" EEV=");   Serial.println(HP.dEEV.EEV);
		// 3. Движение
		while(xQueueReceive(HP.dEEV.stepperEEV.xCommandQueue,&cmd,0) == pdPASS)    // Читаем очередь пока есть чего читать
		{
			pos = pos + (cmd - start_pos);                                            // Суммируем все приращения для получение итогового движения
			start_pos = cmd;                                                      // итоговая абсолютная координата отдельной команды
			//        Serial.print("2. Read Queu cmd: ");   Serial.println(cmd);
			//        if (HP.dEEV.setZero) { step_number=HP.dEEV.stepperEEV.number_of_steps;  break;}  // Если выполняется команда установки 0 то все остальные команды игнорируются до ее выполнения.
			// Если выполняется команда установки 0 то все остальные команды игнорируются до ее выполнения.
			if(HP.dEEV.setZero) {
				step_number = (HP.dEEV.stepperEEV.number_of_steps / 8) * 8 + 32;/*pos=-530;*/
				break;
			} // Должно делится на 8 и 4 без остатка
		}

		// if (pos==0) continue; // двигать нечего
		//      Serial.print("3. Sum command=");   Serial.println(pos);
		// 2. Подготовка к движению
		steps_left = abs(pos);                                    // Определить абсолютное число шагов движения он уменьшается до 0
		// НАПРАВЛЕНИЕ determine direction based on whether steps_to_mode is + or -:
		if(pos > 0) direction = true;
		if(pos < 0) direction = false;

		//    Serial.print("3. step_number=");   Serial.print(step_number); Serial.print(" EEV=");   Serial.println(HP.dEEV.EEV);
		// 3. Движение
		while(steps_left > 0) {
			if(direction)  // направление в увеличение
			{
				step_number++;
				HP.dEEV.EEV++;
			} else                      // направление в уменьшение
			{
				step_number--;
				HP.dEEV.EEV--;
			}
			steps_left--;                                                      // уменьшить счетчик шагов
			if((step_number < 0) && (!HP.dEEV.setZero)) {
				HP.dEEV.set_error(ERR_MIN_EEV);
				break;
			}
#if EEV_PHASE==PHASE_4  // 4 фазы движения
			HP.dEEV.stepperEEV.stepOne(abs(step_number % 4));                  // Сделать один шаг //
#else                     // остальные варианты  8 фаз движения
			HP.dEEV.stepperEEV.stepOne(abs(step_number % 8));                   // Сделать один шаг //
			//     HP.dEEV.stepperEEV.stepOne(abs(HP.dEEV.EEV % 8));                   // Дмитрий говорит что это не правильно устанавливает 0 (открывает????)
#endif

			//     HP.dEEV.stepperEEV.stepOne(abs(HP.dEEV.EEV % 8));                   // Дмитрий говорит что это не правильно устанавливает 0 (открывает????)
			vTaskDelay(HP.dEEV.stepperEEV.step_delay / portTICK_PERIOD_MS);      // Ожитать step_delay для следующего шага.
		}

		//    vTaskDelay(500/portTICK_PERIOD_MS);               // -!            // пауза  0.5 секунда ОБЯЗАТЕЛЬНО, иначе не сбрасывается isBuzy() НЕПОНЯТНО

		if(HP.dEEV.setZero) {
			HP.dEEV.EEV = 0;
			step_number = 0;
			HP.dEEV.setZero = false;
		}  // если стоит признак установки нуля, обнулить и сбросить признак
		//   Serial.print("4. EEV=");   Serial.print(HP.dEEV.EEV); Serial.print("  step_number=");   Serial.println(step_number);

		// 4. Остановить выполнение команад, если очередь пуста, но могли накидать пока двигались
		if(xQueuePeek(HP.dEEV.stepperEEV.xCommandQueue,&cmd,0) == errQUEUE_EMPTY) {
			//     Serial.println("6. TaskSuspend ");
			HP.dEEV.stepperEEV.offBuzy();                                                            // признак Мотор остановлен
			if(!HP.dEEV.get_HoldMotor()) HP.dEEV.stepperEEV.off();                                   // выключить двигатель если нет удержания
			vTaskSuspend(NULL);               // Приостановить задучу vUpdateStepperEEV
		}
		// Дошли до сюда новая, очередь не пуста и новая итерация по разбору очереди
		//    Serial.println("5. new command ");
	} // for
	vTaskDelete( NULL);
}
#endif

// vUpdateCommand /////////////////////////////////////////////////////// 
// Задача Разбор очереди команд
void vUpdateCommand(void *)
{ //const char *pcTaskName = "HP_UpdateCommand\r\n";
	for(;;) {
		HP.runCommand();       // Выполнение команд управления ТН
		vTaskSuspend(NULL);    // Команды выполнены, остановить задачу vUpdateCommand, пуск осуществляется при посылке команды
	}
	vTaskDelete( NULL);
}

// ServiceHP ///////////////////////////////////////////////
// Графики в ОЗУ, счетчики моточасов, сохранение статистики, работа насосов в простое, дисплей Nextion
void vServiceHP(void *)
{
	static uint32_t NextionTick = 0;
	static uint16_t task_updstat_chars = 0;
	static uint8_t  task_updstat_countm = rtcSAM3X8.get_minutes();
	static uint32_t timer_sec = GetTickCount();
	static uint16_t restart_cnt;
	static uint16_t pump_in_pause_timer = 0;
	for(;;) {
		register uint32_t t = GetTickCount();
		if(t - timer_sec >= 1000) { // 1 sec
			timer_sec = t;
			if(HP.IsWorkingNow()) {
				#ifdef CHART_ONLY_COMP_ON  // Накопление точек для графиков ТОЛЬКО если компрессор работает
 				if(++task_updstat_chars >= HP.get_tChart() && HP.is_compressor_on()) { // пришло время и компрессор работает
                #else
 				if(++task_updstat_chars >= HP.get_tChart() && HP.get_State()!=pOFF_HP) { // пришло время и ТН включен
                #endif 					
					task_updstat_chars = 0;
					HP.updateChart();                                       // Обновить графики
				}
				uint8_t m = rtcSAM3X8.get_minutes();
				if(m != task_updstat_countm) { 								// Через 1 минуту
					task_updstat_countm = m;
					HP.updateCount();                                       // Обновить счетчики моточасов
					if(task_updstat_countm == 59) HP.save_motoHour();		// сохранить раз в час
					Stats.History();                                        // запись истории в файл
				}
			}
			if(HP.PauseStart) {
				if(HP.PauseStart == 1) {
					restart_cnt = HP.isCommand() == pRESTART ? HP.Option.delayStartRes : HP.Option.delayRepeadStart;  // Определение времени задержки
					journal.jprintf((const char*) "Start over %d sec . . .\n", restart_cnt);
					HP.PauseStart = 2;
				} else if(restart_cnt-- == 0) {
					HP.PauseStart = 0;
					HP.sendCommand(pAUTOSTART);
				}
			}
			if(HP.startPump) {  // Если разрешена работа насоса( 0 - останов задачи, 1 - запуск, 2 - в работе (выкл), 3 - в работе (вкл))
				if(HP.startPump == 1 && HP.get_pausePump() == 0) { // Постоянно работают
					goto xPumpsOn;
				} else if(HP.get_workPump()) {
					if(pump_in_pause_timer <= 1) {
						if(HP.startPump <= 2) { // включить
							pump_in_pause_timer = HP.get_workPump();
xPumpsOn:					HP.dRelay[PUMP_OUT].set_ON();                  	// включить насос отопления
							HP.Pump_HeatFloor(true);						// включить насос ТП
							HP.startPump = 3;
						} else { // выключить
							HP.dRelay[PUMP_OUT].set_OFF();                 	// выключить насос отопления
							HP.Pump_HeatFloor(false);						// выключить насос ТП
							pump_in_pause_timer = HP.get_pausePump();
							HP.startPump = 2;
						}
					} else pump_in_pause_timer--;
				}
			}
			Stats.CheckCreateNewFile();
		}
#ifdef NEXTION
		myNextion.readCommand();                 // прочитать сообщения от дисплея
		if(xTaskGetTickCount() - NextionTick > NEXTION_UPDATE) {
			myNextion.Update();                  // Обновление дисплея
			NextionTick = xTaskGetTickCount();
		}
#endif
		t = GetTickCount() - timer_sec;
		vTaskDelay(t < NEXTION_READ ? t : NEXTION_READ); // задержка чтения уменьшаем загрузку процессора
	}
	vTaskDelete(NULL);
}
