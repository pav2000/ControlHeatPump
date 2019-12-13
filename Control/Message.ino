/*
 * Copyright (c) 2016-2019 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
 * &                       by Vadim Kulakov vad7@yahoo.com, vad711
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
extern void get_mailState(EthernetClient client, char *tempBuf);
#define JOURNAL(...) if(GETBIT(HP.get_NetworkFlags(), fWebFullLog)) journal.jprintf(__VA_ARGS__)

// Инициализация
// Уведомления по умолчанию настройка на gmail без шифрования
// параметр - номер потока сервера в котором запускается отправка уведомлений
void Message::initMessage(uint8_t web_task)
{
  // инициализации рабочих буферов MQTT
  tempBuf=Socket[web_task].outBuf+0;
  tempBuf[0]=0x00; // Стереть строку
  retMail=Socket[web_task].outBuf+LEN_TEMPBUF;
  retMail[0]=0x00;
  retSMS=Socket[web_task].outBuf+LEN_TEMPBUF+LEN_RETMAIL;
  retSMS[0]=0x00;
//  retTest=Socket[web_task].outBuf+LEN_TEMPBUF+LEN_RETMAIL+LEN_RETSMS;// Хранится отдельно!
  retTest[0]=0x00;
  // Инициализация переменных
  IPAddress zeroIP(0, 0, 0, 0);
  lastmessageSetting = pMESSAGE_NONE;                                              // последнее отправленное уведомление
  dnsUpadateSMS = false;                                                           // Флаг необходимости обновления через dns IP адреса для смс
  dnsUpadateSMTP = false;                                                          // Флаг необходимости обновления через dns IP адреса для smtp
  sendTime = 0;                                                                    // отправок уведомлений не было

  SETBIT0(messageSetting.flags, fMail );                                           // флаг уведомления скидывать на почту
  SETBIT1(messageSetting.flags, fMailAUTH);                                        // флаг необходимости авторизации на почтовом сервере
  SETBIT1(messageSetting.flags, fMailInfo);                                        // флаг необходимости добавления в письмо информации о состянии ТН
  SETBIT0(messageSetting.flags, fSMS);                                             // флаг уведомления скидывать на СМС
  SETBIT1(messageSetting.flags, fMessageReset);                                    // флаг уведомления Сброс
  SETBIT1(messageSetting.flags, fMessageError);                                    // флаг уведомления Ошибка
  SETBIT1(messageSetting.flags, fMessageLife);                                     // флаг уведомления Сигнал жизни
  SETBIT1(messageSetting.flags, fMessageTemp);                                     // флаг уведомления Достижение граничной температуры
  SETBIT1(messageSetting.flags, fMessageSD);                                       // флаг уведомления "Проблемы с sd картой"
  SETBIT1(messageSetting.flags, fMessageWarning);                                  // флаг уведомления "Прочие уведомления"

  //  rtcSAM3X8.set_alarmtime(11, 17, 0);                                              // завести будильник для отправки сигнала жизни
  //  rtcSAM3X8.attachalarm(life_signal);

  strcpy(messageSetting.smtp_server, "smtp-devices.yandex.ru");                    // Адрес сервера без SSL/TSL
  messageSetting.smtp_serverIP = zeroIP;                                           // сделать адрес 0.0.0.0
  messageSetting.smtp_port = 25;                                                   // Адрес порта сервера
  strcpy(messageSetting.smtp_login, "login");                                      // логин сервера если включена авторизация
  strcpy(messageSetting.smtp_password, "password");                                // пароль сервера если включена авторизация
  strcpy(messageSetting.smtp_MailTo, "MK@home");                                   // адрес отправителя
  strcpy(messageSetting.smtp_RCPTTo, "");                                          // адрес получателя

  messageSetting.sms_service = pSMS_RU;                                            // Cервис отправки смс по умолчанию sms.ru
  messageSetting.sms_serviceIP = zeroIP;                                           // сделать адрес 0.0.0.0
  strcpy(messageSetting.sms_phone, "1234567890");                                  // телефон куда отправляется смс
  strcpy(messageSetting.sms_p1, "api_id");                                         // первый параметр для отправки смс
  strcpy(messageSetting.sms_p2, "none");                                           // второй параметр для отправки смс

  messageSetting.mTIN = 1000;                                                      // Критическая температура в доме (если меньше то генерится уведомление)
  messageSetting.mTBOILER = 500;                                                   // Критическая температура бойлера (если меньше то генерится уведомление)
  messageSetting.mTCOMP = 7000;                                                    // Критическая температура компрессора (если больше то генерится уведомление)

  // установка содержания уведомлений по умолчаню
  messageData.ms = pMESSAGE_NONE;    // Тип уведомления
  strcpy(messageData.data, "");      // Полезная информация
  messageData.p1 = 0;                // первый параметр
  messageData.number = 0;            // номер уведомления

}

// Обновление IP адресов серверов через dns
// Возврат - флаг использования дополнительного сокета и времени (нужен для разгрузки 0 потока сервера)
// возвращает true обновление не было (можно нагружать), false - прошло обновление или ошибка (нагружать не надо до следующего цикла)
boolean Message::dnsUpdate() // запускается 0 потоке вебсервера
{
	boolean ret = true; // по умолчанию преобразование не было
	if(xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
		dnsUpadateSMTP = dnsUpadateSMS = true;
		WDT_Restart(WDT);
	}
	if(dnsUpadateSMTP) //надо обновлятся
	{
		if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING && SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE) return false; // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
		check_address(messageSetting.smtp_server, messageSetting.smtp_serverIP);   // Получить адрес IP через DNS При не удаче возвращается 0, при удаче: 1 - IP на входе (были цифры, DNS не нужен), 2 - был запрос к DNS и адрес получен
		dnsUpadateSMTP = false;
		if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) SemaphoreGive(xWebThreadSemaphore);
		ret = false;  // вызов обновления был
	}
	if(dnsUpadateSMS) //надо обновлятся
	{
		if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
			if(SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE) return false; // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
		} else WDT_Restart(WDT);
		switch(messageSetting.sms_service) {
		case pSMS_RU:
			ret = strcpy(tempBuf, ADR_SMS_RU);
			break;
		case pSMSC_RU:
			ret = strcpy(tempBuf, ADR_SMSC_RU);
			break;
		case pSMSC_UA:
			ret = strcpy(tempBuf, ADR_SMSC_UA);
			break;
		case pSMSCLUB_UA:
			ret = strcpy(tempBuf, ADR_SMSCLUB_UA);
			break;
		}
		check_address(tempBuf, messageSetting.sms_serviceIP);// При не удаче возвращается 0, при удаче: 1 - IP на входе (были цифры, DNS не нужен), 2 - был запрос к DNS и адрес получен
		dnsUpadateSMS = false;
		if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) SemaphoreGive(xWebThreadSemaphore);
		ret = false;  // вызов обновления был
	}
	return ret;
}

// Установить параметр Уведомления из строки
boolean Message::set_messageSetting(char *var, char *c)
{
  float x;
  if (strcmp(var, mess_MAIL) == 0) { if (strcmp(c, cZero) == 0)      { SETBIT0(messageSetting.flags, fMail); return true;} else if (strcmp(c, cOne) == 0) { SETBIT1(messageSetting.flags, fMail);  return true;  } else return false;
  } else if (strcmp(var, mess_MAIL_AUTH) == 0) { if (strcmp(c, cZero) == 0)   {  SETBIT0(messageSetting.flags, fMailAUTH);  return true;  } else if (strcmp(c, cOne) == 0) { SETBIT1(messageSetting.flags, fMailAUTH); return true; } else return false;
  } else if (strcmp(var, mess_MAIL_INFO) == 0) { if (strcmp(c, cZero) == 0)   {  SETBIT0(messageSetting.flags, fMailInfo);  return true;  } else if (strcmp(c, cOne) == 0) { SETBIT1(messageSetting.flags, fMailInfo); return true; } else return false;
  } else if (strcmp(var, mess_MESS_RESET) == 0) { if (strcmp(c, cZero) == 0)  {  SETBIT0(messageSetting.flags, fMessageReset); return true; } else if (strcmp(c, cOne) == 0) { SETBIT1(messageSetting.flags, fMessageReset); return true; } else return false;
  } else if (strcmp(var, mess_MESS_ERROR) == 0) { if (strcmp(c, cZero) == 0)  {  SETBIT0(messageSetting.flags, fMessageError); return true; } else if (strcmp(c, cOne) == 0) { SETBIT1(messageSetting.flags, fMessageError); return true; } else return false;
  } else if (strcmp(var, mess_MESS_LIFE) == 0) { if (strcmp(c, cZero) == 0)   {  SETBIT0(messageSetting.flags, fMessageLife);  return true; } else if (strcmp(c, cOne) == 0) { SETBIT1(messageSetting.flags, fMessageLife); return true; } else return false;
  } else if (strcmp(var, mess_MESS_TEMP) == 0) { if (strcmp(c, cZero) == 0)   {  SETBIT0(messageSetting.flags, fMessageTemp);  return true; } else if (strcmp(c, cOne) == 0) { SETBIT1(messageSetting.flags, fMessageTemp); return true; } else return false;
  } else if (strcmp(var, mess_MESS_SD) == 0) { if (strcmp(c, cZero) == 0)     {  SETBIT0(messageSetting.flags, fMessageSD);  return true;  } else if (strcmp(c, cOne) == 0) { SETBIT1(messageSetting.flags, fMessageSD); return true; } else return false;
  } else if (strcmp(var, mess_MESS_WARNING) == 0) { if (strcmp(c, cZero) == 0) { SETBIT0(messageSetting.flags, fMessageWarning); return true; } else if (strcmp(c, cOne) == 0) { SETBIT1(messageSetting.flags, fMessageWarning); return true; } else return false;
  } else if(strcmp(var, mess_SMTP_SERVER) == 0) {
	  if(strlen(c) == 0) return false;                                             // пустая строка
	  if(strlen(c) > sizeof(messageSetting.smtp_server) - 1) return false;         // слишком длиная строка
	  else // ок сохраняем
	  {
		  strcpy(messageSetting.smtp_server, c);
		  dnsUpadateSMTP = true;
		  //       check_address(messageSetting.smtp_server,&messageSetting.smtp_serverIP);      // Получить адрес IP через DNS
		  return true;
	  }
  } else if(strcmp(var, mess_SMTP_PORT) == 0) {
	  x = my_atof(c);
	  if(x == ATOF_ERROR) return false;
	  else if((x <= 1) || (x >= 65535 - 1)) return false;
	  else messageSetting.smtp_port = (int) x;
	  return true;
  } else if(strcmp(var, mess_SMTP_LOGIN) == 0) {
	  if(strlen(c) == 0) return false;
	  if(strlen(c) > sizeof(messageSetting.smtp_login) - 1) return false;
	  else {
		  strcpy(messageSetting.smtp_login, c);
		  return true;
	  }
  } else if(strcmp(var, mess_SMTP_PASS) == 0) {
	  if(strlen(c) == 0) return false;
	  if(strlen(c) > sizeof(messageSetting.smtp_password) - 1) return false;
	  else {
		  strcpy(messageSetting.smtp_password, c);
		  return true;
	  }
  } else if(strcmp(var, mess_SMTP_MAILTO) == 0) {
	  if(strlen(c) == 0) return false;
	  if(strlen(c) > sizeof(messageSetting.smtp_MailTo) - 1) return false;
	  else {
		  strcpy(messageSetting.smtp_MailTo, c);
		  return true;
	  }
  } else if(strcmp(var, mess_SMTP_RCPTTO) == 0) {
	  if(strlen(c) == 0) return false;
	  if(strlen(c) > sizeof(messageSetting.smtp_RCPTTo) - 1) return false;
	  else {
		  strcpy(messageSetting.smtp_RCPTTo, c);
		  return true;
	  }
  } else if(strcmp(var, mess_SMS) == 0) {
	  if(strcmp(c, cZero) == 0) {
		  SETBIT0(messageSetting.flags, fSMS);
		  return true;
	  } else if(strcmp(c, cOne) == 0) {
		  SETBIT1(messageSetting.flags, fSMS);
		  return true;
	  } else return false;
  } else if(strcmp(var, mess_SMS_SERVICE) == 0) {
	  x = my_atof(c);  // При смене сервиса определяем новый IP поле sms_serviceIP
	  if(x == ATOF_ERROR) return false;
	  messageSetting.sms_service = (SMS_SERVICE) x;
	  dnsUpadateSMS = true;
	  return true;
  } else if(strcmp(var, mess_SMS_PHONE) == 0) {
	  if(strlen(c) == 0) return false;
	  if(strlen(c) > sizeof(messageSetting.sms_phone) - 1) return false;
	  else {
		  strcpy(messageSetting.sms_phone, c);
		  return true;
	  }
  } else if(strcmp(var, mess_SMS_P1) == 0) {
	  if(strlen(c) == 0) return false;       // первый параметр для отправки смс
	  if(strlen(c) > sizeof(messageSetting.sms_p1) - 1) return false;
	  else {
		  strcpy(messageSetting.sms_p1, c);
		  return true;
	  }
  } else if(strcmp(var, mess_SMS_P2) == 0) {
	  if(strlen(c) == 0) return false;       // второй параметр для отправки смс
	  if(strlen(c) > sizeof(messageSetting.sms_p2) - 1) return false;
	  else {
		  strcpy(messageSetting.sms_p2, c);
		  return true;
	  }
  } else if(strcmp(var, mess_MESS_TIN) == 0) {
	  x = my_atof(c);
	  if(x == ATOF_ERROR) return false;
	  else messageSetting.mTIN = rd(x, 100);
	  return true;
  } else if(strcmp(var, mess_MESS_TBOILER) == 0) {
	  x = my_atof(c);
	  if(x == ATOF_ERROR) return false;
	  else messageSetting.mTBOILER = rd(x, 100);
	  return true;
  } else if(strcmp(var, mess_MESS_TCOMP) == 0) {
	  x = my_atof(c);
	  if(x == ATOF_ERROR) return false;
	  else messageSetting.mTCOMP = rd(x, 100);
	  return true;
  } else if(strcmp(var, mess_MAIL_RET) == 0) {
	  return true;
  } else if(strcmp(var, mess_SMS_RET) == 0) {
	  return true;
  } else return false;

}
// Получить параметр Уведомления по имени var, результат ДОБАВОЯЕТСЯ в строку ret
char* Message::get_messageSetting(char *var, char *ret)
{
	if(strcmp(var, mess_MAIL) == 0) {
		if(GETBIT(messageSetting.flags, fMail)) return strcat(ret, (char*) cOne);
		else return strcat(ret, (char*) cZero);
	} else if(strcmp(var, mess_MAIL_AUTH) == 0) {
		if(GETBIT(messageSetting.flags, fMailAUTH)) return strcat(ret, (char*) cOne);
		else return strcat(ret, (char*) cZero);
	} else if(strcmp(var, mess_MAIL_INFO) == 0) {
		if(GETBIT(messageSetting.flags, fMailInfo)) return strcat(ret, (char*) cOne);
		else return strcat(ret, (char*) cZero);
	} else if(strcmp(var, mess_SMS) == 0) {
		if(GETBIT(messageSetting.flags, fSMS)) return strcat(ret, (char*) cOne);
		else return strcat(ret, (char*) cZero);
	} else if(strcmp(var, mess_MESS_RESET) == 0) {
		if(GETBIT(messageSetting.flags, fMessageReset)) return strcat(ret, (char*) cOne);
		else return strcat(ret, (char*) cZero);
	} else if(strcmp(var, mess_MESS_ERROR) == 0) {
		if(GETBIT(messageSetting.flags, fMessageError)) return strcat(ret, (char*) cOne);
		else return strcat(ret, (char*) cZero);
	} else if(strcmp(var, mess_MESS_LIFE) == 0) {
		if(GETBIT(messageSetting.flags, fMessageLife)) return strcat(ret, (char*) cOne);
		else return strcat(ret, (char*) cZero);
	} else if(strcmp(var, mess_MESS_TEMP) == 0) {
		if(GETBIT(messageSetting.flags, fMessageTemp)) return strcat(ret, (char*) cOne);
		else return strcat(ret, (char*) cZero);
	} else if(strcmp(var, mess_MESS_SD) == 0) {
		if(GETBIT(messageSetting.flags, fMessageSD)) return strcat(ret, (char*) cOne);
		else return strcat(ret, (char*) cZero);
	} else if(strcmp(var, mess_MESS_WARNING) == 0) {
		if(GETBIT(messageSetting.flags, fMessageWarning)) return strcat(ret, (char*) cOne);
		else return strcat(ret, (char*) cZero);
	} else if(strcmp(var, mess_SMTP_SERVER) == 0) {
		return strcat(ret, messageSetting.smtp_server);
	} else if(strcmp(var, mess_SMTP_IP) == 0) {
		return strcat(ret, IPAddress2String(messageSetting.smtp_serverIP));
	} else if(strcmp(var, mess_SMTP_PORT) == 0) {
		return _itoa(messageSetting.smtp_port, ret);
	} else if(strcmp(var, mess_SMTP_LOGIN) == 0) {
		return strcat(ret, messageSetting.smtp_login);
	} else if(strcmp(var, mess_SMTP_PASS) == 0) {
		return strcat(ret, messageSetting.smtp_password);
	} else if(strcmp(var, mess_SMTP_MAILTO) == 0) {
		return strcat(ret, messageSetting.smtp_MailTo);
	} else if(strcmp(var, mess_SMTP_RCPTTO) == 0) {
		return strcat(ret, messageSetting.smtp_RCPTTo);
	} else if(strcmp(var, mess_SMS_SERVICE) == 0) {
		return web_fill_tag_select(ret, SMS_SERVICE_WEB_SELECT, messageSetting.sms_service);
	} else if(strcmp(var, mess_SMS_IP) == 0) {
		return strcat(ret, IPAddress2String(messageSetting.sms_serviceIP));
	} else if(strcmp(var, mess_SMS_PHONE) == 0) {
		return strcat(ret, messageSetting.sms_phone);
	} else if(strcmp(var, mess_SMS_P1) == 0) { // первый параметр для отправки смс
		return strcat(ret, messageSetting.sms_p1);
	} else if(strcmp(var, mess_SMS_P2) == 0) { // второй параметр для отправки смс
		return strcat(ret, messageSetting.sms_p2);
	} else if(strcmp(var, mess_SMS_NAMEP1) == 0) {
		switch(messageSetting.sms_service)  // описание первого параметра для отправки смс
		{
		case pSMS_RU:
			return strcat(ret, (char*) "API ID");
			break;
		default:
			return strcat(ret, (char*) "Login");
			break; // Этого не должно быть, но если будет то установить по умолчанию
		}
	} else if(strcmp(var, mess_SMS_NAMEP2) == 0) {
		switch(messageSetting.sms_service)   // описание второго параметра для отправки смс
		{
		case pSMS_RU:
			return strcat(ret, (char*) "none");
			break;
		default:
			return strcat(ret, (char*) "Password");
			break; // Этого не должно быть, но если будет то установить по умолчанию
		}
	} else if(strcmp(var, mess_MESS_TIN) == 0) {
		_dtoa(ret, messageSetting.mTIN, 2);
		return ret;
	} else if(strcmp(var, mess_MESS_TBOILER) == 0) {
		_dtoa(ret, messageSetting.mTBOILER, 2);
		return ret;
	} else if(strcmp(var, mess_MESS_TCOMP) == 0) {
		_dtoa(ret, messageSetting.mTCOMP, 2);
		return ret;
	} else if(strcmp(var, mess_MAIL_RET) == 0) {
		if(waitSend) return strcat(ret, (char*) "wait response...");                // В зависимости готов ответ или нет
		else return strcat(ret, retTest);
	} else if(strcmp(var, mess_SMS_RET) == 0) {
		if(waitSend) return strcat(ret, (char*) "wait response...");                // В зависимости готов ответ или нет
		else return strcat(ret, retTest);
	} else return strcat(ret, (char*) cInvalid);

}
// Записать настройки в eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t Message::save(int32_t adr)
{

  // SerialDbg.print(messageSetting.GETBIT(fMessageReset));SerialDbg.println("-1");
  if (writeEEPROM_I2C(adr, (byte*)&messageSetting, sizeof(messageSetting))) {
    set_Error(ERR_SAVE_EEPROM, (char*)nameHeatPump);
    return ERR_SAVE_EEPROM;
  }  adr = adr + sizeof(messageSetting);       // записать параметры Уведомлений
  return adr;
}
// Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t Message::load(int32_t adr)
{
  if (readEEPROM_I2C(adr, (byte*)&messageSetting, sizeof(messageSetting))) {
    set_Error(ERR_LOAD_EEPROM, (char*)nameHeatPump);
    return ERR_LOAD_EEPROM;
  }  adr = adr + sizeof(messageSetting);       // прочитать параметры Уведомлений
  return adr;
}
// Считать настройки из буфера на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t Message::loadFromBuf(int32_t adr, byte *buf)
{
  memcpy((byte*)&messageSetting, buf + adr, sizeof(messageSetting)); adr = adr + sizeof(messageSetting);
  return adr;
}
// Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
uint16_t Message::get_crc16(uint16_t crc)
{
  uint16_t i;
  for (i = 0; i < sizeof(messageSetting); i++) crc = _crc16(crc, *((byte*)&messageSetting + i)); // CRC16 настройки уведомлений
  // SerialDbg.print("Message::get_crc16 0x");SerialDbg.println(crc,HEX);
  return crc;
}

// Послать команду SMTP серверу  и разобрать ответ
// wait - ждать ответ или нет
boolean  Message::SendCommandSMTP(char *c, boolean wait)
{
  uint8_t count = 0;
  byte respCode, thisByte;
  int num;

  if (!clientMessage.connected())  // если клиент не соединен то это ошибка выходим
  {
    JOURNAL("Server no connected, abort send mail???\n");
    return false;
  }

  if ((strlen(c) < LEN_TEMPBUF) && (tempBuf != c)) strcpy(tempBuf, c); // если надо то копируем

  // Послать команду, Если она не пустая (в противном случае только читаем)
  if (strlen(tempBuf) > 0) // Посылаем не пустые комнады
  {
    //    clientMessage.println(tempBuf); // почемуто не отправляет????
    strcat(tempBuf, cStrEnd);  clientMessage.write(tempBuf, strlen(tempBuf));
    //    SerialDbg.print(">>");SerialDbg.print(tempBuf);SerialDbg.print("<< len=");SerialDbg.println(strlen(tempBuf));
    JOURNAL("%s", tempBuf);
  }

  // Необходимость ожидания и получения ответа
  if (!wait) {
    strcpy(tempBuf, "");   // ответа не ожидаем
    return true;
  }


  while (!clientMessage.available()) // ожидание ответа 5 сек
  {
    SemaphoreGive(xWebThreadSemaphore);                                                   // отдать семафор для обработки других задач
    _delay(200);
    if (SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE) {
      return false; // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
    }
    count++;
    if (count > 25)
    {
      strncpy(retMail, "Server not answer . . .", LEN_RETMAIL);
      JOURNAL("%s\n", retMail);
      //    clientMessage.stop();
      //    SemaphoreGive(xWebThreadSemaphore);
      return false;
    }

  }
  // разбор ответа
  respCode = clientMessage.peek();   // первый байт - это старшая цифра ответа, по ней можно определить ошибки
  while (clientMessage.available())  // чтение ответа
  {
    num = clientMessage.read((byte*)tempBuf, LEN_TEMPBUF - 1);  tempBuf[num] = 0; // Обрезать строку
    //   SerialDbg.print("num=");SerialDbg.print(num);SerialDbg.print(" >>"); SerialDbg.print(tempBuf);
    JOURNAL("%s", tempBuf);
  }

  // Проверка на ошибки
  if (respCode >= '4') // ошибка при общении с сервером, закрываем сессию
  {
    strncpy(retMail, tempBuf, LEN_RETMAIL); // запомнить ответ сервера при ошибке
    //    SerialDbg.print("retMail "); SerialDbg.println(retMail);
    //    SerialDbg.print("answer "); SerialDbg.println(answer);
    clientMessage.println("QUIT"); // Послать команду на закрытие сессии
    JOURNAL("QUIT\n");
    while (!clientMessage.available()) // ожидание ответа 1 сек
    {
      _delay(100);
      count++;
      if (count > 10) break;
    }
    while (clientMessage.available())
    {
      thisByte = clientMessage.read();
      JOURNAL("%c", thisByte);
    }
    JOURNAL("disconnected\n");
    clientMessage.stop();
    return false;
  }
  return true;
}

// Установить уведомление (сформировать для отправки но НЕ ОТПРАВЛЯТЬ)
// Проверяется необходимость отправки уведомления в зависимости от установленных флагов
// true - сообщение принято (или запрещено), false - сообщение отвергнуто т.к оно уже посылалось (дубль) или внутренняя ошибка
boolean Message::setMessage(MESSAGE ms, char *c, int p1) // может запускаться из любого потока!!
{
  // Проверка на необходимость посылки сообщения
  if (!(((GETBIT(messageSetting.flags, fMail)) || (GETBIT(messageSetting.flags, fSMS))) && ((messageData.ms != pMESSAGE_TESTMAIL) || (messageData.ms != pMESSAGE_TESTSMS)))) return true;	// посылать ненадо
  //  SerialDbg.print(c);SerialDbg.print(" : ");SerialDbg.print(ms);SerialDbg.println("-5");
  // Проверка необходимости отправки уведомления
  if (((GETBIT(messageSetting.flags, fMessageReset)) == 0) && (ms == pMESSAGE_RESET))       return true; // Попытка отправить не разрешенное сообщение, выходим без ошибок
  if (((GETBIT(messageSetting.flags, fMessageError)) == 0) && (ms == pMESSAGE_ERROR))       return true;
  if (((GETBIT(messageSetting.flags, fMessageLife)) == 0) && (ms == pMESSAGE_LIFE))         return true;
  if (((GETBIT(messageSetting.flags, fMessageTemp)) == 0) && (ms == pMESSAGE_TEMP))         return true;
  if (((GETBIT(messageSetting.flags, fMessageSD)) == 0) && (ms == pMESSAGE_SD))             return true;
  if (((GETBIT(messageSetting.flags, fMessageWarning)) == 0) && (ms == pMESSAGE_WARNING))   return true;
  // else if (((messageSetting.GETBIT(fMessageTemp))&&(ms==pMESSAGE_TEMP))&&((sTemp[TIN].get_Temp()<messageSetting.mTIN)||(sTemp[TBOILER].get_Temp()<messageSetting.mTBOILER)||(sTemp[TCOMP].get_Temp()>messageSetting.mTCOMP)))  return true;  // выходим, температуры в границах!!

  // Проверка на дублирование сообщения. Тестовые сообщения и сообщения жизни  можно посылать многократно  подряд
  if ((rtcSAM3X8.unixtime() - sendTime < REPEAT_TIME) && (messageData.ms == ms) && ((ms != pMESSAGE_TESTMAIL) && (ms != pMESSAGE_TESTSMS) && (ms != pMESSAGE_LIFE))) //дублирующие сообщения полылаются с интервалом
  {
    //JOURNAL("Ignore repeat msg: #%d\n", ms);
    return false;
  } else {
    JOURNAL(pP_TIME, "MSG: #%d: %s\n", ms, c);
  }

  // Подготовить уведомление
  messageData.ms = ms;
  sendTime = rtcSAM3X8.unixtime(); // запомнить время отправки
  strcpy(messageData.data, c);
  // в сообщение pMESSAGE_TEMP добавить значение температуры
  if (ms == pMESSAGE_TEMP) {
    strcat(messageData.data, " t=");
    _dtoa(messageData.data, p1, 2);
  }
  messageData.p1 = p1;
  waitSend = true;                // выставить флаг необходимости отправки Уведомления
  return true;
}

// Установить (сформировать) тестовое письмо, отправка sendMessage();
boolean Message::setTestMail() // может запускаться из любого потока!!
{
  return setMessage(pMESSAGE_TESTMAIL, (char*)"Тестовое уведомление, для проверки почты", 0);
}

//Установить (сформировать) тестовое СМС, отправка sendMessage();
boolean Message::setTestSMS() // может запускаться из любого потока!!
{
  return setMessage(pMESSAGE_TESTSMS, (char*)"Тестовое уведомление, для проверки SMS", 0);
}

// Послать уведомление согласно выбранных настроек cформированное setMessage
// проверяется наличие неоправленныхуведомлений
// true - уведомление отправлено или его не было false - при отправке произошли ошибки
boolean Message::sendMessage()  // запуск из 0 потока
{
  uint16_t i;

  if (!waitSend) return true;                            // Отправлять нечего выходим
  strcpy(retTest, "Ничего не отправлено. Проверьте флаг разрешения отправки сообщений.");
  if (HP.get_uptime() < cDELAY_START_MESSAGE) {
    // ждать не будем, ибо зачем? _delay(200);  // Прошло мало времени после старта возможно инет еще не поднят
    return true;
  }
  clearBuf(); // очистка рабочих буферов
  // Отправка Уведомления. Все таки отправлять придется -))
  if ((GETBIT(messageSetting.flags, fMail)) && (messageData.ms != pMESSAGE_TESTSMS)) // Разрешены уведомления по почте и не тест SMS
  {
    if (sendMail()) // Формирование ответа
    {
      // Отправка удачна
      for (i = 0; i < strlen(retMail); i++) if (retMail[i] == '=') retMail[i] = ':'; // замена знака = на : т.к. это запрещенный знак в запросах
      strcpy(retTest, "Тестовое письмо отправлено на "); get_messageSetting((char*)mess_SMTP_RCPTTO, retTest); //strcat(retTest,HP.message.get_messageSetting(pSMTP_RCPTTO));
      strcat(retTest, "\nОтвет: "); strcat(retTest, retMail);
    }
    else
    {
      // Отправка не удачна
      for (i = 0; i < strlen(retMail); i++) if (retMail[i] == '=') retMail[i] = ':'; // замена знака = на : т.к. это запрещенный знак в запросах
      strcpy(retTest, "Тестовое письмо НЕ отправлено на "); get_messageSetting((char*)mess_SMTP_RCPTTO, retTest); //strcat(retTest,HP.message.get_messageSetting(pSMTP_RCPTTO));
      strcat(retTest, "\nОтвет: "); strcat(retTest, retMail);
    }
  }

  if ((!(GETBIT(messageSetting.flags, fMail))) && (messageData.ms == pMESSAGE_TESTMAIL)) // Почта не разрешена а отправляется тестовое письмо
  {
    strcpy(retTest, "Письмо не отправлено. Проверьте флаг разрешения отправки почты.");
  }

  if ((GETBIT(messageSetting.flags, fSMS)) && (messageData.ms != pMESSAGE_TESTMAIL)) // Разрешены уведомления по SMS и не тест mail
  {
    switch (messageSetting.sms_service)
    {
      case pSMS_RU:
        if (sendSMS())
        { // Удачно
          strcpy(retTest, "Тестовое SMS отправлено на номер "); get_messageSetting((char*)mess_SMS_PHONE, retTest); //strcat(retTest,HP.message.get_messageSetting(pSMS_PHONE));
          strcat(retTest, "\nОтвет: "); strcat(retTest, retSMS);
        }
        else
        { // Не удачно
          strcpy(retTest, "Тестовое SMS НЕ отправлено на номер "); get_messageSetting((char*)mess_SMS_PHONE, retTest); //strcat(retTest,HP.message.get_messageSetting(pSMS_PHONE));
          strcat(retTest, "\nОтвет: "); strcat(retTest, retSMS);
        }
        break;

      case pSMSC_RU:
        if (sendSMSC())
        { // Удачно
          strcpy(retTest, "Тестовое SMS отправлено на номер "); get_messageSetting((char*)mess_SMS_PHONE, retTest); //strcat(retTest,HP.message.get_messageSetting(pSMS_PHONE));
          strcat(retTest, "\nОтвет: "); strcat(retTest, retSMS);
        }
        else
        { // Не удачно
          strcpy(retTest, "Тестовое SMS НЕ отправлено на номер "); get_messageSetting((char*)mess_SMS_PHONE, retTest); //strcat(retTest,HP.message.get_messageSetting(pSMS_PHONE));
          strcat(retTest, "\nОтвет: "); strcat(retTest, retSMS);
        }
        break;

      case pSMSC_UA:
        if (sendSMSC())
        { // Удачно
          strcpy(retTest, "Тестовое SMS отправлено на номер "); get_messageSetting((char*)mess_SMS_PHONE, retTest); //strcat(retTest,HP.message.get_messageSetting(pSMS_PHONE));
          strcat(retTest, "\nОтвет: "); strcat(retTest, retSMS);
        }
        else
        { // Не удачно
          strcpy(retTest, "Тестовое SMS НЕ отправлено на номер "); get_messageSetting((char*)mess_SMS_PHONE, retTest); //strcat(retTest,HP.message.get_messageSetting(pSMS_PHONE));
          strcat(retTest, "\nОтвет: "); strcat(retTest, retSMS);
        }
        break;

      case pSMSCLUB_UA:
        if (sendSMSCLUB())
        { // Удачно
          strcpy(retTest, "Тестовое SMS отправлено на номер "); get_messageSetting((char*)mess_SMS_PHONE, retTest); //strcat(retTest,HP.message.get_messageSetting(pSMS_PHONE));
          strcat(retTest, "\nОтвет: "); strcat(retTest, retSMS);
        }
        else
        { // Не удачно
          strcpy(retTest, "Тестовое SMS НЕ отправлено на номер "); get_messageSetting((char*)mess_SMS_PHONE, retTest); //strcat(retTest,HP.message.get_messageSetting(pSMS_PHONE));
          strcat(retTest, "\nОтвет: "); strcat(retTest, retSMS);
        }
        break;

      default:  return false;
    } // ничего не делаем но это ошибка
  }


  if ((!(GETBIT(messageSetting.flags, fSMS))) && (messageData.ms == pMESSAGE_TESTSMS)) // SMS не разрешена а отправляется тестовое SMS
    strcpy(retTest, "SMS не отправлено. Проверьте флаг разрешения отправки SMS.");

  waitSend = false;        // Сбросить флаг необходимости отпвки уведомления
  return true;             // Послано
}

// Отправить почту --------------------------------------------------------------------
boolean Message::sendMail()
{
  strcpy(retMail, ""); // Обнулить ошибку
  // Подготовка
  if (SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE)  {
    return false; // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
  }

  // 1. Соединение по телнету
  if (clientMessage.connect(messageSetting.smtp_server, messageSetting.smtp_port, W5200_SOCK_SYS))
  {
    JOURNAL("Connected server: %s port: %d, sock: %d\n", messageSetting.smtp_server, messageSetting.smtp_port, W5200_SOCK_SYS);
  }
  else
  {
    JOURNAL("Connection failed server: %s port: %d, sock: %d\n", messageSetting.smtp_server, messageSetting.smtp_port, W5200_SOCK_SYS);
    strncpy(retMail, "No connect", LEN_RETMAIL);
    SemaphoreGive(xWebThreadSemaphore);
    return false;
  }

  // 2. Общение с сервером, получаем приветствие при соединении
  if (!SendCommandSMTP((char*)"", true))    {
    clientMessage.stop();  // ответ содержит ошибки
    SemaphoreGive(xWebThreadSemaphore);
    if (strlen(retMail) == 0) strncpy(retMail, (char*)"No answer", LEN_RETMAIL);
    return false;
  }
  // 3. Авторизация
  if (GETBIT(messageSetting.flags, fMailAUTH))                        // Требуется авторизация
  {
    if (!SendCommandSMTP((char*)"EHLO host", true))    {
      clientMessage.stop();  // ответ содержит ошибки
      SemaphoreGive(xWebThreadSemaphore);
      return false;
    }
    if (!SendCommandSMTP((char*)"AUTH LOGIN", true))   {
      clientMessage.stop();  // ответ содержит ошибки
      SemaphoreGive(xWebThreadSemaphore);
      return false;
    }
    strcpy(tempBuf, "");
    base64_encode(tempBuf, messageSetting.smtp_login, strlen(messageSetting.smtp_login)); // Послать логин
    if (!SendCommandSMTP(tempBuf, true))   {
      clientMessage.stop();  // ответ содержит ошибки
      SemaphoreGive(xWebThreadSemaphore);
      return false;
    }
    strcpy(tempBuf, "");
    base64_encode(tempBuf, messageSetting.smtp_password, strlen(messageSetting.smtp_password)); // Послать пароль
    if (!SendCommandSMTP(tempBuf, true))   {
      clientMessage.stop();  // ответ содержит ошибки
      SemaphoreGive(xWebThreadSemaphore);
      return false;
    }
  }
  else                                                               // Авторизация не требуется
  {
    if (!SendCommandSMTP((char*)"HELO host", true))    {
      clientMessage.stop();  // ответ содержит ошибки
      SemaphoreGive(xWebThreadSemaphore);
      return false;
    }
  }

  // 4. Формирование адресов
  strcpy(tempBuf, "MAIL From: <"); strcat(tempBuf, messageSetting.smtp_MailTo);  strcat(tempBuf, ">");
  if (!SendCommandSMTP(tempBuf, true))    {
    clientMessage.stop();  // ответ содержит ошибки
    SemaphoreGive(xWebThreadSemaphore);
    return false;
  }
  strcpy(tempBuf, "RCPT To: <");   strcat(tempBuf, messageSetting.smtp_RCPTTo);  strcat(tempBuf, ">");
  if (!SendCommandSMTP(tempBuf, true))    {
    clientMessage.stop();  // ответ содержит ошибки
    SemaphoreGive(xWebThreadSemaphore);
    return false;
  }

  // 5. Заголовок сообщения
  if (!SendCommandSMTP((char*)"DATA", true))   {
    clientMessage.stop();  // ответ содержит ошибки
    SemaphoreGive(xWebThreadSemaphore);
    return false;
  }
  strcpy(tempBuf, "To: <");      strcat(tempBuf, messageSetting.smtp_RCPTTo); strcat(tempBuf, ">");
  if (!SendCommandSMTP(tempBuf, false))   {
    clientMessage.stop();  // ответ содержит ошибки
    SemaphoreGive(xWebThreadSemaphore);
    return false;
  }
  strcpy(tempBuf, "From: <");    strcat(tempBuf, messageSetting.smtp_MailTo); strcat(tempBuf, ">");
  if (!SendCommandSMTP(tempBuf, false))    {
    clientMessage.stop();  // ответ содержит ошибки
    SemaphoreGive(xWebThreadSemaphore);
    return false;
  }
  strcpy(tempBuf, "Content-type: text/plain; charset=\"utf-8\"");             // Кодировка
  if (!SendCommandSMTP(tempBuf, false))    {
    clientMessage.stop();  // ответ содержит ошибки
    SemaphoreGive(xWebThreadSemaphore);
    return false;
  }
  strcpy(tempBuf, "Subject: "); // Тема письма
  strcat(tempBuf, "Controller ");
  strcat(tempBuf, (char*)nameHeatPump);
  switch ((int)messageData.ms)   // Заголовок уведомления - добавляем тип уведомления
  {
    case pMESSAGE_NONE    : break;                                                                                          // Нет уведомлений
    case pMESSAGE_TESTMAIL: strcat(tempBuf, "TESTMAIL\r\n"); break;                                                         // Тестовое почта
    case pMESSAGE_TESTSMS : strcat(tempBuf, "TESTSMS\r\n");  break;                                                         // Тестовое смс
    case pMESSAGE_RESET   : strcat(tempBuf, "RESET\r\n"); break;                                                            // Уведомление Сброс
    case pMESSAGE_ERROR   : strcat(tempBuf, "ERROR\r\n"); break;                                                            // Уведомление Ошибка
    case pMESSAGE_LIFE    : strcat(tempBuf, "LIFE\r\n"); break;                                                             // Уведомление Сигнал жизни
    case pMESSAGE_TEMP    : strcat(tempBuf, "TEMP\r\n"); break;                                                             // Уведомление Достижение граничной температуры
    case pMESSAGE_SD      : strcat(tempBuf, "ERROR SD\r\n"); break;                                                         // Уведомление "Проблемы с sd картой"
    case pMESSAGE_WARNING : strcat(tempBuf, "WARNING\r\n"); break;                                                          // Уведомление "Прочие уведомления"
    default               : strcat(tempBuf, "ERROR TYPE \r\n"); clientMessage.stop(); SemaphoreGive(xWebThreadSemaphore); return false; break; // ответ содержит ошибки
  }
  if (!SendCommandSMTP(tempBuf, false))    {
    clientMessage.stop();  // послать тему письма
    SemaphoreGive(xWebThreadSemaphore);
    return false;
  }

  // 6. Текст сообщения
  strcpy(tempBuf, " ------ Народный контроллер теплового насоса ver. "); strcat(tempBuf, VERSION); strcat(tempBuf, " ------");
  strcat(tempBuf, cStrEnd);  clientMessage.write(tempBuf, strlen(tempBuf));


  strcpy(tempBuf, "Конфигурация: "); strcat(tempBuf, CONFIG_NAME); strcat(tempBuf, " ("); strcat(tempBuf, CONFIG_NOTE ); strcat(tempBuf, ")");
  strcat(tempBuf, cStrEnd);  clientMessage.write(tempBuf, strlen(tempBuf));


  strcpy(tempBuf, "Создание уведомления: "); strcat(tempBuf, NowDateToStr()); strcat(tempBuf, " "); strcat(tempBuf, NowTimeToStr());
  strcat(tempBuf, cStrEnd);  clientMessage.write(tempBuf, strlen(tempBuf));

  strcat(messageData.data, cStrEnd);  clientMessage.write(messageData.data, strlen(messageData.data));


  // 7. Дополнительная информация если требуется добавляется в уведомление
  if (GETBIT(messageSetting.flags, fMailInfo))  get_mailState(clientMessage, tempBuf);


  // 8. Завершение сессии  и отправка
  if (!SendCommandSMTP((char*)".", true))      {
    clientMessage.stop();  // ответ содержит ошибки
    SemaphoreGive(xWebThreadSemaphore);
    return false;
  }
  strncpy(retMail, (char*)tempBuf, LEN_RETMAIL);                                                                  // Копирование сообщения сервера при удачной отправки
  if (!SendCommandSMTP((char*)"QUIT", true))   {
    clientMessage.stop();  // ответ содержит ошибки
    SemaphoreGive(xWebThreadSemaphore);
    return false;
  }
  clientMessage.stop();
  JOURNAL("OK disconnected\n");
  SemaphoreGive(xWebThreadSemaphore);
  return true;
}

// Отправить SMS sms.ru ----------------------------------------------------------------------------------
// true - была отправка смс false - не было отправки смс
boolean Message::sendSMS()
{
  uint16_t i;
  uint8_t count = 0;
  IPAddress zeroIP(0, 0, 0, 0);

  if (SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE)  {
    return false; // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
  }

  strcpy(retSMS, ADR_SMS_RU); // Используется как временный буфер check_address с констанатми не работает??

  if (messageSetting.sms_serviceIP == zeroIP) // возможно днс надо всегда обновлять
  {
    //обновляем IP адрес сервера
    if (!check_address(retSMS, messageSetting.sms_serviceIP)) {
      strcpy(retSMS, ADR_SMS_RU);  // Обновить адрес
      strcat(retSMS, " DNS lookup failed!");
      return false;
    }
  }

  strcpy(retSMS, "");         // Обнулить ответ
  if (clientMessage.connect(messageSetting.sms_serviceIP, 80, W5200_SOCK_SYS)) // Соединение по HTTP
  {
    JOURNAL("Connected server: %s", ADR_SMS_RU); JOURNAL(" port: %d\n", 80);
  }
  else
  {
    JOURNAL("Connection failed server: %s", ADR_SMS_RU); JOURNAL(" port: %d\n", 80);
    strcpy(retSMS, "No connect "); strcat(retSMS, ADR_SMS_RU);
    clientMessage.stop();
    SemaphoreGive(xWebThreadSemaphore);
    return false;
  }

  // посылка смс
  strcpy(tempBuf, "GET http://sms.ru/sms/send?api_id=");                    // Начало
  strcat(tempBuf, messageSetting.sms_p1);                                   // id
  strcat(tempBuf, "&to=");
  strcat(tempBuf, messageSetting.sms_phone);                                // номер
  strcat(tempBuf, "&text=");
  strcat(tempBuf, "Control+");
  switch ((int)messageData.ms)   // Заголовок уведомления - добавляем тип уведомления
  {
    case pMESSAGE_NONE    : break;                                                                                       // Нет уведомлений
    case pMESSAGE_TESTMAIL: strcat(tempBuf, "TESTMAIL+"); break;                                                         // Тестовое почта
    case pMESSAGE_TESTSMS : strcat(tempBuf, "TESTSMS+");  break;                                                         // Тестовое смс
    case pMESSAGE_RESET   : strcat(tempBuf, "RESET+"); break;                                                            // Уведомление Сброс
    case pMESSAGE_ERROR   : strcat(tempBuf, "ERROR+"); break;                                                            // Уведомление Ошибка
    case pMESSAGE_LIFE    : strcat(tempBuf, "LIFE+"); break;                                                             // Уведомление Сигнал жизни
    case pMESSAGE_TEMP    : strcat(tempBuf, "TEMP+"); break;                                                             // Уведомление Достижение граничной температуры
    case pMESSAGE_SD      : strcat(tempBuf, "ERROR+SD+"); break;                                                         // Уведомление "Проблемы с sd картой"
    case pMESSAGE_WARNING : strcat(tempBuf, "WARNING+"); break;                                                          // Уведомление "Прочие уведомления"
    default               : strcat(tempBuf, "ERROR+TYPE+"); SemaphoreGive(xWebThreadSemaphore); return false; break;    // ответ содержит ошибки
  }
  for (i = 0; i < strlen(messageData.data); i++) if (messageData.data[i] == ' ') messageData.data[i] = '+'; // замена пробела на знак "+" т.к. это запрещенный знак при отправке сообщения
  strcat(tempBuf, messageData.data);                       // содержимое
  strcat(tempBuf, cStrEnd);  clientMessage.write(tempBuf, strlen(tempBuf));

  //JOURNAL("%s\n",tempBuf);

  // Ожидание и получение ответа
  while (!clientMessage.available()) // ожидание ответа 5 сек
  {
    SemaphoreGive(xWebThreadSemaphore);
    _delay(200);
    if (SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE)  {
      return false; // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
    }
    count++;
    if (count > 25)
    {
      strcpy(retSMS, "Server not answer . . .");
      JOURNAL("%s\n", retSMS);
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
    if (thisByte == 0x0a)
    {
      thisByte = clientMessage.read();
      if (thisByte == 0x0d)
      {
        thisByte = clientMessage.read();
        if (thisByte == 0x0a)
        {
          i = clientMessage.read((byte*)retSMS, LEN_RETSMS/* sizeof(retSMS)*/ - 1); // получаем код ответа
          retSMS[i] = 0;                                         // обрезаем строку
          for (i = 0; i < strlen(retSMS); i++) if (retSMS[i] == '=') retSMS[i] = ':'; // замена = на знак ":" веб морда глючит в ответах - обрезает по  =
          JOURNAL("sms.ru return: %s \n", retSMS);
          clientMessage.stop();
          SemaphoreGive(xWebThreadSemaphore);
          return true; // смс отправлено
        }
      }
    }

  }
  clientMessage.stop();
  SemaphoreGive(xWebThreadSemaphore);
  return false;
}
//Отправить SMS smsc.ru, smsc.ua------------------------------------------------------------

