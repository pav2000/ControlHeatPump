
//=================================================== Start WIFI_STA ==========================================================================

void WIFIinit() { 
  // Попытка подключения к точке доступа

_DHCP = DHCP.toInt(); 
byte tries = 10;
mac_adr = WiFi.macAddress();

int n = WiFi.scanNetworks();                                                                    // сканируем эфир на наличее сетей WIFI
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

st = "<html><head><meta http-equiv='Content-Type' content='text/html; charset=UTF-8'/><title>WIFI</title></head><body><ol>"; // Список сетей WIFI
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
      st +=" Требуется авторизация</h55>";}                                                       // Если сеть с шифрованием
      else {
       st +=" </h55>";
       st +="<h56> Открытая сеть</h56>";                                                          // Если открытая сеть
      }
      st += "</a></li>";
    }
  st += "</ol></body></html>";
_st = st;
char *cls = 0;
st = cls;        
    }
  }
  
if (_DHCP == 0)                                                                                   // Подключаемся к WIFI Если работаем со статическим IP
  {
unsigned char* buf = new unsigned char[20];                                                       // Получаем статический IP адрес в виде const char *
const_ip.getBytes(buf, 20, 0);
const char* ip = (const char*)buf;

unsigned char* buf2 = new unsigned char[20];                                                      // Получаем IP адрес шлюза в виде const char *
const_gw.getBytes(buf2, 20, 0);
const char* gw = (const char*)buf2;

unsigned char* buf3 = new unsigned char[20];                                                      // Получаем маску подсети шлюза в виде const char *
const_sn.getBytes(buf3, 20, 0);
const char* sn = (const char*)buf3;

  
   IPAddress _ip,_gw,_sn;
  _ip.fromString(ip);
  _gw.fromString(gw);
  _sn.fromString(sn);

  WiFi.mode(WIFI_STA);
  delay(1000);
  
     WiFi.config(_ip, _gw, _sn);
     WiFi.begin(_ssid.c_str(), _password.c_str());
       }
      
  else {                                                                                          // Подключаемся к WIFI получая адорес от DHCP

    WiFi.mode(WIFI_STA);
   delay(1000);
    WiFi.begin(_ssid.c_str(), _password.c_str()); }       
                                                                                                  // Делаем проверку подключения до тех пор пока счетчик tries
                                                                                                  // не станет равен нулю или не получим подключение
  while (--tries && WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  if (WiFi.status() != WL_CONNECTED)
  {
                                                                                                  // Если не удалось подключиться запускаем в режиме AP
    Serial.println("");
    Serial.println("WiFi up AP");
    StartAPMode();
  }
  else {
                                                                                                  // Иначе удалось подключиться отправляем сообщение о подключении и выводим адрес IP
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}


//=================================================== StartAPMode ==========================================================================

bool StartAPMode()                                                                                // Включаем WIFI в режиме точки доступа
{                                                                                                 // Отключаем WIFI
  WiFi.disconnect();
                                                                                                  // Меняем режим на режим точки доступа
  WiFi.mode(WIFI_AP);
                                                                                                  // Задаем настройки сети
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
                                                                                                  // Включаем WIFI в режиме точки доступа с именем и паролем хронящихся в переменных _ssidAP _passwordAP
  WiFi.softAP(_ssidAP.c_str(), _passwordAP.c_str());
  Serial.println("");
    Serial.println("WiFi start");
    Serial.println("IP address: ");
    Serial.println(WiFi.softAPIP());

  return true;
}
