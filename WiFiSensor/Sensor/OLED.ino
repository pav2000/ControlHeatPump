void Oled() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_16);
      display.drawString(64, 0, "CONTROLLER");
      display.drawString(64, 16, "HEAT PUMP");
      display.setFont(ArialMT_Plain_10);
      display.drawString(64, 40, "ESP REMOTE SENSOR");
      display.display();
}

void Oled_Updat() {

  display.clear();
       
  if (Id2 != 0 && Id1 != 0) {    
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(1, 15, "Sensor #1 °C");
    
    
    if (sensorValueIN > -10.00){
      display.setFont(ArialMT_Plain_24);
      display.drawString(1, 25, String(sensorValueIN));
    }
    else {
      display.setFont(ArialMT_Plain_16);
      display.drawString(5, 30, String(sensorValueIN));
    }
    display.setFont(ArialMT_Plain_10);
    display.drawString(68, 15, "Sensor #2 °C");
    
    if (sensorValueOUT > -10.00){
      display.setFont(ArialMT_Plain_24);
      display.drawString(68, 25, String(sensorValueOUT));
    }
    else {
      display.setFont(ArialMT_Plain_16);
      display.drawString(73, 30, String(sensorValueOUT));
    }
    
}

  else if (Id2 == 0 && Id1 != 0){
   display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 15, "Sensor #1 °C");
    display.setFont(ArialMT_Plain_24);
    display.drawString(64, 25, String(sensorValueIN));
     
}
else if (Id2 != 0 && Id1 == 0) {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 15, "Sensor #2 °C");
    display.setFont(ArialMT_Plain_24);
    display.drawString(64, 25, String(sensorValueOUT));
    
}
else if (Id1 == 0 && Id2 == 0 ) {
  display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 13, "NO SENSOR"); //PARAM_1
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 31, "TIN °C " + String(PARAM_1));
    display.drawString(64, 41, "TOUT °C " + String(PARAM_2));
    
}


  if (wifi_get_opmode () != 2 && wifi_get_opmode () != 3){
    display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 0, "Updating controller data");
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 54, "WiFi OK! IP" + String(const_ip) );
 
}
  else {

  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 0, "No network connection...");
  display.drawString(64, 54, "WiFi AP IP 192.168.4.1");
  
}
display.display();
}

void Oled_ok() {

display.clear();
       
    
  if (Id2 != 0 && Id1 != 0) {    
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(1, 15, "Sensor #1 °C");
    
    
    if (sensorValueIN > -10.00){
      display.setFont(ArialMT_Plain_24);
      display.drawString(1, 25, String(sensorValueIN));
    }
    else {
      display.setFont(ArialMT_Plain_16);
      display.drawString(5, 30, String(sensorValueIN));
    }
    display.setFont(ArialMT_Plain_10);
    display.drawString(68, 15, "Sensor #2 °C");
    display.setFont(ArialMT_Plain_24);

    if (sensorValueOUT > -10.00){
      display.setFont(ArialMT_Plain_24);
      display.drawString(68, 25, String(sensorValueOUT));
    }
    else {
      display.setFont(ArialMT_Plain_16);
      display.drawString(73, 30, String(sensorValueOUT));
    }
    
}

  else if (Id2 == 0 && Id1 != 0){
   display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 15, "Sensor #1 °C");
    display.setFont(ArialMT_Plain_24);
    display.drawString(64, 25, String(sensorValueIN));
 
}
else if (Id2 != 0 && Id1 == 0) {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 15, "Sensor #2 °C");
    display.setFont(ArialMT_Plain_24);
    display.drawString(64, 25, String(sensorValueOUT));
   
}
else if (Id1 == 0 && Id2 == 0 ) {
  display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 13, "NO SENSOR");
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 31, "TIN °C " + String(PARAM_1));
    display.drawString(64, 41, "TOUT °C " + String(PARAM_2));
   
}

  if (wifi_get_opmode () != 2 && wifi_get_opmode () != 3) {
    display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.setFont(ArialMT_Plain_10);
      display.drawString(0, 0, "Param OK");
      display.setTextAlignment(TEXT_ALIGN_RIGHT);
      display.drawString(128, 0, "Mode:" + String(PARAM_10));
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 54, "WiFi OK! IP" + String(const_ip) );
   
}
    else {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 0, "No network connection...");
    display.drawString(64, 54, "WiFi AP IP 192.168.4.1");
   
}
display.display();
}

void Oled_error1() {
      display.clear();
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_16);
      display.drawString(64, 0, "WARNING!");
      display.drawString(64, 14, "HP - ERROR!");
      
if (Id2 != 0 && Id1 != 0) {    
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(1, 32, "Sensor #1 °C");
    display.setFont(ArialMT_Plain_24);
    display.drawString(1, 42, String(sensorValueIN));
    display.setFont(ArialMT_Plain_10);
    display.drawString(68, 32, "Sensor #2 °C");
    display.setFont(ArialMT_Plain_24);
    display.drawString(68, 42, String(sensorValueOUT));
    display.display();
}

  else if (Id2 == 0 && Id1 != 0){
   display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 32, "Sensor #1 °C");
    display.setFont(ArialMT_Plain_24);
    display.drawString(64, 42, String(sensorValueIN));
    display.display(); 
}
else if (Id2 != 0 && Id1 == 0) {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 32, "Sensor #2 °C");
    display.setFont(ArialMT_Plain_24);
    display.drawString(64, 42, String(sensorValueOUT));
    display.display();
}
else if (Id1 == 0 && Id2 == 0 ) {
  display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 40, "NO SENSOR");
    display.display();
}
      
}

void Oled_error2() {
      display.clear();
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_16);
      display.drawString(64, 0, "    ");
      
if (Id2 != 0 && Id1 != 0) {    
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(1, 32, "Sensor #1 °C");
    display.setFont(ArialMT_Plain_24);
    display.drawString(1, 42, String(sensorValueIN));
    display.setFont(ArialMT_Plain_10);
    display.drawString(68, 32, "Sensor #2 °C");
    display.setFont(ArialMT_Plain_24);
    display.drawString(68, 42, String(sensorValueOUT));
    display.display();
}

  else if (Id2 == 0 && Id1 != 0){
   display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 32, "Sensor #1 °C");
    display.setFont(ArialMT_Plain_24);
    display.drawString(64, 42, String(sensorValueIN));
    display.display(); 
}
else if (Id2 != 0 && Id1 == 0) {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 32, "Sensor #2 °C");
    display.setFont(ArialMT_Plain_24);
    display.drawString(64, 42, String(sensorValueOUT));
    display.display();
}
else if (Id1 == 0 && Id2 == 0 ) {
  display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 40, "NO SENSOR");
    display.display();
}

}

void drawImage() {
  display.clear();
  display.drawXbm(34, 14, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  display.display();
}

