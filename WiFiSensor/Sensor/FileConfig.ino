// Загрузка данных сохраненных в файл  config.json
bool loadConfig() {
  // Открываем файл для чтения
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
  // если файл не найден  
    Serial.println("Failed to open config file");
  //  Создаем файл запиав в него даные по умолчанию
    saveConfig();
    return false;
  }
  // Проверяем размер файла, будем использовать файл размером меньше 1024 байта
  size_t size = configFile.size();
  if (size > 2048) {
    Serial.println("Config file size is too large");
    return false;
  }


// загружаем файл конфигурации в глобальную переменную
  jsonConfig = configFile.readString();
  // Резервируем памяь для json обекта буфер может рости по мере необходимти предпочтительно для ESP8266 
    DynamicJsonBuffer jsonBuffer;
  //  вызовите парсер JSON через экземпляр jsonBuffer
  //  строку возьмем из глобальной переменной String jsonConfig
    JsonObject& root = jsonBuffer.parseObject(jsonConfig);
  // Теперь можно получить значения из root  
    _ssidAP = root["ssidAPName"].as<String>();
    _passwordAP = root["ssidAPPassword"].as<String>();
    Id2 = root["Id2"];
    Id1 = root["Id1"];
    SSDP_Name = root["SSDPName"].as<String>();
    _ssid = root["ssidName"].as<String>();
    _password = root["ssidPassword"].as<String>();
    host = root["host"].as<String>();
    httpPort = root["httpPort"].as<String>(); 
    interval = root["interval"];
    login = root["login"].as<String>();
    pass = root["pass"].as<String>();
    const_ip= root["const_ip"].as<String>();
    const_gw= root["const_gw"].as<String>();
    const_sn= root["const_sn"].as<String>();
    DHCP = root["DHCP"].as<String>();

    return true;
}



// Запись данных в файл config.json
bool saveConfig() {
  // Резервируем памяь для json обекта буфер может рости по мере необходимти предпочтительно для ESP8266 
  DynamicJsonBuffer jsonBuffer;
  //  вызовите парсер JSON через экземпляр jsonBuffer
  JsonObject& json = jsonBuffer.parseObject(jsonConfig);
  // Заполняем поля json 
  json["SSDPName"] = SSDP_Name;
  json["ssidAPName"] = _ssidAP;
  json["ssidAPPassword"] = _passwordAP;
  json["ssidName"] = _ssid;
  json["ssidPassword"] = _password;
  json["Id2"] = Id2;
  json["Id1"] = Id1;
  json["host"] = host;
  json["httpPort"] = httpPort;
  json["interval"] = interval;
  json["login"] = login;
  json["pass"] = pass;
  json["const_ip"] = const_ip;
  json["const_gw"] = const_gw;
  json["const_sn"] = const_sn;
  json["DHCP"] = DHCP;
  json["temp1"] = sensorValueIN;
  json["temp2"] = sensorValueOUT;
  json["voltage"] = ESP.getVcc() / 1000.00;
  json["time"] = (millis() - curMillis) / 1000;
  json["count"] = query_count -1;
  json["rssid"] = WiFi.RSSI();
  json["mac_adr"] = WiFi.macAddress();
  json["uptime"] = up_time;
  json["Param1"] = PARAM_1;
  json["Param2"] = PARAM_2;
  json["Param3"] = PARAM_3;
  json["Param4"] = PARAM_4;
  json["Param5"] = PARAM_5;
  json["Param6"] = PARAM_6;
  json["Param7"] = PARAM_7;
  json["Param8"] = PARAM_8;
  json["Param9"] = PARAM_9;
  json["Param10"] = PARAM_10;
  json["Param11"] = DHCP;
     // Помещаем созданный json в глобальную переменную json.printTo(jsonConfig);
  json.printTo(jsonConfig);
  // Открываем файл для записи
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    //Serial.println("Failed to open config file for writing");
    return false;
  }
  // Записываем строку json в файл 
  json.printTo(configFile);
  return true;
  }


// Загрузка данных сохраненных в файл  temp.json
bool loadTemp() {
  // Открываем файл для чтения
  File configFile = SPIFFS.open("/temp.json", "r");
  if (!configFile) {
  // если файл не найден  
    Serial.println("Failed to open config file");
  //  Создаем файл запиав в него даные по умолчанию
    saveConfig();
    return false;
  }
  // Проверяем размер файла, будем использовать файл размером меньше 1024 байта
  size_t size = configFile.size();
  if (size > 2048) {
    Serial.println("Config file size is too large");
    return false;
  }
}

// Запись данных в файл temp.json
bool saveTemp() {
  // Резервируем памяь для json обекта буфер может рости по мере необходимти предпочтительно для ESP8266 
  DynamicJsonBuffer jsonBuffer;
  //  вызовите парсер JSON через экземпляр jsonBuffer
  JsonObject& json = jsonBuffer.parseObject(jsonConfig);
  // Заполняем поля json 
  json["temp1"] = sensorValueIN;
  json["temp2"] = sensorValueOUT;
  json["voltage"] = ESP.getVcc() / 1000.00;
  json["time"] = (millis() - curMillis) / 1000;
  json["count"] = query_count -1;
  json["rssid"] = WiFi.RSSI();
  json["mac_adr"] = WiFi.macAddress();
  json["uptime"] = up_time;
  
  
   // Помещаем созданный json в глобальную переменную json.printTo(jsonConfig);
  json.printTo(jsonConfig);
  // Открываем файл для записи
  File configFile = SPIFFS.open("/temp.json", "w");
  if (!configFile) {
    //Serial.println("Failed to open config file for writing");
    return false;
  }
  // Записываем строку json в файл 
  json.printTo(configFile);
  return true;
  }


// Загрузка данных сохраненных в файл  config.json
bool loadEsp() {
  // Открываем файл для чтения
  File configFile = SPIFFS.open("/esp.json", "r");
  if (!configFile) {
  // если файл не найден  
    Serial.println("Failed to open config file");
  //  Создаем файл запиав в него даные по умолчанию
    saveConfig();
    return false;
  }
  // Проверяем размер файла, будем использовать файл размером меньше 1024 байта
  size_t size = configFile.size();
  if (size > 2048) {
    Serial.println("Config file size is too large");
    return false;
  }
}

// Запись данных в файл esp.json
bool saveEsp() {
  // Резервируем памяь для json обекта буфер может рости по мере необходимти предпочтительно для ESP8266 
  DynamicJsonBuffer jsonBuffer;
  //  вызовите парсер JSON через экземпляр jsonBuffer
  JsonObject& json = jsonBuffer.parseObject(jsonConfig);
  // Заполняем поля json 
  json["Param1"] = PARAM_1;
  json["Param2"] = PARAM_2;
  json["Param3"] = PARAM_3;
  json["Param4"] = PARAM_4;
  json["Param5"] = PARAM_5;
  json["Param6"] = PARAM_6;
  json["Param7"] = PARAM_7;
  json["Param8"] = PARAM_8;
  json["Param9"] = PARAM_9;
  json["Param10"] = PARAM_10;
  json["Param11"] = DHCP;
    
   // Помещаем созданный json в глобальную переменную json.printTo(jsonConfig);
  json.printTo(jsonConfig);
  // Открываем файл для записи
  File configFile = SPIFFS.open("/esp.json", "w");
  if (!configFile) {
    //Serial.println("Failed to open config file for writing");
    return false;
  }
  // Записываем строку json в файл 
  json.printTo(configFile);
  return true;
  }







