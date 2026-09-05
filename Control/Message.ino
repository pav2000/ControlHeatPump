/*
 * Copyright (c) 2016-2023 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
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
extern void get_mailState(char *buf, uint16_t buf_size);
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
  mailBuf=Socket[web_task].outBuf+MAILBUF_OFFSET;
  mailBuf[0]=0x00;
  txBuf=Socket[web_task].outBuf+MAILBUF_OFFSET+LEN_MAILBUF;
  txLen=0;
  smtpCaps=0;
  smtpCode=0;
  smtpSemLost=false;
  sendRetry=0;
  sendRetryTime=0;
  smsSent=false;
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
  WorkFlags = 0;
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
		case pSMSCLUB:
			ret = strcpy(tempBuf, ADR_SMSCLUB);
			break;
		}
		check_address(tempBuf, messageSetting.sms_serviceIP);// При не удаче возвращается 0, при удаче: 1 - IP на входе (были цифры, DNS не нужен), 2 - был запрос к DNS и адрес получен
		dnsUpadateSMS = false;
		if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) SemaphoreGive(xWebThreadSemaphore);
		ret = false;  // вызов обновления был
	}
	return ret;
}

// Есть ли в строке управляющие символы.
// Веб-сервер вызывает urldecode() ДО разбора команд, поэтому "%0D%0A" в запросе превращается
// в настоящие CR/LF. Попав в адрес, они разорвут команду SMTP надвое ("RCPT TO:<a>" + чужая
// команда), а в заголовках письма пустая строка оборвет их досрочно и остаток уйдет в тело.
static boolean mail_has_ctrl(const char *s)
{
	for(; *s; s++) if((uint8_t)*s < 0x20 || (uint8_t)*s == 0x7F) return true;
	return false;
}

// Установить параметр Уведомления из строки
boolean Message::set_messageSetting(char *var, char *c)
{
  float x;
  if(mail_has_ctrl(c)) return false;   // ни в одной настройке уведомлений управляющих символов быть не может
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
		if(waitSend) return strcat(ret, (char*) "wait response");                // В зависимости готов ответ или нет
		else return strcat(ret, retTest);
	} else if(strcmp(var, mess_SMS_RET) == 0) {
		if(waitSend) return strcat(ret, (char*) "wait response");                // В зависимости готов ответ или нет
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

// ------------------------- ТРАНСПОРТ SMTP -------------------------------------------------
// Все команды и все строки письма завершаются SMTP_CRLF. Общая константа проекта cStrEnd ("\n")
// здесь неприменима: RFC 5321 требует именно CR+LF, а после закрытия CVE-2023-51764 (SMTP smuggling)
// современные серверы на одиночном переводе строки не распознают конец письма и рвут соединение.

// Скопировать ответ сервера в retMail с гарантированным завершающим нулем
void Message::setRetMail(const char *s)
{
	strncpy(retMail, s, LEN_RETMAIL - 1);
	retMail[LEN_RETMAIL - 1] = '\0';   // strncpy не терминирует строку, если источник не короче лимита
}

// Собрать ответ на тестовое уведомление: <prefix><значение параметра var>\nОтвет: <answer>
// Сборка идет с контролем границ: раньше цепочка strcpy/strcat при длинном ответе сервера
// вылезала за retTest[LEN_RETTEST] и затирала соседние поля класса.
void Message::setRetTest(const char *prefix, const char *var, const char *answer)
{
	uint16_t n;
	m_snprintf(retTest, sizeof(retTest), "%s", prefix);
	get_messageSetting((char*) var, retTest);   // адрес получателя / номер телефона ДОБАВЛЯЕТСЯ в строку
	retTest[sizeof(retTest) - 1] = '\0';
	n = strlen(retTest);
	m_snprintf(retTest + n, sizeof(retTest) - n, "\nОтвет: %s", answer);
}

// Сбросить накопленный исходящий поток в сокет.
// Письмо копится в txBuf и уходит крупными кусками: EthernetClient::write() сразу выполняет
// Sock_SEND с активным ожиданием SEND_OK, поэтому построчная отправка - это десятки блокирующих
// операций и десятки TCP-пакетов на одно письмо.
// Пишем кусками не больше SMTP_MAX_SEND: send() в библиотеке обрезает запись по W5100.SSIZE (2048)
// и молча теряет хвост, а EthernetClient::write() все равно возвращает полный размер.
void Message::smtpFlush()
{
	uint16_t pos = 0, chunk;
	while(pos < txLen) {
		chunk = txLen - pos;
		if(chunk > SMTP_MAX_SEND) chunk = SMTP_MAX_SEND;
		if(clientMessage.write((const uint8_t*) txBuf + pos, chunk) == 0) break;  // соединение потеряно
		pos += chunk;
	}
	txLen = 0;
}

// Вывести одну строку тела письма. ЕДИНСТВЕННАЯ точка вывода тела, отсюда все гарантии:
//  - удвоение ведущей точки (RFC 5321 4.5.2), иначе такая строка оборвет письмо;
//  - разрезание строк длиннее SMTP_MAX_LINE (предел RFC - 998 байт без CRLF), по границе utf-8;
//  - завершение ровно CR+LF, благодаря чему завершающая точка всегда дает CRLF.CRLF
void Message::smtpWriteBody(const char *line)
{
	if(line == NULL) line = "";
	smtpWriteBodyLen(line, strlen(line));
}

// То же, но длина задана явно: строка может быть куском большего буфера и не завершаться нулем.
// Входной буфер НЕ изменяется, поэтому допустимы и строковые константы.
void Message::smtpWriteBodyLen(const char *line, uint16_t len)
{
	uint16_t part;
	if(line == NULL) { line = ""; len = 0; }
	do {
		part = len;
		if(part > SMTP_MAX_LINE) {
			part = SMTP_MAX_LINE;
			while(part > 1 && (line[part] & 0xC0) == 0x80) part--;  // не рвать многобайтный символ utf-8
		}
		if(txLen + part + 3 > LEN_TXBUF) smtpFlush();              // +3: удвоенная точка и CRLF
		if(part && line[0] == '.') txBuf[txLen++] = '.';
		memcpy(txBuf + txLen, line, part); txLen += part;
		txBuf[txLen++] = '\r'; txBuf[txLen++] = '\n';
		if(txLen >= SMTP_FLUSH_AT) smtpFlush();
		line += part; len -= part;
	} while(len);
}

// Разобрать готовый многострочный блок (разделители "\n" или "\r\n") и вывести его построчно.
// Нужен для вывода get_Counts(), который формирует весь текст сразу в буфере.
void Message::smtpWriteBlock(const char *block)
{
	const char *p = block, *e;
	uint16_t len;
	while(*p) {
		e = strchr(p, '\n');
		if(e == NULL) { smtpWriteBodyLen(p, strlen(p)); break; }
		len = e - p;
		if(len && *(e - 1) == '\r') len--;      // отбросить CR перед LF
		smtpWriteBodyLen(p, len);
		p = e + 1;
	}
}

// Прочитать одну строку ответа сервера (до LF), хвостовой CR срезается.
// На время паузы семафор отдается, иначе на секундных таймаутах встанет весь веб-сервер.
// Опрос сначала частый (реле в локальной сети отвечает за единицы мсек), потом редкий -
// иначе каждый обмен стоил бы полного шага ожидания, а их за сеанс около десятка.
boolean Message::smtpReadLine(char *buf, uint16_t size, uint32_t timeout)
{
	uint16_t pos = 0, poll = 0;
	uint32_t startTick = GetTickCount();
	for(;;) {
		while(clientMessage.available()) {
			int c = clientMessage.read();
			if(c < 0) break;
			if(c == '\n') {
				if(pos && buf[pos - 1] == '\r') pos--;
				buf[pos] = '\0';
				return true;
			}
			if(pos < size - 1) buf[pos++] = (char) c;   // слишком длинную строку молча обрезаем
		}
		if(!clientMessage.connected()) {                // сервер закрыл сессию, данных больше не будет
			buf[pos] = '\0';
			return false;
		}
		if(GetTickCount() - startTick > timeout) {
			buf[pos] = '\0';
			return false;
		}
		SemaphoreGive(xWebThreadSemaphore);             // отдать семафор для обработки других задач
		_delay(poll < SMTP_POLL_FAST_N ? SMTP_POLL_FAST : SMTP_POLL_SLOW);
		if(poll < SMTP_POLL_FAST_N) poll++;
		if(SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE) {
			// Семафор обратно не получен. Он сейчас у другой задачи, поэтому НИ трогать сокет,
			// НИ отдавать семафор нельзя: он бинарный, и лишний Give освободил бы SPI из-под владельца.
			smtpSemLost = true;
			buf[pos] = '\0';
			return false;
		}
	}
}

// Прочитать ответ сервера ЦЕЛИКОМ, в том числе многострочный (RFC 5321 4.2.1):
// признак продолжения - дефис в 4-й позиции, у последней строки там пробел.
// Без этого остаток ответа (например, на EHLO) достался бы следующей команде и диалог разъехался бы.
// Код ответа кладется в smtpCode, первая строка - в retMail.
// Попутно из ответа на EHLO взводятся биты возможностей сервера в smtpCaps.
boolean Message::smtpReadReply(uint32_t timeout)
{
	boolean first = true;
	smtpCode = 0;
	for(;;) {
		if(!smtpReadLine(tempBuf, LEN_TEMPBUF, timeout)) {
			if(first) setRetMail("No answer from server");
			return false;
		}
		JOURNAL("< %s\n", tempBuf);
		if(strlen(tempBuf) < 3 || tempBuf[0] < '2' || tempBuf[0] > '5'
		   || tempBuf[1] < '0' || tempBuf[1] > '9' || tempBuf[2] < '0' || tempBuf[2] > '9') {
			setRetMail("Bad reply format");
			return false;
		}
		smtpCode = (tempBuf[0] - '0') * 100 + (tempBuf[1] - '0') * 10 + (tempBuf[2] - '0');
		if(first) { setRetMail(tempBuf); first = false; }
		else if(strlen(tempBuf) > 4) {
			// Пояснение к отказу серверы часто дают во второй строке - дописываем, пока влезает.
			// Строка короче 5 символов содержательного текста после кода не несет.
			uint16_t rl = strlen(retMail);
			if(rl < LEN_RETMAIL - 2) {
				retMail[rl++] = ' ';
				strncpy(retMail + rl, tempBuf + 4, LEN_RETMAIL - 1 - rl);
				retMail[LEN_RETMAIL - 1] = '\0';
			}
		}
		// Разбор объявленных расширений: строки вида "250-AUTH PLAIN LOGIN", "250-8BITMIME"
		if(strstr(tempBuf + 3, "8BITMIME")) SETBIT1(smtpCaps, fSMTP_8BITMIME);
		char *a = strstr(tempBuf + 3, "AUTH");
		if(a) {
			if(strstr(a, "PLAIN")) SETBIT1(smtpCaps, fSMTP_AUTH_PLAIN);
			if(strstr(a, "LOGIN")) SETBIT1(smtpCaps, fSMTP_AUTH_LOGIN);
		}
		if(strlen(tempBuf) == 3 || tempBuf[3] == ' ') break;   // последняя строка ответа
		if(tempBuf[3] != '-') { setRetMail("Bad reply format"); return false; }
	}
	return smtpCode < 400;
}

// Послать команду серверу и разобрать ответ. true - код ответа меньше 400.
boolean Message::smtpCmd(const char *cmd, uint32_t timeout)
{
	JOURNAL("> %s\n", cmd);
	return smtpCmdSecret(cmd, timeout);
}

// То же, но текст команды в журнал НЕ пишется: через нее уходят логин и пароль в base64,
// а журнал доступен через веб-морду.
boolean Message::smtpCmdSecret(const char *cmd, uint32_t timeout)
{
	if(!clientMessage.connected()) {
		setRetMail("Connection lost");
		smtpCode = 0;
		return false;
	}
	smtpFlush();                                  // команда не должна перемешаться с остатком тела
	uint16_t len = strlen(cmd);
	if(len > LEN_TXBUF - 3) len = LEN_TXBUF - 3;
	memcpy(txBuf, cmd, len);
	txBuf[len] = '\r'; txBuf[len + 1] = '\n';
	txLen = len + 2;
	smtpFlush();
	return smtpReadReply(timeout);
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
    JOURNAL("MSG: #%d: %s\n", ms, c);
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
  sendRetry = 0;                  // новое уведомление - счетчик повторов и признак ухода SMS сбрасываются
  sendRetryTime = 0;
  smsSent = false;
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
  boolean retryMail = false;                             // письмо не ушло по временной причине, ждем следующей попытки

  if (!waitSend) return true;                            // Отправлять нечего выходим
  if (sendRetry && rtcSAM3X8.unixtime() < sendRetryTime) return true;  // время повторной попытки еще не пришло
  strcpy(retTest, "Ничего не отправлено. Проверьте флаг разрешения отправки сообщений.");
  if (HP.get_uptime() < cDELAY_START_MESSAGE) {
    // ждать не будем, ибо зачем? _delay(200);  // Прошло мало времени после старта возможно инет еще не поднят
    return true;
  }
  clearBuf(); // очистка рабочих буферов
  // Отправка Уведомления. Все таки отправлять придется -))
  if ((GETBIT(messageSetting.flags, fMail)) && (messageData.ms != pMESSAGE_TESTSMS)) // Разрешены уведомления по почте и не тест SMS
  {
    boolean ok = sendMail(); // Формирование ответа
    for (i = 0; i < strlen(retMail); i++) if (retMail[i] == '=') retMail[i] = ':'; // замена знака = на : т.к. это запрещенный знак в запросах
    setRetTest(ok ? "Тестовое письмо отправлено на " : "Тестовое письмо НЕ отправлено на ", mess_SMTP_RCPTTO, retMail);
    // Ошибка с кодом 4xx или обрыв связи (smtpCode == 0) - временная, повтор имеет смысл.
    // При 5xx повторять бессмысленно: нужна правка настроек.
    if (!ok && smtpCode < 500 && sendRetry < MAIL_RETRY_MAX) {
      sendRetry++;
      sendRetryTime = rtcSAM3X8.unixtime() + MAIL_RETRY_DELAY;
      retryMail = true;
      journal.jprintf("Mail retry %d/%d in %d sec\n", sendRetry, MAIL_RETRY_MAX, MAIL_RETRY_DELAY);
    } else sendRetry = 0;
  }

  if ((!(GETBIT(messageSetting.flags, fMail))) && (messageData.ms == pMESSAGE_TESTMAIL)) // Почта не разрешена а отправляется тестовое письмо
  {
    strcpy(retTest, "Письмо не отправлено. Проверьте флаг разрешения отправки почты.");
  }

  // !smsSent - чтобы повторная попытка отправить письмо не продублировала уже ушедшее SMS
  if ((GETBIT(messageSetting.flags, fSMS)) && (messageData.ms != pMESSAGE_TESTMAIL) && !smsSent)
  {
    boolean ok = false;
    switch (messageSetting.sms_service)
    {
      case pSMS_RU:
        ok = sendSMS();
        setRetTest(ok ? "Тестовое SMS отправлено на номер " : "Тестовое SMS НЕ отправлено на номер ", mess_SMS_PHONE, retSMS);
        break;

      case pSMSC_RU:
      case pSMSC_UA:
        ok = sendSMSC();
        setRetTest(ok ? "Тестовое SMS отправлено на номер " : "Тестовое SMS НЕ отправлено на номер ", mess_SMS_PHONE, retSMS);
        break;

      case pSMSCLUB:
        ok = sendSMSCLUB();
        setRetTest(ok ? "Тестовое SMS отправлено на номер " : "Тестовое SMS НЕ отправлено на номер ", mess_SMS_PHONE, retSMS);
        break;

      default:               // некорректная настройка сервиса. Раньше здесь был return ДО сброса
        strcpy(retSMS, "Invalid SMS service");   // waitSend, и отправка дергалась на каждом цикле
        journal.jprintf("%s: %d\n", retSMS, (int)messageSetting.sms_service);
        break;
    }
    smsSent = ok;          // ушло - при повторной попытке письма SMS больше не трогаем
  }


  if ((!(GETBIT(messageSetting.flags, fSMS))) && (messageData.ms == pMESSAGE_TESTSMS)) // SMS не разрешена а отправляется тестовое SMS
    strcpy(retTest, "SMS не отправлено. Проверьте флаг разрешения отправки SMS.");

  if (retryMail) return false;   // waitSend оставляем взведенным - письмо будет повторено
  waitSend = false;        // Сбросить флаг необходимости отпвки уведомления
  sendRetry = 0;
  return true;             // Послано
}

// Отправить почту --------------------------------------------------------------------
// Диалог ведется строго по RFC 5321/5322: EHLO с адресным литералом, конверт без пробела после
// двоеточия, тело завершается CRLF.CRLF, 8-битное тело объявляется через MIME и BODY=8BITMIME.
// Благодаря этому письмо принимает любой стандартный сервер и послабления на стороне реле
// (smtpd_forbid_bare_newline, отказ от reject_non_fqdn_helo_hostname) не нужны.
boolean Message::sendMail()
{
	// ВСЕ переменные объявляются здесь: ниже используется переход на единую точку выхода smtp_error
	static const char dowNames[] = "SunMonTueWedThuFriSat";
	static const char monNames[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	static uint16_t msgIdCnt = 0;
	const char *stage = "connect";       // на каком этапе оборвался сеанс, попадает в журнал и в retMail
	const char *type;                    // тип уведомления для темы письма
	const char *dom;                     // домен отправителя для Message-ID
	IPAddress ip;                        // собственный адрес контроллера, для приветствия EHLO
	IPAddress oldIP;                     // адрес сервера из кеша, чтобы заметить его смену
	IPAddress zeroIP(0, 0, 0, 0);
	boolean connected, nonAscii;
	uint16_t i;
	int n;
	uint32_t ut;
	char dowName[4], monName[4];

	setRetMail("");                      // Обнулить ошибку
	txLen = 0;
	smtpCaps = 0;
	smtpCode = 0;
	smtpSemLost = false;

	// Адреса могли попасть в структуру из EEPROM в обход проверок веб-морды, а управляющий
	// символ в адресе разорвал бы команду SMTP надвое или оборвал заголовки письма
	if(mail_has_ctrl(messageSetting.smtp_MailTo) || mail_has_ctrl(messageSetting.smtp_RCPTTo)) {
		setRetMail("Control char in mail address, check settings");
		smtpCode = 500;                  // постоянная ошибка: повторять бессмысленно
		if(!GETBIT(WorkFlags, fWF_MessageSendError)) journal.jprintf("Error send mail: %s\n", retMail);
		SETBIT1(WorkFlags, fWF_MessageSendError);
		return false;
	}

	// Подготовка
	if(SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE) {
		return false; // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
	}

	// 1. Соединение. Быстрый путь - по адресу из кеша, который заполняет dnsUpdate():
	//    connect() по имени каждый раз делает полный запрос к DNS на том же сокете.
	//    Кеш обновляется только при старте и при смене имени сервера в настройках, поэтому
	//    при неудаче имя ОБЯЗАТЕЛЬНО перепроверяется через DNS: иначе после переезда сервера
	//    на другой адрес контроллер молчал бы до перезагрузки.
	connected = false;
	oldIP = messageSetting.smtp_serverIP;
	if(!(oldIP == zeroIP)) connected = clientMessage.connect(oldIP, messageSetting.smtp_port, W5200_SOCK_SYS);
	if(!connected) {
		check_address(messageSetting.smtp_server, messageSetting.smtp_serverIP); // 0 - не удалось, 1 - на входе был IP, 2 - получен через DNS
		if(!(messageSetting.smtp_serverIP == zeroIP) && !(messageSetting.smtp_serverIP == oldIP)) {
			if(!(oldIP == zeroIP)) journal.jprintf("SMTP server address changed\n");
			connected = clientMessage.connect(messageSetting.smtp_serverIP, messageSetting.smtp_port, W5200_SOCK_SYS);
		}
	}
	if(!connected) {
		if(!GETBIT(WorkFlags, fWF_MessageSendError)) journal.jprintf("Connect failed: %s port: %d\n", messageSetting.smtp_server, messageSetting.smtp_port);
		setRetMail("No connect");
		SETBIT1(WorkFlags, fWF_MessageSendError);
		SemaphoreGive (xWebThreadSemaphore);
		return false;
	}
	JOURNAL("Connect server: %s port: %d\n", messageSetting.smtp_server, messageSetting.smtp_port);

	// 2. Приветствие сервера
	stage = "greeting";
	if(!smtpReadReply(SMTP_TO_GREETING) || smtpCode != 220) goto smtp_error;

	// 3. Представление. EHLO нужен ВСЕГДА, даже без авторизации: только из его ответа известны
	//    поддерживаемые механизмы AUTH и признак 8BITMIME. Имя - адресный литерал в скобках,
	//    RFC 5321 4.1.1.1 требует FQDN либо литерал, прежнее "host" не является ни тем ни другим.
	stage = "EHLO";
	smtpCaps = 0;                        // расширения берем только из ответа на EHLO, текст приветствия не в счет
	ip = Ethernet.localIP();             // именно фактический адрес, а не сохраненный в настройках
	if(ip == zeroIP) ip = HP.get_ip();
	m_snprintf(tempBuf, LEN_TEMPBUF, "EHLO [%d.%d.%d.%d]", ip[0], ip[1], ip[2], ip[3]);
	if(!smtpCmd(tempBuf, SMTP_TO_COMMAND)) {
		if(smtpCode < 500) goto smtp_error;                    // обрыв связи или временная ошибка
		smtpCaps = 0;                                          // сервер не знает ESMTP - откат на HELO
		stage = "HELO";
		m_snprintf(tempBuf, LEN_TEMPBUF, "HELO [%d.%d.%d.%d]", ip[0], ip[1], ip[2], ip[3]);
		if(!smtpCmd(tempBuf, SMTP_TO_COMMAND)) goto smtp_error;
	}

	// 4. Авторизация. Механизм выбирается по ответу EHLO: PLAIN укладывается в один обмен вместо трех,
	//    но предлагают его не все серверы. Защиты ни один из них не дает - base64 это не шифрование.
	if(GETBIT(messageSetting.flags, fMailAUTH))
	{
		stage = "AUTH";
		if(GETBIT(smtpCaps, fSMTP_AUTH_PLAIN)) {
			// AUTH PLAIN: <NUL>логин<NUL>пароль в base64, RFC 4616. Собираем вручную - в строке есть нули
			n = 0;
			mailBuf[n++] = '\0';
			i = strlen(messageSetting.smtp_login);  memcpy(mailBuf + n, messageSetting.smtp_login, i);    n += i;
			mailBuf[n++] = '\0';
			i = strlen(messageSetting.smtp_password); memcpy(mailBuf + n, messageSetting.smtp_password, i); n += i;
			strcpy(tempBuf, "AUTH PLAIN ");
			base64_encode(tempBuf + strlen(tempBuf), mailBuf, n);
			memset(mailBuf, 0, n);        // не оставлять пароль открытым текстом в общем буфере веб-сервера
			JOURNAL("> AUTH PLAIN <hidden>\n");
			if(!smtpCmdSecret(tempBuf, SMTP_TO_COMMAND)) goto smtp_error;
		} else if(GETBIT(smtpCaps, fSMTP_AUTH_LOGIN)) {
			if(!smtpCmd("AUTH LOGIN", SMTP_TO_COMMAND)) goto smtp_error;
			base64_encode(tempBuf, messageSetting.smtp_login, strlen(messageSetting.smtp_login));         // Послать логин
			JOURNAL("> <login hidden>\n");
			if(!smtpCmdSecret(tempBuf, SMTP_TO_COMMAND)) goto smtp_error;
			base64_encode(tempBuf, messageSetting.smtp_password, strlen(messageSetting.smtp_password));   // Послать пароль
			JOURNAL("> <password hidden>\n");
			if(!smtpCmdSecret(tempBuf, SMTP_TO_COMMAND)) goto smtp_error;
		} else {
			// Обычно так отвечает сервер, требующий STARTTLS до авторизации, а TLS контроллер не умеет
			setRetMail("Server offers no AUTH PLAIN/LOGIN");
			smtpCode = 500;                        // постоянная ошибка: повторять бессмысленно, нужна правка настроек
			goto smtp_error;
		}
	}

	// 5. Конверт. RFC 5321 4.1.1.2: пробела после двоеточия быть не должно
	stage = "MAIL FROM";
	m_snprintf(tempBuf, LEN_TEMPBUF, "MAIL FROM:<%s>%s", messageSetting.smtp_MailTo,
	           GETBIT(smtpCaps, fSMTP_8BITMIME) ? " BODY=8BITMIME" : "");  // тело в utf-8, оно 8-битное
	if(!smtpCmd(tempBuf, SMTP_TO_COMMAND)) goto smtp_error;

	stage = "RCPT TO";
	m_snprintf(tempBuf, LEN_TEMPBUF, "RCPT TO:<%s>", messageSetting.smtp_RCPTTo);
	if(!smtpCmd(tempBuf, SMTP_TO_COMMAND)) goto smtp_error;

	stage = "DATA";
	if(!smtpCmd("DATA", SMTP_TO_DATA_INIT) || smtpCode != 354) goto smtp_error;

	// 6. Заголовки письма. С этого места и до завершающей точки вывод идет ТОЛЬКО через
	//    smtpWriteBody(): он гарантирует CRLF, экранирование ведущей точки и предел длины строки.
	stage = "headers";
	// Date - свой, чтобы почтовые клиенты показывали время события, а не время получения (RFC 5322 3.3)
	ut = rtcSAM3X8.unixtime();
	memcpy(dowName, dowNames + (uint8_t)(((ut / 86400UL) + 4) % 7) * 3, 3); dowName[3] = '\0'; // 01.01.1970 - четверг
	i = rtcSAM3X8.get_months(); if(i < 1 || i > 12) i = 1;
	memcpy(monName, monNames + (i - 1) * 3, 3); monName[3] = '\0';
	m_snprintf(tempBuf, LEN_TEMPBUF, "Date: %s, %d %s %d %02d:%02d:%02d %c%02d00",
	           dowName, rtcSAM3X8.get_days(), monName, rtcSAM3X8.get_years(),
	           rtcSAM3X8.get_hours(), rtcSAM3X8.get_minutes(), rtcSAM3X8.get_seconds(),
	           TIME_ZONE < 0 ? '-' : '+', TIME_ZONE < 0 ? -(TIME_ZONE) : TIME_ZONE);
	smtpWriteBody(tempBuf);
	// Message-ID - без него почтовые клиенты не связывают ответы в цепочку (RFC 5322 3.6.4)
	dom = strchr(messageSetting.smtp_MailTo, '@');
	m_snprintf(tempBuf, LEN_TEMPBUF, "Message-ID: <%u.%u@%s>", ut, ++msgIdCnt, dom ? dom + 1 : "heatpump.local");
	smtpWriteBody(tempBuf);
	m_snprintf(tempBuf, LEN_TEMPBUF, "From: <%s>", messageSetting.smtp_MailTo);
	smtpWriteBody(tempBuf);
	m_snprintf(tempBuf, LEN_TEMPBUF, "To: <%s>", messageSetting.smtp_RCPTTo);
	smtpWriteBody(tempBuf);
	// Тема письма
	switch((int) messageData.ms)   // Заголовок уведомления - добавляем тип уведомления
	{
	case pMESSAGE_NONE:		type = "";			break;                     // Нет уведомлений
	case pMESSAGE_TESTMAIL:	type = "TESTMAIL";	break;                     // Тестовое почта
	case pMESSAGE_TESTSMS:	type = "TESTSMS";	break;                     // Тестовое смс
	case pMESSAGE_RESET:	type = "RESET";		break;                     // Уведомление Сброс
	case pMESSAGE_ERROR:	type = "ERROR";		break;                     // Уведомление Ошибка
	case pMESSAGE_LIFE:		type = "LIFE";		break;                     // Уведомление Сигнал жизни
	case pMESSAGE_TEMP:		type = "TEMP";		break;                     // Уведомление Достижение граничной температуры
	case pMESSAGE_SD:		type = "ERROR SD";	break;                     // Уведомление "Проблемы с sd картой"
	case pMESSAGE_WARNING:	type = "WARNING";	break;                     // Уведомление "Прочие уведомления"
	default:
		setRetMail("Unknown message type");
		smtpCode = 500;                    // постоянная ошибка: повторять бессмысленно
		goto smtp_error;
	}
	// Тема ограничена 150 байтами: после base64 она вырастет на треть и должна уместиться в tempBuf
	if(*type) n = m_snprintf(mailBuf, 151, "Controller %s - %s", nameHeatPump, type);
	else      n = m_snprintf(mailBuf, 151, "Controller %s", nameHeatPump);   // без висящего дефиса
	// Если обрезка попала внутрь многобайтного символа utf-8 - отбросить незаконченную последовательность
	i = n;
	while(i > 0 && (mailBuf[i - 1] & 0xC0) == 0x80) i--;    // хвостовые байты-продолжения
	if(i > 0 && (mailBuf[i - 1] & 0x80)) {                  // перед ними ведущий байт последовательности
		i--;                                                // длина: 2 при 110xxxxx, 3 при 1110xxxx, иначе 4
		if(n - i < ((mailBuf[i] & 0xE0) == 0xC0 ? 2 : (mailBuf[i] & 0xF0) == 0xE0 ? 3 : 4)) {
			n = i; mailBuf[n] = '\0';                       // последовательность не поместилась целиком
		}
	}
	nonAscii = false;                       // заголовки по RFC 5322 семибитные, кириллицу надо кодировать
	for(i = 0; mailBuf[i]; i++) if(mailBuf[i] & 0x80) { nonAscii = true; break; }
	if(nonAscii) {                          // RFC 2047: =?UTF-8?B?<base64>?=
		strcpy(tempBuf, "Subject: =?UTF-8?B?");
		base64_encode(tempBuf + strlen(tempBuf), mailBuf, n);
		strcat(tempBuf, "?=");
	} else m_snprintf(tempBuf, LEN_TEMPBUF, "Subject: %s", mailBuf);
	smtpWriteBody(tempBuf);
	smtpWriteBody("MIME-Version: 1.0");
	smtpWriteBody("Content-Type: text/plain; charset=utf-8");
	smtpWriteBody("Content-Transfer-Encoding: 8bit");   // тело содержит кириллицу в utf-8, а не us-ascii

	// 7. ПУСТАЯ СТРОКА - обязательный разделитель заголовков и тела (RFC 5322 2.1).
	//    Без нее тело приклеивается к последнему заголовку и получатель видит пустое письмо.
	smtpWriteBody("");

	// 8. Текст сообщения
	m_snprintf(tempBuf, LEN_TEMPBUF, "--- %s ver.%s ---", SendMessageTitle, VERSION);
	smtpWriteBody(tempBuf);
	m_snprintf(tempBuf, LEN_TEMPBUF, "Конфигурация: %s (%s)", CONFIG_NAME, CONFIG_NOTE);
	smtpWriteBody(tempBuf);
	m_snprintf(tempBuf, LEN_TEMPBUF, "Создание уведомления: %s %s", NowDateToStr(), NowTimeToStr());
	smtpWriteBody(tempBuf);
	smtpWriteBlock(messageData.data);      // само уведомление, буфер не изменяется (его ждет еще sendSMS)

	// 9. Дополнительная информация если требуется добавляется в уведомление
	if(GETBIT(messageSetting.flags, fMailInfo)) get_mailState(mailBuf, LEN_MAILBUF);

	// 10. Завершение письма. Тело гарантированно закончилось на CRLF, поэтому точка дает CRLF.CRLF
	stage = "end of DATA";
	smtpFlush();
	if(!smtpCmd(".", SMTP_TO_DATA_END)) goto smtp_error;
	// retMail уже содержит ответ сервера ("250 ... queued as ..."), записанный в smtpReadReply()

	stage = "QUIT";
	m_snprintf(mailBuf, LEN_MAILBUF, "%s", retMail);   // сохранить ответ: его перезапишет ответ на QUIT
	smtpCmd("QUIT", SMTP_TO_QUIT);                     // результат не важен, письмо уже принято
	setRetMail(mailBuf);
	SETBIT0(WorkFlags, fWF_MessageSendError);
	// Письмо уже принято сервером, поэтому неудача QUIT ничего не меняет - возвращаем успех.
	// Но если на QUIT потерян семафор, сокет трогать нельзя и отдавать семафор нельзя тоже.
	if(smtpSemLost) {
		journal.jprintf("Mail sent, but network mutex lost\n");
		return true;
	}
	clientMessage.stop();
	JOURNAL("OK disconnected\n");
	SemaphoreGive (xWebThreadSemaphore);
	return true;

smtp_error:
	// Единая точка выхода по ошибке. Сначала штатный QUIT - тогда сервер запишет в свой журнал
	// внятную причину вместо "lost connection", по которой потом трудно понять, что произошло.
	// Текст ошибки формируем ДО QUIT: ответ на него перезапишет retMail.
	if(smtpCode >= 400)   // 4xx - временная (повтор имеет смысл), 5xx - постоянная (нужна правка настроек)
		m_snprintf(mailBuf, LEN_MAILBUF, "%s error on %s: %s", smtpCode < 500 ? "Temporary" : "Permanent", stage, retMail);
	else
		m_snprintf(mailBuf, LEN_MAILBUF, "Failed on %s: %s", stage, strlen(retMail) ? retMail : "no answer");
	if(!GETBIT(WorkFlags, fWF_MessageSendError)) journal.jprintf("Error send mail: %s\n", mailBuf);
	txLen = 0;                             // недописанное тело в сокет не отправляем
	SETBIT1(WorkFlags, fWF_MessageSendError);
	// Семафор сети потерян: он сейчас у другой задачи, поэтому ни сокет трогать, ни отдавать
	// семафор нельзя - он бинарный, и лишний Give освободил бы SPI из-под владельца.
	// Сокет не течет: следующий connect() на этот же сокет начинает с Sock_CLOSE.
	if(smtpSemLost) {
		journal.jprintf("Send mail aborted: network mutex lost\n");
		setRetMail(mailBuf);
		return false;
	}
	n = smtpCode;                          // ответ на QUIT затрет код ошибки, а по нему sendMessage
	if(clientMessage.connected()) smtpCmd("QUIT", SMTP_TO_QUIT);   // решает, имеет ли смысл повтор
	smtpCode = n;
	setRetMail(mailBuf);                   // ставим ПОСЛЕ QUIT: его ответ перезаписал бы retMail
	if(!smtpSemLost) {                     // семафор мог потеряться уже на самом QUIT
		clientMessage.stop();
		SemaphoreGive (xWebThreadSemaphore);
	} else journal.jprintf("Send mail aborted: network mutex lost\n");
	return false;
}

// Отправить SMS sms.ru ----------------------------------------------------------------------------------
// true - была отправка смс false - не было отправки смс
boolean Message::sendSMS()
{
	uint16_t i;
	uint8_t count = 0;
	IPAddress zeroIP(0, 0, 0, 0);

	if(SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE) {
		return false; // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
	}

	strcpy(retSMS, ADR_SMS_RU); // Используется как временный буфер check_address с констанатми не работает??

	if(messageSetting.sms_serviceIP == zeroIP) // возможно днс надо всегда обновлять
	{
		//обновляем IP адрес сервера
		if(!check_address(retSMS, messageSetting.sms_serviceIP)) {
			strcpy(retSMS, ADR_SMS_RU);  // Обновить адрес
			strcat(retSMS, " DNS lookup failed!");
			return false;
		}
	}

	strcpy(retSMS, "");         // Обнулить ответ
	if(clientMessage.connect(messageSetting.sms_serviceIP, 80, W5200_SOCK_SYS)) // Соединение по HTTP
	{
		JOURNAL("Connect server: %s port: %d\n", ADR_SMS_RU, 80);
	} else {
		if(!GETBIT(WorkFlags, fWF_SMSSendError)) journal.jprintf("Connect failed: %s port: %d\n", ADR_SMS_RU, 80);
		strcpy(retSMS, "No connect ");
		strcat(retSMS, ADR_SMS_RU);
		clientMessage.stop();
		SETBIT1(WorkFlags, fWF_SMSSendError);
		SemaphoreGive (xWebThreadSemaphore);
		return false;
	}

	// посылка смс
	strcpy(tempBuf, "GET http://sms.ru/sms/send?api_id=");                    // Начало
	strcat(tempBuf, messageSetting.sms_p1);                                   // id
	strcat(tempBuf, "&to=");
	strcat(tempBuf, messageSetting.sms_phone);                                // номер
	strcat(tempBuf, "&text=");
	strcat(tempBuf, SendSMSTitle);
	strcat(tempBuf, "+");
	switch((int) messageData.ms)   // Заголовок уведомления - добавляем тип уведомления
	{
	case pMESSAGE_NONE:	break;                                                                                       // Нет уведомлений
	case pMESSAGE_TESTMAIL:	strcat(tempBuf, "TESTMAIL+"); break;                                                         // Тестовое почта
	case pMESSAGE_TESTSMS:	strcat(tempBuf, "TESTSMS+"); break;                                                         // Тестовое смс
	case pMESSAGE_RESET:    strcat(tempBuf, "RESET+");	 break;                                                            // Уведомление Сброс
	case pMESSAGE_ERROR:	strcat(tempBuf, "ERROR+");	break;                                                            // Уведомление Ошибка
	case pMESSAGE_LIFE: 	strcat(tempBuf, "LIFE+");   break;                                                             // Уведомление Сигнал жизни
	case pMESSAGE_TEMP:		strcat(tempBuf, "TEMP+");	break;                                                             // Уведомление Достижение граничной температуры
	case pMESSAGE_SD:		strcat(tempBuf, "ERROR+SD+");	break;                                                         // Уведомление "Проблемы с sd картой"
	case pMESSAGE_WARNING:	strcat(tempBuf, "WARNING+");  break;                                                          // Уведомление "Прочие уведомления"
	default:
		strcat(tempBuf, "ERROR+TYPE+");
		SemaphoreGive (xWebThreadSemaphore);
		return false;
	}
	// Замена пробела на знак "+" т.к. это запрещенный знак при отправке сообщения.
	// Заменяем в КОПИИ: сам messageData.data править нельзя, его использует и отправка почты
	i = strlen(tempBuf);
	strcat(tempBuf, messageData.data);                       // содержимое
	for(; tempBuf[i]; i++) if(tempBuf[i] == ' ') tempBuf[i] = '+';
	strcat(tempBuf, cStrEnd);
	clientMessage.write(tempBuf, strlen(tempBuf));

	//JOURNAL("%s\n",tempBuf);

	// Ожидание и получение ответа
	while(!clientMessage.available()) // ожидание ответа 5 сек
	{
		SemaphoreGive (xWebThreadSemaphore);
		_delay(200);
		if(SemaphoreTake(xWebThreadSemaphore, (W5200_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE) {
			return false; // Захват семафора потока или ОЖИДАНИЕ W5200_TIME_WAIT, если семафор не получен то выходим
		}
		count++;
		if(count > 25) {
			strcpy(retSMS, "SMS server not responding!");
			if(!GETBIT(WorkFlags, fWF_SMSSendError)) journal.jprintf("%s\n", retSMS);
			SETBIT1(WorkFlags, fWF_SMSSendError);
			clientMessage.stop();
			SemaphoreGive(xWebThreadSemaphore);
			return false;
		}

	}

	// разбор ответа
	byte thisByte;
	// режем заголовок, он отделен последовательностью 0x0a 0x0d 0x0a от ответа
	while(clientMessage.available()) {
		thisByte = clientMessage.read();
		if(thisByte == 0x0a) {
			thisByte = clientMessage.read();
			if(thisByte == 0x0d) {
				thisByte = clientMessage.read();
				if(thisByte == 0x0a) {
					i = clientMessage.read((byte*) retSMS, LEN_RETSMS/* sizeof(retSMS)*/- 1); // получаем код ответа
					retSMS[i] = 0;                                         // обрезаем строку
					for(i = 0; i < strlen(retSMS); i++)
						if(retSMS[i] == '=') retSMS[i] = ':'; // замена = на знак ":" веб морда глючит в ответах - обрезает по  =
					JOURNAL("sms.ru return: %s \n", retSMS);
					clientMessage.stop();
					SETBIT0(WorkFlags, fWF_SMSSendError);
					SemaphoreGive (xWebThreadSemaphore);
					return true; // смс отправлено
				}
			}
		}

	}
	clientMessage.stop();
	SemaphoreGive (xWebThreadSemaphore);
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
	  JOURNAL("Connect server: %s port: %d\n", messageSetting.sms_service == pSMSC_RU ? ADR_SMS_RU : ADR_SMSC_UA, 80);
  }
  else
  {
	  if(!GETBIT(WorkFlags, fWF_SMSSendError)) journal.jprintf("Connect failed: %s port: %d\n", messageSetting.sms_service == pSMSC_RU ? ADR_SMS_RU : ADR_SMSC_UA, 80);
      strcpy(retSMS, "No connect "); strcat(retSMS, ADR_SMSC_RU);
      clientMessage.stop();
      SETBIT1(WorkFlags, fWF_SMSSendError);
      SemaphoreGive(xWebThreadSemaphore);
      return false;
  }

  // посылка смс
  if ((messageSetting.sms_service) == pSMSC_RU) strcpy(tempBuf, "GET http://smsc.ru/sys/send.php?login=");         // Начало
  if ((messageSetting.sms_service) == pSMSC_UA) strcpy(tempBuf, "GET http://smsc.ua/sys/send.php?login=");         // Начало
  strcat(tempBuf, messageSetting.sms_p1);                                   // login
  strcat(tempBuf, messageSetting.sms_p2);                                   // password
  strcat(tempBuf, "&phones=");
  strcat(tempBuf, messageSetting.sms_phone);                                // номер
  strcat(tempBuf, "&mes=");
  strcat(tempBuf, SendSMSTitle);
  strcat(tempBuf, "+");
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
  // Замена пробела на знак "+" т.к. это запрещенный знак при отправке сообщения.
  // Заменяем в КОПИИ: сам messageData.data править нельзя, его использует и отправка почты
  i = strlen(tempBuf);
  strcat(tempBuf, messageData.data);                       // содержимое
  for (; tempBuf[i]; i++) if (tempBuf[i] == ' ') tempBuf[i] = '+';
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
      strcpy(retSMS, "SMS server not responding!");
      if(!GETBIT(WorkFlags, fWF_SMSSendError)) journal.jprintf("%s\n", retSMS);
      clientMessage.stop();
      SETBIT1(WorkFlags, fWF_SMSSendError);
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
    clientMessage.stop();
    SemaphoreGive(xWebThreadSemaphore);
    if (strstr(retSMS, "ERROR")) {
    	if(!GETBIT(WorkFlags, fWF_SMSSendError)) journal.jprintf("Error send SMS: %s\n", retSMS);
    	SETBIT1(WorkFlags, fWF_SMSSendError);
    	return false;
    } else {
        JOURNAL("server return: %s \n", retSMS);
        SETBIT0(WorkFlags, fWF_SMSSendError);
    	return true;
    }
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
    JOURNAL("Connect server: %s port: %d\n", ADR_SMSC_RU, 80);
  }
  else
  {
	if(!GETBIT(WorkFlags, fWF_SMSSendError)) journal.jprintf("Connect failed: %s port: %d\n", ADR_SMSC_RU, 80);
    strcpy(retSMS, "No connect "); strcat(retSMS, ADR_SMSC_RU);
    clientMessage.stop();
    SETBIT1(WorkFlags, fWF_SMSSendError);
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
  strcat(tempBuf, "]]></to>\r\n<text><![CDATA[");
  strcat(tempBuf, SendSMSTitle);
  strcat(tempBuf, "+");

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
      strcpy(retSMS, "SMS server not responding!");
      if(!GETBIT(WorkFlags, fWF_SMSSendError)) journal.jprintf("%s\n", retSMS);
      clientMessage.stop();
      SETBIT1(WorkFlags, fWF_SMSSendError);
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
            clientMessage.stop();
            SemaphoreGive(xWebThreadSemaphore);
            if (strstr(retSMS, ">OK<")) {
            	if(!GETBIT(WorkFlags, fWF_SMSSendError)) journal.jprintf("Error send SMS: %s\n", retSMS);
            	SETBIT1(WorkFlags, fWF_SMSSendError);
            	return false;
            } else {
                JOURNAL("server return: %s \n", retSMS);
                SETBIT0(WorkFlags, fWF_SMSSendError);
            	return true;
            }
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
