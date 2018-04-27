/*
 * Copyright (c) 2016-2018 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
 * vad711, vad7@yahoo.com
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
// https://github.com/pav2000/ControlHeatPump проект на гитхабе
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
#ifdef MQTT                                 // признак использования MQTT
    #include <PubSubClient.h>               // передаланная под многозадачность  http://knolleary.net
#endif

#include "Hardware.h"
#include "HeatPump.h"
#include "StepMotor.h" 
#include "Nextion.h"
#include "Message.h"
#include "Information.h"
 

// Мютексы блокираторы железа
SemaphoreHandle_t xModbusSemaphore;                   // Семафор Modbus, инвертор запас на счетчик
SemaphoreHandle_t xWebThreadSemaphore;                // Семафор потоки вебсервера,  деление сетевой карты
SemaphoreHandle_t xI2CSemaphore;                      // Семафор шины I2C, часы, память, мастер OneWire
SemaphoreHandle_t xSPISemaphore;                      // Семафор шины SPI  сетевая карта, память. SD карта // пока не используется
static uint16_t lastErrorFreeRtosCode;                // код последней ошибки операционки нужен для отладки
static uint32_t startSupcStatusReg;                   // Состояние при старте SUPC Supply Controller Status Register - проверяем что с питание


#if defined(W5500_ETHERNET_SHIELD)                    // Задание имени чипа для вывода сообщений
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
PubSubClient w5200_MQTT(ethClient,W5200_SOCK_SYS);  // клиент MQTT через служебный сокет

extEEPROM eepromI2C(I2C_SIZE_EEPROM,1,I2C_PAGE_EEPROM,I2C_ADR_EEPROM,I2C_FRAM_MEMORY); // I2C eeprom Размер в килобитах, число чипов, страница в байтах, адрес на шине, тип памяти
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

// Функция задержки (мсек) в зависимости от шедулера задач FreeRtos
__attribute__((always_inline)) inline void _delay(int t)
{
  if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) vTaskDelay(t/portTICK_PERIOD_MS);
  else delay(t);
}

// Захватить семафор с проверкой, что шедуллер работает
BaseType_t SemaphoreTake(QueueHandle_t xSemaphore, TickType_t xBlockTime)
{
	if(xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) return pdTRUE;
	else return xSemaphoreTake(xSemaphore, xBlockTime);
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
    if (HP.dRelay[RCOMP].get_Relay()||HP.dFC.isfOnOff())         // СРАЗУ Если компрессор включен, выключить  ГЛАВНАЯ ЗАЩИТА
    {
     journal.jprintf("$Compressor protection: "); 
     if(HP.dFC.get_present()) HP.dFC.stop_FC(); else  HP.dRelay[RCOMP].set_OFF();    // Выключить компрессор
    }
 //   if ((HP.get_State()==pOFF_HP)&&(HP.error!=OK)) return HP.error;  // Если ТН НЕ работает, не стартует не останавливается и уже есть ошибка то останавливать нечего и выключать нечего выходим - ошибка не обновляется - важна ПЕРВАЯ ошибка

    if (HP.error!=OK) return HP.error;                              // Ошибка уже есть выходим
 //   if((_err!=HP.error)||(strcmp(nam,HP.source_error)!=0))     // Если приходит ошибка отличная от предыдущей то запоминаем
    {    
    HP.error=_err;
    strcpy(HP.source_error,nam);
    strcpy(HP.note_error,NowTimeToStr());       // Cтереть всю строку и поставить время
    strcat(HP.note_error," ");
    strcat(HP.note_error,nam);                  // Имя кто сгенерировал ошибку
    strcat(HP.note_error,": ");
    strcat(HP.note_error,noteError[abs(_err)]); // Описание ошибки
    journal.jprintf(pP_TIME,"$ERROR source: %s, code: %d\n",nam,_err);//journal.jprintf(", code: %d\n",_err);  
    if(xTaskGetSchedulerState()==taskSCHEDULER_RUNNING) HP.save_DumpJournal(true);  // вывод отладочной информации для начала  если запущена freeRTOS
    HP.message.setMessage(pMESSAGE_ERROR,HP.note_error,0);    // сформировать уведомление об ошибке
   }
    // Сюда ставить надо останов ТН !!!!!!!!!!!!!!!!!!!!!
   if (HP.get_State()!=pOFF_HP)    // Насос не ВЫКЛЮЧЕН есть что выключать
   { 
   if (HP.get_nStart()==0)  HP.sendCommand(pSTOP);        // Послать команду на останов ТН  если нет попыток повторного пуск
   else
   { // сюда ставить повторные пуски ТН при ошибке.
    if (HP.num_repeat<HP.get_nStart())                    // есть еще попытки
         {
            HP.sendCommand(pREPEAT);                     // Повторный пуск ТН
         }  
    else  HP.sendCommand(pSTOP);                         // Послать команду на останов ТН  БЕЗ ПОПЫТОК ПУСКА
   }
   }
  return HP.error;  
}


void setup() {
// 1. Инициализация SPI
// Баг разводки дуе (вероятность). Есть проблема с инициализацией spi.  Ручками прописываем
// https://groups.google.com/a/arduino.cc/forum/#!topic/developers/0PUzlnr7948
// http://forum.arduino.cc/index.php?topic=243778.0;nowap
pinMode(87,INPUT_PULLUP);                   // SD Pin 87
pinMode(77,INPUT_PULLUP);                   // Eth Pin 77  
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
// https://forum.arduino.cc/index.php?topic=288573.0  
pinMode(21, OUTPUT);  
  for (int i = 0; i < 8; i++) {
    digitalWriteDirect(21, HIGH); delayMicroseconds(3);
    digitalWriteDirect(21, LOW);  delayMicroseconds(3);
  }
  pinMode(21, INPUT);
  
// 2. Инициализация журнала и в нем последовательный порт
  journal.Init();

  #ifdef DEMO
     journal.jprintf("DEMO - DEMO - DEMO - DEMO - DEMO - DEMO - DEMO\n"); 
  #endif 
  journal.jprintf("Vesion firmware: %s\n",VERSION); 
  showID();                                                                  // информация о чипе
  journal.jprintf("Chip ID SAM3X8E: %s\n",getIDchip((char*)Socket[0].inBuf));// информация об серийном номере чипа
  if(GPBR->SYS_GPBR[0] & 0x80000000) GPBR->SYS_GPBR[0] = 0; else GPBR->SYS_GPBR[0] |= 0x80000000; // очистка старой причины
  lastErrorFreeRtosCode = GPBR->SYS_GPBR[0] & 0x7FFFFFFF;         // Сохранение кода ошибки при страте (причину перегрузки)
  journal.jprintf("Last reason for reset SAM3x: %s\n", ResetCause());
  journal.jprintf("Last Free RTOS task + error: 0x%04x\n", lastErrorFreeRtosCode);

  #ifdef PIN_LED1                            // Включение (точнее индикация) питания платы если необходимо
    pinMode(PIN_LED1,OUTPUT);  
    digitalWriteDirect(PIN_LED1, HIGH);
  #endif
  #ifdef POWER_CONTROL
    journal.jprintf("Power +5V, +3.3V on board: ON\n"); 
  #endif

//  #ifdef POWER_CONTROL
  SupplyMonitorON(SUPC_SMMR_SMTH_3_2V);           // включение монитора питания
//  #endif
   
  #ifdef DRV_EEV_L9333                     // Контроль за работой драйвера ЭРВ
    pinMode(PIN_STEP_DIAG,INPUT_PULLUP); 
    journal.jprintf("Control EEV driver L9333: ON \n"); 
  #else
    journal.jprintf("Control EEV driver no support \n"); 
  #endif
  
// 3. Инициализация и проверка шины i2c
   journal.jprintf("1. Setting and checking I2C device . . .\n");
 
   Wire.begin();
   uint8_t eepStatus=0;
   for(uint8_t i=0; i<I2C_NUM_INIT; i++ )
    {
    if ((eepStatus=eepromI2C.begin(I2C_SPEED))>0)    // переходим на 400 кгц  OK==0 все остальное ошибки и пытаемся инициализировать
      {
      journal.jprintf("$ERROR - I2C bus failed, status = %d\n",eepStatus);
      journal.jprintf("$WARNING - Repeat initialization I2C bus\n",eepStatus);
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
      }
   else {  journal.jprintf("I2C bus init on %d kHz - OK\n",I2C_SPEED/1000); break; }  // Все хорошо
    } // for
    
    if (eepStatus!=0)  // если шина не инициализирована делаем попытку запустится на малой частоте один раз!
     {
      if ((eepStatus=eepromI2C.begin(twiClock100kHz))>0) 
           journal.jprintf("$ERROR - I2C bus init failed on speed %d kHz, status = %d\n",twiClock100kHz/1000,eepStatus);
      else journal.jprintf("I2C bus low speed, init on %d kHz - OK\n",I2C_SPEED/1000);
     } 
    
     // Сканирование шины i2c
    if (eepStatus==0)   // есть инициализация
    {
        byte error, address;
        const byte start_address = 8;       // lower addresses are reserved to prevent conflicts with other protocols
        const byte end_address = 119;       // higher addresses unlock other modes, like 10-bit addressing
      
        for(address = start_address; address < end_address; address++ )
         {
              #ifdef ONEWIRE_DS2482         // если есть мост
              if(address == I2C_ADR_DS2482) {
            	  error = !OneWireBus.check_presence();
			    #ifdef ONEWIRE_DS2482_SECOND
              } else if(address == I2C_ADR_DS2482two) {
               	  error = !OneWireBus2.check_presence();
		        #endif
              } else {
            	  Wire.beginTransmission(address); error = Wire.endTransmission();
              }
              #else                          
              Wire.beginTransmission(address);
              error = Wire.endTransmission();
              #endif
              if(error==0)
              {
               journal.jprintf("I2C device found at address %s",byteToHex(address));
               switch (address)
                    {    
               	   	case I2C_ADR_DS2482two: journal.jprintf(" - OneWire DS2482-100 second\n");              break;
                    case I2C_ADR_DS2482:  	journal.jprintf(" - OneWire DS2482-100\n");  		            break; // 0x18 есть варианты
                    case I2C_ADR_EEPROM:	journal.jprintf(" - EEPROM AT24CXXX %d kBit\n",I2C_SIZE_EEPROM);break; // 0x50 возможны варианты
                    case I2C_ADR_RTC   :	journal.jprintf(" - RTC DS3231\n");                             break; // 0x68
                    default            :	journal.jprintf(" - Unknow\n");                                 break; // не определенный тип
                    }
              _delay(100);      
              }
          //    else journal.jprintf("I2C device bad endTransmission at address %s code %d",byteToHex(address), error);   
          } // for
    } //  (eepStatus==0) 

    if(OneWireBus.Init()) journal.jprintf("Error OneWire init: %d\n", OneWireBus.GetLastErr());
    else journal.jprintf("OneWire init Ok.\n");
#ifdef ONEWIRE_DS2482_SECOND
    if(OneWireBus2.Init()) journal.jprintf("Error OneWire2 init: %d\n", OneWireBus2.GetLastErr());
    else journal.jprintf("OneWire2 init Ok.\n");
#endif

// 4. Инициализировать основной класс
  journal.jprintf("2. Init %s main class . . .\n",(char*)nameHeatPump); 
  HP.initHeatPump();                                               // Основной класс

// 5. Установка сервисных пинов
   journal.jprintf("3. Read safe Network botton . . .\n");
   pinMode(PIN_KEY1, INPUT);               // Копка 1, Нажатие при включении - режим safeNetwork (настрока сети по умолчанию 192.168.0.177  шлюз 192.168.0.1, не спрашивает пароль на вход в веб морду)
   HP.safeNetwork=!digitalReadDirect(PIN_KEY1); 
   if (HP.safeNetwork)  journal.jprintf("Mode safeNetwork ON \n"); else journal.jprintf("Mode safeNetwork OFF \n"); 
  
   pinMode(PIN_BEEP, OUTPUT);              // Выход на пищалку
   pinMode(PIN_LED_OK, OUTPUT);            // Выход на светодиод мигает 0.5 герца - ОК  с частотой 2 герца ошибка
   digitalWriteDirect(PIN_BEEP,LOW);       // Выключить пищалку
   digitalWriteDirect(PIN_LED_OK,HIGH);    // Выключить светодиод

// 7. Инициализация СД карты и запоминание результата 3 попытки
   journal.jprintf("4. Init and checking SD card . . .\n"); 
   HP.set_fSD(initSD(SD_REPEAT));
   WDT_Restart(WDT);                          // Сбросить вачдог  иногда карта долго инициализируется
   digitalWriteDirect(PIN_LED_OK,LOW);        // Включить светодиод - признак того что сд карта инициализирована
   _delay(100);

// 8. Чтение ЕЕПРОМ
   journal.jprintf("5. Load data from EEPROM . . .\n"); 
//  HP.save();                                         // Записать настройки по умолчанию для перехода с демо на боевую
  if(HP.load_motoHour()==ERR_HEADER2_EEPROM)           // Загрузить счетчики ТН,
  {
   journal.jprintf("I2C eeprom is empty, save default setting\n");  
  // HP.save();                                          //если ошибка ERR_HEADER2_EEPROM то скорее всего память чистая, считывать нечего и записывам настроки по умолчанию
   HP.save_motoHour();
  } 
  else HP.load();                                      // Загрузить настройки ТН и текущий профиль
#ifdef USE_SCHEDULER
  HP.Schdlr.load();							// Загрузка настроек расписания
#endif

  // обновить хеш для пользователей
  HP.set_hashUser();
  HP.set_hashAdmin();

// 9. Сетевые настройки
   journal.jprintf("6. Setting Network . . .\n"); 
   initW5200(true);   // Инициализация сети с выводом инфы в консоль
   digitalWriteDirect(PIN_BEEP,LOW);          // Выключить пищалку
 
// 10. Разбираемся со всеми часами и синхронизацией
   journal.jprintf("7. Setting time and clock . . .\n"); 
   set_time();        
   
 // 11. Инициалазация уведомлений
   journal.jprintf("8. Message update IP from DNS . . .\n"); 
   HP.message.dnsUpdateStart(); 
   
 // 12. Инициалазация MQTT
    #ifdef MQTT  
      journal.jprintf("9. Client MQTT update IP from DNS . . .\n"); 
      HP.clMQTT.dnsUpdateStart();
    #else
      journal.jprintf("9. Client MQTT no support firmware\n");
    #endif 

  // 13. Инициалазация Statistics
   #ifdef I2C_EEPROM_64KB  
     HP.InitStatistics();                               // записать состояния счетчиков в RAM для начала работы статистики
     journal.jprintf("10. Init counter statictic.\n");  
  #else    
     journal.jprintf("10. Statistic no support (low eeprom).\n"); 
  #endif

#ifdef USE_SCHEDULER
   int8_t _profile = HP.Schdlr.calc_active_profile();
   if(_profile > SCHDLR_Profile_off && _profile != HP.Prof.get_idProfile()) {
	   HP.Prof.load(_profile);
	   HP.set_profile();
	   journal.jprintf("Profile changed to %d\n", _profile);
   }
#endif

  if(HP.get_SaveON()==0)  HP.set_HP_OFF();    // Сбросить флаг включение ТН если стоит соответсвующий флаг в опциях
  journal.jprintf("11. Delayed start %s: ",(char*)nameHeatPump); if(HP.get_HP_ON()) journal.jprintf("YES\n"); else journal.jprintf("NO\n");

  start_ADC(); // после инициализации HP
  journal.jprintf("12. Start read ADC sensors\n"); 

  #ifdef NEXTION   
    journal.jprintf("13. Init Nextion dispaly\n");
    myNextion.init(cZero);
  #else
    journal.jprintf("13. Nextion dispaly absent in config\n");
  #endif

  // Создание задач Free RTOS  ----------------------
    journal.jprintf("14. Create tasks free RTOS . . .\n");
HP.mRTOS=236;  //расчет памяти для задач 236 - размер данных шедуллера, каждая задача требует 64 байта+ стек (он в словах!!)
HP.mRTOS=HP.mRTOS+64+4*configMINIMAL_STACK_SIZE;  // задача бездействия
HP.mRTOS=HP.mRTOS+4*configTIMER_TASK_STACK_DEPTH;  // программные таймера

// ПРИОРИТЕТ 4 Высший приоритет датчики читаются всегда и шаговик ЭРВ всегда шагает если нужно
if (xTaskCreate(vReadSensor,"rSensor",300,NULL,4,&HP.xHandleReadSensor)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)    set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS);
HP.mRTOS=HP.mRTOS+64+4*300;//300 - начало глючить при переходе на либы vad711  увеличил до 330

#ifdef EEV_DEF
  if (xTaskCreate(vUpdateStepperEEV,"upStepper",100,NULL,4,&HP.dEEV.stepperEEV.xHandleStepperEEV)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)  set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
  HP.mRTOS=HP.mRTOS+64+4*100;//200
  vTaskSuspend(HP.dEEV.stepperEEV.xHandleStepperEEV);                                 // Остановить задачу
  HP.dEEV.stepperEEV.xCommandQueue = xQueueCreate( EEV_QUEUE, sizeof( int ) );  // Создать очередь комманд для ЭРВ
#endif

// ПРИОРИТЕТ 3 Очень высокий приоритет Выполнение команд управления (разбор очереди комманд) - должен быть выше чем задачи обновления ТН и ЭРВ
if (xTaskCreate(vUpdateCommand,"Command",300,NULL,3,&HP.xHandleUpdateCommand)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)     set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
HP.mRTOS=HP.mRTOS+64+4*300;//300  при 200 уже не работает инвертор!!!!! ПРОВЕРЕНО 250 - проблемы не в демо инертор
vTaskSuspend(HP.xHandleUpdateCommand);                              // Оставновить задачу разбор очереди комнад
vSemaphoreCreateBinary(HP.xCommandSemaphore);                       // Создание семафора
if (HP.xCommandSemaphore==NULL) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
                    
// ПРИОРИТЕТ 2 высокий - это управление ТН управление ЭРВ
if (xTaskCreate(vUpdate,"updateHP",350,NULL,2,&HP.xHandleUpdate)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)    set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
HP.mRTOS=HP.mRTOS+64+4*350;//400  при 300 вылет при ошибке
vTaskSuspend(HP.xHandleUpdate);                                 // Оставновить задачу обновление ТН

#ifdef EEV_DEF
  if (xTaskCreate(vUpdateEEV,"updateEEV",200,NULL,2,&HP.xHandleUpdateEEV)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)     set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
  HP.mRTOS=HP.mRTOS+64+4*200;  //200 Проверено минимум 200 при configMINIMAL_STACK_SIZE не работает, сыпится при выключении ТН
  vTaskSuspend(HP.xHandleUpdateEEV);                              // Оставновить задачу обновление EEV
#endif  

// ПРИОРИТЕТ 1 средний - обслуживание вебморды в несколько потоков и дисплея Nextion
#if    W5200_THREARD < 2 
  if ( xTaskCreate(vWeb0,"Web0", W5200_STACK_SIZE,NULL,1,&HP.xHandleUpdateWeb0)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
  HP.mRTOS=HP.mRTOS+64+4*W5200_STACK_SIZE;
#elif  W5200_THREARD < 3
  if ( xTaskCreate(vWeb0,"Web0", W5200_STACK_SIZE,NULL,1,&HP.xHandleUpdateWeb0)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS);
  HP.mRTOS=HP.mRTOS+64+4*W5200_STACK_SIZE;
  if ( xTaskCreate(vWeb1,"Web1", W5200_STACK_SIZE,NULL,1,&HP.xHandleUpdateWeb1)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
  HP.mRTOS=HP.mRTOS+64+4*W5200_STACK_SIZE;
#elif  W5200_THREARD < 4
  if ( xTaskCreate(vWeb0,"Web0", W5200_STACK_SIZE,NULL,1,&HP.xHandleUpdateWeb0)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS);
  HP.mRTOS=HP.mRTOS+64+4*W5200_STACK_SIZE;
  if ( xTaskCreate(vWeb1,"Web1", W5200_STACK_SIZE,NULL,1,&HP.xHandleUpdateWeb1)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
  HP.mRTOS=HP.mRTOS+64+4*W5200_STACK_SIZE;
  if ( xTaskCreate(vWeb2,"Web2", W5200_STACK_SIZE,NULL,1,&HP.xHandleUpdateWeb2)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
  HP.mRTOS=HP.mRTOS+64+4*W5200_STACK_SIZE;
#else
  if ( xTaskCreate(vWeb0,"Web0", W5200_STACK_SIZE,NULL,1,&HP.xHandleUpdateWeb0)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS);
  HP.mRTOS=HP.mRTOS+64+4*W5200_STACK_SIZE;
  if ( xTaskCreate(vWeb1,"Web1", W5200_STACK_SIZE,NULL,1,&HP.xHandleUpdateWeb1)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
  HP.mRTOS=HP.mRTOS+64+4*W5200_STACK_SIZE;
  if ( xTaskCreate(vWeb2,"Web2", W5200_STACK_SIZE,NULL,1,&HP.xHandleUpdateWeb2)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
  HP.mRTOS=HP.mRTOS+64+4*W5200_STACK_SIZE;
  if ( xTaskCreate(vWeb3,"Web3", W5200_STACK_SIZE,NULL,1,&HP.xHandleUpdateWeb3)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
  HP.mRTOS=HP.mRTOS+64+4*W5200_STACK_SIZE;
#endif

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

#ifdef NEXTION    
  if ( xTaskCreate(vNextion,"Nextion",200,NULL,1,&HP.xHandleUpdateNextion)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
  HP.mRTOS=HP.mRTOS+64+4*200; //200
  HP.updateNextion();  // Обновить настройки дисплея
#endif 

// ПРИОРИТЕТ 0 низкий - накопление статистики и задача работа насоса кондесатора в простое компрессора
if (xTaskCreate(vUpdateStat,"upStat",100,NULL,0,&HP.xHandleUpdateStat)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)  set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
HP.mRTOS=HP.mRTOS+64+4*100;  //100
vTaskSuspend(HP.xHandleUpdateStat);                              // Оставновить задачу обновление статистики
// Создание задачи по переодической работе насоса конденсатора
if (xTaskCreate(vUpdatePump,"upPump",200,NULL,0,&HP.xHandleUpdatePump)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)  set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
HP.mRTOS=HP.mRTOS+64+4*200; // 200
vTaskSuspend(HP.xHandleUpdatePump); 

// Создание задачи для отложенного пуска ТН
if (xTaskCreate(vPauseStart,"delayStart",200,NULL,3,&HP.xHandlePauseStart)==errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)  set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS); 
HP.mRTOS=HP.mRTOS+64+4*200;  // 200 - проверено при configMINIMAL_STACK_SIZE или 150 виснет при попытке повторного пуска при ошибке
vTaskSuspend(HP.xHandlePauseStart);  
if(HP.get_HP_ON()>0)  HP.sendCommand(pRESTART);  // если надо запустить ТН - отложенный старт

// Дополнительные семафоры (почему то именно здесь) Создается когда есть модбас
//if(Modbus.get_present())
//{  
// vSemaphoreCreateBinary(xModbusSemaphore);                       // Создание мютекса
// if (xModbusSemaphore==NULL) set_Error(ERR_MEM_FREERTOS,(char*)nameFREERTOS);
//}

journal.jprintf(" Create tasks - OK, size %d bytes\n",HP.mRTOS);
journal.jprintf("15. If you want to send a notification about resetting the controller . . .\n");
HP.message.setMessage(pMESSAGE_RESET,(char*)"Контроллер теплового насоса был сброшен",0);    // сформировать уведомление о сбросе контролла
journal.jprintf("16. Information about contoller:\n");
freeRamShow();
HP.startRAM=freeRam()-HP.mRTOS;   // оценка свободной памяти до пуска шедулера, поправка на 1054 байта
journal.jprintf("FREE MEMORY %d bytes\n",HP.startRAM); 
journal.jprintf("Temperature SAM3X8E: %.2f\n",temp_DUE()); 
journal.jprintf("Temperature DS2331: %.2f\n",getTemp_RtcI2C()); 
//HP.Stat.generate_TestData(STAT_POINT); // Сгенерировать статистику STAT_POINT точек только тестирование
journal.jprintf("Start Free RTOS scheduler :-))\n");
journal.jprintf("READY ----------------------\n");
eepromI2C.use_RTOS_delay = 1;       //vad711
vTaskStartScheduler();              // СТАРТ !!
journal.jprintf("CRASH Free RTOS!!!\n");
}


void loop() 
{
//web_server(); 
}

//  З А Д А Ч И -------------------------------------------------
// Это и есть поток с минимальным приоритетом измеряем простой процессора
//extern "C" 
//{
extern "C" void vApplicationIdleHook(void)  // FreeRTOS expects C linkage
{
	static boolean ledState = LOW;
	static unsigned long countLastTick = 0;
	static unsigned long countLED = 0;
	static unsigned long ulIdleCycleCount = 0;                                    // наш трудяга счетчик
	static unsigned long max_count = 0; // максимальное значение счетчика, вычисляется при калибровке и соответствует 100% CPU idle

	WDT_Restart(WDT);                                                            // Сбросить вачдог
	ulIdleCycleCount++;                                                          // приращение счетчика
	if(countLastTick > xTaskGetTickCount()) countLastTick = 0;                     // Переполнение

	if((xTaskGetTickCount() - countLastTick) > 4000)                // если прошло 4000 тиков (4 сек для моей платформы)
	{
		countLastTick = xTaskGetTickCount();                            // расчет нагрузки
		if(ulIdleCycleCount > max_count) max_count = ulIdleCycleCount; // это калибровка запоминаем максимальные значения
		HP.CPU_IDLE = (100 * ulIdleCycleCount) / max_count;              // вычисляем текущую загрузку
		ulIdleCycleCount = 0;

	} //if end  4 sec

	// Светодиод мигание в зависимости от ошибки и подача звукового сигнала при ошибке
	if(((long) xTaskGetTickCount() - countLED) > (unsigned long) TIME_LED_ERR) {
		if(HP.get_errcode() != OK) {
			countLED = xTaskGetTickCount();
			ledState = !ledState;         // Ошибка
			digitalWriteDirect(PIN_LED_OK, ledState);
			if(HP.get_Beep()) digitalWriteDirect(PIN_BEEP, ledState); // звукового сигнала
		} else if(((long) xTaskGetTickCount() - countLED) > (unsigned long) TIME_LED_OK)   // Ошибок нет и время пришло
		{
			countLED = xTaskGetTickCount();
			ledState = !ledState;       // ОК
			digitalWriteDirect(PIN_LED_OK, ledState);
			digitalWriteDirect(PIN_BEEP, LOW);
		}
	}
} //функция
//} // 

// --------------------------- W E B ------------------------
// Задача обслуживания web сервера
// Сюда надо пихать все что связано с сетью иначе конфликты не избежны
// Здесь также обслуживается посылка уведомлений MQTT
// Первый поток веб сервера - дополнительно нагружен различными сервисами
void vWeb0( void *pvParameters )
{ //const char *pcTaskName = "Web server is running\r\n";
   volatile unsigned long timeResetW5200=0;
   volatile unsigned long thisTime=0;
   volatile unsigned long resW5200=0;
   volatile unsigned long iniW5200=0;
   volatile unsigned long pingt=0;
   volatile unsigned long narmont=0;
   volatile unsigned long mqttt=0;
   volatile boolean active=true;  // ФЛОГ Одно дополнительное действие за один цикл - распределяем нагрузку
   
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
          if ((HP.get_fInitW5200())&&(thisTime-iniW5200>60*1000UL)&&(active))    // проверка связи с чипом сети раз в минуту
          {
           iniW5200=thisTime;
           if (!(linkStatusWiznet(false)))
             {
              journal.jprintf(pP_TIME,"Connection with the chip %s is consumed, resetting . . .\n", nameWiznet);
              HP.sendCommand(pNETWORK);       // Если связь потеряна то подать команду на сброс сетевого чипа
              HP.num_resW5200++;              // Добавить счетчик инициализаций
              active=false;
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
void vWeb1( void *pvParameters )
{ //const char *pcTaskName = "Web server is running\r\n";
   for( ;; )
    {
      web_server(1);
     vTaskDelay(TIME_WEB_SERVER/portTICK_PERIOD_MS); // задержка чтения уменьшаем загрузку процессора
 
    }
  vTaskDelete( NULL );  
}
// Третий поток
void vWeb2( void *pvParameters )
{ //const char *pcTaskName = "Web server is running\r\n";
   for( ;; )
    {
      web_server(2);
     vTaskDelay(TIME_WEB_SERVER/portTICK_PERIOD_MS); // задержка чтения уменьшаем загрузку процессора
 
    }
  vTaskDelete( NULL );  
}
// Четвертый поток
void vWeb3( void *pvParameters )
{ //const char *pcTaskName = "Web server is running\r\n";
   for( ;; )
    {
      web_server(3);
      vTaskDelay(TIME_WEB_SERVER/portTICK_PERIOD_MS); // задержка чтения уменьшаем загрузку процессора
     }
  vTaskDelete( NULL );  
}

// Задача обслуживания Nextion
void vNextion( void *pvParameters )
{ 
  static unsigned long NextionTick=0;
    for( ;; )
    {
     #ifdef NEXTION    
      myNextion.Listen();                  // прочитать сообщения от дисплея
      if(((long)xTaskGetTickCount()-NextionTick ) >  NEXTION_UPDATE)   
      {
        NextionTick=xTaskGetTickCount();
        myNextion.Update();                  // Обновление дисплея
      }
     #endif
      vTaskDelay(NEXTION_READ/portTICK_PERIOD_MS); // задержка чтения уменьшаем загрузку процессора
    }
  vTaskDelete( NULL );  
}

// Задача обновление статистики
void vUpdateStat( void *pvParameters )
{ //const char *pcTaskName = "statChart is running\r\n";
   for( ;; )
    {
      HP.updateChart();                                       // Обновить статитсику
      vTaskDelay((HP.get_tChart()*1000)/portTICK_PERIOD_MS); // задержка чтения уменьшаем загрузку процессора
     }
  vTaskDelete( NULL );  
}

//////////////////////////////////////////////////////////////////////////
// Задача чтения датчиков
void vReadSensor(void *pvParameters)
{ //const char *pcTaskName = "ReadSensor\r\n";
	volatile unsigned long readFC = 0;
	volatile unsigned long readSDM = 0;
	uint32_t ttime;                                                 // новое время
	uint32_t oldTime;                                               // старое вермя
	uint32_t countI2C = TimeToUnixTime(getTime_RtcI2C());             // Последнее врямя обновления часов
	int8_t i;

	for(;;) {
		watchdogReset();

#ifndef DEMO  // Если не демо
		if(OneWireBus.PrepareTemp()) set_Error(ERR_ONEWIRE, (char*) __FUNCTION__);
#ifdef ONEWIRE_DS2482_SECOND
		if(OneWireBus2.PrepareTemp()) set_Error(ERR_DS2482_ONEWIRE, (char*)__FUNCTION__);
#endif
#endif     // не DEMO
		vReadSensor_delay10ms(cDELAY_DS1820 / 10); 						 // Ожитать время преобразования

		for(i = 0; i < INUMBER; i++) HP.sInput[i].Read();                // Прочитать данные сухой контакт
		for(i = 0; i < FNUMBER; i++) HP.sFrequency[i].Read();            // Получить значения датчиков потока
		for(i = 0; i < ANUMBER; i++) HP.sADC[i].Read();                  // Прочитать данные с датчика давления
		for(i = 0; i < TNUMBER; i++) {                                   // Прочитать данные с температурных датчиков
			//uint32_t m1 = micros();
			HP.sTemp[i].Read();
			//uint32_t m2 = micros(); //Serial.print(HP.sTemp[i].get_name()); Serial.print(':'); Serial.print(m2 - m1); Serial.print(", ");
			_delay(2);     												// пауза
		}
		//Serial.print("\n");

		// Вычисление перегрева используются РАЗНЫЕ датчики при нагреве и охлаждении
		// Режим работы определяется по состоянию четырехходового клапана при его отсутвии только нагрев
#ifdef EEV_DEF
		if((HP.get_mode() != pCOOL) || (HP.get_mode() != pNONE_C))    // Если не охлаждение
			HP.dEEV.set_Overheat(HP.sTemp[TRTOOUT].get_Temp(), HP.sTemp[TEVAOUT].get_Temp(), HP.sTemp[TEVAIN].get_Temp(), HP.sADC[PEVA].get_Press());   // Нагрев (включен)
		else HP.dEEV.set_Overheat(HP.sTemp[TRTOOUT].get_Temp(), HP.sTemp[TCONIN].get_Temp(), HP.sTemp[TCONOUT].get_Temp(), HP.sADC[PEVA].get_Press());   // Охлаждение
#endif

		vReadSensor_delay10ms(TIME_READ_SENSOR / 10);     // Ожитать время нужное для цикла чтения

		//  Опрос состояния инвертора
		if((HP.dFC.get_present()) && (xTaskGetTickCount() - readFC > FC_TIME_READ)) {
			readFC = xTaskGetTickCount();
			HP.dFC.get_readState();
		}
#ifdef USE_ELECTROMETER_SDM   // Опрос состяния счетчика
		if ((HP.dSDM.get_present())&&(xTaskGetTickCount()-readSDM>SDM_TIME_READ))
		{
			readSDM=xTaskGetTickCount();
			HP.dSDM.get_readState();
		}
#endif

#ifdef DRV_EEV_L9333  // Опрос состяния драйвера ЭРВ
		if (digitalReadDirect(PIN_STEP_DIAG)) // Перечитываем два раза
		{
			vReadSensor_delay10ms(5);
			if (digitalReadDirect(PIN_STEP_DIAG)) set_Error(ERR_DRV_EEV,(char*)__FUNCTION__); // Контроль за работой драйвера ЭРВ
		}
#endif

		//  Синхронизация часов с I2C часами если стоит соответсвующий флаг
		if(HP.get_updateI2C())  // если надо обновить часы из I2c
		{
			if((oldTime = rtcSAM3X8.unixtime()) - countI2C > TIME_I2C_UPDATE) // время пришло обновляться надо Период синхронизации внутренних часов с I2C часами (сек)
			{
				ttime = TimeToUnixTime(getTime_RtcI2C());       // Прочитать время из часов i2c тут проблема
				rtcSAM3X8.set_clock(ttime, 0);                 // Установить внутренние часы по i2c
				HP.updateDateTime((int32_t) (ttime - oldTime));  // Обновить переменные времени с новым значением часов
				journal.jprintf((const char*) "Sync form i2c RTC DS3231: %s %s\n", NowDateToStr(), NowTimeToStr()); // тут может быть засада переменные для хранения строк
				countI2C = ttime;                               // запомнить время
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
			} else countTEMP += (cDELAY_DS1820 + TIME_READ_SENSOR + 2 * TNUMBER) / 100; // в 0.1 сек
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

	}  // for
	vTaskDelete( NULL);
}

// Вызывается во время задержек в задаче чтения датчиков
void vReadSensor_delay10ms(uint16_t msec)
{
	while(msec--) {
		_delay(10);
#ifdef  KEY_ON_OFF // Если надо проверяем кнопку включения ТН нажимать надо более 4 сек  по хорошему надо это переделать - переместить в более быстрыю задачу
		static boolean Key1_ON = HIGH;                                   // кнопка вкл/вкл дребез подавление
		if ((!digitalReadDirect(PIN_KEY1))&&(Key1_ON)) {
			_delay(100);
			//if(msec > 100) msec -= 100; else msec = 0;
			if (!digitalReadDirect(PIN_KEY1)) {  // дребезг
				journal.jprintf("Press KEY_ON_OFF\n");
				if (HP.get_State()==pOFF_HP) HP.sendCommand(pSTART); else HP.sendCommand(pSTOP);
			}
		} else Key1_ON=digitalReadDirect(PIN_KEY1); // запоминаем состояние
#endif
	}
}
//////////////////////////////////////////////////////////////////////////
// Задача Управление тепловым насосом
 void vUpdate( void *pvParameters )
{ //const char *pcTaskName = "HP_Update\r\n";
  static unsigned long RPUMPBTick=0;
  for( ;; )
  {
    if (HP.get_State()==pWORK_HP){ //Код обслуживания работы ТН выполняется только если состяние ТН - работа а вот расписание всегда выполняется
     // 1. Обновится, В это время команды управления не выполняются!!!!!
     if (SemaphoreTake(HP.xCommandSemaphore,0)==pdPASS)                                           // Cемафор  захвачен
       { 
       if (HP.get_State()==pWORK_HP)  HP.vUpdate();                                                 // ТН работает и идет процесс контроля
       SemaphoreGive(HP.xCommandSemaphore);                                                        // Семафор отдан
       }
     // 2. Отработка пауз
     if ((HP.get_State()==pOFF_HP)||(HP.get_State()==pSTOPING_HP))                                     // Если  насос не работает или идет останов насоса то остановить задачу Обновления ТН (Вообще то это лишнее, надо убрать)
     {
      journal.jprintf((const char*)" WARNING: Stop task update %s from vUpdate?\n",(char*)nameHeatPump); 
      vTaskSuspend(HP.xHandleUpdate);    //???????????????         
      } 
     else   // Время паузы очень разное в зависимости от настроек

        if (HP.get_mode())    // true отопление
            {
            if  (HP.get_ruleHeat()==pHYSTERESIS)  vTaskDelay(TIME_CONTROL/portTICK_PERIOD_MS);       // Гистерезис
            else
                #ifdef DEMO
                   vTaskDelay(10*1000/portTICK_PERIOD_MS);                                           // для демо 10 сек
                #else 
                    vTaskDelay(FC_UPTIME/portTICK_PERIOD_MS);                                     // Время интегрирования ПИД  секунды
                #endif    
            }
        else                 // Охлаждение
           {
            if  (HP.get_ruleCool()==pHYSTERESIS)  vTaskDelay(TIME_CONTROL/portTICK_PERIOD_MS);        // Гистерезис
             else 
                 #ifdef DEMO
                   vTaskDelay(10*1000/portTICK_PERIOD_MS);                                            // для демо 10 сек
                #else 
                   vTaskDelay(FC_UPTIME/portTICK_PERIOD_MS);                                       // Время интегрирования ПИД секунды
                #endif   
            };
            
      // 3. Управление циркуляционным насосом ГВС
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
     } // НЕ ОЖИДАНИЕ if HP.get_State()==pWAIT_HP)
     // Расписание проверка всегда
	   #ifdef USE_SCHEDULER
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
						}
						vTaskSuspendAll();	// без проверки
						HP.Prof.load(_profile);
						HP.set_profile();
						xTaskResumeAll();
						journal.jprintf("Profile changed to %d\n", _profile);
						if(frestart) HP.sendCommand(pRESUME);
					}
				} else if(HP.get_State() == pOFF_HP) {
					HP.sendCommand(pRESUME);
				}
			}
		}
	   #endif
     // Расписание

   }// for
 vTaskDelete( NULL ); 
}

// Задача Управление ЭРВ
#ifdef EEV_DEF
void vUpdateEEV( void *pvParameters )
{ //const char *pcTaskName = "HP_UpdateEEV\r\n";
  static int16_t  cmd=0;
  for( ;; )
  {
 //  if ((rtcSAM3X8.unixtime()-HP.get_startTime())>DELAY_ON1_EEV)    // ЭРВ контролирует если прошла задержка после включения ТН (первый раз)
  if ((rtcSAM3X8.unixtime()-HP.get_startCompressor())>DELAY_ON_PID_EEV)    // ЭРВ контролирует если прошла задержка после включения компрессора (пауза перед началом работы ПИД)
   {
    // Для большей надежности если очередь заданий на шаговик пуста поставить флаг отсутвия движения
    // Если очередь пуста а флаг что есть движение - предупреждение потеря синхронизации ЭРВ  и сброс флага
    if ((xQueuePeek(HP.dEEV.stepperEEV.xCommandQueue,&cmd,0)==errQUEUE_EMPTY)&&(HP.dEEV.stepperEEV.isBuzy()))
      {
 //     journal.jprintf("$WARNING! Loss of sync EEV\n"); 
      HP.dEEV.stepperEEV.offBuzy();  // признак Мотор остановлен
      }
 
     // Обновить и выполнить итерацию по контролю ЭРВ Для алгоритма таблица передаем СРЕДНИЕ (IN+OUT)/2 температуры
     HP.dEEV.Update((HP.sTemp[TEVAOUT].get_Temp()+HP.sTemp[TEVAIN].get_Temp())/2,(HP.sTemp[TCONOUT].get_Temp()+HP.sTemp[TCONIN].get_Temp())/2); 
     
    if ((HP.get_State()==pOFF_HP)||(HP.get_State()==pSTOPING_HP))                // Если  насос не работает или идет останов насоса то остановить задачу Обновления ЭРВ
     {
      journal.jprintf((const char*)" Stop task update EEV\n"); 
      vTaskSuspend(HP.xHandleUpdateEEV); 
      continue;                             // продолжение задачи работы ЭРВ начитается с этого места, по этому сразу на начало цикла контроля
      }   
     else    // штатная пауза (в зависимости от настроек)
         if  ((HP.dEEV.get_ruleEEV()==TEVAOUT_PEVA)||(HP.dEEV.get_ruleEEV()==TRTOOUT_PEVA))   vTaskDelay(HP.dEEV.get_timeIn()*1000/portTICK_PERIOD_MS);  // интегрирование ПИД
         else                                                                                 vTaskDelay(TIME_EEV/portTICK_PERIOD_MS);                   // Ожитать TIME_EEV  задержка в мсек.  для все остальных режимов
   
     } //if ((rtcSAM3X8.unixtime()-HP.get_startCompressor())>DELAY_ON_PID_EEV) 
     else vTaskDelay(TIME_EEV/portTICK_PERIOD_MS);        // Просто задержка ЭРВ не рабоатет
   } // for
 vTaskDelete( NULL ); 
}
#endif
// Задача Разбор очереди команд
void vUpdateCommand( void *pvParameters )
{ //const char *pcTaskName = "HP_UpdateCommand\r\n";
  for( ;; )
  {
//  if (SemaphoreTake(HP.xCommandSemaphore,(30*1000/portTICK_PERIOD_MS))==pdPASS)                // Cемафор  захвачен
    HP.runCommand();                                                                            // Выполнение команд управления ТН
//  SemaphoreGive(HP.xCommandSemaphore);                                                         // Семафор отдан
//    vTaskDelay(TIME_COMMAND/portTICK_PERIOD_MS); 
    vTaskSuspend(HP.xHandleUpdateCommand);    // Команды выполнены, остановить задачу, пуск осуществляется при посылке команды
  }
 vTaskDelete( NULL ); 
}  

// Задача обеспечения движения шаговика EEV
#ifdef EEV_DEF
void vUpdateStepperEEV( void *pvParameters )
{ //const char *pcTaskName = "HP_UpdateStepperEEV\r\n";
  static int16_t  cmd=0;
  volatile int16_t steps_left=0, step_number=0, start_pos=0, pos=0;
  volatile boolean direction=true;
  
  for( ;; )
  {
    // Полный цикл движения шаговика с разгребанием очереди команд,
    // В очереди лежат АБСОЛЮТНЫЕ координаты
    // При этом если очередь содержит более одной команды - просто суммируем все команды и двигаемся по итоговой сумме
    // Это значит что шаговик не успевает за темпом выдачи команд программой. Экономим время
     
    // 1. Чтение очереди команд, для выяснения все таки куда надо двигаться, переходим на относительные координаты
     pos=0; // текущее суммарное движение - обнулится
     start_pos=HP.dEEV.EEV;  // получить текущее положение шаговика абсолютное в начале очереди
  //   step_number=HP.dEEV.EEV; 
  //  Serial.print("1. step_number=");   Serial.print(step_number); Serial.print(" EEV=");   Serial.println(HP.dEEV.EEV); 
      // 3. Движение
     while (xQueueReceive(HP.dEEV.stepperEEV.xCommandQueue,&cmd,0)==pdPASS)    // Читаем очередь пока есть чего читать
      {
          pos=pos+(cmd-start_pos);                                            // Суммируем все приращения для получение итогового движения
          start_pos=cmd;                                                      // итоговая абсолютная координата отдельной команды
  //        Serial.print("2. Read Queu cmd: ");   Serial.println(cmd); 
  //        if (HP.dEEV.setZero) { step_number=HP.dEEV.stepperEEV.number_of_steps;  break;}  // Если выполняется команда установки 0 то все остальные команды игнорируются до ее выполнения.
            // Если выполняется команда установки 0 то все остальные команды игнорируются до ее выполнения.
            if (HP.dEEV.setZero) { step_number=(HP.dEEV.stepperEEV.number_of_steps/8)*8+32;/*pos=-530;*/ break;} // Должно делится на 8 и 4 без остатка
      }   

      // if (pos==0) continue; // двигать нечего
 //      Serial.print("3. Sum command=");   Serial.println(pos);  
      // 2. Подготовка к движению
      steps_left = abs(pos);                                    // Определить абсолютное число шагов движения он уменьшается до 0
      // НАПРАВЛЕНИЕ determine direction based on whether steps_to_mode is + or -:
      if (pos > 0)  direction = true; 
      if (pos < 0)  direction = false; 

      
   //    Serial.print("3. step_number=");   Serial.print(step_number); Serial.print(" EEV=");   Serial.println(HP.dEEV.EEV); 
      // 3. Движение
      while (steps_left>0)
      {
         if (direction)  // направление в увеличение
          {
            step_number++;
            HP.dEEV.EEV++;                        
          }
          else                      // направление в уменьшение
          {
            step_number--;
            HP.dEEV.EEV--; 
          }
          steps_left--;                                                      // уменьшить счетчик шагов
          if ((step_number<0)&&(!HP.dEEV.setZero)) journal.jprintf(" Warring step_number<0\n"); 
          #if EEV_PHASE==PHASE_4  // 4 фазы движения
              HP.dEEV.stepperEEV.stepOne(abs(step_number % 4));                  // Сделать один шаг //
          #else                     // остальные варианты  8 фаз движения
              HP.dEEV.stepperEEV.stepOne(abs(step_number % 8));                   // Сделать один шаг //
       //     HP.dEEV.stepperEEV.stepOne(abs(HP.dEEV.EEV % 8));                   // Дмитрий говорит что это не правильно устанавливает 0 (открывает????)
          #endif
             
       //     HP.dEEV.stepperEEV.stepOne(abs(HP.dEEV.EEV % 8));                   // Дмитрий говорит что это не правильно устанавливает 0 (открывает????)
         vTaskDelay(HP.dEEV.stepperEEV.step_delay/portTICK_PERIOD_MS);      // Ожитать step_delay для следующего шага.
      }
         
      vTaskDelay(500/portTICK_PERIOD_MS);               // -!            // пауза  0.5 секунда ОБЯЗАТЕЛЬНО, иначе не сбрасывается isBuzy() НЕПОНЯТНО
      
      if (HP.dEEV.setZero) {HP.dEEV.setZero=false; HP.dEEV.EEV=0; step_number=0;}  // если стоит признак установки нуля, обнулить и сбросить признак
   //   Serial.print("4. EEV=");   Serial.print(HP.dEEV.EEV); Serial.print("  step_number=");   Serial.println(step_number);  
        
      // 4. Остановить выполнение команад, если очередь пуста, но могли накидать пока двигались
      if (xQueuePeek(HP.dEEV.stepperEEV.xCommandQueue,&cmd,0)==errQUEUE_EMPTY)
      {
   //     Serial.println("6. vTaskSuspend ");   
        HP.dEEV.stepperEEV.offBuzy();                                                            // признак Мотор остановлен
       if (!EEV_HOLD_MOTOR) HP.dEEV.stepperEEV.off();                                            // выключить двигатель если нет удержания
        vTaskSuspend(HP.dEEV.stepperEEV.xHandleStepperEEV);                                      // Приостановить задучу
      } 
   // Дошли до сюда новая, очередь не пуста и новая итерация по разбору очереди
  //    Serial.println("5. new command ");  
  } // for
 vTaskDelete( NULL ); 
}
#endif

