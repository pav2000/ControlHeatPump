void HTTP_init(void) {

// Включаем работу с файловой системой
  FS_init();

  // SSDP дескриптор
  HTTP.on("/description.xml", HTTP_GET, []() {
    SSDP.schema(HTTP.client());
  });
  

  HTTP.on("/configs.json", handle_ConfigJSON);  // формирование configs.json страницы для передачи данных в web интерфейс
  HTTP.on("/temps.json", handle_tempJSON);  // формирование temp.json страницы для передачи данных в web интерфейс
  HTTP.on("/esps.html", handle_espJSON);  // формирование esp.json страницы для передачи данных в web интерфейс
  // API для устройства
  HTTP.on("/ssdp", handle_Set_Ssdp);            // Установить имя SSDP устройства
  HTTP.on("/ssid", handle_Set_Ssid);            // Установить имя и пароль роутера
  HTTP.on("/ssidap", handle_Set_Ssidap);        // Установить имя и пароль для точки доступа
  HTTP.on("/Id2", handle_Id2);                  // Установка порядкового номера датчика №2
  HTTP.on("/Contrl", handle_host);              // Установка IP и порта контроллера
  HTTP.on("/Login", handle_login);              // Установка логина и пароля доступа к контроллеру
  HTTP.on("/Id", handle_set_Id1);               // Установка порядкового номера датчика №1
  HTTP.on("/Interval", handle_set_interval);    // Установка интервала отправки данных
  HTTP.on("/restart", handle_Restart);          // Перезагрузка модуля
  HTTP.on("/xml",handleXML); // формирование xml страницы для передачи данных в web интерфейс
  // Запускаем HTTP сервер
  HTTP.begin();

}

// Функции API-Set
void handle_Set_Ssdp() {
  SSDP_Name = HTTP.arg("ssdp");               // Получаем значение ssdp из запроса сохраняем в глобальной переменной
  saveConfig();                               // Функция сохранения данных во Flash пока пустая
  HTTP.send(200, "text/plain", "OK");         // отправляем ответ о выполнении
}

void handle_host() {
  host = HTTP.arg("host");                    // Получаем значение IP контроллера из запроса сохраняем в глобальной переменной
  unsigned char* buf4 = new unsigned char[20];     // Получаем IP адрес контроллера в виде const char *
  host.getBytes(buf4, 20, 0);
  _host = (const char*)buf4;
  httpPort = HTTP.arg("httpPort");            // Получаем значение порта контроллера из запроса сохраняем в глобальной переменной
  _httpPort = httpPort.toInt();               // Получаем Порт контроллера в виде Int
  saveConfig();
  HTTP.send(200, "text/plain", "OK");         // отправляем ответ о выполнении
  WIFIinit();
}

void handle_login() {
  login = HTTP.arg("login");                  // Получаем значение логина подключения к контроллеру из запроса сохраняем в глобальной переменной
  pass = HTTP.arg("pass");                    // Получаем значение пароля подключения к контроллеру из запроса сохраняем в глобальной переменной
  logp = login;                               // Получаем строку login + pass для кодирования
       logp += ":";
       logp += pass;
  lp = rbase64.encode(logp);                  // Кодируем строку для отправки контроллеру
  saveConfig();                               // Функция сохранения данных во Flash пока пустая
  HTTP.send(200, "text/plain", "OK");         // отправляем ответ о выполнении
}

void handle_Set_Ssid() {
  _ssid = HTTP.arg("ssid");                   // Получаем значение ssid из запроса сохраняем в глобальной переменной
  _password = HTTP.arg("password");           // Получаем значение password из запроса сохраняем в глобальной переменной
  const_ip = HTTP.arg("const_ip");            // Получаем значение статического IP из запроса сохраняем в глобальной переменной
  const_gw = HTTP.arg("const_gw");            // Получаем значение IP шлюза из запроса сохраняем в глобальной переменной
  const_sn = HTTP.arg("const_sn");            // Получаем значение маски шлюза контроллера из запроса сохраняем в глобальной переменной
  DHCP = HTTP.arg("DHCP");                    // Получаем значение DHCP из запроса сохраняем в глобальной переменной
  saveConfig();                               // Функция сохранения данных во Flash пока пустая
  HTTP.send(200, "text/plain", "OK");         // отправляем ответ о выполнении
}

