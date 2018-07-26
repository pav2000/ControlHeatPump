/*
Удаленное устройство для проекта "Народный контроллер"

 * "Народный контроллер" для тепловых насосов.
 * Програмноое обеспечение предназначеное для управления 
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

#include <Wire.h>  
#include "SSD1306.h" // библиотека дисплея OLED
#include "OLEDDisplayUi.h"
#include "images.h"
SSD1306  display(0x3c, 4, 5);

extern "C" {
#include "user_interface.h"
uint16 readvdd33(void);}

#include <rBase64.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>        
#include <ESP8266WebServer.h>   
#include <ESP8266SSDP.h>
#include <FS.h>                 
#include <ArduinoJson.h> 

// Web интерфейс для устройства
ESP8266WebServer HTTP(80);


// Для файловой системы
File fsUploadFile;

ADC_MODE(ADC_VCC);


//=================================== Для первого старта устройства необходимо заполнить:=========================================

// Для работы устройства в память чипа необходимо залить папку DATA
// Для загрузки папки DATA (через плагин esp8266fs) небходимо сделать следующее
    //- Плагин положить в папку Arduino/Tools/ESP8266FS/tool/
    //- В настройках чипа выбрать память 1М(128К SPIFFS) или больше на 512 работать не будет
    //- Перед загрузкой перевести плату в режим загрузки (так же как шить прошивку)

String SSDP_Name = "ESP-12";             // Имя удаленного устройства

String _ssid     = "name net";           // Имя точки доступа, к которой должно подключиться удаленное устройство
String _password = "parol";              // Пароль точки доступа, к которой должно подключиться удаленное устройство

String DHCP = "0";                            // 0 - DHCP не используется, 1 - DHCP используется

String const_ip = "192.168.1.112";            // IP адрес удаленного устройства, при подключении к сети WIFI
String const_gw = "192.168.1.1";              // IP адрес шлюза сети WIFI
String const_sn = "255.255.255.0";            // Маска подсети сети WIFI

String _ssidAP = "WiFi-12";                   // Имя точки сети WIFI, которая включается при отсутствии подключения к точки доступа
String _passwordAP = "";                      // пароль точки сети WIFI, которая включается при отсутствии подключения к точки доступа
IPAddress apIP(192, 168, 4, 1);               // IP адрес, по которому будет достпен удаленное устройство, если подключиться к точки доступа не удалось

String host = "77.50.254.24";                 // IP адрес контроллера, к которому подключаем удаленное устройство
String httpPort = "25401";                    // Порт на котором висит контроллер
String login = "admin";                       // Логин доступа к контроллеру
String pass = "admin";                        // Пароль доступа к контроллеру
int interval = 30;                            // Период отправления данных на контроллер

int Id1 = 1;                                  // Порядковый номер датчика №1
int Id2 = 4;                                  // Порядковый номер датчика №2
#define ONE_WIRE_BUS_1 D2                      // ПИН на который подключен датчик 1
#define ONE_WIRE_BUS_2 D8                      // ПИН на который подключен датчик 2

//================================================================================================================================

char ip[20];                                  // Переменная с IP адресом, по которому будет достпен удаленный датчик, после удачного подключения сети WIFI
char gw[20];                                  // Переменная с IP адресом IP адрес шлюза сети WIFI
char sn[20];                                  // Переменная с Маской подсети сети WIFI
String mac_adr;

String url;                                    // Строка, в которой формируется запрос к контроллеру
String url2 = "/&get_infoESP&&";
int _httpPort;                                 // Переменная с Портом контроллера
const char *_host;
int _DHCP = 0;
String st;
String _st;
unsigned int query_count = 1;                  // Счетчик отправленных контроллеру запросов
unsigned long previousMillis = 0;              // Интервальный счетчик
unsigned long currentMillis;
unsigned long curMillis;
unsigned long timeout2;
unsigned long timeout1;
unsigned long uptime;
String up_time;
String logp;
String lp;
String line2;
String line1;
int sek=0;                                      //значение секунд
int minu=0;                                     //значение минут
int chas=0;                                     //значение часов
int day=0;                                      //значение дней

String jsonConfig = "{}";                      // Глобальная переменная файла конфигурации
String PARAM_1="00.00";                        // параметр, полученный от контроллера
String PARAM_2="00.00";                        // параметр, полученный от контроллера
String PARAM_3="00.00";                        // параметр, полученный от контроллера
String PARAM_4="0.000";                        // параметр, полученный от контроллера
String PARAM_5="0000";                         // параметр, полученный от контроллера
String PARAM_6="000";                          // параметр, полученный от контроллера
String PARAM_7="00.00";                        // параметр, полученный от контроллера
String PARAM_8="00.00";                        // параметр, полученный от контроллера
String PARAM_9="0";                            // параметр, полученный от контроллера
String PARAM_10="0";                           // параметр, полученный от контроллера

OneWire oneWire_in(ONE_WIRE_BUS_1);               // Датчик температуры 1
DallasTemperature sensor_inhouse(&oneWire_in);    
OneWire oneWire_out(ONE_WIRE_BUS_2);              // Датчик температуры 2
DallasTemperature sensor_outhouse(&oneWire_out);  
const byte averageFactor = 5;                     // коэффициент сглаживания показаний (0 = не сглаживать)
float sensorValueIN;                              // Усредненное значение температуры 1
float sensorValueOUT;                             // Усредненное значение температуры 2

int voltage;
String XML;
String FreeHeap;


//================================================================================================================================

//================================================== SETUP ========================================================================

void setup() {
  Serial.begin(115200);

display.init();
display.clear();
display.flipScreenVertically();

Oled();
  
  Serial.println("");
  //Запускаем файловую систему
  Serial.println("Start 4-FS");
  FS_init();
  Serial.println("Step7-FileConfig");
  // Загрузка данных из файла config.json
  loadConfig();
  //Настраиваем и запускаем SSDP интерфейс
  Serial.println("Start 3-SSDP");
  SSDP_init();
  //Запускаем WIFI
  Serial.println("Start 1-WIFI");
  WIFIinit();
  //Настраиваем и запускаем HTTP интерфейс
  Serial.println("Start 2-WebServer");
  HTTP_init();



sensor_inhouse.begin();                               // запускаем датчик температуры 1
sensor_outhouse.begin();                              // запускаем датчик температуры 2
sensor_inhouse.requestTemperatures();
sensor_outhouse.requestTemperatures();
sensorValueIN = sensor_inhouse.getTempCByIndex(0);    // Получаем данные температуры 1
sensorValueOUT = sensor_outhouse.getTempCByIndex(0);  // Получаем данные температуры 2

logp = (String (login + ":" + pass));                 // Получаем строку login + pass для кодирования
lp = rbase64.encode(logp);                            // Кодируем строку для отправки контроллеру
Serial.print("logp: ");
Serial.println(logp);
Serial.print("lp: ");
Serial.println(lp);
unsigned char* buf4 = new unsigned char[20];          // Получаем IP адрес контроллера в виде const char *
host.getBytes(buf4, 20, 0);
_host = (const char*)buf4;
Serial.print("_host:");
Serial.println(_host);
_httpPort = httpPort.toInt();                         // Получаем Порт контроллера в виде Int



}
//================================================================================================================================

//================================================== LOOP ========================================================================
  
void loop() {
  
  HTTP.handleClient();
  delay(100);
              //Serial.println();
              //Serial.println("Requesting temperatures...");                                          // опрашиваем датчики температуры
              sensor_inhouse.requestTemperatures();
              sensor_outhouse.requestTemperatures();

  if (averageFactor > 0)                                                                             // усреднение показаний для устранения "скачков"
  {      
    float newsensorValueIN = sensor_inhouse.getTempCByIndex(0);                                 // Для датчика №1

if (newsensorValueIN == -127.00)                                                                    // если датчик не подключен
    {
      //Id1 = 0;                                                                                      // данные по датчику не отправляем
      sensorValueIN = 10.01;                                                                        // на экран выводим 33.33
    }

else {
    
    sensorValueIN = (sensorValueIN * (averageFactor - 1) + newsensorValueIN) / averageFactor;        // <новое среднее> = (<старое среднее>*4 + <текущее значение>) / 5
    sensorValueIN = (sensorValueIN + newsensorValueIN) / 2;                                          // <новое среднее> = (<новое среднее> + <текущее значение>) / 2

}
    
    float newsensorValueOUT = sensor_outhouse.getTempCByIndex(0);                             // Для датчика №2   

if (newsensorValueOUT == -127.00)                                                                    // если датчик не подключен
    {
      //Id2 = 0;                                                                                       // данные по датчику не отправляем
      sensorValueOUT = 33.33;                                                                        // на экран выводим 33.33
    }
else {
     
    sensorValueOUT = (sensorValueOUT * (averageFactor - 1) + newsensorValueOUT) / averageFactor;     // <новое среднее> = (<старое среднее>*4 + <текущее значение>) / 5
    sensorValueOUT = (sensorValueOUT + newsensorValueOUT) / 2;                                       // <новое среднее> = (<новое среднее> + <текущее значение>) / 2
}
  }
    
currentMillis = millis();

if(((millis()/1000)%2==0) && PARAM_10 == "Error") {
Oled_error1();
}

else if(((millis()/1000)%2==0) && PARAM_10 == "EEror") {
Oled_error1();
}

else if(((millis()/1000)%2 - 1 ==0) && PARAM_10 == "Error") {
Oled_error2();
}

else if(((millis()/1000)%2 - 1 ==0) && PARAM_10 == "EEror") {
Oled_error2();
}
else if (PARAM_10 == "0") {
  Oled_Updat();
}
else {
  Oled_ok();
}

if(currentMillis - previousMillis > interval*1000) {                                                 //проверяем не прошел ли нужный интервал, если прошел то
Serial.println("");
Serial.print("=====================Flash size DO ZAPROS: ");
Serial.print(ESP.getFreeHeap());
Serial.println("");
  Zapros();                                                                                          // Запускаем функцию отправки данных на контроллер
  //Oled_ok();
 }

if((millis()/1000)%30==0 && (WiFi.status() == WL_CONNECTED) ){                                        // если секунды равны 30 и есть подключение к сети

Serial.println("");
Serial.print("=====================Flash size DO STATUS: ");
Serial.print(ESP.getFreeHeap());
Serial.println("");  
  Status();                                                                                           // Запускаем функцию получения данных о состояние контроллера
  
//Oled_ok();
}

uptime = millis() / 1000;                                                                              //UPTIME
chas = (uptime/60/60);
if (uptime/60/60<10)
{
up_time = day;
up_time += ":";
up_time += "0";
up_time += chas;
}
  else if (uptime/60/60>23)                                    
   {
     chas = 0;
     day++;                                       
      up_time = day;
      up_time += ":";
      up_time += "0";
      up_time += chas;
   }
   else {
    up_time = day;
      up_time += ":";
      up_time += chas;
   }
   
minu = (uptime/60%60);
if (uptime/60%60<10) 
{
up_time += ":";
up_time += "0";
up_time += minu;
}
else {
  up_time += ":";
  up_time += minu;
}

sek = (uptime%60);

   if ((millis()/1000)%3==0)                                                                           // Вывлдим значение UPTIME и свободной памяти в порт
   {
Serial.print("UPTIME: ");
Serial.print (up_time); Serial.print (":"); Serial.println (sek);
Serial.println("");
Serial.print("Flash size: ");
Serial.print(ESP.getFreeHeap());
Serial.println("");
   }
     
}







