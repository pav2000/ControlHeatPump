/*
 * Copyright (c) 2016-2018 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav; by vad711 (vad7@yahoo.com)
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
// --------------------------------------- функции общего использования -------------------------------------------
// Быстрый вывод в порт
 __attribute__((always_inline))  inline void digitalWriteDirect(int pin, boolean val)
{
  if(val) g_APinDescription[pin].pPort -> PIO_SODR = g_APinDescription[pin].ulPin;
  else    g_APinDescription[pin].pPort -> PIO_CODR = g_APinDescription[pin].ulPin;
}

// Быстрый чтение из порта
 __attribute__((always_inline)) inline int digitalReadDirect(int pin){
  return !!(g_APinDescription[pin].pPort -> PIO_PDSR & g_APinDescription[pin].ulPin);
}
// Функции работы с сетевыми переменными ----------------------------------
// Перевод IPAddress в массив байт
byte * IPAddressToBytes(IPAddress ip)
{
  static byte b[4];
  b[0]=ip[0]; b[1]=ip[1]; b[2]=ip[2]; b[3]=ip[3];
  return b; 
}

// Перевод  массив байт в IPAddress
IPAddress BytesToIPAddress(byte *ip)
{
  static IPAddress b;
  b[0]=ip[0]; b[1]=ip[1]; b[2]=ip[2]; b[3]=ip[3];
  return b; 
}
// разбор строки побайтно ОШИБКИ ПЛОХО не ловит!
//  для IP          const char* ipStr = "50.100.150.200"; byte ip[4]; parseBytes(ipStr, '.', ip, 4, 10);
//  для mac address const char* macStr = "90-A2-AF-DA-14-11"; byte mac[6]; parseBytes(macStr, '-', mac, 6, 16);
boolean parseBytes(const char* str, char sep, byte* bytes, int maxBytes, int base) 
{ int i,x;
  char y;
    for (i = 0; i < maxBytes; i++) {
        y=str[0];
        x = strtoul(str, NULL, base);          // Convert byte
        if (x>255) return false;               // Значение байта не верно
        if ((x==0)&&(y!='0')) return false;    // Значение байта не верно
        bytes[i]=x;
        str = strchr(str, sep);               // Find next separator
        if (str == NULL || *str == '\0') {
            break;                            // No more separators, exit
        }
        str++;                                // Point to next character after separator
    }
 if (i<maxBytes-1) return false; else return true;   
}
// разбор строки с разделителями в int16_t
boolean parseInt16_t(const char* str, char sep, int16_t* ints, int maxNum, int base) 
{ int i,x;
  char y;
    for (i = 0; i < maxNum; i++) {
        y=str[0];
        x = strtoul(str, NULL, base);          // Convert byte
        if ((x==0)&&(y!='0')) return false;    // Значение байта не верно
        ints[i]=x;
        str = strchr(str, sep);               // Find next separator
        if (str == NULL || *str == '\0') {
            break;                            // No more separators, exit
        }
        str++;                                // Point to next character after separator
    }
 if (i<maxNum-1) return false; else return true;   
}

// Разбор строки в IP адрес, если удачно то возвращает true, если не удачно то возвращает false и адрес не меняет
boolean parseIPAddress(const char* str, char sep, IPAddress &ip)
{   int i,x;
    char y;
    byte tmp[4];
    for (i = 0; i < 4; i++) 
        {
        y=str[0];  
        x= strtoul(str, NULL, 10);             // Convert byte
        if (x>255) return false;               // Значение байта не верно
        if ((x==0)&&(y!='0')) return false;    // Значение байта не верно
        tmp[i]=x;
        str = strchr(str, sep);               // Find next separator
        if (str == NULL || *str == '\0') {
            break;                            // No more separators, exit
        }
        str++;                                // Point to next character after separator
    }
 if (i<4-1) return false; else { ip=tmp;return true; }  
}

///////////////////////////////////////////////////////////////////////////////////
// Функции ниже использовать только в WebServer или с семафором xWebThreadSemaphore!
char _buf[16+1];
// IP адрес в строку
char *IPAddress2String(IPAddress & ip) 
{
  char * last = _buf;
  for (int8_t i = 0; i < 4; i++) {
    itoa(ip[i], last, 10);
    last = last + strlen(last);
    if (i == 3) *last = '\0'; else *last++ = '.';
  }
  return _buf;
}

// MAC  адрес в строку
char *MAC2String(byte* mac) 
{
  char * last = _buf;
  for (int8_t i = 0; i < 6; i++) {
    if(mac[i]<10) { *last='0'; last++;}  // поставить 0 в переди если число меньше 10
    itoa(mac[i], last, 16);
    last = last + strlen(last);
    if (i == 6-1) *last = '\0'; else *last++ = ':';
  }
  return _buf;
}

/// Адрес температурного датчика (шесть байт) в строку в шеснадцатеричном виде
const char codeHex[]={"0123456789ABCDEF"};
char* addressToHex(byte *f)
{ 
  strcpy(_buf,"");             // очистить строку
  for(int i=0;i<8;i++)
  {
	_buf[i*2]=codeHex[0x0f & (unsigned char)(f[i]>>4)];
	_buf[i*2+1]=codeHex[0x0f & (unsigned char)(f[i])];
  }
 _buf[sizeof(_buf)-1]=0; // Конец строки
 return _buf;
}

// Байт в текстовую строку вида 0xf5
char* byteToHex(byte f)
{ 
 strcpy(_buf,"");             // очистить строку
_buf[0]='0';
_buf[1]='x';
_buf[2]=codeHex[0x0f & (unsigned char)(f>>4)];
_buf[3]=codeHex[0x0f & (unsigned char)(f)];
_buf[4]=0; // Конец строки
return _buf;
}

// uint16_t в текстовую строку вида 0xf50e
char* uint16ToHex(uint16_t f)
{ 
 strcpy(_buf,"");             // очистить строку
_buf[0]='0';
_buf[1]='x';
_buf[2]=codeHex[0x0f & (unsigned char)(highByte(f)>>4)];
_buf[3]=codeHex[0x0f & (unsigned char)(highByte(f))];
_buf[4]=codeHex[0x0f & (unsigned char)(lowByte(f)>>4)];
_buf[5]=codeHex[0x0f & (unsigned char)(lowByte(f))];
_buf[6]=0; // Конец строки
return _buf;
}

// uint32_t в текстовую строку вида 0xabcdf50e
char* uint32ToHex(uint32_t f)
{ 
 strcpy(_buf,"");             // очистить строку
 unsigned char *ptr;
 ptr=(unsigned char*)&f;
_buf[0]='0';
_buf[1]='x';
_buf[2]=codeHex[0x0f & (unsigned char)(ptr[3]>>4)];
_buf[3]=codeHex[0x0f & (unsigned char)(ptr[3])];
_buf[4]=codeHex[0x0f & (unsigned char)(ptr[2]>>4)];
_buf[5]=codeHex[0x0f & (unsigned char)(ptr[2])];
_buf[6]=codeHex[0x0f & (unsigned char)(ptr[1]>>4)];
_buf[7]=codeHex[0x0f & (unsigned char)(ptr[1])];
_buf[8]=codeHex[0x0f & (unsigned char)(ptr[0]>>4)];
_buf[9]=codeHex[0x0f & (unsigned char)(ptr[0])];
_buf[10]=0; // Конец строки
return _buf;
}
// Функции выше использовать только в WebServer или с семафором xWebThreadSemaphore!
///////////////////////////////////////////////////////////////////////////////////

// *char to float - экономим место и скорость ---------------------------------------------------------------------
// При ошибке возвращает магическое число -9876543.00
//#define ATOF_ERROR -9876543.00
float my_atof(const char* s){
  float rez = 0, fact = 1;
  if (*s == '-') { s++;   fact = -1; }; // Знак минус
  for (int point_seen = 0; *s; s++){
    if (*s == '.'){  point_seen = 1;  continue;  }; // Дробная часть
    int d = *s - '0';
    if (d >= 0 && d <= 9){
      if (point_seen) fact /= 10.0f;
      rez = rez * 10.0f + (float)d;
    }
    else return ATOF_ERROR;   // ошибка преобразования
  };
  return rez * fact;
};

/*
// float в *char  экономим место и скорость и стек -----------------------------------
char * ftoa(char * outstr, float val, byte precision){
 byte i;
 // compute the rounding factor and fractional multiplier
 float roundingFactor = 0.5;
 unsigned long mult = 1;
 for (i = 0; i < precision; i++)
 {
   roundingFactor /= 10.0;
   mult *= 10;
 }
 
 outstr[0]='\0';

 if(val < 0.0){
   strcpy(outstr,"-\0");
   val = -val;
 }

 val += roundingFactor;
 itoa(int(val), outstr + strlen(outstr), 10);
 if( precision > 0) {
   strcat(outstr, ".\0"); // print the decimal point
   unsigned long frac;
   unsigned long mult = 1;
   byte padding = precision -1;
   while(precision--)
     mult *=10;

   if(val >= 0)
     frac = (val - int(val)) * mult;
   else
     frac = (int(val)- val ) * mult;
   unsigned long frac1 = frac;

   while(frac1 /= 10)  padding--;

   while(padding--)  strcat(outstr,"0\0");

   itoa(frac, outstr + strlen(outstr), 10);
 }
 return outstr;
}
*/

