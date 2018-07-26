void Status() {                                                                                  // Функция получения данных о состояние контроллера

unsigned char* buf4 = new unsigned char[20];                                                       // Получаем IP адрес контроллера в виде const char *
host.getBytes(buf4, 20, 0);
const char *_host = (const char*)buf4;

_httpPort = httpPort.toInt();                                                                        // Получаем Порт контроллера в виде Int
FreeHeap = ESP.getFreeHeap();

Serial.print("=====================Flash size DO STATUS: ");
Serial.println(ESP.getFreeHeap(), DEC);
  
  WiFiClient client;

if (!client.connect(_host, _httpPort)) {                                                             // Подключаемся к контроллеру
    Serial.println("Status - connection failed");                                                             //Если неудачно, сообщаем и выходим
    return;
  }
  

  Serial.println();    
  Serial.print("STATUS: ");
  Serial.println(url2);
  Serial.println();
  
  {String logp = login;                                                                                 // Получаем строку login + pass для кодирования
       logp += ":";
       logp += pass;

    String lp = rbase64.encode(logp);                                                                    // Кодируем строку для отправки контроллеру
    
    
    client.print(String("GET ") + url2 + " HTTP/1.1\r\n" +                                          // Формируем запрос и отправляем
               "Host: " + _host + "\r\n" +
               "Authorization: Basic " + lp + "\r\n" +
               "Connection: close\r\n\r\n");}
  unsigned long timeout2 = millis();
  while (client.available() == 0) {
    if (millis() - timeout2 > 5000) {                                                            // Если ответа нет, сообщаем
      Serial.println(">>> Client Timeout !");
      client.flush();
      client.stop();
      return;
    }
  }

  while(client.available()){                                                                     //Если ответ есть, выводим его с порт
    String line2 = client.readStringUntil('\r');
    if (line2.startsWith("&", 1)) {
//Serial.println("Original string: " + line2); //&get_infoESP=26.2;9.5;4.3;0.771 beta;6908;4;05m ;7.89;0.00;Off;&&
line2.replace("&get_infoESP=", "{\"get_infoESP\":[");
line2.replace(";&&", "]}");
//Serial.println("Modified string: " + line2);//{"get_infoESP"=["26.2;9.5;4.3;0.771 beta;6908;4;05m ;7.89;0.00;Off"]} TIN, TOUT, TBOILER, ВЕРСИЯ, ПАМЯТЬ, ЗАГРУЗКА, АПТАЙМ, ПЕРЕГРЕВ, ОБОРОТЫ, СОСТОЯНИЕ.
line2.replace(";", ",");
line2.replace(" beta", "");
line2.replace("d ", ".");
line2.replace("h ", ".");
line2.replace("m ", "");
line2.replace("Off", "0");
line2.replace("Start...", "1");
line2.replace("Stop...", "2");
line2.replace("Pre-heat", "3");
line2.replace("Pre-cooling", "31");
line2.replace("Pre-boiler", "32");
line2.replace("Heating", "33");
line2.replace("Cooling", "34");
line2.replace("Boiler", "35");
line2.replace("Error -", "99");
line2.replace("EEror", "5");
line2.replace("Heat", "6");
line2.replace("Cool", "7");
line2.replace("Boiler", "8");
line2.replace("Pause", "9");
//line2.replace("-", "0.00");
//Serial.println("Modified string2222222: " + line2);//{"get_infoESP"=["26.2,9.5,4.3,0.771 beta,6908,4,01.02.05 ,7.89,0.00,Off"]}

const char* jsonConfig = line2.c_str();

Serial.println(">>>>>>>>>> Dannie POLUCHENY<<<<<<<<<<<");
      client.flush();
      client.stop();

DynamicJsonBuffer jsonBuffer;
JsonObject& root = jsonBuffer.parseObject(jsonConfig);
if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }
String PARAM1 = root["get_infoESP"][0]; //TIN
String PARAM2 = root["get_infoESP"][1]; //TOUT
String PARAM3 = root["get_infoESP"][2]; //TBOILER
String PARAM4 = root["get_infoESP"][3]; //ВЕРСИЯ + beta
String PARAM5 = root["get_infoESP"][4]; //ПАМЯТЬ
String PARAM6 = root["get_infoESP"][5]; //ЗАГРУЗКА
String PARAM7 = root["get_infoESP"][6]; //АПТАЙМ 23d 12h 34m
String PARAM8 = root["get_infoESP"][7]; //ПЕРЕГРЕВ
String PARAM9 = root["get_infoESP"][8]; //ОБОРОТЫ
String PARAM10 = root["get_infoESP"][9]; //СОСТОЯНИЕ

PARAM_1 = PARAM1;
PARAM_2 = PARAM2;
PARAM_3 = PARAM3;
PARAM_4 = String(PARAM4 + " b"); //=  String(stringTwo + " with more"); byte
PARAM_5 = PARAM5;
PARAM_6 = String(PARAM6 + "%");
PARAM_7 = PARAM7;
PARAM7.replace(".", ":");
PARAM_8 = PARAM8;
PARAM_9 = PARAM9;
if (PARAM10 == "0"){
  PARAM_10 = "Off";
}
else if (PARAM10 == "1") {
    PARAM_10 = "Start...";
  }
    else if (PARAM10 == "2") {
    PARAM_10 = "Stop...";
  }
        else if (PARAM10 == "3") {
    PARAM_10 = "Pre-heat";
  }
        else if (PARAM10 == "31") {
    PARAM_10 = "Pre-cooling";
  }
        else if (PARAM10 == "32") {
    PARAM_10 = "Pre-boiler";
  }
        else if (PARAM10 == "33") {
    PARAM_10 = "Heating";
  }
        else if (PARAM10 == "34") {
    PARAM_10 = "Cooling";
  }
          else if (PARAM10 == "35") {
    PARAM_10 = "Boiler";
  }
        else if (PARAM10 > "990") {
    PARAM_10 = "Error";
  }          
          else if (PARAM10 == "5") {
    PARAM_10 = "EEror";
  }
            else if (PARAM10 == "6") {
    PARAM_10 = "Heat";
  }
              else if (PARAM10 == "7") {
    PARAM_10 = "Cool";
  }
                else if (PARAM10 == "8") {
    PARAM_10 = "Boiler";
  }
                  else if (PARAM10 == "9") {
    PARAM_10 = "Pause";
  }
  else{
    PARAM_10 = PARAM10;
  }


//Serial.print("TIN: ");
//Serial.println(PARAM_1);
//Serial.print("TOUT: "); 
//Serial.println(PARAM_2);
//Serial.print("TBOILER: ");
//Serial.println(PARAM_3);
//Serial.print("VERSIYA: ");
//Serial.print(PARAM_4); Serial.println(" beta");
//Serial.print("MEMORY: ");
//Serial.print(PARAM_5); Serial.println("b");
//Serial.print("LOAD: ");
//Serial.print(PARAM_6); Serial.println("%");
//Serial.print("APTIME: ");
//Serial.print(PARAM_7); Serial.println(" (D.H.M)");
//Serial.print("PEREGREV: ");
//Serial.println(PARAM_8);
//Serial.print("REV: ");
//Serial.print(PARAM_9); Serial.println("Hz");
//Serial.print("STATUS: ");
//Serial.println(PARAM_10);
//Serial.print("STATUS s Control: ");
//Serial.println(PARAM10);

    }
  } 

Serial.print("=====================Flash size POSLE STATUS: ");
Serial.println(ESP.getFreeHeap(), DEC);
  
}