// Задача "Работа насоса конденсатора при выключенном компрессоре"
void vUpdatePump( void *pvParameters )
{ //const char *pcTaskName = "Pump is running\r\n";
 uint16_t i;
   for( ;; )
    {
   //   if (!HP.startPump) {journal.jprintf(" Task vUpdatePump RPUMPO off  . . .\n");  vTaskSuspend(HP.xHandleUpdatePump);  }       // Остановить задачу насос
      if ((HP.get_workPump()==0)&&(HP.startPump)) {HP.dRelay[PUMP_OUT].set_OFF();  vTaskDelay(2000/portTICK_PERIOD_MS); }         // все время выключено  но раз в 2 секунды проверяем
         else if ((HP.get_pausePump()==0)&&(HP.startPump)) {HP.dRelay[PUMP_OUT].set_ON();  vTaskDelay(2000/portTICK_PERIOD_MS);}  // все время включено  но раз в 2 секунды проверяем
          else if(HP.startPump)                                                                                                   // нормальный цикл вкл выкл
              {
                  if (HP.startPump) HP.dRelay[PUMP_OUT].set_OFF();                 // выключить насос отопления
                  for(i=0;i<HP.get_pausePump()*60/2;i++)                           // Режем задержку для быстрого выхода
                    {            
                     if (!HP.startPump)  break;                                    // Остановить задачу насос
                     vTaskDelay(2*1000/portTICK_PERIOD_MS);                        // пауза выключено2 секунда
                    } 
                  if (HP.startPump) HP.dRelay[PUMP_OUT].set_ON();                  // включить насос отопления
                  for(i=0;i<HP.get_workPump()*60/2;i++)                            // Режем задержку для быстрого выхода
                    {            
                     if (!HP.startPump)  break;                                    // Остановить задачу насос
                     vTaskDelay(2*1000/portTICK_PERIOD_MS);                        // пауза выключено 2 секунда
                    }                  
              }        
      }  //for       
  vTaskDelete( NULL );  
}

