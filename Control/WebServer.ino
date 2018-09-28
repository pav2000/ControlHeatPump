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
// ------------------------------------------------------------------------------------------------------
// Простой веб серевер с обработкой запросов
// ------------------------------------------------------------------------------------------------------
#include "HeatPump.h"
#include "utility/socket.h" 
#include "FreeRTOS_ARM.h"     
// HTTP типы запросов
#define HTTP_invalid   0
#define HTTP_GET       1      // Обычный запрос GET
#define HTTP_REQEST    2      // Наш запрос "GET /&"
#define HTTP_POST      3      // POST передача файла
#define HTTP_POST_     4      // POST - подготовка
#define UNAUTHORIZED   5      // Авторизация не пройдена
#define BAD_LOGIN_PASS 6      // Не верный логин или пароль


extern "C" void TaskGetRunTimeStats(void);
extern void  get_txtSettings(uint8_t thread);
extern void  get_fileState(uint8_t thread);
extern void  get_fileSettings(uint8_t thread);
extern void  get_txtJournal(uint8_t thread);
extern uint16_t get_csvStatistic(uint8_t thread);
extern void  get_datTest(uint8_t thread);
extern uint16_t get_csvChart(uint8_t thread);
extern int16_t  get_indexNoSD(uint8_t thread);
extern void  noCsvChart_SD(uint8_t thread);


// Названия режимов теста
const char *noteTestMode[] =   {"NORMAL","SAFE_TEST","TEST","HARD_TEST"};
// Описание режима теста
static const char *noteRemarkTest[] = {"Тестирование отключено. Основной режим работы",
                                       "Значения датчиков берутся из соответвующих полей ""Тест"", работа исполнительны устройств эмулируется. Безопасно.",
                                       "Значения датчиков берутся из соответвующих полей ""Тест"", исполнительные устройства работают за исключениме компрессора (FC и RCOMP). Почти безопасно.",
                                       "Значения датчиков берутся из соответвующих полей ""Тест"", все исполнительные устройства работают. Внимание! Может быть поврежден компрессор!."};
                               
                               
const char* file_types[] = {"text/html", "image/x-icon", "text/css", "application/javascript", "image/jpeg", "image/png", "image/gif", "text/plain", "text/ajax"};

const char*  pageUnauthorized     = {"HTTP/1.0 401 Unauthorized\r\nWWW-Authenticate: Basic real_m=Admin Zone\r\nContent-Type: text/html\r\nAccess-Control-Allow-Origin: *\r\n\r\n"};
const char* HEADER_FILE_NOT_FOUND = {"HTTP/1.1 404 Not Found\r\n\r\n<html>\r\n<head><title>404 NOT FOUND</title><meta charset=\"utf-8\" /></head>\r\n<body><h1>404 NOT FOUND</h1></body>\r\n</html>\r\n\r\n"};
//const char* HEADER_FILE_WEB       = {"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: keep-alive\r\n\r\n"}; // КЕШ НЕ ИСПОЛЬЗУЕМ
const char* HEADER_FILE_WEB       = {"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nCache-Control: max-age=3600, must-revalidate\r\n\r\n"}; // КЕШ ИСПОЛЬЗУЕМ
const char* HEADER_FILE_CSS       = {"HTTP/1.1 200 OK\r\nContent-Type: text/css\r\nConnection: keep-alive\r\nCache-Control: max-age=3600, must-revalidate\r\n\r\n"}; // КЕШ ИСПОЛЬЗУЕМ

const char* HEADER_ANSWER         = {"HTTP/1.1 200 OK\r\nContent-Type: text/ajax\r\nAccess-Control-Allow-Origin: *\r\n\r\n"};  // начало ответа на запрос
const char* NO_SUPPORT            = {"no support"};                                                                            // сокращение места
const char* NO_STAT               = {"Statistics are not supported in the firmware"};

const char *postRet[]            = {"Настройки из выбранного файла восстановлены, CRC16 OK\r\n\r\n",                           //  Ответы на пост запросы
									"Ошибка восстановления настроек из файла (см. журнал)\r\n\r\n",
									"", // пустая строка - ответ не выводится
									"Файлы загружены, подробности смотри в журнале\r\n\r\n",
									"Ошибка загрузки файла, подробности смотри в журнале\r\n\r\n",
									"Внутренняя ошибка парсера post запросов\r\n\r\n",
									"Файл настроек для разбора не влезает во внутренний буфер (max 6144 bytes)\r\n\r\n"
									};


#define ADD_WEBDELIM(str) strcat(str, WEBDELIM)

void web_server(uint8_t thread)
{
	int32_t len;
	int8_t sock;

	if(SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE) {
		return;
	}          // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим

	Socket[thread].sock = -1;                      // Сокет свободный

	SPI_switchW5200();                    // Это лишнее но для надежности пусть будет
	for(sock = 0; sock < W5200_SOCK_SYS; sock++)  // Цикл по сокетам веб сервера!!!! служебный не трогаем!!
	{

#if    W5200_THREARD < 2
		if (Socket[0].sock==sock) continue;   // исключение повторного захвата сокетов
#elif  W5200_THREARD < 3
		if((Socket[0].sock==sock)||(Socket[1].sock==sock)) continue; // исключение повторного захвата сокетов
#elif  W5200_THREARD < 4
		if((Socket[0].sock == sock) || (Socket[1].sock == sock) || (Socket[2].sock == sock)) continue; // исключение повторного захвата сокетов
#else
		if((Socket[0].sock==sock)||(Socket[1].sock==sock)||(Socket[2].sock==sock)||(Socket[3].sock==sock)) continue; // исключение повторного захвата сокетов
#endif

		// Настройка  переменных потока для работы
		Socket[thread].http_req_type = HTTP_invalid;        // нет полезной инфы
		SETBIT0(Socket[thread].flags, fABORT_SOCK);          // Сокет сброса нет
		SETBIT0(Socket[thread].flags, fUser); // Признак идентификации как обычный пользователь (нужно подменить файл меню)
		Socket[thread].client = server1.available_(sock);  // надо обработать
		Socket[thread].sock = sock;                           // запомнить сокет с которым рабоатет поток

		if(Socket[thread].client) // запрос http заканчивается пустой строкой
		{
			while(Socket[thread].client.connected()) {
				// Ставить вот сюда
				if(Socket[thread].client.available()) {
					len = Socket[thread].client.get_ReceivedSizeRX();                  // получить длину входного пакета
					if(len > W5200_MAX_LEN - 1) len = W5200_MAX_LEN - 1; // Ограничить размером в максимальный размер пакета w5200
					Socket[thread].client.read(Socket[thread].inBuf, len);                      // прочитать буфер
					Socket[thread].inBuf[len] = 0;                                // обрезать строку
					// Ищем в запросе полезную информацию (имя файла или запрос ajax)
#ifdef LOG
					journal.jprintf("$INPUT: %s\n",(char*)Socket[thread].inBuf);
#endif
					// пройти авторизацию и разобрать заголовок -  получить имя файла, тип, тип запроса, и признак меню пользователя
					Socket[thread].http_req_type = GetRequestedHttpResource(thread);
#ifdef LOG
					journal.jprintf("\r\n$QUERY: %s\r\n",Socket[thread].inPtr);
#endif
					switch(Socket[thread].http_req_type)  // По типу запроса
					{
					case HTTP_invalid: {
#ifndef DEBUG
						journal.jprintf("WEB:Error GET request\n");
#endif
						sendConstRTOS(thread, "HTTP/1.1 Error GET request\r\n\r\n");
						break;
					}
					case HTTP_GET:     // чтение файла
					{
						// Для обычного пользователя подменить файл меню, для сокращения функционала
						if((GETBIT(Socket[thread].flags, fUser)) && (strcmp(Socket[thread].inPtr, "menu.js") == 0)) strcpy(Socket[thread].inPtr, "menu-user.js");
						urldecode(Socket[thread].inPtr, Socket[thread].inPtr, len);
						readFileSD(Socket[thread].inPtr, thread);
						break;
					}
					case HTTP_REQEST:  // Запрос AJAX
					{
						strcpy(Socket[thread].outBuf, HEADER_ANSWER);   // Начало ответа
						parserGET(Socket[thread].inPtr, Socket[thread].outBuf, sock);    // выполнение запроса
#ifdef LOG
						journal.jprintf("$RETURN: %s\n",Socket[thread].outBuf);
#endif
						if(sendBufferRTOS(thread, (byte*) (Socket[thread].outBuf), strlen(Socket[thread].outBuf)) == 0) journal.jprintf("$Error send buf:  %s\n", (char*) Socket[thread].inBuf);
						break;
					}
					case HTTP_POST:    // загрузка настроек
					{
                        uint8_t ret= parserPOST(thread, len);         // разобрать и получить тип ответа
                        strcpy(Socket[thread].outBuf, HEADER_ANSWER);
						strcat(Socket[thread].outBuf,postRet[ret]);   // вернтуть текстовый ответ, котороый надо вывести
               			if(sendBufferRTOS(thread, (byte*) (Socket[thread].outBuf), strlen(Socket[thread].outBuf)) == 0) journal.jprintf("$Error send buf:  %s\n", (char*) Socket[thread].inBuf);
						break;
					}
					case HTTP_POST_: // предвариательный запрос post
					{
						sendConstRTOS(thread,
								"HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: HEAD, OPTIONS, GET, POST\r\nAccess-Control-Allow-Headers: Overwrite, Content-Type, Cache-Control, Title\r\n\r\n");
						break;
					}
					case UNAUTHORIZED: {
						journal.jprintf("$UNAUTHORIZED\n");
						sendConstRTOS(thread, pageUnauthorized);
						break;
					}
					case BAD_LOGIN_PASS: {
						journal.jprintf("$Wrong login or password\n");
						sendConstRTOS(thread, pageUnauthorized);
						break;
					}

					default:
						journal.jprintf("$Unknow  %s\n", (char*) Socket[thread].inBuf);
						break;
					}

					Socket[thread].inBuf[0] = 0;
					break;   // Подготовить к следующей итерации
				} // end if (client.available())
			} // end while (client.connected())
			taskYIELD();
			Socket[thread].client.stop();   // close the connection
			Socket[thread].sock = -1;
		} // end if (client)
#ifdef FAST_LIB  // Переделка
	}  // for (int sock = 0; sock < W5200_SOCK_SYS; sock++)
#endif
	SemaphoreGive (xWebThreadSemaphore);              // Семафор отдать
}

//  Чтение файла с SD или его генерация
void readFileSD(char *filename, uint8_t thread)
{
	volatile int n, i;
	SdFile webFile;
	char *ch1, *ch2;
	char buf[8];  // для расширения файла хватит 8 байт
	//  journal.jprintf("$Thread: %d socket: %d read file: %s\n",thread,Socket[thread].sock,filename);

	// Реализация функционала подмены для имен файлов по типу: plan[HPscheme].png -> plan2.png
	if((ch1 = strchr(filename, '[')) != NULL) // скобка найдена надо обрабатывать
	{
		if(strstr(filename, "HPscheme") != 0) // найден аргумент (схема ТН) надо подменять на значение HP_SHEME
		{
			if((ch2 = strchr(filename, ']')) != NULL) {
				strncpy(buf, ch2 + 1, sizeof(buf) - 1); // скопировать хвост в промежуточный буфер
				*ch1 = 0x00;  // обрезать строку filename перед [
				_itoa(HP_SCHEME, filename); // добавить номер схемы
				strcat(filename, buf);               // добавить расширение
			} else journal.jprintf("Not found ] in: %s", filename); // нет закрывающейся скобки
		} // if (strstr(filename,"HPscheme")!=0)
		else journal.jprintf("Bad argument in: %s", filename);   // не верный аргумент в скобках
	}

	// В начале обрабатываем генерируемые файлы (для выгрузки из контроллера)
	if(strcmp(filename, "state.txt") == 0) { get_txtState(thread, true); return; }
	if(strcmp(filename, "settings.txt") == 0) {	get_txtSettings(thread); return; }
	if(strcmp(filename, "settings.bin") == 0) {	get_binSettings(thread); return; }
	if(strcmp(filename, "chart.csv") == 0) { get_csvChart(thread); return; }
	if((strcmp(filename, FILE_CHART) == 0) && (!card.exists(FILE_CHART))) { noCsvChart_SD(thread); return; }   // Если файла статистики нет то сгенерить файл с объяснением
	if(strcmp(filename, "journal.txt") == 0) { get_txtJournal(thread); return; }
	if(strcmp(filename, "test.dat") == 0) { get_datTest(thread); return; }
	if(strncmp(filename, "TEST_SD:", 8) == 0) { // Тестирует скорость чтения файла с SD карты
		sendConstRTOS(thread, HEADER_FILE_WEB);
		filename += 8;
		journal.jprintf("SD card test: %s - ", filename);
		SPI_switchSD();
		if(webFile.open(filename, O_READ)) {
			uint32_t startTick = millis();
			uint32_t size = 0;
			for(;;) {
				int n = webFile.read(Socket[thread].outBuf, sizeof(Socket[thread].outBuf));
				if(n < 0) journal.jprintf("Read SD error (%d,%d)!\n", card.cardErrorCode(), card.cardErrorData());
				if(n <= 0) break;
				size += n;
				if(millis() - startTick > (3*W5200_TIME_WAIT/portTICK_PERIOD_MS) - 1000) break; // на секунду меньше, чем блок семафора
				WDT_Restart(WDT);
			}
			startTick = millis() - startTick;
			journal.jprintf("read %d bytes, %d b/sec\n", size, size * 1000 / startTick);
			webFile.close();
			/*/ check write!
			if(!webFile.open(filename, O_RDWR)) journal.jprintf("Error open for writing!\n");
			else {
				n = webFile.write("Test write!");
				journal.jprintf("Wrote %d byte\n", n);
				if(!webFile.sync()) journal.jprintf("Sync failed (%d,%d)\n", card.cardErrorCode(), card.cardErrorData());
				webFile.close();
			}
			//*/
		} else {
			journal.jprintf("not found!\n");
		}
		SPI_switchW5200();
		return;
	}
#ifdef I2C_EEPROM_64KB
//	if (strcmp(filename,"statistic.csv")==0) {get_csvStatistic(thread); return;}
#endif
	if(!HP.get_fSD()) {
		get_indexNoSD(thread);
		return;
	}                  // СД карта не работает - упрощенный интерфейс

	// Чтение с карты  файлов
	SPI_switchSD();
	if(!card.exists(filename))  // проверка на сущестование файла
	{
		SPI_switchW5200();
		sendConstRTOS(thread, HEADER_FILE_NOT_FOUND);
		journal.jprintf((char*) "$WARNING - Can't find %s file!\n", filename);
		return;
	} // файл не найден

	for(i = 0; i < SD_REPEAT; i++)   // Делаем SD_REPEAT попыток открытия файла
	{
		if(!webFile.open(filename, O_READ))    // Карта не читатаеся
		{
			if(i >= SD_REPEAT - 1)                   // Исчерпано число попыток
			{
				SPI_switchW5200();
				sendConstRTOS(thread, HEADER_FILE_NOT_FOUND);
				journal.jprintf("$ERROR - opening %s for read failed!\n", filename);
				HP.message.setMessage(pMESSAGE_SD, (char*) "Ошибка открытия файла с SD карты", 0); // сформировать уведомление об ошибке чтения
				HP.set_fSD(false);                                                      // Отказ карты, работаем без нее
				return;
			}                                                                 //if
		} else break;  // Прочиталось
		_delay(50);
		journal.jprintf("Error opening file %s repeat open . . .\n", filename);

	}  // for

	SPI_switchW5200();         // переключение на сеть

	// Файл открыт читаем данные и кидаем в сеть
#ifdef LOG
	journal.jprintf("$Thread: %d socket: %d read file: %s\n",thread,Socket[thread].sock,filename);
#endif
	//   if (strstr(filename,".css")>0) sendConstRTOS(thread,HEADER_FILE_CSS);
	if(strstr(filename, ".css") != NULL) sendConstRTOS(thread, HEADER_FILE_CSS); // разные заголовки
	else sendConstRTOS(thread, HEADER_FILE_WEB);
	SPI_switchSD();
	while((n = webFile.read(Socket[thread].outBuf, sizeof(Socket[thread].outBuf))) > 0) {
		SPI_switchW5200();
		if(sendBufferRTOS(thread, (byte*) (Socket[thread].outBuf), n) == 0) break;
		SPI_switchSD();
	} // while
	if(n < 0) journal.jprintf("Read SD error (%d,%d)!\n", card.cardErrorCode(), card.cardErrorData());

	SPI_switchSD();
	webFile.close();
	SPI_switchW5200();
}

