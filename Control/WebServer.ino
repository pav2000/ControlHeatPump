/* 
 * Copyright (c) 2016-2019 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
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
extern void  noCsvStatistic(uint8_t thread);


// Названия режимов теста
const char *noteTestMode[] =   {"NORMAL","SAFE_TEST","TEST","HARD_TEST"};
// Описание режима теста
static const char *noteRemarkTest[] = {"Тестирование отключено, основной режим работы.",
                                       "Значения датчиков берутся из полей 'Тест', работа исполнительных устройств эмулируется - Безопасно.",
                                       "Значения датчиков берутся из полей 'Тест', исполнительные устройства работают за исключением компрессора (FC и RCOMP) - Почти безопасно.",
                                       "Значения датчиков берутся из полей 'Тест', все исполнительные устройства работают. Внимание! Может быть поврежден компрессор!"};
                               
                               
const char* file_types[] = {"text/html", "image/x-icon", "text/css", "application/javascript", "image/jpeg", "image/png", "image/gif", "text/plain", "text/ajax"};

const char header_Authorization_1[] = "Authorization: Basic ";
const char header_Authorization_2[] = "&&!Z";
const char* pageUnauthorized      = {"HTTP/1.0 401 Unauthorized\r\nWWW-Authenticate: Basic real_m=Admin Zone\r\nContent-Type: text/html\r\nAccess-Control-Allow-Origin: *\r\n\r\n"};
const char* NO_SUPPORT            = {"not supported"};
const char* NO_STAT               = {"Statistics are not supported in the firmware"};

const char *postRet[]            = {"Настройки из выбранного файла восстановлены, CRC16 OK\r\n\r\n",                           //  Ответы на пост запросы
									"Ошибка восстановления настроек из файла (см. журнал)\r\n\r\n",
									"", // пустая строка - ответ не выводится
									"Файлы загружены, подробности в журнале\r\n\r\n",
									"Ошибка загрузки файла, подробности в журнале\r\n\r\n",
									"Внутренняя ошибка парсера post запросов\r\n\r\n",
									"Флеш диск не найден, загружать файлы некуда\r\n\r\n",
									"Файл настроек для разбора не влезает во внутренний буфер (max 6144 bytes)\r\n\r\n"
									};

static SdFile wFile;
static fname_t wfname;

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

#if    W5200_THREAD < 2
		if (Socket[0].sock==sock) continue;   // исключение повторного захвата сокетов
#elif  W5200_THREAD < 3
		if((Socket[0].sock==sock)||(Socket[1].sock==sock)) continue; // исключение повторного захвата сокетов
#elif  W5200_THREAD < 4
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
				STORE_DEBUG_INFO(10);
				if(Socket[thread].client.available()) {
					len = Socket[thread].client.get_ReceivedSizeRX();                  // получить длину входного пакета
					if(len > W5200_MAX_LEN) {
						journal.jprintf("WEB:Big packet: %d - truncated!\n", len);
#ifdef DEBUG
						journal.jprintf("%s\n\n", Socket[thread].inBuf);
#endif
						len = W5200_MAX_LEN; // Ограничить размером в максимальный размер пакета w5200
					}
					if(Socket[thread].client.read(Socket[thread].inBuf, len) != len) {
						journal.jprintf("WEB:Read error\n");
						break;
					}
					// Ищем в запросе полезную информацию (имя файла или запрос ajax)
					// пройти авторизацию и разобрать заголовок -  получить имя файла, тип, тип запроса, и признак меню пользователя
					Socket[thread].http_req_type = GetRequestedHttpResource(thread);
					if(Socket[thread].http_req_type != HTTP_POST || len < W5200_MAX_LEN) {
						Socket[thread].inBuf[len + 1] = 0;              // обрезать строку
					}
#ifdef LOG
					journal.jprintf("$QUERY: %s\n",Socket[thread].inPtr);
					journal.jprintf("$INPUT: %s\n",(char*)Socket[thread].inBuf);
#endif
					switch(Socket[thread].http_req_type)  // По типу запроса
					{
					case HTTP_invalid: {
#ifdef DEBUG
						if(HP.get_NetworkFlags() & ((1<<fWebLogError) | (1<<fWebFullLog))) {
							uint8_t ip[4];
							W5100.readSnDIPR(sock, ip);
							journal.jprintf("WEB:Wrong request %d.%d.%d.%d (%d): ", ip[0], ip[1], ip[2], ip[3], len);
							if(HP.get_NetworkFlags() & (1<<fWebFullLog)) {
								for(int8_t i = 0; i < len; i++) journal.jprintf("%c(%d) ", (char)Socket[thread].inBuf[i], Socket[thread].inBuf[i]);
								journal.jprintf("\n");
							} else {
								for(len = 0; len < 4; len++) journal.jprintf("%d ", Socket[thread].inBuf[len]);
								journal.jprintf("...\n");
							}
						}
#endif
						//sendConstRTOS(thread, "HTTP/1.1 Error GET request\r\n\r\n");
						break;
					}
					case HTTP_GET:     // чтение файла
					{
						// Для обычного пользователя подменить файл меню, для сокращения функционала
						if(GETBIT(Socket[thread].flags, fUser)) {
							if(strcmp(Socket[thread].inPtr, "menu.js") == 0) strcpy(Socket[thread].inPtr, "menu-user.js");
							else if(strstr(Socket[thread].inPtr, ".html")) {
								if(!(strcmp(Socket[thread].inPtr, "index.html") == 0
									|| strcmp(Socket[thread].inPtr, "plan.html") == 0
									|| strcmp(Socket[thread].inPtr, "about.html") == 0)) goto xUNAUTHORIZED;
							}
						}
						urldecode(Socket[thread].inPtr, Socket[thread].inPtr, len + 1);
						readFileSD(Socket[thread].inPtr, thread);
						break;
					}
					case HTTP_REQEST:  // Запрос AJAX
					{
						strcpy(Socket[thread].outBuf, HEADER_ANSWER);   // Начало ответа
						parserGET(thread, sock);    // выполнение запроса
#ifdef LOG
						journal.jprintf("$RETURN: %s\n",Socket[thread].outBuf);
#endif
						if(sendBufferRTOS(thread, (byte*) (Socket[thread].outBuf), strlen(Socket[thread].outBuf)) == 0) {
							if(HP.get_NetworkFlags() & (1<<fWebLogError)) {
								uint8_t ip[4];
								W5100.readSnDIPR(sock, ip);
								journal.jprintf("$Error send AJAX(%d.%d.%d.%d): %s\n", ip[0], ip[1], ip[2], ip[3], (char*) Socket[thread].inBuf);
							}
						}
						break;
					}
					case HTTP_POST:    // загрузка настроек
					{
                        uint8_t ret= parserPOST(thread, len);         // разобрать и получить тип ответа
                        strcpy(Socket[thread].outBuf, HEADER_ANSWER);
						strcat(Socket[thread].outBuf, postRet[ret]);   // вернуть текстовый ответ, который надо вывести
               			if(sendBufferRTOS(thread, (byte*) (Socket[thread].outBuf), strlen(Socket[thread].outBuf)) == 0) {
							if(HP.get_NetworkFlags() & (1<<fWebLogError)) {
								uint8_t ip[4];
								W5100.readSnDIPR(sock, ip);
								journal.jprintf("$Error send POST(%d.%d.%d.%d): %s\n", ip[0], ip[1], ip[2], ip[3], (char*) Socket[thread].inBuf);
							}
               			}
						break;
					}
					case HTTP_POST_: // предварительный запрос post
					{
						sendConstRTOS(thread,
								"HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: HEAD, OPTIONS, GET, POST\r\nAccess-Control-Allow-Headers: Overwrite, Content-Type, Cache-Control, Title\r\n\r\n");
						break;
					}
					case UNAUTHORIZED: {
xUNAUTHORIZED:
						if(HP.get_NetworkFlags() & (1<<fWebLogError)) {
							uint8_t ip[4];
							W5100.readSnDIPR(sock, ip);
							journal.jprintf("$UNAUTHORIZED (%d.%d.%d.%d)\n", ip[0], ip[1], ip[2], ip[3]);
						}
						sendConstRTOS(thread, pageUnauthorized);
						break;
					}
					case BAD_LOGIN_PASS: {
						if(HP.get_NetworkFlags() & (1<<fWebLogError)) {
							uint8_t ip[4];
							W5100.readSnDIPR(sock, ip);
							journal.jprintf("$Wrong login or password (%d.%d.%d.%d)\n", ip[0], ip[1], ip[2], ip[3]);
						}
						sendConstRTOS(thread, pageUnauthorized);
						break;
					}

					default:
						journal.jprintf("$Unknow  %s\n", (char*) Socket[thread].inBuf);
					}

					SPI_switchW5200();
					Socket[thread].inBuf[0] = 0;
					break;   // Подготовить к следующей итерации
				} // end if (client.available())
			} // end while (client.connected())
	//		taskYIELD();
			Socket[thread].client.stop();   // close the connection
			Socket[thread].sock = -1;
		//	vTaskDelay(TIME_WEB_SERVER / portTICK_PERIOD_MS); // задержка чтения уменьшаем загрузку процессора
			taskYIELD();
		} // end if (client)
#ifdef FAST_LIB  // Переделка
	}  // for (int sock = 0; sock < W5200_SOCK_SYS; sock++)
#endif
	SemaphoreGive (xWebThreadSemaphore);              // Семафор отдать
}

const char filename_subst_scheme[] = "HPscheme";

//  Чтение файла с SD или его генерация
void readFileSD(char *filename, uint8_t thread)
{
	int n;
	SdFile webFile;
	//  journal.jprintf("$Thread: %d socket: %d read file: %s\n",thread,Socket[thread].sock,filename);
	// Реализация функционала подмены для имен файлов по типу: plan[HPscheme].png -> plan2.png
	char *str;
	STORE_DEBUG_INFO(11);
	if((str = strchr(filename, '[')) != NULL) // скобка найдена надо обрабатывать
	{
		if(strncmp(str + 1, filename_subst_scheme, sizeof(filename_subst_scheme)-1) == 0) // найден аргумент (схема ТН) надо подменять на значение HP_SHEME
		{
			if(*(str + 1 + sizeof(filename_subst_scheme)-1) == ']') {
				itoa(HP_SCHEME, str, 10); // вставить номер схемы
				strcat(filename, str + 1 + sizeof(filename_subst_scheme)-1 + 1);
			} else journal.jprintf("Not found ] in: %s", filename); // нет закрывающейся скобки
		}
	}

	// В начале обрабатываем генерируемые файлы (для выгрузки из контроллера)
	if(strcmp(filename, "state.txt") == 0) { get_txtState(thread, true); return; }
	if(strncmp(filename, "settings", 8) == 0) {
		filename += 8;
		if(strcmp(filename, ".txt") == 0) {	get_txtSettings(thread); return; }
		else if(strcmp(filename, ".bin") == 0) { get_binSettings(thread); return; }
		filename -= 8;
	}
	if(strcmp(filename, "chart.csv") == 0) { get_csvChart(thread); return; }
	if(strcmp(filename, "journal.txt") == 0) { get_txtJournal(thread); return; }
	if(strncmp(filename, stats_file_start, sizeof(stats_file_start)-1) == 0) { // Файл статистики, stats_yyyy.dat, stats_yyyy.csv
		STORE_DEBUG_INFO(12);
	    strcpy(Socket[thread].outBuf, WEB_HEADER_OK_CT);
	    strcat(Socket[thread].outBuf, WEB_HEADER_BIN_ATTACH);
	    strcat(Socket[thread].outBuf, filename);
	    strcat(Socket[thread].outBuf, "\"");
	    strcat(Socket[thread].outBuf, WEB_HEADER_END);
		n = strncmp(filename + sizeof(stats_file_start)-1, stats_file_header, sizeof(stats_file_header)-1) == 0;
		if((str = strstr(filename, stats_csv_file_ext)) != NULL) { // формат csv - нужен заголовок
			Stats.StatsFileHeader(Socket[thread].outBuf, n);
			strcpy(str, stats_file_ext);
		}
		if(!n) {
			Stats.SendFileData(thread, &webFile, filename);
		} else {
			sendPacketRTOS(thread, (byte*)Socket[thread].outBuf, strlen(Socket[thread].outBuf), 0);
		}
		return;
	}
	if(strncmp(filename, history_file_start, sizeof(history_file_start)-1) == 0) { // Файл Истории полностью: только для бакапа - hist_yyyy.dat, hist_yyyy.csv, обрезанный по периоду - hist__yyyymmdd-yyyymmdd
		STORE_DEBUG_INFO(13);
	    strcpy(Socket[thread].outBuf, WEB_HEADER_OK_CT);
	    strcat(Socket[thread].outBuf, WEB_HEADER_BIN_ATTACH);
	    strcat(Socket[thread].outBuf, filename);
	    strcat(Socket[thread].outBuf, "\"");
	    strcat(Socket[thread].outBuf, WEB_HEADER_END);
	    str = strchr(filename, '-');
	    if(str) { // Period format: "hist__yyyymmdd-yyyymmdd"
	    	filename[sizeof(history_file_start)-1] = '\0';
	    	*str = '\0';
	    	Stats.SendFileDataByPeriod(thread, &webFile, filename, filename + sizeof(history_file_start), str + 1);
	    } else {
			n = strncmp(filename + sizeof(history_file_start)-1, stats_file_header, sizeof(stats_file_header)-1) == 0;
			if((str = strstr(filename, stats_csv_file_ext)) != NULL) { // формат csv - нужен заголовок
				Stats.HistoryFileHeader(Socket[thread].outBuf, n);
				strcpy(str, stats_file_ext);
			}
			if(!n) {
				Stats.SendFileData(thread, &webFile, filename);
			} else {
				sendPacketRTOS(thread, (byte*)Socket[thread].outBuf, strlen(Socket[thread].outBuf), 0);
			}
	    }
		return;
	}
	if(strncmp(filename, "TEST.", 5) == 0) {
		STORE_DEBUG_INFO(14);
		filename += 5;
		if(strcmp(filename, "DAT") == 0) { // TEST.DAT
			get_datTest(thread);
		} else if(strcmp(filename, "NOSD") == 0) { // TEST.NOSD
			get_indexNoSD(thread); // минимальная морда
		} else if(strncmp(filename, "SD:", 3) == 0) {  // TEST.SD:<filename> - Тестирует скорость чтения файла с SD карты
			filename += 3;
			journal.jprintf("SD card test: %s - ", filename);
			sendConstRTOS(thread, HEADER_FILE_WEB);
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
				webFile.close();
				journal.jprintf("read %u bytes, %u b/sec\n", size, (uint64_t)size * 1000 / startTick);
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
				journal.jprintf("not found (%d,%d)!\n", card.cardErrorCode(), card.cardErrorData());
			}
		}
		return;
	}
#ifdef I2C_EEPROM_64KB
//	if (strcmp(filename,"statistic.csv")==0) {get_csvStatistic(thread); return;}
#endif

// загрузка файла -----------
	// Разбираемся откуда грузить надо (три варианта)
	switch(HP.get_SourceWeb()) {
	case pMIN_WEB: // минимальная морда
		get_indexNoSD(thread);
		break;
	case pSD_WEB:
		{ // Чтение с карты  файлов
			STORE_DEBUG_INFO(15);
			SPI_switchSD();
			if(!webFile.open(filename, O_READ))
			{
				if(card.cardErrorCode() > SD_CARD_ERROR_NONE && card.cardErrorCode() < SD_CARD_ERROR_READ
						&& card.cardErrorData() == 255) { // reinit card
					if(card.begin(PIN_SPI_CS_SD, SD_SCK_MHZ(SD_CLOCK))) {
						if(webFile.open(filename, O_READ)) goto xFileFound;
					} else {
						journal.jprintf("Reinit SD card failed!\n");
						//HP.set_fSD(false);
					}
				}
				sendConstRTOS(thread, HEADER_FILE_NOT_FOUND);
				uint8_t ip[4];
				W5100.readSnDIPR(Socket[thread].sock, ip);
				journal.jprintf((char*) "WEB: %d.%d.%d.%d - File not found: %s\n", ip[0], ip[1], ip[2], ip[3], filename);
				return;
			} // файл не найден
xFileFound:
			// Файл открыт читаем данные и кидаем в сеть
	#ifdef LOG
			journal.jprintf("$Thread: %d socket: %d read file: %s\n",thread,Socket[thread].sock,filename);
	#endif
			if(strstr(filename, ".css") != NULL) sendConstRTOS(thread, HEADER_FILE_CSS); // разные заголовки
			else sendConstRTOS(thread, HEADER_FILE_WEB);
			SPI_switchSD();
			while((n = webFile.read(Socket[thread].outBuf, sizeof(Socket[thread].outBuf))) > 0) {
				if(sendBufferRTOS(thread, (byte*) (Socket[thread].outBuf), n) == 0) break;
				SPI_switchSD();
			} // while
			if(n < 0) journal.jprintf("Read SD error (%d,%d)!\n", card.cardErrorCode(), card.cardErrorData());
			SPI_switchSD();
			webFile.close();
		}
		break;
	case pFLASH_WEB:
		{
			STORE_DEBUG_INFO(16);
			SerialFlashFile ff = SerialFlash.open(filename);
			if(ff) {
	#ifdef LOG
				journal.jprintf("$Thread: %d socket: %d read file: %s\n",thread,Socket[thread].sock,filename);
	#endif
				if(strstr(filename, ".css") != NULL) sendConstRTOS(thread, HEADER_FILE_CSS); // разные заголовки
				else sendConstRTOS(thread, HEADER_FILE_WEB);
				//	SPI_switchSD();
				while((n = ff.read(Socket[thread].outBuf, sizeof(Socket[thread].outBuf))) > 0) {
					//SPI_switchW5200();
					if(sendBufferRTOS(thread, (byte*) (Socket[thread].outBuf), n) == 0) break;
					//	SPI_switchSD();
				} // while
				//if(n < 0) journal.jprintf("Read SD error (%d,%d)!\n", card.cardErrorCode(), card.cardErrorData());
				//SPI_switchSD();
				//webFile.close();
			} else {
				sendConstRTOS(thread, HEADER_FILE_NOT_FOUND);
				uint8_t ip[4];
				W5100.readSnDIPR(Socket[thread].sock, ip);
				journal.jprintf((char*) "WEB GET(%d.%d.%d.%d) - Not found: %s\n", ip[0], ip[1], ip[2], ip[3], filename);
				return;
			}
		}
		break;
	default:
		get_indexNoSD(thread);
		break;
	}
}

// ========================== P A R S E R  G E T =================================
// Разбор и обработка строк запросов buf (начало &) входная строка, strReturn = Socket[0].outBuf выходная
// ТОЛЬКО запросы!
// возвращает число обработанных одиночных запросов
#ifdef SENSOR_IP
void parserGET(uint8_t thread, int8_t sock)
{
#else
void parserGET(uint8_t thread, int8_t )
{
#endif
	char *str,*x,*y,*z;
	float pm=0;
	int16_t i;
	int32_t l_i32;
#ifdef SENSOR_IP                           // Получение данных удаленного датчика
	// переменные для удаленных датчиков
	char *ptr;
	int16_t a,b,c,d;
#endif

	char *buf = Socket[thread].inPtr;
	char *strReturn = Socket[thread].outBuf;
	ADD_WEBDELIM(strReturn);   // начало запроса
	str = strstr(buf,"&&");
	if(str) str[1] = 0;   // Обрезать строку после комбинации &&
	urldecode(buf, buf, W5200_MAX_LEN);
	while ((str = strtok_r(buf, "&", &buf)) != NULL) // разбор отдельных комманд
	{
		STORE_DEBUG_INFO(20);
		strReturn += strlen(strReturn);
		if((strpbrk(str,"="))==0) // Повторить тело запроса и добавить "=" ДЛЯ НЕ set_ запросов
		{
			strcat(strReturn,str);
			*(strReturn += strlen(strReturn)) = '=';
			*(++strReturn) = 0;
		}
		if(strReturn > Socket[thread].outBuf + sizeof(Socket[thread].outBuf) - 250)  // Контроль длины выходной строки - если слишком длинная обрезаем и выдаем ошибку
		{
			journal.jprintf("$ERROR - Response buffer overflowed!\n");
			journal.jprintf("%s\n",strReturn);
			strcat(strReturn,"E07");
			ADD_WEBDELIM(strReturn);
			break;   // выход из обработки запроса
		}
		// 1. Функции без параметра
		if (strcmp(str,"get_status")==0) // Команда get_status Получить состояние ТН - основные параметры ТН
		{
			HP.get_datetime((char*)time_TIME,strReturn); strcat(strReturn,"|SD>");
			HP.get_datetime((char*)time_DATE,strReturn);
#ifdef WEB_STATUS_SHOW_VERSION
			strcat(strReturn,"|SV>");
			strcat(strReturn,VERSION);
#endif
			strcat(strReturn,"|ST>");
			TimeIntervalToStr(HP.get_uptime(),strReturn);
			strcat(strReturn,"|SC>");
			uint32_t t = HP.get_command_completed();
			if(t) TimeIntervalToStr(rtcSAM3X8.unixtime() - t, strReturn, 1);
			else strcat(strReturn,"-");
			strcat(strReturn,"|SI>");
			_itoa(HP.CPU_LOAD,strReturn);
#if defined(WEB_STATUS_SHOW_OVERHEAT) && defined(EEV_DEF)
			strcat(strReturn,"%|SO>");
			_dtoa(strReturn, HP.dEEV.get_Overheat(), 2);
			strcat(strReturn,"°C|SF>");
#else
			strcat(strReturn,"%|SF>");
#endif
			//_itoa(freeRam()+HP.startRAM,strReturn);                          strcat(strReturn,"b|");
#ifdef FC_VACON
			HP.dFC.get_paramFC((char*)fc_cFC,strReturn);
			strcat(strReturn,"|SS>");
#else
			if(HP.dFC.get_present()) HP.dFC.get_paramFC((char*) fc_FC, strReturn); else strcat(strReturn, " - ");
			strcat(strReturn,"Гц|SS>");
#endif
			if (HP.get_errcode() == OK) {
				if(HP.get_State() == pOFF_HP) {
					strcat(strReturn, MODE_HP_STR[0]);
				} else if(HP.get_State() == pWAIT_HP) {
#ifdef USE_UPS
					if(HP.NO_Power) strcat(strReturn,"No Power!");
					else
#endif
						strcat(strReturn, "...");
				} else if(HP.get_State() == pWORK_HP) {
					if((HP.get_modWork() & pHEAT)) strcat(strReturn, MODE_HP_STR[1]);
					else if((HP.get_modWork() & pCOOL)) strcat(strReturn, MODE_HP_STR[2]);
					else if((HP.get_modWork() & pBOILER)) strcat(strReturn, MODE_HP_STR[3]);
					else if((HP.get_modWork() & pDEFROST)) strcat(strReturn, MODE_HP_STR[4]);
					else strcat(strReturn, MODE_HP_STR[0]);
					if((HP.get_modWork() & pCONTINUE)) strcat(strReturn, MODE_HP_STR[5]);
					strcat(strReturn, " ["); strcat(strReturn, (char *)codeRet[HP.get_ret()]); strcat(strReturn, "]");
				}
			} else {strcat(strReturn,"Error "); _itoa(HP.get_errcode(),strReturn);} // есть ошибки
			ADD_WEBDELIM(strReturn);
			continue;
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
			ADD_WEBDELIM(strReturn);
			continue;
		}
		if (strcmp(str,"get_configNote")==0)  // Функция get_configNote
		{
			strcat(strReturn,CONFIG_NOTE);
			ADD_WEBDELIM(strReturn);
			continue;
		}
		if (strcmp(str,"get_freeRam")==0)  // Функция freeRam
		{
			_itoa(freeRam()+HP.startRAM,strReturn);
			strcat(strReturn," b" WEBDELIM);
			continue;
		}
		if (strcmp(str,"get_loadingCPU")==0)  // Функция freeRam
		{
			_itoa(HP.CPU_LOAD,strReturn);
			strcat(strReturn,"%" WEBDELIM);
			continue;
		}
		if (strcmp(str,"get_socketInfo")==0)  // Функция  get_socketInfo
		{
			socketInfo(strReturn);    // Информация  о сокетах
			ADD_WEBDELIM(strReturn);
			continue;
		}
		if (strcmp(str,"get_socketRes")==0)  // Функция  get_socketRes
		{
			_itoa(HP.socketRes(),strReturn);
			ADD_WEBDELIM(strReturn);
			continue;
		}
		if(strncmp(str, "get_list", 8) == 0) // get_list*
		{
			str += 8;
			if(strcmp(str,"Chart")==0)  // Функция get_listChart - получить список доступных графиков
			{
				HP.get_listChart(strReturn);  // строка добавляется
				ADD_WEBDELIM(strReturn);
				continue;
			}
			if(strncmp(str,"Prof", 4)==0)  // Функции get_listProf*
			{
				if(*(str + 4) == '_') _itoa(HP.Prof.get_idProfile(), strReturn);  // get_listProf_ - текущий профиль
				else HP.Prof.get_list(strReturn); // Функция get_listProf - получить список доступных профилей
				ADD_WEBDELIM(strReturn);
				continue;
			}
			if(strcmp(str,"Press")==0)     // Функция get_listPress
			{
				for(i=0;i<ANUMBER;i++) if (HP.sADC[i].get_present()){strcat(strReturn,HP.sADC[i].get_name());strcat(strReturn,";");}
				ADD_WEBDELIM(strReturn);
				continue;
			}
			if(strcmp(str,"DSR")==0) {    // Функция get_listDSR
				strcat(strReturn, "---;");
				for(i = RCOMP + 1; i < RNUMBER; i++) if(HP.dRelay[i].get_present()) {
					strcat(strReturn, HP.dRelay[i].get_name()); strcat(strReturn,";");
				}
				ADD_WEBDELIM(strReturn);
				continue;
			}

			i = 0;
			if(strcmp(str,"Stats")==0) i = 1;   // Функция get_listStats
			else if(strcmp(str,"Hist")==0) i = 2;   // Функция get_listHist
			if(i) { // получить список доступных файлов статистики/истории в формате csv
				str = (char*)(i == 1 ? stats_file_start : history_file_start);
				i = 1;
				x = strReturn;
				for(l_i32 = rtcSAM3X8.get_years(); l_i32 > 2000; l_i32--) {
					x += m_strlen(x);
					m_snprintf(x, 8 + 4 + sizeof(stats_csv_file_ext), "%s%04d%s", str, l_i32, stats_file_ext);
					if(!wFile.opens(x, O_READ, &wfname)) {
						*x = '\0';
						break;
					} else wFile.close();
					if((y = strchr(x, '.'))) strcpy(y, stats_csv_file_ext);
					if(i) {
						strcat(x, ":1;");
						i = 0;
					} else strcat(x, ":0;");
				}
				ADD_WEBDELIM(strReturn) ;
				continue;
			} else goto x_FunctionNotFound;
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
			web_fill_tag_select(strReturn, MODE_HOUSE_WEBSTR, HP.get_modeHouse());
			ADD_WEBDELIM(strReturn) ; continue;
		}

		if (strcmp(str,"get_relayOut")==0)  // Функция Строка выходных насосов: RPUMPO = Вкл, RPUMPBH = Бойлер
		{
#ifdef RPUMPBH
			i = HP.dRelay[RPUMPBH].get_Relay();
#else
			i = 0;
#endif
			if(HP.dRelay[PUMP_OUT].get_Relay()) {
				strcat(strReturn,  "Вкл");
#ifdef RPUMPFL
				if(HP.dRelay[RPUMPFL].get_Relay()) {
					strcat(strReturn,  ", ТП");
				}
#endif
				if(i) strcat(strReturn,  ", ");
			} else {
#ifdef RPUMPFL
				if(HP.dRelay[RPUMPFL].get_Relay()) {
					strcat(strReturn,  "ТП");
					if(i) strcat(strReturn,  ", ");
				} else
#endif
					if(!i) strcat(strReturn,  "Выкл");
			}
			if(i) strcat(strReturn, "ГВС");
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
		STORE_DEBUG_INFO(21);

		if (strncmp(str, "set_SAVE", 8) == 0)  // Функция set_SAVE -
		{
			str += 8;
			if(strcmp(str, "_SCHDLR") == 0) {
				_itoa(HP.Schdlr.save(), strReturn); // сохранение расписаний
			} else if(strcmp(str, "_STATS") == 0) { // Сохранить счетчики и статистику
				xSaveStats:		if((i = HP.save_motoHour()) == OK)
					if((i = Stats.SaveStats(1)) == OK)
						i = Stats.SaveHistory(1);
				_itoa(i, strReturn);
			} else if(strcmp(str, "_UPD") == 0) { // Подготовка к обновлению
				if(HP.is_compressor_on()) _itoa(-1, strReturn);
				else {
					if(HP.dEEV.EEV != -1) HP.dEEV.set_EEV(HP.dEEV.get_maxEEV());
					goto xSaveStats;
				}
			} else {
				l_i32 = HP.save();   // записать настройки в еепром, а потом будем их писать и получить размер записываемых данных
				if(l_i32 > 0) {
					int16_t len2 = HP.Prof.save(HP.Prof.get_idProfile());
					if(len2 > 0) l_i32 += len2; else l_i32 = len2;
				}
				_itoa(l_i32, strReturn); // сохранение настроек ВСЕХ!
				HP.save_motoHour();
			}
			ADD_WEBDELIM(strReturn);
			continue;
		}
		
		if (strncmp(str, "progFC", 8) == 0)  // Функция progFC - программирование инвертора
		{
		if (!HP.dFC.get_present()) {
					strcat(strReturn,"Инвертор отсутствует");
		} else if (HP.get_modWork()==pOFF)  // Только на выключеном ТН
				{
			    strcat(strReturn,"Программирование инвертора, подробности смотри в журнале. Ожидайте 10 сек . . .");
				HP.sendCommand(pPROG_FC);        // Послать команду
				}
				else strcat(strReturn,"The heat pump must be switched OFF");	
         ADD_WEBDELIM(strReturn);
         continue;
		}
		
		
		if (strncmp(str, "set_LOAD", 8) == 0)  // Функция set_LOAD -
		{
			if(strcmp(str+8, "_SCHDLR") == 0) {
				_itoa(HP.Schdlr.load(), strReturn); // загрузка расписаний
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
		STORE_DEBUG_INFO(22);

		if (strcmp(str,"get_errcode")==0)  // Функция get_errcode
		{
			_itoa(HP.get_errcode(),strReturn); ADD_WEBDELIM(strReturn) ;    continue;
		}
		if (strcmp(str,"get_error")==0)  // Функция get_error
		{
			strcat(strReturn,HP.get_lastErr()); ADD_WEBDELIM(strReturn) ;    continue;
		}
//		if (strcmp(str,"get_tempSAM3x")==0)  // Функция get_tempSAM3x  - получение температуры чипа sam3x
//		{
//			_ftoa(strReturn,temp_DUE(),2); ADD_WEBDELIM(strReturn); continue;
//		}
		if (strcmp(str,"get_tempDS3231")==0)  // Функция get_tempDS3231  - получение температуры DS3231
		{
			_dtoa(strReturn, getTemp_RtcI2C(), 2); ADD_WEBDELIM(strReturn); continue;
		}
		if (strcmp(str,"get_fullCOP")==0)  //  получение полного COP
		{
			if (HP.fullCOP!=-1000) _dtoa(strReturn, HP.fullCOP, 2); else strcat(strReturn,"-"); ADD_WEBDELIM(strReturn); continue;
		}
		if (strcmp(str,"get_PowerCO") == 0)
		{
			_ftoa(strReturn, HP.powerCO/1000.0f,3); ADD_WEBDELIM(strReturn); continue;
		}
		if (strcmp(str,"get_PowerGEO") == 0)
		{
			_ftoa(strReturn, HP.powerGEO/1000.0f,3); ADD_WEBDELIM(strReturn); continue;
		}
		if (strcmp(str,"get_Power220") == 0)
		{
#ifdef USE_UPS
			if(HP.NO_Power) strcat(strReturn,"*.***");
			else
#endif
				_ftoa(strReturn, HP.power220/1000.0f,3);
			ADD_WEBDELIM(strReturn); continue;
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
		if (strcmp(str,"get_numberIP") == 0)  // Удаленные датчики - получить число датчиков
		{
#ifdef SENSOR_IP
			_itoa(IPNUMBER,strReturn);ADD_WEBDELIM(strReturn); continue;
#else
			strcat(strReturn,"0" WEBDELIM);continue;
#endif
		}

		if(strncmp(str + 4, "TrgT", 4) == 0) { // целевая температура
			if(*str == 's') {	// "set_TrgT"
				if(HP.get_modeHouse() != pOFF) {
					if((x = strchr(str, '='))) {
						x++;
						HP.setTargetTemp(rd(my_atof(x), 100) - (HP.get_modeHouse() == pCOOL ? HP.get_targetTempCool() : HP.get_targetTempHeat()));
						*x = '\0';
						strcat(strReturn, str);
					}
				}
			}
			// "get_TrgT" -> "x.x / y.y", "get_TrgT(1)" -> "x.x"
			if(HP.get_modeHouse() == pOFF) strcat(strReturn, "-");
			else {
				HP.getTargetTempStr(strReturn + m_strlen(strReturn));
				if(HP.get_modeHouseSettings()->Rule != pHYSTERESIS && str[8] != '(') {
					strcat(strReturn, " / ");
					strReturn = dptoa(strReturn + m_strlen(strReturn), HP.CalcTargetPID(*HP.get_modeHouseSettings()), 2);
					*--strReturn = '\0';
				}
			}
			ADD_WEBDELIM(strReturn); continue;
		}
		STORE_DEBUG_INFO(23);

		if(strcmp(str,"TEST")==0)   // Команда TEST
		{
			_itoa(random(-50,50),strReturn);
			ADD_WEBDELIM(strReturn);
			continue;
		}

		if(strncmp(str,"TASK_LIST", 9)==0)  // Функция получение списка задач и статистики
		{
			str += 9;
			if (strcmp(str,"_RST")==0)  // Функция сброс статистики по задачам
			{
#ifdef STAT_FREE_RTOS   // определена в utility/FreeRTOSConfig.h
				vTaskResetRunTimeCounters();
#else
				strcat(strReturn,NO_SUPPORT);
#endif
			} else {
#ifdef STAT_FREE_RTOS   // определена в utility/FreeRTOSConfig.h
				//strcat(strReturn,cStrEnd);
				vTaskList(strReturn + m_strlen(strReturn));
#else
				strcat(strReturn,NO_SUPPORT);
#endif
			}
			ADD_WEBDELIM(strReturn);  continue;
		}

		if(strncmp(str, "RESET_", 6)==0) {
			STORE_DEBUG_INFO(24);
			str += 6;
			if(strcmp(str,"TERR")==0) {     // Функция RESET_TERR
				HP.Reset_TempErrors();
			} else if (strcmp(str,"STAT")==0)   // RESET_STAT, Команда очистки статистики (в зависимости от типа)
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
			} else if (strcmp(str,"JOURNAL")==0)   // RESET_JOURNAL,  Команда очистки журнала (в зависимости от типа)
			{
#ifndef I2C_EEPROM_64KB     // журнал в памяти
				strcat(strReturn,"Очистка системного журнала в RAM . . .");
				journal.Clear();       // Послать команду на очистку журнала в памяти
				journal.jprintf("Reset system RAM journal . . .\n");
#else                      // Журнал в ЕЕПРОМ
				journal.Format();
				strcat(strReturn,"OK");
#endif
			} else if (strcmp(str,"DUE")==0)   // RESET_DUE, Команда сброса контроллера
			{
				strcat(strReturn,"Сброс контроллера, подождите 10 секунд . . .");
				HP.sendCommand(pRESET);        // Послать команду на сброс
			} else if(strncmp(str, "CNT_", 4) == 0) { // Команды RESET_CNT_*
				str += 4;
				if(strncmp(str, "VAR_", 4) == 0) {	// Изменение счетчиков: http://<ip:port>/&RESET_CNT_VAR_xx=n&&
					str += 4;
					*(str + 2) = '\0';
					uint32_t tmp = strtoul(str + 3, NULL, 0);
					if(strcmp(str, 		"D1") == 0) HP.get_motoHour()->D1 = tmp;
					else if(strcmp(str, "D2") == 0) HP.get_motoHour()->D2 = tmp;
					else if(strcmp(str, "H1") == 0) HP.get_motoHour()->H1 = tmp;
					else if(strcmp(str, "H2") == 0) HP.get_motoHour()->H2 = tmp;
					else if(strcmp(str, "C1") == 0) HP.get_motoHour()->C1 = tmp;
					else if(strcmp(str, "C2") == 0) HP.get_motoHour()->C2 = tmp;
					else {
						pm = my_atof(str + 3);
						if(strcmp(str, "E1") == 0) HP.get_motoHour()->E1 = pm;
						else if(strcmp(str, "E2") == 0) HP.get_motoHour()->E2 = pm;
						else if(strcmp(str, "P1") == 0) HP.get_motoHour()->P1 = pm;
						else if(strcmp(str, "P2") == 0) HP.get_motoHour()->P2 = pm;
						else goto x_FunctionNotFound;
					}
					strcat(strReturn, "OK");
				} else if(strcmp(str, "ALL") == 0) {	// RESET_CNT_ALL
					journal.jprintf("$RESET All Сounters. . .\n");
					strcat(strReturn,"Сброс ВСЕХ счетчиков");
					HP.resetCount(true);  // Полный сброс
				} else {								// RESET_CNT
					journal.jprintf("$RESET Season Counters. . .\n");
					strcat(strReturn,"Сброс счетчиков за сезон");
					HP.resetCount(false);
				}
			} else if (strcmp(str,"SETTINGS")==0) // RESET_SETTINGS, Команда сброса настроек HP
			{
				if(HP.get_State() == pOFF_HP) {
					journal.jprintf("$RESET All HP settings . . .\n");
					uint16_t d = 0;
					writeEEPROM_I2C(I2C_SETTING_EEPROM, (byte*)&d, sizeof(d));
					HP.sendCommand(pRESET);        // Послать команду на сброс
					strcat(strReturn, "OK");
				}
			}
			// FC запросы, те которые без параметра ------------------------------
			else if (strcmp(str,"ErrorFC")==0)  // Функция RESET_ErrorFC
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
			} else if (strcmp(str,"FC")==0)    // RESET_FC
			{
				if (!HP.dFC.get_present()) {
					strcat(strReturn,"Инвертор отсутствует");
				} else {
					HP.dFC.reset_FC();                             // подать команду на сброс
					strcat(strReturn,"Cброс преобразователя частоты . . .");
				}

			} else goto x_FunctionNotFound;
			ADD_WEBDELIM(strReturn); continue;
		}

#ifdef SENSOR_IP
		if (strcmp(str,"get_infoESP") == 0)  // Удаленные датчики - запрос состояния контрола
		{
			// TIN, TOUT, TBOILER, ВЕРСИЯ, ПАМЯТЬ, ЗАГРУЗКА, АПТАЙМ, ПЕРЕГРЕВ, ОБОРОТЫ, СОСТОЯНИЕ.
			_ftoa(strReturn,(float)HP.sTemp[TIN].get_Temp()/100.0,1);     strcat(strReturn,"|");
			_ftoa(strReturn,(float)HP.sTemp[TOUT].get_Temp()/100.0,1);    strcat(strReturn,"|");
			_ftoa(strReturn,(float)HP.sTemp[TBOILER].get_Temp()/100.0,1); strcat(strReturn,"|");
			strcat(strReturn,VERSION);                                    strcat(strReturn,"|");
			_itoa(freeRam()+HP.startRAM,strReturn);                       strcat(strReturn,"|");
			_itoa(HP.CPU_LOAD,strReturn);                             strcat(strReturn,"|");
			TimeIntervalToStr(HP.get_uptime(),strReturn);                 strcat(strReturn,"|");
#ifdef EEV_DEF
			_ftoa(strReturn,(float)HP.dEEV.get_Overheat()/100,2);         strcat(strReturn,"|");
#else
			strcat(strReturn,"-;");
#endif
			if (HP.dFC.get_present()) {HP.dFC.get_paramFC((char*)fc_FC,strReturn);    strcat(strReturn,"|");} // В зависимости от наличия инвертора
			else                      {strcat(strReturn," - ");                       strcat(strReturn,"|");}
			//  strcat(strReturn,HP.StateToStrEN());                                      strcat(strReturn,";");
			if (HP.get_errcode()==OK)  strcat(strReturn,HP.StateToStrEN());                   // Ошибок нет
			else {strcat(strReturn,"Error "); _itoa(HP.get_errcode(),strReturn);} // есть ошибки
			strcat(strReturn,"|");   ADD_WEBDELIM(strReturn) ;    continue;
		}
#endif
		if(strncmp(str, "hide_", 5) == 0) { // Удаление элементов внутри tag name="hide_*"
			str += 5;
			if(strcmp(str, "fcan") == 0) {
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
			} else if(strcmp(str, "tro_ei") == 0) { // hide: TCOMPIN, TEVAIN
#ifdef TCOMPIN
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
			} else if(strcmp(str, "pid2") == 0) { // hide: PID2
#ifdef PID_FORMULA2
				strcat(strReturn,"0");
#else
				strcat(strReturn,"1");
#endif
			} else if(strcmp(str, "EEVpid") == 0) { //  hide_EEVpid
#ifdef EEV_DEF
				strcat(strReturn, GETBIT(HP.dEEV.get_flags(), fEEV_DirectAlgorithm) ? "1" : "0");
#else
				strcat(strReturn,"1");
#endif
			} else if(strcmp(str, "EEVU") == 0) { //  hide_EEVU
#ifdef EEV_PREFER_PERCENT
				strcat(strReturn, "%");
#else
				strcat(strReturn, "шаги");
#endif
			}
			ADD_WEBDELIM(strReturn); continue;
		}

		if (strcmp(str,"CONST")==0)   // Команда CONST  Информация очень большая по этому разбито на 2 запроса CONST CONST1
		{
			STORE_DEBUG_INFO(25);
			strcat(strReturn,"VERSION|Версия прошивки|");strcat(strReturn,VERSION);strcat(strReturn,";");
			strcat(strReturn,"__DATE__ __TIME__|Дата и время сборки прошивки|");strcat(strReturn,__DATE__);strcat(strReturn," ");strcat(strReturn,__TIME__) ;strcat(strReturn,";");
			strcat(strReturn,"CONFIG_NAME|Имя конфигурации|");strcat(strReturn,CONFIG_NAME);strcat(strReturn,";");
			strcat(strReturn,"CONFIG_NOTE|");strcat(strReturn,CONFIG_NOTE);strcat(strReturn,"|;");
			strcat(strReturn,"configCPU_CLOCK_HZ|Частота CPU (МГц)|");_itoa(configCPU_CLOCK_HZ/1000000,strReturn);strcat(strReturn,";");
			strcat(strReturn,"SD_SPI_SPEED|Частота SPI SD карты (МГц)|");_itoa(SD_CLOCK, strReturn);strcat(strReturn,";");
			strcat(strReturn,"W5200_SPI_SPEED|Частота SPI сети "); strcat(strReturn,nameWiznet);strcat(strReturn," (МГц)|");_itoa(84/W5200_SPI_SPEED, strReturn);strcat(strReturn,";");
			strcat(strReturn,"I2C_SPEED|Частота работы шины I2C (кГц)|"); _itoa(I2C_SPEED/1000,strReturn); strcat(strReturn,";");
			strcat(strReturn,"UART_SPEED|Скорость отладочного порта (бод)|");_itoa(UART_SPEED,strReturn);strcat(strReturn,";");
			strcat(strReturn,"WDT_TIME|Период сторожевого таймера, 0 - нет (сек)|");_itoa(WDT_TIME,strReturn);strcat(strReturn,";");
			strcat(strReturn,"MODBUS_PORT_NUM|Используемый порт для обмена по Modbus RTU|Serial");
			if(&MODBUS_PORT_NUM==&Serial1) strcat(strReturn,cOne);
			else if(&MODBUS_PORT_NUM==&Serial2) strcat(strReturn,"2");
			else if(&MODBUS_PORT_NUM==&Serial3) strcat(strReturn,"3");
			else strcat(strReturn,"?");
			strcat(strReturn,";");
			strcat(strReturn,"MODBUS_PORT_SPEED|Скорость обмена (бод)|");_itoa(MODBUS_PORT_SPEED,strReturn);strcat(strReturn,";");
			strcat(strReturn,"MODBUS_PORT_CONFIG|Конфигурация порта|8N1;");
			strcat(strReturn,"MODBUS_TIME_WAIT|Максимальное время ожидания освобождения порта (мсек)|");_itoa(MODBUS_TIME_WAIT,strReturn);strcat(strReturn,";");
			// Частотник
			strcat(strReturn,"DEVICEFC|Поддержка инвертора для компрессора|");
			if (HP.dFC.get_present())
			{
				strcat(strReturn,HP.dFC.get_name());strcat(strReturn,";");
				strcat(strReturn,"FC_MODBUS_ADR|Адрес инвертора на шине Modbus RTU|");strcat(strReturn,byteToHex(FC_MODBUS_ADR));strcat(strReturn,";");
			}
			else strcat(strReturn,"Нет;");
			// NEXTION
			strcat(strReturn,"NEXTION|Использование дисплея Nextion|");
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
			strcat(strReturn,"NEXTION_PORT|Порт подключения дисплея Nextion|Serial");
			if(&NEXTION_PORT==&Serial1) strcat(strReturn,cOne);
			else if(&NEXTION_PORT==&Serial2) strcat(strReturn,"2");
			else if(&NEXTION_PORT==&Serial3) strcat(strReturn,"3");
			else strcat(strReturn,"?");
			strcat(strReturn,";");
			strcat(strReturn,"NEXTION_PORT_SPEED|Скорость обмена (бод)|");_itoa(NEXTION_PORT_SPEED,strReturn);strcat(strReturn,";");
			strcat(strReturn,"NEXTION_UPDATE|Время обновления информации на дисплее Nextion (мсек)|");_itoa(NEXTION_UPDATE,strReturn);strcat(strReturn,";");
			strcat(strReturn,"NEXTION_READ|Время опроса дисплея Nextion (мсек)|");_itoa(NEXTION_READ,strReturn);strcat(strReturn,";");

			// Карта
			m_snprintf(strReturn + strlen(strReturn), 128, "SD_FAT_VERSION|Версия библиотеки SdFat|%s;", SD_FAT_VERSION);
			m_snprintf(strReturn + strlen(strReturn), 128, "USE_SD_CRC|SD - Использовать проверку CRC|%c;", USE_SD_CRC ? '0'+USE_SD_CRC : USE_SD_CRC_FOR_WRITE ? 'W' : '-');
			strcat(strReturn,"SD_REPEAT|SD - Число попыток чтения, при неудаче переход на работу без карты|");_itoa(SD_REPEAT,strReturn);strcat(strReturn,";");

			// W5200
			strcat(strReturn,"W5200_THREAD|Число потоков для сетевого чипа (web сервера) "); strcat(strReturn,nameWiznet);strcat(strReturn,"|");_itoa(W5200_THREAD,strReturn);strcat(strReturn,";");
			strcat(strReturn,"W5200_TIME_WAIT|Время ожидания захвата мютекса, для управления потоками (мсек)|");_itoa( W5200_TIME_WAIT,strReturn);strcat(strReturn,";");
			strcat(strReturn,"STACK_vWebX|Размер стека для задачи одного web потока "); strcat(strReturn,nameWiznet);strcat(strReturn," (х4 байта)|");_itoa(STACK_vWebX,strReturn);strcat(strReturn,";");
			strcat(strReturn,"W5200_NUM_PING|Число попыток пинга до определения потери связи |");_itoa(W5200_NUM_PING,strReturn);strcat(strReturn,";");
			strcat(strReturn,"W5200_MAX_LEN|Размер аппаратного буфера  сетевого чипа "); strcat(strReturn,nameWiznet);strcat(strReturn," (байт)|");_itoa(W5200_MAX_LEN,strReturn);strcat(strReturn,";");
			strcat(strReturn,"INDEX_FILE|Файл загружаемый по умолчанию|");strcat(strReturn,INDEX_FILE);strcat(strReturn,";");
			strcat(strReturn,"TIME_ZONE|Часовой пояс|");_itoa(TIME_ZONE,strReturn);strcat(strReturn,";");
			// FreeRTOS
			strcat(strReturn,"FREE_RTOS_ARM_VERSION|Версия библиотеки FreeRTOS|");_itoa(FREE_RTOS_ARM_VERSION,strReturn);strcat(strReturn,";");
			strcat(strReturn,"configTICK_RATE_HZ|Квант времени системы FreeRTOS (мкс)|");_itoa(configTICK_RATE_HZ,strReturn);strcat(strReturn,";");

			strcat(strReturn,"TIME_READ_SENSOR|Период опроса датчиков (мсек)|");_itoa(TIME_READ_SENSOR,strReturn);//strcat(strReturn,";");
			ADD_WEBDELIM(strReturn);  continue;
		} // end CONST

		if (strcmp(str,"CONST1")==0)   // Команда CONST1 Информация очень большая по этому разбито на 2 запроса CONST CONST1
		{
			strcat(strReturn,"TIME_CONTROL|Период управления тепловым насосом (мсек)|");_itoa(TIME_CONTROL,strReturn);strcat(strReturn,";");
			strcat(strReturn,"TIME_EEV|Период управления ЭРВ (мсек)|");_itoa(TIME_EEV,strReturn);strcat(strReturn,";");
			strcat(strReturn,"TIME_WEB_SERVER|Период опроса web сервера "); strcat(strReturn,nameWiznet);strcat(strReturn," (мсек)|");_itoa(TIME_WEB_SERVER,strReturn);strcat(strReturn,";");
			strcat(strReturn,"TIME_COMMAND|Период разбора команд управления ТН (мсек)|");_itoa(TIME_COMMAND,strReturn);strcat(strReturn,";");
			strcat(strReturn,"TIME_I2C_UPDATE |Период синхронизации внутренних часов с I2C часами (мсек)|");_itoa(TIME_I2C_UPDATE,strReturn);strcat(strReturn,";");
			// i2c
			strcat(strReturn,"I2C_COUNT_EEPROM|Адрес внутри чипа I2C с которого пишется счетчики ТН|"); strcat(strReturn,uint16ToHex(I2C_COUNT_EEPROM)); strcat(strReturn,";");
			strcat(strReturn,"I2C_SETTING_EEPROM|Адрес внутри чипа I2C с которого пишутся настройки ТН|"); strcat(strReturn,uint16ToHex(I2C_SETTING_EEPROM)); strcat(strReturn,";");
			strcat(strReturn,"I2C_PROFILE_EEPROM|Адрес внутри чипа I2C с которого пишется профили ТН|"); strcat(strReturn,uint16ToHex(I2C_PROFILE_EEPROM)); strcat(strReturn,";");
			// Датчики
			strcat(strReturn,"P_NUMSAMLES|Число значений для усреднения показаний давления|");_itoa(P_NUMSAMLES,strReturn);strcat(strReturn,";");
			strcat(strReturn,"T_NUMSAMLES|Число значений для усреднения показаний температуры|");_itoa(T_NUMSAMLES,strReturn);strcat(strReturn,";");
			strcat(strReturn,"GAP_TEMP_VAL|Допустимая разница показаний между двумя считываниями (°C)|");_dtoa(strReturn, GAP_TEMP_VAL, 2);strcat(strReturn,";");
			strcat(strReturn,"MAX_TEMP_ERR|Максимальная систематическая ошибка датчика температуры (°C)|");_ftoa(strReturn, MAX_TEMP_ERR, 2);strcat(strReturn,";");
			// Удаленные датчики
			strcat(strReturn,"SENSOR_IP|Использование удаленных датчиков|");
#ifdef SENSOR_IP
			strcat(strReturn,"ON;");
			strcat(strReturn,"IPNUMBER|Максимальное число удаленных датчиков, нумерация начинается с 1|");_itoa(IPNUMBER,strReturn);strcat(strReturn,";");
			strcat(strReturn,"UPDATE_IP|Максимальное время между посылками данных с удаленного датчика (сек)|");_itoa(UPDATE_IP,strReturn);strcat(strReturn,";");
#else
			strcat(strReturn,"OFF;");
#endif
#ifdef VCC_CONTROL  // если разрешено чтение напряжение питания
			strcat(strReturn,"K_VCC_POWER|Коэффициент пересчета для канала контроля напряжения питания (отсчеты/В)|");
			_ftoa(strReturn,(float)K_VCC_POWER,2);strcat(strReturn,";");
#endif
			// SALLMONELA
			strcat(strReturn,"SALLMONELA_DAY|День недели когда проводится обеззараживание ГВС (1-понедельник)|");_itoa(SALLMONELA_DAY,strReturn);strcat(strReturn,";");
			strcat(strReturn,"SALLMONELA_HOUR|Час когда начинаятся обеззарживание ГВС|");_itoa(SALLMONELA_HOUR,strReturn);strcat(strReturn,";");
			strcat(strReturn,"SALLMONELA_TEMP|Целевая температура обеззараживания ГВС (°C)|");_dtoa(strReturn, SALLMONELA_TEMP, 2);strcat(strReturn,";");
			// ЭРВ
#ifdef EEV_DEF
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
			strcat(strReturn,"DEMO|Режим демонстрации (эмуляция датчиков)|");
#ifdef DEMO
			strcat(strReturn,"ON;");
#else
			strcat(strReturn,"OFF;");
#endif
			strcat(strReturn,"VER_SAVE|Версия формата сохраненных данных в I2C памяти|");
			_itoa(VER_SAVE,strReturn);
			//if(VER_SAVE != HP.Option.ver) { strcat(strReturn," ("); _itoa(HP.Option.ver, strReturn); strcat(strReturn,")"); }
			strcat(strReturn,";");
			strcat(strReturn,"DEBUG|Вывод в порт отладочных сообщений|");
#ifdef DEBUG
			strcat(strReturn,"ON;");
#else
			strcat(strReturn,"OFF;");
#endif
			strcat(strReturn,"STAT_FREE_RTOS|Накопление статистики FreeRTOS (отладка)|");
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
			strcat(strReturn,"CHART_POINT|Максимальное число точек графиков|");_itoa(CHART_POINT,strReturn);strcat(strReturn,";");
			strcat(strReturn,"I2C_EEPROM_64KB|Место хранения системного журнала|");
#ifdef I2C_EEPROM_64KB
			strcat(strReturn,"I2C flash memory;");
#else
			strcat(strReturn,"RAM memory;");
#endif
			strcat(strReturn,"JOURNAL_LEN|Размер кольцевого буфера системного журнала (байт)|");_itoa(JOURNAL_LEN,strReturn);//strcat(strReturn,";");
			ADD_WEBDELIM(strReturn) ;
			continue;
		} // end CONST1

		if(strncmp(str, "get_sys", 7) == 0)
		{
			str += 7;
			STORE_DEBUG_INFO(26);
			if(strcmp(str, "Info") == 0) { // "get_sysInfo" - Функция вывода системной информации для разработчика
				strcat(strReturn,"Источник загрузкки web интерфейса |");
				switch (HP.get_SourceWeb())
				{
				case pMIN_WEB:   strcat(strReturn,"internal;"); break;
				case pSD_WEB:    strcat(strReturn,"SD card;"); break;
				case pFLASH_WEB: strcat(strReturn,"SPI Flash;"); break;
				default:         strcat(strReturn,"unknown;"); break;
				}

				strcat(strReturn, "Входное напряжение питания контроллера, V:|");
	#ifdef VCC_CONTROL  // если разрешено чтение напряжение питания
				_ftoa(strReturn,(float)HP.AdcVcc/K_VCC_POWER,2);strcat(strReturn,";");
	#else
				strReturn += m_snprintf(strReturn += strlen(strReturn), 256, "%sесли < %.1d - сброс;", Is_otg_vbus_high() ? ", +USB" : "", ((SUPC->SUPC_SMMR & SUPC_SMMR_SMTH_Msk) >> SUPC_SMMR_SMTH_Pos) + 19);
	#endif
				strReturn += m_snprintf(strReturn += strlen(strReturn), 256, "Режим safeNetwork (%sадрес:%d.%d.%d.%d шлюз:%d.%d.%d.%d, не спрашивать пароль)|%s;", defaultDHCP ?"DHCP, ":"",defaultIP[0],defaultIP[1],defaultIP[2],defaultIP[3],defaultGateway[0],defaultGateway[1],defaultGateway[2],defaultGateway[3],HP.safeNetwork ?cYes:cNo);
				//strcat(strReturn,"Уникальный ID чипа SAM3X8E|");
				//getIDchip(strReturn); strcat(strReturn,";");
				//strcat(strReturn,"Значение регистра VERSIONR сетевого чипа WizNet (51-w5100, 3-w5200, 4-w5500)|");_itoa(W5200VERSIONR(),strReturn);strcat(strReturn,";");

	#ifdef DRV_EEV_L9333          // Контроль за работой драйвера ЭРВ
				strcat(strReturn,"Контроль за работой драйвера ЭРВ |");
				if (digitalReadDirect(PIN_STEP_DIAG))  strcat(strReturn,"Error L9333;"); else strcat(strReturn,"Normal;");
	#endif
				m_snprintf(strReturn+strlen(strReturn), 256, "Состояние FreeRTOS при старте (task+err_code) <sup>2</sup>|0x%04X;", lastErrorFreeRtosCode);

				startSupcStatusReg |= SUPC->SUPC_SR;                                  // Копим изменения
				m_snprintf(strReturn += strlen(strReturn), 256, "Регистры контроллера питания (SUPC) SAM3X8E [SUPC_SMMR SUPC_MR SUPC_SR]|0x%08X %08X %08X", SUPC->SUPC_SMMR, SUPC->SUPC_MR, startSupcStatusReg);  // Регистры состояния контроллера питания
				if((startSupcStatusReg|SUPC_SR_SMS)==SUPC_SR_SMS_PRESENT) strcat(strReturn," bad VDDIN!");
				strcat(strReturn,";");

				// Вывод строки статуса
				STORE_DEBUG_INFO(46);
				strReturn += m_snprintf(strReturn += strlen(strReturn), 256, "Строка статуса ТН <sup>4</sup>|State:%d modWork:%X[%s] fHP:%X", HP.get_State(), HP.get_modWork(), codeRet[HP.get_ret()], HP.flags);
				//for(i = 0; i < RNUMBER; i++) strReturn += m_snprintf(strReturn, 32, " %s:%d", HP.dRelay[i].get_name(), HP.dRelay[i].get_Relay());
				//if(HP.dFC.get_present())  {strcat(strReturn," freqFC:"); _ftoa(strReturn,(float)HP.dFC.get_frequency()/100.0,2); }
				//if(HP.dFC.get_present())  {strcat(strReturn," Power:"); _ftoa(strReturn,(float)HP.dFC.get_power()/1000.0,3);  }
				strcat(strReturn,";");

				STORE_DEBUG_INFO(47);
				strcat(strReturn,"<b> Времена</b>|;");
				strcat(strReturn,"Текущее время|"); DecodeTimeDate(rtcSAM3X8.unixtime(),strReturn); strcat(strReturn,";");
				strcat(strReturn,"Время последнего включения ТН|");DecodeTimeDate(HP.get_startTime(),strReturn);strcat(strReturn,";");
				strcat(strReturn,"Время текущего состояния ТН|");DecodeTimeDate(HP.get_command_completed(),strReturn);strcat(strReturn,";");
				strcat(strReturn,"Время старта компрессора|");DecodeTimeDate(HP.get_startCompressor(),strReturn);strcat(strReturn,";");
				strcat(strReturn,"Время останова компрессора|");DecodeTimeDate(HP.get_stopCompressor(),strReturn);strcat(strReturn,";");
				strcat(strReturn,"Время сохранения текущих настроек ТН|");DecodeTimeDate(HP.get_saveTime(),strReturn);strcat(strReturn,";");

				strcat(strReturn,"<b> Счетчики ошибок</b>|;");
				strcat(strReturn,"Счетчик текущего числа повторных попыток пуска ТН|");
				if(HP.get_State()==pWORK_HP) { _itoa(HP.num_repeat,strReturn);strcat(strReturn,";");} else strcat(strReturn,"0;");
				strcat(strReturn,"Счетчик \"Потеря связи с "); strcat(strReturn,nameWiznet);strcat(strReturn,"\", повторная инициализация  <sup>3</sup>|");_itoa(HP.num_resW5200,strReturn);strcat(strReturn,";");
				strcat(strReturn,"Счетчик числа сбросов мютекса захвата шины SPI|");_itoa(HP.num_resMutexSPI,strReturn);strcat(strReturn,";");
				strcat(strReturn,"Счетчик числа сбросов мютекса захвата шины I2C|");_itoa(HP.num_resMutexI2C,strReturn);strcat(strReturn,";");
	#ifdef MQTT
				strcat(strReturn,"Счетчик числа повторных соединений MQTT клиента|");_itoa(HP.num_resMQTT,strReturn);strcat(strReturn,";");
	#endif
				strcat(strReturn,"Счетчик неудачных ping|");_itoa(HP.num_resPing,strReturn);strcat(strReturn,";");
	#ifdef USE_ELECTROMETER_SDM
				strcat(strReturn,"Счетчик числа ошибок чтения электро-счетчика|");_itoa(HP.dSDM.get_numErr(),strReturn);strcat(strReturn,";");
	#endif
				if (HP.dFC.get_present()) strcat(strReturn,"Счетчик числа ошибок чтения частотного преобразователя (RS485)|");_itoa(HP.dFC.get_numErr(),strReturn);strcat(strReturn,";");
				strcat(strReturn,"Счетчик числа ошибок чтения датчиков температуры (DS18x20)|");_itoa(HP.get_errorReadDS18B20(),strReturn);strcat(strReturn,";");

				strcat(strReturn,"<b> Глобальные счетчики (Всего за весь период)</b>|;");
				strcat(strReturn,"Время сброса счетчиков|");DecodeTimeDate(HP.get_motoHour()->D1,strReturn);strcat(strReturn,";");
				strcat(strReturn,"Часы работы ТН (час)|");_ftoa(strReturn,(float)HP.get_motoHour()->H1/60.0f,1);strcat(strReturn,";");
				strcat(strReturn,"Часы работы компрессора ТН (час)|");_ftoa(strReturn,(float)HP.get_motoHour()->C1/60.0f,1);strcat(strReturn,";");
				strcat(strReturn,"Потребленная энергия ТН (кВт*ч)|");_dtoa(strReturn, HP.get_motoHour()->E1 / 1000, 3);strcat(strReturn,";");
	#ifdef  FLOWCON
				if(HP.sTemp[TCONING].get_present() & HP.sTemp[TCONOUTG].get_present()) {
					strcat(strReturn,"Выработанная энергия ТН (кВт*ч)|");_dtoa(strReturn, HP.get_motoHour()->P1 / 1000, 3);strcat(strReturn,";"); // Если есть оборудование
				}
	#endif
				strcat(strReturn,"<b> Сезонные счетчики</b>|;");
				strcat(strReturn,"Время сброса сезонных счетчиков ТН|");DecodeTimeDate(HP.get_motoHour()->D2,strReturn);strcat(strReturn,";");
				strcat(strReturn,"Часы работы ТН за сезон (час)|");_ftoa(strReturn,(float)HP.get_motoHour()->H2/60.0f,1);strcat(strReturn,";");
				strcat(strReturn,"Часы работы компрессора ТН за сезон (час)|");_ftoa(strReturn,(float)HP.get_motoHour()->C2/60.0f,1);strcat(strReturn,";");
				strcat(strReturn,"Потребленная энергия ТН за сезон (кВт*ч)|");_dtoa(strReturn, HP.get_motoHour()->E2 / 1000, 3);strcat(strReturn,";");
	#ifdef  FLOWCON
				if(HP.sTemp[TCONING].get_present() & HP.sTemp[TCONOUTG].get_present()) {
					strcat(strReturn,"Выработанная энергия ТН за сезон (кВт*ч)|");_dtoa(strReturn, HP.get_motoHour()->P2 / 1000, 3);strcat(strReturn,";"); // Если есть оборудование
				}
	#endif

				STORE_DEBUG_INFO(48);
				strcat(strReturn,"<b> Статистика за день</b>|;");
				strReturn += strlen(strReturn);
				Stats.StatsWebTable(strReturn);

			} else if(strcmp(str, "Err") == 0) { // "get_sysErr" - Описание всех ошибок
				strReturn += strlen(strReturn);
				for(i = 0; i >= ERR_ERRMAX; i--) {
					strReturn += m_snprintf(strReturn, 256, "%d|%s;", i, noteError[abs(i)]);
					if(strReturn >= Socket[thread].outBuf + sizeof(Socket[thread].outBuf) - 256) {
						if(sendBufferRTOS(thread, (byte*) (Socket[thread].outBuf), strlen(Socket[thread].outBuf)) == 0) {
							journal.jprintf("$Error send get_sysErr!\n");
							break;
						}
						strReturn = Socket[thread].outBuf;
						strReturn[0] = '\0';
					}
				}
			}
			ADD_WEBDELIM(strReturn); continue;
		}
		STORE_DEBUG_INFO(27);

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
			_dtoa(strReturn, HP.get_overcool(), 2);
			ADD_WEBDELIM(strReturn);
			continue;
		}
#ifdef EEV_DEF
		if(strcmp(str, "get_OverHeat") == 0) { // Выводит 2 перегрева сразу
			_dtoa(strReturn, HP.dEEV.get_Overheat(), 2);
#ifdef TCOMPIN
#ifdef TCONIN
			if(HP.dEEV.get_ruleEEV() != TCOMPIN_PEVA) {
#else
			if(HP.dEEV.get_ruleEEV() != TCOMPIN_PEVA && HP.is_heating()) {
#endif
				strcat(strReturn, " / ");
				_dtoa(strReturn, HP.dEEV.OverheatTCOMP, 2);
			}
#endif
			ADD_WEBDELIM(strReturn);
			continue;
		}
#endif
		if(strcmp(str, "get_Evapor") == 0) {
			if(HP.sADC[PEVA].get_present()) _dtoa(strReturn, HP.get_temp_evaporating(), 2);
			else strcat(strReturn,"-");
			ADD_WEBDELIM(strReturn);
			continue;
		}
		if(strcmp(str, "get_TCOMP_TCON") == 0) { // Нагнетание - Конденсация
			_dtoa(strReturn, HP.sTemp[TCOMP].get_Temp() - HP.get_temp_condensing(), 2);
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
		if(strncmp(str, "get_tbl", 7) == 0) {
			str += 7;
			if(strcmp(str, "TempF") == 0) // get_tblTempF, Возвращает список датчиков через ";"
			{
				for(i = 0; i < TNUMBER; i++) if(HP.sTemp[i].get_present()) { strcat(strReturn, HP.sTemp[i].get_name()); strcat(strReturn, ";"); }
			} else if(strncmp(str, "Temp", 4) == 0) // get_tblTempN - Возвращает список датчиков через ";", N число в конце - возвращаются датчики имеющие этот бит в SENSORTEMP[]
			{
				uint8_t m = atoi(str + 4);
				for(i = 0; i < TNUMBER; i++)
					if((HP.sTemp[i].get_cfg_flags() & (1<<m)) && ((HP.sTemp[i].get_cfg_flags()&(1<<0)) || HP.sTemp[i].get_fAddress())) {
						strcat(strReturn, HP.sTemp[i].get_name()); strcat(strReturn, ";");
					}
			} else if(strcmp(str,"Input")==0)     // Функция get_tblInput
			{
				for(i=0;i<INUMBER;i++) if(HP.sInput[i].get_present()){strcat(strReturn,HP.sInput[i].get_name());strcat(strReturn,";");}
			} else if(strcmp(str,"Relay")==0)     // Функция get_tblRelay
			{
				for(i=0;i<RNUMBER;i++) if(HP.dRelay[i].get_present()){strcat(strReturn,HP.dRelay[i].get_name());strcat(strReturn,";");}
			} else if(strcmp(str,"Flow")==0)     // Функция get_tblFlow
			{
				for(i=0;i<FNUMBER;i++) if(HP.sFrequency[i].get_present()){strcat(strReturn,HP.sFrequency[i].get_name());strcat(strReturn,";");}
			} else if(strcmp(str,"PDS")==0) {    // Функция get_tblPDS
				for(i = 0; i < DAILY_SWITCH_MAX; i++) {
					if(HP.Prof.DailySwitch[i].Device == 0) {
						strcat(strReturn, "---;;00:00;00:00|");
						break;
					}
					strReturn += m_snprintf(strReturn += m_strlen(strReturn), 256, "%s;%s;%02d:%d0;%02d:%d0|", HP.dRelay[HP.Prof.DailySwitch[i].Device].get_name(), HP.dRelay[HP.Prof.DailySwitch[i].Device].get_note(),
							HP.Prof.DailySwitch[i].TimeOn / 10, HP.Prof.DailySwitch[i].TimeOn % 10, HP.Prof.DailySwitch[i].TimeOff / 10, HP.Prof.DailySwitch[i].TimeOff % 10);
				}
#ifdef CORRECT_POWER220
			} else if(strcmp(str,"PwrC")==0) {    // Функция get_tblPwrC
				for(i = 0; i < (int8_t)(sizeof(correct_power220)/sizeof(correct_power220[0])); i++) {
					m_snprintf(strReturn + m_strlen(strReturn), 64, "%s;%d;", HP.dRelay[correct_power220[i].num].get_name(), correct_power220[i].value);
				}
#endif
			} else goto x_FunctionNotFound;
			ADD_WEBDELIM(strReturn);
			continue;
		}
#ifdef RADIO_SENSORS
		if(strcmp(str, "set_radio_cmd") == 0) {
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
		STORE_DEBUG_INFO(28);
		if (((x=strpbrk(str,"("))!=0)&&((y=strpbrk(str,")"))!=0))  // Функция с одним параметром - найдена открывающиеся и закрывающиеся скобка
		{
			// Выделяем параметр функции на выходе число - номер параметра
			// применяется кодирование 0-19 - температуры 20-29 - сухой контакт 30-39 -аналоговые датчики
			y[0]=0;             // Стираем скобку ")"  строка х содержит параметр
			*x++ = 0;			// '(' -> '\0'

			// -----------------------------------------------------------------------------------------------------
			// 2.1  Удаленный датчик
			// -----------------------------------------------------------------------------------------------------
#ifdef SENSOR_IP                           // Получение данных удаленного датчика
			// Получение данных с удаленного датчика
			if (strcmp(str,"set_sensorIP") == 0)           // Удаленные датчики - получить значения
			{
				// разбор строки формат "номер:температура:уровень_сигнала:питание:счетчик"
				//     SerialDbg.println(x);
				ptr=strtok(x,":");
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
				l_i32=atoi(ptr);
				if (l_i32<0)                      {strcat(strReturn,"E24" WEBDELIM);continue;}  // счетичк больше 0


				// Дошли до сюда - ошибок нет, можно использовать данные
				byte adr[4];
				W5100.readSnDIPR(sock, adr);
				HP.sIP[a-1].set_DataIP(a,b,c,d,l_i32,BytesToIPAddress(adr));
				strcat(strReturn,"OK" WEBDELIM); continue;
			}

			if (strcmp(str,"get_sensorParamIP") == 0)    // Удаленные датчики - Получить отдельное значение конкретного параметра
			{
				ptr=strtok(x,":");     // Нужно
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

			if (strcmp(str,"get_sensorIP") == 0)    // Удаленные датчики - Получить параметры (ВСЕ) удаленного датчика в виде строки разделитель "|"
			{
				ptr=x;
				if ((a=atoi(ptr))==0)         {strcat(strReturn,"E22" WEBDELIM);continue;}  // если возвращен 0 то ошибка преобразования
				if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23" WEBDELIM);continue;}  // проверка диапазона номеров датчиков
				// Формируем строку
				HP.sIP[a-1].get_sensorIP((char*)ip_SENSOR_NUMBER,strReturn); strcat(strReturn,"|");

				if (HP.sIP[a-1].get_update()>UPDATE_IP)  strcat(strReturn,"-|") ;                       // Время просрочено, удаленный датчик не используем
				else { HP.sIP[a-1].get_sensorIP((char*)ip_SENSOR_TEMP,strReturn);strcat(strReturn,"|"); }

				if (HP.sIP[a-1].get_count()>0)     // Если были пакеты то выводим данные по ним
				{
					HP.sIP[a-1].get_sensorIP((char*)ip_STIME,strReturn);strcat(strReturn,"|");
					HP.sIP[a-1].get_sensorIP((char*)ip_SENSOR_IP,strReturn);strcat(strReturn,"|");
					HP.sIP[a-1].get_sensorIP((char*)ip_RSSI,strReturn);strcat(strReturn,"|");
					HP.sIP[a-1].get_sensorIP((char*)ip_VCC,strReturn); strcat(strReturn,"|");
					HP.sIP[a-1].get_sensorIP((char*)ip_SENSOR_COUNT,strReturn); //strcat(strReturn,"|");
				}
				else strcat(strReturn,"-|-|-|-|-");  // После включения еще ни разу данные не поступали поэтому прочерки
				ADD_WEBDELIM(strReturn) ; continue;
			}

			if (strcmp(str,"get_listIP") == 0)    // Удаленные датчики - список привязки удаленного датчика
			{
				ptr=x;
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


			if (strcmp(str,"get_sensorUseIP") == 0)    // Удаленные датчики - ПОЛУЧИТЬ использование удаленного датчика
			{
				if ((a=atoi(x))==0)         {strcat(strReturn,"E22" WEBDELIM);continue;}  // если возвращен 0 то ошибка преобразования
				if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23" WEBDELIM);continue;}  // проверка диапазона номеров датчиков
				HP.sIP[a-1].get_sensorIP((char*)ip_SENSOR_USE,strReturn);ADD_WEBDELIM(strReturn) ;continue;
			}


			if (strcmp(str,"get_sensorRuleIP") == 0)    // Удаленные датчики - ПОЛУЧИТЬ использование усреднение
			{
				if ((a=atoi(x))==0)         {strcat(strReturn,"E22" WEBDELIM);continue;}  // если возвращен 0 то ошибка преобразования
				if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23" WEBDELIM);continue;}  // проверка диапазона номеров датчиков
				HP.sIP[a-1].get_sensorIP((char*)ip_SENSOR_RULE,strReturn);ADD_WEBDELIM(strReturn) ;continue;
			}

#else
			if (strcmp(str,"set_sensorIP")==0)   {strcat(strReturn,"E25" WEBDELIM);continue;}        // Удаленные датчики  НЕ ПОДДЕРЖИВАЕТСЯ
#endif

			// ----------------------------------------------------------------------------------------------------------
			// 2.2 Функции с одним параметром
			// ----------------------------------------------------------------------------------------------------------
			STORE_DEBUG_INFO(29);

			if (strcmp(str,"set_modeHP")==0)           // Функция set_modeHP - установить режим отопления ТН
			{
				if((pm = my_atof(x)) == ATOF_ERROR)  strcat(strReturn,"E09");      // Ошибка преобразования   - завершить запрос с ошибкой
				else {
					if(pm > pCOOL) pm = pOFF;
					HP.set_mode(pm);
					web_fill_tag_select(strReturn, MODE_HOUSE_WEBSTR, HP.get_modeHouse());
				}
				ADD_WEBDELIM(strReturn) ; continue;
			}
			// ------------------------------------------------------------------------
			if (strcmp(str,"set_testMode")==0)  // Функция set_testMode  - Установить режим работы бойлера
			{
				if ((pm=my_atof(x))==ATOF_ERROR)  strcat(strReturn,"E09");      // Ошибка преобразования   - завершить запрос с ошибкой
				else
				{
					HP.set_testMode((TEST_MODE)pm);             // Установить режим работы тестирования
					for(i=0;i<=HARD_TEST;i++)                    // Формирование списка
					{ strcat(strReturn,noteTestMode[i]); strcat(strReturn,":"); if(i==HP.get_testMode()) strcat(strReturn,cOne); else strcat(strReturn,cZero); strcat(strReturn,";");  }
				} // else
				ADD_WEBDELIM(strReturn) ;    continue;
			}

			// -----------------------------------------------------------------------------
			// ПРОФИЛИ функции с одним параметром
			// -----------------------------------------------------------------------------
			if (strcmp(str,"saveProfile")==0)  // Функция saveProfile сохранение текущего профиля
			{
				if ((pm=my_atof(x))==ATOF_ERROR)  strcat(strReturn,"E29");      // Ошибка преобразования   - завершить запрос с ошибкой
				else
				{
					if ((pm>=0)&&(pm<I2C_PROFIL_NUM)) _itoa(HP.Prof.save((int8_t)pm),strReturn); else strcat(strReturn,"E29");  ADD_WEBDELIM(strReturn) ;    continue;
				}
			}
			// -----------------------------------------------------------------------------
			if (strcmp(str,"loadProfile")==0)  // Функция loadProfile загрузка профиля в текущий
			{
				if ((pm=my_atof(x))==ATOF_ERROR)  strcat(strReturn,"E29");      // Ошибка преобразования   - завершить запрос с ошибкой
				else
				{
					if ((pm>=0)&&(pm<I2C_PROFIL_NUM)) _itoa(HP.Prof.load((int8_t)pm),strReturn); else strcat(strReturn,"E29");  ADD_WEBDELIM(strReturn) ;    continue;
				}
			}
			// -----------------------------------------------------------------------------
			if (strcmp(str,"infoProfile")==0)  // Функция infoProfile получить информацию о профиле
			{
				if ((pm=my_atof(x))==ATOF_ERROR)  strcat(strReturn,"E29");      // Ошибка преобразования   - завершить запрос с ошибкой
				else
				{
					if ((pm>=0)&&(pm<I2C_PROFIL_NUM)) HP.Prof.get_info(strReturn,(int8_t)pm); else strcat(strReturn,"E29");  ADD_WEBDELIM(strReturn) ;    continue;
				}
			}
			// -----------------------------------------------------------------------------
			if (strcmp(str,"eraseProfile")==0)  // Функция eraseProfile стереть профиль
			{
				if ((pm=my_atof(x))==ATOF_ERROR)  strcat(strReturn,"E29");      // Ошибка преобразования   - завершить запрос с ошибкой
				else
				{
					if (pm==HP.Prof.get_idProfile())  {strcat(strReturn,"E30");}  // попытка стереть текущий профиль
					else if((pm>=0)&&(pm<I2C_PROFIL_NUM)) { _itoa(HP.Prof.erase((int8_t)pm),strReturn); HP.Prof.update_list(HP.Prof.get_idProfile()); }
					else strcat(strReturn,"E29");
					ADD_WEBDELIM(strReturn) ;    continue;
				}
			}
			// -----------------------------------------------------------------------------
			if (strcmp(str,"set_listProf")==0)  // Функция set_listProf загрузить профиль из списка и сразу СОХРАНИТЬ !!!!!!
			{
				if ((pm=my_atof(x))==ATOF_ERROR)  strcat(strReturn,"E29");      // Ошибка преобразования   - завершить запрос с ошибкой
				else {
					if((pm >= 0) && (pm < I2C_PROFIL_NUM)) {
						HP.Prof.set_list((int8_t)pm);
						//HP.save();
						//HP.Prof.save(HP.Prof.get_idProfile());
						HP.Prof.get_list(strReturn/*,HP.Prof.get_idProfile()*/);
					} else strcat(strReturn,"E29");
					ADD_WEBDELIM(strReturn) ;    continue;
				}
			}

			//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			//  2.3 Функции с параметрами
			// проверяем наличие функции set_  конструкция типа (TIN=23)
			STORE_DEBUG_INFO(30);
			if((z=strpbrk(x,"=")) != NULL)  // нашли знак "=" запрос на установку параметра
			{
				*z++ = 0; // '=' -> '\0'
				pm = my_atof(z);
				// формируем начало ответа - повторение запроса без параметра установки  ДЛЯ set_ запросов
				strcat(strReturn,str);
				strcat(strReturn,"(");
				strcat(strReturn,x);
				strcat(strReturn,")=");
				//       { strcat(strReturn,"E04");ADD_WEBDELIM(strReturn);  continue;  }
			} // "=" - не обнаружено, значит значение пустая строка

			// --------------------------------НОВЫЙ ПАРСЕР -------------------------------------------------------------------
			// Вот сюда будет вставлятся код нового парсера (который не будет кодировать параметры в целые числа)
			// ВХОД str - полное имя запроса до (), x - содержит строку (имя параметра), z - после = (значение), pm - флоат z
			// ВЫХОД strReturn  надо Добавлять + в конце &

			// 1. Проверка для запросов содержащих EEV  ----------------------------------------------------