// Задача отложеного старта ТН
// используется при старте контроллера если есть запись состояния
// также используется для повторных попыток пуска контроллера
void vPauseStart( void *pvParameters )
{ 
 volatile int16_t i, tt;
   for( ;; )
    {
     HP.PauseStart=false;               // мы в начале задачи ставим флаг
     journal.jprintf(pP_TIME,(const char*)"Start vPauseStart\n"); 
     #ifdef DEMO
      tt=30;
     #else 
        if (HP.isCommand()== pRESTART)   tt=DELAY_START_RES; else tt=DELAY_REPEAD_START;  // Определение времени задержки
     #endif
      // задержка перед пуском ТН
      for(i=tt;i>0;i=i-10) // задержка перед стартом обратный отсчет
       { 
          if (HP.PauseStart) break;               // если задача пущена не сначала
          if(i % 60 == 0) journal.jprintf((const char*)"Start over %d sec . . .\n",i);
//          if (HP.PauseStart) break;               // если задача пущена не сначала
          vTaskDelay(10*1000/portTICK_PERIOD_MS); // задержка перед повторным пуском ТН, ШАГ 10 секунд
          if (HP.PauseStart) break;               // если задача пущена не сначала
   //       if ((i==DELAY_REPEAD_START/2)&&(HP.get_State()== pREPEAT)) 
          if ((i==DELAY_REPEAD_START/2)&&(HP.isCommand()== pREPEAT)) 
                {
                  HP.eraseError();  
                  if (HP.PauseStart) break;               // если задача пущена не сначала
                  journal.jprintf((const char*)"Erase error %s\n",(char*)nameHeatPump);
                }
       }

       if (!HP.PauseStart)                    // если задача пущена сначала то запускаемся
       {
        HP.sendCommand(pAUTOSTART);
 //       vTaskSuspend(HP.xHandlePauseStart);  // Останов задачи выполнение отложенного старта
       } 
     vTaskSuspend(HP.xHandlePauseStart);  // Останов задачи выполнение отложенного старта
          
    }
   journal.jprintf((const char*)"Delete task vPauseStart?\n");     
   vTaskDelete( NULL );  
}


