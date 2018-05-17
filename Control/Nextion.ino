/*
 * Copyright (c) 2016-2018 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
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
#include "Nextion.h"

// Команды и строки дисплея (экономим место)
// В NEXTION надо посылать сроки в кодировке iso8859-5 перевод http://codepage-encoding.online-domain-tools.com/
const char *_YES_8859      ={"\xB4\xB0"};                                             // ДА
const char *_NO_8859       ={"\xBD\xB5\xC2"};                                         // НЕТ
const char *_HP_OFF_8859   ={"\xc2\xbd\x20\xd2\xeb\xda\xdb\xee\xe7\xd5\xdd\x0d\x0a"}; // ТН выключен
const char *_xB0           ={"\xB0"}; // strcat(ftoa(temp,(float)HP.sTemp[TEVAOUTG].get_Temp()/100.0,1),"\xB0"); setComponentText("t4", temp);


/* Не используемые функции
void Nextion::buttonToggle(boolean &buttonState, String objName, uint8_t picDefualtId, uint8_t picSelected){
  String tempStr = "";
  if (buttonState) {
    tempStr = objName + ".picc="+String(picDefualtId);//Select this picture
    sendCommand(tempStr.c_str());
    tempStr = "ref "+objName;//Refresh component
    sendCommand(tempStr.c_str());
    buttonState = false;
  } else {
    tempStr = objName + ".picc="+String(picSelected);//Select this picture
    sendCommand(tempStr.c_str());
    tempStr = "ref "+objName;//Refresh this component
    sendCommand(tempStr.c_str());
    buttonState = true;
  }
}//end buttonPressed

uint8_t Nextion::buttonOnOff(String find_component, String unknown_component, uint8_t pin, int btn_prev_state){  
  uint8_t btn_state = btn_prev_state;
  if((unknown_component == find_component) && (!btn_state)){
    btn_state = 1;//Led is ON
    digitalWriteDirect(pin, HIGH);
  }else if((unknown_component == find_component) && (btn_state)){
    btn_state = 0;
    digitalWriteDirect(pin, LOW);
  }else{
    //return -1;
  }//end if
  return btn_state;
}//end buttonOnOff

boolean Nextion::setComponentValue(String component, int value){
  String compValue = component +".val=" + value;//Set component value
  sendCommand(compValue.c_str());
  boolean acki = ack();
  return acki;
}//set_component_value

unsigned int Nextion::getComponentValue(String component){
  String getValue = "get "+ component +".val";//Get componetn value
    unsigned int value = 0;
  sendCommand(getValue.c_str());
  uint8_t temp[8] = {0};
  NEXTION_PORT.setTimeout(20);
  if (sizeof(temp) != NEXTION_PORT.readBytes((char *)temp, sizeof(temp))){
    return -1;
  }//end if
  if((temp[0]==(0x71))&&(temp[5]==0xFF)&&(temp[6]==0xFF)&&(temp[7]==0xFF)){
    value = (temp[4] << 24) | (temp[3] << 16) | (temp[2] << 8) | (temp[1]);//Little-endian convertion
  }//end if
  return value;
}//get_component_value 

String Nextion::listen(unsigned long timeout){
  //TODO separar todos los eventos 0x65 0x66 0x67 0x68

  char _bite;
  char _end = 0xff;//end of file x3
  String cmd;
  int countEnd = 0;
  unsigned long start = millis();

  while(nextion->available()>0){
  delay(10);
  if(nextion->available()>0){
    _bite = nextion->read();
    cmd += String(_bite, HEX);
    if(_bite == _end){
    countEnd++;
    }//end if
    if(countEnd == 3){
    break;
    }//end if
  }//end if
  }//end while
  return cmd;
}//end listen_nextion

String Nextion::getComponentText(String component, uint32_t timeout){
  String tempStr = "get " + component + ".txt";
  sendCommand(tempStr.c_str());
  tempStr = "";
  tempStr = readCommand(timeout);
  unsigned long start = millis();
  uint8_t ff = 0;//end message
  while((millis()-start < timeout)){
    if(nextion->available()){
      char b = nextion->read();
      if(String(b, HEX) == "ffff"){ff++;}
       tempStr += String(b);
     if(ff == 3){//End line
     ff = 0;
     break;
     }//end if
    }//end if
  }//end while
  if(tempStr.startsWith("p")){//0x70
  tempStr = tempStr.substring(1, tempStr.length()-3);
  }else{
  return "1a";
  }//end if
  return tempStr;
}//getComponentText

boolean Nextion::updateProgressBar(int x, int y, int maxWidth, int maxHeight, int value, int emptyPictureID, int fullPictureID, int orientation){
  int w1 = 0;
  int h1 = 0;
  int w2 = 0;
  int h2 = 0;
  int offset1 = 0;
  int offset2 = 0;

  if(orientation == 0){ // horizontal
  value = map(value, 0, 100, 0, maxWidth);
  w1 = value;
  h1 = maxHeight;
  w2 = maxWidth - value;
  h2 = maxHeight;
  offset1 = x + value;
  offset2 = y;
  
  }else{ // vertical
  value = map(value, 0, 100, 0, maxHeight);
  offset2 = y;  
  y = y + maxHeight - value;
  w1 = maxWidth;
  h1 = value;
  w2 = maxWidth;
  h2 = maxHeight - value;
  offset1 = x;
  }//end if
  String wipe = "picq " + String(x) + "," + String(y) + "," + String(w1) + "," + String(h1) + "," + String(fullPictureID);
  sendCommand(wipe.c_str());
  wipe = "picq " + String(offset1) + "," + String(offset2) + "," + String(w2) + "," + String(h2) + "," + String(emptyPictureID);
  sendCommand(wipe.c_str());
  return ack();
}//end updateProgressBar
*/