// Преобразование во float двух слов из двух байт
float fromInt16ToFloat(uint16_t lowInt, uint16_t highInt)
{
	union  float_int {
		float f;
		uint16_t i[2];
	} float_map;

	float_map.i[0]=highInt;
	float_map.i[1]=lowInt;
	return float_map.f;
}

// int to str - для уменьшения кода и увеличения быстродействия ---------------------------------------------------
char _int2str[12];
char *int2str(register int i) 
{
 itoa(i,_int2str,10);
 return _int2str;
}

#include <malloc.h>
extern char _end;
extern "C" char *sbrk(int i);
void freeRamShow()
{
  char *ramstart = (char *) 0x20070000;
  char *ramend = (char *) 0x20088000;
  char *heapend = sbrk(0);
  register char * stack_ptr asm( "sp" );
  struct mallinfo mi = mallinfo();
 
  journal.jprintf("Ram used (bytes):\n");
  journal.jprintf("  dynamic: %d\n",mi.uordblks); 
  journal.jprintf("  static:  %d\n",&_end - ramstart); 
  journal.jprintf("  stack:   %d\n",ramend - stack_ptr); 
  journal.jprintf("Estimation free Ram: %d\n",stack_ptr - heapend + mi.fordblks);
 
}

int freeRam()
{
//  char *ramstart = (char *) 0x20070000;
//  char *ramend = (char *) 0x20088000;
  char *heapend = sbrk(0);
  register char * stack_ptr asm( "sp" );
  struct mallinfo mi = mallinfo();
  // return 0x20088000-0x20070000-(&_end - ramstart + mi.fordblks);
 //  return ramend - stack_ptr-(&_end - ramstart + mi.fordblks);
 return stack_ptr - heapend + mi.fordblks;
}

