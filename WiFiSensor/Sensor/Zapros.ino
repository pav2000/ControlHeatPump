void Zapros() {
int voltage = ESP.getVcc();                                                                        // сохраняем значение напряжения питания
FreeHeap = ESP.getFreeHeap();                                                                      // Свободно памяти
mac_adr = WiFi.macAddress();                                                                       //Читаем MAC адрес


if (WiFi.status() == WL_CONNECTED)                                                                 // Проверяем подключение к сети и если подключены, включаем клиента
  {
  WiFiClient client;
  delay(10);
  
if (!client.connect(_host, _httpPort)) {                                                             // Подключаемся к контроллеру
    Serial.println("Zapros - connection failed");                                                     //Если неудачно, сообщаем и выходим
    Serial.println("");
    Serial.print("=====================Flash size POSLE ZAPROS: ");
    Serial.println(ESP.getFreeHeap());
      client.stop();
      client.flush();
    return;
  }                                                                                                  //Если удачно, формируем и отправляем строку запроса

  if (Id2 != 0 && Id1 != 0){                                                                         // Формируем строку если два датчика
  url = "/&set_sensorIP("; 
  url += Id1;
  url += ":";
  url += sensorValueIN;
  url += ":";
  url += WiFi.RSSI();
  url += ":";
  url += voltage;
  url += ":";
  url += query_count;
  url += ")&";
  url += "set_sensorIP(";
  url += Id2;
  url += ":";
  url += sensorValueOUT;
  url += ":";
  url += WiFi.RSSI();
  url += ":";
  url += voltage;
  url += ":";
  url += query_count;
  url += ")&&";
  }
   if (Id2 == 0 && Id1 != 0)                                                                        // Формируем строку если подключен только датчик №1
  { 
        url = "/&set_sensorIP("; 
        url += Id1;
        url += ":";
        url += sensorValueIN;
        url += ":";
        url += WiFi.RSSI();
        url += ":";
        url += voltage;
        url += ":";
        url += query_count;
        url += ")&&";
  }

 if (Id2 != 0 && Id1 == 0)                                                                         // Формируем строку если подключен только датчик №2
  { 
        url = "/&set_sensorIP("; 
        url += Id2;
        url += ":";
        url += sensorValueOUT;
        url += ":";
        url += WiFi.RSSI();
        url += ":";
        url += voltage;
        url += ":";
        url += query_count;
        url += ")&&";
         }

if (Id1 == 0 && Id2 == 0 )                                                                        // Формируем запрос если есть датчики и выходим если нет
  { 
  url = "NO ZAPROS";
  Serial.println();    
  Serial.print("ZAPROS: ");
  Serial.println(url);
  Serial.println();
  previousMillis = currentMillis;                                                               // сохраняем время последней попытки
  return;
    }
   else{ 
  Serial.print("ZAPROS: ");
Serial.println();    
Serial.print(url);
Serial.println();    
    
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +                                          // Формируем запрос и отправляем
               "Host: " + _host + "\r\n" +
               "Authorization: Basic " + lp + "\r\n" +
               "Connection: close\r\n\r\n");
  timeout1 = millis();
  while (client.available() == 0) {
    if (millis() - timeout1 > 5000) {                                                            // Если ответа нет, сообщаем и выходим
      Serial.println(">>> Client Timeout !");
      client.stop();
      client.flush();
      char *cls = 0;
      url = cls;
      Serial.println("");
      Serial.print("=====================Flash size POSLE ZAPROS: ");                            // Сколько свободно память после запроса
      Serial.println(ESP.getFreeHeap());
      return;
    }
  }
  while(client.available()){                                                                     //Если ответ есть, выводим в порт
    Serial.println("OTVET POLUCHEN");
    client.stop();
    client.flush();
  }
 }
       }
    else if (wifi_get_opmode () != 2 && wifi_get_opmode () != 3)                                // если подключения к сети нет, перезагружаем WIFI в режим точки доступа
    {
        Serial.println("");
        Serial.println("REGIM =");
        Serial.println(wifi_get_opmode());
        WIFIinit();
     }
     else {
      int n = WiFi.scanNetworks();                                                              // сканируем эфир на наличее сетей WIFI
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");                                                        // Есои сетей нет, выводим сообщение в порт и выходим
  else
  {
    Serial.print(n);                                                                            // Если сети есть, выводим список сетей в порт
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));                                                               // Выводим уровень сигнала найденой сети
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");                        // Если есть шифрование - выводим *
      delay(10);

st = "<html><head><meta http-equiv='Content-Type' content='text/html; charset=UTF-8'/><title>WIFI</title></head><body><ol>";    // Формируем список доступных сетей
  for (int i = 0; i < n; ++i)
    {
      st += "<li><a>";
      st += WiFi.SSID(i);
      st += "<br><h55>";
      st += " Уровень сигнала: ";
      st += WiFi.RSSI(i);
      st += "dB ";
      st += "<br>";
      if (WiFi.encryptionType(i) != ENC_TYPE_NONE){
      st +=" Требуется авторизация</h55>";}
      else {
       st +=" </h55>";
       st +="<h56> Открытая сеть</h56>"; 
      }
      st += "</a></li>";
    }
  st += "</ol></body></html>";
_st = st;
char *cls = 0;
st = cls; 
          
      if (WiFi.SSID(i) == _ssid)                                                                // если имя одной из найденых сетей соответствует имени нашей сети
      {
       WIFIinit();                                                                              // Перезагружаем WIFI, подключаемся к сети 
        }
    }
  }
}
char *cls = 0;                                                                                  // очищаем переменную url
url = cls;
   
  query_count += 1;                                                                             // Увеличиваем счетчик количества отправленных запросов на 1
  previousMillis = currentMillis;                                                               // сохраняем время последнего подключения к контроллеру
  curMillis = millis();                                                                         // Фиксируем время последней отправки данных

Serial.println("");
Serial.print("=====================Flash size POSLE ZAPROS: ");                                 // Свободно памяти
Serial.println(ESP.getFreeHeap());
return;
}