boolean Message::sendSMSC()
{
  uint16_t i;
  uint8_t count = 0;
  IPAddress zeroIP(0, 0, 0, 0);

  if (SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE)  {
    return false; // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
  }

  switch (messageSetting.sms_service)
  {
    case pSMSC_RU:
      strcpy(retSMS, ADR_SMSC_RU); // Используется как временный буфер check_address с констанатми не работает??
      break;

    case pSMSC_UA:
      strcpy(retSMS, ADR_SMSC_UA); // Используется как временный буфер check_address с констанатми не работает??
      break;
    default: strcpy(retSMS, ADR_SMSC_RU); break; // Используется как временный буфер check_address с констанатми не работает??
  }
  if (messageSetting.sms_serviceIP == zeroIP) // возможно днс надо всегда обновлять
  {
    //обновляем IP адрес сервера
    if (!check_address(retSMS, messageSetting.sms_serviceIP)) {
      strcat(retSMS, " DNS lookup failed!");  // Обновить адрес
      return false;
    }
  }

  strcpy(retSMS, "");         // Обнулить ответ
  if (clientMessage.connect(messageSetting.sms_serviceIP, 80, W5200_SOCK_SYS)) // Соединение по HTTP
  {
    if ((messageSetting.sms_service) == pSMSC_RU) {
      JOURNAL("Connected server: %s", ADR_SMSC_RU);
      JOURNAL(" port: %d\n", 80);
    }
    else {
      JOURNAL("Connected server: %s", ADR_SMSC_UA);
      JOURNAL(" port: %d\n", 80);
    }
  }
  else
  {
    if ((messageSetting.sms_service) == pSMSC_RU)
    {
      JOURNAL("Connection failed server: %s", ADR_SMSC_RU); JOURNAL(" port: %d\n", 80);
      strcpy(retSMS, "No connect "); strcat(retSMS, ADR_SMSC_RU);
      clientMessage.stop();
      SemaphoreGive(xWebThreadSemaphore);
      return false;
    } else
    {
      JOURNAL("Connection failed server: %s", ADR_SMSC_UA); JOURNAL(" port: %d\n", 80);
      strcpy(retSMS, "No connect "); strcat(retSMS, ADR_SMSC_UA);
      clientMessage.stop();
      SemaphoreGive(xWebThreadSemaphore);
      return false;
    }
  }

  // посылка смс
  if ((messageSetting.sms_service) == pSMSC_RU) strcpy(tempBuf, "GET http://smsc.ru/sys/send.php?login=");         // Начало
  if ((messageSetting.sms_service) == pSMSC_UA) strcpy(tempBuf, "GET http://smsc.ua/sys/send.php?login=");         // Начало
  strcat(tempBuf, messageSetting.sms_p1);                                   // login
  strcat(tempBuf, messageSetting.sms_p2);                                   // password
  strcat(tempBuf, "&phones=");
  strcat(tempBuf, messageSetting.sms_phone);                                // номер
  strcat(tempBuf, "&mes=");
  strcat(tempBuf, "Control+");
  switch ((int)messageData.ms)   // Заголовок уведомления - добавляем тип уведомления
  {
    case pMESSAGE_NONE    : break;                                                                                       // Нет уведомлений
    case pMESSAGE_TESTMAIL: strcat(tempBuf, "TESTMAIL+"); break;                                                         // Тестовое почта
    case pMESSAGE_TESTSMS : strcat(tempBuf, "TESTSMS+");  break;                                                         // Тестовое смс
    case pMESSAGE_RESET   : strcat(tempBuf, "RESET+"); break;                                                            // Уведомление Сброс
    case pMESSAGE_ERROR   : strcat(tempBuf, "ERROR+"); break;                                                            // Уведомление Ошибка
    case pMESSAGE_LIFE    : strcat(tempBuf, "LIFE+"); break;                                                             // Уведомление Сигнал жизни
    case pMESSAGE_TEMP    : strcat(tempBuf, "TEMP+"); break;                                                             // Уведомление Достижение граничной температуры
    case pMESSAGE_SD      : strcat(tempBuf, "ERROR+SD+"); break;                                                         // Уведомление "Проблемы с sd картой"
    case pMESSAGE_WARNING : strcat(tempBuf, "WARNING+"); break;                                                          // Уведомление "Прочие уведомления"
    default               : strcat(tempBuf, "ERROR+TYPE+"); SemaphoreGive(xWebThreadSemaphore); return false; break;    // ответ содержит ошибки
  }
  for (i = 0; i < strlen(messageData.data); i++) if (messageData.data[i] == ' ') messageData.data[i] = '+'; // замена пробела на знак "+" т.к. это запрещенный знак при отправке сообщения
  strcat(tempBuf, messageData.data);                       // содержимое
  strcat(tempBuf, cStrEnd);  clientMessage.write(tempBuf, strlen(tempBuf));

  //JOURNAL("%s\n",tempBuf);

  // Ожидание и получение ответа
  while (!clientMessage.available()) // ожидание ответа 5 сек
  {
    SemaphoreGive(xWebThreadSemaphore);
    _delay(200);
    if (SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE)  {
      return false; // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
    }
    count++;
    if (count > 25)
    {
      strcpy(retSMS, "Server not answer . . .");
      JOURNAL("%s\n", retSMS);
      clientMessage.stop();
      SemaphoreGive(xWebThreadSemaphore);
      return false;
    }

  }

  // разбор ответа
  //byte thisByte;

  while (clientMessage.available())
  {
    i = clientMessage.read((byte*)retSMS, LEN_RETSMS/* sizeof(retSMS)*/ - 1); // получаем код ответа
    retSMS[i] = 0;                                         // обрезаем строку
    for (i = 0; i < strlen(retSMS); i++) if (retSMS[i] == '=') retSMS[i] = ':'; // замена = на знак ":" веб морда глючит в ответах - обрезает по  =
    JOURNAL("server return: %s \n", retSMS);
    clientMessage.stop();
    SemaphoreGive(xWebThreadSemaphore);
    if (strstr(retSMS, "ERROR"))  return false; else return true;
  }

  clientMessage.stop();
  SemaphoreGive(xWebThreadSemaphore);
  return false;
}
//---------------------------------------------------------------------------------
//Отправить SMS smsclub.mobi------------------------------------------------------------