// Чтение температуры чипа SAM3X ----------------------------------------------
//http://arduino.cc/forum/index.php/topic,129013.0.html
#define TEMP_TRANS       (float)(SAM3X_ADC_REF/4096.0)
#define TEMP_OFFSET      0.80
#define TEMP_FACTOR      0.00265
#define TEMP_FIXTEMP     27.0
float temp_DUE() 
{
  uint32_t ulValue = 0;
 
  adc_enable_channel(ADC, ADC_TEMPERATURE_SENSOR);                                                 // Enable the corresponding channel
  adc_enable_ts(ADC);                                                                              // Enable the temperature sensor
  adc_start(ADC);                                                                                  // Start the ADC
  //  while ((adc_get_status(ADC) & ADC_ISR_DRDY) != ADC_ISR_DRDY);                                // Wait for end of conversion
 while ((ADC->ADC_ISR & (0x1u << ADC_TEMPERATURE_SENSOR)) != (0x1u << ADC_TEMPERATURE_SENSOR));    // Wait for end of conversion
//    ulValue = adc_get_latest_value(ADC);  // Read the value
  ulValue =  (unsigned int)(*(ADC->ADC_CDR+ADC_TEMPERATURE_SENSOR)) ;                              // Read the value
  adc_disable_ts(ADC);
  adc_disable_channel(ADC, ADC_TEMPERATURE_SENSOR);                                                // Disable the corresponding channel
 
  //Serial.println(HP.AdcTempSAM3x);
  return TEMP_FIXTEMP +((float)(ulValue*TEMP_TRANS)-TEMP_OFFSET)/TEMP_FACTOR;
 // return TEMP_FIXTEMP +((float)(HP.AdcTempSAM3x*TEMP_TRANS)-TEMP_OFFSET)/TEMP_FACTOR;
}