#ifdef EEV_DEF
			if (strcmp(str,"get_pEEV")==0)           // Функция get_pEEV - получить значение параметра ЭРВ
			{
				HP.dEEV.get_paramEEV(x,strReturn);
				ADD_WEBDELIM(strReturn);
				continue;
			} else if (strcmp(str,"set_pEEV")==0)    // Функция set_pEEV - установить значение параметра ЭРВ
			{
				if (pm!=ATOF_ERROR) {   // нет ошибки преобразования
					if (HP.dEEV.set_paramEEV(x,pm)) HP.dEEV.get_paramEEV(x,strReturn);
					else  strcat(strReturn,"E11");  // выход за диапазон значений
				} else strcat(strReturn,"E11");   // ошибка преобразования во флоат
				ADD_WEBDELIM(strReturn) ;
				continue;
			}
#endif
			// 2. Проверка для запросов содержащих MQTT ---------------------------------------------
#ifdef MQTT
			if (strcmp(str,"get_MQTT")==0){           // Функция получить настройки MQTT
				HP.clMQTT.get_paramMQTT(x,strReturn);
				ADD_WEBDELIM(strReturn);
				continue;
			} else if (strcmp(str,"set_MQTT")==0) {         // Функция записать настройки MQTT
				if (HP.clMQTT.set_paramMQTT(x,z))  HP.clMQTT.get_paramMQTT(x,strReturn);   // преобразование удачно
				else strcat(strReturn,"E32") ; // ошибка преобразования строки
				ADD_WEBDELIM(strReturn);
				continue;
			}