void handle_Set_Ssidap() {              
  _ssidAP = HTTP.arg("ssidAP");               // Получаем значение ssidAP из запроса сохраняем в глобальной переменной
  _passwordAP = HTTP.arg("passwordAP");       // Получаем значение passwordAP из запроса сохраняем в глобальной переменной
  saveConfig();                               // Функция сохранения данных во Flash пока пустая
  HTTP.send(200, "text/plain", "OK");         // отправляем ответ о выполнении
}

void handle_Id2() {               
  Id2 = HTTP.arg("Id2").toInt();              // Получаем значение порядкового номера датчика №2 из запроса конвертируем в int сохраняем в глобальной переменной
  saveConfig();
  HTTP.send(200, "text/plain", "OK");         // отправляем ответ о выполнении
}

void handle_set_Id1() {               
  Id1 = HTTP.arg("Id1").toInt();              // Получаем значение порядкового номера датчика №2 из запроса конвертируем в int сохраняем в глобальной переменной
  saveConfig();
  HTTP.send(200, "text/plain", "OK");         // отправляем ответ о выполнении
}

void handle_set_interval() {               
  interval = HTTP.arg("interval").toInt();    // Получаем значение интервала отправки данных из запроса конвертируем в int сохраняем в глобальной переменной
  saveConfig();
  HTTP.send(200, "text/plain", "OK");         // отправляем ответ о выполнении
}

void handle_Restart() {
  String restart = HTTP.arg("device");              // Получаем значение device из запроса
  if (restart == "ok") {                            // Если значение равно Ок
    HTTP.send(200, "text / plain", "Reset OK");     // Oтправляем ответ Reset OK
    WIFIinit();                                     // перезагружаем модуль
  }
  else {                                            // иначе
    HTTP.send(200, "text / plain", "No Reset");     // Oтправляем ответ No Reset
  }
}

void handle_ConfigJSON() {
  String json = "{";                                // Формировать строку для отправки в браузер json формат
  json += "\"SSDP\":\"";                            // Имя SSDP
  json += SSDP_Name;
  json += "\",\"ssid\":\"";                         // Имя сети
  json += _ssid;
  json += "\",\"password\":\"";                     // Пароль сети
  json += _password;
  json += "\",\"ssidAP\":\"";                       // Имя точки доступа
  json += _ssidAP;
  json += "\",\"passwordAP\":\"";                   // Пароль точки доступа
  json += _passwordAP;
  json += "\",\"Id2\":\"";                          // Номер датчика №2
  json += Id2;
  json += "\",\"Id1\":\"";                          // Номер датчика №1
  json += Id1;
  json += "\",\"interval\":\"";                     // Интервал отправки данных
  json += interval;
  json += "\",\"host\":\"";                         // IP контроллера
  json += host;
  json += "\",\"httpPort\":\"";                     // Порт контроллера
  json += httpPort;
  json += "\",\"login\":\"";                        // Логин доступа к контроллеру
  json += login;
  json += "\",\"pass\":\"";                         // Пароль доступа к контроллеру
  json += pass;
  json += "\",\"ip\":\"";                           // IP устройства
  json += WiFi.localIP().toString();
  json += "\",\"const_ip\":\"";                     // статический IP при подключении к WIFI
  json += const_ip;
  json += "\",\"const_gw\":\"";                     // IP шлюза сети WIFI
  json += const_gw;
  json += "\",\"const_sn\":\"";                     // Маска подсети сети WIFI
  json += const_sn; 
  json += "\",\"DHCP\":\"";                         // использовать DHCP
  json += DHCP;
  json += "\",\"temp1\":\"";                        // Температура датчика №1
  json += sensorValueIN;
  json += "\",\"temp2\":\"";                        // Температура датчика №2
  json += sensorValueOUT;
  json += "\",\"voltage\":\"";                      // Напряжение питания
  json += ESP.getVcc() / 1000.00;
  json += "\",\"time\":\"";                         // Время с последней отправки данных
  json += (millis() - curMillis) / 1000;
  json += "\",\"count\":\"";                        // Количество отправленных данных
  json += query_count -1;
  json += "\",\"rssid\":\"";                        // Уровень сигнала WIFI
  json += WiFi.RSSI();
  json += "\",\"mac_adr\":\"";                      // MAC адрес
  json += WiFi.macAddress();
  json += "\",\"uptime\":\"";                       // Время с последней перезагрузки
  json += up_time;
  json += "\"}";
  HTTP.send(200, "text/json", json);
}