// Включение монитора питания ---------------------------------------------------
void SupplyMonitorON(uint32_t voltage)
{
startSupcStatusReg=SUPC->SUPC_SR;                        // запомнить состояние при старте
journal.jprintf("Supply Controller Status Register [SUPC_SR]: 0x%08x\n",startSupcStatusReg);

SUPC->SUPC_SMMR |= voltage|SUPC_SMMR_SMSMPL_CSM;//SUPC_SMMR_SMSMPL_2048SLCK;  // Настройка контроля пититания
SUPC->SUPC_MR   |= SUPC_MR_KEY(SUPC_KEY_VALUE) | SUPC_MR_BODDIS_ENABLE;       // Включение контроля (это лишнее при сбросе это включено)
char buf[8];
switch (voltage)
     {
     case SUPC_SMMR_SMTH_1_9V: strcpy(buf,"1.9V"); break;
     case SUPC_SMMR_SMTH_2_0V: strcpy(buf,"2.0V"); break;
     case SUPC_SMMR_SMTH_2_1V: strcpy(buf,"2.1V"); break;
     case SUPC_SMMR_SMTH_2_2V: strcpy(buf,"2.2V"); break;
     case SUPC_SMMR_SMTH_2_3V: strcpy(buf,"2.3V"); break;
     case SUPC_SMMR_SMTH_2_4V: strcpy(buf,"2.4V"); break;
     case SUPC_SMMR_SMTH_2_5V: strcpy(buf,"2.5V"); break;
     case SUPC_SMMR_SMTH_2_6V: strcpy(buf,"2.6V"); break;
     case SUPC_SMMR_SMTH_2_7V: strcpy(buf,"2.7V"); break;
     case SUPC_SMMR_SMTH_2_8V: strcpy(buf,"2.8V"); break;
     case SUPC_SMMR_SMTH_2_9V: strcpy(buf,"2.9V"); break;
     case SUPC_SMMR_SMTH_3_0V: strcpy(buf,"3.0V"); break;
     case SUPC_SMMR_SMTH_3_1V: strcpy(buf,"3.1V"); break;
     case SUPC_SMMR_SMTH_3_2V: strcpy(buf,"3.2V"); break;
     case SUPC_SMMR_SMTH_3_3V: strcpy(buf,"3.3V"); break;
     case SUPC_SMMR_SMTH_3_4V: strcpy(buf,"3.4V"); break;
     default:    strcpy(buf,cError); break;    
     }
 journal.jprintf("Supply monitor ON, voltage: %s\n",buf);
}

// Программный сброс контроллера
void Software_Reset() {
  const int RSTC_KEY = 0xA5;
  RSTC->RSTC_CR = RSTC_CR_KEY(RSTC_KEY) | RSTC_CR_PROCRST | RSTC_CR_PERRST;
  while (true);
}

// Получить причину последнего сброса контроллера, wdt_addr=1 - адрес, где сработал watchdog
char* ResetCause(void)
{
	uint32_t resetCause = rstc_get_reset_cause(RSTC);
	// strcpy(_buf,"");
	switch ( resetCause )
	{
	case RSTC_GENERAL_RESET:  strcpy(_buf, "General");  break;
	case RSTC_BACKUP_RESET:   strcpy(_buf, "Backup");   break;
	case RSTC_WATCHDOG_RESET: strcpy(_buf, "Watchdog"); break;
	case RSTC_SOFTWARE_RESET: strcpy(_buf, "Software"); break;
	case RSTC_USER_RESET:     strcpy(_buf, "User");     break;
	default:                  strcpy(_buf, "Unknown");  break;
	}
	if((resetCause = lastErrorFreeRtosCode & 0xFF)) { // FreeRTOS error
		strcpy(_buf, "RTOS ");
		switch(resetCause)
		{
		case 1: strcat(_buf, "CONFIG");  break;
		case 2: strcat(_buf, "MALLOC");  break;
		case 3: strcpy(_buf, "MEM "); strncat(_buf, (char *)&GPBR->SYS_GPBR[1], 11);  break;
		case 4: strcat(_buf, "HARD FAULT");  break;
		case 5: strcat(_buf, "BUS FAULT");  break;
		case 6: strcat(_buf, "USAGE FAULT");  break;
		case 7: strcat(_buf, "TASK #");  break;
		}
	}
	return _buf;
}

#define CHIPiD_BASE_REG_ADDR 0x400E0940
#define CHIPiD_CIDR_OFFSET 0x0
#define CHIPiD_EXID_OFFSET 0x4
#define CHIPiD_CIDR   (*((volatile unsigned long *) (CHIPiD_BASE_REG_ADDR + CHIPiD_CIDR_OFFSET)))
#define CHIPiD_EXID   (*((volatile unsigned long *) (CHIPiD_BASE_REG_ADDR + CHIPiD_EXID_OFFSET)))
void showID() 
{
uint32_t test = 0x0;
test = CHIPiD_CIDR;
//test = CHIPiD_EXID;
journal.jprintf("Chip ID EXID: %d\n",test); 
}


