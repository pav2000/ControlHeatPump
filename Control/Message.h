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
// -----------------------------------------------------------------
// Работа с уведомлениями
// Класс message
// -----------------------------------------------------------------
#ifndef Message_h
#define Message_h
#include "Constant.h"                       // Вся конфигурация и константы проекта Должен быть первым !!!!

// Константы 
#define ADR_SMS_RU       "sms.ru"
#define ADR_SMSC_RU      "smsc.ru"
#define ADR_SMSC_UA      "smsc.ua"
#define ADR_SMSCLUB_UA   "gate.smsclub.mobi"
#define LEN_TEMPBUF      250                // Длина рабочего буфера используемого для работы уведомлений
#define LEN_RETMAIL      100                // Длина ответа почты
#define LEN_RETSMS       64                 // Длина ответа sms
#define REPEAT_TIME      (3*60*60)          // Время посылки дублирующего сообщения (повтор одинаковых сообщений) сек


// Структуры
// Структура для хранения параметров уведомлений.
// Работа с отдельными флагами type_messageHP
#define fMail            0                  // флаг уведомления скидывать на почту 
#define fMailAUTH        1                  // флаг необходимости авторизации на почтовом сервере
#define fMailInfo        2                  // флаг необходимости добавления в письмо информации о состянии ТН
#define fSMS             3                  // флаг уведомления скидывать на СМС
#define fMessageReset    4                  // флаг уведомления Сброс
#define fMessageError    5                  // флаг уведомления Ошибка
#define fMessageLife     6                  // флаг уведомления Сигнал жизни
#define fMessageTemp     7                  // флаг уведомления Достижение граничной температуры
#define fMessageSD       8                  // флаг уведомления "Проблемы с sd картой"
#define fMessageWarning  9                  // флаг уведомления "Прочие уведомления"

// Настройки уведомлений
struct type_messageHP
{
 uint16_t flags;                             // Флаги уведомлений до 16 флагов
// настройки соединения с SMTP
 char smtp_server[32];                       // Адрес сервера
 IPAddress smtp_serverIP;                    // IP Адрес сервера  
 uint16_t smtp_port;                         // Адрес порта сервера 
 char smtp_login[32];                        // логин сервера если включена авторизация
 char smtp_password[16];                     // пароль сервера если включена авторизация
 char smtp_MailTo[32];                       // адрес отправителя
 char smtp_RCPTTo[32];                       // адрес получателя  
// Настройка СМС
 SMS_SERVICE sms_service;                    // сервис отправки смс
 IPAddress sms_serviceIP;                    // IP Адрес сервера  смс
 char sms_phone[16];                         // телефон куда отправляется смс
 char sms_p1[40];                            // первый параметр для отправки смс
 char sms_p2[40];                            // второй параметр для отправки смс
// настройки самих сообщений
 int16_t mTIN;                               // Критическая температура в доме (если меньше то генерится уведомление)
 int16_t mTBOILER;                           // Критическая температура бойлера (если меньше то генерится уведомление)
 int16_t mTCOMP;                             // Критическая температура компрессора (если больше то генерится уведомление)
 int16_t P1,P2;
};
// Само уведомление (данные)
struct type_messageData
{
  MESSAGE ms;              // Тип уведомления
  char data[200];          // Полезная информация кодировка utf-8 русские буквы 2 байта!!
  int p1;                  // первый параметр
  int number;              // номер уведомления
};

// ------------------------- КЛАСС MESSAGE --------------------------------------
class Message
  {
   public:
     void initMessage();                                                    // Инициализация переменных
     boolean setMessage(MESSAGE ms, char *c, int p1);                       // Установить уведомление (сформировать для отправки но НЕ ОТПРАВЛЯТЬ)
     boolean setTestMail();                                                 // Установить (сформировать) тестовое письмо, отправка sendMessage();  
     boolean setTestSMS();                                                  // Установить (сформировать) тестовое СМС, отправка sendMessage();  
     boolean sendMessage();                                                 // Послать уведомление согласно выбранных настроек cформированное setMessage
     boolean dnsUpdate();                                                   // Обновление IP адресов серверов через dns 
     boolean dnsUpdateStart();                                              // Обновление IP адресов серверов через dns при СТАРТЕ!!! семафоры не используются
     
     boolean set_messageSetting(char *var, char *c);                        // Установить параметр Уведомления из строки
     char*   get_messageSetting(char *var, char *ret);                      // Получить параметр Уведомления по имени var, результат ДОБАВЛЯЕТСЯ в строку ret
     // Чтение отдельных параметров
     boolean get_fMessageTemp(){return GETBIT(messageSetting.flags,fMessageTemp);}// чтение флага уведомлений Достижение граничной температуры
     boolean get_fMessageLife(){return GETBIT(messageSetting.flags,fMessageLife);}// чтение флага уведомлений Сигнал жизния
     
     int16_t get_mTIN(){return messageSetting.mTIN;}                        // чтение Критическая температура в доме
     int16_t get_mTBOILER(){return messageSetting.mTBOILER;}                // чтение Критическая температура бойлера
     int16_t get_mTCOMP(){return messageSetting.mTCOMP;}                    // чтение Критическая температура компрессора

     uint8_t *get_save_addr(void) { return (uint8_t *)&messageSetting; } // Адрес структуры сохранения
     uint16_t get_save_size(void) { return sizeof(messageSetting); } // Размер структуры сохранения
     int32_t save(int32_t adr);                                             // Записать настройки в eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
     int32_t load(int32_t adr);                                             // Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки 
     int32_t loadFromBuf(int32_t adr, byte *buf);                           // Считать настройки из буфера на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки 
     uint16_t get_crc16(uint16_t crc);                                      // Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
     char    retTest[220];                                                  // ПОСЛЕДНИЙ ответ от посылки тестового УВЕДОМЛЕНИЯ для экономии места он единый для писем и смс
            
   private:
    uint32_t sendTime;                                                       // время отправки последнего уведомления
    char tempBuf[LEN_TEMPBUF+1];                                             // буфер для работы уведомлений, используется и на прием и на отправку
    EthernetClient clientMessage;                                            // Клиент для отправки уведомлений
    boolean dnsUpadateSMS;                                                   // Флаг необходимости обновления через dns IP адреса для смс
    boolean dnsUpadateSMTP;                                                  // Флаг необходимости обновления через dns IP адреса для smtp
    char retMail[LEN_RETMAIL+1];                                             // ответ сервера при отправке почты
    char retSMS[LEN_RETSMS];                                                 // ответ сервера при отправке sms  
    // Уведомления
    type_messageHP     messageSetting;                                       // Структура для хранения НАСТРОЕК уведомлений
    type_messageData   messageData;                                          // Структура для хранения уведомления (ДАННЫЕ)
    MESSAGE lastmessageSetting;                                              // последнее отправленное уведомление 
    boolean waitSend;                                                        // true - есть не отправленные сообщения
    boolean sendMail();                                                      // Отправить почту
    boolean sendSMS();                                                       // Отправить SMS через sms.ru
    boolean sendSMSC();                                                      // Отправить SMS через smsc.ru
    boolean sendSMSCLUB();                                                   // Отправить SMS через smsclub.mobi (Ukraine)
    boolean SendCommandSMTP(char *c,boolean w);                              // Послать команду SMTP серверу  и разобрать ответ  
  };

#endif  