boolean Message::sendSMSCLUB()
{
  uint16_t headerLength;
  char tmp_buf [10];
  uint16_t i;
  uint8_t count = 0;
  IPAddress zeroIP(0, 0, 0, 0);

  if (SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE)  {
    return false; // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
  }

  strcpy(retSMS, ADR_SMSC_RU); // Используется как временный буфер check_address с констанатми не работает??

  if (messageSetting.sms_serviceIP == zeroIP) // возможно днс надо всегда обновлять
  {
    //обновляем IP адрес сервера
    if (!check_address(retSMS, messageSetting.sms_serviceIP)) {
      strcat(retSMS, " DNS lookup failed!");  // Обновить адрес
      return false;
    }
  }

  strcpy(retSMS, "");         // Обнулить ответ
  if (clientMessage.connect(messageSetting.sms_serviceIP, 80, W5200_SOCK_SYS)) // Соединение по HTTP
  {
    JOURNAL("Connected server: %s", ADR_SMSCLUB_UA); JOURNAL(" port: %d\n", 80);
  }
  else
  {
    JOURNAL("Connection failed server: %s", ADR_SMSCLUB_UA); JOURNAL(" port: %d\n", 80);
    strcpy(retSMS, "No connect "); strcat(retSMS, ADR_SMSCLUB_UA);
    clientMessage.stop();
    SemaphoreGive(xWebThreadSemaphore);
    return false;

  }
  // посылка смс
  strcpy(tempBuf, "POST /xml/ HTTP/1.1\r\nHost: gate.smsclub.mobi\r\nUser-Agent: HK 1.017\r\nConnection: close\r\nContent-Type: application/xml\r\nContent-Length:    ");      // заголовок POST запроса
  strcat(tempBuf, "\r\n\r\n");
  headerLength = strlen(tempBuf);			// длина заголовка
  strcat(tempBuf, "<?xml version='1.0' encoding='utf-8'?>\r\n<request_sendsms>\r\n<username><![CDATA[");
  strcat(tempBuf, messageSetting.sms_p1);
  strcat(tempBuf, "]]></username>\r\n<password><![CDATA[");
  strcat(tempBuf, messageSetting.sms_p2);
  strcat(tempBuf, "]]></password>\r\n<from><![CDATA[infomir]]></from>\r\n<to><![CDATA[");
  strcat(tempBuf, messageSetting.sms_phone);
  strcat(tempBuf, "]]></to>\r\n<text><![CDATA[Control+");

  switch ((int)messageData.ms)   // Заголовок уведомления - добавляем тип уведомления
  {
    case pMESSAGE_NONE    : break;                                                                                       // Нет уведомлений
    case pMESSAGE_TESTMAIL: strcat(tempBuf, "TESTMAIL+"); break;                                                         // Тестовое почта
    case pMESSAGE_TESTSMS : strcat(tempBuf, "TESTSMS+");  break;                                                         // Тестовое смс
    case pMESSAGE_RESET   : strcat(tempBuf, "RESET+"); break;                                                            // Уведомление Сброс
    case pMESSAGE_ERROR   : strcat(tempBuf, "ERROR+"); break;                                                            // Уведомление Ошибка
    case pMESSAGE_LIFE    : strcat(tempBuf, "LIFE+"); break;                                                             // Уведомление Сигнал жизни
    case pMESSAGE_TEMP    : strcat(tempBuf, "TEMP+"); break;                                                             // Уведомление Достижение граничной температуры
    case pMESSAGE_SD      : strcat(tempBuf, "ERROR+SD+"); break;                                                         // Уведомление "Проблемы с sd картой"
    case pMESSAGE_WARNING : strcat(tempBuf, "WARNING+"); break;                                                          // Уведомление "Прочие уведомления"
    default               : strcat(tempBuf, "ERROR+TYPE+"); SemaphoreGive(xWebThreadSemaphore); return false; break;    // ответ содержит ошибки
  }
  strcat(tempBuf, messageData.data);										// содержимое
  strcat(tempBuf, "]]></text>\r\n</request_sendsms>\r\n");
  itoa((strlen(tempBuf) - headerLength), tmp_buf, 10);							//длина контента
  tempBuf[headerLength - 7] = tmp_buf[0];
  tempBuf[headerLength - 6] = tmp_buf[1];
  tempBuf[headerLength - 5] = tmp_buf[2];
  clientMessage.write(tempBuf, strlen(tempBuf));
  //SerialDbg.println(tempBuf);
  //JOURNAL("%s\n",tempBuf);

  // Ожидание и получение ответа
  while (!clientMessage.available()) // ожидание ответа 5 сек
  {
    SemaphoreGive(xWebThreadSemaphore);
    _delay(200);
    if (SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE)  {
      return false; // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
    }
    count++;
    if (count > 25)
    {
      strcpy(retSMS, "Server not answer . . .");
      JOURNAL("%s\n", retSMS);
      clientMessage.stop();
      SemaphoreGive(xWebThreadSemaphore);
      return false;
    }

  }

  // разбор ответа
  byte thisByte;
  while (clientMessage.available())
  {
    thisByte = clientMessage.read();
    if (thisByte == 0x3c)
    {
      thisByte = clientMessage.read();
      if (thisByte == 0x73)
      {
        thisByte = clientMessage.read();
        if (thisByte == 0x74)
        {
          thisByte = clientMessage.read();
          if (thisByte == 0x61)
          {
            i = clientMessage.read((byte*)retSMS, LEN_RETSMS/* sizeof(retSMS)*/ - 1); // получаем код ответа
            retSMS[i] = 0;                                         // обрезаем строку
            for (i = 0; i < strlen(retSMS); i++) if (retSMS[i] == '=') retSMS[i] = ':'; // замена = на знак ":" веб морда глючит в ответах - обрезает по  =
            JOURNAL("server return: %s \n", retSMS);
            clientMessage.stop();
            SemaphoreGive(xWebThreadSemaphore);
            if (strstr(retSMS, ">OK<"))  return true; else return false;
          }
        }
      }
    }
  }
  clientMessage.stop();
  SemaphoreGive(xWebThreadSemaphore);
  return false;
}
//---------------------------------------------------------------------------------




// Отправить SMS smsc.ru ----------------------------------------------------------
/*
  boolean Message::sendSMSC()
  {
  EthernetClient client;
  client.stop();
  _delay(100);
  // 1. Соединение по телнету
  if (client.connect("http://sms.ru/sms/send",80))
  //     if (client.connect(smtp,messageSetting.smtp_port))
  {
   JOURNAL("Connected server: http://sms.ru/sms/send port:80\n");
  }
  else
  {
   JOURNAL("Connection failed server: http://sms.ru/sms/send port:80\n");
   return false;
  }
  client.println("GET api_id=[]&to=[номер получателя]&text=hello+world HTTP/1.1");
  client.println("Host: sms.ru");
  client.println("Connection: close");
  client.println();

  return true;

  }
*/