//--------------------------------------------------------------
// Определение уникального номера чипа пишет в строку
#define EFC_ACCESS_MODE_128       0
#define FLASH_ACCESS_MODE_128    EFC_ACCESS_MODE_128
#define _EFC0_                   (0x400E0A00U)   // Возможно переопределение имен
#define EFC                      ((Efc*)EFC0)
char* getIDchip(char *outstr) 
  {
    char buf[10];  
    uint32_t unique_id[4]; 
    efc_init(EFC,FLASH_ACCESS_MODE_128,4);
    if (0 != efc_perform_read_sequence(EFC, EFC_FCMD_STUI, EFC_FCMD_SPUI, unique_id, 4)) { return  strcat(outstr,"FLASH_RC_ERROR");  }
    for (uint8_t i=0; i<4;i++) 
     {
      itoa(unique_id[i],buf, 16);
      strcat(outstr,buf);
      if (i<3) strcat(outstr,"-");
     }
return outstr;
}

// Преобразование строкового расписания в бинарный массив ----------------------------------------------------------------
// одно число один день по часам, массив 7 дней недели.
// входная строка разделитель часов "," разделитель дней ";"
// в случае успеха возвращает true
boolean set_Schedule(char *c, uint32_t *sh)
{
  uint32_t t[7];  
  uint16_t i,j, pos=0;

  if (strlen(c)<175) return false;                          // длина входной строки меньше  24*7+7=175 байт, явная ошибка
  for (i=0;i<7;i++)
  {
     t[i]=0;  
     for (j=0;j<24;j++)
     { 
       if (c[pos]=='1') t[i]=t[i]|(0x1u <<j);
       else if (c[pos]!='0') return false;                  // символ не ноль
       pos++;
     }
     if (c[pos]!='/')  return false;                        // не правильный разделитель
     pos++;
  }  
  // Дошли до сюда ошибок нет
  for (i=0;i<7;i++)  sh[i]=t[i];  // копируем результат
  return true;
}

// преревод расписания в символьный вид
char * get_Schedule( uint32_t *sh)
{
uint16_t i,j, pos=0;
static char buf[24*7+7+1];

for (i=0;i<7;i++)
{
   for (j=0;j<24;j++)
   { 
     if (((0x1u <<j)&sh[i])>0) buf[pos]='1'; else buf[pos]='0';
     pos++;
   }
  buf[pos]='/';                    // разделитель
  pos++; 
} 
buf[pos]=0; // конец строки
return buf;
}