boolean Nextion::ack(void){
  /* CODE+END*/
  uint8_t bytes[4] = {0};
  NEXTION_PORT.setTimeout(20);
  if (sizeof(bytes) != NEXTION_PORT.readBytes((char *)bytes, sizeof(bytes))){
    return false;
  }//end if
  if((bytes[1]==0xFF)&&(bytes[2]==0xFF)&&(bytes[3]==0xFF)){
    switch (bytes[0]) {
	case 0x00:
	  return false; break;
	  //return cZero; break;      
	case 0x01:
	  return true; break;
	  //return cOne; break;
	  /*case 0x03:
	  return "3"; break;
	case 0x04:
	  return "4"; break;
	case 0x05:
	  return "5"; break;
	case 0x1A:
	  return "1A"; break;
	case 0x1B:
	  return "1B"; break;//*/
	default: 
	  return false;
    }//end switch
  }//end if
 return false; 
}//end

/*
boolean Nextion::setComponentText(String component, String txt){
  String componentText = component + ".txt=\"" + txt + "\"";//Set Component text
  sendCommand(componentText.c_str());
  return ack();
}//end set_component_txt
*/
boolean Nextion::setComponentText(char* component, char* txt){
  char componentText[32];
  strcpy(componentText,component);
  strcat(componentText,".txt=\"");
  strcat(componentText,txt);
  strcat(componentText,"\"");
  sendCommand(componentText);  
  return ack();
}//end set_component_txt



String Nextion::readCommand(){//returns generic
  char _bite;
  char _end = 0xff;//end of file x3
  String cmd;
  int countEnd = 0;
  boolean f=false;   // флаг начала команды

  while(NEXTION_PORT.available()>0){
  _delay(1);
	if(NEXTION_PORT.available()>0)
        	{
        	  _bite = NEXTION_PORT.read();
            if ((_bite == _end)&&(f==false)) continue;   // отбрасываем если впереди FF
            f=true; // нашли начало команды
        	  cmd += _bite;
        	  if(_bite == _end){
        		countEnd++;
        	  }//end if
        	  if(countEnd == 3){
        		break;
        	  }//end if
	}//end if
  }//end while

#ifdef NEXTION_DEBUG
  if(cmd != ""){
  journal.jprintf("Nextion get: ");  
	for(int o  = 0 ; o < cmd.length(); o++){
	  journal.jprintf("%x",cmd[o]);
	}
	journal.jprintn(cStrEnd);
	}//
#endif

  String temp = "";
  int8_t  oldPageID=PageID; // Запомнить старую страницу
  switch (cmd[0]) {
  case 'e'://0x65   Same than default -.-
	countEnd = 0;//Revision for not include last space " "
	for(uint8_t i = 0; i<cmd.length(); i++){
	  if(cmd[i] == _end){countEnd++;}//end if
	  temp += String(cmd[i], HEX);//add hexadecimal value
	  if(countEnd == 3){
		return temp;
	  }//end if
	  temp += " ";//For easy visualization   
	}//end for
	break;
  case 'f'://0x66
	//Serial.print(String(cmd[1], HEX));
//	return String(cmd[1], DEC);
  PageID=(int8_t)cmd.charAt(1);     // 
  if (PageID!=oldPageID) { fPageID=true;  Update();}   // Произошла смена страницы
  return cmd;
	break;
  case 'g'://0x67
	cmd = String(cmd[2], DEC) + "," + String(cmd[4], DEC) +","+ String(cmd[5], DEC);
	return cmd;
	break;
  case 'h'://0x68
	cmd = String(cmd[2], DEC) + "," + String(cmd[4], DEC) +","+ String(cmd[5], DEC);
	cmd = "68 " + cmd;	
	return cmd;
	break;
  case 'p'://0x70
	cmd = cmd.substring(1, cmd.length()-3);
	cmd = "70 " + cmd;
	return cmd;
	break;
  case 0x87://0x87  выход из сна
  Update();
  fPageID=true;
  return cmd;
  break;
  
  default: 
	//	cmd += String(b, HEX);
	//if(ff == 3){break;}//end if
	//cmd += " ";//
	return cmd;//
	break;
  }//end switch	
  return "";
}//end listen