void handle_tempJSON() {
  String json = "{";                                 // Формировать строку для отправки в браузер json формат
  json += "\"temp1\":\"";                            // Температура датчика №1
  json += sensorValueIN;
  json += "\",\"temp2\":\"";                         // Температура датчика №2
  json += sensorValueOUT;
  json += "\",\"voltage\":\"";                       // Напряжение питания
  json += ESP.getVcc() / 1000.00;
  json += "\",\"time\":\"";                          // Время с последней отправки данных
  json += (millis() - curMillis) / 1000;
  json += "\",\"count\":\"";                          // Количество отправленных данных
  json += query_count -1;
  json += "\",\"rssid\":\"";                          // Уровень сигнала WIFI
  json += WiFi.RSSI();
  json += "\",\"mac_adr\":\"";                        // MAC адрес
  json += WiFi.macAddress();
  json += "\",\"uptime\":\"";                         // Время с последней перезагрузки
  json += up_time;
  json += "\"}";
  HTTP.send(200, "text/json", json);
}

void handle_espJSON() {
  String json = _st;                                  // Список длступных сетей WIFI
  HTTP.send(200, "text/html", json);
}

void handleXML(){
  buildXML();                                         // Данные с устройства и с контроллера
  HTTP.send(200,"text/xml",XML);
}
// создаем xml данные
void buildXML(){
  
  double getVcc = ESP.getVcc() / 1000.00;
  int Time = (millis() - curMillis) / 1000;
  int query = query_count -1;
  
  XML="<?xml version='1.0'?>";
  XML+="<Donnees>"; 
    XML+="<temp1>";
    XML+=sensorValueIN;
    XML+="</temp1>";
    XML+="<temp2>";
    XML+=sensorValueOUT;
    XML+="</temp2>";
    XML+="<voltage>";
    XML+=getVcc;
    XML+="</voltage>";
    XML+="<time>";
    XML+=Time;
    XML+="</time>";
    XML+="<count>";
    XML+=query;
    XML+="</count>";
    XML+="<rssid>";
    XML+=WiFi.RSSI();
    XML+="</rssid>";
    XML+="<mac_adr>";
    XML+=WiFi.macAddress();
    XML+="</mac_adr>";
    XML+="<uptime>";
    XML+=up_time;
    XML+="</uptime>";
    XML+="<Param1>";
    XML+=PARAM_1;
    XML+="</Param1>";
    XML+="<Param2>";
    XML+=PARAM_2;
    XML+="</Param2>";
    XML+="<Param3>";
    XML+=PARAM_3;
    XML+="</Param3>";
    XML+="<Param4>";
    XML+=PARAM_4;
    XML+="</Param4>";
    XML+="<Param5>";
    XML+=PARAM_5;
    XML+="</Param5>";
    XML+="<Param6>";
    XML+=PARAM_6;
    XML+="</Param6>";
    XML+="<Param7>";
    XML+=PARAM_7;
    XML+="</Param7>";
    XML+="<Param8>";
    XML+=PARAM_8;
    XML+="</Param8>";
    XML+="<Param9>";
    XML+=PARAM_9;
    XML+="</Param9>";
    XML+="<Param10>";
    XML+=PARAM_10;
    XML+="</Param10>";
    XML+="<Param11>";
    XML+=DHCP;
    XML+="</Param11>";
    XML+="<FreeHeap>";
    XML+=FreeHeap;
    XML+="</FreeHeap>";
    XML+="<up_time>";
    XML+=up_time;
    XML+="</up_time>";
  XML+="</Donnees>"; 
}

