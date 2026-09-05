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
// -----------------------------------------------------------------
// Работа с уведомлениями
// Класс message
// -----------------------------------------------------------------
#ifndef Message_h
#define Message_h
#include "Constant.h"                       // Вся конфигурация и константы проекта Должен быть первым !!!!

// Константы
#define LEN_TEMPBUF      256                // Длина рабочего буфера используемого для работы уведомлений
#define LEN_RETMAIL      128                // Длина ответа почты
#define LEN_RETSMS       64                 // Длина ответа sms
#define LEN_RETTEST      220                // Длина ответа от посылки тестового УВЕДОМЛЕНИЯ для экономии места он единый для писем и смс
#define REPEAT_TIME      (3*60*60)          // Время посылки дублирующего сообщения (повтор одинаковых сообщений) сек

// SMTP: константы протокола (RFC 5321/5322)
#define SMTP_CRLF        "\r\n"             // ЕДИНСТВЕННЫЙ допустимый разделитель строк в SMTP, cStrEnd ("\n") здесь применять нельзя
#define LEN_MAILBUF      1536               // Рабочая область для сборки строк тела письма и приема вывода get_Counts()
#define SMTP_MAX_LINE    900                // Порог разрезания строки тела, RFC 5321 разрешает до 998 без CRLF
#define SMTP_FLUSH_AT    1400               // Порог сброса накопителя в сокет
#define SMTP_MAX_SEND    1024               // Предел одной записи в сокет, заведомо меньше W5100.SSIZE (2048):
                                            // send() обрезает по SSIZE и молча теряет хвост, write() ошибку не возвращает
