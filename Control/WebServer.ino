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

void web_server(uint8_t thread)
{
 int32_t len;
 int8_t sock;

if(SemaphoreTake(xWebThreadSemaphore,(W5200_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE)  { return;}          // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим

Socket[thread].sock=-1;                      // Сокет свободный

SPI_switchW5200();                    // Это лишнее но для надежности пусть будет
    for (sock = 0; sock < W5200_SOCK_SYS; sock++)  // Цикл по сокетам веб сервера!!!! служебный не трогаем!!
       {

        #if    W5200_THREARD < 2 
         if (Socket[0].sock==sock)  continue;   // исключение повторного захвата сокетов
        #elif  W5200_THREARD < 3
          if((Socket[0].sock==sock)||(Socket[1].sock==sock))  continue;   // исключение повторного захвата сокетов
        #elif  W5200_THREARD < 4
          if((Socket[0].sock==sock)||(Socket[1].sock==sock)||(Socket[2].sock==sock))  continue;   // исключение повторного захвата сокетов
        #else
          if((Socket[0].sock==sock)||(Socket[1].sock==sock)||(Socket[2].sock==sock)||(Socket[3].sock==sock))  continue;   // исключение повторного захвата сокетов
        #endif

        // Настройка  переменных потока для работы
        Socket[thread].http_req_type = HTTP_invalid;        // нет полезной инфы
        SETBIT0(Socket[thread].flags,fABORT_SOCK);          // Сокет сброса нет
        SETBIT0(Socket[thread].flags,fUser);                // Признак идентификации как обычный пользователь (нужно подменить файл меню)
        Socket[thread].client =  server1.available_(sock);  // надо обработать
        Socket[thread].sock=sock;                           // запомнить сокет с которым рабоатет поток

if (Socket[thread].client) // запрос http заканчивается пустой строкой
{  
        while (Socket[thread].client.connected())
        {   
          // Ставить вот сюда
            if (Socket[thread].client.available()) 
             {
             len=Socket[thread].client.get_ReceivedSizeRX();                            // получить длину входного пакета
             if(len>W5200_MAX_LEN-1) len=W5200_MAX_LEN-1;                               // Ограничить размером в максимальный размер пакета w5200
             Socket[thread].client.read(Socket[thread].inBuf,len);                      // прочитать буфер
             Socket[thread].inBuf[len]=0;                                // обрезать строку
                     // Ищем в запросе полезную информацию (имя файла или запрос ajax)
                     #ifdef LOG
                        journal.jprintf("$INPUT: %s\n",(char*)Socket[thread].inBuf);
                     #endif
                      // пройти авторизацию и разобрать заголовок -  получить имя файла, тип, тип запроса, и признак меню пользователя
                      Socket[thread].http_req_type = GetRequestedHttpResource(thread);  
                    #ifdef LOG
                        journal.jprintf("\r\n$QUERY: %s\r\n",Socket[thread].inPtr);
                     #endif
                //       Serial.print(">>Thread=");Serial.print(thread); Serial.print(" Sock=");Serial.print(sock);Serial.print(" IP=");Serial.print(IPAddress2String(temp));Serial.print(" MAC=");Serial.println(MAC2String(mac));
                      switch (Socket[thread].http_req_type)  // По типу запроса
                          {
                          case HTTP_invalid:
                               {
							 	#ifndef DEBUG
                                   journal.jprintf("WEB:Error GET request\n");
								#endif
                               sendConstRTOS(thread,"HTTP/1.1 Error GET request\r\n\r\n");
                               break;
                               }
                          case HTTP_GET:     // чтение файла
                               {
                               // Для обычного пользователя подменить файл меню, для сокращения функционала
                               if ((GETBIT(Socket[thread].flags,fUser))&&(strcmp(Socket[thread].inPtr,"menu.js")==0)) strcpy(Socket[thread].inPtr,"menu-user.js");
                               readFileSD(Socket[thread].inPtr,thread); 
                               break;
                               }
                          case HTTP_REQEST:  // Запрос AJAX
                               {
                               strcpy(Socket[thread].outBuf,HEADER_ANSWER);   // Начало ответа
                               parserGET(Socket[thread].inPtr,Socket[thread].outBuf,sock);    // выполнение запроса
                               #ifdef LOG
                                journal.jprintf("$RETURN: %s\n",Socket[thread].outBuf);
                               #endif
                               if (sendBufferRTOS(thread,(byte*)(Socket[thread].outBuf),strlen(Socket[thread].outBuf))==0) journal.jprintf("$Error send buf:  %s\n",(char*)Socket[thread].inBuf);
                               break;
                               }
                          case HTTP_POST:    // загрузка настроек
                               {
                               strcpy(Socket[thread].outBuf,HEADER_ANSWER);   // Начало ответа
                               if(parserPOST(thread))    strcat(Socket[thread].outBuf,"Настройки из выбранного файла восстановлены, CRC16 OK\r\n\r\n");
                               else                      strcat(Socket[thread].outBuf,"Ошибка восстановления настроек из файла (см. журнал)\r\n\r\n");
                               if (sendBufferRTOS(thread,(byte*)(Socket[thread].outBuf),strlen(Socket[thread].outBuf))==0) journal.jprintf("$Error send buf:  %s\n",(char*)Socket[thread].inBuf);
                               break;
                               }
                          case HTTP_POST_: // предвариательный запрос post
                               {
                                sendConstRTOS(thread,"HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: HEAD, OPTIONS, GET, POST\r\nAccess-Control-Allow-Headers: Overwrite, Content-Type, Cache-Control\r\n\r\n");  
                                break;
                               }
                         case UNAUTHORIZED:
                               {
                               journal.jprintf("$UNAUTHORIZED\n");
                               sendConstRTOS(thread,pageUnauthorized);
                               break;
                               }
                          case BAD_LOGIN_PASS:
                               {
                               journal.jprintf("$Wrong login or password\n");
                               sendConstRTOS(thread,pageUnauthorized);
                               break;
                               }

                          default: journal.jprintf("$Unknow  %s\n",(char*)Socket[thread].inBuf);   break;        
                         }
                   
                   Socket[thread].inBuf[0]=0;    break;   // Подготовить к следующей итерации
             } // end if (client.available())
        } // end while (client.connected())
       taskYIELD();
       Socket[thread].client.stop();   // close the connection
       Socket[thread].sock=-1;
     } // end if (client)
#ifdef FAST_LIB  // Переделка
  }  // for (int sock = 0; sock < W5200_SOCK_SYS; sock++)
#endif
    SemaphoreGive(xWebThreadSemaphore);              // Семафор отдать
}

//  Чтение файла с SD или его генерация
void readFileSD(char *filename,uint8_t thread)
{   
  volatile int n,i;
  SdFile  webFile; 
  char *ch1,*ch2;
  char buf[8];  // для расширения файла хватит 8 байт
  //  journal.jprintf("$Thread: %d socket: %d read file: %s\n",thread,Socket[thread].sock,filename); 
  
         // Реализация функционала подмены для имен файлов по типу: plan[HPscheme].png -> plan2.png
         if ((ch1=strchr(filename,'['))!=NULL) // скобка найдена надо обрабатывать
         {
           if (strstr(filename,"HPscheme")!=0) // найден аргумент (схема ТН) надо подменять на значение HP_SHEME
            {
             if ((ch2=strchr(filename,']'))!=NULL)
              {
              strncpy(buf,ch2+1,sizeof(buf)-1); // скопировать хвост в промежуточный буфер
              *ch1=0x00;  // обрезать строку filename перед [
              strcat(filename,int2str(HP_SCHEME)); // добавить номер схемы
              strcat(filename,buf);               // добавить расширение
              }
            else journal.jprintf("Not found ] in: %s",filename); // нет закрывающейся скобки
            } // if (strstr(filename,"HPscheme")!=0) 
         else journal.jprintf("Bad argument in: %s",filename);   // не верный аргумент в скобках
         }
    
  	  	 // В начале обрабатываем генерируемые файлы (для выгрузки из контроллера)
         if (strcmp(filename,"state.txt")==0)     {get_txtState(thread,true); return;}  
         if (strcmp(filename,"settings.txt")==0)  {get_txtSettings(thread);   return;}      
         if (strcmp(filename,"settings.bin")==0)  {get_binSettings(thread);   return;}   
         if (strcmp(filename,"chart.csv")==0)     {get_csvChart(thread);      return;}   
         if ((strcmp(filename,FILE_CHART)==0)&&(!card.exists(FILE_CHART))) {noCsvChart_SD(thread); return;}   // Если файла статистики нет то сгенерить файл с объяснением
         if (strcmp(filename,"journal.txt")==0)   {get_txtJournal(thread);    return;}  
         if (strcmp(filename,"test.dat")==0)      {get_datTest(thread);       return;}  
         #ifdef I2C_EEPROM_64KB    
         if (strcmp(filename,"statistic.csv")==0) {get_csvStatistic(thread);  return;}  
         #endif
         if (!HP.get_fSD())                       { get_indexNoSD(thread);    return;}                  // СД карта не работает - упрощенный интерфейс
         
          // Чтение с карты  файлов
          SPI_switchSD();
          if (!card.exists(filename))  // проверка на сущестование файла
              {  
               SPI_switchW5200(); 
               sendConstRTOS(thread,HEADER_FILE_NOT_FOUND);
               journal.jprintf((char*)"$WARNING - Can't find %s file!\n",filename); 
               return;    
              } // файл не найден
              
          for(i=0;i<SD_REPEAT;i++)   // Делаем SD_REPEAT попыток открытия файла
          {
              if (!webFile.open(filename, O_READ))    // Карта не читатаеся
              {
                if (i>=SD_REPEAT-1)                   // Исчерпано число попыток
                 {
                  SPI_switchW5200();  
                  sendConstRTOS(thread,HEADER_FILE_NOT_FOUND);
                  journal.jprintf("$ERROR - opening %s for read failed!\n",filename); 
                  HP.message.setMessage(pMESSAGE_SD,(char*)"Ошибка открытия файла с SD карты",0);    // сформировать уведомление об ошибке чтения
                  HP.set_fSD(false);                                                                 // Отказ карты, работаем без нее
                  return;
                  }//if
              }
              else  break;  // Прочиталось
			  _delay(50);	
              journal.jprintf("Error opening file %s repeat open . . .\n",filename);
 
          }  // for     

          SPI_switchW5200();         // переключение на сеть

                // Файл открыт читаем данные и кидаем в сеть
                 #ifdef LOG  
                   journal.jprintf("$Thread: %d socket: %d read file: %s\n",thread,Socket[thread].sock,filename); 
                 #endif
              //   if (strstr(filename,".css")>0) sendConstRTOS(thread,HEADER_FILE_CSS);
                 if (strstr(filename,".css")!=NULL) sendConstRTOS(thread,HEADER_FILE_CSS); // разные заголовки
                 else                               sendConstRTOS(thread,HEADER_FILE_WEB);
                 SPI_switchSD();
               while ((n=webFile.read(Socket[thread].outBuf,sizeof(Socket[thread].outBuf))) > 0) 
                 {
                  SPI_switchW5200();
                  if (sendBufferRTOS(thread,(byte*)(Socket[thread].outBuf),n)==0) break;
                  SPI_switchSD();    
                } // while
               SPI_switchSD(); 
               webFile.close(); 
               SPI_switchW5200();         
 }

// ========================== P A R S E R  G E T =================================
// Разбор и обработка строк запросов buf (начало &) входная строка strReturn выходная
// ТОЛЬКО запросы!
// возвращает число обработанных одиночных запросов
int parserGET(char *buf, char *strReturn, int8_t sock)
{ 
   
  char *str,*x,*y, *z;
  int count=0;
  int param=-1,p=-1;
  char temp[16];   // временный буфер
  float pm=0;
  int8_t i;
  // переменные для удаленных датчиков
   #ifdef SENSOR_IP                           // Получение данных удаленного датчика
       char *ptr;
       int16_t a,b,c,d;
   #endif
   int32_t e;
  
 // strcpy(strReturn,"&");   // начало запроса
   strcat(strReturn,"&");   // начало запроса
  strstr(buf,"&&")[1]=0;   // Обрезать строку после комбинации &&
  while ((str = strtok_r(buf, "&", &buf)) != NULL) // разбор отдельных комманд
   {
   count++;
   if ((strpbrk(str,"="))==0) // Повторить тело запроса и добавить "=" ДЛЯ НЕ set_ запросов
     {
      strcat(strReturn,str); strcat(strReturn,"="); 
     } 
     if (strlen(strReturn)>sizeof(Socket[0].outBuf)-200)  // Контроль длины выходной строки - если слишком длинная обрезаем и выдаем ошибку 200 байт на заголовок
     {  
         strcat(strReturn,"E07"); 
         strcat(strReturn,"&") ;
         journal.jprintf("$ERROR - strReturn long, request circumcised . . . \n"); 
         journal.jprintf("%s\n",strReturn);    
          break;   // выход из обработки запроса
     }
    // 1. Функции без параметра
   if (strcmp(str,"TEST")==0)   // Команда TEST
       {
       strcat(strReturn,int2str(random(-50,50)));
       strcat(strReturn,"&") ;
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
         strcat(strReturn,"&");  continue;
       } 
  if (strcmp(str,"TASK_LIST_RST")==0)  // Функция сброс статистики по задачам
       {
         #ifdef STAT_FREE_RTOS   // определена в utility/FreeRTOSConfig.h
	  	 vTaskResetRunTimeCounters();
         #else
         strcat(strReturn,NO_SUPPORT);
       #endif
         strcat(strReturn,"&");  continue;
       }
   if (strcmp(str,"RESET_STAT")==0)   // Команда очистки статистики (в зависимости от типа)
       {
      
       #ifndef I2C_EEPROM_64KB     // Статистика в памяти
           strcat(strReturn,"Статистика не поддерживается в конфигурации . . .&");
           journal.jprintf("No support statistics (low I2C) . . .\n");
       #else                      // Статистика в ЕЕПРОМ
           if (HP.get_modWork()==pOFF)
             {
              strcat(strReturn,"Форматирование I2C статистики, ожидайте 10 сек . . .&");
              HP.sendCommand(pSFORMAT);        // Послать команду форматирование статитсики
             }
             else strcat(strReturn,"The heat pump must be switched OFF&");  
       #endif
       continue;
       }         
        
     if (strcmp(str,"RESET_JOURNAL")==0)   // Команда очистки журнала (в зависимости от типа)
       {
      
       #ifndef I2C_EEPROM_64KB     // журнал в памяти
           strcat(strReturn,"Сброс системного журнала в RAM . . .&");
           journal.Clear();       // Послать команду на очистку журнала в памяти
           journal.jprintf("Reset system RAM journal . . .\n"); 
       #else                      // Журнал в ЕЕПРОМ
            journal.Format(strReturn);
            //HP.sendCommand(pJFORMAT);        // Послать команду форматирование журнала
            strcpy(strReturn, HEADER_ANSWER);   // Начало ответа
            strcat(strReturn,"OK&");
       #endif
       continue;
       }        
    if (strcmp(str,"RESET")==0)   // Команда сброса контроллера
       {
       strcat(strReturn,"Сброс контроллера, подождите 10 секунд . . .");
       strcat(strReturn,"&");
       HP.sendCommand(pRESET);        // Послать команду на сброс
       continue;
       }
    if (strcmp(str,"RESET_COUNT")==0) // Команда RESET_COUNT
       {
       journal.jprintf("$RESET counter moto hour . . .\n"); 
       strcat(strReturn,"Сброс счетчика моточасов за сезон");
       strcat(strReturn,"&") ;
       HP.resetCount(false);
       continue;
       }
   if (strcmp(str,"RESET_ALL_COUNT")==0) // Команда RESET_COUNT
       {
       journal.jprintf("$RESET All counter moto hour . . .\n"); 
       strcat(strReturn,"Сброс ВСЕХ счетчика моточасов");
       strcat(strReturn,"&") ;
       HP.resetCount(true);  // Полный сброс
       continue;
       }    
   if (strcmp(str,"RESET_SETTINGS")==0) // Команда сброса настроек HP
   {
	   if(HP.get_State() == pOFF_HP) {
	       journal.jprintf("$RESET All HP settings . . .\n");
	       HP.headerEEPROM.magic = 0;
	   	   writeEEPROM_I2C(I2C_SETTING_EEPROM, (byte*)&HP.headerEEPROM.magic, sizeof(HP.headerEEPROM.magic));
	       HP.sendCommand(pRESET);        // Послать команду на сброс
	       //HP.resetSettingHP(); // не работает!!
	       strcat(strReturn, "OK&");
	       continue;
	   }
   }
   if (strcmp(str,"get_status")==0) // Команда get_status Получить состояние ТН - основные параметры ТН
       {
        HP.get_datetime((char*)time_TIME,strReturn);                     strcat(strReturn,"|");
        HP.get_datetime((char*)time_DATE,strReturn);                     strcat(strReturn,"|");
        strcat(strReturn,VERSION);                                       strcat(strReturn,"|");
        strcat(strReturn,int2str(freeRam()+HP.startRAM));                strcat(strReturn,"b|");
        strcat(strReturn,int2str(100-HP.CPU_IDLE));                      strcat(strReturn,"%|");
        strcat(strReturn,TimeIntervalToStr(HP.get_uptime()));            strcat(strReturn,"|");
        #ifdef EEV_DEF
        strcat(strReturn,ftoa(temp,(float)HP.dEEV.get_Overheat()/100,2));strcat(strReturn,"°C|");
        #else
        strcat(strReturn,"-°C|");
        #endif
        if (HP.dFC.get_present()) {HP.dFC.get_paramFC((char*)fc_FC,strReturn);strcat(strReturn,"Гц|");} // В зависимости от наличия инвертора
        else                      {strcat(strReturn," - ");strcat(strReturn,"Гц|");}
        if (HP.get_errcode()==OK)  strcat(strReturn,HP.StateToStr());                   // Ошибок нет
        else {strcat(strReturn,"Error "); strcat(strReturn,int2str(HP.get_errcode()));} // есть ошибки
        strcat(strReturn,"|");   strcat(strReturn,"&") ;    continue;
       }
       
   if (strcmp(str,"get_version")==0) // Команда get_version
       {
       strcat(strReturn,VERSION);
       strcat(strReturn,"&") ;
       continue;
       }
   if (strcmp(str,"get_uptime")==0) // Команда get_uptime
       {
       strcat(strReturn,TimeIntervalToStr(HP.get_uptime()));
       strcat(strReturn,"&") ;
       continue;
       }
    if (strcmp(str,"get_startDT")==0) // Команда get_startDT
       {
       strcat(strReturn,DecodeTimeDate(HP.get_startDT()));
       strcat(strReturn,"&") ;
       continue;
       }
   if (strcmp(str,"get_resetCause")==0) // Команда  get_resetCause
       {
       strcat(strReturn,ResetCause());
       strcat(strReturn,"&") ;
       continue;
       }
    if (strcmp(str,"get_config")==0)  // Функция get_config
       {
       strcat(strReturn,CONFIG_NAME);
       strcat(strReturn,"&") ;
       continue;
       }
    if (strcmp(str,"get_configNote")==0)  // Функция get_configNote
       {
       strcat(strReturn,CONFIG_NOTE);
       strcat(strReturn,"&") ;
       continue;
       }
    if (strcmp(str,"get_freeRam")==0)  // Функция freeRam
       {
       strcat(strReturn,int2str(freeRam()+HP.startRAM));
       strcat(strReturn," b&") ;
       continue;
       }   
    if (strcmp(str,"get_loadingCPU")==0)  // Функция freeRam
       {
        strcat(strReturn,int2str(100-HP.CPU_IDLE));
        strcat(strReturn,"%&") ;
       continue;
       }        
     if (strcmp(str,"get_socketInfo")==0)  // Функция  get_socketInfo
       {
       socketInfo(strReturn);    // Информация  о сокетах
       strcat(strReturn,"&") ;
       continue;
       }
     if (strcmp(str,"get_socketRes")==0)  // Функция  get_socketRes
       {
       strcat(strReturn,int2str(HP.socketRes()));
       strcat(strReturn,"&") ;
       continue;
       }  
     if (strcmp(str,"get_listChart")==0)  // Функция get_listChart - получить список доступных графиков
       {
       HP.get_listChart(strReturn, true);  // строка добавляется
       strcat(strReturn,"&") ;
       continue;
       }
     if (strcmp(str,"get_listStat")==0)  // Функция get_listChart - получить список доступных статистик
       {
       #ifdef I2C_EEPROM_64KB 
       HP.Stat.get_listStat(strReturn, true);  // строка добавляется
       #else
       strcat(strReturn,"absent:1;") ;
       #endif
       strcat(strReturn,"&") ;
       continue;
       }   
    if (strncmp(str,"get_listProfile", 15)==0)  // Функция get_listProfile - получить список доступных профилей
       {
       HP.Prof.get_list(strReturn /*,HP.Prof.get_idProfile()*/);  // текущий профиль
       strcat(strReturn,"&") ;
       continue;
       }
       if (strcmp(str,"update_NTP")==0)  // Функция update_NTP обновление времени по NTP
       {
      // set_time_NTP();                                                 // Обновить время
       HP.timeNTP=0;                                    // Время обновления по NTP в тиках (0-сразу обновляемся)
       strcat(strReturn,"Update time from NTP");
       strcat(strReturn,"&");
       continue;
       }
       if ((strcmp(str,"set_updateNet")==0)||(strcmp(str,"RESET_NET")==0))  // Функция Сброс w5200 и применение сетевых настроек, подождите 5 сек . . .
       {
       journal.jprintf("Update network setting . . .\r\n");
       HP.sendCommand(pNETWORK);        // Послать команду применение сетевых настроек
       strcat(strReturn,"Сброс Wiznet w5XXX и применение сетевых настроек, подождите 5 сек . . .");
       strcat(strReturn,"&") ;
       continue;
       }         
    if (strcmp(str,"get_WORK")==0)  // Функция get_WORK  ТН включен если он работает или идет его пуск
       {
       if ((HP.get_State()==pWORK_HP)||(HP.get_State()==pSTARTING_HP)||(HP.get_State()==pWAIT_HP)||(HP.get_State()==pSTOPING_HP))  strcat(strReturn,"ON"); else  strcat(strReturn,"OFF"); strcat(strReturn,"&"); continue;
       }   
    if (strcmp(str,"get_MODE")==0)  // Функция get_MODE в каком состояниии находится сейчас насос
       {
       strcat(strReturn,HP.StateToStr());
       strcat(strReturn,"&") ;    continue;
       }    
 
    if (strcmp(str,"get_modeHP")==0)           // Функция get_modeHP - получить режим отопления ТН
        {
            switch ((MODE_HP)HP.get_mode())   // режим работы отопления
                   {
                    case pOFF:   strcat(strReturn,(char*)"Выключено:1;Отопление:0;Охлаждение:0;"); break;
                    case pHEAT:  strcat(strReturn,(char*)"Выключено:0;Отопление:1;Охлаждение:0;"); break;
                    case pCOOL:  strcat(strReturn,(char*)"Выключено:0;Отопление:0;Охлаждение:1;"); break;
                    default: HP.set_mode(pOFF);  strcat(strReturn,(char*)"Выключено:1;Отопление:0;Охлаждение:0;"); break;   // Исправить по умолчанию
                   }  
          strcat(strReturn,"&") ; continue;
        } // strcmp(str,"get_modeHP")==0)   
    if (strcmp(str,"get_testMode")==0)  // Функция get_testMode
       {
       for(i=0;i<=HARD_TEST;i++) // Формирование списка
           { strcat(strReturn,noteTestMode[i]); strcat(strReturn,":"); if(i==HP.get_testMode()) strcat(strReturn,cOne); else strcat(strReturn,cZero); strcat(strReturn,";");  }          
       strcat(strReturn,"&") ;    continue;
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
       strcat(strReturn,"&") ;    continue;
       }  
     
        
    if (strncmp(str, "set_SAVE", 8) == 0)  // Функция set_SAVE -
		{
			if(strncmp(str+8, "_SCHDLR", 7) == 0) {
				#ifdef USE_SCHEDULER
				strcat(strReturn, int2str(HP.Schdlr.save())); // сохранение расписаний
				#endif
			} else {
				strcat(strReturn, int2str(HP.save())); // сохранение настроек ВСЕХ!
				HP.save_motoHour();
			}
			strcat(strReturn,"&");
			continue;
		}
    if (strncmp(str, "set_LOAD", 8) == 0)  // Функция set_LOAD -
		{
			if(strncmp(str+8, "_SCHDLR", 7) == 0) {
				#ifdef USE_SCHEDULER
				strcat(strReturn, int2str(HP.Schdlr.load())); // сохранение расписаний
				#endif
			} else {
			}
			strcat(strReturn,"&");
			continue;
		}
    if (strcmp(str,"set_ON")==0)  // Функция set_ON
       {
       HP.sendCommand(pSTART);        // Послать команду на пуск ТН
       if (HP.get_State()==pWORK_HP)  strcat(strReturn,cOne); else  strcat(strReturn,cZero); strcat(strReturn,"&") ;    continue;
       }   
    if (strcmp(str,"set_OFF")==0)  // Функция set_OFF
       {
       HP.sendCommand(pSTOP);        // Послать команду на останов ТН
       if (HP.get_State()==pWORK_HP)  strcat(strReturn,cOne); else  strcat(strReturn,cZero); strcat(strReturn,"&") ;    continue;
       }   
    if (strcmp(str,"get_errcode")==0)  // Функция get_errcode
       {
       strcat(strReturn,int2str(HP.get_errcode())); strcat(strReturn,"&") ;    continue;
       }   
    if (strcmp(str,"get_error")==0)  // Функция get_error
       {
       strcat(strReturn,HP.get_lastErr()); strcat(strReturn,"&") ;    continue;
       } 
    if (strcmp(str,"get_tempSAM3x")==0)  // Функция get_tempSAM3x  - получение температуры чипа sam3x
       {
       strcat(strReturn,ftoa(temp,temp_DUE(),2)); strcat(strReturn,"&"); continue;
       }   
    if (strcmp(str,"get_tempDS3231")==0)  // Функция get_tempDS3231  - получение температуры DS3231
       {
       strcat(strReturn,ftoa(temp,getTemp_RtcI2C(),2)); strcat(strReturn,"&"); continue;
       }  
    if (strcmp(str,"get_fullCOP")==0)  //  получение полного COP
       {
       if (HP.fullCOP!=-1000) strcat(strReturn,ftoa(temp,HP.fullCOP/100.0,2)); else strcat(strReturn,"-"); strcat(strReturn,"&"); continue;
       }  
    if (strcmp(str,"get_VCC")==0)  // Функция get_VCC  - получение напряжение питания контроллера
       {
       #ifdef VCC_CONTROL  // если разрешено чтение напряжение питания
           strcat(strReturn,ftoa(temp,(float)HP.AdcVcc/K_VCC_POWER,2)); 
        #else 
           strcat(strReturn,NO_SUPPORT); 
        #endif
       strcat(strReturn,"&"); continue;
       }       
    if (strcmp(str,"get_OneWirePin")==0)  // Функция get_OneWirePin
       {
       #ifdef ONEWIRE_DS2482  
        strcat(strReturn,"I2C"); 
		#ifdef ONEWIRE_DS2482_SECOND
        strcat(strReturn,"(2)"); 
		#endif
       #else
        strcat(strReturn,"D"); strcat(strReturn,int2str((int)(PIN_ONE_WIRE_BUS))); 
       #endif
       strcat(strReturn,"&") ;    continue;
       }       
    if (strcmp(str,"scan_OneWire")==0)  // Функция scan_OneWire  - сканирование датчикиков
       {
    	HP.scan_OneWire(strReturn); strcat(strReturn,"&"); continue;
       }
     if (strstr(str,"get_numberIP"))  // Удаленные датчики - получить число датчиков
       { 
        #ifdef SENSOR_IP                           
         strcat(strReturn,int2str(IPNUMBER));strcat(strReturn,"&"); continue;
        #else
         strcat(strReturn,"0&");continue;
        #endif 
       }   
      if (strcmp(str,"set_testStat")==0)  // сгенерить тестовые данные статистики ОЧИСТКА СТАРЫХ ДАННЫХ!!!!!
       {
       #ifdef I2C_EEPROM_64KB  // рабоатет на выключенном ТН
       if (HP.get_modWork()==pOFF)
       {
         HP.Stat.generate_TestData(STAT_POINT); // Сгенерировать статистику STAT_POINT точек только тестирование
         strcat(strReturn,"Generation of test data - OK&");
       }
       else strcat(strReturn,"The heat pump must be switched OFF&");  
       #else
       strcat(strReturn,NO_STAT);
       #endif
       continue;
       }   
     if (strcmp(str,"get_infoStat")==0)  // Получить информацию о статистике
       {
       #ifdef I2C_EEPROM_64KB    
       HP.Stat.get_Info(strReturn,true);
       #else
       strcat(strReturn,NO_STAT) ;
       #endif
       strcat(strReturn,"&") ;
       continue;
       }   

    if (strstr(str,"get_infoESP"))  // Удаленные датчики - запрос состояния контрола
       { 
       // TIN, TOUT, TBOILER, ВЕРСИЯ, ПАМЯТЬ, ЗАГРУЗКА, АПТАЙМ, ПЕРЕГРЕВ, ОБОРОТЫ, СОСТОЯНИЕ.
        strcat(strReturn,ftoa(temp,(float)HP.sTemp[TIN].get_Temp()/100.0,1));     strcat(strReturn,";");
        strcat(strReturn,ftoa(temp,(float)HP.sTemp[TOUT].get_Temp()/100.0,1));    strcat(strReturn,";");
        strcat(strReturn,ftoa(temp,(float)HP.sTemp[TBOILER].get_Temp()/100.0,1)); strcat(strReturn,";");
        strcat(strReturn,VERSION);                                                strcat(strReturn,";");        
       strcat(strReturn,int2str(freeRam()+HP.startRAM));                          strcat(strReturn,";");
        strcat(strReturn,int2str(100-HP.CPU_IDLE));                               strcat(strReturn,";");
        strcat(strReturn,TimeIntervalToStr(HP.get_uptime()));                     strcat(strReturn,";");
        #ifdef EEV_DEF 
        strcat(strReturn,ftoa(temp,(float)HP.dEEV.get_Overheat()/100,2));         strcat(strReturn,";");
        #else
        strcat(strReturn,"-;");
        #endif
        if (HP.dFC.get_present()) {HP.dFC.get_paramFC((char*)fc_FC,strReturn);    strcat(strReturn,";");} // В зависимости от наличия инвертора
        else                      {strcat(strReturn," - ");                       strcat(strReturn,";");}
      //  strcat(strReturn,HP.StateToStrEN());                                      strcat(strReturn,";");
        if (HP.get_errcode()==OK)  strcat(strReturn,HP.StateToStrEN());                   // Ошибок нет
        else {strcat(strReturn,"Error "); strcat(strReturn,int2str(HP.get_errcode()));} // есть ошибки
        strcat(strReturn,";");   strcat(strReturn,"&") ;    continue;
       }   
     if(strncmp(str, "hide_", 5) == 0) { // Удаление элементов внутри tag name="hide_*"
    	str += 5;
    	if(strcmp(str, "fcanalog") == 0) {
			#ifdef FC_ANALOG_CONTROL
    			strcat(strReturn,"0&");
			#else
    			strcat(strReturn,"1&");
			#endif
    	} else if(strcmp(str, "rpumpfl") == 0) {
			#ifdef RPUMPFL
    			strcat(strReturn,"0&");
			#else
    			strcat(strReturn,"1&");
			#endif
    	} else if(strcmp(str, "tro_ei") == 0) { // hide: TRTOOUT, TEVAIN
			#ifdef TRTOUT
    			strcat(strReturn,"0&");
			#else
    			strcat(strReturn,"1&");
			#endif
    	}
    	continue;
     }
 
    if (strcmp(str,"CONST")==0)   // Команда CONST  Информация очень большая по этому разбито на 2 запроса CONST CONST1
        {
       strcat(strReturn,"VERSION|Версия прошивки|");strcat(strReturn,VERSION);strcat(strReturn,";");
       strcat(strReturn,"__DATE__ __TIME__|Дата и время сборки прошивки|");strcat(strReturn,__DATE__);strcat(strReturn," ");strcat(strReturn,__TIME__) ;strcat(strReturn,";");
       strcat(strReturn,"VER_SAVE|Версия формата данных, при чтении eeprom, если версии не совпадают, отказ от чтения|");strcat(strReturn,int2str(VER_SAVE));strcat(strReturn,";");
       strcat(strReturn,"CONFIG_NAME|Имя конфигурации|");strcat(strReturn,CONFIG_NAME);strcat(strReturn,";");
       strcat(strReturn,"CONFIG_NOTE|");strcat(strReturn,CONFIG_NOTE);strcat(strReturn,"|-");strcat(strReturn,";");  
       strcat(strReturn,"UART_SPEED|Скорость отладочного порта (бод)|");strcat(strReturn,int2str(UART_SPEED));strcat(strReturn,";");
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
       strcat(strReturn,"STAT_POINT|Максимальное число дней накопления статистики|");strcat(strReturn,int2str(STAT_POINT));strcat(strReturn,";");
       #endif
       strcat(strReturn,"CHART_POINT|Максимальное число точек графиков|");strcat(strReturn,int2str(CHART_POINT));strcat(strReturn,";");
       strcat(strReturn,"I2C_EEPROM_64KB|Место хранения системного журнала|");
            #ifdef I2C_EEPROM_64KB
             strcat(strReturn,"I2C flash memory;");
            #else
             strcat(strReturn,"RAM memory;");
            #endif 
        strcat(strReturn,"JOURNAL_LEN|Размер кольцевого буфера системного журнала (байт)|");strcat(strReturn,int2str(JOURNAL_LEN));strcat(strReturn,";");
                   
       // Карта
       strcat(strReturn,"SD_FAT_VERSION|Версия библиотеки SdFat|");strcat(strReturn,int2str(SD_FAT_VERSION));strcat(strReturn,";");
       strcat(strReturn,"SD_SPI_CONFIGURATION|Режим SPI SD карты, меняется в файле SdFatConfig.h (0-DMA, 1-standard, 2-software, 3-custom)|");strcat(strReturn,int2str(SD_SPI_CONFIGURATION));strcat(strReturn,";");
       strcat(strReturn,"SD_REPEAT|Число попыток чтения карты и открытия файлов, при неудаче переход на работу без карты|");strcat(strReturn,int2str(SD_REPEAT));strcat(strReturn,";");
       strcat(strReturn,"SD_SPI_SPEED|Частота SPI SD карты, пересчитывается через делитель базовой частоты CPU 84 МГц (МГц)|");strcat(strReturn,int2str(84/SD_SPI_SPEED));strcat(strReturn,";");

       // W5200
       strcat(strReturn,"W5200_THREARD|Число потоков для сетевого чипа (web сервера) "); strcat(strReturn,nameWiznet);strcat(strReturn,"|");strcat(strReturn,int2str(W5200_THREARD));strcat(strReturn,";");
       strcat(strReturn,"W5200_TIME_WAIT|Время ожидания захвата мютекса, для управления потоками (мсек)|");strcat(strReturn,int2str( W5200_TIME_WAIT));strcat(strReturn,";");
       strcat(strReturn,"W5200_STACK_SIZE|Размер стека для одного потока чипа "); strcat(strReturn,nameWiznet);strcat(strReturn," (х4 байта)|");strcat(strReturn,int2str(W5200_STACK_SIZE));strcat(strReturn,";");
       strcat(strReturn,"W5200_NUM_PING|Число попыток пинга до определения потери связи |");strcat(strReturn,int2str(W5200_NUM_PING));strcat(strReturn,";");
       strcat(strReturn,"W5200_MAX_LEN|Размер аппаратного буфера  сетевого чипа "); strcat(strReturn,nameWiznet);strcat(strReturn," (байт)|");strcat(strReturn,int2str(W5200_MAX_LEN));strcat(strReturn,";");
       strcat(strReturn,"W5200_SPI_SPEED|Частота SPI чипа "); strcat(strReturn,nameWiznet);strcat(strReturn,", пересчитывается через делитель базовой частоты CPU 84 МГц (МГц)|");strcat(strReturn,int2str(84/W5200_SPI_SPEED));strcat(strReturn,";");
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
       strcat(strReturn,"MODBUS_PORT_SPEED|Скорость обмена (бод)|");strcat(strReturn,int2str(MODBUS_PORT_SPEED));strcat(strReturn,";");
       strcat(strReturn,"MODBUS_PORT_CONFIG|Конфигурация порта|");strcat(strReturn,"SERIAL_8N1");strcat(strReturn,";");
       strcat(strReturn,"MODBUS_TIME_WAIT|Максимальное время ожидания освобождения порта (мсек)|");strcat(strReturn,int2str(MODBUS_TIME_WAIT));strcat(strReturn,";");

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
       strcat(strReturn,"NEXTION_UPDATE|Время обновления информации на дисплее Nextion (мсек)|");strcat(strReturn,int2str(NEXTION_UPDATE));strcat(strReturn,";");
       strcat(strReturn,"NEXTION_READ|Время опроса дисплея Nextion (мсек)|");strcat(strReturn,int2str(NEXTION_READ));strcat(strReturn,";");
       strcat(strReturn,"TIME_ZONE|Часовой пояс|");strcat(strReturn,int2str(TIME_ZONE));strcat(strReturn,";");
       // Free RTOS
       strcat(strReturn,"FREE_RTOS_ARM_VERSION|Версия библиотеки Free RTOS due|");strcat(strReturn,int2str(FREE_RTOS_ARM_VERSION));strcat(strReturn,";");
       strcat(strReturn,"configCPU_CLOCK_HZ|Частота CPU (мГц)|");strcat(strReturn,int2str(configCPU_CLOCK_HZ/1000000));strcat(strReturn,";");
       strcat(strReturn,"configTICK_RATE_HZ|Квант времени системы Free RTOS (мкс)|");strcat(strReturn,int2str(configTICK_RATE_HZ));strcat(strReturn,";");
       strcat(strReturn,"WDT_TIME|Период Watchdog таймера, 0 - запрет таймера (сек)|");strcat(strReturn,int2str(WDT_TIME));strcat(strReturn,";");
    
       // Удаленные датчики
       strcat(strReturn,"SENSOR_IP|Использование удаленных датчиков|");
       #ifdef SENSOR_IP 
        strcat(strReturn,"ON;");
        strcat(strReturn,"IPNUMBER|Максимальное число удаленных датчиков, нумерация начинается с 1|");strcat(strReturn,int2str(IPNUMBER));strcat(strReturn,";");
        strcat(strReturn,"UPDATE_IP|Максимальное время между посылками данных с удаленного датчика (сек)|");strcat(strReturn,int2str(UPDATE_IP));strcat(strReturn,";");
       #else
        strcat(strReturn,"OFF;");
       #endif 
       
        strcat(strReturn,"K_VCC_POWER|Коэффициент пересчета для канала контроля напряжения питания (отсчеты/В)|");
        #ifdef VCC_CONTROL  // если разрешено чтение напряжение питания
          strcat(strReturn,ftoa(temp,(float)K_VCC_POWER,2));strcat(strReturn,";");
        #else 
           strcat(strReturn,NO_SUPPORT); strcat(strReturn,";");
        #endif
        
       strcat(strReturn,"&") ;    continue;
       } // end CONST
       
    if (strcmp(str,"CONST1")==0)   // Команда CONST1 Информация очень большая по этому разбито на 2 запроса CONST CONST1
       {
       
       // i2c
       strcat(strReturn,"I2C_SPEED|Частота работы шины I2C (кГц)|"); strcat(strReturn,int2str(I2C_SPEED/1000)); strcat(strReturn,";");
       strcat(strReturn,"I2C_COUNT_EEPROM|Адрес внутри чипа eeprom с которого пишется счетчики ТН|"); strcat(strReturn,uint16ToHex(I2C_COUNT_EEPROM)); strcat(strReturn,";");
       strcat(strReturn,"I2C_SETTING_EEPROM|Адрес внутри чипа eeprom с которого пишутся настройки ТН|"); strcat(strReturn,uint16ToHex(I2C_SETTING_EEPROM)); strcat(strReturn,";");
       strcat(strReturn,"I2C_PROFILE_EEPROM|Адрес внутри чипа eeprom с которого пишется профили ТН|"); strcat(strReturn,uint16ToHex(I2C_PROFILE_EEPROM)); strcat(strReturn,";");
       strcat(strReturn,"TIME_READ_SENSOR|Период опроса датчиков + DELAY_DS1820 (мсек)|");strcat(strReturn,int2str(TIME_READ_SENSOR+cDELAY_DS1820));strcat(strReturn,";");
       strcat(strReturn,"TIME_CONTROL|Период управления тепловым насосом (мсек)|");strcat(strReturn,int2str(TIME_CONTROL));strcat(strReturn,";");
       strcat(strReturn,"TIME_EEV|Период управления ЭРВ (мсек)|");strcat(strReturn,int2str(TIME_EEV));strcat(strReturn,";");
       strcat(strReturn,"TIME_WEB_SERVER|Период опроса web сервера "); strcat(strReturn,nameWiznet);strcat(strReturn," (мсек)|");strcat(strReturn,int2str(TIME_WEB_SERVER));strcat(strReturn,";");
       strcat(strReturn,"TIME_COMMAND|Период разбора команд управления ТН (мсек)|");strcat(strReturn,int2str(TIME_COMMAND));strcat(strReturn,";");
       strcat(strReturn,"TIME_I2C_UPDATE |Период синхронизации внутренних часов с I2C часами (сек)|");strcat(strReturn,int2str(TIME_I2C_UPDATE ));strcat(strReturn,";");
       // Датчики
       strcat(strReturn,"P_NUMSAMLES|Число значений для усреднения показаний давления|");strcat(strReturn,int2str(P_NUMSAMLES));strcat(strReturn,";");
       strcat(strReturn,"PRESS_FREQ|Частота опроса датчика давления (Гц)|");strcat(strReturn,int2str(PRESS_FREQ));strcat(strReturn,";");
       strcat(strReturn,"FILTER_SIZE|Длина фильтра датчика давления (отсчеты)|");strcat(strReturn,int2str(FILTER_SIZE));strcat(strReturn,";");
       strcat(strReturn,"T_NUMSAMLES|Число значений для усреднения показаний температуры|");strcat(strReturn,int2str(T_NUMSAMLES));strcat(strReturn,";");
       strcat(strReturn,"GAP_TEMP_VAL|Допустимая разница показаний между двумя считываниями|");strcat(strReturn,ftoa(temp,(float)GAP_TEMP_VAL/100.0,2));strcat(strReturn,";");
       strcat(strReturn,"MAX_TEMP_ERR|Максимальная систематическая ошибка датчика температуры|");strcat(strReturn,ftoa(temp,(float)MAX_TEMP_ERR/100.0,2));strcat(strReturn,";");
       // ЭРВ
       #ifdef EEV_DEF
       strcat(strReturn,"EEV_STEPS|Максимальное число шагов ЭРВ|");strcat(strReturn,int2str(EEV_STEPS));strcat(strReturn,";");
       strcat(strReturn,"EEV_QUEUE|Длина очереди команд шагового двигателя ЭРВ|");strcat(strReturn,int2str(EEV_QUEUE));strcat(strReturn,";");
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
    //   strcat(strReturn,"EVI_TEMP_CON|Температура кондесатора для включения соленойда EVI|");strcat(strReturn,ftoa(temp,(float)EVI_TEMP_CON/100.0,2));strcat(strReturn,";");
        #endif   // EEV
       #ifdef MQTT
//       strcat(strReturn,"MQTT_REPEAT|Число попыток соединениея с MQTT сервером за одну итерацию|");strcat(strReturn,int2str(MQTT_REPEAT));strcat(strReturn,";");
       strcat(strReturn,"MQTT_NUM_ERR_OFF|Число ошибок отправки подряд при котором отключается сервис MQTT (флаг сбрасывается)|");strcat(strReturn,int2str(MQTT_NUM_ERR_OFF));strcat(strReturn,";");
       
       #endif 
       strcat(strReturn,"&") ;
       continue;
       } // end CONST1
       
      if (strcmp(str,"get_sysInfo")==0)  // Функция вывода системной информации для разработчика
       {
        strcat(strReturn,"Входное напряжение питания контроллера (В): |");
        #ifdef VCC_CONTROL  // если разрешено чтение напряжение питания
          strcat(strReturn,ftoa(temp,(float)HP.AdcVcc/K_VCC_POWER,2));strcat(strReturn,";");
        #else 
            strcat(strReturn,NO_SUPPORT); strcat(strReturn,";");
        #endif
         
        strcat(strReturn,"Режим safeNetwork (адрес:192.168.0.177 шлюз:192.168.0.1, не спрашиваеть пароль на вход) [активно 1]|");strcat(strReturn,int2str(HP.safeNetwork));strcat(strReturn,";");
        strcat(strReturn,"Уникальный ID чипа SAM3X8E|");getIDchip(strReturn);strcat(strReturn,";");
        strcat(strReturn,"Значение регистра VERSIONR сетевого чипа WizNet (51-w5100, 3-w5200, 4-w5500)|");strcat(strReturn,int2str(W5200VERSIONR()));strcat(strReturn,";");
      
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
            if(HP.get_State()==pWORK_HP) { strcat(strReturn,int2str(HP.num_repeat));strcat(strReturn,";");} else strcat(strReturn,"0;");
        strcat(strReturn,"Счетчик \"Потеря связи с "); strcat(strReturn,nameWiznet);strcat(strReturn,"\", повторная инициализация  <sup>1</sup>|");strcat(strReturn,int2str(HP.num_resW5200));strcat(strReturn,";");
        strcat(strReturn,"Счетчик числа сбросов мютекса захвата шины SPI|");strcat(strReturn,int2str(HP.num_resMutexSPI));strcat(strReturn,";");
        strcat(strReturn,"Счетчик числа сбросов мютекса захвата шины I2C|");strcat(strReturn,int2str(HP.num_resMutexI2C));strcat(strReturn,";");
        #ifdef MQTT
        strcat(strReturn,"Счетчик числа повторных соединений MQTT клиента|");strcat(strReturn,int2str(HP.num_resMQTT));strcat(strReturn,";");
        #endif
        strcat(strReturn,"Счетчик потерянных пакетов ping|");strcat(strReturn,int2str(HP.num_resPing));strcat(strReturn,";");
        #ifdef USE_ELECTROMETER_SDM  
        strcat(strReturn,"Счетчик числа ошибок чтения счетчика SDM120 (RS485)|");strcat(strReturn,int2str(HP.dSDM.get_numErr()));strcat(strReturn,";");
        #endif
        if (HP.dFC.get_present()) strcat(strReturn,"Счетчик числа ошибок чтения частотного преобразователя (RS485)|");strcat(strReturn,int2str(HP.dFC.get_numErr()));strcat(strReturn,";");
       
        strcat(strReturn,"Счетчик числа ошибок чтения датчиков температуры (ds18b20)|");strcat(strReturn,int2str(HP.get_errorReadDS18B20()));strcat(strReturn,";");

        strcat(strReturn,"Время последнего включения ТН|");strcat(strReturn,DecodeTimeDate(HP.get_startTime()));strcat(strReturn,";");
        strcat(strReturn,"Время сохранения текущих настроек ТН|");strcat(strReturn,DecodeTimeDate(HP.get_saveTime()));strcat(strReturn,";");
        
        // Вывод строки статуса
        strcat(strReturn,"Строка статуса ТН| modWork:");strcat(strReturn,int2str((int)HP.get_modWork()));strcat(strReturn,"[");strcat(strReturn,codeRet[HP.get_ret()]);strcat(strReturn,"]");
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
        if(HP.dFC.get_present())  {strcat(strReturn," freqFC:"); strcat(strReturn,ftoa(temp,(float)HP.dFC.get_freqFC()/100.0,2)); }
        if(HP.dFC.get_present())  {strcat(strReturn," Power:"); strcat(strReturn,ftoa(temp,(float)HP.dFC.get_power()/10.0,2));  } 
        strcat(strReturn,";");  
   
           
        strcat(strReturn,"Время сброса счетчиков с момента запуска ТН|");strcat(strReturn,DecodeTimeDate(HP.get_motoHourD1()));strcat(strReturn,";");
        strcat(strReturn,"Часы работы ТН с момента запуска (час)|");strcat(strReturn,ftoa(temp,(float)HP.get_motoHourH1()/60.0,1));strcat(strReturn,";");
        strcat(strReturn,"Часы работы компрессора ТН с момента запуска (час)|");strcat(strReturn,ftoa(temp,(float)HP.get_motoHourC1()/60.0,1));strcat(strReturn,";");
        #ifdef USE_ELECTROMETER_SDM  
          strcat(strReturn,"Потребленная энергия ТН с момента запуска (кВт*ч)|");strcat(strReturn,ftoa(temp, HP.dSDM.get_Energy()-HP.get_motoHourE1(),2));strcat(strReturn,";");
        #endif
        if(HP.ChartPowerCO.get_present())  strcat(strReturn,"Выработанная энергия ТН с момента запуска (кВт*ч)|");strcat(strReturn,ftoa(temp, HP.get_motoHourP1()/1000.0,2));strcat(strReturn,";"); // Если есть оборудование
  
        strcat(strReturn,"Время сброса сезонных счетчиков ТН|");strcat(strReturn,DecodeTimeDate(HP.get_motoHourD2()));strcat(strReturn,";");
        strcat(strReturn,"Часы работы ТН за сезон (час)|");strcat(strReturn,ftoa(temp,(float)HP.get_motoHourH2()/60.0,1));strcat(strReturn,";");
        strcat(strReturn,"Часы работы компрессора ТН за сезон (час)|");strcat(strReturn,ftoa(temp,(float)HP.get_motoHourC2()/60.0,1));strcat(strReturn,";");
        #ifdef USE_ELECTROMETER_SDM  
          strcat(strReturn,"Потребленная энергия ТН за сезон (кВт*ч)|");strcat(strReturn,ftoa(temp, HP.dSDM.get_Energy()-HP.get_motoHourE2(),2));strcat(strReturn,";");
        #endif
        if(HP.ChartPowerCO.get_present())  strcat(strReturn,"Выработанная энергия ТН за сезон (кВт*ч)|");strcat(strReturn,ftoa(temp, HP.get_motoHourP2()/1000.0,2));strcat(strReturn,";"); // Если есть оборудование
  
        strcat(strReturn,"&") ;    continue;
       } // sisInfo
       
       if (strcmp(str,"test_Mail")==0)  // Функция test_mail
       {
       if (HP.message.setTestMail()) { strcat(strReturn,"Send test mail to "); HP.message.get_messageSetting((char*)mess_SMTP_RCPTTO,strReturn); }
       else { strcat(strReturn,"Error send test mail.");}
       strcat(strReturn,"&") ;   
        continue;  
       }   // test_Mail  
       if (strcmp(str,"test_SMS")==0)  // Функция test_mail
       {
       if (HP.message.setTestSMS()) { strcat(strReturn,"Send SMS to "); HP.message.get_messageSetting((char*)mess_SMS_PHONE,strReturn);}  //strcat(strReturn,HP.message.get_messageSetting(pSMS_PHONE));}
       else { strcat(strReturn,"Error send test sms.");}
       strcat(strReturn,"&") ;           
       continue;  
       }   // test_Mail    
       if(strcmp(str, "get_OverCool") == 0) {
           ftoa(strReturn + m_strlen(strReturn), HP.get_overcool() / 100.0, 2);
           strcat(strReturn,"&") ;    continue;
       }
       // ЭРВ запросы , те которые без параметра ------------------------------
         if (strcmp(str,"set_zeroEEV")==0)  // Функция set_zeroEEV
         {  
            #ifdef EEV_DEF 
            strcat(strReturn,int2str(HP.dEEV.set_zero())); 
            #else
            strcat(strReturn,"-");  
            #endif   
           strcat(strReturn,"&") ; continue;
         }          
        if (strncmp(str,"get_EEV", 7)==0)  // Функция get_EEV
         {
           #ifdef EEV_DEF 
           if(HP.dEEV.stepperEEV.isBuzy())  strcat(strReturn,"<<");  // признак движения
           i = 0;
           if(str[7] == 'p') { // get_EEVp - только проценты
        	   i = 2;
        	   if(str[8] == 'p') i = 1; // get_EEVpp - добавить проценты
           }
           if(i < 2) {
        	   itoa(HP.dEEV.get_EEV(), strReturn + m_strlen(strReturn), 10);
           }
           if(i > 0) {
        	   if(i == 1) strcat(strReturn, " (");
        	   if(HP.dEEV.get_EEV() >= 0) itoa(HP.dEEV.get_EEV_percent(), strReturn + m_strlen(strReturn), 10); else strcat(strReturn, "?");
               strcat(strReturn, "%");
               if(i == 1) strcat(strReturn, ")");
           }
           if (HP.dEEV.stepperEEV.isBuzy())  strcat(strReturn,">>");  // признак движения
           #else
           strcat(strReturn,"-");  
           #endif   
           strcat(strReturn,"&") ;    continue;
         }
  
        // FC запросы, те которые без параметра ------------------------------
        if (strcmp(str,"reset_errorFC")==0)  // Функция get_dacFC
         {
          if (!HP.dFC.get_present()) {strcat(strReturn,"Инвертор отсутствует&"); ;continue;}
          #ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
              HP.dFC.reset_errorFC();
              strcat(strReturn,"Ошибки инвертора сброшены . . .");
          #endif
          strcat(strReturn,"&") ;    continue;
         } 
        if (strcmp(str,"reset_FC")==0)    
         {
          if (!HP.dFC.get_present()) {strcat(strReturn,"Инвертор отсутствует&"); ;continue;}
           HP.dFC.reset_FC();                             // подать команду на сброс
           strcat(strReturn,"Cброс преобразователя частоты . . ."); strcat(strReturn,"&") ;    continue;
         } 
         #ifdef USE_ELECTROMETER_SDM
        // SDM запросы которые без параметра
        if (strcmp(str,"settingSDM")==0)     // Функция settingSDM  Запрограммировать параметры связи счетчика
         {
          if (!HP.dSDM.get_present()) {strcat(strReturn,"Счетчик отсутвует &"); ;continue;}
          HP.dSDM.progConnect();
          strcat(strReturn,"Счетчик запрограммирован, необходимо сбросить счетчик!!"); strcat(strReturn,"&") ;    continue;
         } 
        if (strcmp(str,"uplinkSDM")==0)     // Функция settingSDM  Попытаться возобновить связь со счетчиком при ее потери
         {
          if (!HP.dSDM.get_present()) {strcat(strReturn,"Счетчик отсутвует &"); ;continue;}
          HP.dSDM.uplinkSDM();
          strcat(strReturn,"Проверка связи со счетчиком"); strcat(strReturn,"&") ;    continue;
         } 
          #endif
        // -------------- СПИСКИ ДАТЧИКОВ и ИСПОЛНИТЕЛЬНЫХ УСТРОЙСТВ  -----------------------------------------------------
        // Список аналоговых датчиков выводятся только присутсвующие датчики список вида name:0;
        if (strcmp(str,"get_listPress")==0)     // Функция get_listPress
         {
          for(i=0;i<ANUMBER;i++) if (HP.sADC[i].get_present()){strcat(strReturn,HP.sADC[i].get_name());strcat(strReturn,";");}
          strcat(strReturn,"&") ;    continue;
         } 
        if (strcmp(str,"get_listTemp")==0)     // Функция get_listTemp
         {
          for(i=0;i<TNUMBER;i++) if (HP.sTemp[i].get_present()){strcat(strReturn,HP.sTemp[i].get_name());strcat(strReturn,";");}
          strcat(strReturn,"&") ;    continue;
         } 
        if (strcmp(str,"get_listInput")==0)     // Функция get_listInput
         {
          for(i=0;i<INUMBER;i++) if (HP.sInput[i].get_present()){strcat(strReturn,HP.sInput[i].get_name());strcat(strReturn,";");}
          strcat(strReturn,"&") ;    continue;
         }         
        if (strcmp(str,"get_listRelay")==0)     // Функция get_listRelay
         {
          for(i=0;i<RNUMBER;i++) if (HP.dRelay[i].get_present()){strcat(strReturn,HP.dRelay[i].get_name());strcat(strReturn,";");}
          strcat(strReturn,"&") ;    continue;
         }
        if (strcmp(str,"get_listFlow")==0)     // Функция get_lisFlow
         {
          for(i=0;i<FNUMBER;i++) if (HP.sFrequency[i].get_present()){strcat(strReturn,HP.sFrequency[i].get_name());strcat(strReturn,";");}
          strcat(strReturn,"&") ;    continue;
         }
         
       // -----------------------------------------------------------------------------------------------------        
       // 2. Функции с параметром ------------------------------------------------------------------------------
       // Ищем скобки ------------------------------------------------------------------------------------------
      if (((x=strpbrk(str,"("))!=0)&&((y=strpbrk(str,")"))!=0))  // Функция с одним параметром - найдена открывающиеся и закрывающиеся скобка
       {
       // Выделяем параметр функции на выходе число - номер параметра
       // применяется кодирование 0-19 - температуры 20-29 - сухой контакт 30-39 -аналоговые датчики
       y[0]=0;                                  // Стираем скобку ")"  строка х+1 содержит параметр
       param=-1;                                // по умолчанию параметр не валидный

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
       if (ptr==NULL)                {strcat(strReturn,"E21&");continue;}
       if ((a=atoi(ptr))==0)         {strcat(strReturn,"E22&");continue;}  // если возвращен 0 то ошибка преобразования
       if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23&");continue;}  // проверка диапазона номеров датчиков

       ptr=strtok(NULL,":");
       if (ptr==NULL)                {strcat(strReturn,"E21&");continue;}
       b=100*my_atof(ptr);
       if ((b<-4000)||(b>12000))      {strcat(strReturn,"E24&");continue;}  // проверка диапазона температуры -40...+120
         
       ptr=strtok(NULL,":");
       if (ptr==NULL)                {strcat(strReturn,"E21&");continue;}
       if ((c=10*my_atof(ptr))==0)   {strcat(strReturn,"E22&");continue;}  // если возвращен 0 то ошибка преобразования
       if ((c<-1000)||(c>0))         {strcat(strReturn,"E24&");continue;}  // проверка диапазона уровня сигнала -100...-1 дБ
     
       ptr=strtok(NULL,":");
       if (ptr==NULL)                {strcat(strReturn,"E21&");continue;}
       if ((d=atoi(ptr))==0)         {strcat(strReturn,"E22&");continue;}  // если возвращен 0 то ошибка преобразования
       if ((d<1000)||(d>5000))       {strcat(strReturn,"E24&");continue;}  // проверка диапазона питания 1000...4000 мВ
       
       ptr=strtok(NULL,":");
       if (ptr==NULL)                {strcat(strReturn,"E21&");continue;}
       e=atoi(ptr);
       if (e<0)                      {strcat(strReturn,"E24&");continue;}  // счетичк больше 0
         
       
       // Дошли до сюда - ошибок нет, можно использовать данные
       byte adr[4];
       W5100.readSnDIPR(sock, adr);
       HP.sIP[a-1].set_DataIP(a,b,c,d,e,BytesToIPAddress(adr));
       strcat(strReturn,"OK&"); continue;
      }
 
      if (strstr(str,"get_sensorParamIP"))    // Удаленные датчики - Получить отдельное значение конкретного параметра
      {
       ptr=strtok(x+1,":");     // Нужно
       if (ptr==NULL)                {strcat(strReturn,"E21&");continue;}  // нет параметра
       if ((a=atoi(ptr))==0)         {strcat(strReturn,"E22&");continue;}  // если возвращен 0 то ошибка преобразования не число
       if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23&");continue;}  // проверка диапазона номеров датчиков
       // Получили номер запрашиваемого датчика, теперь определяем параметр
         ptr=strtok(NULL,":");
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
       strcat(strReturn,"&") ; continue;
      }
      
     if (strstr(str,"get_sensorIP"))    // Удаленные датчики - Получить параметры (ВСЕ) удаленного датчика в виде строки
      {
       ptr=x+1; 
       if ((a=atoi(ptr))==0)         {strcat(strReturn,"E22&");continue;}  // если возвращен 0 то ошибка преобразования
       if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23&");continue;}  // проверка диапазона номеров датчиков
       // Формируем строку
       strcat(strReturn,HP.sIP[a-1].get_sensorIP(pSENSOR_NUMBER)); strcat(strReturn,":");
       
       if (HP.sIP[a-1].get_update()>UPDATE_IP)  strcat(strReturn,"-:") ;                       // Время просрочено, удаленный датчик не используем
       else { strcat(strReturn,HP.sIP[a-1].get_sensorIP(pSENSOR_TEMP));strcat(strReturn,":"); }

       if (HP.sIP[a-1].get_count()>0)     // Если были пакеты то выводим данные по ним
       {
           strcat(strReturn,HP.sIP[a-1].get_sensorIP(pSTIME));strcat(strReturn,":");
           strcat(strReturn,HP.sIP[a-1].get_sensorIP(pSENSOR_IP));strcat(strReturn,":");
           strcat(strReturn,HP.sIP[a-1].get_sensorIP(pRSSI));strcat(strReturn,":");
           strcat(strReturn,HP.sIP[a-1].get_sensorIP(pVCC)); strcat(strReturn,":");
           strcat(strReturn,HP.sIP[a-1].get_sensorIP(pSENSOR_COUNT)); strcat(strReturn,":");  
       }
       else strcat(strReturn,"-:-:-:-:-:");  // После включения еще ни разу данные не поступали поэтому прочерки
        
       strcat(strReturn,"&") ; continue; 
      }  
      
      if (strstr(str,"get_sensorListIP"))    // Удаленные датчики - список привязки удаленного датчика
      {
       ptr=x+1; 
       if ((a=atoi(ptr))==0)         {strcat(strReturn,"E22&");continue;}  // если возвращен 0 то ошибка преобразования
       if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23&");continue;}  // проверка диапазона номеров датчиков
       
       strcat(strReturn,"Reset:"); if (HP.sIP[a-1].get_link()==-1) strcat(strReturn,"1;"); else strcat(strReturn,"0;");
       for(i=0;i<TNUMBER;i++)        // формирование списка
           {
               if (HP.sTemp[i].get_present())  // только представленные датчики
               {
               strcat(strReturn,HP.sTemp[i].get_name()); strcat(strReturn,":");
               if (HP.sIP[a-1].get_link()==i) strcat(strReturn,"1;"); else strcat(strReturn,"0;");
               }
           }
       strcat(strReturn,"&") ; continue; 
      }   
      

      if (strstr(str,"get_sensorUseIP"))    // Удаленные датчики - ПОЛУЧИТЬ использование удаленного датчика
      {
       if ((a=atoi(x+1))==0)         {strcat(strReturn,"E22&");continue;}  // если возвращен 0 то ошибка преобразования
       if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23&");continue;}  // проверка диапазона номеров датчиков
       strcat(strReturn,HP.sIP[a-1].get_sensorIP(pSENSOR_USE));strcat(strReturn,"&") ;continue;  
      }

 
      if (strstr(str,"get_sensorRuleIP"))    // Удаленные датчики - ПОЛУЧИТЬ использование усреднение
      {
       if ((a=atoi(x+1))==0)         {strcat(strReturn,"E22&");continue;}  // если возвращен 0 то ошибка преобразования
       if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23&");continue;}  // проверка диапазона номеров датчиков
       strcat(strReturn,HP.sIP[a-1].get_sensorIP(pSENSOR_RULE));strcat(strReturn,"&") ;continue;  
      }
   
      #else
       if (strstr(str,"set_sensorIP"))   {strcat(strReturn,"E25&");continue;}        // Удаленные датчики  НЕ ПОДДЕРЖИВАЕТСЯ
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
   //        Serial.print(pm); Serial.print("   "); Serial.println(HP.get_mode());
           strcat(strReturn,"&") ; continue;
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
          strcat(strReturn,"&") ;    continue;
        } //  if (strcmp(str,"set_testMode")==0)  

         // -----------------------------------------------------------------------------  
        if (strstr(str,"set_EEV"))  // Функция set_EEV
             {
             #ifdef EEV_DEF 
             if ((pm=my_atof(x+1))==ATOF_ERROR)  strcat(strReturn,"E09");      // Ошибка преобразования   - завершить запрос с ошибкой
              else
                {
                if(HP.dEEV.set_EEV((int)pm)==0) 
                strcat(strReturn,int2str(pm));  // если значение правильное его возвращаем сразу
                else strcat(strReturn,"E12"); 
     //           Serial.print("set_EEV ");    Serial.print(pm); Serial.print(" set "); Serial.println(int2str(HP.dEEV.get_EEV())); 
                strcat(strReturn,"&") ;    continue; 
                } 
             #else
                strcat(strReturn,"-&") ;    continue;
             #endif 
               }  //  if (strcmp(str,"set_set_EEV")==0)    
         // -----------------------------------------------------------------------------  
        if (strstr(str,"set_targetFreq"))  // Функция set_EEV
             {
            if ((pm=my_atof(x+1))==ATOF_ERROR)  strcat(strReturn,"E09");      // Ошибка преобразования   - завершить запрос с ошибкой
              else
                {
              //  if(HP.dFC.set_targetFreq(pm*100)==0) strcat(strReturn,ftoa(temp,(float)HP.dFC.get_targetFreq()/100.0,2)); else strcat(strReturn,"E12");  strcat(strReturn,"&") ;    continue; 
                 if(HP.dFC.set_targetFreq(pm*100,true,HP.dFC.get_minFreqUser() ,HP.dFC.get_maxFreqUser())==0) strcat(strReturn,int2str(HP.dFC.get_targetFreq()/100)); else strcat(strReturn,"E12");  strcat(strReturn,"&") ;    continue;   // ручное управление границы максимальны
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
                  if ((pm>=0)&&(pm<I2C_PROFIL_NUM)) strcat(strReturn,int2str(HP.Prof.save((int8_t)pm))); else strcat(strReturn,"E29");  strcat(strReturn,"&") ;    continue; 
                }
               }  //if (strstr(str,"saveProfile"))  
           // -----------------------------------------------------------------------------     
            if (strstr(str,"loadProfile"))  // Функция loadProfile загрузка профиля в текущий
             {
            if ((pm=my_atof(x+1))==ATOF_ERROR)  strcat(strReturn,"E29");      // Ошибка преобразования   - завершить запрос с ошибкой
              else
                {
                  if ((pm>=0)&&(pm<I2C_PROFIL_NUM)) strcat(strReturn,int2str(HP.Prof.load((int8_t)pm))); else strcat(strReturn,"E29");  strcat(strReturn,"&") ;    continue; 
                }
               }  //if (strstr(str,"loadProfile"))  
            // -----------------------------------------------------------------------------     
            if (strstr(str,"infoProfile"))  // Функция infoProfile получить информацию о профиле
             {
            if ((pm=my_atof(x+1))==ATOF_ERROR)  strcat(strReturn,"E29");      // Ошибка преобразования   - завершить запрос с ошибкой
              else
                {
                  if ((pm>=0)&&(pm<I2C_PROFIL_NUM)) HP.Prof.get_info(strReturn,(int8_t)pm); else strcat(strReturn,"E29");  strcat(strReturn,"&") ;    continue; 
                }
               }  //if (strstr(str,"infoProfile"))   
           // -----------------------------------------------------------------------------     
            if (strstr(str,"eraseProfile"))  // Функция eraseProfile стереть профиль
             {
            if ((pm=my_atof(x+1))==ATOF_ERROR)  strcat(strReturn,"E29");      // Ошибка преобразования   - завершить запрос с ошибкой
              else
                {
                  if (pm==HP.Prof.get_idProfile())  {strcat(strReturn,"E30");}  // попытка стереть текущий профиль
                  else if((pm>=0)&&(pm<I2C_PROFIL_NUM)) { strcat(strReturn,int2str(HP.Prof.erase((int8_t)pm))); HP.Prof.update_list(HP.Prof.get_idProfile()); }
                  else strcat(strReturn,"E29"); 
                  strcat(strReturn,"&") ;    continue; 
                }
               }  //if (strstr(str,"eraseProfile"))     
          // -----------------------------------------------------------------------------     
            if (strstr(str,"set_listProfile"))  // Функция set_listProfil загрузить профиль из списка и сразу СОХРАНИТЬ !!!!!!
             {
            if ((pm=my_atof(x+1))==ATOF_ERROR)  strcat(strReturn,"E29");      // Ошибка преобразования   - завершить запрос с ошибкой
              else
                {
                  if ((pm>=0)&&(pm<I2C_PROFIL_NUM)) { HP.Prof.set_list((int8_t)pm); HP.save(); HP.Prof.get_list(strReturn/*,HP.Prof.get_idProfile()*/);} 
                  else strcat(strReturn,"E29");  
                  strcat(strReturn,"&") ;    continue; 
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
   //       { strcat(strReturn,"E04");strcat(strReturn,"&");  continue;  }
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
                  strcat(strReturn,"&"); continue;	 
                  }
               else if (strcmp(str,"set_paramEEV")==0)    // Функция set_paramEEV - установить значение паремтра ЭРВ 
                  {
                  if (pm!=ATOF_ERROR) {   // нет ошибки преобразования
                    if (HP.dEEV.set_paramEEV(x+1,pm)) HP.dEEV.get_paramEEV(x+1,strReturn);
                    else  strcat(strReturn,"E11");  // выход за диапазон значений   
                  } else strcat(strReturn,"E11");   // ошибка преобразования во флоат
                  strcat(strReturn,"&") ; 
                  continue;	 
                  }
                else   strcat(strReturn,"E03&");  continue;	 
              #else
               strcat(strReturn,"no support EEV&");  continue;	 
              #endif   
              }  //  if (strstr(str,"EEV"))    
          // 2. Проверка для запросов содержащих MQTT --------------------------------------------- 
              if (strstr(str,"MQTT"))          // Проверка для запросов содержащих MQTT
              {
			   #ifdef MQTT
                   if (strcmp(str,"get_MQTT")==0){           // Функция получить настройки MQTT
                        HP.clMQTT.get_paramMQTT(x+1,strReturn);
                        strcat(strReturn,"&") ; continue;
                   } else if (strcmp(str,"set_MQTT")==0) {         // Функция записать настройки MQTT
                         if (HP.clMQTT.set_paramMQTT(x+1,strbuf))  HP.clMQTT.get_paramMQTT(x+1,strReturn);   // преобразование удачно
                         else strcat(strReturn,"E32") ; // ошибка преобразования строки
                      strcat(strReturn,"&") ; continue;
                      } // (strcmp(str,"set_MQTT")==0) 
				#else
					 strcat(strReturn,"no support MQTT&");  continue; // не поддерживается
				#endif
               } //if ((strstr(str,"MQTT")>0)
          
           // 3. Расписание -------------------------------------------------------- 
		 #ifdef USE_SCHEDULER // vad711
			// ошибки: E33 - не верный номер расписания, E34 - не хватает места для календаря
			if(strstr(str,"SCHDLR")) { // Класс Scheduler
				x++;
				if(strncmp(str, "set", 3) == 0) { // set_SCHDLR(x=n)
					if((i = HP.Schdlr.web_set_param(x, z+1))) {
						strcat(strReturn, i == 1 ? "E33&" : "E34&");
						continue;
					}
				} else if(strncmp(str, "get", 3) == 0) { // get_SCHDLR(x)
				} else goto x_FunctionNotFound;
				HP.Schdlr.web_get_param(x, strReturn);
				strcat(strReturn,"&");
				continue;
			}
		 #endif

         // 4. Настройки счетчика SDM ------------------------------------------------
               if (strstr(str,"SDM"))          // Проверка для запросов содержащих SDM
                 {
                  #ifdef USE_ELECTROMETER_SDM  	
                   if (strcmp(str,"get_SDM")==0)           // Функция получить настройки счетчика
                      {
                        HP.dSDM.get_paramSDM(x+1,strReturn);
                        strcat(strReturn,"&") ; continue;
                      } // (strcmp(str,"get_SDM")==0) 
                   else if (strcmp(str,"set_SDM")==0)           // Функция записать настройки счетчика
                      {
                       if (HP.dSDM.set_paramSDM(x+1,strbuf)) HP.dSDM.get_paramSDM(x+1,strReturn); // преобразование удачно
                       else                                  strcat(strReturn,"E31") ;            // ошибка преобразования строки
                      strcat(strReturn,"&") ; continue;
                      }  strcat(strReturn,"E03&");  continue;	 
	              	#else
					 strcat(strReturn,"no support SDM&");  continue; // не поддерживается
					#endif   
                  } //if ((strstr(str,"SDM")>0)  

            // 5.  Настройки профилей ---------------------------------------------------------         
		        if (strstr(str,"Profile"))          // Проверка для запросов содержащих Profile
		          {
		             if (strcmp(str,"get_Profile")==0)           // Функция получить настройки профиля
		                  {
		                    HP.Prof.get_paramProfile(x+1,strReturn);
		                    strcat(strReturn,"&") ; continue;
		                  } // (strcmp(str,"get_Profile")==0) 
		              else if (strcmp(str,"set_Profile")==0)           // Функция записать настройки профиля
		                  {
		                   if (HP.Prof.set_paramProfile(x+1,strbuf)) HP.Prof.get_paramProfile(x+1,strReturn); // преобразование удачно
		                   else                                      strcat(strReturn,"E28") ; // ошибка преобразования строки
		                  strcat(strReturn,"&") ; continue;
		                  } else strcat(strReturn,"E03&");  continue;	// (strcmp(str,"set_Profile")==0) 
		           } //if ((strstr(str,"Profile")>0)
		           
             //6.  Настройки Уведомлений --------------------------------------------------------
            if (strstr(str,"Message"))          // Проверка для запросов содержащих messageSetting
             {
              if (strcmp(str,"get_Message")==0)           // Функция get_Message - получить значение настройки уведомлений
                  {
                    HP.message.get_messageSetting(x+1,strReturn);
                    strcat(strReturn,"&") ; continue;
                  } // (strcmp(str,"get_messageSetting")==0) 
              else if (strcmp(str,"set_Message")==0)           // Функция set_Message - установить значениена стройки уведомлений
                  {
                   if (HP.message.set_messageSetting(x+1,strbuf)) HP.message.get_messageSetting(x+1,strReturn); // преобразование удачно
                   else                                                     strcat(strReturn,"E20") ; // ошибка преобразования строки
                  strcat(strReturn,"&") ; continue;
                  } else strcat(strReturn,"E03&");  continue;	// (strcmp(str,"set_messageSetting")==0) 
              } //if ((strstr(str,"messageSetting")>0)   
              
	          //7.  Настройки бойлера --------------------------------------------------
	          if (strstr(str,"Boiler"))          // Проверка для запросов содержащих Boiler
	           {
	              if (strcmp(str,"get_Boiler")==0)           // Функция get_Boiler - получить значение настройки бойлера
	                  {
	                    HP.Prof.get_boiler(x+1,strReturn);
	                    strcat(strReturn,"&") ; continue;
	                  } // (strcmp(str,"get_Boiler")==0) 
	              else if (strcmp(str,"set_Boiler")==0)           // Функция set_Boiler - установить значениена стройки бойлера
	                  {
	                   if (HP.Prof.set_boiler(x+1,strbuf)) HP.Prof.get_boiler(x+1,strReturn);  // преобразование удачно
	                   else 	                      strcat(strReturn,"E19") ; // ошибка преобразования строки
	                  strcat(strReturn,"&") ; continue;
	                  } else strcat(strReturn,"E03&");  continue; // (strcmp(str,"set_Boiler")==0) 
	             } //if ((strstr(str,"Boiler")>0)   

	           //8.  Настройки дата время --------------------------------------------------------
		          if (strstr(str,"datetime"))          // Проверка для запросов содержащих datetime
		             {
		              if (strcmp(str,"get_datetime")==0)           // Функция get_datetim - получить значение даты времени
		                  {
		                    HP.get_datetime(x+1,strReturn);
		                    strcat(strReturn,"&") ; continue;
		                  } // (strcmp(str,"get_datetime")==0) 
   		              else if (strcmp(str,"set_datetime")==0)           // Функция set_datetime - установить значение даты и времени
 		                  {
		                   if (HP.set_datetime(x+1,strbuf))  HP.get_datetime(x+1,strReturn);    // преобразование удачно
	                       else  strcat(strReturn,"E18") ; // ошибка преобразования строки
		                  strcat(strReturn,"&") ; continue;
		                  }  else strcat(strReturn,"E03&");  continue;// (strcmp(str,"set_datetime")==0) 
		            } //if ((strstr(str,"datetime")>0)   

	           //9.  Настройки сети -----------------------------------------------------------
	          if (strstr(str,"Network"))          // Проверка для запросов содержащих Network
	             {
	               if (strcmp(str,"get_Network")==0)           // Функция get_Network - получить значение параметра Network
	                  {
	                    HP.get_network(x+1,strReturn);
	                    strcat(strReturn,"&") ; continue;
	                  } // (strcmp(str,"get_Network")==0) 
	              else if (strcmp(str,"set_Network")==0)           // Функция set_Network - установить значение паремтра Network
	                  {
	                   if (HP.set_network(x+1,strbuf))  HP.get_network(x+1,strReturn);     // преобразование удачно
	                   else strcat(strReturn,"E15") ; // ошибка преобразования строки
	                  strcat(strReturn,"&") ; continue;
	                  }  else strcat(strReturn,"E03&");  continue; // (strcmp(str,"set_Network")==0) 
	             } //if ((strstr(str,"Network")>0)   

		         //10.  Статистика используется в одной функции get_Stat ---------------------------------------
		          if (strstr(str,"Stat"))   // Проверка для запросов содержащих Stat
		           {
		               if (strcmp(str,"get_Stat")==0)           
		                  {
		                  #ifdef I2C_EEPROM_64KB    
		                  HP.Stat.get_Stat(x+1,strReturn, true);
		                  #else
		                  strcat(strReturn,"");
		                  #endif
		                  strcat(strReturn,"&") ; continue;
		                  } else strcat(strReturn,"E03&");  continue; // (strcmp(str,"get_Stat")==0) 
		            } //if ((strstr(str,"Stat")>0) 
                  
		          //11.  Графики смещение  используется в одной функции get_Chart -------------------------------------------
		          if (strstr(str,"Chart"))          // Проверка для запросов содержащих Chart
		             {
		               if (strcmp(str,"get_Chart")==0)           // Функция get_Chart - получить график
		                  {
		                  HP.get_Chart(x+1,strReturn, true);
		                  strcat(strReturn,"&") ; continue;
		                  } else strcat(strReturn,"E03&");  continue;  // (strcmp(str,"get_Chart")==0) 
		            } //if ((strstr(str,"Chart")>0)  
         
                  //12.  Частотный преобразователь -----------------------------------------------------------------
		          if (strstr(str,"FC"))          // Проверка для запросов содержащих FC get_paramFC set_paramFC 
		             {
     	               if (strcmp(str,"get_paramFC")==0)           // Функция get_paramFC - получить значение параметра FC
		                  {
		                    HP.dFC.get_paramFC(x+1,strReturn); strcat(strReturn,"&") ; continue;
		                  } 
		              else if (strcmp(str,"set_paramFC")==0)           // Функция set_paramFC - установить значение паремтра FC
		                  {
							if (pm!=ATOF_ERROR) {   // нет ошибки преобразования
                    		if (HP.dFC.set_paramFC(x+1,pm)) HP.dFC.get_paramFC(x+1,strReturn);
                    		else  strcat(strReturn,"E27");  // выход за диапазон значений   
                  			} else strcat(strReturn,"E11");   // ошибка преобразования во флоат
                           strcat(strReturn,"&") ; 
   		                  } else strcat(strReturn,"E03&");  continue; // (strcmp(str,"set_paramFC")==0) 
		            } //if ((strstr(str,"FC")>0)  

                  // 13 Опции теплового насоса
         	     if (strstr(str,"optionHP"))          // Проверка для запросов содержащих optionHP
		             {         
                      if (strcmp(str,"get_optionHP")==0)           // Функция get_optionHP - получить значение параметра отопления ТН
		                  {
		                   HP.get_optionHP(x+1,strReturn); strcat(strReturn,"&") ; continue; 
		                  } 
	                  else if (strcmp(str,"set_optionHP")==0)           // Функция set_optionHP - установить значение паремтра  опций
				          {
				           if (pm!=ATOF_ERROR) {   // нет ошибки преобразования
				           if (HP.set_optionHP(x+1,pm))   HP.get_optionHP(x+1,strReturn);  // преобразование удачно, 
				           else strcat(strReturn,"E17") ; // выход за диапазон значений
				           } else strcat(strReturn,"E11");   // ошибка преобразования во флоат
				          strcat(strReturn,"&") ; continue;
				          } else strcat(strReturn,"E03&");  continue;   
		             }
		          //14.  Параметры  отопления и охлаждения ТН
		          if (strstr(str,"HP"))          // Проверка для запросов содержащих HP
		             {
		               if (strcmp(str,"get_paramCoolHP")==0)           // Функция get_paramCoolHP - получить значение параметра охлаждения ТН
		                  { HP.Prof.get_paramCoolHP(x+1,strReturn,HP.dFC.get_present()); strcat(strReturn,"&") ; continue;   } 
		              else if (strcmp(str,"set_paramCoolHP")==0)           // Функция set_paramCoolHP - установить значение паремтра охлаждения ТН
		                  {
		                  if (pm!=ATOF_ERROR) {   // нет ошибки преобразования
		                    if (HP.Prof.set_paramCoolHP(x+1,pm))  HP.Prof.get_paramCoolHP(x+1,strReturn,HP.dFC.get_present());    // преобразование удачно
		                    else  strcat(strReturn,"E16") ; // ошибка преобразования строки
		                    } else strcat(strReturn,"E11");   // ошибка преобразования во флоат 
		                  strcat(strReturn,"&") ; continue;
		                  } // (strcmp(str,"paramCoolHP")==0) 
		               else if (strcmp(str,"get_paramHeatHP")==0)           // Функция get_paramHeatHP - получить значение параметра отопления ТН
		                  { HP.Prof.get_paramHeatHP(x+1,strReturn,HP.dFC.get_present()); strcat(strReturn,"&") ; continue;   } 
		               else if (strcmp(str,"set_paramHeatHP")==0)           // Функция set_paramHeatHP - установить значение паремтра отопления ТН
		                  {
		                  if (pm!=ATOF_ERROR) {   // нет ошибки преобразования
		                    if (HP.Prof.set_paramHeatHP(x+1,pm))  HP.Prof.get_paramHeatHP(x+1,strReturn,HP.dFC.get_present());    // преобразование удачно
		                    else  strcat(strReturn,"E16") ; // ошибка преобразования строки
		                    } else strcat(strReturn,"E11");   // ошибка преобразования во флоат 
		                  strcat(strReturn,"&") ; continue;
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
							if((i = Modbus.readHoldingRegisters16(id, par, &par)) == OK) itoa(par, strReturn + strlen(strReturn), 10);
						} else if(*y == 'l') {
								if((i = Modbus.readHoldingRegisters32(id, par, (uint32_t *)&e)) == OK) itoa(e, strReturn + strlen(strReturn), 10);
						} else if(*y == 'i') {
							if((i = Modbus.readInputRegistersFloat(id, par, &pm)) == OK) ftoa(strReturn + strlen(strReturn), pm, 2);
						} else if(*y == 'f') {
							if((i = Modbus.readHoldingRegistersFloat(id, par, &pm)) == OK) ftoa(strReturn + strlen(strReturn), pm, 2);
						} else if(*y == 'c') {
							if((i = Modbus.readCoil(id, par, (boolean *)&par)) == OK) itoa(par, strReturn + strlen(strReturn), 10);;
						} else goto x_FunctionNotFound;
					}
					if(i != OK) {
						strcat(strReturn, "E"); itoa(i, strReturn + strlen(strReturn), 10);
					}
					strcat(strReturn, "&");
					continue;
				}
			}
		}

       // --- УДАЛЕННЫЕ ДАТЧИКИ ----------  кусок кода для удаленного датчика - установка параметров ответ - повторение запроса уже сделали
         #ifdef SENSOR_IP                           // Получение данных удаленного датчика
              
              if (strstr(str,"set_sensorListIP"))    // Удаленные датчики - привязка датчика
              {
               // первое число (имя удаленного датчика)
               if ((x+1)==NULL)              {strcat(strReturn,"E21&");continue;}
               if ((a=atoi(x+1))==0)         {strcat(strReturn,"E22&");continue;}  // если возвращен 0 то ошибка преобразования
               if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23&");continue;}  // проверка диапазона номеров датчиков
               
               // Второе число (новое значение параметра)
               if (strbuf==NULL)             {strcat(strReturn,"E21&");continue;}  
               b=atoi(strbuf);
               if ((b<0)||(b>TNUMBER))       {strcat(strReturn,"E24&");continue;}  //  проверка диапазона номеров датчиков учитываем reset!!
               
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
                 strcat(strReturn,"&") ; continue;     
                
                } 
      
             if (strstr(str,"set_sensorUseIP"))    // Удаленные датчики - УСТАНОВИТЬ использование удаленного датчика
              {
               // первое число (имя удаленного датчика)
               if ((x+1)==NULL)              {strcat(strReturn,"E21&");continue;}
               if ((a=atoi(x+1))==0)         {strcat(strReturn,"E22&");continue;}  // если возвращен 0 то ошибка преобразования
               if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23&");continue;}  // проверка диапазона номеров датчиков
               // Второе число (зновое значение параметра)
               if (strbuf==NULL)             {strcat(strReturn,"E21&");continue;}  
               b=atoi(strbuf);
               if ((b<0)||(b>1))             {strcat(strReturn,"E24&");continue;}  //  проверка диапазона
 
               if (b==1) HP.sIP[a-1].set_fUse(true); else HP.sIP[a-1].set_fUse(false);
               
               HP.updateLinkIP();                 // Обновить ВСЕ привязки удаленных датчиков
               
               strcat(strReturn,int2str(HP.sIP[a-1].get_fUse()));strcat(strReturn,"&") ;continue;  
              }

             if (strstr(str,"set_sensorRuleIP"))    // Удаленные датчики - УСТАНОВИТЬ использование усреднение
              {
               // первое число (имя удаленного датчика)
               if ((x+1)==NULL)              {strcat(strReturn,"E21&");continue;}
               if ((a=atoi(x+1))==0)         {strcat(strReturn,"E22&");continue;}  // если возвращен 0 то ошибка преобразования
               if ((a<1)||(a>IPNUMBER))      {strcat(strReturn,"E23&");continue;}  // проверка диапазона номеров датчиков
               
               if (strbuf==NULL)             {strcat(strReturn,"E21&");continue;}  
               b=atoi(strbuf);
               if ((b<0)||(b>1))             {strcat(strReturn,"E24&");continue;}  //  проверка диапазона
          
               if (b==1) HP.sIP[a-1].set_fRule(true); else HP.sIP[a-1].set_fRule(false);
               strcat(strReturn,int2str(HP.sIP[a-1].get_fRule()));strcat(strReturn,"&") ;continue;  
              }              
         #endif //  #ifdef SENSOR_IP  
       param=-1;
       // Температуры 0-19 смещение 0
            if (strcmp(x+1,"TOUT")==0)           { param=TOUT; }  // Температура улицы
       else if (strcmp(x+1,"TIN")==0)            { param=TIN; }  // Температура в доме
       else if (strcmp(x+1,"TEVAIN")==0)         { param=TEVAIN; }  // Температура на входе испарителя (направление по фреону)
       else if (strcmp(x+1,"TEVAOUT")==0)        { param=TEVAOUT; }  // Температура на выходе испарителя (направление по фреону)
       else if (strcmp(x+1,"TCONIN")==0)         { param=TCONIN; }  // Температура на входе конденсатора (направление по фреону)
       else if (strcmp(x+1,"TCONOUT")==0)        { param=TCONOUT; }  // Температура на выходе конденсатора (направление по фреону)
       else if (strcmp(x+1,"TBOILER")==0)        { param=TBOILER; }  // Температура в бойлере ГВС
       else if (strcmp(x+1,"TACCUM")==0)         { param=TACCUM; }  // Температура на выходе теплоаккмулятора
       else if (strcmp(x+1,"TRTOOUT")==0)        { param=TRTOOUT; } // Температура на выходе RTO (по фреону)
       else if (strcmp(x+1,"TCOMP")==0)          { param=TCOMP; } // Температура нагнетания компрессора
       else if (strcmp(x+1,"TEVAING")==0)        { param=TEVAING;} // Температура на входе испарителя (по гликолю)
       else if (strcmp(x+1,"TEVAOUTG")==0)       { param=TEVAOUTG;} // Температура на выходе испарителя (по гликолю)
       else if (strcmp(x+1,"TCONING")==0)        { param=TCONING;} // Температура на входе конденсатора (по гликолю)
       else if (strcmp(x+1,"TCONOUTG")==0)       { param=TCONOUTG;} // Температура на выходе конденсатора (по гликолю)
       // Сухой контакт 20-25 смещение 20
       /*
       else if (strcmp(x+1,"SEVA")==0)           { param=20;}  // Датчик протока по испарителю
       else if (strcmp(x+1,"SLOWP")==0)          { param=21;}  // Датчик низкого давления
       else if (strcmp(x+1,"SHIGHP")==0)         { param=22;}  // Датчик высокого давления
       else if (strcmp(x+1,"SFROZEN")==0)        { param=23;}  // Датчик заморозки SFROZEN
       */
        else   // Поиск среди имен контактных датчиков смещение 20 (максимум 6)
       {
         for(i=0;i<INUMBER;i++) if(strcmp(x+1,HP.sInput[i].get_name())==0) {param=20+i; break;} 
       }
   //    if (param==-1)  // имя не найдено, дальше разбираем строку
       
       // Частотные датчики смещение 26 (максимум 4)
        if (param==-1) for(i=0;i<FNUMBER;i++) if(strcmp(x+1,HP.sFrequency[i].get_name())==0) {param=26+i; break;} 
       
       if (param==-1)  // имя не найдено, дальше разбираем строку
       {
			 // Аналоговые датчики 30-35 смещение 30  количество до 6 штук
    	   	 if (strcmp(x+1,"PEVA")==0)          		 { param=30;}  //  Датчик давления испарителя
			 else if (strcmp(x+1,"PCON")==0)           	 { param=31;}  //  Датчик давления кондесатора
			 // Реле  36-49 смещение 36  количество до 14 штук
			 else { // Поиск среди имен среди исполнительных устройств
				for(i=0;i<RNUMBER;i++) if (strcmp(x+1,HP.dRelay[i].get_name())==0) {param=36+i; break;}
			 }
       }
 /*      
       if (param==-1)  // имя не найдено, дальше разбираем строку
        {
    
                // Параметры и опции ТН  смещение 60 занимат 40 позиций не 10!!!!!!!!!!!!!!!!!!!! используется в двух разных функциях, единый список
                    if (strcmp(x+1,"RULE")==0)           { param=60;}  // 0  Алгоритм отопления
               else if (strcmp(x+1,"TEMP1")==0)          { param=61;}  // 1  целевая температура в доме
               else if (strcmp(x+1,"TEMP2")==0)          { param=62;}  // 2  целевая температура обратки
               else if (strcmp(x+1,"TARGET")==0)         { param=63;}  // 3  что является целью ПИД - значения  0 (температура в доме), 1 (температура обратки).
               else if (strcmp(x+1,"DTEMP")==0)          { param=64;}  // 4  гистерезис целевой температуры
               else if (strcmp(x+1,"HP_TIME")==0)        { param=65;}  // 5  Постоянная интегрирования времени в секундах ПИД ТН
               else if (strcmp(x+1,"HP_PRO")==0)         { param=66;}  // 6  Пропорциональная составляющая ПИД ТН
               else if (strcmp(x+1,"HP_IN")==0)          { param=67;}  // 7  Интегральная составляющая ПИД ТН
               else if (strcmp(x+1,"HP_DIF")==0)         { param=68;}  // 8  Дифференциальная составляющая ПИД ТН
               else if (strcmp(x+1,"TEMP_IN")==0)        { param=69;}  // 9  температура подачи
               else if (strcmp(x+1,"TEMP_OUT")==0)       { param=70;}  // 10 температура обратки
               else if (strcmp(x+1,"PAUSE")==0)          { param=71;}  // 11 минимальное время простоя компрессора
               else if (strcmp(x+1,"D_TEMP")==0)         { param=72;}  // 12 максимальная разность температур конденсатора
               else if (strcmp(x+1,"TEMP_PID")==0)       { param=73;}  // 13 Целевая темпеартура ПИД
               else if (strcmp(x+1,"WEATHER")==0)        { param=74;}  // 14 Использование погодозависимости
               else if (strcmp(x+1,"K_WEATHER")==0)      { param=75;}  // 15 Коэффициент погодозависимости
        
     
        }
    */    
        if ((pm==ATOF_ERROR)&&((param<170)||(param>320)))        // Ошибка преобразования для чисел но не для строк (смещение 170)! - завершить запрос с ошибкой
          { strcat(strReturn,"E04");strcat(strReturn,"&");  continue;  }
       
       if (param==-1)  { strcat(strReturn,"E02");strcat(strReturn,"&");  continue; }  // Не верный параметр
       x[0]=0;                                                                        // Обрезаем строку до скобки (
       // Все готово к разбору имен функций c параметром
       // 1. Датчики температуры смещение param 0
       if (strstr(str,"Temp"))          // Проверка для запросов содержащих Temp
        {
        if(param>=20)  {strcat(strReturn,"E03");strcat(strReturn,"&");  continue; }  // Не соответсвие имени функции и параметра
         else  // параметр верный
           {    p=param-0;                                // вычисление параметра без смещения
              if (strcmp(str,"get_Temp")==0)              // Функция get_Temp
                { if (HP.sTemp[p].get_present())          // Если датчик есть в конфигурации то выводим значение
                  strcat(strReturn,ftoa(temp,(float)HP.sTemp[p].get_Temp()/100.0,1)); 
                  else strcat(strReturn,"-");             // Датчика нет ставим прочерк
                  strcat(strReturn,"&"); continue; }
               if (strcmp(str,"get_rawTemp")==0)           // Функция get_RawTemp
                { if (HP.sTemp[p].get_present())          // Если датчик есть в конфигурации то выводим значение
                  strcat(strReturn,ftoa(temp,(float)HP.sTemp[p].get_rawTemp()/100.0,1)); 
                  else strcat(strReturn,"-");             // Датчика нет ставим прочерк
                  strcat(strReturn,"&"); continue; }    
               if (strcmp(str,"get_fullTemp")==0)         // Функция get_FulTemp
                { if (HP.sTemp[p].get_present())          // Если датчик есть в конфигурации то выводим значение
                  { 
                    #ifdef SENSOR_IP
                    strcat(strReturn,ftoa(temp,(float)HP.sTemp[p].get_rawTemp()/100.0,1));   // Значение проводного датчика вывод
                    if((HP.sTemp[p].devIP!=NULL)&& (HP.sTemp[p].devIP->get_fUse())&&(HP.sTemp[p].devIP->get_link()>-1)) // Удаленный датчик привязан к данному проводному датчику надо использовать
                       {
                        strcat(strReturn," [");
                        strcat(strReturn,ftoa(temp,(float)HP.sTemp[p].get_Temp()/100.0,1));
                        strcat(strReturn,"]");
                       }
                    #else
                     strcat(strReturn,ftoa(temp,(float)HP.sTemp[p].get_Temp()/100.0,1)); 
                    #endif    
                  }  
                  else strcat(strReturn,"-");             // Датчика нет ставим прочерк
                  strcat(strReturn,"&"); continue; }    
                
              if (strcmp(str,"get_minTemp")==0)           // Функция get_minTemp
                { if (HP.sTemp[p].get_present()) // Если датчик есть в конфигурации то выводим значение
                  strcat(strReturn,ftoa(temp,(float)HP.sTemp[p].get_minTemp()/100.0,1)); 
                  else strcat(strReturn,"-");              // Датчика нет ставим прочерк
                  strcat(strReturn,"&"); continue; }
                
              if (strcmp(str,"get_maxTemp")==0)           // Функция get_maxTemp
                { if (HP.sTemp[p].get_present())          // Если датчик есть в конфигурации то выводим значение
                  strcat(strReturn,ftoa(temp,(float)HP.sTemp[p].get_maxTemp()/100.0,1)); 
                  else strcat(strReturn,"-");             // Датчика нет ставим прочерк
                  strcat(strReturn,"&"); continue; }
                
              if (strcmp(str,"get_errTemp")==0)           // Функция get_errTemp
                { strcat(strReturn,ftoa(temp,(float)HP.sTemp[p].get_errTemp()/100.0,1)); strcat(strReturn,"&"); continue; }
                               
              if (strcmp(str,"get_addressTemp")==0)           // Функция get_addressTemp
                { strcat(strReturn,HP.sTemp[p].get_fAddress() ? addressToHex(HP.sTemp[p].get_address()): "не привязан"); strcat(strReturn,"&"); continue; }
                        
              if (strcmp(str,"get_testTemp")==0)           // Функция get_testTemp
                { strcat(strReturn,ftoa(temp,(float)HP.sTemp[p].get_testTemp()/100.0,1)); strcat(strReturn,"&"); continue; }
                
              if (strcmp(str,"get_errcodeTemp")==0)           // Функция get_errcodeTemp
                 { strcat(strReturn,int2str(HP.sTemp[p].get_lastErr())); strcat(strReturn,"&"); continue; }
                 
              if (strcmp(str,"get_presentTemp")==0)           // Функция get_presentTemp
                  {
                  if (HP.sTemp[p].get_present()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
                  strcat(strReturn,"&") ;    continue;
                  }
              if (strcmp(str,"get_noteTemp")==0)           // Функция get_noteTemp
                 { strcat(strReturn,HP.sTemp[p].get_note()); strcat(strReturn,"&"); continue; }    

             /*    
             if (strcmp(str,"get_targetTemp")==0)           // Функция get_targetTemp резрешены не все датчики при этом.
                 {
                  if (p==1) {strcat(strReturn,ftoa(temp,HP.get_TempTargetIn()/100.0,1));  }
                   else if (p==5)  {strcat(strReturn,ftoa(temp,HP.get_TempTargetCO()/100.0,1)); }
                     else if (p==6)  {strcat(strReturn,ftoa(temp,HP.get_TempTargetBoil()/100.0,1)); }
                       else  strcat(strReturn,"E06");                 // использование имя устанавливаемого параметра «здесь» запрещено
                         strcat(strReturn,"&");  continue;
                 }
               */    
              // ---- SET ----------------- Для температурных датчиков - запросы на УСТАНОВКУ парметров
              if (strcmp(str,"set_testTemp")==0)           // Функция set_testTemp
                 { if (HP.sTemp[p].set_testTemp(pm*100)==OK)    // Установить значение в сотых градуса
                   { strcat(strReturn,ftoa(temp,(float)HP.sTemp[p].get_testTemp()/100.0,1)); strcat(strReturn,"&");  continue;  } 
                    else { strcat(strReturn,"E05");strcat(strReturn,"&");  continue;}       // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
                 }
               if (strcmp(str,"set_errTemp")==0)           // Функция set_errTemp
                  { if (HP.sTemp[p].set_errTemp(pm*100)==OK)    // Установить значение в сотых градуса
                    { strcat(strReturn,ftoa(temp,(float)HP.sTemp[p].get_errTemp()/100.0,1)); strcat(strReturn,"&"); continue; }   
                    else { strcat(strReturn,"E05");strcat(strReturn,"&");  continue;}      // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
                  }

                /*  
              if (strcmp(str,"set_targetTemp")==0)           // Функция set_targetTemp резрешены не все датчики при этом.
                 {
                  if (p==1) {HP.set_TempTargetIn(pm*100);  }
                   else if (p==5)  {HP.set_TempTargetCO(pm*100); }
                     else if (p==6)  {HP.set_TempTargetBoil(pm*100); }
                       else  strcat(strReturn,"E06");                 // использование имя устанавливаемого параметра «здесь» запрещено
                         strcat(strReturn,"&");  continue;
                 }
                 */
               if (strcmp(str,"set_addressTemp")==0)        // Функция set_addressTemp
               {
            	   uint8_t n = pm;
            	   if(n <= TNUMBER)                  // Если индекс находится в диапазоне допустимых значений Здесь индекс начинается с 1, ЗНАЧЕНИЕ 0 - обнуление адреса!!
            	   {
            		   if(n == 0) HP.sTemp[p].set_address(NULL, 0);   // Сброс адреса
            		   else if(OW_scanTable) HP.sTemp[p].set_address(OW_scanTable[n-1].address, OW_scanTable[n-1].bus_type);
            	   }
            	   //      strcat(strReturn,int2str(pm)); strcat(strReturn,"&"); continue;}   // вернуть номер
            	   strcat(strReturn,addressToHex(HP.sTemp[p].get_address())); strcat(strReturn,"&"); continue;
               }  // вернуть адрес
               else { strcat(strReturn,"E08");strcat(strReturn,"&");   continue;}      // выход за диапазон допустимых номеров, значение не установлено

            }  // end else    
         } //if ((strstr(str,"Temp")>0)
           
        // 2.  Датчики аналоговые, давления смещение param 30, ТОЧНОСТЬ СОТЫЕ дипазон 6
        if (strstr(str,"Press"))          // Проверка для запросов содержащих Press
          {
          if ((param>=36)||(param<30))  {strcat(strReturn,"E03");strcat(strReturn,"&");  continue; }  // Не соответсвие имени функции и параметра
          else  // параметр верный
            {   p=param-30;                             // Убрать смещение
              if (strcmp(str,"get_Press")==0)           // Функция get_Press
              { if (HP.sADC[p].get_present())         // Если датчик есть в конфигурации то выводим значение
                {
                    int16_t x=HP.sADC[p].get_Press();
                    strcat(strReturn,ftoa(temp,(float)x/100.0,2));
                    strcat(strReturn," [t:");
                    #ifdef EEV_DEF
                    strcat(strReturn,ftoa(temp,(float)PressToTemp(x,HP.dEEV.get_typeFreon())/100.0,2));strcat(strReturn,"]");
                    #else
                    strcat(strReturn," -.-]");
                    #endif
                }
                else strcat(strReturn,"-");             // Датчика нет ставим прочерк
              strcat(strReturn,"&"); continue; }
                  
              if (strcmp(str,"get_adcPress")==0)           // Функция get_adcPress
                { strcat(strReturn,int2str(HP.sADC[p].get_lastADC())); strcat(strReturn,"&"); continue; }
                
              if (strcmp(str,"get_minPress")==0)           // Функция get_minPress
                { if (HP.sADC[p].get_present())          // Если датчик есть в конфигурации то выводим значение
                  strcat(strReturn,ftoa(temp,(float)HP.sADC[p].get_minPress()/100.0,2)); 
                  else strcat(strReturn,"-");              // Датчика нет ставим прочерк
                strcat(strReturn,"&"); continue; }
                
               if (strcmp(str,"get_maxPress")==0)           // Функция get_maxPress
                { if (HP.sADC[p].get_present())           // Если датчик есть в конфигурации то выводим значение
                  strcat(strReturn,ftoa(temp,(float)HP.sADC[p].get_maxPress()/100.0,2)); 
                  else strcat(strReturn,"-");               // Датчика нет ставим прочерк
                strcat(strReturn,"&"); continue; }
                
              if (strcmp(str,"get_zeroPress")==0)           // Функция get_zeroTPress
                { strcat(strReturn,int2str(HP.sADC[p].get_zeroPress())); strcat(strReturn,"&"); continue; }

               if (strcmp(str,"get_transPress")==0)           // Функция get_transTPress
                { strcat(strReturn,ftoa(temp,(float)HP.sADC[p].get_transADC(),3)); strcat(strReturn,"&"); continue; }
                  
              if (strcmp(str,"get_pinPress")==0)           // Функция get_pinPress
                { strcat(strReturn,"AD"); strcat(strReturn,int2str(HP.sADC[p].get_pinA())); strcat(strReturn,"&"); continue; }
                
              if (strcmp(str,"get_testPress")==0)           // Функция get_testPress
                { strcat(strReturn,ftoa(temp,(float)HP.sADC[p].get_testPress()/100.0,2)); strcat(strReturn,"&"); continue; }

              if (strcmp(str,"get_errcodePress")==0)           // Функция get_errcodePress
                 { strcat(strReturn,int2str(HP.sADC[p].get_lastErr())); strcat(strReturn,"&"); continue; }

              if (strcmp(str,"get_presentPress")==0)           // Функция get_presentPress
                  {
                  if (HP.sADC[p].get_present()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
                  strcat(strReturn,"&") ;    continue;
                  }   
              if (strcmp(str,"get_notePress")==0)           // Функция get_notePress
                 { strcat(strReturn,HP.sADC[p].get_note()); strcat(strReturn,"&"); continue; }    

             // ---- SET ----------------- Для аналоговых  датчиков - запросы на УСТАНОВКУ парметров
              if (strcmp(str,"set_testPress")==0)           // Функция set_testPress
                 { if (HP.sADC[p].set_testPress(pm*100)==OK)    // Установить значение
                   {strcat(strReturn,ftoa(temp,(float)HP.sADC[p].get_testPress()/100.0,2)); strcat(strReturn,"&"); continue; } 
                   else { strcat(strReturn,"E05");strcat(strReturn,"&");  continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
                  }

              if (strcmp(str,"set_transPress")==0)           // Функция set_transPress float
                 { if (HP.sADC[p].set_transADC(pm)==OK)    // Установить значение
                   {strcat(strReturn,ftoa(temp,(float)HP.sADC[p].get_transADC(),3)); strcat(strReturn,"&"); continue;} 
                   else { strcat(strReturn,"E05");strcat(strReturn,"&");  continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
                  }
              
              if (strcmp(str,"set_zeroPress")==0)           // Функция set_zeroTPress
                 { if (HP.sADC[p].set_zeroPress((int16_t)pm)==OK)    // Установить значение
                   {strcat(strReturn,int2str(HP.sADC[p].get_zeroPress())); strcat(strReturn,"&"); continue;} 
                   else { strcat(strReturn,"E05");strcat(strReturn,"&");  continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
                  }    
       
              }  // end else
           } //if ((strstr(str,"Press")>0) 
         
           
         //3.  Датчики сухой контакт смещение param 20
        if (strstr(str,"Input"))          // Проверка для запросов содержащих Input
          {
           if ((param>=26)||(param<20))  {strcat(strReturn,"E03");strcat(strReturn,"&");  continue; }  // Не соответсвие имени функции и параметра
           else  // параметр верный
            {   p=param-20; 
              if (strcmp(str,"get_Input")==0)           // Функция get_Input
                  {
                  if (HP.sInput[p].get_present())          // Если датчик есть в конфигурации то выводим значение
                     { if (HP.sInput[p].get_Input()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);}
                   else strcat(strReturn,"-");               // Датчика нет ставим прочерк
                  strcat(strReturn,"&") ;    continue;
                  } 
               if (strcmp(str,"get_presentInput")==0)           // Функция get_presentInput
                  {
                  if (HP.sInput[p].get_present()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
                  strcat(strReturn,"&") ;    continue;
                  } 
               if (strcmp(str,"get_noteInput")==0)           // Функция get_noteInput
                 { strcat(strReturn,HP.sInput[p].get_note()); strcat(strReturn,"&"); continue; }  
                 
               if (strcmp(str,"get_testInput")==0)           // Функция get_testInput
                  {
                  if (HP.sInput[p].get_testInput()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
                  strcat(strReturn,"&") ;    continue;
                  }  

               if (strcmp(str,"get_alarmInput")==0)           // Функция get_alarmInput
                  {
                  if (HP.sInput[p].get_alarmInput()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
                  strcat(strReturn,"&") ;    continue;
                  }  
                if (strcmp(str,"get_errcodeInput")==0)           // Функция get_errcodeInput
                 { strcat(strReturn,int2str(HP.sInput[p].get_lastErr())); strcat(strReturn,"&"); continue; }
                   
              if (strcmp(str,"get_pinInput")==0)           // Функция get_pinInput
                { strcat(strReturn,"D"); strcat(strReturn,int2str(HP.sInput[p].get_pinD()));
                  strcat(strReturn,"&"); continue; }    
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
                   strcat(strReturn,"&"); continue; }    
               if (strcmp(str,"get_noteInput")==0)           // Функция get_noteInput
                 { strcat(strReturn,HP.sInput[p].get_note()); strcat(strReturn,"&"); continue; }    
                            

             // ---- SET ----------------- Для датчиков сухой контакт - запросы на УСТАНОВКУ парметров
              if (strcmp(str,"set_testInput")==0)           // Функция set_testInput
                 { if (HP.sInput[p].set_testInput((int16_t)pm)==OK)    // Установить значение
                   { if (HP.sInput[p].get_testInput()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero); strcat(strReturn,"&"); continue; } 
                   else { strcat(strReturn,"E05");strcat(strReturn,"&");  continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
                  }
                  
              if (strcmp(str,"set_alarmInput")==0)           // Функция set_alarmInput
                 { if (HP.sInput[p].set_alarmInput((int16_t)pm)==OK)    // Установить значение
                   { if (HP.sInput[p].get_alarmInput()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero); strcat(strReturn,"&"); continue; } 
                   else { strcat(strReturn,"E05");strcat(strReturn,"&");  continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
                  }
             }  // else end 
           } //if ((strstr(str,"Input")>0)  
           
          // 4 Частотные датчики ДАТЧИКИ ПОТОКА
           if (strstr(str,"Flow"))          // Проверка для запросов содержащих Frequency
          {
           if ((param>=30)||(param<26))  {strcat(strReturn,"E03");strcat(strReturn,"&");  continue; }  // Не соответсвие имени функции и параметра
           else  // параметр верный
            {   p=param-26; 
               if (strcmp(str,"get_Flow")==0)           // Функция get_Flow
                  {
                  if (HP.sFrequency[p].get_present())          // Если датчик есть в конфигурации то выводим значение
                   strcat(strReturn,ftoa(temp,(float)HP.sFrequency[p].get_Value()/1000.0,3)); 
                   else strcat(strReturn,"-");               // Датчика нет ставим прочерк
                  strcat(strReturn,"&") ;    continue;
                  }
               if (strcmp(str,"get_frFlow")==0)           // Функция get_frFlow
                  {
                  if (HP.sFrequency[p].get_present())          // Если датчик есть в конфигурации то выводим значение
                   strcat(strReturn,ftoa(temp,(float)HP.sFrequency[p].get_Frequency()/1000.0,3)); 
                   else strcat(strReturn,"-");               // Датчика нет ставим прочерк
                  strcat(strReturn,"&") ;    continue;
                  }
             if (strcmp(str,"get_minFlow")==0)           // Функция get_minFlow
                  {
                  if (HP.sFrequency[p].get_present())          // Если датчик есть в конфигурации то выводим значение
                   strcat(strReturn,ftoa(temp,(float)HP.sFrequency[p].get_minValue()/1000.0,3)); 
                   else strcat(strReturn,"-");               // Датчика нет ставим прочерк
                  strcat(strReturn,"&") ;    continue;
                  }
            if (strcmp(str,"get_kfFlow")==0)           // Функция get_kfFlow
                  {
                  if (HP.sFrequency[p].get_present())          // Если датчик есть в конфигурации то выводим значение
                   strcat(strReturn,ftoa(temp,HP.sFrequency[p].get_kfValue(),3)); 
                   else strcat(strReturn,"-");               // Датчика нет ставим прочерк
                  strcat(strReturn,"&") ;    continue;
                  }
            if (strcmp(str,"get_capacityFlow")==0)           // Функция get_capacityFlow
                  {
                  if (HP.sFrequency[p].get_present())        // Если датчик есть в конфигурации то выводим значение
                   strcat(strReturn,int2str(HP.sFrequency[p].get_Capacity())); 
                   else strcat(strReturn,"-");               // Датчика нет ставим прочерк
                  strcat(strReturn,"&") ;    continue;
                  }                  
            if (strcmp(str,"get_testFlow")==0)           // Функция get_testFlow
                  {
                  if (HP.sFrequency[p].get_present())          // Если датчик есть в конфигурации то выводим значение
                   strcat(strReturn,ftoa(temp,(float)HP.sFrequency[p].get_testValue()/1000.0,3)); 
                   else strcat(strReturn,"-");               // Датчика нет ставим прочерк
                  strcat(strReturn,"&") ;    continue;
                  }      
            if (strcmp(str,"get_pinFlow")==0)              // Функция get_pinFlow
                { strcat(strReturn,"D"); strcat(strReturn,int2str(HP.sFrequency[p].get_pinF()));
                  strcat(strReturn,"&"); continue; } 
            if (strcmp(str,"get_errcodeFlow")==0)           // Функция get_errcodeFlow
                { strcat(strReturn,int2str(HP.sFrequency[p].get_lastErr()));
                  strcat(strReturn,"&"); continue; } 
            if (strcmp(str,"get_noteFlow")==0)               // Функция get_noteFlow
                 { strcat(strReturn,HP.sFrequency[p].get_note()); strcat(strReturn,"&"); continue; }   

            // ---- SET ----------------- Для частотных  датчиков - запросы на УСТАНОВКУ парметров
              if (strcmp(str,"set_testFlow")==0)           // Функция set_testFlow
                 { if (HP.sFrequency[p].set_testValue(pm*1000)==OK)    // Установить значение
                   {strcat(strReturn,ftoa(temp,(float)HP.sFrequency[p].get_testValue()/1000.0,3)); strcat(strReturn,"&"); continue; } 
                   else { strcat(strReturn,"E05");strcat(strReturn,"&");  continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
                  }
              if (strcmp(str,"set_kfFlow")==0)           // Функция set_kfFlow float
                 { if (HP.sFrequency[p].set_kfValue(pm)==OK)    // Установить значение
                   {strcat(strReturn,ftoa(temp,HP.sFrequency[p].get_kfValue(),3)); strcat(strReturn,"&"); continue;} 
                   else { strcat(strReturn,"E05");strcat(strReturn,"&");  continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
                  }
              if (strcmp(str,"set_capacityFlow")==0)           // Функция set_capacityFlow float
                 { 
                   if (HP.sFrequency[p].set_Capacity(pm)==OK)    // Установить значение
                   {  strcat(strReturn,int2str(HP.sFrequency[p].get_Capacity()));  strcat(strReturn,"&"); continue;} 
                   else { strcat(strReturn,"E35");strcat(strReturn,"&");  continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
                  }   
             }  // else end 
           } //if ((strstr(str,"Flow")>0)          
           
          //5.  РЕЛЕ смещение param 36 диапазон 14
          if (strstr(str,"Relay"))          // Проверка для запросов содержащих Relay
             {
             if ((param>=50)||(param<36))  {strcat(strReturn,"E03");strcat(strReturn,"&");  continue; }  // Не соответсвие имени функции и параметра
             else  // параметр верный
               {   p=param-36; 
               if (strcmp(str,"get_Relay")==0)           // Функция get_relay
                  {
                  if (HP.dRelay[p].get_Relay()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
                  strcat(strReturn,"&") ;    continue;
                  } 
               if (strcmp(str,"get_presentRelay")==0)           // Функция get_presentRelay
                  {
                  if (HP.dRelay[p].get_present()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero);
                  strcat(strReturn,"&") ;    continue;
                  } 
               if (strcmp(str,"get_noteRelay")==0)           // Функция get_noteRelay
                 { strcat(strReturn,HP.dRelay[p].get_note()); strcat(strReturn,"&"); continue; }  
                 
               if (strcmp(str,"get_pinRelay")==0)           // Функция get_pinRelay
                { strcat(strReturn,"D"); strcat(strReturn,int2str(HP.dRelay[p].get_pinD())); strcat(strReturn,"&"); continue; }    
 
              // ---- SET ----------------- Для реле - запросы на УСТАНОВКУ парметров
              if (strcmp(str,"set_Relay")==0)           // Функция set_Relay
                 { if (HP.dRelay[p].set_Relay((int16_t)pm)==OK)    // Установить значение
                   { if (HP.dRelay[p].get_Relay()==true)  strcat(strReturn,cOne); else  strcat(strReturn,cZero); strcat(strReturn,"&"); continue; } 
                   else { strcat(strReturn,"E05");strcat(strReturn,"&");  continue;}         // выход за диапазон ПРЕДУПРЕЖДЕНИЕ значение не установлено
                  }
              }  // else end 
          } //if ((strstr(str,"Relay")>0)  5

       // --------------------------------------------------------------------------------------------------------------------------

       
        // ------------------------ конец разбора -------------------------------------------------
x_FunctionNotFound:    
       strcat(strReturn,"E01");                             // функция не найдена ошибка
       strcat(strReturn,"&") ;
       continue;
       } // 2. Функции с параметром
       
  if (str[0]=='&') {break; } // второй символ & подряд признак конца запроса и мы выходим
  strcat(strReturn,"E01");   // Ошибка нет такой команды
  strcat(strReturn,"&") ;
  }
strcat(strReturn,"&") ; // двойной знак закрытие посылки
return count;           // сколько найдено запросов
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

	// journal.jprintf(">%s\n",Socket[thread].inBuf);

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
	else if(strcmp(str_token, "POST") == 0) return HTTP_POST;    // Запрос POST
	else if(strcmp(str_token, "OPTIONS") == 0) return HTTP_POST_;
	#ifdef DEBUG
	journal.jprintf("WEB:Error request %s\n", Socket[thread].inBuf);
	#endif
	return HTTP_invalid;
}

// ========================== P A R S E R  P O S T =================================
// Разбор и обработка POST запроса buf входная строка strReturn выходная
// Сейчас реализована загрузка настроек
// Возврат - true ok  false - error
boolean parserPOST(uint8_t thread)
{
byte *ptr; 
  // Определение начала данных
  if ((ptr=(byte*)strstr((char*)Socket[thread].inPtr,HEADER_BIN))==NULL) {journal.jprintf("Not found header in file.\n");return false;} // Заголовок не найден
  ptr=ptr+strlen(HEADER_BIN);
   // Проверка загловка
  if ((ptr[0]!=0xaa)||(ptr[1]!=0x00)) {journal.jprintf("Invalid header file setting.\n");return false;}                 // не верный заголовок
  if ((ptr[2]+256*ptr[3])!=VER_SAVE)  {journal.jprintf("Invalid version file setting.\n");return false;}                // не совпадение версий
//  len=ptr[4]+256*ptr[5];  // длина данных  по заголовку
   // Чтение настроек
  if (OK!=HP.loadFromBuf(0,ptr)) return false;    
  // Чтение профиля
  if (OK!=HP.Prof.loadFromBuf(HP.headerEEPROM.len,ptr)) return false;
#ifdef USE_SCHEDULER
  if(HP.Schdlr.loadFromBuf(HP.headerEEPROM.len + HP.Prof.get_lenProfile(), ptr) != OK) return false;
#endif
  HP.Prof.update_list(HP.Prof.get_idProfile());                                                                        // обновить список
  return true;
}

