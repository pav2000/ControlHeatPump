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
// Класс для уведомлений
#include "Message.h"
extern void get_mailState(EthernetClient client,char *tempBuf);

 // Инициализация 
 // Уведомления по умолчанию настройка на gmail без шифрования
void Message::initMessage()
{  
  IPAddress zeroIP(0,0,0,0);   
  lastmessageSetting=pMESSAGE_NONE;                                                // последнее отправленное уведомление  
  dnsUpadateSMS=false;                                                             // Флаг необходимости обновления через dns IP адреса для смс
  dnsUpadateSMTP=false;                                                            // Флаг необходимости обновления через dns IP адреса для smtp
  strcpy(retTest,"");                                                              // обнулить ответ от посылки тестового письма
  strcpy(retMail,"");                                                              // обнулить ошибку от посылки тестового письма
  strcpy(retSMS,"");                                                               // обнулить ошибку от посылки тестового SMS
  sendTime=0;                                                                      // отправок уведомлений не было

  SETBIT0(messageSetting.flags,fMail );                                            // флаг уведомления скидывать на почту 
  SETBIT1(messageSetting.flags,fMailAUTH);                                         // флаг необходимости авторизации на почтовом сервере
  SETBIT1(messageSetting.flags,fMailInfo);                                         // флаг необходимости добавления в письмо информации о состянии ТН
  SETBIT0(messageSetting.flags,fSMS);                                              // флаг уведомления скидывать на СМС (пока не реализовано)
  SETBIT1(messageSetting.flags,fMessageReset);                                     // флаг уведомления Сброс
  SETBIT1(messageSetting.flags,fMessageError);                                     // флаг уведомления Ошибка
  SETBIT1(messageSetting.flags,fMessageLife);                                      // флаг уведомления Сигнал жизни
  SETBIT1(messageSetting.flags,fMessageTemp);                                      // флаг уведомления Достижение граничной температуры
  SETBIT1(messageSetting.flags,fMessageSD);                                        // флаг уведомления "Проблемы с sd картой"
  SETBIT1(messageSetting.flags,fMessageWarning);                                   // флаг уведомления "Прочие уведомления"

//  rtcSAM3X8.set_alarmtime(11, 17, 0);                                              // завести будильник для отправки сигнала жизни
//  rtcSAM3X8.attachalarm(life_signal);

  strcpy(messageSetting.smtp_server,"smtp.qip.ru");                                // Адрес сервера
  messageSetting.smtp_serverIP=zeroIP;                                             // сделать адрес 0.0.0.0 
  messageSetting.smtp_port=25;                                                     // Адрес порта сервера 
  strcpy(messageSetting.smtp_login,"controlhp@qip.ru");                            // логин сервера если включена авторизация
  strcpy(messageSetting.smtp_password,"2control");                                 // пароль сервера если включена авторизация
  strcpy(messageSetting.smtp_MailTo,"controlhp@qip.ru");                           // адрес отправителя
  strcpy(messageSetting.smtp_RCPTTo,"controlhp@qip.ru");                           // адрес получателя  
 
  messageSetting.sms_service=pSMS_RU;                                              // Cервис отправки смс по умолчанию sms.ru
  messageSetting.sms_serviceIP=zeroIP;                                             // сделать адрес 0.0.0.0
  strcpy(messageSetting.sms_phone,"1234567890");                                   // телефон куда отправляется смс
  strcpy(messageSetting.sms_p1,"api_id");                                          // первый параметр для отправки смс
  strcpy(messageSetting.sms_p2,"none");                                            // второй параметр для отправки смс

  messageSetting.mTIN=1000;                                                        // Критическая температура в доме (если меньше то генерится уведомление)
  messageSetting.mTBOILER=500;                                                     // Критическая температура бойлера (если меньше то генерится уведомление)
  messageSetting.mTCOMP=7000;                                                      // Критическая температура компрессора (если больше то генерится уведомление)

  // установка содержания уведомлений по умолчаню
 messageData.ms= pMESSAGE_NONE;     // Тип уведомления
 strcpy(messageData.data,"");       // Полезная информация 
 messageData.p1=0;                  // первый параметр
 messageData.number=0;              // номер уведомления
 
 }
 