// Bнициализация СД карты, параметр num - число попыток
// возврат true - если успешно, false - карты нет работаем без нее
boolean initSD(uint8_t num)
{
uint8_t i;
boolean success=false;   // флаг успешности инициализации

 journal.jprintf("Initializing SD card...\n");
 #ifdef NO_SD_CONTROL                // Если реализован механизм проверки наличия карты в слоте (выключатель в слоте карты)
   pinMode(PIN_NO_SD_CARD,INPUT);     // ++ CD Программирование проверки наличия карты
   if (digitalReadDirect(PIN_NO_SD_CARD)) { journal.jprintf("ERROR - No SD card in slot.\n"); return false;}
   else journal.jprintf("SUCCESS - SD card insert in slot.\n");
 #endif
 
// 1. Инициалазация карты
digitalWriteDirect(10, HIGH);

  #ifdef SD_LOW_SPEED            // Если этот дефайн то скорость для КАРТЫ понижается вдвое
     if (card.begin(4,SPI_HALF_SPEED))  // Половина скорости
  #else
     if (card.begin(4,SD_SPI_SPEED))    // Полная скорость
  #endif  
  for (i=0;i<num;i++) // Карта не инициализируется c ходу/ Дополнительные попытки инициализировать карту
  {
   WDT_Restart(WDT);  // это операция долгая, сбросить вачдог
   journal.jprintf("Repeat initializing SD card . . .\n");
   // пытаемся реанимировать СД карту
   digitalWriteDirect(10,HIGH);  
   SPI.end();                 // Stop SPI
   // MISO - Digital Pin 74, MOSI - Digital Pin 75, and SCK - Digital Pin 76.
   pinMode(74,INPUT_PULLUP);    // MISO - Digital Pin 74
   pinMode(75,INPUT_PULLUP);    // MOSI - Digital Pin 75
   pinMode(76,INPUT_PULLUP);    // SCK - Digital Pin 76.
   pinMode(10,INPUT_PULLUP);    // CS w5200 - Digital Pin 10.
   pinMode(4,INPUT_PULLUP);     // CS SD - Digital Pin 4.
   pinMode(87,INPUT_PULLUP);    // SD Pin 87
   pinMode(77,INPUT_PULLUP);    // Eth Pin 77  
   _delay(200);
   pinMode(74,OUTPUT);          // MISO - Digital Pin 74
   pinMode(75,OUTPUT);          // MOSI - Digital Pin 75
   pinMode(76,OUTPUT);          // SCK - Digital Pin 76.
 //  pinMode(10,OUTPUT);        // CS w5200 - Digital Pin 10.
   pinMode(4,OUTPUT);           // CS SD - Digital Pin 4.
   digitalWriteDirect(74, LOW);
   digitalWriteDirect(75, LOW);
   digitalWriteDirect(76, LOW);
  _delay(200);

  #ifdef SD_LOW_SPEED            // Если этот дефайн то скорость для КАРТЫ понижается вдвое
     if (card.begin(4,SPI_HALF_SPEED)){success=true; break;}  // Половина скорости
  #else
     if (card.begin(4,SD_SPI_SPEED)){success=true; break;}    // Полная скорость
  #endif   
  }
    else success=true;  // Карта инициализирована с первого раза
  
    if (success)  // Запоминаем результаты
      journal.jprintf("SUCCESS - SD card initialized.\n");
    else 
    {
    journal.jprintf((char*)"$ERROR - SD card initialization failed!\n");
    //  set_Error(ERR_SD_INIT,"SD card");   // Уведомить об ошибке
    HP.message.setMessage(pMESSAGE_SD,(char*)"Ошибка инициализации SD карты",0);    // сформировать уведомление
    SPI_switchW5200(); 
    return false;
    }    
  
// 2. Проверка наличия индексного файла
if (!card.exists(INDEX_FILE))
    {   
     journal.jprintf((char*)"$ERROR - Can't find index.xxx file!\n"); 
   //   set_Error(ERR_SD_INDEX,"SD card");   // Уведомить об ошибке
     HP.message.setMessage(pMESSAGE_SD,(char*)"Файл index.xxx не найден на SD карте",0);    // сформировать уведомление
     SPI_switchW5200(); 
     return false;    
     } // стартовый файл не найден
     journal.jprintf((char*)"SUCCESS - Found %s file\n",INDEX_FILE); 
      
// 3. Вывод инфо о карте
      cid_t cid;
      if (!card.card()->readCID(&cid))
      { 
        journal.jprintf((char*)"$ERROR - readCID SD card failed!\n"); 
        HP.message.setMessage(pMESSAGE_SD,(char*)"Ошибка чтения информации о карте (readCID)",0);    // сформировать уведомление
        SPI_switchW5200();
        return false;
      }
      // вывод информации о карте
      journal.jprintf((char*)"SD card info\n");
      journal.jprintf((char*)" Manufacturer ID: 0x%x\n",int(cid.mid));
      journal.jprintf((char*)" OEM ID: %c%c\n",cid.oid[0],cid.oid[1]);
      journal.jprintf((char*)" Serial number: 0x%x\n",int(cid.psn));
      journal.jprintf((char*)" Volume is FAT%d\n",int(card.vol()->fatType()));
      journal.jprintf((char*)" blocksPerCluster: %d\n",int(card.vol()->blocksPerCluster()));
      journal.jprintf((char*)" clusterCount: %d\n",card.vol()->clusterCount());
      uint32_t volFree = card.vol()->freeClusterCount();
      float fs = 0.000512*volFree*card.vol()->blocksPerCluster();
      journal.jprintf((char*)" freeSpace: %.2f Mb\n",fs);
  SPI_switchW5200(); 
  return true;
 }

// base64 -хеш функция ------------------------------------------------------------------------------------------------
/* Copyright (c) 2013 Adam Rudd. */
const char b64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                            "abcdefghijklmnopqrstuvwxyz"
                            "0123456789+/";

/* 'Private' declarations */
inline void a3_to_a4(unsigned char * a4, unsigned char * a3);
inline void a4_to_a3(unsigned char * a3, unsigned char * a4);
inline unsigned char b64_lookup(char c);

int base64_encode(char *output, char *input, int inputLen) {
  int i = 0, j = 0;
  int encLen = 0;
  unsigned char a3[3];
  unsigned char a4[4];

  while(inputLen--) {
    a3[i++] = *(input++);
    if(i == 3) {
      a3_to_a4(a4, a3);

      for(i = 0; i < 4; i++) {
        output[encLen++] = pgm_read_byte(&b64_alphabet[a4[i]]);
      }

      i = 0;
    }
  }

  if(i) {
    for(j = i; j < 3; j++) {
      a3[j] = '\0';
    }

    a3_to_a4(a4, a3);

    for(j = 0; j < i + 1; j++) {
      output[encLen++] = pgm_read_byte(&b64_alphabet[a4[j]]);
    }

    while((i++ < 3)) {
      output[encLen++] = '=';
    }
  }
  output[encLen] = '\0';
  return encLen;
}