#endif
			STORE_DEBUG_INFO(31);

			// 3. Расписание --------------------------------------------------------
			// ошибки: E33 - не верный номер расписания, E34 - не хватает места для календаря
			if(strstr(str,"SCHDLR")) { // Класс Scheduler
				if(strncmp(str, "set", 3) == 0) { // set_SCHDLR(x=n)
					if((i = HP.Schdlr.web_set_param(x, z))) {
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
#ifdef USE_ELECTROMETER_SDM
			if(strcmp(str, "get_SDM") == 0) {          // Функция получить настройки счетчика
				HP.dSDM.get_paramSDM(x, strReturn);
				ADD_WEBDELIM(strReturn); continue;
			} else if(strcmp(str, "set_SDM") == 0) {          // Функция записать настройки счетчика
				if(HP.dSDM.set_paramSDM(x, z)) HP.dSDM.get_paramSDM(x, strReturn); // преобразование удачно
				else strcat(strReturn, "E31");            // ошибка преобразования строки
				ADD_WEBDELIM(strReturn); continue;
			}
#endif
			STORE_DEBUG_INFO(32);

			// 5.  Настройки профилей ---------------------------------------------------------
			if(strcmp(str, "get_Prof") == 0)           // Функция получить настройки профиля
			{
				HP.Prof.get_paramProfile(x, strReturn);
				ADD_WEBDELIM(strReturn); continue;
			} else if(strcmp(str, "set_Prof") == 0)           // Функция записать настройки профиля
			{
				if(HP.Prof.set_paramProfile(x, z)) HP.Prof.get_paramProfile(x, strReturn); // преобразование удачно
				else strcat(strReturn, "E28"); // ошибка преобразования строки
				ADD_WEBDELIM(strReturn); continue;
			}

			//6.  Настройки Уведомлений --------------------------------------------------------
			if (strcmp(str,"get_Message")==0)           // Функция get_Message - получить значение настройки уведомлений
			{
				HP.message.get_messageSetting(x,strReturn);
				ADD_WEBDELIM(strReturn) ; continue;
			} else if (strcmp(str,"set_Message")==0)           // Функция set_Message - установить значениена стройки уведомлений
			{
				if (HP.message.set_messageSetting(x,z)) HP.message.get_messageSetting(x,strReturn); // преобразование удачно
				else strcat(strReturn,"E20") ; // ошибка преобразования строки
				ADD_WEBDELIM(strReturn) ; continue;
			}

			//7.  Настройки бойлера --------------------------------------------------
			if (strcmp(str,"get_Boiler")==0)           // Функция get_Boiler - получить значение настройки бойлера
			{
				HP.Prof.get_boiler(x,strReturn);
				ADD_WEBDELIM(strReturn) ; continue;
			} else if (strcmp(str,"set_Boiler")==0)           // Функция set_Boiler - установить значениена стройки бойлера
			{
				if (HP.Prof.set_boiler(x,z)) HP.Prof.get_boiler(x,strReturn);  // преобразование удачно
				else strcat(strReturn,"E19") ; // ошибка преобразования строки
				ADD_WEBDELIM(strReturn) ; continue;
			}

			//8.  Настройки дата время --------------------------------------------------------
			if (strcmp(str,"get_datetime")==0)           // Функция get_datetim - получить значение даты времени
			{
				STORE_DEBUG_INFO(33);
				HP.get_datetime(x,strReturn);
				ADD_WEBDELIM(strReturn) ; continue;
			} else if (strcmp(str,"set_datetime")==0)           // Функция set_datetime - установить значение даты и времени
			{
				STORE_DEBUG_INFO(34);
				if (HP.set_datetime(x,z))  HP.get_datetime(x,strReturn);    // преобразование удачно
				else  strcat(strReturn,"E18") ; // ошибка преобразования строки
				ADD_WEBDELIM(strReturn) ; continue;
			}

			//9.  Настройки сети -----------------------------------------------------------
			if (strcmp(str,"get_Net")==0)           // Функция get_Network - получить значение параметра Network
			{
				HP.get_network(x,strReturn);
				ADD_WEBDELIM(strReturn) ; continue;
			} else if (strcmp(str,"set_Net")==0)           // Функция set_Network - установить значение паремтра Network
			{
				if (HP.set_network(x,z))  HP.get_network(x,strReturn);     // преобразование удачно
				else strcat(strReturn,"E15") ; // ошибка преобразования строки
				ADD_WEBDELIM(strReturn) ; continue;
			}

			//11.  Графики смещение  используется в одной функции get_Chart -------------------------------------------
			if (strcmp(str,"get_Chart")==0)           // Функция get_Chart - получить график
			{
				HP.get_Chart(x,strReturn);
				ADD_WEBDELIM(strReturn); continue;
			}

			//12.  Частотный преобразователь -----------------------------------------------------------------
			if (strcmp(str,"get_pFC")==0)           // Функция get_paramFC - получить значение параметра FC
			{
				STORE_DEBUG_INFO(35);
				HP.dFC.get_paramFC(x,strReturn);
				ADD_WEBDELIM(strReturn);
				continue;
			} else if (strcmp(str,"set_pFC")==0)           // Функция set_paramFC - установить значение паремтра FC
			{
				STORE_DEBUG_INFO(36);
				if (pm!=ATOF_ERROR) {   // нет ошибки преобразования
					if (HP.dFC.set_paramFC(x,pm)) HP.dFC.get_paramFC(x,strReturn);
					else  strcat(strReturn,"E27");  // выход за диапазон значений
				} else strcat(strReturn,"E11");   // ошибка преобразования во флоат
				ADD_WEBDELIM(strReturn);
				continue;
			}

			// 13 Опции теплового насоса
			if (strcmp(str,"get_oHP")==0)           // Функция get_optionHP - получить значение параметра отопления ТН
			{
				HP.get_optionHP(x,strReturn); ADD_WEBDELIM(strReturn) ; continue;
			} else if (strcmp(str,"set_oHP")==0)           // Функция set_optionHP - установить значение паремтра  опций
			{
				if (pm!=ATOF_ERROR) {   // нет ошибки преобразования
					if (HP.set_optionHP(x,pm))   HP.get_optionHP(x,strReturn);  // преобразование удачно,
					else strcat(strReturn,"E17") ; // выход за диапазон значений
				} else strcat(strReturn,"E11");   // ошибка преобразования во флоат
				ADD_WEBDELIM(strReturn); continue;
			}
			//14.  Параметры  отопления и охлаждения ТН
			if (strcmp(str,"get_Cool")==0)           // Функция get_paramCoolHP - получить значение параметра охлаждения ТН
			{
				HP.Prof.get_paramCoolHP(x,strReturn,HP.dFC.get_present()); ADD_WEBDELIM(strReturn) ; continue;
			} else if (strcmp(str,"set_Cool")==0)           // Функция set_paramCoolHP - установить значение паремтра охлаждения ТН
			{
				if (pm!=ATOF_ERROR) {   // нет ошибки преобразования
					if (HP.Prof.set_paramCoolHP(x,pm))  HP.Prof.get_paramCoolHP(x,strReturn,HP.dFC.get_present());    // преобразование удачно
					else  strcat(strReturn,"E16") ; // ошибка преобразования строки
				} else strcat(strReturn,"E11");   // ошибка преобразования во флоат
				ADD_WEBDELIM(strReturn) ; continue;
			} else if (strcmp(str,"get_Heat")==0)           // Функция get_paramHeatHP - получить значение параметра отопления ТН
			{
				HP.Prof.get_paramHeatHP(x,strReturn,HP.dFC.get_present()); ADD_WEBDELIM(strReturn) ; continue;
			} else if (strcmp(str,"set_Heat")==0)           // Функция set_paramHeatHP - установить значение паремтра отопления ТН
			{
				if (pm!=ATOF_ERROR) {   // нет ошибки преобразования
					if (HP.Prof.set_paramHeatHP(x,pm))  HP.Prof.get_paramHeatHP(x,strReturn,HP.dFC.get_present());    // преобразование удачно
					else  strcat(strReturn,"E16") ; // ошибка преобразования строки
				} else strcat(strReturn,"E11");   // ошибка преобразования во флоат
				ADD_WEBDELIM(strReturn) ; continue;
			}
			STORE_DEBUG_INFO(37);


			// str - полное имя запроса до (), x - содержит строку что между (), z - после =
			// код обработки установки значений модбас
			// get_modbus_val(N:D:X), set_modbus_val(N:D:X=YYY)
			// N - номер устройства, D - тип данных, X - адрес, Y - новое значение
			if(strncmp(str+1, "et_modbus_", 10) == 0) {
				STORE_DEBUG_INFO(38);
				if((y = strchr(x, ':'))) {
					*y++ = '\0';
					uint8_t id = atoi(x);
					uint16_t par = atoi(y + 2); // Передается нумерация регистров с 1, а в modbus с 0
					if(par--) {
						i = OK;
						if(strncmp(str, "set", 3) == 0) {
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
								if((i = Modbus.readHoldingRegisters32(id, par, (uint32_t *)&l_i32)) == OK) _itoa(l_i32, strReturn);
							} else if(*y == 'i') {
								if((i = Modbus.readInputRegistersFloat(id, par, &pm)) == OK) _ftoa(strReturn, pm, 3);
							} else if(*y == 'f') {
								if((i = Modbus.readHoldingRegistersFloat(id, par, &pm)) == OK) _ftoa(strReturn, pm, 3);
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

#ifdef CORRECT_POWER220
			if(strcmp(str,"set_PwrC")==0) { // Функция установки correct_power220
				for(i = 0; i < (int8_t)(sizeof(correct_power220)/sizeof(correct_power220[0])); i++) {
					if(strcmp(x, HP.dRelay[correct_power220[i].num].get_name()) == 0) {
						correct_power220[i].value = pm;
						strcat(strReturn, z);
						break;
					}
				}
				ADD_WEBDELIM(strReturn);
				continue;
			}
#endif

			// --- УДАЛЕННЫЕ ДАТЧИКИ ----------  кусок кода для удаленного датчика - установка параметров ответ - повторение запроса уже сделали
#ifdef SENSOR_IP                           // Получение данных удаленного датчика

			if (strcmp(str,"set_listIP") == 0)    // Удаленные датчики - привязка датчика
			{
				// первое число (имя удаленного датчика)
				if ((x)==NULL)              {strcat(strReturn,"E21" WEBDELIM);continue;}
				if ((a=atoi(x))==0)         {strcat(strReturn,"E22" WEBDELIM);continue;}  // если возвращен 0 то ошибка преобразования
				if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23" WEBDELIM);continue;}  // проверка диапазона номеров датчиков

				// Второе число (новое значение параметра)
				if (z==NULL)             {strcat(strReturn,"E21" WEBDELIM);continue;}
				b=atoi(z);
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

			if (strcmp(str,"set_sensorUseIP") == 0)    // Удаленные датчики - УСТАНОВИТЬ использование удаленного датчика
			{
				// первое число (имя удаленного датчика)
				if ((x)==NULL)              {strcat(strReturn,"E21" WEBDELIM);continue;}
				if ((a=atoi(x))==0)         {strcat(strReturn,"E22" WEBDELIM);continue;}  // если возвращен 0 то ошибка преобразования
				if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23" WEBDELIM);continue;}  // проверка диапазона номеров датчиков
				// Второе число (зновое значение параметра)
				if (z==NULL)             {strcat(strReturn,"E21" WEBDELIM);continue;}
				b=atoi(z);
				if ((b<0)||(b>1))             {strcat(strReturn,"E24" WEBDELIM);continue;}  //  проверка диапазона

				if (b==1) HP.sIP[a-1].set_fUse(true); else HP.sIP[a-1].set_fUse(false);

				HP.updateLinkIP();                 // Обновить ВСЕ привязки удаленных датчиков

				_itoa(HP.sIP[a-1].get_fUse(),strReturn);ADD_WEBDELIM(strReturn) ;continue;
			}

			if (strcmp(str,"set_sensorRuleIP") == 0)    // Удаленные датчики - УСТАНОВИТЬ использование усреднение
			{
				// первое число (имя удаленного датчика)
				if ((x)==NULL)              {strcat(strReturn,"E21" WEBDELIM);continue;}
				if ((a=atoi(x))==0)         {strcat(strReturn,"E22" WEBDELIM);continue;}  // если возвращен 0 то ошибка преобразования
				if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23" WEBDELIM);continue;}  // проверка диапазона номеров датчиков

				if (z==NULL)             {strcat(strReturn,"E21" WEBDELIM);continue;}
				b=atoi(z);
				if ((b<0)||(b>1))             {strcat(strReturn,"E24" WEBDELIM);continue;}  //  проверка диапазона

				if (b==1) HP.sIP[a-1].set_fRule(true); else HP.sIP[a-1].set_fRule(false);
				_itoa(HP.sIP[a-1].get_fRule(),strReturn);ADD_WEBDELIM(strReturn) ;continue;
			}
#endif //  #ifdef SENSOR_IP

			//////////////////////////////////////////// массивы датчиков ////////////////////////////////////////////////
			STORE_DEBUG_INFO(40);

			{ // Массивы датчиков
				int p = -1;
				// Датчики температуры смещение
				if(strstr(str,"Temp"))          // Проверка для запросов содержащих Temp
				{
					for(i=0; i<TNUMBER; i++) if(strcmp(x,HP.sTemp[i].get_name())==0) {p=i; break;} // Поиск среди имен  смещение 0
					if((p<0)||(p>=TNUMBER))  {strcat(strReturn,"E03");ADD_WEBDELIM(strReturn);  continue; }  // Не соответсвие имени функции и параметра
					else  // параметр верный
					{
						 if(strncmp(str,"get_", 4)==0) {              // Функция get_
							str += 4;
							if(strcmp(str,"Temp")==0)              // Функция get_Temp
							{
								if(HP.sTemp[p].get_present() && HP.sTemp[p].get_Temp() != STARTTEMP)  // Если датчик есть в конфигурации то выводим значение
									_dtoa(strReturn, HP.sTemp[p].get_Temp(), 2);
								else strcat(strReturn,"-");             // Датчика нет ставим прочерк
								ADD_WEBDELIM(strReturn); continue;
							} 
							if (strncmp(str,"raw",3)==0)           // Функция get_RawTemp
							{ if(HP.sTemp[p].get_present() && HP.sTemp[p].get_Temp() != STARTTEMP)  // Если датчик есть в конфигурации то выводим значение
									_dtoa(strReturn, HP.sTemp[p].get_rawTemp(), 2);
								else strcat(strReturn,"-");             // Датчика нет ставим прочерк
								ADD_WEBDELIM(strReturn); continue;
							}
							if(strncmp(str, "full", 4) == 0)         // Функция get_fullTemp
							{
								if(HP.sTemp[p].get_present() && HP.sTemp[p].get_Temp() != STARTTEMP) // Если датчик есть в конфигурации то выводим значение
								{
	#ifdef SENSOR_IP
									_ftoa(strReturn, (float) HP.sTemp[p].get_rawTemp() / 100, 2); // Значение проводного датчика вывод
									if((HP.sTemp[p].devIP != NULL) && (HP.sTemp[p].devIP->get_fUse())
											&& (HP.sTemp[p].devIP->get_link() > -1)) // Удаленный датчик привязан к данному проводному датчику надо использовать
									{
										strcat(strReturn, " [");
										_dtoa(strReturn, HP.sTemp[p].get_Temp(), 2);
										strcat(strReturn, "]");
									}
	#else
									if(HP.sTemp[p].get_lastTemp() == STARTTEMP) strcat(strReturn, "-.-");
									else {
										strReturn = dptoa(strReturn + m_strlen(strReturn), HP.sTemp[p].get_Temp(), 2);
										if(*HP.sTemp[p].get_address() == tRadio) *--strReturn = '\0';
									}
	#endif
								} else strcat(strReturn, "-");             // Датчика нет ставим прочерк
								ADD_WEBDELIM(strReturn);
								continue;
							}
 							if(strncmp(str, "min", 3)==0)           // Функция get_minTemp
							{
								if (HP.sTemp[p].get_present()) // Если датчик есть в конфигурации то выводим значение
									_dtoa(strReturn, HP.sTemp[p].get_minTemp(), 2);
								else strcat(strReturn,"-");              // Датчика нет ставим прочерк
								ADD_WEBDELIM(strReturn); continue;
							}

							if(strncmp(str, "max", 3)==0)           // Функция get_maxTemp
							{
								if (HP.sTemp[p].get_present())          // Если датчик есть в конфигурации то выводим значение
									_dtoa(strReturn, HP.sTemp[p].get_maxTemp(), 2);
								else strcat(strReturn,"-");             // Датчика нет ставим прочерк
								ADD_WEBDELIM(strReturn); continue;
							}

							if(strncmp(str, "err", 3)==0)           // Функция get_errTemp
							{ _dtoa(strReturn, HP.sTemp[p].get_errTemp(), 2); ADD_WEBDELIM(strReturn); continue; }

							if(strncmp(str, "aT", 2) == 0)           // Функция get_aTemp (address)
							{
x_get_aTemp:
								if(!HP.sTemp[p].get_fAddress()) strcat(strReturn, "не привязан");
								else if(*HP.sTemp[p].get_address() == tRadio) _itoa(*(uint32_t*)(HP.sTemp[p].get_address() + 1), strReturn);
								else if(*HP.sTemp[p].get_address() > tRadio) _itoa(*(HP.sTemp[p].get_address() + 1), strReturn);
								else strcat(strReturn, addressToHex(HP.sTemp[p].get_address()));
								ADD_WEBDELIM(strReturn); continue;
							}

							if(strncmp(str, "test", 4)==0)           // Функция get_testTemp
							{ _dtoa(strReturn, HP.sTemp[p].get_testTemp(), 2); ADD_WEBDELIM(strReturn); continue; }

							if (strncmp(str, "eT", 2)==0)           // Функция get_eTemp (errcode)
							{ _itoa(HP.sTemp[p].get_lastErr(),strReturn); ADD_WEBDELIM(strReturn); continue; }

							if (strncmp(str, "esT", 3) == 0)           // Функция get_esTemp (errors)
							{ _itoa(HP.sTemp[p].get_sumErrorRead(),strReturn); ADD_WEBDELIM(strReturn); continue; }

							if (strncmp(str, "isT", 3)==0)           // Функция get_isTemp (present)
							{
								if (HP.sTemp[p].get_present()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
								ADD_WEBDELIM(strReturn) ;    continue;
							}
							if(strncmp(str, "nTemp", 5) == 0)           // Функция get_nTemp, если радиодатчик: добавляется уровень сигнала, если get_nTemp2 - +напряжение батарейки
							{
								strcat(strReturn, HP.sTemp[p].get_note());
	#ifdef RADIO_SENSORS
								if(HP.sTemp[p].get_bus() == tRadio_Bus) {
									i = HP.sTemp[p].get_radio_received_idx();
									if(i >= 0) {
										m_snprintf(strReturn + strlen(strReturn), 20, " \xF0\x9F\x93\xB6%c", Radio_RSSI_to_Level(radio_received[i].RSSI));
										if(str[5] == '2') m_snprintf(strReturn + strlen(strReturn), 20, ", %.1dV", radio_received[i].battery);
									} else strcat(strReturn, " \xF0\x9F\x93\xB6");
								}
	#endif
								ADD_WEBDELIM(strReturn); continue;
							}

							if(strncmp(str, "bT", 2) == 0)           // Функция get_bTemp (note)
							{
								if(HP.sTemp[p].get_fAddress()) _itoa(HP.sTemp[p].get_bus() + 1, strReturn); else strcat(strReturn, "-");
								ADD_WEBDELIM(strReturn); continue;
							}

							if(strncmp(str, "fTemp", 5)==0){  // get_fTempX(N): X - номер флага fTEMP_* (1..), N - имя датчика (flag)
								_itoa(HP.sTemp[p].get_setup_flag(str[5] - '0' - 1 + fTEMP_ignory_errors), strReturn);
								ADD_WEBDELIM(strReturn);  continue;
							}

						// ---- SET ----------------- Для температурных датчиков - запросы на УСТАНОВКУ парметров
						} else if(strncmp(str,"set_", 4)==0) {              // Функция set_
							str += 4;
							if(pm == ATOF_ERROR) {   // Ошибка преобразования для чисел - завершить запрос с ошибкой
								strcat(strReturn, "E04");
								ADD_WEBDELIM(strReturn);
								continue;
							}
							if(strncmp(str, "test", 4)==0)           // Функция set_testTemp
							{ 	if (HP.sTemp[p].set_testTemp(rd(pm, 100))==OK)    // Установить значение в сотых градуса
									{ _dtoa(strReturn, HP.sTemp[p].get_testTemp(), 2); ADD_WEBDELIM(strReturn);  continue;  }
								else { strcat(strReturn,"E05" WEBDELIM);  continue;}       // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
							}
							if(strncmp(str, "err", 3)==0)           // Функция set_errTemp
							{ 	if (HP.sTemp[p].set_errTemp(rd(pm, 100))==OK)    // Установить значение в сотых градуса
									{ _dtoa(strReturn, HP.sTemp[p].get_errTemp(), 2); ADD_WEBDELIM(strReturn); continue; }
								else { strcat(strReturn,"E05" WEBDELIM);  continue;}      // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
							}

							if(strncmp(str, "fTemp", 5) == 0) {   // set_fTempX(N=V): X - номер флага fTEMP_* (1..), N - имя датчика (flag)
								i = str[5] - '0' - 1 + fTEMP_ignory_errors;
								HP.sTemp[p].set_setup_flag(i, int(pm));
								_itoa(HP.sTemp[p].get_setup_flag(i), strReturn);
								ADD_WEBDELIM(strReturn); continue;
							}

							if (strncmp(str, "aT", 2)==0)        // Функция set_aTemp (address)
							{
								uint8_t n = pm;
								if(n <= TNUMBER)                  // Если индекс находится в диапазоне допустимых значений Здесь индекс начинается с 1, ЗНАЧЕНИЕ 0 - обнуление адреса!!
								{
									if(n == 0) HP.sTemp[p].set_address(NULL, 0);   // Сброс адреса
									else if(OW_scanTable) HP.sTemp[p].set_address(OW_scanTable[n-1].address, OW_scanTable[n-1].bus);
								}
								goto x_get_aTemp;
							}  // вернуть адрес
						}
						strcat(strReturn,"E08"); // выход за диапазон, значение не установлено
						ADD_WEBDELIM(strReturn);
						continue;

					}  // end else
				} //if ((strstr(str,"Temp")>0)

				// РЕЛЕ
				if(strstr(str,"Relay"))          // Проверка для запросов содержащих Relay
				{
					STORE_DEBUG_INFO(44);
					for(i=0;i<RNUMBER;i++) if(strcmp(x,HP.dRelay[i].get_name())==0) {p=i; break;} // Реле
					if ((p<0)||(p>=RNUMBER))  {strcat(strReturn,"E03");ADD_WEBDELIM(strReturn);  continue; }  // Не соответсвие имени функции и параметра
					else  // параметр верный
					{
						if(strncmp(str,"get_", 4)==0) {              // Функция get_
							str += 4;
							if(strncmp(str, "Relay", 5)==0)           // Функция get_Relay
							{
								if (HP.dRelay[p].get_Relay()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
								ADD_WEBDELIM(strReturn) ;    continue;
							}
							if(strncmp(str, "is", 2)==0)           // Функция get_isRelay
							{
								if (HP.dRelay[p].get_present()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
								ADD_WEBDELIM(strReturn) ;    continue;
							}
							if(strncmp(str, "nR", 2)==0)           // Функция get_nRelay (note)
							{ strcat(strReturn,HP.dRelay[p].get_note()); ADD_WEBDELIM(strReturn); continue; }

							if(strncmp(str, "pin", 3)==0)           // Функция get_pinRelay
							{ strcat(strReturn,"D"); _itoa(HP.dRelay[p].get_pinD(),strReturn); ADD_WEBDELIM(strReturn); continue; }

						// ---- SET ----------------- Для реле - запросы на УСТАНОВКУ парметров
						} else if(strncmp(str,"set_", 4)==0) {              // Функция set_
							str += 4;
							if(pm == ATOF_ERROR) {   // Ошибка преобразования для чисел - завершить запрос с ошибкой
								strcat(strReturn, "E04");
								ADD_WEBDELIM(strReturn);
								continue;
							}
							if(strncmp(str, "Relay", 5) == 0)           // Функция set_Relay
							{
								if(HP.dRelay[p].set_Relay(pm == 0 ? fR_StatusAllOff : fR_StatusManual) == OK) // Установить значение
								{
									if(HP.dRelay[p].get_Relay()) strcat(strReturn, cOne); else strcat(strReturn, cZero);
									ADD_WEBDELIM(strReturn);
									continue;
								} else { // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
									strcat(strReturn, "E05" WEBDELIM);
									continue;
								}
							}
						}
						strcat(strReturn,"E08"); // выход за диапазон, значение не установлено
						ADD_WEBDELIM(strReturn);
						continue;
					}  // else end
				} //if ((strstr(str,"Relay")>0)  5

				// Датчики аналоговые, давления, ТОЧНОСТЬ СОТЫЕ
				if(strstr(str,"Press"))          // Проверка для запросов содержащих Press
				{
					STORE_DEBUG_INFO(41);
					for(i=0;i<ANUMBER;i++) if(strcmp(x,HP.sADC[i].get_name())==0) {p=i; break;} // Поиск среди имен
					if ((p<0)||(p>=ANUMBER))  {strcat(strReturn,"E03");ADD_WEBDELIM(strReturn);  continue; }  // Не соответсвие имени функции и параметра
					else  // параметр верный
					{
						if(strncmp(str,"get_", 4)==0) {              // Функция get_
							str += 4;
							if(strcmp(str,"Press")==0)           // Функция get_Press
							{
								if(HP.sADC[p].get_present())         // Если датчик есть в конфигурации то выводим значение
								{
									_dtoa(strReturn, HP.sADC[p].get_Press(), 2);
	#ifdef EEV_DEF
									if(p < 2) {
										m_snprintf(strReturn + m_strlen(strReturn), 20, " [%.2d°]", PressToTemp(p));
									}
	#endif
								} else strcat(strReturn,"-");             // Датчика нет ставим прочерк
								ADD_WEBDELIM(strReturn); continue;
							}

							if(strncmp(str, "adc", 3)==0)           // Функция get_adcPress
							{ _itoa(HP.sADC[p].get_lastADC(),strReturn); ADD_WEBDELIM(strReturn); continue; }

							if(strncmp(str, "min", 3)==0)           // Функция get_minPress
							{
								if (HP.sADC[p].get_present())          // Если датчик есть в конфигурации то выводим значение
x_get_minPress: 					_dtoa(strReturn, HP.sADC[p].get_minPress(), 2);
								else strcat(strReturn,"-");              // Датчика нет ставим прочерк
								ADD_WEBDELIM(strReturn); continue;
							}

							if(strncmp(str, "max", 3)==0)           // Функция get_maxPress
							{
								if (HP.sADC[p].get_present())           // Если датчик есть в конфигурации то выводим значение
x_get_maxPress: 					_dtoa(strReturn, HP.sADC[p].get_maxPress(), 2);
								else strcat(strReturn,"-");               // Датчика нет ставим прочерк
								ADD_WEBDELIM(strReturn); continue;
							}

							if(strncmp(str, "zero", 4)==0)           // Функция get_zeroPress
							{ _itoa(HP.sADC[p].get_zeroPress(),strReturn); ADD_WEBDELIM(strReturn); continue; }

							if(strncmp(str, "trans", 5)==0)           // Функция get_transPress
							{ _dtoa(strReturn, HP.sADC[p].get_transADC(), 3); ADD_WEBDELIM(strReturn); continue; }

							if(strncmp(str, "pin", 3)==0)           // Функция get_pinPress
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

							if(strncmp(str, "test", 4)==0)           // Функция get_testPress
							{ _dtoa(strReturn, HP.sADC[p].get_testPress(), 2); ADD_WEBDELIM(strReturn); continue; }

							if(strncmp(str, "eP", 2)==0)           // Функция get_ePress (errorcode)
							{ _itoa(HP.sADC[p].get_lastErr(),strReturn); ADD_WEBDELIM(strReturn); continue; }

							if(strncmp(str, "isP", 3)==0)           // Функция get_isPress
							{
								if (HP.sADC[p].get_present()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
								ADD_WEBDELIM(strReturn) ;    continue;
							}
							if(strncmp(str, "nP", 2)==0)           // Функция get_nPress (note)
							{ strcat(strReturn,HP.sADC[p].get_note()); ADD_WEBDELIM(strReturn); continue; }

						// ---- SET ----------------- Для аналоговых  датчиков - запросы на УСТАНОВКУ парметров
						} else if(strncmp(str,"set_", 4)==0) {              // Функция set_
							str += 4;
							if(pm == ATOF_ERROR) {   // Ошибка преобразования для чисел - завершить запрос с ошибкой
								strcat(strReturn, "E04");
								ADD_WEBDELIM(strReturn);
								continue;
							}
							if(strncmp(str, "test", 4) == 0)           // Функция set_testPress
							{
								if(HP.sADC[p].set_testPress(rd(pm, 100)) == OK)    // Установить значение
								{
									_dtoa(strReturn, HP.sADC[p].get_testPress(), 2);
									ADD_WEBDELIM(strReturn); continue;
								} else {// выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
									strcat(strReturn, "E05" WEBDELIM);	continue;
								}
							}
							if(strncmp(str, "min", 3) == 0) {  // set_minPress
								if(HP.sADC[p].set_minPress(rd(pm, 100)) == OK) goto x_get_minPress;
								else { strcat(strReturn, "E05" WEBDELIM);  continue; }         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
							}
							if(strncmp(str, "max", 3) == 0) { // set_maxPress
								if(HP.sADC[p].set_maxPress(rd(pm, 100)) == OK) goto x_get_maxPress;
								else {  strcat(strReturn, "E05" WEBDELIM); continue;  }         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
							}

							if(strncmp(str, "trans", 5)==0)           // Функция set_transPress float
							{ if (HP.sADC[p].set_transADC(pm)==OK)    // Установить значение
								{_dtoa(strReturn, HP.sADC[p].get_transADC(), 3); ADD_WEBDELIM(strReturn); continue;}
								else { strcat(strReturn,"E05" WEBDELIM);  continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
							}

							if(strncmp(str, "zero", 4) == 0)           // Функция set_zeroPress
							{
								if(HP.sADC[p].set_zeroPress((int16_t) pm) == OK)    // Установить значение
								{
									_itoa(HP.sADC[p].get_zeroPress(), strReturn);
									ADD_WEBDELIM(strReturn);
									continue;
								} else {   // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
									strcat(strReturn, "E05" WEBDELIM);	continue;
								}
							}
						}
						strcat(strReturn,"E08"); // выход за диапазон, значение не установлено
						ADD_WEBDELIM(strReturn);
						continue;

					}  // end else
				} //if ((strstr(str,"Press")>0)

				// Частотные датчики ДАТЧИКИ ПОТОКА
				if(strstr(str,"Flow"))          // Проверка для запросов содержащих Frequency
				{
					STORE_DEBUG_INFO(43);
					for(i=0;i<FNUMBER;i++) if(strcmp(x,HP.sFrequency[i].get_name())==0) {p=i; break;} // Частотные датчики
					if ((p<0)||(p>=FNUMBER))  {strcat(strReturn,"E03");ADD_WEBDELIM(strReturn);  continue; }  // Не соответсвие имени функции и параметра
					else  // параметр верный
					{
						if(strncmp(str,"get_", 4)==0) {              // Функция get_
							str += 4;
							if(strncmp(str, "Flow", 4)==0)           // Функция get_Flow
							{
								if (HP.sFrequency[p].get_present())          // Если датчик есть в конфигурации то выводим значение
									_dtoa(strReturn, HP.sFrequency[p].get_Value(), 3);
								else strcat(strReturn,"-");               // Датчика нет ставим прочерк
								ADD_WEBDELIM(strReturn) ;    continue;
							}
							if(strncmp(str, "frF", 3)==0)           // Функция get_frFlow
							{
								if (HP.sFrequency[p].get_present())          // Если датчик есть в конфигурации то выводим значение
									_dtoa(strReturn, HP.sFrequency[p].get_Frequency(), 3);
								else strcat(strReturn,"-");               // Датчика нет ставим прочерк
								ADD_WEBDELIM(strReturn) ;    continue;
							}
							if(strncmp(str, "min", 3)==0)           // Функция get_minFlow
							{
								if (HP.sFrequency[p].get_present())          // Если датчик есть в конфигурации то выводим значение
									_dtoa(strReturn, HP.sFrequency[p].get_minValue() / 100, 1);
								else strcat(strReturn,"-");               // Датчика нет ставим прочерк
								ADD_WEBDELIM(strReturn) ;    continue;
							}
							if(strncmp(str, "kfF", 3)==0)           // Функция get_kfFlow
							{
								if (HP.sFrequency[p].get_present())          // Если датчик есть в конфигурации то выводим значение
									_dtoa(strReturn, HP.sFrequency[p].get_kfValue(), 2);
								else strcat(strReturn,"-");               // Датчика нет ставим прочерк
								ADD_WEBDELIM(strReturn) ;    continue;
							}
							if(strncmp(str, "cF", 2)==0)           // Функция get_cFlow (capacity)
							{
								if (HP.sFrequency[p].get_present())        // Если датчик есть в конфигурации то выводим значение
									_itoa(HP.sFrequency[p].get_Capacity(),strReturn);
								else strcat(strReturn,"-");               // Датчика нет ставим прочерк
								ADD_WEBDELIM(strReturn) ;    continue;
							}
							if(strncmp(str, "test", 4)==0)           // Функция get_testFlow
							{
								if (HP.sFrequency[p].get_present())          // Если датчик есть в конфигурации то выводим значение
									_dtoa(strReturn, HP.sFrequency[p].get_testValue(), 3);
								else strcat(strReturn,"-");               // Датчика нет ставим прочерк
								ADD_WEBDELIM(strReturn) ;    continue;
							}
							if(strncmp(str, "pin", 3)==0)              // Функция get_pinFlow
							{ strcat(strReturn,"D"); _itoa(HP.sFrequency[p].get_pinF(),strReturn);
								ADD_WEBDELIM(strReturn); continue; }
							if(strncmp(str, "eF", 2)==0)           // Функция get_eFlow (errorcode)
							{ _itoa(HP.sFrequency[p].get_lastErr(),strReturn);
								ADD_WEBDELIM(strReturn); continue; }
							if(strncmp(str, "nF", 2)==0)               // Функция get_nFlow (note)
							{ strcat(strReturn,HP.sFrequency[p].get_note()); ADD_WEBDELIM(strReturn); continue; }

							if(strncmp(str, "check", 5)==0) // get_checkFlow
							{ _itoa(HP.sFrequency[p].get_checkFlow(), strReturn); ADD_WEBDELIM(strReturn); continue; }

						// ---- SET ----------------- Для частотных  датчиков - запросы на УСТАНОВКУ парметров
						} else if(strncmp(str, "set_", 4)==0) {              // Функция set_
							str += 4;
							if(pm == ATOF_ERROR) {   // Ошибка преобразования для чисел - завершить запрос с ошибкой
								strcat(strReturn, "E04");
								ADD_WEBDELIM(strReturn);
								continue;
							}
							if(strncmp(str, "min", 3)==0) {           // Функция set_minFlow
								HP.sFrequency[p].set_minValue(pm);
								_dtoa(strReturn, HP.sFrequency[p].get_minValue() / 100, 1); ADD_WEBDELIM(strReturn); continue;
							}
							if(strncmp(str, "check", 5)==0) {           // Функция set_checkFlow
								HP.sFrequency[p].set_checkFlow(pm != 0);
								_itoa(HP.sFrequency[p].get_checkFlow(), strReturn); ADD_WEBDELIM(strReturn); continue;
							}
							if(strncmp(str, "test", 4)==0)           // Функция set_testFlow
							{ if (HP.sFrequency[p].set_testValue(rd(pm, 1000))==OK)    // Установить значение
								{_dtoa(strReturn, HP.sFrequency[p].get_testValue(), 3); ADD_WEBDELIM(strReturn); continue; }
								else { strcat(strReturn,"E35" WEBDELIM); continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
							}
							if(strncmp(str, "kfF", 3)==0)           // Функция set_kfFlow float
							{ HP.sFrequency[p].set_kfValue(rd(pm, 100));    // Установить значение
								_dtoa(strReturn, HP.sFrequency[p].get_kfValue(), 2); ADD_WEBDELIM(strReturn); continue;
							}
							if(strncmp(str, "cF", 2)==0)           // Функция set_cFlow float (capacity)
							{
								if (HP.sFrequency[p].set_Capacity(pm)==OK)    // Установить значение
								{  _itoa(HP.sFrequency[p].get_Capacity(),strReturn);  ADD_WEBDELIM(strReturn); continue;}
								else { strcat(strReturn,"E35" WEBDELIM);  continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
							}
						}
						strcat(strReturn,"E08"); // выход за диапазон, значение не установлено
						ADD_WEBDELIM(strReturn);
						continue;
					}  // else end
				} //if ((strstr(str,"Flow")>0)

				//  Датчики сухой контакт
				if(strstr(str,"Input"))          // Проверка для запросов содержащих Input
				{
					STORE_DEBUG_INFO(42);
					for(i=0;i<INUMBER;i++) if(strcmp(x,HP.sInput[i].get_name())==0) {p=i; break;} // Поиск среди имен
					if ((p<0)||(p>=INUMBER))  {strcat(strReturn,"E03");ADD_WEBDELIM(strReturn);  continue; }  // Не соответсвие имени функции и параметра
					else  // параметр верный
					{
						if(strncmp(str,"get_", 4)==0) {              // Функция get_
							str += 4;
							if(strcmp(str,"Input")==0)           // Функция get_Input
							{
								if (HP.sInput[p].get_present())          // Если датчик есть в конфигурации то выводим значение
								{ if (HP.sInput[p].get_Input()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);}
								else strcat(strReturn,"-");               // Датчика нет ставим прочерк
								ADD_WEBDELIM(strReturn) ;    continue;
							}
							if(strncmp(str, "isI", 3)==0)           // Функция get_isInput
							{
								if (HP.sInput[p].get_present()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
								ADD_WEBDELIM(strReturn) ;    continue;
							}
							if(strncmp(str, "nI", 2)==0)           // Функция get_nInput (note)
							{ strcat(strReturn,HP.sInput[p].get_note()); ADD_WEBDELIM(strReturn); continue; }

							if(strncmp(str, "test", 4)==0)           // Функция get_testInput
							{
								if (HP.sInput[p].get_testInput()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
								ADD_WEBDELIM(strReturn) ;    continue;
							}

							if(strncmp(str, "alarm", 5)==0)           // Функция get_alarmInput
							{
								if (HP.sInput[p].get_alarmInput()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
								ADD_WEBDELIM(strReturn) ;    continue;
							}
							if(strncmp(str, "eI", 2)==0)           // Функция get_eInput (errorcode)
							{ _itoa(HP.sInput[p].get_lastErr(),strReturn); ADD_WEBDELIM(strReturn); continue; }

							if(strncmp(str, "pin", 3)==0)           // Функция get_pinInput
							{ strcat(strReturn,"D"); _itoa(HP.sInput[p].get_pinD(),strReturn);
								ADD_WEBDELIM(strReturn); continue; }
							if(strncmp(str, "type", 4)==0)           // Функция get_typeInput
							{
								if (HP.sInput[p].get_present()==true) {  // датчик есть в кнфигурации
									switch((int)HP.sInput[p].get_typeInput())
									{
										case pALARM: strcat(strReturn,"Alarm"); break;                // 0 Аварийный датчик, его срабатываение приводит к аварии и останове Тн
										case pSENSOR:strcat(strReturn,"Work");  break;                // 1 Обычный датчик, его значение используется в алгоритмах ТН
										case pPULSE: strcat(strReturn,"Pulse"); break;                // 2 Импульсный висит на прерывании и считает частоты - выходная величина ЧАСТОТА
										default:strcat(strReturn,"err_type"); break;                  // Ошибка??
									}
								} else strcat(strReturn,"none");                                 // датчик отсутвует
								ADD_WEBDELIM(strReturn); continue;
							}

					// ---- SET ----------------- Для датчиков сухой контакт - запросы на УСТАНОВКУ парметров
						} else if(strncmp(str,"set_", 4)==0) {              // Функция set_
							str += 4;
							if(pm == ATOF_ERROR) {   // Ошибка преобразования для чисел - завершить запрос с ошибкой
								strcat(strReturn, "E04");
								ADD_WEBDELIM(strReturn);
								continue;
							}
							if(strncmp(str, "test", 4)==0)           // Функция set_testInput
							{ if (HP.sInput[p].set_testInput((int16_t)pm)==OK)    // Установить значение
								{ if (HP.sInput[p].get_testInput()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero); ADD_WEBDELIM(strReturn); continue; }
								else { strcat(strReturn,"E05" WEBDELIM);  continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
							}
							if(strncmp(str, "alarm", 5)==0)           // Функция set_alarmInput
							{ if (HP.sInput[p].set_alarmInput((int16_t)pm)==OK)    // Установить значение
								{ if (HP.sInput[p].get_alarmInput()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero); ADD_WEBDELIM(strReturn); continue; }
								else { strcat(strReturn,"E05" WEBDELIM); continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
							}
						}
						strcat(strReturn,"E08"); // выход за диапазон, значение не установлено
						ADD_WEBDELIM(strReturn);
						continue;
					}  // else end
				} //if ((strstr(str,"Input")>0)

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
	STORE_DEBUG_INFO(45);
	ADD_WEBDELIM(strReturn) ; // двойной знак закрытие посылки
}

// ===============================================================================================================
// Выделение имени файла (или содержания запроса) и типа файла и типа запроса клиента
// thread - номер потока, возсращает тип запроса
uint16_t GetRequestedHttpResource(uint8_t thread)
{
	STORE_DEBUG_INFO(50);
	if((HP.get_fPass()) && (!HP.safeNetwork))  // идентификация если установлен флаг и перемычка не в нуле
	{
		//Serial.print("\n"); Serial.print((char*)Socket[thread].inBuf); Serial.print("\n");
		char *str = strstr((char*)Socket[thread].inBuf, header_Authorization_1);
		if(str) str += sizeof(header_Authorization_1) - 1;
		else if((str = strstr((char*)Socket[thread].inBuf, header_Authorization_2))) str += sizeof(header_Authorization_2) - 1;
		if(str) {
			if(strncmp(str, HP.Security.hashAdmin, HP.Security.hashAdminLen) == 0) goto x_ok;
			else if(strncmp(str, HP.Security.hashUser, HP.Security.hashUserLen) == 0 || !*HP.get_passUser()) SETBIT1(Socket[thread].flags, fUser); else return BAD_LOGIN_PASS;
		} else if(!*HP.get_passUser()) SETBIT1(Socket[thread].flags, fUser); else return UNAUTHORIZED;
	}
x_ok:

	// Идентификация пройдена
	//if(strstr((char*)Socket[thread].inBuf,"Access-Control-Request-Method: POST")) {request_type = HTTP_POST_; return request_type; }  //обработка предваритаельного запроса перед получением файла
	char *str_token, *tmpptr;
	str_token = strtok_r((char*) Socket[thread].inBuf, " ", &tmpptr);    // Обрезаем по пробелам
	if(strcmp(str_token, "GET") == 0)   // Ищем GET
	{
		str_token = strtok_r(NULL, " ", &tmpptr);                       // get the file name
		if(strcmp(str_token, "/") == 0)                   // Имени файла нет, берем файл по умолчанию
		{
			Socket[thread].inPtr = (char*) INDEX_FILE;      // Указатель на имя файла по умолчанию
			return HTTP_GET;
		} else if(strcmp(str_token, (char*) MOB_PATH) == 0) // Имени файла нет, но указан путь до мобильной морды
		{
			Socket[thread].inPtr = (char*) (str_token + 1);   // Указатель на путь до мобильной морды
			strcat(Socket[thread].inPtr, (char*) INDEX_MOB_FILE);
			return HTTP_GET;
		} else if(strlen(str_token) <= W5200_MAX_LEN - 100)   // Проверка на длину запроса или имени файла
		{
			Socket[thread].inPtr = (char*) (str_token + 1);       // Указатель на имя файла
			if(Socket[thread].inPtr[0] == '&') return HTTP_REQEST;       // Проверка на аякс запрос
			return HTTP_GET;
		} // if ((len=strlen(str_token)) <= W5200_MAX_LEN-100)
		else return HTTP_invalid;  // слишком длинная строка HTTP_invalid
	}   //if (strcmp(str_token, "GET") == 0)
	else if(strcmp(str_token, "POST") == 0) {Socket[thread].inPtr = (char*) (str_token +strlen("POST") + 1);  return HTTP_POST;}    // Запрос POST Socket[thread].inPtr - указывает на начало запроса (начало полезных данных)
	else if(strcmp(str_token, "OPTIONS") == 0) return HTTP_POST_;
	return HTTP_invalid;
}

// ========================== P A R S E R  P O S T =================================
#define emptyStr			WEB_HEADER_END  	   // пустая строка после которой начинаются данные
#define MAX_FILE_LEN		64  	              // максимальная длина имени файла
const char Title[]          = "Title: ";          // где лежит имя файла
const char Length[]         = "Content-Length: "; // где лежит длина файла
const char SETTINGS[]       = "*SETTINGS*";       // Идентификатор передачи настроек (лежит в Title:)
const char LOAD_FLASH_START[]= "*SPI_FLASH*";     // Идентификатор начала загрузки веб морды в SPI Flash (лежит в Title:)
const char LOAD_FLASH_END[]  = "*SPI_FLASH_END*"; // Идентификатор колнца загрузки веб морды в SPI Flash (лежит в Title:)
const char LOAD_SD_START[]   = "*SD_CARD*";       // Идентификатор начала загрузки веб морды в SD (лежит в Title:)
const char LOAD_SD_END[]     = "*SD_CARD_END*";   // Идентификатор колнца загрузки веб морды в SD (лежит в Title:)

uint16_t numFilesWeb = 0;                   // Число загруженных файлов

// Разбор и обработка POST запроса inPtr входная строка использует outBuf для хранения файла настроек!
// Сейчас реализована загрузка настроек и загрузка веб морды в спи диск
// Возврат тип ответа (потом берется из массива строк)
TYPE_RET_POST parserPOST(uint8_t thread, uint16_t size)
{
	byte *ptr, *pStart;
	char *nameFile;      // указатель имя файла
	int32_t buf_len, lenFile;

	//journal.printf(" POST =>"); journal.printf("%s\n", Socket[thread].inPtr); if(strlen(Socket[thread].inPtr) >= PRINTF_BUF) journal.printf("%s\n", Socket[thread].inPtr + PRINTF_BUF - 1);
	STORE_DEBUG_INFO(51);

	// Поиски во входном буфере: данных, имени файла и длины файла
	ptr = (byte*) strstr(Socket[thread].inPtr, emptyStr) + sizeof(emptyStr) - 1;    // поиск начала даных

	if((nameFile = strstr(Socket[thread].inPtr, Title)) == NULL) { // Имя файла не найдено, запрос не верен, выходим
		journal.jprintf("Upload: Name not found!\n");
		return pLOAD_ERR;
	}
	nameFile += sizeof(Title) - 1;
	char *tmp = strchr(nameFile, '\r');
	if(tmp == NULL || tmp - nameFile >= MAX_FILE_LEN) {
		nameFile[MAX_FILE_LEN] = '\0';
		journal.jprintf("Upload: %s name length > %d bytes!\n", nameFile, MAX_FILE_LEN - 1);
		return pLOAD_ERR;
	}
	pStart = (byte*) strstr(Socket[thread].inPtr, Length);
	if(pStart == NULL) { // Размер файла не найден, запрос не верен, выходим
		journal.jprintf("Upload: %s - length not found!\n", nameFile);
		return pLOAD_ERR;
	}
	pStart += sizeof(Length) - 1;
	*tmp = '\0';
	urldecode(Socket[thread].outBuf, nameFile, 128);
	nameFile = Socket[thread].outBuf;
	tmp = strchr((char*) pStart, '\r');
	if(tmp) {
		*tmp = '\0';
		lenFile = atoi((char*) pStart);	// получить длину
	} else lenFile = 0;
	// все нашлось, можно обрабатывать
	buf_len = size - (ptr - (byte *) Socket[thread].inBuf);                  // длина (остаток) данных (файла) в буфере
	// В зависимости от имени файла (Title)
	if(strcmp(nameFile, SETTINGS) == 0) {  // Чтение настроек
		STORE_DEBUG_INFO(52);
		int32_t len;
		// Определение начала данных (поиск HEADER_BIN)
		pStart=(byte*)strstr((char*) ptr, HEADER_BIN);    // Поиск заголовка
		if( pStart== NULL || lenFile == 0) {              // Заголовок не найден
			journal.jprintf("Upload: Wrong save format: %s!\n", nameFile);
			return pSETTINGS_ERR;
		}
		len=pStart+sizeof(HEADER_BIN) - (byte*) Socket[thread].inBuf-1;         // размер текстового заголовка в буфере до окончания HEADER_BIN, дальше идут бинарные данные
		buf_len = size - len;                                                   // определяем размер бинарных данных в первом пакете 
		memcpy(Socket[thread].outBuf, pStart+sizeof(HEADER_BIN)-1, buf_len);    // копируем бинарные данные в буфер, без заголовка!
	    lenFile=lenFile-len;                                                    // корректируем длину файла на длину заголовка (только бинарные данные)
		while(buf_len < lenFile)  // Чтение остальных бинарных данных по сети
		{
			for(uint8_t i=0;i<20;i++) if(!Socket[thread].client.available()) _delay(1);else break; // ждем получние пакета до 20 мсек (может быть плохая связь)
			if(!Socket[thread].client.available()) break;                                          // пакета нет - выходим
			len = Socket[thread].client.get_ReceivedSizeRX();                                      // получить длину входного пакета
			if(len > W5200_MAX_LEN - 1) len = W5200_MAX_LEN - 1;                                   // Ограничить размером в максимальный размер пакета w5200
			Socket[thread].client.read(Socket[thread].inBuf, len);                                 // прочитать буфер
			if(buf_len + len >= (int32_t) sizeof(Socket[thread].outBuf)) return pSETTINGS_MEM;     // проверить длину если не влезает то выходим
			memcpy(Socket[thread].outBuf + buf_len, Socket[thread].inBuf, len);                    // Добавить пакет в буфер
			buf_len = buf_len + len;                                                               // определить размер данных
		}
	    ptr = (byte*) Socket[thread].outBuf;     
		journal.jprintf("Loading %s, length data %d bytes:\n", SETTINGS, buf_len);
		// Чтение настроек из ptr
		len = HP.load(ptr, 1);
		if(len <= 0) return pSETTINGS_ERR; // ошибка загрузки настроек
		boolean ret = true;
		// Чтение профиля
		ptr += len;
		if(HP.Prof.loadFromBuf(0, ptr) != OK) ret = false;   // чтение профиля
		if(HP.Schdlr.loadFromBuf(ptr + HP.Prof.get_lenProfile()) != OK) ret = false;  // чтения расписания
		HP.Prof.update_list(HP.Prof.get_idProfile());             // обновить список
		if(ret) return pSETTINGS_OK;
		else return pSETTINGS_ERR;
	} //if (strcmp(nameFile,"*SETTINGS*")==0)

	// загрузка вебморды
	 else if(HP.get_fSPIFlash() || HP.get_fSD())  { // если есть куда писать
		STORE_DEBUG_INFO(53);
		if(strcmp(nameFile, LOAD_FLASH_START) == 0) {  // начало загрузки вебморды в SPI Flash
			if(!HP.get_fSPIFlash()) {
				journal.jprintf("Upload: No SPI Flash installed!\n");
				SemaphoreGive (xLoadingWebSemaphore);
				return pLOAD_ERR;
			}
			fWebUploadingFilesTo = 1;
			if(SemaphoreTake(xLoadingWebSemaphore, 10) == pdFALSE) {
				journal.jprintf("%s: Upload already started\n", (char*) __FUNCTION__);
				SemaphoreGive (xLoadingWebSemaphore);
				return pLOAD_ERR;
			}
			numFilesWeb = 0;
			journal.jprintf_time("Start upload, erase SPI disk ");
			SerialFlash.eraseAll();
			while(SerialFlash.ready() == false) {
				xSemaphoreGive(xWebThreadSemaphore); // отдать семафор вебморды, что бы обработались другие потоки веб морды
				vTaskDelay(1000 / portTICK_PERIOD_MS);
				if(SemaphoreTake(xWebThreadSemaphore, (3 * W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE) { // получить семафор веб морды
					journal.jprintf("%s: Socket %d %s\n", (char*) __FUNCTION__, Socket[thread].sock, MutexWebThreadBuzy);
					return pLOAD_ERR;
				} // если не удается захватить мютекс, то ошибка и выход
				journal.jprintf(".");
			}
			journal.jprintf(" Ok, free %d bytes\n", SerialFlash.free_size());
			return pNULL;
		} else if(strcmp(nameFile, LOAD_SD_START) == 0) {  // начало загрузки вебморды в SD
			if(!HP.get_fSD()) {
				journal.jprintf("Upload: No SD card available!\n");
				SemaphoreGive (xLoadingWebSemaphore);
				return pLOAD_ERR;
			}
			fWebUploadingFilesTo = 2;
			if(SemaphoreTake(xLoadingWebSemaphore, 10) == pdFALSE) {
				journal.jprintf("%s: Upload already started\n", (char*) __FUNCTION__);
				SemaphoreGive (xLoadingWebSemaphore);
				return pLOAD_ERR;
			}
			numFilesWeb = 0;
			journal.jprintf_time("Start upload to SD.\n");
			return pNULL;
		} else if(strcmp(nameFile, LOAD_FLASH_END) == 0 || strcmp(nameFile, LOAD_SD_END) == 0) {  // Окончание загрузки вебморды
			if(SemaphoreTake(xLoadingWebSemaphore, 0) == pdFALSE) { // Семафор не захвачен (был захвачен ранее) все ок
				journal.jprintf_time("Ok, %d files uploaded, free %.1f KB\n", numFilesWeb, fWebUploadingFilesTo == 1 ? (float)SerialFlash.free_size() / 1024 : (float)card.vol()->freeClusterCount() * card.vol()->blocksPerCluster() * 512 / 1024);
				fWebUploadingFilesTo = 0;
				SemaphoreGive (xLoadingWebSemaphore);
				return pLOAD_OK;
			} else { 	// семафор БЫЛ захвачен, ошибка, отдать обратно
				journal.jprintf("%s: Unable to finish upload!\n", (char*) __FUNCTION__);
				fWebUploadingFilesTo = 0;
				SemaphoreGive (xLoadingWebSemaphore);
				return pLOAD_ERR;
			}
		} else { // загрузка отдельных файлов веб морды
			if(SemaphoreTake(xLoadingWebSemaphore, 0) == pdFALSE) { // Cемафор занят - загрузка файла
				if(lenFile == 0) {
					journal.jprintf("Upload: %s length = %s!\n", nameFile, pStart);
					return pLOAD_ERR;
				}
				// Файл может лежать во множестве пакетов. Если в SPI Flash, то считается что spi диск отформатирован и ожидает запись файлов с "нуля"
				// Входные параметры:
				// nameFile - имя файла
				// lenFile - общая длина файла
				// thread - поток веб сервера,котрый обрабатывает post запрос
				// ptr - указатель на начало данных (файла) в буфере Socket[thread].inPtr.
				// buf_len - размер данных в буфере ptr (по сети осталось принять lenFile-buf_len)
				if(fWebUploadingFilesTo == 1) {
					uint16_t numPoint = 0;
					int32_t loadLen; // Обработанная (загруженная) длина
					STORE_DEBUG_INFO(54);
					journal.jprintf("%s (%d) ", nameFile, lenFile);
					loadLen = SerialFlash.free_size();
					if(lenFile > loadLen) {
						journal.jprintf("Not enough space, free: %d\n", loadLen);
						loadLen = 0;
					} else {
						loadLen = SerialFlash.create(nameFile, lenFile);
						if(loadLen == 0) {
							SerialFlashFile ff = SerialFlash.open(nameFile);
							if(ff) {
								if(buf_len > 0) loadLen = ff.write(ptr, buf_len); // первый пакет упаковали если он не нулевой
								while(loadLen < lenFile)  // Чтение остальных пакетов из сети
								{
									_delay(2);                                                 // время на приход данных
									buf_len = Socket[thread].client.get_ReceivedSizeRX(); // получить длину входного пакета
									if(buf_len == 0) {
										if(Socket[thread].client.connected()) continue;	else break;
									}
									//      if(len>W5200_MAX_LEN-1) len=W5200_MAX_LEN-1;                             // Ограничить размером в максимальный размер пакета w5200
									Socket[thread].client.read(Socket[thread].inBuf, buf_len);        // прочитать буфер
									loadLen = loadLen + ff.write(Socket[thread].inBuf, buf_len);             // записать
									numPoint++;
									if(numPoint >= 20) {                   // точка на 30 кб приема (20 пакетов по 1540)
										numPoint = 0;
										journal.jprintf(".");
									}
								}
								ff.close();
								if(loadLen == lenFile) journal.jprintf("Ok\n");
								else { // Длины не совпали
									journal.jprintf("%db, Error length!\n", loadLen);
									loadLen = 0;
								}
							} else journal.jprintf("Error Open!\n");
						} else journal.jprintf("Error Create %d!\n", loadLen);
					}
					if(loadLen == lenFile) {
						numFilesWeb++;
						return pNULL;
					} else {
						SemaphoreGive (xLoadingWebSemaphore);
						return pLOAD_ERR;
					}
				} else if(fWebUploadingFilesTo == 2) { // Запись на SD,
					STORE_DEBUG_INFO(54);
					journal.jprintf("%s (%d) ", nameFile, lenFile);
					for(uint16_t _timeout = 0; _timeout < 2000 && card.card()->isBusy(); _timeout++) _delay(1);
					if(wFile.opens(nameFile, O_CREAT | O_TRUNC | O_RDWR, &wfname)) {
						wFile.timestamp(T_CREATE | T_ACCESS | T_WRITE, rtcSAM3X8.get_years(), rtcSAM3X8.get_months(), rtcSAM3X8.get_days(), rtcSAM3X8.get_hours(), rtcSAM3X8.get_minutes(), rtcSAM3X8.get_seconds());
						if((int32_t)wFile.write(ptr, buf_len) != buf_len) {
							journal.jprintf("Error write file %s (%d,%d)!\n", nameFile, card.cardErrorCode(), card.cardErrorData());
						} else {
							uint16_t numPoint = 0;
							while((lenFile -= buf_len) > 0)  // Чтение остальных пакетов из сети
							{
								_delay(2);                                                                 // время на приход данных
								buf_len = Socket[thread].client.get_ReceivedSizeRX();                  // получить длину входного пакета
								if(buf_len == 0) {
									if(Socket[thread].client.connected()) continue; else break;
								}
								Socket[thread].client.read(Socket[thread].inBuf, buf_len);                      // прочитать буфер
								STORE_DEBUG_INFO(56);
								for(uint16_t _timeout = 0; _timeout < 2000 && card.card()->isBusy(); _timeout++) _delay(1);
								if((int32_t)wFile.write(Socket[thread].inBuf, buf_len) != buf_len) {
									journal.jprintf("Error write file %s (%d,%d)!\n", nameFile, card.cardErrorCode(), card.cardErrorData());
									break;
								}
								STORE_DEBUG_INFO(57);
								if(++numPoint >= 20) {// точка на 30 кб приема (20 пакетов по 1540)
									numPoint = 0;
									journal.jprintf(".");
								}
							}
						}
						STORE_DEBUG_INFO(58);
						if(!wFile.close()) {
							journal.jprintf("Error close file (%d,%d)!\n", card.cardErrorCode(), card.cardErrorData());
						}
						if(lenFile == 0) journal.jprintf("Ok\n"); else journal.jprintf("Error - rest %d!\n", lenFile);
					} else journal.jprintf("Error create (%d,%d)!\n", card.cardErrorCode(), card.cardErrorData());
					if(lenFile == 0) {
						numFilesWeb++;
						return pNULL;
					} else {
						SemaphoreGive (xLoadingWebSemaphore);
						return pLOAD_ERR;
					}
				}
			} else { // семафор БЫЛ захвачен, ошибка, отдать обратно
				uint8_t ip[4];
				W5100.readSnDIPR(Socket[thread].sock, ip);
				journal.jprintf("Unable to upload file %s (%d.%d.%d.%d)!\n", nameFile, ip[0], ip[1], ip[2], ip[3]);
				SemaphoreGive (xLoadingWebSemaphore);
				return pLOAD_ERR;
			}
		}
	} else {
		journal.jprintf("%s: Upload: No web store!\n", (char*) __FUNCTION__);
		SemaphoreGive (xLoadingWebSemaphore);
		return pNO_DISK;
	}
	return pPOST_ERR; // До сюда добегать не должны
}