// Обновление IP адресов серверов через dns 
// возвращает true обновление не было false - прошло обновление или ошибка
boolean Message::dnsUpdate()
{
  boolean ret=true;
  char buf[16];
  if  (dnsUpadateSMTP) //надо обновлятся
  {
     dnsUpadateSMTP=false;
     if(SemaphoreTake(xWebThreadSemaphore,(W5200_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE)  {return false;}  // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
     ret=check_address(messageSetting.smtp_server,messageSetting.smtp_serverIP);      // Получить адрес IP через DNS
     SemaphoreGive(xWebThreadSemaphore);
     _delay(20);
     ret=false;
  }
  if  (dnsUpadateSMS) //надо обновлятся
  {
     strcpy(buf,ADR_SMS_RU);
     switch (messageSetting.sms_service)
     {
      case pSMS_RU: strcpy(buf,ADR_SMS_RU);  break;
      case pSMSC_RU:strcpy(buf,ADR_SMSC_RU); break;
      default:      strcpy(buf,ADR_SMS_RU);  break; // Этого не должно быть, но если будет то установить по умолчанию
     } 
     dnsUpadateSMS=false;
     if(SemaphoreTake(xWebThreadSemaphore,(W5200_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE)  {return false;}  // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
     ret=check_address((char*)buf,messageSetting.sms_serviceIP);
     SemaphoreGive(xWebThreadSemaphore);
     _delay(20);
     ret=false;
   }
  return ret; 
}
// Обновление IP адресов серверов через dns при СТАРТЕ!!!  вачдог сбрасывается т.к. может сеть не рабоать  
boolean Message::dnsUpdateStart()
{    boolean ret=true;
     char buf[16];
     IPAddress zeroIP(0,0,0,0);  
     
     strcpy(buf,ADR_SMS_RU);
     switch (messageSetting.sms_service)
     {
      case pSMS_RU: strcpy(buf,ADR_SMS_RU);  break;
      case pSMSC_RU:strcpy(buf,ADR_SMSC_RU); break;
      default:      strcpy(buf,ADR_SMS_RU);  break; // Этого не должно быть, но если будет то установить по умолчанию
     } 
     dnsUpadateSMS=false;
     WDT_Restart(WDT);                                                               // Сбросить вачдог  
     if(messageSetting.sms_serviceIP==zeroIP) ret=check_address((char*)buf,messageSetting.sms_serviceIP);                      // если адрес нулевой Получить адрес IP через DNS
     dnsUpadateSMTP=false;
     WDT_Restart(WDT);                                                              // Сбросить вачдог  
     if(messageSetting.smtp_serverIP==zeroIP) ret=check_address(messageSetting.smtp_server,messageSetting.smtp_serverIP);      //  если адрес нулевой Получить адрес IP через DNS
     return ret;                                               
}
 
// Установить параметр Уведомления из строки
boolean Message::set_messageSetting(MESSAGE_HP p, char *c)
{
  float x;
//  return true;

 switch (p)
   {
    case pMAIL:        if (strcmp(c,cZero)==0)      { SETBIT0(messageSetting.flags,fMail); return true;}
                       else if (strcmp(c,cOne)==0) { SETBIT1(messageSetting.flags,fMail);  return true;}
                       else return false;  
                       break; 
    case pMAIL_AUTH:   if (strcmp(c,cZero)==0)      { SETBIT0(messageSetting.flags,fMailAUTH); return true;}
                       else if (strcmp(c,cOne)==0) { SETBIT1(messageSetting.flags,fMailAUTH);  return true;}
                       else return false;  
                       break; 
    case pMAIL_INFO:   if (strcmp(c,cZero)==0)      { SETBIT0(messageSetting.flags,fMailInfo); return true;}
                       else if (strcmp(c,cOne)==0) { SETBIT1(messageSetting.flags,fMailInfo);  return true;}
                       else return false;  
                       break;
    case pSMS:         if (strcmp(c,cZero)==0)      { SETBIT0(messageSetting.flags,fSMS); return true;}
                       else if (strcmp(c,cOne)==0) { SETBIT1(messageSetting.flags,fSMS);  return true;}
                       else return false;  
                       break;
    case pMESS_RESET:  if (strcmp(c,cZero)==0)      { SETBIT0(messageSetting.flags,fMessageReset); return true;}
                       else if (strcmp(c,cOne)==0) { SETBIT1(messageSetting.flags,fMessageReset);  return true;}
                       else return false;  
                       break;
    case pMESS_ERROR:  if (strcmp(c,cZero)==0)      { SETBIT0(messageSetting.flags,fMessageError); return true;}
                       else if (strcmp(c,cOne)==0) { SETBIT1(messageSetting.flags,fMessageError);  return true;}
                       else return false;  
                       break;
    case pMESS_LIFE:   if (strcmp(c,cZero)==0)      { SETBIT0(messageSetting.flags,fMessageLife); return true;}
                       else if (strcmp(c,cOne)==0) { SETBIT1(messageSetting.flags,fMessageLife); return true;}
                       else return false;  
                       break;
    case pMESS_TEMP:   if (strcmp(c,cZero)==0)      { SETBIT0(messageSetting.flags,fMessageTemp); return true;}
                       else if (strcmp(c,cOne)==0) { SETBIT1(messageSetting.flags,fMessageTemp);  return true;}
                       else return false;  
                       break;
    case pMESS_SD:     if (strcmp(c,cZero)==0)      { SETBIT0(messageSetting.flags,fMessageSD); return true;}
                       else if (strcmp(c,cOne)==0) { SETBIT1(messageSetting.flags,fMessageSD);  return true;}
                       else return false;  
                       break; 
    case pMESS_WARNING:if (strcmp(c,cZero)==0)      { SETBIT0(messageSetting.flags,fMessageWarning); return true;}
                       else if (strcmp(c,cOne)==0) { SETBIT1(messageSetting.flags,fMessageWarning);  return true;}
                       else return false;  
                       break;  
    case pSMTP_SERVER: if(strlen(c)==0) return false;                                                // пустая строка
                       if(strlen(c)>sizeof(messageSetting.smtp_server)-1) return false;              // слишком длиная строка
                       else // ок сохраняем 
                        {  
                        strcpy(messageSetting.smtp_server,c); 
                        dnsUpadateSMTP=true;
                 //       check_address(messageSetting.smtp_server,&messageSetting.smtp_serverIP);      // Получить адрес IP через DNS
                        return true;  
                        }                        
                       break; 
    case pSMTP_IP:     return true; break;  //  Только на чтение. описание первого параметра для отправки смс                                  
    case pSMTP_PORT:   x=my_atof(c);  
                       if (x==ATOF_ERROR) return   false;
                       else if((x<=1)||(x>=65535-1)) return   false;   
                       else messageSetting.smtp_port=(int)x; return true;
                       break;  
    case pSMTP_LOGIN:  if(strlen(c)==0) return false;                                              
                       if(strlen(c)>sizeof(messageSetting.smtp_login)-1) return false;                     
                       else { strcpy(messageSetting.smtp_login,c); return true;  }                         
                       break;                
    case pSMTP_PASS:   if(strlen(c)==0) return false;                                               
                       if(strlen(c)>sizeof(messageSetting.smtp_password)-1) return false;                   
                       else { strcpy(messageSetting.smtp_password,c); return true;  }                       
                       break; 
    case pSMTP_MAILTO: if(strlen(c)==0) return false;                                             
                       if(strlen(c)>sizeof(messageSetting.smtp_MailTo)-1) return false;                   
                       else { strcpy(messageSetting.smtp_MailTo,c); return true;  }                       
                       break; 
    case pSMTP_RCPTTO: if(strlen(c)==0) return false;                                              
                       if(strlen(c)>sizeof(messageSetting.smtp_RCPTTo)-1) return false;                   
                       else { strcpy(messageSetting.smtp_RCPTTo,c); return true;  }                       
                       break; 
    case pSMS_SERVICE: x=my_atof(c);    // При смене сервиса определяем новый IP поле sms_serviceIP
                       if (x==ATOF_ERROR) return   false;
                       messageSetting.sms_service=(SMS_SERVICE)x;
                       dnsUpadateSMS=true;
                       return true;
                       break;
    case pSMS_IP:      return true;    break;    // Только на чтение
    case pSMS_PHONE:   if(strlen(c)==0) return false;                                            
                       if(strlen(c)>sizeof(messageSetting.sms_phone)-1) return false;                   
                       else { strcpy(messageSetting.sms_phone,c); return true;  }                       
                       break;   
    case pSMS_P1:      if(strlen(c)==0) return false;          // первый параметр для отправки смс                                     
                       if(strlen(c)>sizeof(messageSetting.sms_p1)-1) return false;                   
                       else { strcpy(messageSetting.sms_p1,c); return true;  }                       
                       break;
    case pSMS_P2:      if(strlen(c)==0) return false;          // второй параметр для отправки смс                                     
                       if(strlen(c)>sizeof(messageSetting.sms_p2)-1) return false;                   
                       else { strcpy(messageSetting.sms_p2,c); return true;  }                       
                       break;
    case pSMS_NAMEP1:  return true; break;  //  Только на чтение. описание первого параметра для отправки смс
    case pSMS_NAMEP2:  return true; break;  //  Только на чтение. описание второго параметра для отправки смс
    case pMESS_TIN:    x=my_atof(c);  
                       if (x==ATOF_ERROR) return   false;
                       else if((x<0)||(x>30)) return   false;   
                       else messageSetting.mTIN=(int)(x*100.0); return true;
                       break;  
    case pMESS_TBOILER:x=my_atof(c);  
                       if (x==ATOF_ERROR) return   false;
                       else if((x<0)||(x>70)) return   false;   
                       else messageSetting.mTBOILER=(int)(x*100.0); return true;
                       break; 
    case pMESS_TCOMP:  x=my_atof(c);  
                       if (x==ATOF_ERROR) return   false;
                       else if((x<-5)||(x>80)) return   false;   
                       else messageSetting.mTCOMP=(int)(x*100.0); return true;
                       break;
    case pMAIL_RET:    return true;                                break;   
    case pSMS_RET:     return true;                                break;                     
   default:            return false;                               break;   
   }
   
  return false;                        
}
// Получить параметр Уведомления в строке 
char*   Message::get_messageSetting(MESSAGE_HP p)
{
  char static temp[16];   

// return "test";
   switch (p)
   {
    case pMAIL:        if (GETBIT(messageSetting.flags,fMail))           return  (char*)cOne; else return  (char*)cZero; break;
    case pMAIL_AUTH:   if (GETBIT(messageSetting.flags,fMailAUTH))       return  (char*)cOne; else return  (char*)cZero; break;
    case pMAIL_INFO:   if (GETBIT(messageSetting.flags,fMailInfo))       return  (char*)cOne; else return  (char*)cZero; break;
    case pSMS:         if (GETBIT(messageSetting.flags,fSMS))            return  (char*)cOne; else return  (char*)cZero; break;
    case pMESS_RESET:  if (GETBIT(messageSetting.flags,fMessageReset))   return  (char*)cOne; else return  (char*)cZero; break;
    case pMESS_ERROR:  if (GETBIT(messageSetting.flags,fMessageError))   return  (char*)cOne; else return  (char*)cZero; break;
    case pMESS_LIFE:   if (GETBIT(messageSetting.flags,fMessageLife))    return  (char*)cOne; else return  (char*)cZero; break;
    case pMESS_TEMP:   if (GETBIT(messageSetting.flags,fMessageTemp))    return  (char*)cOne; else return  (char*)cZero; break;
    case pMESS_SD:     if (GETBIT(messageSetting.flags,fMessageSD))      return  (char*)cOne; else return  (char*)cZero; break;
    case pMESS_WARNING:if (GETBIT(messageSetting.flags,fMessageWarning)) return  (char*)cOne; else return  (char*)cZero; break;
    case pSMTP_SERVER: return messageSetting.smtp_server;                       break;  
    case pSMTP_IP:     return IPAddress2String(messageSetting.smtp_serverIP);   break;   
    case pSMTP_PORT:   return int2str(messageSetting.smtp_port);                break; 
    case pSMTP_LOGIN:  return messageSetting.smtp_login;                        break;
    case pSMTP_PASS:   return messageSetting.smtp_password;                     break; 
    case pSMTP_MAILTO: return messageSetting.smtp_MailTo;                       break;
    case pSMTP_RCPTTO: return messageSetting.smtp_RCPTTo;                       break;
    case pSMS_SERVICE: switch (messageSetting.sms_service)
                       {
                        case pSMS_RU:  return (char*)"sms.ru:1;smsc.ru:0;";     break;
                        case pSMSC_RU: return (char*)"sms.ru:0;smsc.ru:1;";     break;
                        default: return (char*)"sms.ru:1;smsc.ru:0;";            break; // Этого не должно быть, но если будет то установить по умолчанию
                       }                                                        break; 
    case pSMS_IP:      return IPAddress2String(messageSetting.sms_serviceIP);   break;    
    case pSMS_PHONE:   return messageSetting.sms_phone;                         break;  
    case pSMS_P1:      return messageSetting.sms_p1;                            break;  // первый параметр для отправки смс
    case pSMS_P2:      return messageSetting.sms_p2;                            break;  // второй параметр для отправки смс
    case pSMS_NAMEP1:  switch (messageSetting.sms_service)  // описание первого параметра для отправки смс
                       {
                        case pSMS_RU:  return (char*)"API ID";  break;
                        case pSMSC_RU: return (char*)"Login";   break;
                        default: return (char*)"API ID";        break; // Этого не должно быть, но если будет то установить по умолчанию
                       }                                                        break;
    case pSMS_NAMEP2:  switch (messageSetting.sms_service)   // описание второго параметра для отправки смс
                       {
                        case pSMS_RU:  return (char*)"none";      break;
                        case pSMSC_RU: return (char*)"Password";  break;
                        default: return (char*)"none";            break; // Этого не должно быть, но если будет то установить по умолчанию
                       }                                                        break;
    case pMESS_TIN:    return ftoa(temp,(float)messageSetting.mTIN/100.0,1);    break;
    case pMESS_TBOILER:return ftoa(temp,(float)messageSetting.mTBOILER/100.0,1);break;
    case pMESS_TCOMP:  return ftoa(temp,(float)messageSetting.mTCOMP/100.0,1);  break;
    case pMAIL_RET:    if (waitSend) return (char*)"wait response...";                 // В зависимости готов ответ или нет
                       else return retTest;                               
                       break;   
    case pSMS_RET:     if (waitSend) return (char*)"wait response...";                 // В зависимости готов ответ или нет                 
                       else return retTest;                               
                       break;   
    default:           return false;                                     break;   
   }
 return (char*)cInvalid;                                    
}
// Записать настройки в eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t Message::save(int32_t adr)    
{

// Serial.print(messageSetting.GETBIT(fMessageReset));Serial.println("-1"); 
 if (writeEEPROM_I2C(adr, (byte*)&messageSetting, sizeof(messageSetting))) { set_Error(ERR_SAVE_EEPROM,(char*)nameHeatPump); return ERR_SAVE_EEPROM;}  adr=adr+sizeof(messageSetting);           // записать параметры Уведомлений
 return adr;   
}
// Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки                                         
int32_t Message::load(int32_t adr)
{
  if (readEEPROM_I2C(adr, (byte*)&messageSetting, sizeof(messageSetting))) { set_Error(ERR_LOAD_EEPROM,(char*)nameHeatPump); return ERR_LOAD_EEPROM;}  adr=adr+sizeof(messageSetting);           // прочитать параметры Уведомлений     
  return adr;                                       
}
// Считать настройки из буфера на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки 
int32_t Message::loadFromBuf(int32_t adr, byte *buf)
{
 memcpy((byte*)&messageSetting,buf+adr,sizeof(messageSetting)); adr=adr+sizeof(messageSetting); 
 return adr;                         
}
// Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
uint16_t Message::get_crc16(uint16_t crc)
{
  uint16_t i;
  for(i=0;i<sizeof(messageSetting);i++) crc=_crc16(crc,*((byte*)&messageSetting+i));   // CRC16 настройки уведомлений
 // Serial.print("Message::get_crc16 0x");Serial.println(crc,HEX);
  return crc;                                    
}

// Послать команду SMTP серверу  и разобрать ответ
// wait - ждать ответ или нет
boolean  Message::SendCommandSMTP(char *c, boolean wait)   
{
    uint8_t count=0;
    byte respCode,thisByte;
    int num;

     if(!clientMessage.connected())   // если клиент не соединен то это ошибка выходим
     {
       journal.jprintf("Server no connected, abort send mail???\n");
       return false; 
     }
   
   if((strlen(c)<LEN_TEMPBUF)&&(tempBuf!=c)) strcpy(tempBuf,c);  // если надо то копируем
   
   // Послать команду, Если она не пустая (в противном случае только читаем)
  if (strlen(tempBuf)>0)  // Посылаем не пустые комнады
  {
  //    clientMessage.println(tempBuf); // почемуто не отправляет????
         strcat(tempBuf,cStrEnd);  clientMessage.write(tempBuf,strlen(tempBuf));
   //    Serial.print(">>");Serial.print(tempBuf);Serial.print("<< len=");Serial.println(strlen(tempBuf));
      journal.jprintf("%s",tempBuf);
  }  
 
  // Необходимость ожидания и получения ответа
  if (!wait) {strcpy(tempBuf,"");  return true; }  // ответа не ожидаем
  
    
  while(!clientMessage.available()) // ожидание ответа 5 сек
  {
    SemaphoreGive(xWebThreadSemaphore);                                                   // отдать семафор для обработки других задач
    _delay(200);
    if(SemaphoreTake(xWebThreadSemaphore,(W5200_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE) {return false;}  // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
    count++;
    if (count>25)
     {
       strncpy(retMail,"Server not answer . . .",LEN_RETMAIL);
       journal.jprintf("%s\n",retMail);
  //    clientMessage.stop();
  //    SemaphoreGive(xWebThreadSemaphore);
       return false;  
     }
     
  }
  // разбор ответа
  respCode = clientMessage.peek();   // первый байт - это старшая цифра ответа, по ней можно определить ошибки
  while(clientMessage.available())   // чтение ответа
  { 
     num=clientMessage.read((byte*)tempBuf,LEN_TEMPBUF-1);  tempBuf[num]=0;  // Обрезать строку
  //   Serial.print("num=");Serial.print(num);Serial.print(" >>"); Serial.print(tempBuf);
     journal.jprintf("%s",tempBuf);
  }

  // Проверка на ошибки
  if(respCode >= '4') // ошибка при общении с сервером, закрываем сессию
  {
   strncpy(retMail,tempBuf,LEN_RETMAIL);  // запомнить ответ сервера при ошибке
//    Serial.print("retMail "); Serial.println(retMail);
//    Serial.print("answer "); Serial.println(answer);
     clientMessage.println("QUIT"); // Послать команду на закрытие сессии
     journal.jprintf("QUIT\n");
       while(!clientMessage.available()) // ожидание ответа 1 сек
        {
        _delay(100);
        count++;
        if (count>10) break;
       }
   while(clientMessage.available())
     { 
      thisByte = clientMessage.read();   
      journal.jprintf("%c",thisByte);
     }
    journal.jprintf("disconnected\n");
    clientMessage.stop();
    return false; 
  }
  return true;    
}

// Установить уведомление (сформировать для отправки но НЕ ОТПРАВЛЯТЬ)
// Проверяется необходимость отправки уведомления в зависимости от установленных флагов 
// true - сообщение принято (или запрещено), false - сообщение отвергнуто т.к оно уже посылалось (дубль) или внутренняя ошибка
boolean Message::setMessage(MESSAGE ms, char *c, int p1)
{
char temp[10];    
//  Serial.print(c);Serial.print(" : ");Serial.print(ms);Serial.println("-5");
   // Проверка необходимости отправки уведомления
       if (((GETBIT(messageSetting.flags,fMessageReset))==0)&&(ms==pMESSAGE_RESET))       return true;   // Попытка отправить не разрешенное сообщение, выходим без ошибок
       if (((GETBIT(messageSetting.flags,fMessageError))==0)&&(ms==pMESSAGE_ERROR))       return true;   
       if (((GETBIT(messageSetting.flags,fMessageLife))==0)&&(ms==pMESSAGE_LIFE))         return true;   
       if (((GETBIT(messageSetting.flags,fMessageTemp))==0)&&(ms==pMESSAGE_TEMP))         return true;   
       if (((GETBIT(messageSetting.flags,fMessageSD))==0)&&(ms==pMESSAGE_SD))             return true; 
       if (((GETBIT(messageSetting.flags,fMessageWarning))==0)&&(ms==pMESSAGE_WARNING))   return true; 
 // else if (((messageSetting.GETBIT(fMessageTemp))&&(ms==pMESSAGE_TEMP))&&((sTemp[TIN].get_Temp()<messageSetting.mTIN)||(sTemp[TBOILER].get_Temp()<messageSetting.mTBOILER)||(sTemp[TCOMP].get_Temp()>messageSetting.mTCOMP)))  return true;  // выходим, температуры в границах!!
 
    // Проверка на дублирование сообщения. Тестовые сообщения и сообщения жизни  можно посылать многократно  подряд
    if ((rtcSAM3X8.unixtime()-sendTime<REPEAT_TIME)&&(messageData.ms==ms)&&((ms!=pMESSAGE_TESTMAIL)&&(ms!=pMESSAGE_TESTSMS)&&(ms!=pMESSAGE_LIFE)))   //дублирующие сообщения полылаются с интервалом
      {
        journal.jprintf("Ignore repeat message: #%d:%s\n",ms,c);
        return false; 
      }
      
    // Подготовить уведомление
    messageData.ms=ms;
    sendTime=rtcSAM3X8.unixtime(); // запомнить время отправки
    strcpy(messageData.data,c);
    // в сообщение pMESSAGE_TEMP добавить значение температуры
    if (ms==pMESSAGE_TEMP) { strcat(messageData.data," t=");  strcat(messageData.data,ftoa(temp,(float)p1/100.0,1)); }
    messageData.p1=p1;
    // очистить ответы
    strcpy(retTest,"");             // обнулить ответ от посылки тестового письма
    strcpy(retMail,"");             // обнулить ошибку от посылки тестового письма
    strcpy(retSMS,"");              // обнулить ошибку от посылки тестового SMS
    waitSend=true;                  // выставить флаг необходимости отправки Уведомления   
    return true;
}

// Установить (сформировать) тестовое письмо, отправка sendMessage();  
boolean Message::setTestMail()
{
  return setMessage(pMESSAGE_TESTMAIL,(char*)"Тестовое уведомление, для проверки почты", 0);                                              
}

//Установить (сформировать) тестовое СМС, отправка sendMessage();  
boolean Message::setTestSMS()
{
    return setMessage(pMESSAGE_TESTSMS,(char*)"Тестовое уведомление, для проверки SMS", 0);     
}

// Послать уведомление согласно выбранных настроек cформированное setMessage
// проверяется наличие неоправленныхуведомлений
// true - уведомление отправлено или его не было false - при отправке произошли ошибки 
boolean Message::sendMessage()   
{
uint16_t i;  

  if (!waitSend) return true;                            // Отправлять нечего выходим
  if(HP.get_uptime()<cDELAY_START_MESSAGE) {_delay(200); return true;}   // Прошло мало времени после старта возможно инет еще не поднят
 
   // Отправка Уведомления. Все таки отправлять придется -))  
     if ((GETBIT(messageSetting.flags,fMail))&&(messageData.ms!=pMESSAGE_TESTSMS))   // Разрешены уведомления по почте и не тест SMS
      {
      if(sendMail()) // Формирование ответа
         {
           // Отправка удачна
           for(i=0;i<strlen(retMail);i++) if(retMail[i]=='=') retMail[i]=':';  // замена знака = на : т.к. это запрещенный знак в запросах
           strcpy(retTest,"Тестовое письмо отправлено на "); strcat(retTest,HP.message.get_messageSetting(pSMTP_RCPTTO)); 
           strcat(retTest,"\nОтвет: "); strcat(retTest,retMail); 
         }
       else
         {
           // Отправка не удачна
           for(i=0;i<strlen(retMail);i++) if(retMail[i]=='=') retMail[i]=':';  // замена знака = на : т.к. это запрещенный знак в запросах           
           strcpy(retTest,"Тестовое письмо НЕ отправлено на "); strcat(retTest,HP.message.get_messageSetting(pSMTP_RCPTTO)); 
           strcat(retTest,"\nОтвет: "); strcat(retTest,retMail); 
         }
      }
  
   if ((!(GETBIT(messageSetting.flags,fMail)))&&(messageData.ms==pMESSAGE_TESTMAIL))   // Почта не разрешена а отправляется тестовое письмо
   { strcpy(retTest,"Письмо не отправлено. Проверьте флаг разрешения отправки почты.");}

   if ((GETBIT(messageSetting.flags,fSMS))&&(messageData.ms!=pMESSAGE_TESTMAIL))  // Разрешены уведомления по SMS и не тест mail
      {
      switch (messageSetting.sms_service)
             {
              case pSMS_RU: 
                   if (sendSMS())
                      {  // Удачно
                      strcpy(retTest,"Тестовое SMS отправлено на номер "); strcat(retTest,HP.message.get_messageSetting(pSMS_PHONE)); 
                      strcat(retTest,"\nОтвет: "); strcat(retTest,retSMS); 
                      }
                      else
                      { // Не удачно
                      strcpy(retTest,"Тестовое SMS НЕ отправлено на номер "); strcat(retTest,HP.message.get_messageSetting(pSMS_PHONE)); 
                      strcat(retTest,"\nОтвет: "); strcat(retTest,retSMS); 
                      }
              break;
              case pSMSC_RU:  strcpy(retTest,"Сервис пока не поддерживается");  break;
              default:    return false;      break; // ничего не делаем но это ошибка
             }  
      }
      
   if ((!(GETBIT(messageSetting.flags,fSMS)))&&(messageData.ms==pMESSAGE_TESTSMS))   // SMS не разрешена а отправляется тестовое SMS
   strcpy(retTest,"SMS не отправлено. Проверьте флаг разрешения отправки SMS.");
   
   waitSend=false;          // Сбросить флаг необходимости отпвки уведомления      
   return true;             // Послано                                               
}

// Отправить почту --------------------------------------------------------------------
boolean Message::sendMail()   
{
strcpy(retMail,"");  // Обнулить ошибку
// Подготовка
if(SemaphoreTake(xWebThreadSemaphore,(W5200_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE)  {return false;}  // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
    
     // 1. Соединение по телнету 
     if (clientMessage.connect(messageSetting.smtp_server,messageSetting.smtp_port,W5200_SOCK_SYS)) 
        {
        journal.jprintf("Connected server: %s port: %d, sock: %d\n",messageSetting.smtp_server,messageSetting.smtp_port,W5200_SOCK_SYS);
        } 
      else 
        {
        journal.jprintf("Connection failed server: %s port: %d, sock: %d\n",messageSetting.smtp_server,messageSetting.smtp_port,W5200_SOCK_SYS);
        strncpy(retMail,"No connect",LEN_RETMAIL);  
        SemaphoreGive(xWebThreadSemaphore);
        return false;
        }
   
      // 2. Общение с сервером, получаем приветствие при соединении
      if (!SendCommandSMTP((char*)"",true))    { clientMessage.stop(); SemaphoreGive(xWebThreadSemaphore);if (strlen(retMail)==0) strncpy(retMail,(char*)"No answer",LEN_RETMAIL);return false; }  // ответ содержит ошибки
      // 3. Авторизация
      if (GETBIT(messageSetting.flags,fMailAUTH))                         // Требуется авторизация
        {
        if (!SendCommandSMTP((char*)"EHLO host",true))    { clientMessage.stop(); SemaphoreGive(xWebThreadSemaphore);return false; }       // ответ содержит ошибки
        if (!SendCommandSMTP((char*)"AUTH LOGIN",true))   { clientMessage.stop(); SemaphoreGive(xWebThreadSemaphore);return false; }       // ответ содержит ошибки
        strcpy(tempBuf,"");
        base64_encode(tempBuf,messageSetting.smtp_login,strlen(messageSetting.smtp_login));// Послать логин 
        if (!SendCommandSMTP(tempBuf,true))   { clientMessage.stop(); SemaphoreGive(xWebThreadSemaphore);  return false; }                 // ответ содержит ошибки
        strcpy(tempBuf,"");
        base64_encode(tempBuf,messageSetting.smtp_password,strlen(messageSetting.smtp_password));  // Послать пароль
        if (!SendCommandSMTP(tempBuf,true))   { clientMessage.stop(); SemaphoreGive(xWebThreadSemaphore); return false; }                  // ответ содержит ошибки
        }
      else                                                               // Авторизация не требуется
       {
        if (!SendCommandSMTP((char*)"HELO host",true))    { clientMessage.stop(); SemaphoreGive(xWebThreadSemaphore); return false; }        // ответ содержит ошибки
       }
       
       // 4. Формирование адресов
       strcpy(tempBuf,"MAIL From: <"); strcat(tempBuf,messageSetting.smtp_MailTo);  strcat(tempBuf,">");
       if (!SendCommandSMTP(tempBuf,true))    { clientMessage.stop(); SemaphoreGive(xWebThreadSemaphore);return false; }            // ответ содержит ошибки
       strcpy(tempBuf,"RCPT To: <");   strcat(tempBuf,messageSetting.smtp_RCPTTo);  strcat(tempBuf,">");
       if (!SendCommandSMTP(tempBuf,true))    { clientMessage.stop(); SemaphoreGive(xWebThreadSemaphore);return false; }            // ответ содержит ошибки
       
       // 5. Заголовок сообщения  
       if (!SendCommandSMTP((char*)"DATA",true))   { clientMessage.stop(); SemaphoreGive(xWebThreadSemaphore); return false; }       // ответ содержит ошибки
       strcpy(tempBuf,"To: <");      strcat(tempBuf,messageSetting.smtp_RCPTTo); strcat(tempBuf,">");
       if (!SendCommandSMTP(tempBuf,false))   { clientMessage.stop(); SemaphoreGive(xWebThreadSemaphore); return false; }            // ответ содержит ошибки
       strcpy(tempBuf,"From: <");    strcat(tempBuf,messageSetting.smtp_MailTo); strcat(tempBuf,">");
       if (!SendCommandSMTP(tempBuf,false))    { clientMessage.stop(); SemaphoreGive(xWebThreadSemaphore); return false; }           // ответ содержит ошибки
       strcpy(tempBuf,"Content-type: text/plain; charset=\"utf-8\"");              // Кодировка
       if (!SendCommandSMTP(tempBuf,false))    { clientMessage.stop(); SemaphoreGive(xWebThreadSemaphore); return false; }           // ответ содержит ошибки
       strcpy(tempBuf,"Subject: "); // Тема письма
       strcat(tempBuf,"Controller ");
       strcat(tempBuf,(char*)nameHeatPump);
       switch ((int)messageData.ms)   // Заголовок уведомления - добавляем тип уведомления
           {
            case pMESSAGE_NONE    : break;                                                                                          // Нет уведомлений
            case pMESSAGE_TESTMAIL: strcat(tempBuf,"TESTMAIL\r\n"); break;                                                          // Тестовое почта
            case pMESSAGE_TESTSMS : strcat(tempBuf,"TESTSMS\r\n");  break;                                                          // Тестовое смс
            case pMESSAGE_RESET   : strcat(tempBuf,"RESET\r\n"); break;                                                             // Уведомление Сброс
            case pMESSAGE_ERROR   : strcat(tempBuf,"ERROR\r\n"); break;                                                             // Уведомление Ошибка
            case pMESSAGE_LIFE    : strcat(tempBuf,"LIFE\r\n"); break;                                                              // Уведомление Сигнал жизни
            case pMESSAGE_TEMP    : strcat(tempBuf,"TEMP\r\n"); break;                                                              // Уведомление Достижение граничной температуры
            case pMESSAGE_SD      : strcat(tempBuf,"ERROR SD\r\n"); break;                                                          // Уведомление "Проблемы с sd картой"
            case pMESSAGE_WARNING : strcat(tempBuf,"WARNING\r\n"); break;                                                           // Уведомление "Прочие уведомления"  
            default               : strcat(tempBuf,"ERROR TYPE \r\n");clientMessage.stop(); SemaphoreGive(xWebThreadSemaphore); return false; break;// ответ содержит ошибки
           }   
       if (!SendCommandSMTP(tempBuf,false))    { clientMessage.stop(); SemaphoreGive(xWebThreadSemaphore); return false; }            // послать тему письма
             
         // 6. Текст сообщения  
        strcpy(tempBuf," ------ Народный контроллер теплового насоса ver. ");strcat(tempBuf,VERSION); strcat(tempBuf," ------");
        strcat(tempBuf,cStrEnd);  clientMessage.write(tempBuf,strlen(tempBuf));
 
       
        strcpy(tempBuf,"Конфигурация: "); strcat(tempBuf,CONFIG_NAME); strcat(tempBuf," ("); strcat(tempBuf,CONFIG_NOTE ); strcat(tempBuf,")");
        strcat(tempBuf,cStrEnd);  clientMessage.write(tempBuf,strlen(tempBuf));
    
        
        strcpy(tempBuf,"Создание уведомления: ");strcat(tempBuf,NowDateToStr()); strcat(tempBuf," "); strcat(tempBuf,NowTimeToStr());  
        strcat(tempBuf,cStrEnd);  clientMessage.write(tempBuf,strlen(tempBuf));
      
        strcat(messageData.data,cStrEnd);  clientMessage.write(messageData.data,strlen(messageData.data));
         
    //    strcpy(tempBuf,"P1: ");strcat(tempBuf,int2str(messageData.p1)); 
    //    client.println(tempBuf);
        
        
        // 7. Дополнительная информация если требуется добавляется в уведомление
        if (GETBIT(messageSetting.flags,fMailInfo))  get_mailState(clientMessage,tempBuf); 
     
       
      // 8. Завершение сессии  и отправка
       if (!SendCommandSMTP((char*)".",true))      { clientMessage.stop(); SemaphoreGive(xWebThreadSemaphore); return false; }               // ответ содержит ошибки
       strncpy(retMail,(char*)tempBuf,LEN_RETMAIL);                                                                    // Копирование сообщения сервера при удачной отправки
       if (!SendCommandSMTP((char*)"QUIT",true))   { clientMessage.stop(); SemaphoreGive(xWebThreadSemaphore);return false; }                // ответ содержит ошибки
       clientMessage.stop();
       journal.jprintf("OK disconnected\n");
       SemaphoreGive(xWebThreadSemaphore);
       return true;                  
}

// Отправить SMS sms.ru ----------------------------------------------------------------------------------
boolean Message::sendSMS()
{
uint16_t i;
uint8_t count=0;
IPAddress zeroIP(0,0,0,0);  

if(SemaphoreTake(xWebThreadSemaphore,(W5200_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE)  {return false;}  // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим

 strcpy(retSMS,ADR_SMS_RU);  // Используется как временный буфер check_address с констанатми не работает??

 if(messageSetting.sms_serviceIP==zeroIP) // возможно днс надо всегда обновлять
 {
 //обновляем IP адрес сервера
  if (!check_address(retSMS,messageSetting.sms_serviceIP)) {strcpy(retSMS,ADR_SMS_RU); strcat(retSMS," DNS lookup failed!"); return false;}  // Обновить адрес
 }

 strcpy(retSMS,"");          // Обнулить ответ
 if (clientMessage.connect(messageSetting.sms_serviceIP,80,W5200_SOCK_SYS))  // Соединение по HTTP
        {
         journal.jprintf("Connected server: %s",ADR_SMS_RU); journal.jprintf(" port: %d\n",80);
         } 
      else 
        {
         journal.jprintf("Connection failed server: %s",ADR_SMS_RU); journal.jprintf(" port: %d\n",80); 
         strcpy(retSMS,"No connect "); strcat(retSMS,ADR_SMS_RU);  
         clientMessage.stop();
         SemaphoreGive(xWebThreadSemaphore);
         return false;
        }

// посылка смс     
strcpy(tempBuf,"GET http://sms.ru/sms/send?api_id=");                     // Начало
strcat(tempBuf,messageSetting.sms_p1);                                    // id
strcat(tempBuf,"&to=");  
strcat(tempBuf,messageSetting.sms_phone);                                 // номер
strcat(tempBuf,"&text=");  
strcat(tempBuf,"Control+");
    switch ((int)messageData.ms)   // Заголовок уведомления - добавляем тип уведомления
     {
      case pMESSAGE_NONE    : break;                                                                                       // Нет уведомлений
      case pMESSAGE_TESTMAIL: strcat(tempBuf,"TESTMAIL+"); break;                                                          // Тестовое почта
      case pMESSAGE_TESTSMS : strcat(tempBuf,"TESTSMS+");  break;                                                          // Тестовое смс
      case pMESSAGE_RESET   : strcat(tempBuf,"RESET+"); break;                                                             // Уведомление Сброс
      case pMESSAGE_ERROR   : strcat(tempBuf,"ERROR+"); break;                                                             // Уведомление Ошибка
      case pMESSAGE_LIFE    : strcat(tempBuf,"LIFE+"); break;                                                              // Уведомление Сигнал жизни
      case pMESSAGE_TEMP    : strcat(tempBuf,"TEMP+"); break;                                                              // Уведомление Достижение граничной температуры
      case pMESSAGE_SD      : strcat(tempBuf,"ERROR+SD+"); break;                                                          // Уведомление "Проблемы с sd картой"
      case pMESSAGE_WARNING : strcat(tempBuf,"WARNING+"); break;                                                           // Уведомление "Прочие уведомления"  
      default               : strcat(tempBuf,"ERROR+TYPE+"); SemaphoreGive(xWebThreadSemaphore); return false; break;     // ответ содержит ошибки
     }
for(i=0;i<strlen(messageData.data);i++) if(messageData.data[i]==' ') messageData.data[i]='+';  // замена пробела на знак "+" т.к. это запрещенный знак при отправке сообщения
strcat(tempBuf,messageData.data);                        // содержимое
strcat(tempBuf,cStrEnd);  clientMessage.write(tempBuf,strlen(tempBuf));

//Serial.println(strlen(tempBuf));
journal.jprintf("%s\n",tempBuf);

 // Ожидание и получение ответа
  while(!clientMessage.available()) // ожидание ответа 5 сек
  {
    SemaphoreGive(xWebThreadSemaphore);
    _delay(200);
    if(SemaphoreTake(xWebThreadSemaphore,(W5200_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE)  {return false;}  // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
    count++;
    if (count>25)
     {
       strcpy(retSMS,"Server not answer . . .");
       journal.jprintf("%s\n",retSMS);
       clientMessage.stop();
       SemaphoreGive(xWebThreadSemaphore);
       return false;  
     }
     
  }

// разбор ответа
byte thisByte;
// режем заголовок, он отделен последовательностью 0x0a 0x0d 0x0a от ответа
 while (clientMessage.available()) 
{ 
  thisByte = clientMessage.read(); 
  if (thisByte==0x0a)
    {
     thisByte = clientMessage.read(); 
     if (thisByte==0x0d)
        { 
          thisByte = clientMessage.read();  
          if (thisByte==0x0a)
           {
            i=clientMessage.read((byte*)retSMS,sizeof(retSMS)-1);  // получаем код ответа 
            retSMS[i]=0;                                           // обрезаем строку 
            for(i=0;i<strlen(retSMS);i++) if(retSMS[i]=='=') retSMS[i]=':';  // замена = на знак ":" веб морда глючит в ответах - обрезает по  =
            journal.jprintf("sms.ru return: %s \n",retSMS);  
            clientMessage.stop(); 
            SemaphoreGive(xWebThreadSemaphore);
            return true; 
           }
        }
    }
    
}
clientMessage.stop();
SemaphoreGive(xWebThreadSemaphore);
return false;  
}


// Отправить SMS smsc.ru ----------------------------------------------------------
boolean Message::sendSMSC()
{
EthernetClient client; 
client.stop();
_delay(100);
 // 1. Соединение по телнету 
if (client.connect("http://sms.ru/sms/send",80)) 
//     if (client.connect(smtp,messageSetting.smtp_port)) 
  {
   journal.jprintf("Connected server: http://sms.ru/sms/send port:80\n");
  } 
else 
  {
   journal.jprintf("Connection failed server: http://sms.ru/sms/send port:80\n"); 
   return false;
  }
client.println("GET api_id=[]&to=[номер получателя]&text=hello+world HTTP/1.1");
client.println("Host: sms.ru");
client.println("Connection: close");
client.println();

return true;            
 
}