int base64_decode(char * output, char * input, int inputLen) {
  int i = 0, j = 0;
  int decLen = 0;
  unsigned char a3[3];
  unsigned char a4[4];


  while (inputLen--) {
    if(*input == '=') {
      break;
    }

    a4[i++] = *(input++);
    if (i == 4) {
      for (i = 0; i <4; i++) {
        a4[i] = b64_lookup(a4[i]);
      }

      a4_to_a3(a3,a4);

      for (i = 0; i < 3; i++) {
        output[decLen++] = a3[i];
      }
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 4; j++) {
      a4[j] = '\0';
    }

    for (j = 0; j <4; j++) {
      a4[j] = b64_lookup(a4[j]);
    }

    a4_to_a3(a3,a4);

    for (j = 0; j < i - 1; j++) {
      output[decLen++] = a3[j];
    }
  }
  output[decLen] = '\0';
  return decLen;
}

int base64_enc_len(int plainLen) {
  int n = plainLen;
  return (n + 2 - ((n + 2) % 3)) / 3 * 4;
}

int base64_dec_len(char * input, int inputLen) {
  int i = 0;
  int numEq = 0;
  for(i = inputLen - 1; input[i] == '='; i--) {
    numEq++;
  }

  return ((6 * inputLen) / 8) - numEq;
}

inline void a3_to_a4(unsigned char * a4, unsigned char * a3) {
  a4[0] = (a3[0] & 0xfc) >> 2;
  a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
  a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
  a4[3] = (a3[2] & 0x3f);
}

inline void a4_to_a3(unsigned char * a3, unsigned char * a4) {
  a3[0] = (a4[0] << 2) + ((a4[1] & 0x30) >> 4);
  a3[1] = ((a4[1] & 0xf) << 4) + ((a4[2] & 0x3c) >> 2);
  a3[2] = ((a4[2] & 0x3) << 6) + a4[3];
}

inline unsigned char b64_lookup(char c) {
  if(c >='A' && c <='Z') return c - 'A';
  if(c >='a' && c <='z') return c - 71;
  if(c >='0' && c <='9') return c + 4;
  if(c == '+') return 62;
  if(c == '/') return 63;
  return -1;
}

/*
  Name  : CRC-16
  Poly  : 0x8005    x^16 + x^15 + x^2 + 1
  Init  : 0xFFFF
  Revert: true
  XorOut: 0x0000
  Check : 0x4B37 ("123456789")
  MaxLen: 4095 байт (32767 бит) - обнаружение одинарных, двойных, тройных и всех нечетных ошибок
*/
const uint16_t Crc16Table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

uint16_t calulate_crc16(unsigned char * pcBlock, unsigned short len)
{
    uint16_t crc = 0xFFFF;
    while (len--)
        crc = (crc >> 8) ^ Crc16Table[(crc & 0xFF) ^ *pcBlock++];
    return crc;
}

static uint16_t _crc16(uint16_t crc, uint8_t a)
{
  crc = (crc >> 8) ^ Crc16Table[(crc & 0xFF) ^ a];
  return crc;
} 


// Перевод строки в кодировке URL в UTF8, len - максимальная длина строки приемника
void urldecode(char *dst, char *src, uint16_t len)
{       char a, b;
        uint16_t i=0;
        while (*src) {
                if ((*src == '%') &&
                    ((a = src[1]) && (b = src[2])) &&
                    (isxdigit(a) && isxdigit(b))) {
                        if (a >= 'a')
                                a -= 'a'-'A';
                        if (a >= 'A')
                                a -= ('A' - 10);
                        else
                                a -= '0';
                        if (b >= 'a')
                                b -= 'a'-'A';
                        if (b >= 'A')
                                b -= ('A' - 10);
                        else
                                b -= '0';
                        *dst++ = 16*a+b; i++;
                        src+=3;
                } else if (*src == '+') {
                        *dst++ = ' '; i++;
                        src++;
                } else {
                        *dst++ = *src++; i++;
                }
           if (i == len-1) break;    // Не забываем про конец строки!
        }
        
       *dst = '\0';  // Добавить конец строки
}