// ========================== P A R S E R  G E T =================================
// Разбор и обработка строк запросов buf (начало &) входная строка strReturn выходная
// ТОЛЬКО запросы!
// возвращает число обработанных одиночных запросов
#ifdef SENSOR_IP
void parserGET(char *buf, char *strReturn, int8_t sock)
{
#else
void parserGET(char *buf, char *strReturn, int8_t )
{
#endif
  char *str,*x,*y, *z;
  float pm=0;
  int8_t i;
  // переменные для удаленных датчиков
   #ifdef SENSOR_IP                           // Получение данных удаленного датчика
       char *ptr;
       int16_t a,b,c,d;
   #endif
   int32_t e;
  
  ADD_WEBDELIM(strReturn);   // начало запроса
  str = strstr(buf,"&&");
  if(str) str[1] = 0;   // Обрезать строку после комбинации &&
  urldecode(buf, buf, W5200_MAX_LEN);
  while ((str = strtok_r(buf, "&", &buf)) != NULL) // разбор отдельных комманд
  {
     if ((strpbrk(str,"="))==0) // Повторить тело запроса и добавить "=" ДЛЯ НЕ set_ запросов
     {
       strcat(strReturn,str); strcat(strReturn,"=");
     } 
     if (strlen(strReturn)>sizeof(Socket[0].outBuf)-200)  // Контроль длины выходной строки - если слишком длинная обрезаем и выдаем ошибку 200 байт на заголовок
     {  
         strcat(strReturn,"E07"); 
         ADD_WEBDELIM(strReturn) ;
         journal.jprintf("$ERROR - strReturn long, request circumcised . . . \n"); 
         journal.jprintf("%s\n",strReturn);    
          break;   // выход из обработки запроса
     }
    // 1. Функции без параметра
   if (strcmp(str,"TEST")==0)   // Команда TEST
       {
       _itoa(random(-50,50),strReturn);
       ADD_WEBDELIM(strReturn) ;
       continue;
       }

  if (strcmp(str,"TASK_LIST")==0)  // Функция получение списка задач и статистики
       {
         #ifdef STAT_FREE_RTOS   // определена в utility/FreeRTOSConfig.h
         //strcat(strReturn,cStrEnd);
         vTaskList(strReturn+strlen(strReturn));
         #else
         strcat(strReturn,NO_SUPPORT);
       #endif
         ADD_WEBDELIM(strReturn);  continue;
       } 
  if (strcmp(str,"TASK_LIST_RST")==0)  // Функция сброс статистики по задачам
       {
         #ifdef STAT_FREE_RTOS   // определена в utility/FreeRTOSConfig.h
	  	 vTaskResetRunTimeCounters();
         #else
         strcat(strReturn,NO_SUPPORT);
       #endif
         ADD_WEBDELIM(strReturn);  continue;
       }
   if (strcmp(str,"RESET_STAT")==0)   // Команда очистки статистики (в зависимости от типа)
       {
      
       #ifndef I2C_EEPROM_64KB     // Статистика в памяти
           strcat(strReturn,"Статистика не поддерживается в конфигурации . . .");
           journal.jprintf("No support statistics (low I2C) . . .\n");
       #else                      // Статистика в ЕЕПРОМ
           if (HP.get_modWork()==pOFF)
             {
              strcat(strReturn,"Форматирование I2C статистики, ожидайте 10 сек . . .");
              HP.sendCommand(pSFORMAT);        // Послать команду форматирование статитсики
             }
             else strcat(strReturn,"The heat pump must be switched OFF");
       #endif
           ADD_WEBDELIM(strReturn); continue;
       }         
        
     if (strcmp(str,"RESET_JOURNAL")==0)   // Команда очистки журнала (в зависимости от типа)
       {
      
       #ifndef I2C_EEPROM_64KB     // журнал в памяти
           strcat(strReturn,"Сброс системного журнала в RAM . . .");
           journal.Clear();       // Послать команду на очистку журнала в памяти
           journal.jprintf("Reset system RAM journal . . .\n"); 
       #else                      // Журнал в ЕЕПРОМ
            journal.Format(strReturn);
            //HP.sendCommand(pJFORMAT);        // Послать команду форматирование журнала
            strcpy(strReturn, HEADER_ANSWER);   // Начало ответа
            strcat(strReturn,"OK");
       #endif
            ADD_WEBDELIM(strReturn); continue;
       }        
    if (strcmp(str,"RESET")==0)   // Команда сброса контроллера
    {
       strcat(strReturn,"Сброс контроллера, подождите 10 секунд . . .");
       ADD_WEBDELIM(strReturn);
       HP.sendCommand(pRESET);        // Послать команду на сброс
       continue;
    }
    if (strcmp(str,"RESET_COUNT")==0) // Команда RESET_COUNT
       {
       journal.jprintf("$RESET counter moto hour . . .\n"); 
       strcat(strReturn,"Сброс счетчика моточасов за сезон");
       ADD_WEBDELIM(strReturn) ;
       HP.resetCount(false);
       continue;
       }
   if (strcmp(str,"RESET_ALL_COUNT")==0) // Команда RESET_COUNT
       {
       journal.jprintf("$RESET All counter moto hour . . .\n"); 
       strcat(strReturn,"Сброс ВСЕХ счетчика моточасов");
       ADD_WEBDELIM(strReturn) ;
       HP.resetCount(true);  // Полный сброс
       continue;
       }    
   if (strcmp(str,"RESET_SETTINGS")==0) // Команда сброса настроек HP
   {
	   if(HP.get_State() == pOFF_HP) {
	       journal.jprintf("$RESET All HP settings . . .\n");
	       uint16_t d = 0;
	   	   writeEEPROM_I2C(I2C_SETTING_EEPROM, (byte*)&d, sizeof(d));
	       HP.sendCommand(pRESET);        // Послать команду на сброс
	       //HP.resetSettingHP(); // не работает!!
	       strcat(strReturn, "OK");
	       ADD_WEBDELIM(strReturn); continue;
	   }
   }
   if (strcmp(str,"get_status")==0) // Команда get_status Получить состояние ТН - основные параметры ТН
       {
        HP.get_datetime((char*)time_TIME,strReturn);                     strcat(strReturn,"|");
        HP.get_datetime((char*)time_DATE,strReturn);                     strcat(strReturn,"|");
        TimeIntervalToStr(HP.get_uptime(),strReturn);                    strcat(strReturn,"|");
        uint32_t t = HP.get_command_completed();
        if(t) TimeIntervalToStr(rtcSAM3X8.unixtime() - t, strReturn, 1);
        else strcat(strReturn,"-"); 									 strcat(strReturn,"|");
        _itoa(100-HP.CPU_IDLE,strReturn);                                strcat(strReturn,"%|");
        //_itoa(freeRam()+HP.startRAM,strReturn);                          strcat(strReturn,"b|");
        //strcat(strReturn,VERSION);                                       strcat(strReturn,"|");
        #ifdef EEV_DEF
        _ftoa(strReturn,(float)HP.dEEV.get_Overheat()/100,2);strcat(strReturn,"°C|");
        #else
        strcat(strReturn,"-°C|");
        #endif
		#ifdef FC_VACON
        HP.dFC.get_paramFC((char*)fc_FC,strReturn); strcat(strReturn,"|");
		#else
        if (HP.dFC.get_present()) {HP.dFC.get_paramFC((char*)fc_FC,strReturn);strcat(strReturn,"Гц|");} // В зависимости от наличия инвертора
        else                      {strcat(strReturn," - ");strcat(strReturn,"Гц|");}
		#endif
        if (HP.get_errcode() == OK) {
        	if(HP.get_State() == pOFF_HP) {
        		strcat(strReturn, MODE_HP_STR[0]);
        	} else if(HP.get_State() == pWAIT_HP) {
        		strcat(strReturn, "...");
        	} else if(HP.get_State() == pWORK_HP) {
        		strcat(strReturn, MODE_HP_STR[HP.get_modWork()]); strcat(strReturn, " ["); strcat(strReturn, (char *)codeRet[HP.get_ret()]); strcat(strReturn, "]");
        	}
        } else {strcat(strReturn,"Error "); _itoa(HP.get_errcode(),strReturn);} // есть ошибки
        strcat(strReturn,"|" WEBDELIM); continue;
       }
       
   if (strcmp(str,"get_version")==0) // Команда get_version
       {
       strcat(strReturn,VERSION);
       ADD_WEBDELIM(strReturn) ;
       continue;
       }
   if (strcmp(str,"get_uptime")==0) // Команда get_uptime
       {
       TimeIntervalToStr(HP.get_uptime(),strReturn);
       ADD_WEBDELIM(strReturn) ;
       continue;
       }
    if (strcmp(str,"get_startDT")==0) // Команда get_startDT
       {
       DecodeTimeDate(HP.get_startDT(),strReturn);
       ADD_WEBDELIM(strReturn) ;
       continue;
       }
   if (strcmp(str,"get_resetCause")==0) // Команда  get_resetCause
       {
       strcat(strReturn,ResetCause());
       ADD_WEBDELIM(strReturn) ;
       continue;
       }
    if (strcmp(str,"get_config")==0)  // Функция get_config
       {
       strcat(strReturn,CONFIG_NAME);
       ADD_WEBDELIM(strReturn) ;
       continue;
       }
    if (strcmp(str,"get_configNote")==0)  // Функция get_configNote
       {
       strcat(strReturn,CONFIG_NOTE);
       ADD_WEBDELIM(strReturn) ;
       continue;
       }
    if (strcmp(str,"get_freeRam")==0)  // Функция freeRam
       {
       _itoa(freeRam()+HP.startRAM,strReturn);
       strcat(strReturn," b" WEBDELIM) ;
       continue;
       }   
    if (strcmp(str,"get_loadingCPU")==0)  // Функция freeRam
       {
        _itoa(100-HP.CPU_IDLE,strReturn);
        strcat(strReturn,"%" WEBDELIM) ;
       continue;
       }        
     if (strcmp(str,"get_socketInfo")==0)  // Функция  get_socketInfo
       {
       socketInfo(strReturn);    // Информация  о сокетах
       ADD_WEBDELIM(strReturn) ;
       continue;
       }
     if (strcmp(str,"get_socketRes")==0)  // Функция  get_socketRes
       {
       _itoa(HP.socketRes(),strReturn);
       ADD_WEBDELIM(strReturn) ;
       continue;
       }  
     if (strcmp(str,"get_listChart")==0)  // Функция get_listChart - получить список доступных графиков
       {
       HP.get_listChart(strReturn);  // строка добавляется
       ADD_WEBDELIM(strReturn) ;
       continue;
       }
//     if (strcmp(str,"get_listStat")==0)  // Функция get_listChart - получить список доступных статистик
//       {
//       #ifdef I2C_EEPROM_64KB
//       HP.Stat.get_listStat(strReturn, true);  // строка добавляется
//       #else
//       strcat(strReturn,"absent:1;") ;
//       #endif
//       ADD_WEBDELIM(strReturn) ;
//       continue;
//       }
    if (strncmp(str,"get_listProfile", 15)==0)  // Функция get_listProfile - получить список доступных профилей
       {
       HP.Prof.get_list(strReturn /*,HP.Prof.get_idProfile()*/);  // текущий профиль
       ADD_WEBDELIM(strReturn) ;
       continue;
       }
       if (strcmp(str,"update_NTP")==0)  // Функция update_NTP обновление времени по NTP
       {
      // set_time_NTP();                                                 // Обновить время
       HP.timeNTP=0;                                    // Время обновления по NTP в тиках (0-сразу обновляемся)
       strcat(strReturn,"Update time from NTP");
       ADD_WEBDELIM(strReturn);
       continue;
       }
       if(strcmp(str, "get_NTP") == 0)  // тип NTP
       {
			#ifdef HTTP_TIME_REQUEST
    	   	   strcat(strReturn, "TCP" WEBDELIM);
			#else
    	   	   strcat(strReturn, "NTP" WEBDELIM);
			#endif
    	   continue;
       }
       	   if(strcmp(str, "get_NTPr") == 0)  // Запрос
       	   {
			#ifdef HTTP_TIME_REQUEST
       		   strcat(strReturn, (char *)&HTTP_TIME_REQ);
			#endif
       		ADD_WEBDELIM(strReturn);
       	   }
       if ((strcmp(str,"set_updateNet")==0)||(strcmp(str,"RESET_NET")==0))  // Функция Сброс w5200 и применение сетевых настроек, подождите 5 сек . . .
       {
       journal.jprintf("Update network setting . . .\r\n");
       HP.sendCommand(pNETWORK);        // Послать команду применение сетевых настроек
       strcat(strReturn,"Сброс Wiznet w5XXX и применение сетевых настроек, подождите 5 сек . . .");
       ADD_WEBDELIM(strReturn) ;
       continue;
       }         
    if (strcmp(str,"get_WORK")==0)  // Функция get_WORK  ТН включен если он работает или идет его пуск
    {
    	strcat(strReturn, HP.IsWorkingNow() ? "ON" : "OFF"); ADD_WEBDELIM(strReturn); continue;
    }
    if (strcmp(str,"get_MODE")==0)  // Функция get_MODE в каком состояниии находится сейчас насос
       {
       strcat(strReturn,HP.StateToStr());
       ADD_WEBDELIM(strReturn) ;    continue;
       }    
 
    if (strcmp(str,"get_modeHP")==0)           // Функция get_modeHP - получить режим отопления ТН
        {
    		web_fill_tag_select(strReturn, "Выключено:0;Отопление:0;Охлаждение:0;", HP.get_modeHouse() );
    		ADD_WEBDELIM(strReturn) ; continue;
        } // strcmp(str,"get_modeHP")==0)   
    if (strcmp(str,"get_relayOut")==0)  // Функция Строка выходных насосов: RPUMPO = Вкл, RPUMPBH = Бойлер
       {
		#ifdef RPUMPBH
    	if(HP.dRelay[RPUMPBH].get_Relay()) strcat(strReturn, "Бойлер");
    	else
		#endif
    		strcat(strReturn, HP.dRelay[PUMP_OUT].get_Relay() ? "Вкл" : "Выкл");
    	ADD_WEBDELIM(strReturn) ;    continue;
       }
    if (strcmp(str,"get_testMode")==0)  // Функция get_testMode
       {
       for(i=0;i<=HARD_TEST;i++) // Формирование списка
           { strcat(strReturn,noteTestMode[i]); strcat(strReturn,":"); if(i==HP.get_testMode()) strcat(strReturn,cOne); else strcat(strReturn,cZero); strcat(strReturn,";");  }          
       ADD_WEBDELIM(strReturn) ;    continue;
       }  
    if (strcmp(str,"get_remarkTest")==0)  // Функция remarkTest
       {
       switch (HP.get_testMode())
         {
          case NORMAL:    strcat(strReturn,noteRemarkTest[0]);     break; //  Режим работа не тст, все включаем
          case SAFE_TEST: strcat(strReturn,noteRemarkTest[1]);     break; //  Ничего не включаем
          case TEST:      strcat(strReturn,noteRemarkTest[2]);     break; //  Включаем все кроме компрессора
          case HARD_TEST: strcat(strReturn,noteRemarkTest[3]);     break; //  Все включаем и компрессор тоже
         }     
       ADD_WEBDELIM(strReturn) ;    continue;
       }  
     
        
    if (strncmp(str, "set_SAVE", 8) == 0)  // Функция set_SAVE -
		{
			if(strncmp(str+8, "_SCHDLR", 7) == 0) {
				_itoa(HP.Schdlr.save(),strReturn); // сохранение расписаний
			} else {
				uint16_t len = HP.save();   // записать настройки в еепром, а потом будем их писать и получить размер записываемых данных
				if(len > 0) {
					uint16_t len2 = HP.Prof.save(HP.Prof.get_idProfile());
					if(len2 > 0) len += len2; else len = len2;
				}
				_itoa(len, strReturn); // сохранение настроек ВСЕХ!
				HP.save_motoHour();
			}
			ADD_WEBDELIM(strReturn);
			continue;
		}
    if (strncmp(str, "set_LOAD", 8) == 0)  // Функция set_LOAD -
		{
			if(strncmp(str+8, "_SCHDLR", 7) == 0) {
				_itoa(HP.Schdlr.load(),strReturn); // сохранение расписаний
			} else {
			}
			ADD_WEBDELIM(strReturn);
			continue;
		}
    if (strcmp(str,"set_ON")==0)  // Функция set_ON
       {
       HP.sendCommand(pSTART);        // Послать команду на пуск ТН
       if (HP.get_State()==pWORK_HP)  strcat(strReturn,cOne); else  strcat(strReturn,cZero); ADD_WEBDELIM(strReturn) ;    continue;
       }   
    if (strcmp(str,"set_OFF")==0)  // Функция set_OFF
       {
       HP.sendCommand(pSTOP);        // Послать команду на останов ТН
       if (HP.get_State()==pWORK_HP)  strcat(strReturn,cOne); else  strcat(strReturn,cZero); ADD_WEBDELIM(strReturn) ;    continue;
       }   
    if (strcmp(str,"get_errcode")==0)  // Функция get_errcode
       {
       _itoa(HP.get_errcode(),strReturn); ADD_WEBDELIM(strReturn) ;    continue;
       }   
    if (strcmp(str,"get_error")==0)  // Функция get_error
       {
       strcat(strReturn,HP.get_lastErr()); ADD_WEBDELIM(strReturn) ;    continue;
       } 
    if (strcmp(str,"get_tempSAM3x")==0)  // Функция get_tempSAM3x  - получение температуры чипа sam3x
       {
       _ftoa(strReturn,temp_DUE(),2); ADD_WEBDELIM(strReturn); continue;
       }   
    if (strcmp(str,"get_tempDS3231")==0)  // Функция get_tempDS3231  - получение температуры DS3231
       {
       _ftoa(strReturn,getTemp_RtcI2C(),2); ADD_WEBDELIM(strReturn); continue;
       }  
    if (strcmp(str,"get_fullCOP")==0)  //  получение полного COP
       {
       if (HP.fullCOP!=-1000) _ftoa(strReturn,(float)HP.fullCOP/100.0,2); else strcat(strReturn,"-"); ADD_WEBDELIM(strReturn); continue;
       }  
    if (strcmp(str,"get_PowerCO") == 0)
       {
   		_ftoa(strReturn, HP.powerCO/1000.0,3); ADD_WEBDELIM(strReturn); continue;
       }
    if (strcmp(str,"get_PowerGEO") == 0)
       {
   		_ftoa(strReturn, HP.powerGEO/1000.0,3); ADD_WEBDELIM(strReturn); continue;
       }
    if (strcmp(str,"get_VCC")==0)  // Функция get_VCC  - получение напряжение питания контроллера
       {
       #ifdef VCC_CONTROL  // если разрешено чтение напряжение питания
           _ftoa(strReturn,(float)HP.AdcVcc/K_VCC_POWER,2); 
        #else 
           strcat(strReturn,NO_SUPPORT); 
        #endif
       ADD_WEBDELIM(strReturn); continue;
       }       
    if (strcmp(str,"get_OneWirePin")==0)  // Функция get_OneWirePin
       {
       #ifdef ONEWIRE_DS2482  
        strcat(strReturn, "I2C, DS2482(1");
		#ifdef ONEWIRE_DS2482_SECOND
        strcat(strReturn, ",2");
		#endif
		#ifdef ONEWIRE_DS2482_THIRD
        strcat(strReturn, ",3");
		#endif
		#ifdef ONEWIRE_DS2482_FOURTH
        strcat(strReturn, ",4");
		#endif
        strcat(strReturn, ")" WEBDELIM);
       #else
        strcat(strReturn,"D"); _itoa((int)(PIN_ONE_WIRE_BUS),strReturn); ADD_WEBDELIM(strReturn);
       #endif
        continue;
       }       
    if (strcmp(str,"scan_OneWire")==0)  // Функция scan_OneWire  - сканирование датчикиков
       {
    	HP.scan_OneWire(strReturn); ADD_WEBDELIM(strReturn); continue;
       }
     if (strstr(str,"get_numberIP"))  // Удаленные датчики - получить число датчиков
       { 
        #ifdef SENSOR_IP                           
         _itoa(IPNUMBER,strReturn);ADD_WEBDELIM(strReturn); continue;
        #else
         strcat(strReturn,"0" WEBDELIM);continue;
        #endif 
       }   
//      if (strcmp(str,"set_testStat")==0)  // сгенерить тестовые данные статистики ОЧИСТКА СТАРЫХ ДАННЫХ!!!!!
//       {
//       #ifdef I2C_EEPROM_64KB  // рабоатет на выключенном ТН
//       if (HP.get_modWork()==pOFF)
//       {
//         HP.Stat.generate_TestData(STAT_POINT); // Сгенерировать статистику STAT_POINT точек только тестирование
//         strcat(strReturn,"Generation of test data - OK" WEBDELIM);
//       }
//       else strcat(strReturn,"The heat pump must be switched OFF" WEBDELIM);
//       #else
//       strcat(strReturn,NO_STAT);
//       #endif
//       continue;
//       }
//     if (strcmp(str,"get_infoStat")==0)  // Получить информацию о статистике
//       {
//       #ifdef I2C_EEPROM_64KB
//       HP.Stat.get_Info(strReturn,true);
//       #else
//       strcat(strReturn,NO_STAT) ;
//       #endif
//       ADD_WEBDELIM(strReturn) ;
//       continue;
//       }

    if (strstr(str,"get_infoESP"))  // Удаленные датчики - запрос состояния контрола
       { 
       // TIN, TOUT, TBOILER, ВЕРСИЯ, ПАМЯТЬ, ЗАГРУЗКА, АПТАЙМ, ПЕРЕГРЕВ, ОБОРОТЫ, СОСТОЯНИЕ.
        _ftoa(strReturn,(float)HP.sTemp[TIN].get_Temp()/100.0,1);     strcat(strReturn,";");
        _ftoa(strReturn,(float)HP.sTemp[TOUT].get_Temp()/100.0,1);    strcat(strReturn,";");
        _ftoa(strReturn,(float)HP.sTemp[TBOILER].get_Temp()/100.0,1); strcat(strReturn,";");
        strcat(strReturn,VERSION);                                    strcat(strReturn,";");        
        _itoa(freeRam()+HP.startRAM,strReturn);                       strcat(strReturn,";");
        _itoa(100-HP.CPU_IDLE,strReturn);                             strcat(strReturn,";");
        TimeIntervalToStr(HP.get_uptime(),strReturn);                 strcat(strReturn,";");
        #ifdef EEV_DEF 
        _ftoa(strReturn,(float)HP.dEEV.get_Overheat()/100,2);         strcat(strReturn,";");
        #else
        strcat(strReturn,"-;");
        #endif
        if (HP.dFC.get_present()) {HP.dFC.get_paramFC((char*)fc_FC,strReturn);    strcat(strReturn,";");} // В зависимости от наличия инвертора
        else                      {strcat(strReturn," - ");                       strcat(strReturn,";");}
      //  strcat(strReturn,HP.StateToStrEN());                                      strcat(strReturn,";");
        if (HP.get_errcode()==OK)  strcat(strReturn,HP.StateToStrEN());                   // Ошибок нет
        else {strcat(strReturn,"Error "); _itoa(HP.get_errcode(),strReturn);} // есть ошибки
        strcat(strReturn,";");   ADD_WEBDELIM(strReturn) ;    continue;
       }   
     if(strncmp(str, "hide_", 5) == 0) { // Удаление элементов внутри tag name="hide_*"
    	str += 5;
    	if(strcmp(str, "fcanalog") == 0) {
			#ifdef FC_ANALOG_CONTROL
    			strcat(strReturn,"0");
			#else
    			strcat(strReturn,"1");
			#endif
    	} else if(strcmp(str, "rpumpfl") == 0) {
			#ifdef RPUMPFL
    			strcat(strReturn,"0");
			#else
    			strcat(strReturn,"1");
			#endif
    	} else if(strcmp(str, "tro_ei") == 0) { // hide: TRTOOUT, TEVAIN
			#ifdef TRTOUT
    			strcat(strReturn,"0");
			#else
    			strcat(strReturn,"1");
			#endif
    	} else if(strcmp(str, "sun") == 0) { // hide: SUN
			#ifdef USE_SUN_COLLECTOR
    			strcat(strReturn,"0");
			#else
    			strcat(strReturn,"1");
			#endif
    	}
    	ADD_WEBDELIM(strReturn); continue;
     }
 
    if (strcmp(str,"CONST")==0)   // Команда CONST  Информация очень большая по этому разбито на 2 запроса CONST CONST1
        {
       strcat(strReturn,"VERSION|Версия прошивки|");strcat(strReturn,VERSION);strcat(strReturn,";");
       strcat(strReturn,"__DATE__ __TIME__|Дата и время сборки прошивки|");strcat(strReturn,__DATE__);strcat(strReturn," ");strcat(strReturn,__TIME__) ;strcat(strReturn,";");
       strcat(strReturn,"VER_SAVE|Версия формата данных, при чтении I2C памяти, если версии не совпадают, отказ от чтения|");_itoa(VER_SAVE,strReturn);strcat(strReturn,";");
       strcat(strReturn,"CONFIG_NAME|Имя конфигурации|");strcat(strReturn,CONFIG_NAME);strcat(strReturn,";");
       strcat(strReturn,"CONFIG_NOTE|");strcat(strReturn,CONFIG_NOTE);strcat(strReturn,"|-");strcat(strReturn,";");  
       strcat(strReturn,"UART_SPEED|Скорость отладочного порта (бод)|");_itoa(UART_SPEED,strReturn);strcat(strReturn,";");
       strcat(strReturn,"DEMO|Режим демонстрации (эмуляция датчиков)|");
           #ifdef DEMO
             strcat(strReturn,"ON;");
           #else
             strcat(strReturn,"OFF;");
           #endif 
        strcat(strReturn,"DEBUG|Вывод в порт отладочных сообщений|");
           #ifdef DEBUG
             strcat(strReturn,"ON;");
           #else
             strcat(strReturn,"OFF;");
           #endif 
        strcat(strReturn,"STAT_FREE_RTOS|Накопление статистики Free RTOS (отладка)|");
           #ifdef STAT_FREE_RTOS
             strcat(strReturn,"ON;");
           #else
             strcat(strReturn,"OFF;");
           #endif 
        strcat(strReturn,"LOG|Вывод в порт лога web сервера|");
           #ifdef LOG
             strcat(strReturn,"ON;");
           #else
             strcat(strReturn,"OFF;");
           #endif 
        #ifdef I2C_EEPROM_64KB   
//       strcat(strReturn,"STAT_POINT|Максимальное число дней накопления статистики|");_itoa(STAT_POINT,strReturn);strcat(strReturn,";");
       #endif
       strcat(strReturn,"CHART_POINT|Максимальное число точек графиков|");_itoa(CHART_POINT,strReturn);strcat(strReturn,";");
       strcat(strReturn,"I2C_EEPROM_64KB|Место хранения системного журнала|");
            #ifdef I2C_EEPROM_64KB
             strcat(strReturn,"I2C flash memory;");
            #else
             strcat(strReturn,"RAM memory;");
            #endif 
        strcat(strReturn,"JOURNAL_LEN|Размер кольцевого буфера системного журнала (байт)|");_itoa(JOURNAL_LEN,strReturn);strcat(strReturn,";");
                   
       // Карта
       m_snprintf(strReturn + m_strlen(strReturn), 128, "SD_FAT_VERSION|Версия библиотеки SdFat|%s;", SD_FAT_VERSION);
       strcat(strReturn,"USE_SD_CRC|Использовать проверку CRC|");_itoa(USE_SD_CRC,strReturn);strcat(strReturn,";");
       strcat(strReturn,"SD_REPEAT|Число попыток чтения карты и открытия файлов, при неудаче переход на работу без карты|");_itoa(SD_REPEAT,strReturn);strcat(strReturn,";");
       //strcat(strReturn,"SD_SPI_SPEED|Частота SPI SD карты, пересчитывается через делитель базовой частоты CPU 84 МГц (МГц)|");_itoa(84/SD_SPI_SPEED,strReturn);strcat(strReturn,";");

       // W5200
       strcat(strReturn,"W5200_THREARD|Число потоков для сетевого чипа (web сервера) "); strcat(strReturn,nameWiznet);strcat(strReturn,"|");_itoa(W5200_THREARD,strReturn);strcat(strReturn,";");
       strcat(strReturn,"W5200_TIME_WAIT|Время ожидания захвата мютекса, для управления потоками (мсек)|");_itoa( W5200_TIME_WAIT,strReturn);strcat(strReturn,";");
       strcat(strReturn,"W5200_STACK_SIZE|Размер стека для одного потока чипа "); strcat(strReturn,nameWiznet);strcat(strReturn," (х4 байта)|");_itoa(W5200_STACK_SIZE,strReturn);strcat(strReturn,";");
       strcat(strReturn,"W5200_NUM_PING|Число попыток пинга до определения потери связи |");_itoa(W5200_NUM_PING,strReturn);strcat(strReturn,";");
       strcat(strReturn,"W5200_MAX_LEN|Размер аппаратного буфера  сетевого чипа "); strcat(strReturn,nameWiznet);strcat(strReturn," (байт)|");_itoa(W5200_MAX_LEN,strReturn);strcat(strReturn,";");
       strcat(strReturn,"W5200_SPI_SPEED|Частота SPI чипа "); strcat(strReturn,nameWiznet);strcat(strReturn,", пересчитывается через делитель базовой частоты CPU 84 МГц (МГц)|");_itoa(84/W5200_SPI_SPEED,strReturn);strcat(strReturn,";");
       strcat(strReturn,"INDEX_FILE|Файл загружаемый по умолчанию|");strcat(strReturn,INDEX_FILE);strcat(strReturn,";");
       // Частотник
       if (HP.dFC.get_present())
       {
       strcat(strReturn,"DEVICEFC|Поддержка инвертора для компрессора|");strcat(strReturn,HP.dFC.get_name());strcat(strReturn,";");
       strcat(strReturn,"FC_MODBUS_ADR|Адрес инвертора на шине Modbus RTU|");strcat(strReturn,byteToHex(FC_MODBUS_ADR));strcat(strReturn,";");
       strcat(strReturn,"MODBUS_PORT_NUM|Используемый порт для обмена по Modbus RTU|Serial");
              if(&MODBUS_PORT_NUM==&Serial1) strcat(strReturn,cOne);
         else if(&MODBUS_PORT_NUM==&Serial2) strcat(strReturn,"2");
         else if(&MODBUS_PORT_NUM==&Serial3) strcat(strReturn,"3");   
         else strcat(strReturn,"??");
       strcat(strReturn,";");  
       strcat(strReturn,"MODBUS_PORT_SPEED|Скорость обмена (бод)|");_itoa(MODBUS_PORT_SPEED,strReturn);strcat(strReturn,";");
       strcat(strReturn,"MODBUS_PORT_CONFIG|Конфигурация порта|");strcat(strReturn,"SERIAL_8N1");strcat(strReturn,";");
       strcat(strReturn,"MODBUS_TIME_WAIT|Максимальное время ожидания освобождения порта (мсек)|");_itoa(MODBUS_TIME_WAIT,strReturn);strcat(strReturn,";");

       }
       else strcat(strReturn,"DEVICEFC|Поддержка инвертора для компрессора|Нет;");
      // NEXTION
       strcat(strReturn,"NEXTION|Использование дисплея Nextion 4.3|");
           #ifdef NEXTION
             strcat(strReturn,"ON;");
           #else
             strcat(strReturn,"OFF;");
           #endif 
       strcat(strReturn,"NEXTION_DEBUG|Вывод в порт отладочных сообщений от дисплея|");
           #ifdef NEXTION_DEBUG
             strcat(strReturn,"ON;");
           #else
             strcat(strReturn,"OFF;");
           #endif 
       strcat(strReturn,"NEXTION_PORT|Порт куда присоединен дисплей Nextion|Serial");
              if(&NEXTION_PORT==&Serial1) strcat(strReturn,cOne);
         else if(&NEXTION_PORT==&Serial2) strcat(strReturn,"2");
         else if(&NEXTION_PORT==&Serial3) strcat(strReturn,"3");   
         else strcat(strReturn,"??");
        strcat(strReturn,";"); 
       strcat(strReturn,"NEXTION_UPDATE|Время обновления информации на дисплее Nextion (мсек)|");_itoa(NEXTION_UPDATE,strReturn);strcat(strReturn,";");
       strcat(strReturn,"NEXTION_READ|Время опроса дисплея Nextion (мсек)|");_itoa(NEXTION_READ,strReturn);strcat(strReturn,";");
       strcat(strReturn,"TIME_ZONE|Часовой пояс|");_itoa(TIME_ZONE,strReturn);strcat(strReturn,";");
       // Free RTOS
       strcat(strReturn,"FREE_RTOS_ARM_VERSION|Версия библиотеки Free RTOS due|");_itoa(FREE_RTOS_ARM_VERSION,strReturn);strcat(strReturn,";");
       strcat(strReturn,"configCPU_CLOCK_HZ|Частота CPU (мГц)|");_itoa(configCPU_CLOCK_HZ/1000000,strReturn);strcat(strReturn,";");
       strcat(strReturn,"configTICK_RATE_HZ|Квант времени системы Free RTOS (мкс)|");_itoa(configTICK_RATE_HZ,strReturn);strcat(strReturn,";");
       strcat(strReturn,"WDT_TIME|Период Watchdog таймера, 0 - запрет таймера (сек)|");_itoa(WDT_TIME,strReturn);strcat(strReturn,";");
    
       // Удаленные датчики
       strcat(strReturn,"SENSOR_IP|Использование удаленных датчиков|");
       #ifdef SENSOR_IP 
        strcat(strReturn,"ON;");
        strcat(strReturn,"IPNUMBER|Максимальное число удаленных датчиков, нумерация начинается с 1|");_itoa(IPNUMBER,strReturn);strcat(strReturn,";");
        strcat(strReturn,"UPDATE_IP|Максимальное время между посылками данных с удаленного датчика (сек)|");_itoa(UPDATE_IP,strReturn);strcat(strReturn,";");
       #else
        strcat(strReturn,"OFF;");
       #endif 
       
        strcat(strReturn,"K_VCC_POWER|Коэффициент пересчета для канала контроля напряжения питания (отсчеты/В)|");
        #ifdef VCC_CONTROL  // если разрешено чтение напряжение питания
          _ftoa(strReturn,(float)K_VCC_POWER,2);strcat(strReturn,";");
        #else 
           strcat(strReturn,NO_SUPPORT); strcat(strReturn,";");
        #endif
        
       ADD_WEBDELIM(strReturn);  continue;
       } // end CONST
       
    if (strcmp(str,"CONST1")==0)   // Команда CONST1 Информация очень большая по этому разбито на 2 запроса CONST CONST1
       {
       // i2c
       strcat(strReturn,"I2C_SPEED|Частота работы шины I2C (кГц)|"); _itoa(I2C_SPEED/1000,strReturn); strcat(strReturn,";");
       strcat(strReturn,"I2C_COUNT_EEPROM|Адрес внутри чипа I2C с которого пишется счетчики ТН|"); strcat(strReturn,uint16ToHex(I2C_COUNT_EEPROM)); strcat(strReturn,";");
       strcat(strReturn,"I2C_SETTING_EEPROM|Адрес внутри чипа I2C с которого пишутся настройки ТН|"); strcat(strReturn,uint16ToHex(I2C_SETTING_EEPROM)); strcat(strReturn,";");
       strcat(strReturn,"I2C_PROFILE_EEPROM|Адрес внутри чипа I2C с которого пишется профили ТН|"); strcat(strReturn,uint16ToHex(I2C_PROFILE_EEPROM)); strcat(strReturn,";");
       strcat(strReturn,"TIME_READ_SENSOR|Период опроса датчиков + DELAY_DS1820 (мсек)|");_itoa(TIME_READ_SENSOR+cDELAY_DS1820,strReturn);strcat(strReturn,";");
       strcat(strReturn,"TIME_CONTROL|Период управления тепловым насосом (мсек)|");_itoa(TIME_CONTROL,strReturn);strcat(strReturn,";");
       strcat(strReturn,"TIME_EEV|Период управления ЭРВ (мсек)|");_itoa(TIME_EEV,strReturn);strcat(strReturn,";");
       strcat(strReturn,"TIME_WEB_SERVER|Период опроса web сервера (мсек)"); strcat(strReturn,nameWiznet);strcat(strReturn," (мсек)|");_itoa(TIME_WEB_SERVER,strReturn);strcat(strReturn,";");
       strcat(strReturn,"TIME_COMMAND|Период разбора команд управления ТН (мсек)|");_itoa(TIME_COMMAND,strReturn);strcat(strReturn,";");
       strcat(strReturn,"TIME_I2C_UPDATE |Период синхронизации внутренних часов с I2C часами (сек)|");_itoa(TIME_I2C_UPDATE,strReturn);strcat(strReturn,";");
       // Датчики
       strcat(strReturn,"P_NUMSAMLES|Число значений для усреднения показаний давления|");_itoa(P_NUMSAMLES,strReturn);strcat(strReturn,";");
       strcat(strReturn,"PRESS_FREQ|Частота опроса датчика давления (Гц)|");_itoa(PRESS_FREQ,strReturn);strcat(strReturn,";");
       strcat(strReturn,"FILTER_SIZE|Длина фильтра датчика давления (отсчеты)|");_itoa(FILTER_SIZE,strReturn);strcat(strReturn,";");
       strcat(strReturn,"T_NUMSAMLES|Число значений для усреднения показаний температуры|");_itoa(T_NUMSAMLES,strReturn);strcat(strReturn,";");
       strcat(strReturn,"GAP_TEMP_VAL|Допустимая разница показаний между двумя считываниями (°C)|");_ftoa(strReturn,(float)GAP_TEMP_VAL/100.0,2);strcat(strReturn,";");
       strcat(strReturn,"MAX_TEMP_ERR|Максимальная систематическая ошибка датчика температуры (°C)|");_ftoa(strReturn,(float)MAX_TEMP_ERR/100.0,2);strcat(strReturn,";");
       // SALLMONELA
       strcat(strReturn,"SALLMONELA_DAY|День недели когда проводится обеззараживание ГВС (1-понедельник)|");_itoa(SALLMONELA_DAY,strReturn);strcat(strReturn,";");
       strcat(strReturn,"SALLMONELA_HOUR|Час когда начинаятся обеззарживание ГВС|");_itoa(SALLMONELA_HOUR,strReturn);strcat(strReturn,";");
       strcat(strReturn,"SALLMONELA_TEMP|Целевая температура обеззараживания ГВС (°C)|");_ftoa(strReturn,(float)SALLMONELA_TEMP/100.0,2);strcat(strReturn,";");
       // ЭРВ
       #ifdef EEV_DEF
       strcat(strReturn,"EEV_STEPS|Максимальное число шагов ЭРВ|");_itoa(EEV_STEPS,strReturn);strcat(strReturn,";");
       strcat(strReturn,"EEV_QUEUE|Длина очереди команд шагового двигателя ЭРВ|");_itoa(EEV_QUEUE,strReturn);strcat(strReturn,";");
       strcat(strReturn,"EEV_INVERT|Инвертирование направления движения ЭРВ (по выходам)|");
           #ifdef EEV_INVERT
             strcat(strReturn,"ON;");
           #else
             strcat(strReturn,"OFF;");
           #endif 
       strcat(strReturn,"EEV_PHASE|Управляющая последовательность ЭРВ (фазы движения)|");
           #if EEV_PHASE==PHASE_8s
             strcat(strReturn,"8 фаз, полушаг;");
           #elif EEV_PHASE==PHASE_8
             strcat(strReturn,"8 фаз, шаг;");
           #elif EEV_PHASE==PHASE_4
             strcat(strReturn,"4 фазы, шаг;");
           #else
             strcat(strReturn,"Ошибочная;");
           #endif
        #endif   // EEV
       #ifdef MQTT
//       strcat(strReturn,"MQTT_REPEAT|Число попыток соединениея с MQTT сервером за одну итерацию|");strcat(strReturn,int2str(MQTT_REPEAT));strcat(strReturn,";");
       strcat(strReturn,"MQTT_NUM_ERR_OFF|Число ошибок отправки подряд при котором отключается сервис MQTT (флаг сбрасывается)|");_itoa(MQTT_NUM_ERR_OFF,strReturn);strcat(strReturn,";");
       
       #endif 
       ADD_WEBDELIM(strReturn) ;
       continue;
       } // end CONST1
       
      if (strcmp(str,"get_sysInfo")==0)  // Функция вывода системной информации для разработчика
       {
        strcat(strReturn,"Входное напряжение питания контроллера (В): |");
		#ifdef VCC_CONTROL  // если разрешено чтение напряжение питания
		  _ftoa(strReturn,(float)HP.AdcVcc/K_VCC_POWER,2);strcat(strReturn,";");
		#else
		   m_snprintf(strReturn+m_strlen(strReturn), 256, "если ниже %.1fV - сброс;", (float)((SUPC->SUPC_SMMR & SUPC_SMMR_SMTH_Msk) >> SUPC_SMMR_SMTH_Pos) / 10 + 1.9);
		#endif
        m_snprintf(strReturn+m_strlen(strReturn),256, "Режим safeNetwork (%sадрес:%d.%d.%d.%d шлюз:%d.%d.%d.%d, не спрашиваеть пароль)|%s;", defaultDHCP ?"DHCP, ":"",defaultIP[0],defaultIP[1],defaultIP[2],defaultIP[3],defaultGateway[0],defaultGateway[1],defaultGateway[2],defaultGateway[3],HP.safeNetwork ?cYes:cNo);
        strcat(strReturn,"Уникальный ID чипа SAM3X8E|");getIDchip(strReturn);strcat(strReturn,";");
        strcat(strReturn,"Значение регистра VERSIONR сетевого чипа WizNet (51-w5100, 3-w5200, 4-w5500)|");_itoa(W5200VERSIONR(),strReturn);strcat(strReturn,";");
      
        strcat(strReturn,"Контроль за работой драйвера ЭРВ |");
        #ifdef DRV_EEV_L9333          // Контроль за работой драйвера ЭРВ
           if (digitalReadDirect(PIN_STEP_DIAG))  strcat(strReturn,"Error L9333;"); else strcat(strReturn,"Normal;");
        #else
          strcat(strReturn,NO_SUPPORT); strcat(strReturn,";");
        #endif
         strcat(strReturn,"Состояние FreeRTOS task+err_code [1:configASSERT fails, 2:malloc fails, 3:stack overflow, 4:hard fault, 5:bus fault, 6:usage fault, 7:crash data]|");
         strcat(strReturn,uint16ToHex(lastErrorFreeRtosCode));strcat(strReturn,";");
          
           strcat(strReturn,"Регистры контроллера питания (SUPC) SAM3X8E [SUPC_SMMR SUPC_MR SUPC_SR]|");  // Регистры состояния контроллера питания
           strcat(strReturn,uint32ToHex(SUPC->SUPC_SMMR));strcat(strReturn," ");
           strcat(strReturn,uint32ToHex(SUPC->SUPC_MR));strcat(strReturn," ");
     //      strcat(strReturn,uint32ToHex(SUPC->SUPC_SR));strcat(strReturn,"/");
      //     strcat(strReturn,uint32ToHex(startSupcStatusReg));strcat(strReturn,";");
           startSupcStatusReg |= SUPC->SUPC_SR;                                  // Копим изменения
           strcat(strReturn,uint32ToHex(startSupcStatusReg));
           if ((startSupcStatusReg|SUPC_SR_SMS)==SUPC_SR_SMS_PRESENT)  strcat(strReturn," bad VDDIN!");
           strcat(strReturn,";");
         /*      
        for (uint8_t i=0;i<ANUMBER;i++)    // по всем датчикам
            {
                if (!HP.sADC[i].get_present()) continue;    // датчик отсутсвует в конфигурации пропускаем
                strcat(strReturn,"Счетчик числа ошибок чтения аналогового датчика AD");
                strcat(strReturn,int2str(HP.sADC[i].get_pinA()));
                strcat(strReturn,"|");
                strcat(strReturn,int2str(HP.sADC[i].get_errADC()));strcat(strReturn,";");
            }
         */
        strcat(strReturn,"Счетчик текущего числа повторных попыток пуска ТН|");
            if(HP.get_State()==pWORK_HP) { _itoa(HP.num_repeat,strReturn);strcat(strReturn,";");} else strcat(strReturn,"0;");
        strcat(strReturn,"Счетчик \"Потеря связи с "); strcat(strReturn,nameWiznet);strcat(strReturn,"\", повторная инициализация  <sup>1</sup>|");_itoa(HP.num_resW5200,strReturn);strcat(strReturn,";");
        strcat(strReturn,"Счетчик числа сбросов мютекса захвата шины SPI|");_itoa(HP.num_resMutexSPI,strReturn);strcat(strReturn,";");
        strcat(strReturn,"Счетчик числа сбросов мютекса захвата шины I2C|");_itoa(HP.num_resMutexI2C,strReturn);strcat(strReturn,";");
        #ifdef MQTT
        strcat(strReturn,"Счетчик числа повторных соединений MQTT клиента|");_itoa(HP.num_resMQTT,strReturn);strcat(strReturn,";");
        #endif
        strcat(strReturn,"Счетчик потерянных пакетов ping|");_itoa(HP.num_resPing,strReturn);strcat(strReturn,";");
        #ifdef USE_ELECTROMETER_SDM  
        strcat(strReturn,"Счетчик числа ошибок чтения счетчика SDM120 (RS485)|");_itoa(HP.dSDM.get_numErr(),strReturn);strcat(strReturn,";");
        #endif
        if (HP.dFC.get_present()) strcat(strReturn,"Счетчик числа ошибок чтения частотного преобразователя (RS485)|");_itoa(HP.dFC.get_numErr(),strReturn);strcat(strReturn,";");
       
        strcat(strReturn,"Счетчик числа ошибок чтения датчиков температуры (ds18b20)|");_itoa(HP.get_errorReadDS18B20(),strReturn);strcat(strReturn,";");

        strcat(strReturn,"Время последнего включения ТН|");DecodeTimeDate(HP.get_startTime(),strReturn);strcat(strReturn,";");
        strcat(strReturn,"Время завершения последней команды ТН|");DecodeTimeDate(HP.get_command_completed(),strReturn);strcat(strReturn,";");
        strcat(strReturn,"Время старта компрессора|");DecodeTimeDate(HP.get_startCompressor(),strReturn);strcat(strReturn,";");
        strcat(strReturn,"Время останова компрессора|");DecodeTimeDate(HP.get_stopCompressor(),strReturn);strcat(strReturn,";");
        strcat(strReturn,"Время сохранения текущих настроек ТН|");DecodeTimeDate(HP.get_saveTime(),strReturn);strcat(strReturn,";");
        
        // Вывод строки статуса
        strcat(strReturn,"Строка статуса ТН| modWork:");_itoa((int)HP.get_modWork(),strReturn);strcat(strReturn,"[");strcat(strReturn,codeRet[HP.get_ret()]);strcat(strReturn,"]");
        if(!(HP.dFC.get_present())) { strcat(strReturn," RCOMP:");if (HP.dRelay[RCOMP].get_Relay()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero); }
        #ifdef RPUMPI
        strcat(strReturn," RPUMPI:");                                         if (HP.dRelay[RPUMPI].get_Relay()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero); 
        #endif
        strcat(strReturn," RPUMPO:");                                         if (HP.dRelay[RPUMPO].get_Relay()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
        #ifdef  RTRV
        if (HP.dRelay[RTRV].get_present())    { strcat(strReturn," RTRV:");   if (HP.dRelay[RTRV].get_Relay()==true)    strcat(strReturn,cOne); else  strcat(strReturn,cZero);}
        #endif
        #ifdef R3WAY
        if (HP.dRelay[R3WAY].get_present())   { strcat(strReturn," R3WAY:");  if (HP.dRelay[R3WAY].get_Relay()==true)   strcat(strReturn,cOne); else  strcat(strReturn,cZero);}
        #endif
        #ifdef RBOILER
        if (HP.dRelay[RBOILER].get_present()) { strcat(strReturn," RBOILER:");if (HP.dRelay[RBOILER].get_Relay()==true) strcat(strReturn,cOne); else  strcat(strReturn,cZero);}
        #endif
        #ifdef RHEAT
        if (HP.dRelay[RHEAT].get_present())   { strcat(strReturn," RHEAT:");  if (HP.dRelay[RHEAT].get_Relay()==true)   strcat(strReturn,cOne); else  strcat(strReturn,cZero);}
        #endif
        #ifdef REVI
        if (HP.dRelay[REVI].get_present()) { strcat(strReturn," REVI:");      if (HP.dRelay[REVI].get_Relay()==true)    strcat(strReturn,cOne); else  strcat(strReturn,cZero);}
        #endif
        if(HP.dFC.get_present())  {strcat(strReturn," freqFC:"); _ftoa(strReturn,(float)HP.dFC.get_freqFC()/100.0,2); }
        if(HP.dFC.get_present())  {strcat(strReturn," Power:"); _ftoa(strReturn,(float)HP.dFC.get_power()/1000.0,3);  }
        strcat(strReturn,";");  
   
           
        strcat(strReturn,"Время сброса счетчиков с момента запуска ТН|");DecodeTimeDate(HP.get_motoHourD1(),strReturn);strcat(strReturn,";");
        strcat(strReturn,"Часы работы ТН с момента запуска (час)|");_ftoa(strReturn,(float)HP.get_motoHourH1()/60.0,1);strcat(strReturn,";");
        strcat(strReturn,"Часы работы компрессора ТН с момента запуска (час)|");_ftoa(strReturn,(float)HP.get_motoHourC1()/60.0,1);strcat(strReturn,";");
        #ifdef USE_ELECTROMETER_SDM  
          strcat(strReturn,"Потребленная энергия ТН с момента запуска (кВт*ч)|");_ftoa(strReturn, HP.dSDM.get_Energy()-HP.get_motoHourE1(),2);strcat(strReturn,";");
        #endif
        
        #ifdef  FLOWCON 
        if(HP.sTemp[TCONING].get_present() & HP.sTemp[TCONOUTG].get_present()) { strcat(strReturn,"Выработанная энергия ТН с момента запуска (кВт*ч)|");_ftoa(strReturn, HP.get_motoHourP1()/1000.0,2);strcat(strReturn,";");} // Если есть оборудование
        #endif
        
        strcat(strReturn,"Время сброса сезонных счетчиков ТН|");DecodeTimeDate(HP.get_motoHourD2(),strReturn);strcat(strReturn,";");
        strcat(strReturn,"Часы работы ТН за сезон (час)|");_ftoa(strReturn,(float)HP.get_motoHourH2()/60.0,1);strcat(strReturn,";");
        strcat(strReturn,"Часы работы компрессора ТН за сезон (час)|");_ftoa(strReturn,(float)HP.get_motoHourC2()/60.0,1);strcat(strReturn,";");
        
        #ifdef USE_ELECTROMETER_SDM  
          strcat(strReturn,"Потребленная энергия ТН за сезон (кВт*ч)|");_ftoa(strReturn, HP.dSDM.get_Energy()-HP.get_motoHourE2(),2);strcat(strReturn,";");
        
        #endif
       
        #ifdef  FLOWCON 
	    if(HP.sTemp[TCONING].get_present() & HP.sTemp[TCONOUTG].get_present()) {strcat(strReturn,"Выработанная энергия ТН за сезон (кВт*ч)|");_ftoa(strReturn, HP.get_motoHourP2()/1000.0,2);strcat(strReturn,";");} // Если есть оборудование
        #endif
         
        ADD_WEBDELIM(strReturn) ;    continue;
       } // sisInfo
       
       if (strcmp(str,"test_Mail")==0)  // Функция test_mail
       {
       if (HP.message.setTestMail()) { strcat(strReturn,"Send test mail to "); HP.message.get_messageSetting((char*)mess_SMTP_RCPTTO,strReturn); }
       else { strcat(strReturn,"Error send test mail.");}
       ADD_WEBDELIM(strReturn) ;
        continue;  
       }   // test_Mail  
       if (strcmp(str,"test_SMS")==0)  // Функция test_mail
       {
       if (HP.message.setTestSMS()) { strcat(strReturn,"Send SMS to "); HP.message.get_messageSetting((char*)mess_SMS_PHONE,strReturn);}  //strcat(strReturn,HP.message.get_messageSetting(pSMS_PHONE));}
       else { strcat(strReturn,"Error send test sms.");}
       ADD_WEBDELIM(strReturn) ;
       continue;  
       }   // test_Mail    
       if(strcmp(str, "get_OverCool") == 0) {
           _ftoa(strReturn, HP.get_overcool() / 100.0, 2);
           ADD_WEBDELIM(strReturn);
           continue;
       }
       if(strcmp(str, "get_Evapor") == 0) {
    	   if(HP.sADC[PEVA].get_present()) _ftoa(strReturn, HP.get_temp_evaporating() / 100.0, 2);
    	   else strcat(strReturn,"-");
           ADD_WEBDELIM(strReturn);
    	   continue;
       }
       if(strcmp(str, "get_TCOMP_TCON") == 0) { // Нагнетание - Конденсация
           _ftoa(strReturn, (HP.sTemp[TCOMP].get_Temp() - HP.get_temp_condensing()) / 100.0, 2);
           ADD_WEBDELIM(strReturn) ;    continue;
       }
       // ЭРВ запросы , те которые без параметра ------------------------------
         if (strcmp(str,"set_zeroEEV")==0)  // Функция set_zeroEEV
         {  
            #ifdef EEV_DEF 
            _itoa(HP.dEEV.set_zero(),strReturn); 
            #else
            strcat(strReturn,"-");  
            #endif   
           ADD_WEBDELIM(strReturn) ; continue;
         }          
  
        // FC запросы, те которые без параметра ------------------------------
        if (strcmp(str,"reset_errorFC")==0)  // Функция get_dacFC
         {
          if (!HP.dFC.get_present()) {
        	  strcat(strReturn,"Инвертор отсутствует");
          }
          #ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
          else {
              HP.dFC.reset_errorFC();
              strcat(strReturn,"Ошибки инвертора сброшены . . .");
          }
          #endif
          ADD_WEBDELIM(strReturn); continue;
         } 
        if (strcmp(str,"reset_FC")==0)    
        {
          if (!HP.dFC.get_present()) {
        	  strcat(strReturn,"Инвертор отсутствует");
          } else {
        	  HP.dFC.reset_FC();                             // подать команду на сброс
        	  strcat(strReturn,"Cброс преобразователя частоты . . .");
          }
          ADD_WEBDELIM(strReturn); continue;
        }
         #ifdef USE_ELECTROMETER_SDM
        // SDM запросы которые без параметра
        if (strcmp(str,"settingSDM")==0)     // Функция settingSDM  Запрограммировать параметры связи счетчика
        {
          if (!HP.dSDM.get_present()) {
        	  strcat(strReturn,"Счетчик отсутвует");
          } else {
        	  HP.dSDM.progConnect();
        	  strcat(strReturn,"Счетчик запрограммирован, необходимо сбросить счетчик!!");
          }
          ADD_WEBDELIM(strReturn);  continue;
        }
        if (strcmp(str,"uplinkSDM")==0)     // Функция settingSDM  Попытаться возобновить связь со счетчиком при ее потери
         {
          if (!HP.dSDM.get_present()) {
        	  strcat(strReturn,"Счетчик отсутвует");
          } else {
        	  HP.dSDM.uplinkSDM();
        	  strcat(strReturn,"Проверка связи со счетчиком");
          }
       	  ADD_WEBDELIM(strReturn);  continue;
         } 
          #endif
        // -------------- СПИСКИ ДАТЧИКОВ и ИСПОЛНИТЕЛЬНЫХ УСТРОЙСТВ  -----------------------------------------------------
        // Список аналоговых датчиков выводятся только присутсвующие датчики список вида name:0;
        if (strcmp(str,"get_listPress")==0)     // Функция get_listPress
         {
          for(i=0;i<ANUMBER;i++) if (HP.sADC[i].get_present()){strcat(strReturn,HP.sADC[i].get_name());strcat(strReturn,";");}
          ADD_WEBDELIM(strReturn) ;    continue;
         } 
        if(strcmp(str, "get_listTemp") == 0) // Возвращает список датчиков через ";"
        {
        	for(i = 0; i < TNUMBER; i++) if(HP.sTemp[i].get_present()) { strcat(strReturn, HP.sTemp[i].get_name()); strcat(strReturn, ";"); }
        	ADD_WEBDELIM(strReturn); continue;
        }
        if(strncmp(str, "get_tblTemp", 11) == 0) // get_tblTempN - Возвращает список датчиков через ";", N число в конце - возвращаются датчики имеющие этот бит в SENSORTEMP[]
        {
        	uint8_t m = atoi(str + 11);
        	for(i = 0; i < TNUMBER; i++)
        		if((HP.sTemp[i].get_cfg_flags() & (1<<m)) && ((HP.sTemp[i].get_cfg_flags()&(1<<0)) || HP.sTemp[i].get_fAddress())) {
        			strcat(strReturn, HP.sTemp[i].get_name()); strcat(strReturn, ";");
        		}
        	ADD_WEBDELIM(strReturn); continue;
        }
        if (strcmp(str,"get_listInput")==0)     // Функция get_listInput
         {
          for(i=0;i<INUMBER;i++) if (HP.sInput[i].get_present()){strcat(strReturn,HP.sInput[i].get_name());strcat(strReturn,";");}
          ADD_WEBDELIM(strReturn) ;    continue;
         }         
        if (strcmp(str,"get_listRelay")==0)     // Функция get_listRelay
         {
          for(i=0;i<RNUMBER;i++) if (HP.dRelay[i].get_present()){strcat(strReturn,HP.dRelay[i].get_name());strcat(strReturn,";");}
          ADD_WEBDELIM(strReturn) ;    continue;
         }
        if (strcmp(str,"get_listFlow")==0)     // Функция get_lisFlow
         {
          for(i=0;i<FNUMBER;i++) if (HP.sFrequency[i].get_present()){strcat(strReturn,HP.sFrequency[i].get_name());strcat(strReturn,";");}
          ADD_WEBDELIM(strReturn) ;    continue;
         }
#ifdef RADIO_SENSORS
        if(strstr(str, "set_radio_cmd")) {
        	if((x = strchr(str, '='))) {
        		x++;
        		radio_sensor_send(x);
        	}
        	ADD_WEBDELIM(strReturn);	continue;
        }
#endif

       // -----------------------------------------------------------------------------------------------------        
       // 2. Функции с параметром ------------------------------------------------------------------------------
       // Ищем скобки ------------------------------------------------------------------------------------------
      if (((x=strpbrk(str,"("))!=0)&&((y=strpbrk(str,")"))!=0))  // Функция с одним параметром - найдена открывающиеся и закрывающиеся скобка
       {
       // Выделяем параметр функции на выходе число - номер параметра
       // применяется кодирование 0-19 - температуры 20-29 - сухой контакт 30-39 -аналоговые датчики
       y[0]=0;                                  // Стираем скобку ")"  строка х+1 содержит параметр

      // -----------------------------------------------------------------------------------------------------        
      // 2.1  Удаленный датчик
      // -----------------------------------------------------------------------------------------------------        
     #ifdef SENSOR_IP                           // Получение данных удаленного датчика
     // Получение данных с удаленного датчика
      if (strstr(str,"set_sensorIP"))           // Удаленные датчики - получить значения
      {
       // разбор строки формат "номер:температура:уровень_сигнала:питание:счетчик"
  //     Serial.println(x+1); 
       ptr=strtok(x+1,":");
       if (ptr==NULL)                {strcat(strReturn,"E21" WEBDELIM);continue;}
       if ((a=atoi(ptr))==0)         {strcat(strReturn,"E22" WEBDELIM);continue;}  // если возвращен 0 то ошибка преобразования
       if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23" WEBDELIM);continue;}  // проверка диапазона номеров датчиков

       ptr=strtok(NULL,":");
       if (ptr==NULL)                {strcat(strReturn,"E21" WEBDELIM);continue;}
       b=100*my_atof(ptr);
       if ((b<-4000)||(b>12000))      {strcat(strReturn,"E24" WEBDELIM);continue;}  // проверка диапазона температуры -40...+120
         
       ptr=strtok(NULL,":");
       if (ptr==NULL)                {strcat(strReturn,"E21" WEBDELIM);continue;}
       if ((c=10*my_atof(ptr))==0)   {strcat(strReturn,"E22" WEBDELIM);continue;}  // если возвращен 0 то ошибка преобразования
       if ((c<-1000)||(c>0))         {strcat(strReturn,"E24" WEBDELIM);continue;}  // проверка диапазона уровня сигнала -100...-1 дБ
     
       ptr=strtok(NULL,":");
       if (ptr==NULL)                {strcat(strReturn,"E21" WEBDELIM);continue;}
       if ((d=atoi(ptr))==0)         {strcat(strReturn,"E22" WEBDELIM);continue;}  // если возвращен 0 то ошибка преобразования
       if ((d<1000)||(d>5000))       {strcat(strReturn,"E24" WEBDELIM);continue;}  // проверка диапазона питания 1000...4000 мВ
       
       ptr=strtok(NULL,":");
       if (ptr==NULL)                {strcat(strReturn,"E21" WEBDELIM);continue;}
       e=atoi(ptr);
       if (e<0)                      {strcat(strReturn,"E24" WEBDELIM);continue;}  // счетичк больше 0
         
       
       // Дошли до сюда - ошибок нет, можно использовать данные
       byte adr[4];
       W5100.readSnDIPR(sock, adr);
       HP.sIP[a-1].set_DataIP(a,b,c,d,e,BytesToIPAddress(adr));
       strcat(strReturn,"OK" WEBDELIM); continue;
      }
 
      if (strstr(str,"get_sensorParamIP"))    // Удаленные датчики - Получить отдельное значение конкретного параметра
      {
       ptr=strtok(x+1,":");     // Нужно
       if (ptr==NULL)                {strcat(strReturn,"E21" WEBDELIM);continue;}  // нет параметра
       if ((a=atoi(ptr))==0)         {strcat(strReturn,"E22" WEBDELIM);continue;}  // если возвращен 0 то ошибка преобразования не число
       if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23" WEBDELIM);continue;}  // проверка диапазона номеров датчиков
       // Получили номер запрашиваемого датчика, теперь определяем параметр
         ptr=strtok(NULL,":");
       /*  
            if (strstr(ptr,"SENSOR_TEMP"))     strcat(strReturn,HP.sIP[a-1].get_sensorIP(pSENSOR_TEMP));
       else if (strstr(ptr,"SENSOR_NUMBER"))   strcat(strReturn,HP.sIP[a-1].get_sensorIP(pSENSOR_NUMBER));
       else if (strstr(ptr,"RSSI"))            strcat(strReturn,HP.sIP[a-1].get_sensorIP(pRSSI));
       else if (strstr(ptr,"VCC"))             strcat(strReturn,HP.sIP[a-1].get_sensorIP(pVCC));
       else if (strstr(ptr,"SENSOR_USE"))      strcat(strReturn,HP.sIP[a-1].get_sensorIP(pSENSOR_USE));
       else if (strstr(ptr,"SENSOR_RULE"))     strcat(strReturn,HP.sIP[a-1].get_sensorIP(pSENSOR_RULE));
       else if (strstr(ptr,"SENSOR_IP"))       strcat(strReturn,HP.sIP[a-1].get_sensorIP(pSENSOR_IP));
       else if (strstr(ptr,"SENSOR_COUNT"))    strcat(strReturn,HP.sIP[a-1].get_sensorIP(pSENSOR_COUNT));
       else if (strstr(ptr,"STIME"))           strcat(strReturn,HP.sIP[a-1].get_sensorIP(pSTIME));
       else if (strstr(ptr,"SENSOR"))          strcat(strReturn,"----");
       else strcat(strReturn,"E26");
      */
       HP.sIP[a-1].get_sensorIP(ptr,strReturn);
       ADD_WEBDELIM(strReturn) ; continue;
      }
      
     if (strstr(str,"get_sensorIP"))    // Удаленные датчики - Получить параметры (ВСЕ) удаленного датчика в виде строки
      {
       ptr=x+1; 
       if ((a=atoi(ptr))==0)         {strcat(strReturn,"E22" WEBDELIM);continue;}  // если возвращен 0 то ошибка преобразования
       if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23" WEBDELIM);continue;}  // проверка диапазона номеров датчиков
       // Формируем строку
       HP.sIP[a-1].get_sensorIP((char*)ip_SENSOR_NUMBER,strReturn); strcat(strReturn,":");
       
       if (HP.sIP[a-1].get_update()>UPDATE_IP)  strcat(strReturn,"-:") ;                       // Время просрочено, удаленный датчик не используем
       else { HP.sIP[a-1].get_sensorIP((char*)ip_SENSOR_TEMP,strReturn);strcat(strReturn,":"); }

       if (HP.sIP[a-1].get_count()>0)     // Если были пакеты то выводим данные по ним
       {
          HP.sIP[a-1].get_sensorIP((char*)ip_STIME,strReturn);strcat(strReturn,":");
          HP.sIP[a-1].get_sensorIP((char*)ip_SENSOR_IP,strReturn);strcat(strReturn,":");
          HP.sIP[a-1].get_sensorIP((char*)ip_RSSI,strReturn);strcat(strReturn,":");
          HP.sIP[a-1].get_sensorIP((char*)ip_VCC,strReturn); strcat(strReturn,":");
          HP.sIP[a-1].get_sensorIP((char*)ip_SENSOR_COUNT,strReturn); strcat(strReturn,":");  
       }
       else strcat(strReturn,"-:-:-:-:-:");  // После включения еще ни разу данные не поступали поэтому прочерки
        
       ADD_WEBDELIM(strReturn) ; continue;
      }  
      
      if (strstr(str,"get_sensorListIP"))    // Удаленные датчики - список привязки удаленного датчика
      {
       ptr=x+1; 
       if ((a=atoi(ptr))==0)         {strcat(strReturn,"E22" WEBDELIM);continue;}  // если возвращен 0 то ошибка преобразования
       if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23" WEBDELIM);continue;}  // проверка диапазона номеров датчиков
       
       strcat(strReturn,"Reset:"); if (HP.sIP[a-1].get_link()==-1) strcat(strReturn,"1;"); else strcat(strReturn,"0;");
       for(i=0;i<TNUMBER;i++)        // формирование списка
           {
               if (HP.sTemp[i].get_present())  // только представленные датчики
               {
               strcat(strReturn,HP.sTemp[i].get_name()); strcat(strReturn,":");
               if (HP.sIP[a-1].get_link()==i) strcat(strReturn,"1;"); else strcat(strReturn,"0;");
               }
           }
       ADD_WEBDELIM(strReturn) ; continue;
      }   
      

      if (strstr(str,"get_sensorUseIP"))    // Удаленные датчики - ПОЛУЧИТЬ использование удаленного датчика
      {
       if ((a=atoi(x+1))==0)         {strcat(strReturn,"E22" WEBDELIM);continue;}  // если возвращен 0 то ошибка преобразования
       if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23" WEBDELIM);continue;}  // проверка диапазона номеров датчиков
       HP.sIP[a-1].get_sensorIP((char*)ip_SENSOR_USE,strReturn);ADD_WEBDELIM(strReturn) ;continue;
      }

 
      if (strstr(str,"get_sensorRuleIP"))    // Удаленные датчики - ПОЛУЧИТЬ использование усреднение
      {
       if ((a=atoi(x+1))==0)         {strcat(strReturn,"E22" WEBDELIM);continue;}  // если возвращен 0 то ошибка преобразования
       if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23" WEBDELIM);continue;}  // проверка диапазона номеров датчиков
       HP.sIP[a-1].get_sensorIP((char*)ip_SENSOR_RULE,strReturn);ADD_WEBDELIM(strReturn) ;continue;
      }
   
      #else
       if (strstr(str,"set_sensorIP"))   {strcat(strReturn,"E25" WEBDELIM);continue;}        // Удаленные датчики  НЕ ПОДДЕРЖИВАЕТСЯ
     #endif  

        // ----------------------------------------------------------------------------------------------------------
       // 2.2 Функции с одним параметром
       // ----------------------------------------------------------------------------------------------------------

      if (strstr(str,"set_modeHP"))           // Функция set_modeHP - установить режим отопления ТН
         {
        if ((pm=my_atof(x+1))==ATOF_ERROR)  strcat(strReturn,"E09");      // Ошибка преобразования   - завершить запрос с ошибкой
        else
           {
             switch ((MODE_HP)pm)  // режим работы отопления
                   {
                    case pOFF:   HP.set_mode(pOFF);  strcat(strReturn,(char*)"Выключено:1;Отопление:0;Охлаждение:0;"); break;
                    case pHEAT:  HP.set_mode(pHEAT); strcat(strReturn,(char*)"Выключено:0;Отопление:1;Охлаждение:0;"); break;
                    case pCOOL:  HP.set_mode(pCOOL); strcat(strReturn,(char*)"Выключено:0;Отопление:0;Охлаждение:1;"); break;
                    default: HP.set_mode(pOFF);  strcat(strReturn,(char*)"Выключено:1;Отопление:0;Охлаждение:0;"); break;   // Исправить по умолчанию
                   }  
           }
   //        Serial.print(pm); Serial.print("   "); Serial.println(HP.get_modeHouse() );
           ADD_WEBDELIM(strReturn) ; continue;
          } // strcmp(str,"set_modeHP")==0)      
      // ------------------------------------------------------------------------  
      if (strstr(str,"set_testMode"))  // Функция set_testMode  - Установить режим работы бойлера
       {
       if ((pm=my_atof(x+1))==ATOF_ERROR)  strcat(strReturn,"E09");      // Ошибка преобразования   - завершить запрос с ошибкой
       else
           {
            HP.set_testMode((TEST_MODE)pm);             // Установить режим работы тестирования
           for(i=0;i<=HARD_TEST;i++)                    // Формирование списка
              { strcat(strReturn,noteTestMode[i]); strcat(strReturn,":"); if(i==HP.get_testMode()) strcat(strReturn,cOne); else strcat(strReturn,cZero); strcat(strReturn,";");  }                
           } // else  
          ADD_WEBDELIM(strReturn) ;    continue;
        } //  if (strcmp(str,"set_testMode")==0)  

         // -----------------------------------------------------------------------------  
        if (strstr(str,"set_EEV"))  // Функция set_EEV
             {
             #ifdef EEV_DEF 
             if ((pm=my_atof(x+1))==ATOF_ERROR)  strcat(strReturn,"E09");      // Ошибка преобразования   - завершить запрос с ошибкой
              else
                {
                if(HP.dEEV.set_EEV((int)pm)==0) 
                _itoa(pm,strReturn);  // если значение правильное его возвращаем сразу
                else strcat(strReturn,"E12"); 
     //           Serial.print("set_EEV ");    Serial.print(pm); Serial.print(" set "); Serial.println(int2str(HP.dEEV.get_EEV())); 
                ADD_WEBDELIM(strReturn) ;    continue;
                } 
             #else
                strcat(strReturn,"-" WEBDELIM) ;    continue;
             #endif 
               }  //  if (strcmp(str,"set_set_EEV")==0)    
         // -----------------------------------------------------------------------------  
        if (strstr(str,"set_targetFreq"))  // Функция set_EEV
             {
            if ((pm=my_atof(x+1))==ATOF_ERROR)  strcat(strReturn,"E09");      // Ошибка преобразования   - завершить запрос с ошибкой
              else
                {
                  if(HP.dFC.set_targetFreq(rd(pm, 100),true,HP.dFC.get_minFreqUser() ,HP.dFC.get_maxFreqUser())==0) _itoa(HP.dFC.get_targetFreq()/100,strReturn); else strcat(strReturn,"E12");  ADD_WEBDELIM(strReturn) ;    continue;   // ручное управление границы максимальны
                }
               }  //  if (strcmp(str,"set_set_targetFreq")==0)    
         // -----------------------------------------------------------------------------  
         // ПРОФИЛИ функции с одним параметром
         // ----------------------------------------------------------------------------- 
          if (strstr(str,"saveProfile"))  // Функция saveProfile сохранение текущего профиля
             {
            if ((pm=my_atof(x+1))==ATOF_ERROR)  strcat(strReturn,"E29");      // Ошибка преобразования   - завершить запрос с ошибкой
              else
                {
                  if ((pm>=0)&&(pm<I2C_PROFIL_NUM)) _itoa(HP.Prof.save((int8_t)pm),strReturn); else strcat(strReturn,"E29");  ADD_WEBDELIM(strReturn) ;    continue;
                }
               }  //if (strstr(str,"saveProfile"))  
           // -----------------------------------------------------------------------------     
            if (strstr(str,"loadProfile"))  // Функция loadProfile загрузка профиля в текущий
             {
            if ((pm=my_atof(x+1))==ATOF_ERROR)  strcat(strReturn,"E29");      // Ошибка преобразования   - завершить запрос с ошибкой
              else
                {
                  if ((pm>=0)&&(pm<I2C_PROFIL_NUM)) _itoa(HP.Prof.load((int8_t)pm),strReturn); else strcat(strReturn,"E29");  ADD_WEBDELIM(strReturn) ;    continue;
                }
               }  //if (strstr(str,"loadProfile"))  
            // -----------------------------------------------------------------------------     
            if (strstr(str,"infoProfile"))  // Функция infoProfile получить информацию о профиле
             {
            if ((pm=my_atof(x+1))==ATOF_ERROR)  strcat(strReturn,"E29");      // Ошибка преобразования   - завершить запрос с ошибкой
              else
                {
                  if ((pm>=0)&&(pm<I2C_PROFIL_NUM)) HP.Prof.get_info(strReturn,(int8_t)pm); else strcat(strReturn,"E29");  ADD_WEBDELIM(strReturn) ;    continue;
                }
               }  //if (strstr(str,"infoProfile"))   
           // -----------------------------------------------------------------------------     
            if (strstr(str,"eraseProfile"))  // Функция eraseProfile стереть профиль
             {
            if ((pm=my_atof(x+1))==ATOF_ERROR)  strcat(strReturn,"E29");      // Ошибка преобразования   - завершить запрос с ошибкой
              else
                {
                  if (pm==HP.Prof.get_idProfile())  {strcat(strReturn,"E30");}  // попытка стереть текущий профиль
                  else if((pm>=0)&&(pm<I2C_PROFIL_NUM)) { _itoa(HP.Prof.erase((int8_t)pm),strReturn); HP.Prof.update_list(HP.Prof.get_idProfile()); }
                  else strcat(strReturn,"E29"); 
                  ADD_WEBDELIM(strReturn) ;    continue;
                }
               }  //if (strstr(str,"eraseProfile"))     
          // -----------------------------------------------------------------------------     
            if (strstr(str,"set_listProfile"))  // Функция set_listProfil загрузить профиль из списка и сразу СОХРАНИТЬ !!!!!!
            {
                if ((pm=my_atof(x+1))==ATOF_ERROR)  strcat(strReturn,"E29");      // Ошибка преобразования   - завершить запрос с ошибкой
                else {
            	  if((pm > 0) && (pm <= I2C_PROFIL_NUM)) {
            		  HP.Prof.set_list((int8_t)pm - 1);
            		  HP.save();
            		  HP.Prof.save(HP.Prof.get_idProfile());
            		  HP.Prof.get_list(strReturn/*,HP.Prof.get_idProfile()*/);
            	  } else strcat(strReturn,"E29");
                  ADD_WEBDELIM(strReturn) ;    continue;
                }
            }  //if (strstr(str,"set_listProfile"))

       //  2.3 Функции с параметрами
      // проверяем наличие функции set_  конструкция типа (TIN=23)
        char strbuf[MAX_LEN_PM+1]; // буфер для хранения строкового параметра, описание профиля до 120 байт - может быть 40 русских букв прикодировании URL это по 3 байта на букву
        if ((z=strpbrk(str,"="))!=0)  // нашли знак "=" запрос на установку параметра
        { 
          // Serial.print(strlen(z+1));Serial.print(" >");Serial.print(z+1);Serial.println("<") ;          
           strncpy(strbuf,z+1,MAX_LEN_PM); // Сохранение для запроса у которого параметр не число а строка - адрес
       //    pm = atof(z+1);               // Добавляет 9 кб кода !!!!!!!!!!!!!!!!
           pm = my_atof(z+1);              // мой код 150 байт и отработка ошибки
          // формируем начало ответа - повторение запроса без параметра установки  ДЛЯ set_ запросов
          z[0]=0; // Обрезать строку - откинуть значение параметра - мы его получили pm
          strcat(strReturn,str); 
          strcat(strReturn,")=");
    //      if (pm==ATOF_ERROR)        // Ошибка преобразования   - завершить запрос с ошибкой
   //       { strcat(strReturn,"E04");ADD_WEBDELIM(strReturn);  continue;  }
         } else z=NULL; // "=" - не обнаружено, значит значение пустая строка
         
        // --------------------------------НОВЫЙ ПАРСЕР ------------------------------------------------------------------- 
        // Вот сюда будет вставлятся код нового парсера (который не будет кодировать параметры в целые числа)
        // ВХОД str - полное имя запроса до (), x+1 - содержит строку (имя параметра), z+1 - после = (значение), pm - флоат z+1
        // ВЫХОД strReturn  надо Добавлять + в конце &
        x[0]=0;   // Стираем скобку "("  строка х+1 содержит параметр а str содержит имя запроса

       // 1. Проверка для запросов содержащих EEV  ----------------------------------------------------    
       if (strstr(str,"EEV"))          
              {
              #ifdef EEV_DEF 
              if (strcmp(str,"get_paramEEV")==0)           // Функция get_paramEEV - получить значение параметра ЭРВ
                  {
                  HP.dEEV.get_paramEEV(x+1,strReturn);	
                  ADD_WEBDELIM(strReturn); continue;
                  }
               else if (strcmp(str,"set_paramEEV")==0)    // Функция set_paramEEV - установить значение паремтра ЭРВ 
                  {
                  if (pm!=ATOF_ERROR) {   // нет ошибки преобразования
                    if (HP.dEEV.set_paramEEV(x+1,pm)) HP.dEEV.get_paramEEV(x+1,strReturn);
                    else  strcat(strReturn,"E11");  // выход за диапазон значений   
                  } else strcat(strReturn,"E11");   // ошибка преобразования во флоат
                  ADD_WEBDELIM(strReturn) ;
                  continue;	 
                  }
                else   strcat(strReturn,"E03" WEBDELIM);  continue;
              #else
               strcat(strReturn,"no support EEV" WEBDELIM);  continue;
              #endif   
              }  //  if (strstr(str,"EEV"))    
          // 2. Проверка для запросов содержащих MQTT --------------------------------------------- 
              if (strstr(str,"MQTT"))          // Проверка для запросов содержащих MQTT
              {
			   #ifdef MQTT
                   if (strcmp(str,"get_MQTT")==0){           // Функция получить настройки MQTT
                        HP.clMQTT.get_paramMQTT(x+1,strReturn);
                        ADD_WEBDELIM(strReturn) ; continue;
                   } else if (strcmp(str,"set_MQTT")==0) {         // Функция записать настройки MQTT
                         if (HP.clMQTT.set_paramMQTT(x+1,strbuf))  HP.clMQTT.get_paramMQTT(x+1,strReturn);   // преобразование удачно
                         else strcat(strReturn,"E32") ; // ошибка преобразования строки
                      ADD_WEBDELIM(strReturn) ; continue;
                      } // (strcmp(str,"set_MQTT")==0) 
				#else
					 strcat(strReturn,"no support MQTT" WEBDELIM);  continue; // не поддерживается
				#endif
               } //if ((strstr(str,"MQTT")>0)
          
           // 3. Расписание -------------------------------------------------------- 
			// ошибки: E33 - не верный номер расписания, E34 - не хватает места для календаря
			if(strstr(str,"SCHDLR")) { // Класс Scheduler
				x++;
				if(strncmp(str, "set", 3) == 0) { // set_SCHDLR(x=n)
					if((i = HP.Schdlr.web_set_param(x, z+1))) {
						strcat(strReturn, i == 1 ? "E33" : "E34");
						ADD_WEBDELIM(strReturn);
						continue;
					}
				} else if(strncmp(str, "get", 3) == 0) { // get_SCHDLR(x)
				} else goto x_FunctionNotFound;
				HP.Schdlr.web_get_param(x, strReturn);
				ADD_WEBDELIM(strReturn);
				continue;
			}

         // 4. Настройки счетчика SDM ------------------------------------------------
			if(strstr(str, "SDM"))          // Проверка для запросов содержащих SDM
			{
#ifdef USE_ELECTROMETER_SDM
				if(strcmp(str, "get_SDM") == 0)           // Функция получить настройки счетчика
				{
					HP.dSDM.get_paramSDM(x + 1, strReturn);
				} else if(strcmp(str, "set_SDM") == 0)           // Функция записать настройки счетчика
				{
					if(HP.dSDM.set_paramSDM(x + 1, strbuf)) HP.dSDM.get_paramSDM(x + 1, strReturn); // преобразование удачно
					else strcat(strReturn, "E31");            // ошибка преобразования строки
				} else strcat(strReturn, "E03");
#else
				strcat(strReturn,"no support SDM"); // не поддерживается
#endif
				ADD_WEBDELIM(strReturn); continue;
			}

            // 5.  Настройки профилей ---------------------------------------------------------         
			if(strstr(str, "Profile"))          // Проверка для запросов содержащих Profile
			{
				if(strcmp(str, "get_Profile") == 0)           // Функция получить настройки профиля
				{
					HP.Prof.get_paramProfile(x + 1, strReturn);
				} // (strcmp(str,"get_Profile")==0)
				else if(strcmp(str, "set_Profile") == 0)           // Функция записать настройки профиля
				{
					if(HP.Prof.set_paramProfile(x + 1, strbuf)) HP.Prof.get_paramProfile(x + 1, strReturn); // преобразование удачно
					else strcat(strReturn, "E28"); // ошибка преобразования строки
				} else strcat(strReturn, "E03");
				ADD_WEBDELIM(strReturn); continue;
			}
		           
             //6.  Настройки Уведомлений --------------------------------------------------------
            if (strstr(str,"Message"))          // Проверка для запросов содержащих messageSetting
             {
              if (strcmp(str,"get_Message")==0)           // Функция get_Message - получить значение настройки уведомлений
                  {
                    HP.message.get_messageSetting(x+1,strReturn);
                    ADD_WEBDELIM(strReturn) ; continue;
                  } // (strcmp(str,"get_messageSetting")==0) 
              else if (strcmp(str,"set_Message")==0)           // Функция set_Message - установить значениена стройки уведомлений
                  {
                   if (HP.message.set_messageSetting(x+1,strbuf)) HP.message.get_messageSetting(x+1,strReturn); // преобразование удачно
                   else                                                     strcat(strReturn,"E20") ; // ошибка преобразования строки
                  ADD_WEBDELIM(strReturn) ; continue;
                  } else strcat(strReturn,"E03" WEBDELIM);  continue;	// (strcmp(str,"set_messageSetting")==0)
              } //if ((strstr(str,"messageSetting")>0)   
              
	          //7.  Настройки бойлера --------------------------------------------------
	          if (strstr(str,"Boiler"))          // Проверка для запросов содержащих Boiler
	           {
	              if (strcmp(str,"get_Boiler")==0)           // Функция get_Boiler - получить значение настройки бойлера
	                  {
	                    HP.Prof.get_boiler(x+1,strReturn);
	                    ADD_WEBDELIM(strReturn) ; continue;
	                  } // (strcmp(str,"get_Boiler")==0) 
	              else if (strcmp(str,"set_Boiler")==0)           // Функция set_Boiler - установить значениена стройки бойлера
	                  {
	                   if (HP.Prof.set_boiler(x+1,strbuf)) HP.Prof.get_boiler(x+1,strReturn);  // преобразование удачно
	                   else 	                      strcat(strReturn,"E19") ; // ошибка преобразования строки
	                  ADD_WEBDELIM(strReturn) ; continue;
	                  } else strcat(strReturn,"E03" WEBDELIM);  continue; // (strcmp(str,"set_Boiler")==0)
	             } //if ((strstr(str,"Boiler")>0)   

	           //8.  Настройки дата время --------------------------------------------------------
		          if (strstr(str,"datetime"))          // Проверка для запросов содержащих datetime
		             {
		              if (strcmp(str,"get_datetime")==0)           // Функция get_datetim - получить значение даты времени
		                  {
		                    HP.get_datetime(x+1,strReturn);
		                    ADD_WEBDELIM(strReturn) ; continue;
		                  } // (strcmp(str,"get_datetime")==0) 
   		              else if (strcmp(str,"set_datetime")==0)           // Функция set_datetime - установить значение даты и времени
 		                  {
		                   if (HP.set_datetime(x+1,strbuf))  HP.get_datetime(x+1,strReturn);    // преобразование удачно
	                       else  strcat(strReturn,"E18") ; // ошибка преобразования строки
		                  ADD_WEBDELIM(strReturn) ; continue;
		                  }  else strcat(strReturn,"E03" WEBDELIM);  continue;// (strcmp(str,"set_datetime")==0)
		            } //if ((strstr(str,"datetime")>0)   

	           //9.  Настройки сети -----------------------------------------------------------
	          if (strstr(str,"Network"))          // Проверка для запросов содержащих Network
	             {
	               if (strcmp(str,"get_Network")==0)           // Функция get_Network - получить значение параметра Network
	                  {
	                    HP.get_network(x+1,strReturn);
	                    ADD_WEBDELIM(strReturn) ; continue;
	                  } // (strcmp(str,"get_Network")==0) 
	              else if (strcmp(str,"set_Network")==0)           // Функция set_Network - установить значение паремтра Network
	                  {
	                   if (HP.set_network(x+1,strbuf))  HP.get_network(x+1,strReturn);     // преобразование удачно
	                   else strcat(strReturn,"E15") ; // ошибка преобразования строки
	                  ADD_WEBDELIM(strReturn) ; continue;
	                  }  else strcat(strReturn,"E03" WEBDELIM);  continue; // (strcmp(str,"set_Network")==0)
	             } //if ((strstr(str,"Network")>0)   

		         //10.  Статистика используется в одной функции get_Stat ---------------------------------------
//		          if (strstr(str,"Stat"))   // Проверка для запросов содержащих Stat
//		           {
//		               if (strcmp(str,"get_Stat")==0)
//		                  {
//		                  #ifdef I2C_EEPROM_64KB
//		                  HP.Stats.get_Stat(x+1,strReturn, true);
//		                  #else
//		                  strcat(strReturn,"");
//		                  #endif
//		                  ADD_WEBDELIM(strReturn) ; continue;
//		                  } else strcat(strReturn,"E03" WEBDELIM);  continue; // (strcmp(str,"get_Stat")==0)
//		            } //if ((strstr(str,"Stat")>0)
                  
		          //11.  Графики смещение  используется в одной функции get_Chart -------------------------------------------
		          if (strstr(str,"Chart"))          // Проверка для запросов содержащих Chart
		             {
		               if (strcmp(str,"get_Chart")==0)           // Функция get_Chart - получить график
		                  {
		                  HP.get_Chart(x+1,strReturn);
		                  ADD_WEBDELIM(strReturn) ; continue;
		                  } else strcat(strReturn,"E03" WEBDELIM);  continue;  // (strcmp(str,"get_Chart")==0)
		            } //if ((strstr(str,"Chart")>0)  
         
                  //12.  Частотный преобразователь -----------------------------------------------------------------
		          if (strstr(str,"FC"))          // Проверка для запросов содержащих FC get_paramFC set_paramFC 
		             {
     	               if (strcmp(str,"get_paramFC")==0)           // Функция get_paramFC - получить значение параметра FC
		                  {
		                    HP.dFC.get_paramFC(x+1,strReturn); ADD_WEBDELIM(strReturn) ; continue;
		                  } 
		              else if (strcmp(str,"set_paramFC")==0)           // Функция set_paramFC - установить значение паремтра FC
		                  {
							if (pm!=ATOF_ERROR) {   // нет ошибки преобразования
                    		if (HP.dFC.set_paramFC(x+1,pm)) HP.dFC.get_paramFC(x+1,strReturn);
                    		else  strcat(strReturn,"E27");  // выход за диапазон значений   
                  			} else strcat(strReturn,"E11");   // ошибка преобразования во флоат
                           ADD_WEBDELIM(strReturn) ;
   		                  } else strcat(strReturn,"E03" WEBDELIM);  continue; // (strcmp(str,"set_paramFC")==0)
		            } //if ((strstr(str,"FC")>0)  

                  // 13 Опции теплового насоса
         	     if (strstr(str,"optionHP"))          // Проверка для запросов содержащих optionHP
		             {         
                      if (strcmp(str,"get_optionHP")==0)           // Функция get_optionHP - получить значение параметра отопления ТН
		                  {
		                   HP.get_optionHP(x+1,strReturn); ADD_WEBDELIM(strReturn) ; continue;
		                  } 
	                  else if (strcmp(str,"set_optionHP")==0)           // Функция set_optionHP - установить значение паремтра  опций
				          {
				           if (pm!=ATOF_ERROR) {   // нет ошибки преобразования
				           if (HP.set_optionHP(x+1,pm))   HP.get_optionHP(x+1,strReturn);  // преобразование удачно, 
				           else strcat(strReturn,"E17") ; // выход за диапазон значений
				           } else strcat(strReturn,"E11");   // ошибка преобразования во флоат
				          ADD_WEBDELIM(strReturn) ; continue;
				          } else strcat(strReturn,"E03" WEBDELIM);  continue;
		             }
		          //14.  Параметры  отопления и охлаждения ТН
		          if (strstr(str,"HP"))          // Проверка для запросов содержащих HP
		             {
		               if (strcmp(str,"get_paramCoolHP")==0)           // Функция get_paramCoolHP - получить значение параметра охлаждения ТН
		                  { HP.Prof.get_paramCoolHP(x+1,strReturn,HP.dFC.get_present()); ADD_WEBDELIM(strReturn) ; continue;   }
		              else if (strcmp(str,"set_paramCoolHP")==0)           // Функция set_paramCoolHP - установить значение паремтра охлаждения ТН
		                  {
		                  if (pm!=ATOF_ERROR) {   // нет ошибки преобразования
		                    if (HP.Prof.set_paramCoolHP(x+1,pm))  HP.Prof.get_paramCoolHP(x+1,strReturn,HP.dFC.get_present());    // преобразование удачно
		                    else  strcat(strReturn,"E16") ; // ошибка преобразования строки
		                    } else strcat(strReturn,"E11");   // ошибка преобразования во флоат 
		                  ADD_WEBDELIM(strReturn) ; continue;
		                  } // (strcmp(str,"paramCoolHP")==0) 
		               else if (strcmp(str,"get_paramHeatHP")==0)           // Функция get_paramHeatHP - получить значение параметра отопления ТН
		                  { HP.Prof.get_paramHeatHP(x+1,strReturn,HP.dFC.get_present()); ADD_WEBDELIM(strReturn) ; continue;   }
		               else if (strcmp(str,"set_paramHeatHP")==0)           // Функция set_paramHeatHP - установить значение паремтра отопления ТН
		                  {
		                  if (pm!=ATOF_ERROR) {   // нет ошибки преобразования
		                    if (HP.Prof.set_paramHeatHP(x+1,pm))  HP.Prof.get_paramHeatHP(x+1,strReturn,HP.dFC.get_present());    // преобразование удачно
		                    else  strcat(strReturn,"E16") ; // ошибка преобразования строки
		                    } else strcat(strReturn,"E11");   // ошибка преобразования во флоат 
		                  ADD_WEBDELIM(strReturn) ; continue;
		                  } 
		             }
                   
             
		// str - полное имя запроса до (), x+1 - содержит строку что между (), z+1 - после =
		// код обработки установки значений модбас
		// get_modbus_val(N:D:X), set_modbus_val(N:D:X=YYY)
		// N - номер устройства, D - тип данных, X - адрес, Y - новое значение
		if(strstr(str,"et_modbus_")) {
			x++;
			if((y = strchr(x, ':'))) {
				*y++ = '\0';
				uint8_t id = atoi(x);
				uint16_t par = atoi(y + 2); // Передается нумерация регистров с 1, а в modbus с 0
				if(par--) {
					i = OK;
					if(strncmp(str, "set", 3) == 0) {
						z++;
						if(*y == 'w') i = Modbus.writeHoldingRegisters16(id, par, strtol(z, NULL, 0));
						else if(*y == 'l') i = Modbus.writeHoldingRegisters32(id, par, strtol(z, NULL, 0));
						else if(*y == 'f') i = Modbus.writeHoldingRegistersFloat(id, par, strtol(z, NULL, 0));
						else if(*y == 'c') i = Modbus.writeSingleCoil(id, par, atoi(z));
						else goto x_FunctionNotFound;
						_delay(MODBUS_TIME_TRANSMISION * 10); // Задержка перед чтением
					} else if(strncmp(str, "get", 3) == 0) {
					} else goto x_FunctionNotFound;
					if(i == OK) {
						if(*y == 'w') {
							if((i = Modbus.readHoldingRegisters16(id, par, &par)) == OK) _itoa(par, strReturn);
						} else if(*y == 'l') {
								if((i = Modbus.readHoldingRegisters32(id, par, (uint32_t *)&e)) == OK) _itoa(e, strReturn);
						} else if(*y == 'i') {
							if((i = Modbus.readInputRegistersFloat(id, par, &pm)) == OK) _ftoa(strReturn, pm, 2);
						} else if(*y == 'f') {
							if((i = Modbus.readHoldingRegistersFloat(id, par, &pm)) == OK) _ftoa(strReturn, pm, 2);
						} else if(*y == 'c') {
							if((i = Modbus.readCoil(id, par, (boolean *)&par)) == OK) _itoa(par, strReturn);
						} else goto x_FunctionNotFound;
					}
					if(i != OK) {
						strcat(strReturn, "E"); _itoa(i, strReturn);
					}
					ADD_WEBDELIM(strReturn);
					continue;
				}
			}
		}

       // --- УДАЛЕННЫЕ ДАТЧИКИ ----------  кусок кода для удаленного датчика - установка параметров ответ - повторение запроса уже сделали
         #ifdef SENSOR_IP                           // Получение данных удаленного датчика
              
              if (strstr(str,"set_sensorListIP"))    // Удаленные датчики - привязка датчика
              {
               // первое число (имя удаленного датчика)
               if ((x+1)==NULL)              {strcat(strReturn,"E21" WEBDELIM);continue;}
               if ((a=atoi(x+1))==0)         {strcat(strReturn,"E22" WEBDELIM);continue;}  // если возвращен 0 то ошибка преобразования
               if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23" WEBDELIM);continue;}  // проверка диапазона номеров датчиков
               
               // Второе число (новое значение параметра)
               if (strbuf==NULL)             {strcat(strReturn,"E21" WEBDELIM);continue;}
               b=atoi(strbuf);
               if ((b<0)||(b>TNUMBER))       {strcat(strReturn,"E24" WEBDELIM);continue;}  //  проверка диапазона номеров датчиков учитываем reset!!
               
               c=-1; 
               HP.sIP[a-1].set_link(-1);     // Сбросить старую привязку (привязки нет)
               if(b>0)                       // если привязка есть
               {
               b--;                          // убрать пункт ресет
               for(i=0;i<TNUMBER;i++)        // формирование списка
                   {
                    if (HP.sTemp[i].get_present())  c++;             // считаем индекс с учетом только представленных датчиков
                    if (c==b) { HP.sIP[a-1].set_link(i); break;}     // а запоминаем фактический индекс
         //           if (strstr(ptr,HP.sTemp[i].get_name())) { HP.sIP[a-1].set_link(i); break;}     // а запоминаем фактический индекс
                   }
               }
                HP.updateLinkIP();                 // Обновить ВСЕ привязки удаленных датчиков
                 
                // Формируем список для ответа
                 strcat(strReturn,"Reset:"); if (HP.sIP[a-1].get_link()==-1) strcat(strReturn,"1;"); else strcat(strReturn,"0;");
                 for(i=0;i<TNUMBER;i++)        // формирование списка
                   {
                       if (HP.sTemp[i].get_present())  // только представленные датчики
                       {
                       strcat(strReturn,HP.sTemp[i].get_name()); strcat(strReturn,":");
                       if (HP.sIP[a-1].get_link()==i) strcat(strReturn,"1;"); else strcat(strReturn,"0;");
                       }
                   }
                 ADD_WEBDELIM(strReturn) ; continue;
                
                } 
      
             if (strstr(str,"set_sensorUseIP"))    // Удаленные датчики - УСТАНОВИТЬ использование удаленного датчика
              {
               // первое число (имя удаленного датчика)
               if ((x+1)==NULL)              {strcat(strReturn,"E21" WEBDELIM);continue;}
               if ((a=atoi(x+1))==0)         {strcat(strReturn,"E22" WEBDELIM);continue;}  // если возвращен 0 то ошибка преобразования
               if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23" WEBDELIM);continue;}  // проверка диапазона номеров датчиков
               // Второе число (зновое значение параметра)
               if (strbuf==NULL)             {strcat(strReturn,"E21" WEBDELIM);continue;}
               b=atoi(strbuf);
               if ((b<0)||(b>1))             {strcat(strReturn,"E24" WEBDELIM);continue;}  //  проверка диапазона
 
               if (b==1) HP.sIP[a-1].set_fUse(true); else HP.sIP[a-1].set_fUse(false);
               
               HP.updateLinkIP();                 // Обновить ВСЕ привязки удаленных датчиков
               
               _itoa(HP.sIP[a-1].get_fUse(),strReturn);ADD_WEBDELIM(strReturn) ;continue;
              }

             if (strstr(str,"set_sensorRuleIP"))    // Удаленные датчики - УСТАНОВИТЬ использование усреднение
              {
               // первое число (имя удаленного датчика)
               if ((x+1)==NULL)              {strcat(strReturn,"E21" WEBDELIM);continue;}
               if ((a=atoi(x+1))==0)         {strcat(strReturn,"E22" WEBDELIM);continue;}  // если возвращен 0 то ошибка преобразования
               if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23" WEBDELIM);continue;}  // проверка диапазона номеров датчиков
               
               if (strbuf==NULL)             {strcat(strReturn,"E21" WEBDELIM);continue;}
               b=atoi(strbuf);
               if ((b<0)||(b>1))             {strcat(strReturn,"E24" WEBDELIM);continue;}  //  проверка диапазона
          
               if (b==1) HP.sIP[a-1].set_fRule(true); else HP.sIP[a-1].set_fRule(false);
               _itoa(HP.sIP[a-1].get_fRule(),strReturn);ADD_WEBDELIM(strReturn) ;continue;
              }              
         #endif //  #ifdef SENSOR_IP  

       //////////////////////////////////////////// массивы датчиков ////////////////////////////////////////////////

       if(pm==ATOF_ERROR) { strcat(strReturn,"E04");ADD_WEBDELIM(strReturn);continue; }// Ошибка преобразования для чисел - завершить запрос с ошибкой

       { // Массивы датчиков
    	   int16_t p = -1;
    	   x++;
    	   for(i=0; i<TNUMBER; i++) if(strcmp(x,HP.sTemp[i].get_name())==0) {p=i; break;} // Поиск среди имен  смещение 0
    	   if(p==-1)  {for(i=0;i<ANUMBER;i++) if(strcmp(x,HP.sADC[i].get_name())==0) {p=100+i; break;}} // Поиск среди имен смещение 100
    	   if(p==-1)  {for(i=0;i<INUMBER;i++) if(strcmp(x,HP.sInput[i].get_name())==0) {p=200+i; break;}} // Поиск среди имен смещение 200
    	   if(p==-1)  {for(i=0;i<FNUMBER;i++) if(strcmp(x,HP.sFrequency[i].get_name())==0) {p=300+i; break;}} // Частотные датчики смещение 300
    	   if(p==-1)  {for(i=0;i<RNUMBER;i++) if(strcmp(x,HP.dRelay[i].get_name())==0) {p=400+i; break;}} // Реле
    	   if(p==-1)  { strcat(strReturn,"E02");ADD_WEBDELIM(strReturn);  continue; }  // Не верный параметр

    	   //     x[0]=0;                                                                        // Обрезаем строку до скобки (
    	   // Все готово к разбору имен функций c параметром
    	   // 1. Датчики температуры смещение param 0
    	   if (strstr(str,"Temp"))          // Проверка для запросов содержащих Temp
    	   {
    		   if(p >= TNUMBER)  {strcat(strReturn,"E03");ADD_WEBDELIM(strReturn);  continue; }  // Не соответсвие имени функции и параметра
    		   else  // параметр верный
    		   {
    			   if (strcmp(str,"get_Temp")==0)              // Функция get_Temp
    			   { if (HP.sTemp[p].get_present())          // Если датчик есть в конфигурации то выводим значение
    				   _ftoa(strReturn,(float)HP.sTemp[p].get_Temp()/100.0,1);
    			   else strcat(strReturn,"-");             // Датчика нет ставим прочерк
    			   ADD_WEBDELIM(strReturn); continue; }
    			   if (strcmp(str,"get_rawTemp")==0)           // Функция get_RawTemp
    			   { if (HP.sTemp[p].get_present())          // Если датчик есть в конфигурации то выводим значение
    				   _ftoa(strReturn,(float)HP.sTemp[p].get_rawTemp()/100.0,1);
    			   else strcat(strReturn,"-");             // Датчика нет ставим прочерк
    			   ADD_WEBDELIM(strReturn); continue; }
    			   if(strcmp(str, "get_fullTemp") == 0)         // Функция get_FulTemp
    			   {
    				   if(HP.sTemp[p].get_present() && HP.sTemp[p].get_Temp() != STARTTEMP)         // Если датчик есть в конфигурации то выводим значение
    				   {
    					 #ifdef SENSOR_IP
    					   _ftoa(strReturn, (float) HP.sTemp[p].get_rawTemp() / 100.0, 1); // Значение проводного датчика вывод
    					   if((HP.sTemp[p].devIP != NULL) && (HP.sTemp[p].devIP->get_fUse())
    							   && (HP.sTemp[p].devIP->get_link() > -1)) // Удаленный датчик привязан к данному проводному датчику надо использовать
    					   {
    						   strcat(strReturn, " [");
    						   _ftoa(strReturn, (float) HP.sTemp[p].get_Temp() / 100.0, 1);
    						   strcat(strReturn, "]");
    					   }
    					 #else
    					   if(HP.sTemp[p].get_lastTemp() == STARTTEMP) strcat(strReturn, "-.-");
    					   else _ftoa(strReturn, (float) HP.sTemp[p].get_Temp() / 100.0, 1);
    					 #endif
    				   } else strcat(strReturn, "-");             // Датчика нет ставим прочерк
    				   ADD_WEBDELIM(strReturn);
    				   continue;
    			   }

    			   if (strcmp(str,"get_minTemp")==0)           // Функция get_minTemp
    			   { if (HP.sTemp[p].get_present()) // Если датчик есть в конфигурации то выводим значение
    				   _ftoa(strReturn,(float)HP.sTemp[p].get_minTemp()/100.0,1);
    			   else strcat(strReturn,"-");              // Датчика нет ставим прочерк
    			   ADD_WEBDELIM(strReturn); continue; }

    			   if (strcmp(str,"get_maxTemp")==0)           // Функция get_maxTemp
    			   { if (HP.sTemp[p].get_present())          // Если датчик есть в конфигурации то выводим значение
    				   _ftoa(strReturn,(float)HP.sTemp[p].get_maxTemp()/100.0,1);
    			   else strcat(strReturn,"-");             // Датчика нет ставим прочерк
    			   ADD_WEBDELIM(strReturn); continue; }

    			   if (strcmp(str,"get_errTemp")==0)           // Функция get_errTemp
    			   { _ftoa(strReturn,(float)HP.sTemp[p].get_errTemp()/100.0,1); ADD_WEBDELIM(strReturn); continue; }

    			   if(strcmp(str, "get_aTemp") == 0)           // Функция get_addressTemp
    			   {
    				   x_get_aTemp:
					   strcat(strReturn, HP.sTemp[p].get_fAddress() ? addressToHex(HP.sTemp[p].get_address()) : "не привязан");
					   ADD_WEBDELIM(strReturn); continue;
    			   }

    			   if (strcmp(str,"get_testTemp")==0)           // Функция get_testTemp
    			   { _ftoa(strReturn,(float)HP.sTemp[p].get_testTemp()/100.0,1); ADD_WEBDELIM(strReturn); continue; }

    			   if (strcmp(str,"get_eTemp")==0)           // Функция get_errcodeTemp
    			   { _itoa(HP.sTemp[p].get_lastErr(),strReturn); ADD_WEBDELIM(strReturn); continue; }

    			   if (strcmp(str, "get_esTemp") == 0)           // Функция get_errorsTemp
    			   { _itoa(HP.sTemp[p].get_sumErrorRead(),strReturn); ADD_WEBDELIM(strReturn); continue; }

    			   if (strcmp(str,"get_pTemp")==0)           // Функция get_presentTemp
    			   {
    				   if (HP.sTemp[p].get_present()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
    				   ADD_WEBDELIM(strReturn) ;    continue;
    			   }
    			   if(strncmp(str, "get_nTemp", 9) == 0)           // Функция get_nTemp, если радиодатчик: добавляется уровень сигнала, если get_nTemp2 - +напряжение батарейки
    			   {
    				   strcat(strReturn, HP.sTemp[p].get_note());
					#ifdef RADIO_SENSORS
    				   if(HP.sTemp[p].get_fRadio()) {
    					   i = HP.sTemp[p].get_radio_received_idx(HP.sTemp[p].get_address());
    					   if(i >= 0) {
    						   if(str[9] == '2') m_snprintf(strReturn + m_strlen(strReturn), 20, ", %.1fV", (float)radio_received[i].battery / 10);
    						   m_snprintf(strReturn + m_strlen(strReturn), 20, " ᛉ%d", Radio_RSSI_to_Level(radio_received[i].RSSI));
    					   }
    				   }
					#endif
    				   ADD_WEBDELIM(strReturn); continue;
    			   }

    			   if(strcmp(str, "get_bTemp") == 0)           // Функция get_noteTemp
    			   {
    				   if(HP.sTemp[p].get_fAddress()) _itoa(HP.sTemp[p].get_bus() + 1, strReturn); else strcat(strReturn, "-");
    				   ADD_WEBDELIM(strReturn); continue;
    			   }

    			   if(strncmp(str, "get_fTemp", 9)==0){  // get_flagTempX(N): X - номер флага fTEMP_* (1..), N - имя датчика
    				   _itoa(HP.sTemp[p].get_setup_flag(str[9] - '0' - 1 + fTEMP_ignory_errors), strReturn);
    				   ADD_WEBDELIM(strReturn);  continue;
    			   }

    			   // ---- SET ----------------- Для температурных датчиков - запросы на УСТАНОВКУ парметров
    			   if (strcmp(str,"set_testTemp")==0)           // Функция set_testTemp
    			   { if (HP.sTemp[p].set_testTemp(rd(pm, 100))==OK)    // Установить значение в сотых градуса
    			   { _ftoa(strReturn,(float)HP.sTemp[p].get_testTemp()/100.0,1); ADD_WEBDELIM(strReturn);  continue;  }
    			   else { strcat(strReturn,"E05" WEBDELIM);  continue;}       // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
    			   }
    			   if (strcmp(str,"set_errTemp")==0)           // Функция set_errTemp
    			   { if (HP.sTemp[p].set_errTemp(rd(pm, 100))==OK)    // Установить значение в сотых градуса
    			   { _ftoa(strReturn,(float)HP.sTemp[p].get_errTemp()/100.0,1); ADD_WEBDELIM(strReturn); continue; }
    			   else { strcat(strReturn,"E05" WEBDELIM);  continue;}      // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
    			   }

    			   if(strncmp(str, "set_fTemp", 9) == 0) {   // set_flagTempX(N=V): X - номер флага fTEMP_* (1..), N - имя датчика
    				   i = str[9] - '0' - 1 + fTEMP_ignory_errors;
    				   HP.sTemp[p].set_setup_flag(i, int(pm));
    				   _itoa(HP.sTemp[p].get_setup_flag(i), strReturn);
    				   ADD_WEBDELIM(strReturn); continue;
    			   }

    			   /*
              if (strcmp(str,"set_targetTemp")==0)           // Функция set_targetTemp резрешены не все датчики при этом.
                 {
                  if (p==1) {HP.set_TempTargetIn(rd(pm, 100));  }
                   else if (p==5)  {HP.set_TempTargetCO(rd(pm, 100)); }
                     else if (p==6)  {HP.set_TempTargetBoil(rd(pm, 100)); }
                       else  strcat(strReturn,"E06");                 // использование имя устанавливаемого параметра «здесь» запрещено
                         ADD_WEBDELIM(strReturn);  continue;
                 }
    			    */
    			   if (strcmp(str,"set_aTemp")==0)        // Функция set_addressTemp
    			   {
    				   uint8_t n = pm;
    				   if(n <= TNUMBER)                  // Если индекс находится в диапазоне допустимых значений Здесь индекс начинается с 1, ЗНАЧЕНИЕ 0 - обнуление адреса!!
    				   {
    					   if(n == 0) HP.sTemp[p].set_address(NULL, 0);   // Сброс адреса
    					   else if(OW_scanTable) HP.sTemp[p].set_address(OW_scanTable[n-1].address, OW_scanTable[n-1].bus);
    				   }
    				   //      strcat(strReturn,int2str(pm)); ADD_WEBDELIM(strReturn); continue;}   // вернуть номер
    				   goto x_get_aTemp;
    			   }  // вернуть адрес
    			   else { strcat(strReturn,"E08");ADD_WEBDELIM(strReturn);   continue;}      // выход за диапазон допустимых номеров, значение не установлено

    		   }  // end else
    	   } //if ((strstr(str,"Temp")>0)

    	   // 2.  Датчики аналоговые, давления смещение param 30, ТОЧНОСТЬ СОТЫЕ дипазон 6
    	   if (strstr(str,"Press"))          // Проверка для запросов содержащих Press
    	   {
    		   p -= 100;                             // Убрать смещение
    		   if ((p<0)||(p>=ANUMBER))  {strcat(strReturn,"E03");ADD_WEBDELIM(strReturn);  continue; }  // Не соответсвие имени функции и параметра
    		   else  // параметр верный
    		   {
    			   if (strcmp(str,"get_Press")==0)           // Функция get_Press
    			   { if (HP.sADC[p].get_present())         // Если датчик есть в конфигурации то выводим значение
    			   {
    				   int16_t x=HP.sADC[p].get_Press();
    				   _ftoa(strReturn,(float)x/100.0,2);
					#ifdef EEV_DEF
    				   if(p < 2) {
    					   m_snprintf(strReturn + m_strlen(strReturn), 20, " [%.2f°]", (float)PressToTemp(x,HP.dEEV.get_typeFreon())/100.0);
    				   }
					#endif
    			   }
    			   else strcat(strReturn,"-");             // Датчика нет ставим прочерк
    			   ADD_WEBDELIM(strReturn); continue; }

    			   if (strcmp(str,"get_adcPress")==0)           // Функция get_adcPress
    			   { _itoa(HP.sADC[p].get_lastADC(),strReturn); ADD_WEBDELIM(strReturn); continue; }

    			   if (strcmp(str,"get_minPress")==0)           // Функция get_minPress
    			   { if (HP.sADC[p].get_present())          // Если датчик есть в конфигурации то выводим значение
    				   x_get_minPress: _ftoa(strReturn,(float)HP.sADC[p].get_minPress()/100.0,2);
    			   else strcat(strReturn,"-");              // Датчика нет ставим прочерк
    			   ADD_WEBDELIM(strReturn); continue; }

    			   if (strcmp(str,"get_maxPress")==0)           // Функция get_maxPress
    			   { if (HP.sADC[p].get_present())           // Если датчик есть в конфигурации то выводим значение
    				   x_get_maxPress: _ftoa(strReturn,(float)HP.sADC[p].get_maxPress()/100.0,2);
    			   else strcat(strReturn,"-");               // Датчика нет ставим прочерк
    			   ADD_WEBDELIM(strReturn); continue; }

    			   if (strcmp(str,"get_zeroPress")==0)           // Функция get_zeroTPress
    			   { _itoa(HP.sADC[p].get_zeroPress(),strReturn); ADD_WEBDELIM(strReturn); continue; }

    			   if (strcmp(str,"get_transPress")==0)           // Функция get_transTPress
    			   { _ftoa(strReturn,(float)HP.sADC[p].get_transADC(),3); ADD_WEBDELIM(strReturn); continue; }

    			   if (strcmp(str,"get_pinPress")==0)           // Функция get_pinPress
    			   {
#ifdef ANALOG_MODBUS
    				   if(HP.sADC[p].get_fmodbus()) {
    					   strcat(strReturn,"MB #"); _itoa(ANALOG_MODBUS_REG[p], strReturn);
    				   } else
#endif
    				   {
    					   strcat(strReturn,"AD"); _itoa(HP.sADC[p].get_pinA(),strReturn);
    				   }
    				   ADD_WEBDELIM(strReturn); continue;
    			   }

    			   if (strcmp(str,"get_testPress")==0)           // Функция get_testPress
    			   { _ftoa(strReturn,(float)HP.sADC[p].get_testPress()/100.0,2); ADD_WEBDELIM(strReturn); continue; }

    			   if (strcmp(str,"get_errcodePress")==0)           // Функция get_errcodePress
    			   { _itoa(HP.sADC[p].get_lastErr(),strReturn); ADD_WEBDELIM(strReturn); continue; }

    			   if (strcmp(str,"get_presentPress")==0)           // Функция get_presentPress
    			   {
    				   if (HP.sADC[p].get_present()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
    				   ADD_WEBDELIM(strReturn) ;    continue;
    			   }
    			   if (strcmp(str,"get_notePress")==0)           // Функция get_notePress
    			   { strcat(strReturn,HP.sADC[p].get_note()); ADD_WEBDELIM(strReturn); continue; }

    			   // ---- SET ----------------- Для аналоговых  датчиков - запросы на УСТАНОВКУ парметров

    			   if (strcmp(str,"set_testPress")==0)           // Функция set_testPress
    			   { if (HP.sADC[p].set_testPress(rd(pm, 100))==OK)    // Установить значение
    			   {_ftoa(strReturn,(float)HP.sADC[p].get_testPress()/100.0,2); ADD_WEBDELIM(strReturn); continue; }
    			   else { strcat(strReturn,"E05" WEBDELIM); continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
    			   }
    			   if(strcmp(str, "set_minPress") == 0) {
    				   if(HP.sADC[p].set_minPress(rd(pm, 100)) == OK) goto x_get_minPress;
    				   else { strcat(strReturn, "E05" WEBDELIM);  continue; }         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
    			   }
    			   if(strcmp(str, "set_maxPress") == 0) {
    				   if(HP.sADC[p].set_maxPress(rd(pm, 100)) == OK) goto x_get_maxPress;
    				   else {  strcat(strReturn, "E05" WEBDELIM); continue;  }         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
    			   }

    			   if (strcmp(str,"set_transPress")==0)           // Функция set_transPress float
    			   { if (HP.sADC[p].set_transADC(pm)==OK)    // Установить значение
    			   {_ftoa(strReturn,(float)HP.sADC[p].get_transADC(),3); ADD_WEBDELIM(strReturn); continue;}
    			   else { strcat(strReturn,"E05" WEBDELIM);  continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
    			   }

    			   if (strcmp(str,"set_zeroPress")==0)           // Функция set_zeroTPress
    			   { if (HP.sADC[p].set_zeroPress((int16_t)pm)==OK)    // Установить значение
    			   {_itoa(HP.sADC[p].get_zeroPress(),strReturn); ADD_WEBDELIM(strReturn); continue;}
    			   else { strcat(strReturn,"E05" WEBDELIM); continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
    			   }

    		   }  // end else
    	   } //if ((strstr(str,"Press")>0)


    	   //3.  Датчики сухой контакт смещение param 20
    	   if (strstr(str,"Input"))          // Проверка для запросов содержащих Input
    	   {
    		   p -= 200;
    		   if ((p<0)||(p>=INUMBER))  {strcat(strReturn,"E03");ADD_WEBDELIM(strReturn);  continue; }  // Не соответсвие имени функции и параметра
    		   else  // параметр верный
    		   {
    			   if (strcmp(str,"get_Input")==0)           // Функция get_Input
    			   {
    				   if (HP.sInput[p].get_present())          // Если датчик есть в конфигурации то выводим значение
    				   { if (HP.sInput[p].get_Input()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);}
    				   else strcat(strReturn,"-");               // Датчика нет ставим прочерк
    				   ADD_WEBDELIM(strReturn) ;    continue;
    			   }
    			   if (strcmp(str,"get_presentInput")==0)           // Функция get_presentInput
    			   {
    				   if (HP.sInput[p].get_present()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
    				   ADD_WEBDELIM(strReturn) ;    continue;
    			   }
    			   if (strcmp(str,"get_noteInput")==0)           // Функция get_noteInput
    			   { strcat(strReturn,HP.sInput[p].get_note()); ADD_WEBDELIM(strReturn); continue; }

    			   if (strcmp(str,"get_testInput")==0)           // Функция get_testInput
    			   {
    				   if (HP.sInput[p].get_testInput()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
    				   ADD_WEBDELIM(strReturn) ;    continue;
    			   }

    			   if (strcmp(str,"get_alarmInput")==0)           // Функция get_alarmInput
    			   {
    				   if (HP.sInput[p].get_alarmInput()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
    				   ADD_WEBDELIM(strReturn) ;    continue;
    			   }
    			   if (strcmp(str,"get_errcodeInput")==0)           // Функция get_errcodeInput
    			   { _itoa(HP.sInput[p].get_lastErr(),strReturn); ADD_WEBDELIM(strReturn); continue; }

    			   if (strcmp(str,"get_pinInput")==0)           // Функция get_pinInput
    			   { strcat(strReturn,"D"); _itoa(HP.sInput[p].get_pinD(),strReturn);
    			   ADD_WEBDELIM(strReturn); continue; }
    			   if (strcmp(str,"get_typeInput")==0)           // Функция get_pinInput
    			   { if (HP.sInput[p].get_present()==true)  // датчик есть в кнфигурации
    				   switch((int)HP.sInput[p].get_typeInput())
    				   {
    				   case pALARM: strcat(strReturn,"Alarm"); break;                // 0 Аварийный датчик, его срабатываение приводит к аварии и останове Тн
    				   case pSENSOR:strcat(strReturn,"Work");  break;                // 1 Обычный датчик, его значение используется в алгоритмах ТН
    				   case pPULSE: strcat(strReturn,"Pulse"); break;                // 2 Импульсный висит на прерывании и считает частоты - выходная величина ЧАСТОТА
    				   default:strcat(strReturn,"err_type"); break;                  // Ошибка??
    				   }
    			   else strcat(strReturn,"none");                                 // датчик отсутвует
    			   ADD_WEBDELIM(strReturn); continue; }
    			   if (strcmp(str,"get_noteInput")==0)           // Функция get_noteInput
    			   { strcat(strReturn,HP.sInput[p].get_note()); ADD_WEBDELIM(strReturn); continue; }


    			   // ---- SET ----------------- Для датчиков сухой контакт - запросы на УСТАНОВКУ парметров
    			   if (strcmp(str,"set_testInput")==0)           // Функция set_testInput
    			   { if (HP.sInput[p].set_testInput((int16_t)pm)==OK)    // Установить значение
    			   { if (HP.sInput[p].get_testInput()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero); ADD_WEBDELIM(strReturn); continue; }
    			   else { strcat(strReturn,"E05" WEBDELIM);  continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
    			   }

    			   if (strcmp(str,"set_alarmInput")==0)           // Функция set_alarmInput
    			   { if (HP.sInput[p].set_alarmInput((int16_t)pm)==OK)    // Установить значение
    			   { if (HP.sInput[p].get_alarmInput()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero); ADD_WEBDELIM(strReturn); continue; }
    			   else { strcat(strReturn,"E05" WEBDELIM); continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
    			   }
    		   }  // else end
    	   } //if ((strstr(str,"Input")>0)

    	   // 4 Частотные датчики ДАТЧИКИ ПОТОКА
    	   if (strstr(str,"Flow"))          // Проверка для запросов содержащих Frequency
    	   {
    		   p -= 300;
    		   if ((p<0)||(p>=FNUMBER))  {strcat(strReturn,"E03");ADD_WEBDELIM(strReturn);  continue; }  // Не соответсвие имени функции и параметра
    		   else  // параметр верный
    		   {
    			   if (strcmp(str,"get_Flow")==0)           // Функция get_Flow
    			   {
    				   if (HP.sFrequency[p].get_present())          // Если датчик есть в конфигурации то выводим значение
    					   _ftoa(strReturn,(float)HP.sFrequency[p].get_Value()/1000.0,3);
    				   else strcat(strReturn,"-");               // Датчика нет ставим прочерк
    				   ADD_WEBDELIM(strReturn) ;    continue;
    			   }
    			   if (strcmp(str,"get_frFlow")==0)           // Функция get_frFlow
    			   {
    				   if (HP.sFrequency[p].get_present())          // Если датчик есть в конфигурации то выводим значение
    					   _ftoa(strReturn,(float)HP.sFrequency[p].get_Frequency()/1000.0,3);
    				   else strcat(strReturn,"-");               // Датчика нет ставим прочерк
    				   ADD_WEBDELIM(strReturn) ;    continue;
    			   }
    			   if (strcmp(str,"get_minFlow")==0)           // Функция get_minFlow
    			   {
    				   if (HP.sFrequency[p].get_present())          // Если датчик есть в конфигурации то выводим значение
    					   _ftoa(strReturn,(float)HP.sFrequency[p].get_minValue()/1000.0,1);
    				   else strcat(strReturn,"-");               // Датчика нет ставим прочерк
    				   ADD_WEBDELIM(strReturn) ;    continue;
    			   }
    			   if (strcmp(str,"get_kfFlow")==0)           // Функция get_kfFlow
    			   {
    				   if (HP.sFrequency[p].get_present())          // Если датчик есть в конфигурации то выводим значение
    					   _ftoa(strReturn,(float)HP.sFrequency[p].get_kfValue()/100,2);
    				   else strcat(strReturn,"-");               // Датчика нет ставим прочерк
    				   ADD_WEBDELIM(strReturn) ;    continue;
    			   }
    			   if (strcmp(str,"get_capacityFlow")==0)           // Функция get_capacityFlow
    			   {
    				   if (HP.sFrequency[p].get_present())        // Если датчик есть в конфигурации то выводим значение
    					   _itoa(HP.sFrequency[p].get_Capacity(),strReturn);
    				   else strcat(strReturn,"-");               // Датчика нет ставим прочерк
    				   ADD_WEBDELIM(strReturn) ;    continue;
    			   }
    			   if (strcmp(str,"get_testFlow")==0)           // Функция get_testFlow
    			   {
    				   if (HP.sFrequency[p].get_present())          // Если датчик есть в конфигурации то выводим значение
    					   _ftoa(strReturn,(float)HP.sFrequency[p].get_testValue()/1000.0,3);
    				   else strcat(strReturn,"-");               // Датчика нет ставим прочерк
    				   ADD_WEBDELIM(strReturn) ;    continue;
    			   }
    			   if (strcmp(str,"get_pinFlow")==0)              // Функция get_pinFlow
    			   { strcat(strReturn,"D"); _itoa(HP.sFrequency[p].get_pinF(),strReturn);
    			   ADD_WEBDELIM(strReturn); continue; }
    			   if (strcmp(str,"get_errcodeFlow")==0)           // Функция get_errcodeFlow
    			   { _itoa(HP.sFrequency[p].get_lastErr(),strReturn);
    			   ADD_WEBDELIM(strReturn); continue; }
    			   if (strcmp(str,"get_noteFlow")==0)               // Функция get_noteFlow
    			   { strcat(strReturn,HP.sFrequency[p].get_note()); ADD_WEBDELIM(strReturn); continue; }

    			   if (strcmp(str,"get_checkFlow")==0)
    			   { _itoa(HP.sFrequency[p].get_checkFlow(), strReturn); ADD_WEBDELIM(strReturn); continue; }

    			   // ---- SET ----------------- Для частотных  датчиков - запросы на УСТАНОВКУ парметров
    			   if (strcmp(str,"set_minFlow")==0) {           // Функция set_testPress
    				   HP.sFrequency[p].set_minValue(pm);
    				   _ftoa(strReturn,(float)HP.sFrequency[p].get_minValue()/1000.0,1); ADD_WEBDELIM(strReturn); continue;
    			   }

    			   if (strcmp(str,"set_checkFlow")==0) {           // Функция set_testPress
    				   HP.sFrequency[p].set_checkFlow(pm != 0);
    				   _itoa(HP.sFrequency[p].get_checkFlow(), strReturn); ADD_WEBDELIM(strReturn); continue;
    			   }
    			   if (strcmp(str,"set_testFlow")==0)           // Функция set_testFlow
    			   { if (HP.sFrequency[p].set_testValue(rd(pm, 1000))==OK)    // Установить значение
    			   {_ftoa(strReturn,(float)HP.sFrequency[p].get_testValue()/1000.0,3); ADD_WEBDELIM(strReturn); continue; }
    			   else { strcat(strReturn,"E35" WEBDELIM); continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
    			   }
    			   if (strcmp(str,"set_kfFlow")==0)           // Функция set_kfFlow float
    			   { HP.sFrequency[p].set_kfValue(rd(pm, 100));    // Установить значение
    			   _ftoa(strReturn,(float)HP.sFrequency[p].get_kfValue()/100,2); ADD_WEBDELIM(strReturn); continue;
    			   }
    			   if (strcmp(str,"set_capacityFlow")==0)           // Функция set_capacityFlow float
    			   {
    				   if (HP.sFrequency[p].set_Capacity(pm)==OK)    // Установить значение
    				   {  _itoa(HP.sFrequency[p].get_Capacity(),strReturn);  ADD_WEBDELIM(strReturn); continue;}
    				   else { strcat(strReturn,"E35" WEBDELIM);  continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
    			   }
    		   }  // else end
    	   } //if ((strstr(str,"Flow")>0)

    	   //5.  РЕЛЕ смещение param 36 диапазон 14
    	   if (strstr(str,"Relay"))          // Проверка для запросов содержащих Relay
    	   {
    		   p -= 400;
    		   if ((p<0)||(p>=RNUMBER))  {strcat(strReturn,"E03");ADD_WEBDELIM(strReturn);  continue; }  // Не соответсвие имени функции и параметра
    		   else  // параметр верный
    		   {
    			   if (strcmp(str,"get_Relay")==0)           // Функция get_relay
    			   {
    				   if (HP.dRelay[p].get_Relay()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
    				   ADD_WEBDELIM(strReturn) ;    continue;
    			   }
    			   if (strcmp(str,"get_presentRelay")==0)           // Функция get_presentRelay
    			   {
    				   if (HP.dRelay[p].get_present()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
    				   ADD_WEBDELIM(strReturn) ;    continue;
    			   }
    			   if (strcmp(str,"get_noteRelay")==0)           // Функция get_noteRelay
    			   { strcat(strReturn,HP.dRelay[p].get_note()); ADD_WEBDELIM(strReturn); continue; }

    			   if (strcmp(str,"get_pinRelay")==0)           // Функция get_pinRelay
    			   { strcat(strReturn,"D"); _itoa(HP.dRelay[p].get_pinD(),strReturn); ADD_WEBDELIM(strReturn); continue; }

    			   // ---- SET ----------------- Для реле - запросы на УСТАНОВКУ парметров
    			   if (strcmp(str,"set_Relay")==0)           // Функция set_Relay
    			   { if (HP.dRelay[p].set_Relay(pm == 0 ? fR_StatusAllOff : fR_StatusManual)==OK)    // Установить значение
    			   { if (HP.dRelay[p].get_Relay()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero); ADD_WEBDELIM(strReturn); continue; }
    			   else { strcat(strReturn,"E05" WEBDELIM);  continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
    			   }
    		   }  // else end
    	   } //if ((strstr(str,"Relay")>0)  5
       } // Массивы датчиков
       
        // ------------------------ конец разбора -------------------------------------------------
x_FunctionNotFound:    
       strcat(strReturn,"E01");                             // функция не найдена ошибка
       ADD_WEBDELIM(strReturn) ;
       continue;
       } // 2. Функции с параметром
       
  if (str[0]=='&') {break; } // второй символ & подряд признак конца запроса и мы выходим
  strcat(strReturn,"E01");   // Ошибка нет такой команды
  ADD_WEBDELIM(strReturn) ;
  }
  ADD_WEBDELIM(strReturn) ; // двойной знак закрытие посылки
}

// ===============================================================================================================
const char *header_Authorization_="Authorization: Basic ";
const char *header_POST_="Access-Control-Request-Method: POST";
// Выделение имени файла (или содержания запроса) и типа файла и типа запроса клиента
// thread - номер потока, возсращает тип запроса
uint16_t GetRequestedHttpResource(uint8_t thread)
{
	char *str_token, *pass;
	boolean user, admin;
	uint8_t i;
	uint16_t len;

//	journal.jprintf(">%s\n",Socket[thread].inBuf);
//	journal.jprintf("--------------------------------------------------------\n");
	

	if((HP.get_fPass()) && (!HP.safeNetwork))  // идентификация если установлен флаг и перемычка не в нуле
	{
		if(!(pass = strstr((char*) Socket[thread].inBuf, header_Authorization_))) return UNAUTHORIZED; // строка авторизации не найдена
		else  // Строка авторизации найдена смотрим логин пароль
		{
			pass = pass + strlen(header_Authorization_);
			user = true;
			for(i = 0; i < HP.Security.hashUserLen; i++)
				if(pass[i] != HP.Security.hashUser[i]) {
					user = false;
					break;
				}
			if(user != true) // это не пользователь
			{
				admin = true;
				for(i = 0; i < HP.Security.hashAdminLen; i++)
					if(pass[i] != HP.Security.hashAdmin[i]) {
						admin = false;
						break;
					}
				if(admin != true) return BAD_LOGIN_PASS; // Не верный логин или пароль
			} //  if (user!=true)
			else SETBIT1(Socket[thread].flags, fUser); // зашел простой пользователь
		} // else
	}

	// Идентификация пройдена
	//if(strstr((char*)Socket[thread].inBuf,"Access-Control-Request-Method: POST")) {request_type = HTTP_POST_; return request_type; }  //обработка предваритаельного запроса перед получением файла
	str_token = strtok((char*) Socket[thread].inBuf, " ");    // Обрезаем по пробелам
	if(strcmp(str_token, "GET") == 0)   // Ищем GET
	{
		str_token = strtok(NULL, " ");                       // get the file name
		if(strcmp(str_token, "/") == 0)                   // Имени файла нет, берем файл по умолчанию
		{
			Socket[thread].inPtr = (char*) INDEX_FILE;      // Указатель на имя файла по умолчанию
			return HTTP_GET;
		} else if(strcmp(str_token, (char*) MOB_PATH) == 0) // Имени файла нет, но указан путь до мобильной морды
		{
			Socket[thread].inPtr = (char*) (str_token + 1);   // Указатель на путь до мобильной морды
			strcat(Socket[thread].inPtr, (char*) INDEX_MOB_FILE);
			return HTTP_GET;
		} else if((len = strlen(str_token)) <= W5200_MAX_LEN - 100)   // Проверка на длину запроса или имени файла
		{
			Socket[thread].inPtr = (char*) (str_token + 1);       // Указатель на имя файла
			//        Serial.println(Socket[thread].inPtr=(char*)(str_token+1));
			if(Socket[thread].inPtr[0] == '&') return HTTP_REQEST;       // Проверка на аякс запрос
			return HTTP_GET;
		} // if ((len=strlen(str_token)) <= W5200_MAX_LEN-100)
		else {
			#ifdef DEBUG
			journal.jprintf("WEB:Error GET request, len=%d: %s\n", len, Socket[thread].inBuf);
			#endif
			return HTTP_invalid;  // слишком длинная строка HTTP_invalid
		}
	}   //if (strcmp(str_token, "GET") == 0)
	else if(strcmp(str_token, "POST") == 0) {Socket[thread].inPtr = (char*) (str_token +strlen("POST") + 1);  return HTTP_POST;}    // Запрос POST Socket[thread].inPtr - указывает на начало запроса (начало полезных данных)
	else if(strcmp(str_token, "OPTIONS") == 0) return HTTP_POST_;
	#ifdef DEBUG
	journal.jprintf("WEB:Error request %s\n", Socket[thread].inBuf);
	#endif
	return HTTP_invalid;
}

// ========================== P A R S E R  P O S T =================================
const char Title[]          = {"Title: "};           // где лежит имя файла
const char Length[]         = {"Content-Length: "};  // где лежит длина файла 
const char emptyStr[]       = {"\r\n\r\n"};          // пустая строка после которой начинаются данные
const char SETTINGS[]       = {"*SETTINGS*"};        // Идентификатор передачи настроек (лежит в Title:)
const char LOAD_START[]     = {"*SPI_FLASH*"};       // Идентификатор начала загрузки веб морды (лежит в Title:)
const char LOAD_END[]       = {"*SPI_FLASH_END*"};   // Идентификатор колнца загрузки веб морды (лежит в Title:)

uint16_t numFilesWeb=0;                   // Число загруженных файлов
boolean  settingNow=false;                // признак начала загрузки настроек
// Разбор и обработка POST запроса inPtr входная строка использует outBuf для хранения файла настроек!
// Сейчас реализована загрузка настроек и загрузка веб морды в спи диск
// Возврат тип ответа (потом берется из массива строк)
// char nameFile[128]; // имя файла
TYPE_RET_POST parserPOST(uint8_t thread, uint16_t size)
{
	byte *ptr,*pStart;
	char nameFile[128]; // имя файла
	char sizeFile[8];   // длина файла
	uint8_t  i=0;
	int32_t len, full_len=0, lenFile;
	//journal.jprintf("POST >%s\n",Socket[thread].inPtr);

 	// Определение имени файла
	if((pStart=(byte*)strstr((char*)Socket[thread].inPtr,Title)) == 0) {journal.jprintf("%s: Name file not found.\n",(char*)__FUNCTION__);return pLOAD_ERR;} // Имя файла не найдено, запрос не верен, выходим
    pStart=pStart+strlen((char*)Title);  // начало имени файла
    for(i=0;i<sizeof(nameFile);i++)      // копирование имени файла
    	if (*(pStart+i)!=13) nameFile[i]=*(pStart+i); else {nameFile[i]=0;break;} 
    if (strlen(nameFile)>=sizeof(nameFile)-1){ // проверка длины
    	nameFile[sizeof(nameFile)-1]=0;      // обрезать
 		journal.jprintf("%s: File name %s big size.\n",(char*)__FUNCTION__,nameFile); return pLOAD_ERR; }  	    
    urldecode(nameFile, nameFile, sizeof(nameFile));
 
    // Определение длины файла
	if((pStart=(byte*)strstr((char*)Socket[thread].inPtr, Length)) == 0) {journal.jprintf("%s: Size file %s not found.\n",(char*)__FUNCTION__,nameFile);return pLOAD_ERR;} // Размер файла не найден, запрос не верен, выходим
    pStart=pStart+strlen((char*)Length);  // начало размера файла
    for(i=0;i<sizeof(sizeFile);i++)    // копирование длины файла
       {
        if   (*(pStart+i)==13) {sizeFile[i]=0;break;} // перенос строки - конец длины
       	if   ((*(pStart+i)<'0')||(*(pStart+i)>'9')) { journal.jprintf("%s: Wrong size file %s.\n",(char*)__FUNCTION__,nameFile); return pLOAD_ERR; } // только цифры
    	sizeFile[i]=*(pStart+i);// копирование
       }
    if (strlen(sizeFile)>=sizeof(sizeFile)-1){ journal.jprintf("%s: Size file %s big.\n",(char*)__FUNCTION__,nameFile); return pLOAD_ERR; } // проверка длины
    lenFile=atoi(sizeFile);	
    if ((lenFile==0)&&((strcmp(nameFile,LOAD_START)!=0)&&(strcmp(nameFile,LOAD_END)!=0))) {journal.jprintf("%s: Size file %s zero.\n",(char*)__FUNCTION__,nameFile); return pLOAD_ERR; } 

   // journal.jprintf("POST: file %s size %d bytes\n",nameFile,lenFile);  return pNULL; // тестирование заголовков
   
    // все нашлось, можно обрабатывать
 //  if (lenFile>0) journal.jprintf("POST: file %s size %d bytes\n",nameFile,lenFile);  // Все получилось, начало и конец загрузки не выводим
    ptr = (byte*) strstr((char*) Socket[thread].inPtr,emptyStr)+strlen(emptyStr); // поиск начала даных
    full_len=size-(ptr - (byte *)Socket[thread].inBuf); // длина данных (файла) в буфере
    
       
    // В зависимости от имени файла (Title)
    if (strcmp(nameFile,SETTINGS)==0){  // Чтение настроек
			// Определение начала данных (поиск HEADER_BIN)
		    if((strstr((char*) ptr,HEADER_BIN)) == NULL) {  // Заголовок не найден
				journal.jprintf("%s: Wrong save file %s format.\n",(char*)__FUNCTION__,nameFile);
				return pSETTINGS_ERR;
			}
			full_len=size-(ptr - (byte *)Socket[thread].inBuf); // определяем размер данных в пакете
	    	memcpy(Socket[thread].outBuf,ptr,full_len);         // копируем начало данных в буфер

	        while (full_len<lenFile)  // Чтение остальных данных по сети
	        {
			_delay(10);
		    len=Socket[thread].client.get_ReceivedSizeRX();                            // получить длину входного пакета
		    if(len>W5200_MAX_LEN-1) len=W5200_MAX_LEN-1;                               // Ограничить размером в максимальный размер пакета w5200
		    Socket[thread].client.read(Socket[thread].inBuf,len);                      // прочитать буфер
		    if (full_len+len>=(int32_t)sizeof(Socket[thread].outBuf)) return pSETTINGS_MEM;     // проверить длину если не влезает то выходим
			memcpy(Socket[thread].outBuf+full_len,Socket[thread].inBuf,len);           // Добавить пакет в буфер
			full_len=full_len+len;                                                     // определить размер данных
	        }
			ptr =(byte*)Socket[thread].outBuf+m_strlen(HEADER_BIN);                     // отрезать заголовок в данных

			journal.jprintf("Loading %s length  %d bytes:\n",SETTINGS, full_len);

			// Чтение настроек из ptr
		    len = HP.load(ptr, 1);
			if(len <= 0) return pSETTINGS_ERR; // ошибка загрузки настроек
			boolean ret = true;
			// Чтение профиля
			ptr += len;
			if(HP.Prof.loadFromBuf(0, ptr)!= OK) ret = false;   // чтение профиля
			if(HP.Schdlr.loadFromBuf(ptr + HP.Prof.get_lenProfile()) != OK) ret = false;  // чтения расписания
			HP.Prof.update_list(HP.Prof.get_idProfile());             // обновить список
			if (ret) return pSETTINGS_OK; else return pSETTINGS_ERR;
    } //if (strcmp(nameFile,"*SETTINGS*")==0)
    
    // загрузка вебморды
   else  if (strcmp(nameFile,LOAD_START)==0){  // начало загрузки вебморды
   if (SemaphoreTake(xLoadingWebSemaphore,10)!=pdPASS) {journal.jprintf("%s: Download already in progress.\n",(char*)__FUNCTION__);SemaphoreGive(xLoadingWebSemaphore);return pLOAD_ERR;} // Cемафор не был захвачен,?????? очень странно
   numFilesWeb=0;
   journal.jprintf("POST: %s start download, format SPI disk ",nameFile);
   SerialFlash.eraseAll();
	    while (SerialFlash.ready() == false) {
	      vTaskDelay(1000/ portTICK_PERIOD_MS);    
	      journal.jprintf(".");
	    }
	    journal.jprintf(" OK\n");	
	    return pNULL;
   }
    else  if (strcmp(nameFile,LOAD_END)==0){  // Окончание загрузки вебморды
    if (SemaphoreTake(xLoadingWebSemaphore,0)!=pdPASS) {journal.jprintf("POST: %s end of download, total %d files downloads.\n",nameFile,numFilesWeb); SemaphoreGive(xLoadingWebSemaphore);return pLOAD_OK;} // Семафор не захвачен (был захвачен ранее) все ок
  	else {journal.jprintf("%s: Trying to end download without starting the download.\n",(char*)__FUNCTION__);SemaphoreGive(xLoadingWebSemaphore); return pLOAD_ERR;}	// семафор БЫЛ не захвачен, ошибка, отдать обратно
    }
   else { // загрузка отдельных файлов веб морды
    if (SemaphoreTake(xLoadingWebSemaphore,0)!=pdPASS) {if (loadFileToSpi(nameFile, lenFile, thread, ptr,full_len)){numFilesWeb++; return pNULL;} else return pLOAD_ERR; }// Cемафор  захвачен загрузка файла
   	else {journal.jprintf("%s: Trying to download file without starting the download.\n",(char*)__FUNCTION__);SemaphoreGive(xLoadingWebSemaphore);return pLOAD_ERR;}	// семафор БЫЛ не захвачен, ошибка, отдать обратно
    }

   return pPOST_ERR; // До сюда добегать не должны
}
// Загрузка файла в спай память возвращает число записанных байт на диск 0 если ошибка
// Файл может лежать во множестве пакетов. Считается что spi диск отформатирован и ожидает запись файлов с "нуля"
// Входные параметры:
// nameFile - имя файла 
// lenFile - общая длина файла
// thread - поток веб сервера,котрый обрабатывает post запрос
// ptr - указатель на начало данных (файла) в буфере Socket[thread].inPtr. 
// sizeBuf - размер данных в буфере ptr (по сети осталось принять lenFile-sizeBuf)
uint32_t loadFileToSpi(char * nameFile, uint32_t lenFile, uint8_t thread, byte* ptr, uint16_t sizeBuf) 
{
uint16_t len,numPoint=0;       
uint32_t loadLen=0; // Обработанная (загруженная) длина

if (SerialFlash.create(nameFile,lenFile)) 
  {   SerialFlashFile ff = SerialFlash.open(nameFile);
      if (ff) {
        journal.jprintf("File %s load ",nameFile); 
        loadLen=ff.write(ptr, sizeBuf);  // первый пакет упаковали
        while (loadLen<lenFile)  // Чтене остальных пакетов из сети
        {
        len=Socket[thread].client.get_ReceivedSizeRX();                            // получить длину входного пакета
        if(len>W5200_MAX_LEN-1) len=W5200_MAX_LEN-1;                               // Ограничить размером в максимальный размер пакета w5200
        Socket[thread].client.read(Socket[thread].inBuf,len);                      // прочитать буфер	
        loadLen=loadLen+ff.write(Socket[thread].inBuf,len);                        // записать
  //      journal.jprintf(".");
        numPoint++;
        if (numPoint>=10) {numPoint=0;journal.jprintf(".");}                       // точка на 15 кб приема (10 пакетов по 1540)
        }
       ff.close();  
      if (loadLen==lenFile)  journal.jprintf(" %d bytes OK\n",loadLen); 
      else {journal.jprintf(" %d bytes, error length\n",loadLen);loadLen=0;}        // Длины не совпали   
      }
      else journal.jprintf(" %s: error open file %s\n",(char*)__FUNCTION__,nameFile);
  } // if (SerialFlash.create(filename, length))   
else journal.jprintf(" %s: error create file %s\n",(char*)__FUNCTION__,nameFile);
return 	loadLen;
}



 