#define LEN_TXBUF        (SMTP_FLUSH_AT + SMTP_MAX_LINE + 16) // Накопитель исходящего потока, вмещает порог + самую длинную строку
// Таймауты этапов сеанса, мсек (RFC 5321 4.5.3.2 требует больше, но поток отправки блокирующий)
#define SMTP_TO_GREETING  30000UL           // 220 при подключении
#define SMTP_TO_COMMAND   30000UL           // EHLO, AUTH, MAIL, RCPT
#define SMTP_TO_DATA_INIT 60000UL           // 354 в ответ на DATA
#define SMTP_TO_DATA_END  120000UL          // 250 после завершающей точки
#define SMTP_TO_QUIT      3000UL            // 221 в ответ на QUIT, ждать долго незачем
#define SMTP_POLL_FAST    2                 // Шаг опроса сокета в начале ожидания ответа, мсек
#define SMTP_POLL_SLOW    20                // Шаг опроса дальше, мсек (локальное реле отвечает за единицы мсек)
#define SMTP_POLL_FAST_N  10                // Сколько первых итераций опрашивать часто
// Повтор отправки при ВРЕМЕННОЙ ошибке (код 4xx или обрыв связи). При 5xx повтор бессмысленен
#define MAIL_RETRY_MAX    3                 // Максимум попыток повторной отправки одного уведомления
#define MAIL_RETRY_DELAY  300               // Пауза между попытками, сек
// Возможности сервера, разобранные из ответа на EHLO (поле smtpCaps)
#define fSMTP_AUTH_PLAIN  0                 // сервер предлагает механизм AUTH PLAIN
#define fSMTP_AUTH_LOGIN  1                 // сервер предлагает механизм AUTH LOGIN
#define fSMTP_8BITMIME    2                 // сервер принимает 8-битное тело (BODY=8BITMIME)


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
// Рабочие флаги
#define fWF_MessageSendError 	0			// ошибка отправки email
#define fWF_SMSSendError 		1			// ошибка отправки SMS

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
     void initMessage(uint8_t web_task);                                    // Инициализация переменных параметр - номер потока сервера в котором запускается отправка уведомлений
     boolean setMessage(MESSAGE ms, char *c, int p1);                       // Установить уведомление (сформировать для отправки но НЕ ОТПРАВЛЯТЬ)
     boolean setTestMail();                                                 // Установить (сформировать) тестовое письмо, отправка sendMessage();  
     boolean setTestSMS();                                                  // Установить (сформировать) тестовое СМС, отправка sendMessage();  
     boolean sendMessage();                                                 // Послать уведомление согласно выбранных настроек cформированное setMessage
     boolean dnsUpdate();                                                   // Обновление IP адресов серверов через dns
     // Вывод тела письма. Публичные, т.к. вызываются из get_mailState() в getFiles.ino.
     // Использовать ТОЛЬКО внутри сеанса sendMail(), между DATA и завершающей точкой.
     // Входной буфер НЕ изменяется, поэтому можно передавать и строковые константы.
     void    smtpWriteBody(const char *line);                               // ЕДИНСТВЕННАЯ точка вывода строки тела: экранирование точки, разрезание, CRLF
     void    smtpWriteBodyLen(const char *line, uint16_t len);              // То же, но длина задана явно (строка может быть не завершена нулем)
     void    smtpWriteBlock(const char *block);                             // Разобрать готовый многострочный блок и вывести построчно

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
            
   private:
    uint32_t sendTime;                                                       // время отправки последнего уведомления
    EthernetClient clientMessage;                                            // Клиент для отправки уведомлений
    boolean dnsUpadateSMS;                                                   // Флаг необходимости обновления через dns IP адреса для смс
    boolean dnsUpadateSMTP;                                                  // Флаг необходимости обновления через dns IP адреса для smtp
    // Уведомления
    type_messageHP     messageSetting;                                       // Структура для хранения НАСТРОЕК уведомлений
    type_messageData   messageData;                                          // Структура для хранения уведомления (ДАННЫЕ)
    MESSAGE lastmessageSetting;                                              // последнее отправленное уведомление 
    boolean waitSend;                                                        // true - есть не отправленные сообщения
    boolean sendMail();                                                      // Отправить почту
    boolean sendSMS();                                                       // Отправить SMS через sms.ru
    boolean sendSMSC();                                                      // Отправить SMS через smsc.ru
    boolean sendSMSCLUB();                                                   // Отправить SMS через smsclub.mobi (Ukraine)
    // Транспорт SMTP, все строки завершаются SMTP_CRLF
    void    setRetMail(const char *s);                                       // Скопировать ответ сервера в retMail с гарантированным нулем в конце
    void    setRetTest(const char *prefix, const char *var, const char *answer); // Собрать ответ на тестовое уведомление с контролем границ retTest
    boolean smtpReadLine(char *buf, uint16_t size, uint32_t timeout);        // Прочитать одну строку ответа до LF, хвостовой CR срезается
    boolean smtpReadReply(uint32_t timeout);                                 // Прочитать ответ целиком (учитывая многострочный), код в smtpCode, текст в retMail
    boolean smtpCmd(const char *cmd, uint32_t timeout);                      // Послать команду и разобрать ответ, true если код < 400
    boolean smtpCmdSecret(const char *cmd, uint32_t timeout);                // То же, но содержимое команды в журнал не пишется (логин/пароль)
    void    smtpFlush();                                                     // Сбросить накопитель txBuf в сокет
    void clearBuf(){tempBuf[0]=0x00;retMail[0]=0x00;retSMS[0]=0x00;/*retTest[0]=0x00;*/};// очистка рабочих буферов перед отправкой уведомления
    // Рабочие буфера для отправки увеломлений
    // Для экономии места используется выходной буфер MAIN_WEB_TASK потока веб сервера Socket[MAIN_WEB_TASK].outBuf[3*W5200_MAX_LEN] (в этом потоке ДОЛЖЕН проводится запуск уведомлений и буфер гарантированно не используется другими задачами)
    // Размер выходного буфера char outBuf[3*W5200_MAX_LEN] - 6 кБ - хватит на все. Распределение памяти в выходном буфере:
    //  смещение 0                                  -  tempBuf[LEN_TEMPBUF] - буфер для работы уведомлений, используется и на прием и на отправку, длина до LEN_TEMPBUF
    //  смещение LEN_TEMPBUF                        -  retMail[LEN_RETMAIL] - ответ сервера при отправке почты, длина до  LEN_RETMAIL
    //  смещение LEN_TEMPBUF+LEN_RETMAIL            -  retSMS[LEN_RETSMS]   - ответ сервера при отправке sms, длина до  LEN_RETSMS
    //  смещение MAILBUF_OFFSET (=448)              -  mailBuf[LEN_MAILBUF] - сборка строк тела письма и прием вывода get_Counts() (он формирует ~1.3 кБ, в tempBuf НЕ помещается)
    //  смещение MAILBUF_OFFSET+LEN_MAILBUF (=1984) -  txBuf[LEN_TXBUF]     - накопитель исходящего потока SMTP, чтобы письмо уходило единицами пакетов, а не десятками
    //  ИТОГО занято ~4.3 кБ из 6 кБ. retTest[LEN_RETTEST] хранится отдельно (см. ниже).
    #define MAILBUF_OFFSET  (LEN_TEMPBUF+LEN_RETMAIL+LEN_RETSMS)
    char *tempBuf;                       // буфер для работы уведомлений, используется и на прием и на отправку
    char *retMail;                       // ответ сервера при отправке почты
    char *retSMS;                        // ответ сервера при отправке sms
    char *mailBuf;                       // рабочая область почты, размер LEN_MAILBUF
    char *txBuf;                         // накопитель исходящего потока SMTP, размер LEN_TXBUF
    uint16_t txLen;                      // сколько байт лежит в txBuf
    uint8_t  smtpCaps;                   // возможности сервера из ответа EHLO, биты fSMTP_*
    int16_t  smtpCode;                   // числовой код последнего ответа сервера
    boolean  smtpSemLost;                // семафор сети потерян: трогать сокет и отдавать семафор НЕЛЬЗЯ
    uint8_t  sendRetry;                  // сколько повторных попыток отправки письма уже сделано
    uint32_t sendRetryTime;              // unixtime, раньше которого повторять не начинаем
    boolean  smsSent;                    // SMS по текущему уведомлению уже ушло (чтобы повтор письма его не продублировал)
    char retTest[LEN_RETTEST];           // ПОСЛЕДНИЙ ответ от посылки тестового УВЕДОМЛЕНИЯ для экономии места он единый для писем и смс. К сожалению ответ возвращается в запросе от любого потока и поэтому должен храниться отдельно.
    uint16_t WorkFlags;
  };

#endif  