// ---------------------------------------------------------------------------------------
// Функции работы с шиной I2C под free RTOS   На шине висят 3 устройства и шину надо делить
static byte _retEEPROM_I2C;
// Запись в eeprom, 0 - успешно
__attribute__((always_inline))  inline byte writeEEPROM_I2C(unsigned long addr, byte *values, unsigned int nBytes)
{
	if(SemaphoreTake(xI2CSemaphore, I2C_TIME_WAIT / portTICK_PERIOD_MS) == pdFALSE) {  // Если шедулер запущен то захватываем семафор
		journal.printf((char*) cErrorMutex, __FUNCTION__, MutexI2CBuzy);
		return 0;
	}
	_retEEPROM_I2C = eepromI2C.write(addr, values, nBytes);
	SemaphoreGive (xI2CSemaphore);
	return _retEEPROM_I2C;
}
// Чтение в eeprom, 0 - успешно
__attribute__((always_inline))   inline byte readEEPROM_I2C(unsigned long addr, byte *values, unsigned int nBytes)
{
	if(SemaphoreTake(xI2CSemaphore, I2C_TIME_WAIT / portTICK_PERIOD_MS) == pdFALSE) {  // Если шедулер запущен то захватываем семафор
		journal.printf((char*) cErrorMutex, __FUNCTION__, MutexI2CBuzy);
		return 0;
	}
	_retEEPROM_I2C = eepromI2C.read(addr, values, nBytes);
	SemaphoreGive (xI2CSemaphore);
	return _retEEPROM_I2C;
}
// ЧАСЫ  есть проблема - работают на прямую с i2c не через wire ----------------------------------
// Часы на I2C   Чтение температуры
__attribute__((always_inline)) inline float getTemp_RtcI2C()
{
	if(SemaphoreTake(xI2CSemaphore, I2C_TIME_WAIT / portTICK_PERIOD_MS) == pdFALSE) {  // Если шедулер запущен то захватываем семафор
		journal.printf((char*) cErrorMutex, __FUNCTION__, MutexI2CBuzy);
		return 0;
	}
	static volatile float ret = rtcI2C.temperature() / 100.0;
	SemaphoreGive (xI2CSemaphore);
	return ret;
}

// Часы на I2C   Чтение времени
tmElements_t ret_getTime_RtcI2C;
__attribute__((always_inline))   inline tmElements_t getTime_RtcI2C()
{
	if(SemaphoreTake(xI2CSemaphore, I2C_TIME_WAIT / portTICK_PERIOD_MS) == pdFALSE) {  // Если шедулер запущен то захватываем семафор
		journal.printf("getTime_RtcI2C %s", MutexI2CBuzy);
		return ret_getTime_RtcI2C;
	}
	rtcI2C.read(ret_getTime_RtcI2C);
	SemaphoreGive (xI2CSemaphore);
	return ret_getTime_RtcI2C;
}

// Часы на I2C   Установка времени
__attribute__((always_inline)) inline void setTime_RtcI2C(uint8_t hour, uint8_t min, uint8_t sec)
{
	if(SemaphoreTake(xI2CSemaphore, I2C_TIME_WAIT / portTICK_PERIOD_MS) == pdFALSE) {  // Если шедулер запущен то захватываем семафор
		journal.printf((char*) cErrorMutex, __FUNCTION__, MutexI2CBuzy);
		return;
	}
	rtcI2C.setTime(hour, min, sec);
	SemaphoreGive (xI2CSemaphore);
}

// Часы на I2C   Установка даты
__attribute__((always_inline)) inline void setDate_RtcI2C(uint8_t date, uint8_t mon, uint16_t year)
{
	if(SemaphoreTake(xI2CSemaphore, I2C_TIME_WAIT / portTICK_PERIOD_MS) == pdFALSE) {  // Если шедулер запущен то захватываем семафор
		journal.printf((char*) cErrorMutex, __FUNCTION__, MutexI2CBuzy);
		return;
	}
	rtcI2C.setDate(date, mon, year);
	SemaphoreGive (xI2CSemaphore);
}

// Заполняет и выбирает нужный элемент (нумерация c 0) для тега select
// Формат select: "Первый элемент:0;Второй элемент:0;Третий элемент:0;"
char *web_fill_tag_select(char *str, const char *select, uint8_t selected)
{
	char *outstr = str + m_strlen(str);
	strcat(outstr, select);
	do {
		if((outstr = strchr(++outstr, ';')) == NULL) break;
	} while(selected--);
	if(outstr != NULL) *(outstr-1) = '1';
	return str;
}