int8_t Nextion::pageId(void){
  sendCommand("sendme");
  _delay(20);
  String pagId = readCommand();
 // Serial.print("ID = ");
 // Serial.println(pagId);
  //Serial.println("<-");
  if(pagId != ""){
	return pagId.toInt();
  }
  return -1;
  
}//pageId

void Nextion::sendCommand(const char* cmd){
//  while (NEXTION_PORT.available()){ 	NEXTION_PORT.read();   }//end while
  NEXTION_PORT.print(cmd);
  NEXTION_PORT.write(0xFF);
  NEXTION_PORT.write(0xFF);
  NEXTION_PORT.write(0xFF);
  #ifdef NEXTION_DEBUG 
  journal.jprintf("Nextion send: %s\n",cmd);
  #endif

}//end sendCommand

// первоначальная инициализация вход название начальной страницы
boolean Nextion::init(const char* pageStart){
  NEXTION_PORT.begin(9600);
   sendCommand("rest"); 
  _delay(100); 
// Поднятие скорости обмена
//  sendCommand("baud=115200");        
//  _delay(100);
//  NEXTION_PORT.begin(115200);

  String page = "page " + String(pageStart);//Page
  sendCommand("");
  ack();
  sendCommand(page.c_str());  // установить начальную страницу
  _delay(10);
  sendCommand("bkcmd=0");     // Ответов нет от дисплея
  _delay(10);
  sendCommand("sendxy=0");
  _delay(10);
  sendCommand("thup=1"); 
  _delay(10); 
  sendCommand("sleep=0");  
  _delay(10);
  PageID=0;
  fPageID=false;
  StartON();  
  return ack();
}//end nextion_init

void Nextion::flushSerial(){
  NEXTION_PORT.flush();
  NEXTION_PORT.flush();
}//end flush

// Добавленные функции ------------------------------------------------------
// Обновление информации на дисплее вызывается в цикле
void Nextion::Update()
{
 char temp[24]; 
  setComponentText((char*)"time", NowTimeToStr1());  // Обновить время
 // 1. Определение текущей страницы
  sendCommand("sendme");
  _delay(20);
  Listen();
  #ifdef NEXTION_DEBUG     
     journal.jprintf("Nextion page=%d\n",PageID);
  #endif
 // 2. Вывод в зависмости от страницы
if (PageID==0)  // Обновление данных 0 страницы "Главный экран"
       { 
          strcat(ftoa(temp,(float)HP.sTemp[TIN].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"t0", temp);
          strcat(ftoa(temp,(float)getTargetTemp()/100.0,1),(char*)_xB0); setComponentText((char*)"t1", temp);
          strcat(ftoa(temp,(float)HP.sTemp[TOUT].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"t2", temp);
          strcat(ftoa(temp,(float)HP.sTemp[TBOILER].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"t3", temp);
//          strcat(ftoa(temp,(float)HP.sTemp[TEVAOUTG].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"t4", temp);
          strcat(ftoa(temp,(float)HP.sTemp[TEVAING].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"t4", temp);
//          strcat(ftoa(temp,(float)HP.RET/100.0,1),(char*)_xB0); setComponentText((char*)"t5", temp);
          strcat(ftoa(temp,(float)HP.FEED/100.0,1),(char*)_xB0); setComponentText((char*)"t5", temp);
           if (((HP.get_State()==pWORK_HP)||(HP.get_State()==pSTARTING_HP)))  sendCommand((char*)"bt0.val=0");    // Кнопка включения в положение ВКЛ
           else      sendCommand((char*)"bt0.val=1");    // Кнопка включения в положение ВЫКЛ
      }
else if (PageID==2)  // Обновление данных первой страницы "СЕТЬ"
      {
        /*         
         Использовать DHCP сервер  -web1
        IP адрес контролера  -web2
        Маска подсети - web3
        Адрес шлюза  - web4
        Адрес DNS сервера - web5
        Аппаратный mac адрес - web6 */
         if (HP.get_DHCP()) setComponentText((char*)"web1",(char*)_YES_8859); else setComponentText((char*)"web1",(char*)_NO_8859);
         setComponentText((char*)"web2",HP.get_network((char*)net_IP,temp));
         setComponentText((char*)"web3",HP.get_network((char*)net_SUBNET,temp)); 
         setComponentText((char*)"web4",HP.get_network((char*)net_GATEWAY,temp)); 
         setComponentText((char*)"web5",HP.get_network((char*)net_SDNS,temp)); 
         setComponentText((char*)"web6",HP.get_network((char*)net_MAC,temp)); 
         /*         
         Использование паролей - pas1
        Имя - pas2 пароль - pas3
        Имя - pas4 пароль - pas5 */
         if (HP.get_fPass()) setComponentText((char*)"pas1",(char*)_YES_8859); else setComponentText((char*)"pas1",(char*)_NO_8859);
         setComponentText((char*)"pas2",(char*)NAME_USER);
         setComponentText((char*)"pas3",HP.get_network((char*)net_PASSUSER,temp)); 
         setComponentText((char*)"pas4",(char*)NAME_ADMIN); 
         setComponentText((char*)"pas5",HP.get_network((char*)net_PASSADMIN,temp));    
      }       
else if (PageID==3)  // Обновление данных 3 страницы "Система"
      {  
       setComponentText((char*)"syst1",(char*)VERSION);
       setComponentText((char*)"syst2",TimeIntervalToStr(HP.get_uptime()));
       setComponentText((char*)"syst3",ResetCause());
       if(HP.get_State()==pWORK_HP) setComponentText((char*)"syst4",int2str(HP.num_repeat)); else setComponentText((char*)"syst4",(char*)_HP_OFF_8859);
       setComponentText((char*)"syst5",ftoa(temp,(float)HP.get_motoHourH2()/60.0,1));
       setComponentText((char*)"syst6",ftoa(temp,(float)HP.get_motoHourC2()/60.0,1));
       setComponentText((char*)"syst7",int2str(100-HP.CPU_IDLE));
       setComponentText((char*)"syst8",int2str(HP.get_errcode()));
           
      }
else if (PageID==4)  // Обновление данных 4 страницы "СХЕМА ТН"
      {
      /*         
      темп на улице - tout
      темп в доме - tin
      компрессор - tcomp
      перегрев - tper
      из геоконтура - tevaoutg
      в геоконтур - tevaing
      из системы отопления - tconoutg
      в систему отопления - tconing
      */
      strcat(ftoa(temp,(float)HP.sTemp[TOUT].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tout", temp);
      strcat(ftoa(temp,(float)HP.sTemp[TIN].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tin", temp);
      strcat(ftoa(temp,(float)HP.sTemp[TCOMP].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tcomp", temp);
  //    strcat(ftoa(temp,(float)HP.sTemp[TEVAOUTG].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tevaoutg", temp);
  //    strcat(ftoa(temp,(float)HP.sTemp[TEVAING].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tevaing", temp);    
      strcat(ftoa(temp,(float)HP.sTemp[TEVAOUTG].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tevaing", temp);
      strcat(ftoa(temp,(float)HP.sTemp[TEVAING].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tevaoutg", temp);
  //    strcat(ftoa(temp,(float)HP.sTemp[TCONOUTG].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tconoutg", temp);
  //    strcat(ftoa(temp,(float)HP.sTemp[TCONING].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tconing", temp);   
       strcat(ftoa(temp,(float)HP.sTemp[TCONOUTG].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tconing", temp);
       strcat(ftoa(temp,(float)HP.sTemp[TCONING].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tconoutg", temp);     

      #ifdef EEV_DEF 
      strcat(ftoa(temp,(float)HP.dEEV.get_Overheat()/100.0,1),(char*)_xB0); setComponentText((char*)"tper", temp);
      #else
      strcat(ftoa(temp,0.0/100.0,1),(char*)_xB0); setComponentText((char*)"tper", temp);
      #endif
      }  
else if (PageID==5)  // Обновление данных 5 страницы "Отопление/Охлаждение"
      {
      /*         
      установленная Т - tust
      Т плюс - plus
      Т минус - minus
      Отопление вкл - hotin
      Отопление выкл - hotout
      Охлаждение вкл - coolin
      Охлаждение выкл - coolout
      СО отключено вкл - coin
      СО отключено выкл - coout
      Алгоритм Т в доме - alg1
      Алгоритм Т обратки - alg2
      */
      setComponentText((char*)"tust", ftoa(temp,(float)getTargetTemp()/100.0,1));
      // Состояние системы отопления
       switch ((MODE_HP)HP.get_mode())  
              {
              case  pOFF:   sendCommand((char*)"vis hotin,0");sendCommand((char*)"vis hotout,1");
                            sendCommand((char*)"vis coolin,0");sendCommand((char*)"vis coolout,1");
                            sendCommand((char*)"vis coin,1");sendCommand((char*)"vis coout,0");
                            sendCommand((char*)"vis alg1,0"); sendCommand((char*)"vis alg2,0");  // убрать показ алгоритма
                            break;
              case  pHEAT:  sendCommand((char*)"vis hotin,1");sendCommand((char*)"vis hotout,0");
                            sendCommand((char*)"vis coolin,0");sendCommand((char*)"vis coolout,1");
                            sendCommand((char*)"vis coin,0");sendCommand((char*)"vis coout,1");  
                            switch((RULE_HP) HP.get_ruleHeat())
                                {
                                  case pHYSTERESIS: 
                                  case pPID:         if (HP.get_TargetHeat())
                                                        {sendCommand((char*)"vis alg1,0"); sendCommand((char*)"vis alg2,1");}   // цель дом
                                                        else {sendCommand((char*)"vis alg1,1"); sendCommand((char*)"vis alg2,0");} // цель обратка
                                                        break;
                                  case pHYBRID:       sendCommand((char*)"vis alg1,0"); sendCommand((char*)"vis alg2,1");  // цель дом
                                                      break;        
                                default:              break;  
                                }// switch((RULE_HP) HP.get_ruleHeat())
                            break;
              case  pCOOL:  sendCommand((char*)"vis hotin,0");sendCommand((char*)"vis hotout,1");
                            sendCommand((char*)"vis coolin,1");sendCommand((char*)"vis coolout,0");
                            sendCommand((char*)"vis coin,0");sendCommand((char*)"vis coout,1");
                            switch((RULE_HP) HP.get_ruleCool())
                                {
                                  case pHYSTERESIS: 
                                  case pPID:         if (HP.get_TargetCool())
                                                        {sendCommand((char*)"vis alg1,0"); sendCommand((char*)"vis alg2,1");}
                                                        else {sendCommand((char*)"vis alg1,1"); sendCommand((char*)"vis alg2,0");}
                                                        break;
                                  case pHYBRID:      sendCommand((char*)"vis alg1,0"); sendCommand((char*)"vis alg2,1");  
                                                     break; 
                                 default:            break;                    
                                }  // switch((RULE_HP) HP.get_ruleCool())
                            break;
               default:  break;             
              } // switch ((MODE_HP)HP.get_mode())  
              
      
      }  
else if (PageID==6)  // Обновление данных 6 страницы "ГВС"
      {
      strcat(ftoa(temp,(float)HP.sTemp[TBOILER].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tboiler", temp);  
      strcat(ftoa(temp,(float)HP.sTemp[TCONOUTG].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tconoutg", temp);  
      strcat(ftoa(temp,(float)HP.sTemp[TCONING].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tconing", temp);  
      strcat(ftoa(temp,(float)HP.get_boilerTempTarget()/100.0,1),""); setComponentText((char*)"tustgvs", temp);  
       if (HP.get_BoilerON())  sendCommand((char*)"gvson.val=1");    // Кнопка включения ГВС в положение ВКЛ
      else      sendCommand((char*)"bt0.val=0");                     // Кнопка включения ГВС в положение ВЫКЛ
     
      }        
// обновление статуса (если сменилась страница или изменилось состояниие ТН)
// Определение текущего состояния ТН
   uint8_t tempS;
   // Вычисление статуса
   if (HP.get_errcode()==OK) tempS=1; else tempS=0;
   if (HP.get_State()==pWORK_HP) tempS=tempS+2;
   if (HP.get_BoilerON()) tempS=tempS+4;   
   switch ((int)HP.get_mode())  
              {
              case  pOFF:  tempS=tempS+8;  break;
              case  pHEAT: tempS=tempS+9;  break;
              case  pCOOL: tempS=tempS+10; break;
              } 
             
 //   Serial.print("-"); Serial.println(temp,BIN);
   if ((tempS==Status)&&(fPageID==false)) return;     // Обновлять нечего уходим
   else
   { Status=tempS;  
     fPageID=false;
     StatusLine();
     
   }     

}

// Разбор очереди команд, надо вызывать регулярно
void Nextion::Listen()
{
  char temp[16];  
  String  message = readCommand();
     
   // if (message!= "") Serial.println(message.c_str());
   // парсер комманд
        if (message == (char*)"65 0 2 0 ff ff ff")   { if((HP.get_State()!=pSTARTING_HP)||(HP.get_State()!=pSTOPING_HP)) { if(HP.get_State()==pOFF_HP) HP.sendCommand(pSTART); else HP.sendCommand(pSTOP);}} // событие нажатие кнопки вкл/выкл ТН
   else if (message == (char*)"65 5 18 0 ff ff ff")  {setComponentText((char*)"tust", ftoa(temp,(float)HP.setTargetTemp(20)/100.0,1));}                // Увеличение целевой температуры СО шаг изменения сотые градуса
   else if (message == (char*)"65 5 19 0 ff ff ff")  {setComponentText((char*)"tust", ftoa(temp,(float)HP.setTargetTemp(-20)/100.0,1));}               // Уменьшение целевой температуры СО шаг изменения сотые градуса
   else if (message == (char*)"65 6 15 0 ff ff ff")  {if (HP.get_BoilerON()) HP.set_BoilerOFF(); else HP.set_BoilerON();}                       // событие нажатие кнопки вкл/выкл ГВС
   else if (message == (char*)"65 6 e 0 ff ff ff")   {setComponentText((char*)"tustgvs", ftoa(temp,(float)HP.setTempTargetBoiler(100)/100.0,1));}      // Увеличение целевой температуры ГВС шаг изменения сотые градуса
   else if (message == (char*)"65 6 f 0 ff ff ff")   {setComponentText((char*)"tustgvs", ftoa(temp,(float)HP.setTempTargetBoiler(-100)/100.0,1));}     // Уменьшение целевой температуры ГВС шаг изменения сотые градуса
   
    // Переключение режимов отопления ТОЛЬКО если насос выключен
   else if ((message == (char*)"65 5 1b 0 ff ff ff")&&(HP.get_State()==pOFF_HP))  
       { HP.set_nextMode();  // выбрать следующий режим
        switch ((MODE_HP)HP.get_mode())  
              {
              case  pOFF:   sendCommand((char*)"vis hotin,0");sendCommand((char*)"vis hotout,1");
                            sendCommand((char*)"vis coolin,0");sendCommand((char*)"vis coolout,1");
                            sendCommand((char*)"vis coin,1");sendCommand((char*)"vis coout,0");
                            break;
              case  pHEAT:  sendCommand((char*)"vis hotin,1");sendCommand((char*)"vis hotout,0");
                            sendCommand((char*)"vis coolin,0");sendCommand((char*)"vis coolout,1");
                            sendCommand((char*)"vis coin,0");sendCommand((char*)"vis coout,1");  
                            break;
              case  pCOOL:  sendCommand((char*)"vis hotin,0");sendCommand((char*)"vis hotout,1");
                            sendCommand((char*)"vis coolin,1");sendCommand((char*)"vis coolout,0");
                            sendCommand((char*)"vis coin,0");sendCommand((char*)"vis coout,1"); 
                            break;
              default:  break;               
              } 
       }
     

}
// Подготовка экрана к первому показу
void Nextion::StartON()      
{
sendCommand((char*)"ref_stop");      // Остановить обновление
sendCommand((char*)"vis tninc,0");
sendCommand((char*)"vis tnoff,0");
sendCommand((char*)"vis options,0");
sendCommand((char*)"vis fault,0");
sendCommand((char*)"vis heat,0");
sendCommand((char*)"vis cool,0");
sendCommand((char*)"vis gvs,0");
sendCommand((char*)"vis onlygvs,0");
sendCommand((char*)"bt0.val=1");    // Кнопка включения в положение выключено
sendCommand((char*)"ref_star");     // Восстановить обновление
StatusLine();
}

// Показ строки статуса в зависимости от состояния ТН
void Nextion::StatusLine()      
{
 char temp[16];  

   sendCommand((char*)"ref_stop");      // Остановить обновление
//   setComponentText("time", NowTimeToStr1());
    // Ошибки
    if (HP.get_errcode()==OK)  { sendCommand((char*)"vis options,1"); sendCommand((char*)"vis fault,0");} 
    else { sendCommand((char*)"vis fault,1"); sendCommand((char*)"vis options,0"); }

   if (PageID==0)   
   {
      if (((HP.get_State()==pWORK_HP)||(HP.get_State()==pSTARTING_HP)))  sendCommand((char*)"bt0.val=0");    // Кнопка включения в положение ВКЛ
      else      sendCommand((char*)"bt0.val=1");    // Кнопка включения в положение ВЫКЛ
   }
   else if (PageID==6)   
   {
      if (HP.get_BoilerON())  sendCommand((char*)"gvson.val=1");    // Кнопка включения ГВС в положение ВКЛ
      else      sendCommand((char*)"bt0.val=0");                    // Кнопка включения ГВС в положение ВЫКЛ
   }

      
    if (HP.get_State()==pOFF_HP) // Насос выключен
        {
         sendCommand((char*)"vis tninc,0");sendCommand((char*)"vis tnoff,1");
         sendCommand((char*)"vis heat,0");
         sendCommand((char*)"vis cool,0");
         sendCommand((char*)"vis gvs,0");
         sendCommand((char*)"vis onlygvs,0"); 
        
        }
    else  // насос включен
       {
          sendCommand((char*)"vis tninc,1");sendCommand((char*)"vis tnoff,0");
          switch ((MODE_HP)HP.get_mode())  
              {
              case  pOFF:  
                    if(HP.get_BoilerON()) {  sendCommand((char*)"vis gvs,0"); sendCommand((char*)"vis onlygvs,1"); } 
                    else  {  sendCommand((char*)"vis gvs,0"); sendCommand((char*)"vis onlygvs,0"); } 
                    break;
              case  pHEAT:
                    sendCommand((char*)"vis heat,1");  sendCommand((char*)"vis cool,0");     
                    if(HP.get_BoilerON()) {  sendCommand((char*)"vis gvs,1"); sendCommand((char*)"vis onlygvs,0"); } 
                    else  {  sendCommand((char*)"vis gvs,0"); sendCommand((char*)"vis onlygvs,0"); } 
                    break;
              case  pCOOL:
                    sendCommand((char*)"vis heat,0");  sendCommand((char*)"vis cool,1");     
                    if(HP.get_BoilerON()) {  sendCommand((char*)"vis gvs,1"); sendCommand((char*)"vis onlygvs,0"); } 
                    else  {  sendCommand((char*)"vis gvs,0"); sendCommand((char*)"vis onlygvs,0"); }            
                    break;
               default:  break;       
            } //switch ((int)HP.get_mode())
       } 
    sendCommand((char*)"ref_star");    // Восстановить обновление

// Засыпание дисплея
//  SLEEP режим
     if(HP.get_sleep()>0)   // установлено засыпание дисплея
              {
              strcpy(temp,(char*)"thsp=");
              strcat(temp,int2str(HP.get_sleep()*60)); // секунды
              sendCommand(temp);
              sendCommand((char*)"thup=1");     // sleep режим активировать
              }  
           else sendCommand((char*)"thsp=0")  ;   // sleep режим выключен
}
 // Получить целевую температуру отопления
int16_t  Nextion::getTargetTemp()
{
    switch ((int)HP.get_mode())   // проверка отопления
    {
      case  pOFF:  return -1;      break;
      case  pHEAT: return HP.get_targetTempHeat();  break; 
      case  pCOOL: return HP.get_targetTempCool();  break; 
      default:  break; 
    }  
return -1;            
}

