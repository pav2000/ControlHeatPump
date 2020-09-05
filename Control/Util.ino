/*
 * Copyright (c) 2016-2020 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
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
#include "Util.h"
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

uint8_t calc_bits_in_mask(uint32_t mask)
{
	uint8_t bits = 0;
	while(mask) {
		bits += mask & 1;
		mask >>= 1;
	}
	return bits;
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

// Замена символа в строке
void str_replace(char *str, char find, char replace)
{
	while(*str) {
		if(*str == find) *str = replace;
		str++;
	}
}

///////////////////////////////////////////////////////////////////////////////////
// Функции ниже использовать только в WebServer или с семафором xWebThreadSemaphore!
char _buf[20];
// IP адрес в строку
char *IPAddress2String(IPAddress & ip) 
{
  *_buf = '\0';
  for(uint8_t i = 0; i < 4; i++) {
    _itoa(ip[i], _buf);
    if(i < 3) strcat(_buf, ".");
  }
  return _buf;
}

// MAC  адрес в строку
char *MAC2String(byte* mac) 
{
  m_snprintf(_buf, sizeof(_buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return _buf;
}

/// Адрес температурного датчика (шесть байт) в строку в шеснадцатеричном виде
const char codeHex[]={"0123456789ABCDEF"};
char* addressToHex(byte *f)
{
	uint8_t i = 0;
	for(; i < 8; i++) {
		_buf[i * 2] = codeHex[0x0f & (unsigned char) (f[i] >> 4)];
		_buf[i * 2 + 1] = codeHex[0x0f & (unsigned char) (f[i])];
	}
	_buf[i * 2] = 0; // Конец строки
	return _buf;
}

// Байт в текстовую строку вида 0xf5
char* byteToHex(byte f)
{ 
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
_buf[0]='0';
_buf[1]='x';
_buf[2]=codeHex[0x0f & (f>>28)];
_buf[3]=codeHex[0x0f & (f>>24)];
_buf[4]=codeHex[0x0f & (f>>20)];
_buf[5]=codeHex[0x0f & (f>>16)];
_buf[6]=codeHex[0x0f & (f>>12)];
_buf[7]=codeHex[0x0f & (f>>8)];
_buf[8]=codeHex[0x0f & (f>>4)];
_buf[9]=codeHex[0x0f & (f)];
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

// Decimal (дробное приведенное к целому) в строку, precision: 1..10000
void _dtoa(char *outstr, int val, int precision)
{
    while(*outstr) outstr++;
    dptoa(outstr, val, precision);
}

//float в *char в строку ДОБАВЛЯЕТСЯ значение экономим место и скорость и стек -----------------------------------
uint8_t _ftoa(char *outstr, float val, unsigned char precision)
{
    while(*outstr) outstr++;
	char *instr = outstr;

	// compute the rounding factor and fractional multiplier
	float roundingFactor = 0.5f;
	unsigned long mult = 1;
	unsigned char padding = precision;
	while(precision--) {
		roundingFactor /= 10.0f;
		mult *= 10;
	}
	if(val < 0.0f){
		*outstr++ = '-';
		val = -val;
	}
	val += roundingFactor;
	outstr += i10toa((long)val, outstr, 0);
	if(padding > 0) {
		*(outstr++) = '.';
		outstr += i10toa((val - (long)val) * mult, outstr, padding);
	}
	return outstr - instr;
}

//int в *char в строку ДОБАВЛЯЕТСЯ значение экономим место и скорость и стек radix=10
char* _itoa( int value, char *string)
{
	char *ret = string;
    while(*string) string++;

	char *pbuffer = string;
	unsigned char	negative = 0;

	if (value < 0) {
		negative = 1;
		value = -value;
	}

	/* This builds the string back to front ... */
	do {
		*(pbuffer++) = '0' + value % 10;
		value /= 10;
	} while (value > 0);

	if (negative)
		*(pbuffer++) = '-';

	*(pbuffer) = '\0';

	/* ... now we reverse it (could do it recursively but will
	 * conserve the stack space) */
	uint32_t len = (pbuffer - string);
	for (uint32_t i = 0; i < len / 2; i++) {
		char j = string[i];
		string[i] = string[len-i-1];
		string[len-i-1] = j;
	}

	return ret; 
}

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
	adc_enable_channel(ADC, ADC_TEMPERATURE_SENSOR);                                                 // Enable the corresponding channel
	adc_enable_ts(ADC);                                                                              // Enable the temperature sensor
	WDT_Restart(WDT);
	delayMicroseconds(100);
	adc_start(ADC);                                                                                  // Start the ADC
	uint32_t timeout = 1000000;
	while(!(ADC->ADC_ISR & (1 << ADC_TEMPERATURE_SENSOR)) && --timeout) ;   						// Wait for end of conversion
	uint32_t ulValue = (unsigned int)*(ADC->ADC_CDR + ADC_TEMPERATURE_SENSOR);                              		 // Read the value
	adc_disable_ts(ADC);
	adc_disable_channel(ADC, ADC_TEMPERATURE_SENSOR);                                                // Disable the corresponding channel
	return TEMP_FIXTEMP + (ulValue * TEMP_TRANS - TEMP_OFFSET) / TEMP_FACTOR;
}

// Включение монитора питания ---------------------------------------------------
void SupplyMonitorON(uint32_t voltage)
{
	startSupcStatusReg = SUPC->SUPC_SR;                        // запомнить состояние при старте
	journal.jprintf("Supply Controller Register [SUPC_SR]: 0x%08x\n", startSupcStatusReg);

	SUPC->SUPC_SMMR |= voltage | SUPC_SMMR_SMRSTEN_ENABLE | SUPC_SMMR_SMSMPL_CSM;   // RESET если напряжение просело, контроль 1/32768 сек
	SUPC->SUPC_MR |= SUPC_MR_KEY(SUPC_KEY_VALUE) | SUPC_MR_BODDIS_ENABLE; // Включение контроля (это лишнее при сбросе это включено)
	journal.jprintf("Supply monitor ON, voltage: %.1fV\n", (float) voltage / 10 + 1.9);
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
	switch ( resetCause ) {
		case RSTC_GENERAL_RESET:  strcpy(_buf, "General");  break;
		case RSTC_BACKUP_RESET:   strcpy(_buf, "Backup");   break;
		case RSTC_WATCHDOG_RESET: strcpy(_buf, "Watchdog"); break;
		case RSTC_SOFTWARE_RESET: strcpy(_buf, "Software"); break;
		case RSTC_USER_RESET:     strcpy(_buf, "User");     break;
		default:                  strcpy(_buf, "Unknown");  break;
	}
	if((resetCause = lastErrorFreeRtosCode & 0xFF)) { // FreeRTOS error
		strcpy(_buf, "RTOS ");
		switch(resetCause) {
			case 1: strcat(_buf, "CONFIG");  break;
			case 2: strcat(_buf, "MALLOC");  break;
			case 3: strcpy(_buf, "MEM "); strncat(_buf, (char *)&GPBR->SYS_GPBR[1], 10);  break;
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
enum {
	FLASH_RC_OK = 0,        //!< Operation OK
	FLASH_RC_YES = 1,       //!< Yes
	FLASH_RC_NO = 0,        //!< No
	FLASH_RC_ERROR = 0x10,  //!< General error
	FLASH_RC_INVALID,       //!< Invalid argument input
	FLASH_RC_NOT_SUPPORT = 0xFFFFFFFF    //!< Operation is not supported
};
#define EFC_ACCESS_MODE_128       0
#define FLASH_ACCESS_MODE_128    EFC_ACCESS_MODE_128
#define _EFC0_                   (0x400E0A00U)   // Возможно переопределение имен
#define EFC                      ((Efc*)EFC0)
void getIDchip(char *outstr)
{
	uint32_t unique_id[4];

	portDISABLE_INTERRUPTS();
	efc_init(EFC, FLASH_ACCESS_MODE_128, 4);
	if(efc_perform_read_sequence(EFC, EFC_FCMD_STUI, EFC_FCMD_SPUI, unique_id, 4) != FLASH_RC_OK) {
		unique_id[0] = 0;
	}
	portENABLE_INTERRUPTS();
	if(unique_id[0] == 0) {
		strcat(outstr, "FLASH_RC_ERROR");
	} else {
		for(uint8_t i = 0; i < 4; i++) {
			outstr += strlen(outstr);
			itoa(unique_id[i], outstr, 16);
			if(i < 3) strcat(outstr, "-");
		}
	}
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
char * get_Schedule(uint32_t *sh)
{
	uint16_t i, j, pos = 0;
	static char buf[24 * 7 + 7 + 1];

	for(i = 0; i < 7; i++) {
		for(j = 0; j < 24; j++) {
			if(((0x1u << j) & sh[i]) > 0) buf[pos] = '1';
			else buf[pos] = '0';
			pos++;
		}
		buf[pos] = '/';                    // разделитель
		pos++;
	}
	buf[pos] = 0; // конец строки
	return buf;
}

// Инициализация СД карты
// возврат true - если успешно, false - карты нет работаем без нее
uint8_t initSD(void)
{
	boolean success = false;   // флаг успешности инициализации
	journal.jprintf("Init SD card: ");
#ifdef NO_SD_CONTROL                // Если реализован механизм проверки наличия карты в слоте (выключатель в слоте карты)
	pinMode(PIN_NO_SD_CARD, INPUT);     // ++ CD Программирование проверки наличия карты
	if (digitalReadDirect(PIN_NO_SD_CARD)) {journal.jprintf("slot empty!\n"); return false;}
#endif
	// 1. Инициалазация карты
	if(!card.begin(PIN_SPI_CS_SD, SD_SCK_MHZ(SD_CLOCK))) {
		journal.jprintf("Error %d,%d!\n", card.cardErrorCode(), card.cardErrorData());
	} else success = true;  // Карта инициализирована с первого раза

	if(success)  // Запоминаем результаты
		journal.jprintf("OK\n");
	else {
		//set_Error(ERR_SD_INIT,"SD card");   // Уведомить об ошибке
		HP.message.setMessage(pMESSAGE_SD, (char*) "Ошибка инициализации SD карты", 0);    // сформировать уведомление
		SPI_switchW5200();
		return false;
	}

	// 2. Вывод инфо о карте
	cid_t cid;
	if(!card.card()->readCID(&cid)) {
		journal.jprintf((char*) "$ERROR - readCID SD card failed!\n");
		HP.message.setMessage(pMESSAGE_SD, (char*) "Ошибка чтения информации о карте (readCID)", 0); // сформировать уведомление
		SPI_switchW5200();
		return 0;
	}
	// вывод информации о карте
	journal.jprintf((char*) "SD card info\n");
	journal.jprintf((char*) " Manufacturer ID: 0x%x\n", int(cid.mid));
	journal.jprintf((char*) " OEM ID: %c%c\n", cid.oid[0], cid.oid[1]);
	journal.jprintf((char*) " Serial number: 0x%x\n", int(cid.psn));
	journal.jprintf((char*) " Volume is FAT%d\n", int(card.vol()->fatType()));
	journal.jprintf((char*) " blocksPerCluster: %d\n", int(card.vol()->blocksPerCluster()));
	journal.jprintf((char*) " clusterCount: %d\n", card.vol()->clusterCount());
	journal.jprintf((char*) " freeSpace: %.2f Mb\n", (float) 0.000512 * card.vol()->freeClusterCount() * card.vol()->blocksPerCluster());

	// 3. Проверка наличия индексного файла
	if(!card.exists(INDEX_FILE)) {
		journal.jprintf((char*) " ERROR - Can't find %s file on SD card!\n", INDEX_FILE);
		//   set_Error(ERR_SD_INDEX,"SD card");   // Уведомить об ошибке
		HP.message.setMessage(pMESSAGE_SD, (char*) "Файл индекса не найден на SD карте", 0); // сформировать уведомление
		SPI_switchW5200();
		return 1;
	} // стартовый файл не найден
	journal.jprintf((char*) " Found %s file\n", INDEX_FILE);

	SPI_switchW5200();
	return 2;
}

// Инициализация SPI диска и проверка наличия веб морды (файл индекс), параметр true - вывод инфо в журнал false молча проверяем состояние
// Возврат true - если морда есть на карте (готово к использованию), false - диск не рабоатет или нет морды (использовать нельзя)
uint8_t initSpiDisk(boolean show)
{
#ifdef SPI_FLASH
	unsigned char id[8];
	if(!SerialFlash.begin(PIN_SPI_CS_FLASH)) {
		if(show) journal.jprintf(" SPI flash not found!\n");
		return false;
	} else {
		if(show) {
			SerialFlash.readID(id);
			journal.jprintf(" Manufacturer ID: 0x%02X\n", id[0]);
			journal.jprintf(" Memory type: 0x%02X\n", id[1]);
			journal.jprintf(" Chip size(0x%02X): %d bytes\n", id[2], SerialFlash.Capacity);
			journal.jprintf(" Free: %d bytes\n", SerialFlash.free_size());
			SerialFlash.readSerialNumber(id);
			journal.jprintf(" Serial number: 0x%02x%02x%02x%02x%02x%02x%02x%02x\n", id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7]);
		}
		if (HP.get_WebStoreOnSPIFlash()) { // проверка наличия файла INDEX_FILE, если стоит флаг загрузки из флеша в настройках
			if (!SerialFlash.exists((char*)INDEX_FILE)) { // файл не найден морды во флеше нет
				HP.set_WebStoreOnSPIFlash(false);//  сбрасываем в оперативке (но не на флеше) флаг загрузки из флеш диска
				if(show) journal.jprintf((char*) " ERROR - Can't find %s file on SPI flash!\n", INDEX_FILE);
				HP.message.setMessage(pMESSAGE_SD, (char*) "Файл индекса не найден на SPI флеш диске", 0);// сформировать уведомление
				return 1;
			} else if(show) journal.jprintf((char*) " Found %s file\n", INDEX_FILE);	// файл найден
		} // if HP.get_fSPIFlash()

		return 2;
	}
#endif
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

// defaults crc = 0xFFFF
uint16_t calc_crc16(unsigned char * pcBlock, unsigned short len, uint16_t crc)
{
    while (len--)
        crc = (crc >> 8) ^ Crc16Table[(crc & 0xFF) ^ *pcBlock++];
    return crc;
}

static uint16_t _crc16(uint16_t crc, uint8_t a)
{
  crc = (crc >> 8) ^ Crc16Table[(crc & 0xFF) ^ a];
  return crc;
} 


// Перевод строки в кодировке URL в UTF8, len - максимальная длина строки приемника + '\0'
void urldecode(char *dst, char *src, uint16_t len)
{       char a, b;
        uint16_t i=0;
        while (*src) {
			if (*src == '%') {
				a = src[1];
				b = src[2];
				if(a >= '0' && a <= '9') a -= '0';
				else {
					a &= ~0x20;
					if(a >= 'A' && a <= 'F') a -= 'A' - 10; else goto xCopyChar;
				}
				if(b >= '0' && b <= '9') b -= '0';
				else {
					b &= ~0x20;
					if(b >= 'A' && b <= 'F') b -= 'A' - 10; else goto xCopyChar;
				}
				*dst++ = 16*a+b; i++;
				src+=3;
			} else if (*src == '+') {
				*dst++ = ' '; i++;
				src++;
			} else {
xCopyChar:
				*dst++ = *src++; i++;
			}
			if (i >= len-1) break;    // Не забываем про конец строки!
        }
        
       *dst = '\0';  // Добавить конец строки
}

// ---------------------------------------------------------------------------------------
// Функции работы с шиной I2C под free RTOS   На шине висят 3 устройства и шину надо делить
// Запись в eeprom, 0 - успешно
__attribute__((always_inline))  inline byte writeEEPROM_I2C(unsigned long addr, byte *values, unsigned int nBytes)
{
	if(SemaphoreTake(xI2CSemaphore, I2C_TIME_WAIT / portTICK_PERIOD_MS) == pdFALSE) {  // Если шедулер запущен то захватываем семафор
		journal.printf((char*) cErrorMutex, __FUNCTION__, MutexI2CBuzy);
		return 7;
	}
	uint32_t _ret = eepromI2C.write(addr, values, nBytes);
	SemaphoreGive(xI2CSemaphore);
	return _ret;
}
// Чтение в eeprom, 0 - успешно
__attribute__((always_inline))   inline byte readEEPROM_I2C(unsigned long addr, byte *values, unsigned int nBytes)
{
	if(SemaphoreTake(xI2CSemaphore, I2C_TIME_WAIT / portTICK_PERIOD_MS) == pdFALSE) {  // Если шедулер запущен то захватываем семафор
		journal.printf((char*) cErrorMutex, __FUNCTION__, MutexI2CBuzy);
		return 7;
	}
	uint32_t _ret = eepromI2C.read(addr, values, nBytes);
	SemaphoreGive(xI2CSemaphore);
	return _ret;
}
// ЧАСЫ  есть проблема - работают на прямую с i2c не через wire ----------------------------------
// Часы на I2C   Чтение температуры
__attribute__((always_inline)) inline int16_t getTemp_RtcI2C()
{
	if(SemaphoreTake(xI2CSemaphore, I2C_TIME_WAIT / portTICK_PERIOD_MS) == pdFALSE) {  // Если шедулер запущен то захватываем семафор
		journal.printf((char*) cErrorMutex, __FUNCTION__, MutexI2CBuzy);
		return ERROR_TEMPERATURE;
	}
	int16_t rtc_temp = rtcI2C.temperature();
	SemaphoreGive(xI2CSemaphore);
	return rtc_temp;
}

// Часы на I2C   Чтение времени
tmElements_t ret_getTime_RtcI2C;
__attribute__((always_inline))   inline tmElements_t *getTime_RtcI2C()
{
	if(SemaphoreTake(xI2CSemaphore, I2C_TIME_WAIT / portTICK_PERIOD_MS) == pdFALSE) {  // Если шедулер запущен то захватываем семафор
		journal.printf("getTime_RtcI2C %s", MutexI2CBuzy);
		ret_getTime_RtcI2C.Year = 0;
	} else {
		if(rtcI2C.read(ret_getTime_RtcI2C)) ret_getTime_RtcI2C.Year = 0;
		SemaphoreGive(xI2CSemaphore);
	}
	return &ret_getTime_RtcI2C;
}

// Часы на I2C   Установка времени
__attribute__((always_inline)) inline void setTime_RtcI2C(uint8_t hour, uint8_t min, uint8_t sec)
{
	if(SemaphoreTake(xI2CSemaphore, I2C_TIME_WAIT / portTICK_PERIOD_MS) == pdFALSE) {  // Если шедулер запущен то захватываем семафор
		journal.printf((char*) cErrorMutex, __FUNCTION__, MutexI2CBuzy);
		return;
	}
	rtcI2C.setTime(hour, min, sec);
	SemaphoreGive(xI2CSemaphore);
}

// Часы на I2C   Установка даты
__attribute__((always_inline)) inline void setDate_RtcI2C(uint8_t date, uint8_t mon, uint16_t year)
{
	if(SemaphoreTake(xI2CSemaphore, I2C_TIME_WAIT / portTICK_PERIOD_MS) == pdFALSE) {  // Если шедулер запущен то захватываем семафор
		journal.printf((char*) cErrorMutex, __FUNCTION__, MutexI2CBuzy);
		return;
	}
	rtcI2C.setDate(date, mon, year);
	SemaphoreGive(xI2CSemaphore);
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

// Сохраняет структуру настроек: <длина><структура>, считает CRC, возвращает ошибку или OK
int8_t save_struct(uint32_t &addr_to, uint8_t *addr_from, uint16_t size, uint16_t &crc)
{
	if(size < 128) { // Меньше 128 байт, длина 1 байт, первый бит = 0
		size <<= 1;
		if(writeEEPROM_I2C(addr_to++, (uint8_t *)&size, 1)) {
			set_Error(ERR_SAVE_EEPROM, (char*)nameHeatPump);
			return ERR_SAVE_EEPROM;
		}
		crc = _crc16(crc, size);
	} else { // длина 2 байта, первый бит = 1
		size = (size << 1) | 1;
		if(writeEEPROM_I2C(addr_to, (uint8_t *)&size, 2)) {
			set_Error(ERR_SAVE_EEPROM, (char*)nameHeatPump);
			return ERR_SAVE_EEPROM;
		}
		addr_to += 2;
		crc = _crc16(crc, size & 0xFF);
		crc = _crc16(crc, size >> 8);
	}
	size >>= 1;
	if(writeEEPROM_I2C(addr_to, addr_from, size)) {
		set_Error(ERR_SAVE_EEPROM, (char*)nameHeatPump);
		return ERR_SAVE_EEPROM;
	}
	addr_to += size;
	while(size--) crc = _crc16(crc, *addr_from++);
	return OK;
}

int8_t save_2bytes(uint32_t &addr_to, uint16_t data, uint16_t &crc)
{
	if(writeEEPROM_I2C(addr_to, (uint8_t *)&data, 2)) {
		set_Error(ERR_SAVE_EEPROM, (char*)nameHeatPump);
		return ERR_SAVE_EEPROM;
	}
	addr_to += 2;
	crc = _crc16(crc, data & 0xFF);
	crc = _crc16(crc, data >> 8);
	return OK;
}

// memcpy: <size[byte: 1|2]><struct>
void load_struct(void *to, uint8_t **from, uint16_t to_size)
{
	uint16_t size = *((uint16_t *)*from);
	if((size & 1) == 0) size &= 0xFF; else (*from)++;
	(*from)++;
	size >>= 1;
	if(to != NULL) memcpy(to, *from, size <= to_size ? size : to_size);
	*from += size;
}

// Round float * mul, mul: 10, 100, 1000
int16_t rd(float num, int16_t mul)
{
	return num * mul + (mul == 1000 ? 0.0005 : mul == 100 ? 0.005 : mul == 10 ? 0.05 : 0.0005) * (num < 0 ? -1 : 1);
}

// format int/div value to string with decimal point
void int_to_dec_str(int32_t value, int32_t div, char **ret, uint8_t maxfract)
{
	*ret += m_itoa(value / div, *ret, 10, 0);
	if(div > 1 && maxfract) {
		value = abs(value) % div;
		if(value == 0) return;
		**ret = '.';
		m_itoa(value, ++*ret, 10, maxfract);
		*ret += maxfract;
		**ret = '\0'; // max after dot
	}
}

// округление к ближайшему целому, div: 10 / 100 / 1000 / 10000
int32_t round_div_int32(int32_t value, int16_t div)
{
	if(value >= 0) {
		if(value % div >= (div >> 1)) value = value / div + 1; else value /= div;
	} else {
		if(value % div <= -(div >> 1)) value = value / div - 1; else value /= div;
	}
	return value;
}

inline uint32_t mapResolution(uint32_t value, uint32_t from, uint32_t to) {
	if (from == to)
		return value;
	if (from > to)
		return value >> (from-to);
	else
		return value << (to-from);
}

// PWM output to PWM/TIMER pins
void PWM_Write(uint32_t ulPin, uint32_t ulValue) {
	uint32_t attr = g_APinDescription[ulPin].ulPinAttribute;
	if ((attr & PIN_ATTR_PWM) == PIN_ATTR_PWM) {
		if (!PWMEnabled) {
			// PWM Startup code
		    pmc_enable_periph_clk(PWM_INTERFACE_ID);
		    PWMC_ConfigureClocks(PWM_WRITE_OUT_FREQUENCY * ((1<<PWM_WRITE_OUT_RESOLUTION)-1), 0, VARIANT_MCK);
			PWMEnabled = 1;
		}
		uint32_t chan = g_APinDescription[ulPin].ulPWMChannel;
		if ((g_pinStatus[ulPin] & 0xF) != PIN_STATUS_PWM) {
			// Setup PWM for this pin
			PIO_Configure(g_APinDescription[ulPin].pPort,
					g_APinDescription[ulPin].ulPinType,
					g_APinDescription[ulPin].ulPin,
					g_APinDescription[ulPin].ulPinConfiguration);
			PWMC_ConfigureChannel(PWM_INTERFACE, chan, PWM_CMR_CPRE_CLKA, 0, 0);
			PWMC_SetPeriod(PWM_INTERFACE, chan, PWM_MAX_DUTY_CYCLE);
			PWMC_SetDutyCycle(PWM_INTERFACE, chan, ulValue);
			PWMC_EnableChannel(PWM_INTERFACE, chan);
			g_pinStatus[ulPin] = (g_pinStatus[ulPin] & 0xF0) | PIN_STATUS_PWM;
		}
		PWMC_SetDutyCycle(PWM_INTERFACE, chan, ulValue);
	} else if ((attr & PIN_ATTR_TIMER) == PIN_ATTR_TIMER) {
		// We use MCLK/2 as clock.
		const uint32_t TC = VARIANT_MCK / 2 / PWM_WRITE_OUT_FREQUENCY;
		// Map value to Timer ranges 0..RES => 0..TC
		ulValue = ulValue * TC;
		ulValue = ulValue / ((1<<PWM_WRITE_OUT_RESOLUTION)-1);
		// Setup Timer for this pin
		ETCChannel channel = g_APinDescription[ulPin].ulTCChannel;
		static const uint32_t channelToChNo[] = { 0, 0, 1, 1, 2, 2, 0, 0, 1, 1, 2, 2, 0, 0, 1, 1, 2, 2 };
		static const uint32_t channelToAB[]   = { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
		static Tc *channelToTC[] = {
			TC0, TC0, TC0, TC0, TC0, TC0,
			TC1, TC1, TC1, TC1, TC1, TC1,
			TC2, TC2, TC2, TC2, TC2, TC2 };
		static const uint32_t channelToId[]   = { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8 };
		uint32_t chNo = channelToChNo[channel];
		uint32_t chA  = channelToAB[channel];
		Tc *chTC = channelToTC[channel];
		uint32_t interfaceID = channelToId[channel];
		if (!TCChanEnabled[interfaceID]) {
			pmc_enable_periph_clk(TC_INTERFACE_ID + interfaceID);
			TC_Configure(chTC, chNo,
				TC_CMR_TCCLKS_TIMER_CLOCK1 |
				TC_CMR_WAVE |			// Waveform mode
				TC_CMR_WAVSEL_UP_RC |	// Counter running up and reset when equals to RC
				TC_CMR_ACPA_CLEAR | TC_CMR_ACPC_CLEAR |
				TC_CMR_BCPB_CLEAR | TC_CMR_BCPC_CLEAR |
#ifdef WR_ONE_PERIOD_PWM
				WR_ZERO_CROSS_EDGE |	// External Event Edge Selection
				TC_CMR_ENETRG |			// External Event Trigger Enable
				WR_ZERO_CROSS_TC_CMR_EEVT // Set external events
#else
				TC_CMR_EEVT_XC0 		// Set external events from XC0 (this setup TIOB as output)
#endif
			);
#ifdef WR_ONE_PERIOD_PWM
			WR_ZERO_CROSS_TC_BMR_SET;
		    PIO_SetPeripheral(g_APinDescription[PIN_PWM_ZERO_CROSS].pPort, WR_ZERO_CROSS_PERIPH, g_APinDescription[PIN_PWM_ZERO_CROSS].ulPin); // Set as input trigger
#endif
		    TC_SetRC(chTC, chNo, TC);
		}
		uint32_t cmr;
		if(chA)	{
			cmr = chTC->TC_CHANNEL[chNo].TC_CMR & ~(TC_CMR_ACPA_Msk | TC_CMR_ACPC_Msk | TC_CMR_AEEVT_Msk);
			if(ulValue == 0) {
				chTC->TC_CHANNEL[chNo].TC_CMR = cmr | TC_CMR_ACPA_SET | TC_CMR_ACPC_SET
#ifdef WR_ONE_PERIOD_PWM
												| TC_CMR_AEEVT_SET
#endif
																	;
			} else {
				TC_SetRA(chTC, chNo, ulValue);
				chTC->TC_CHANNEL[chNo].TC_CMR = cmr | TC_CMR_ACPA_SET | TC_CMR_ACPC_CLEAR
#ifdef WR_ONE_PERIOD_PWM
												| TC_CMR_AEEVT_CLEAR
#endif
																	;
			}
		} else {
			cmr = chTC->TC_CHANNEL[chNo].TC_CMR & ~(TC_CMR_BCPB_Msk | TC_CMR_BCPC_Msk | TC_CMR_BEEVT_Msk);
			if(ulValue == 0) {
				chTC->TC_CHANNEL[chNo].TC_CMR = cmr | TC_CMR_BCPB_SET | TC_CMR_BCPC_SET
#ifdef WR_ONE_PERIOD_PWM
												| TC_CMR_BEEVT_SET
#endif
																	;
			} else {
				TC_SetRB(chTC, chNo, ulValue);
				chTC->TC_CHANNEL[chNo].TC_CMR = cmr | TC_CMR_BCPB_SET | TC_CMR_BCPC_CLEAR
#ifdef WR_ONE_PERIOD_PWM
												| TC_CMR_BEEVT_CLEAR
#endif
																	;
			}
		}
		if ((g_pinStatus[ulPin] & 0xF) != PIN_STATUS_PWM) {
			PIO_Configure(g_APinDescription[ulPin].pPort,
					g_APinDescription[ulPin].ulPinType,
					g_APinDescription[ulPin].ulPin,
					g_APinDescription[ulPin].ulPinConfiguration);
			g_pinStatus[ulPin] = (g_pinStatus[ulPin] & 0xF0) | PIN_STATUS_PWM;
		}
		if (!TCChanEnabled[interfaceID]) {
			TC_Start(chTC, chNo);
			TCChanEnabled[interfaceID] = 1;
		}
	}
}

#ifdef WATTROUTER
void WR_Switch_Load(uint8_t idx, boolean On)
{
	int8_t pin = WR_Load_pins[idx];
	if(pin < 0) { // HTTP
		strcpy(Socket[MAIN_WEB_TASK].outBuf, HTTP_MAP_RELAY_SW_1);
		_itoa(abs(pin), Socket[MAIN_WEB_TASK].outBuf + sizeof(HTTP_MAP_RELAY_SW_1)-1);
		strcat(Socket[MAIN_WEB_TASK].outBuf + sizeof(HTTP_MAP_RELAY_SW_1)-1, HTTP_MAP_RELAY_SW_2);
		_itoa(On, Socket[MAIN_WEB_TASK].outBuf + sizeof(HTTP_MAP_RELAY_SW_1)-1 + sizeof(HTTP_MAP_RELAY_SW_2)-1);
		if(Send_HTTP_Request(HTTP_MAP_Server, Socket[MAIN_WEB_TASK].outBuf, 3) == 1) { // Ok?
			goto xSwitched;
		} else {
			if(GETBIT(WR.Flags, WR_fLog)) journal.jprintf("WR: Error set R%d\n", idx + 1);
		}
	} else {
		digitalWriteDirect(pin, On ? WR_RELAY_LEVEL_ON : !WR_RELAY_LEVEL_ON);
xSwitched:
		if((WR_LoadRun[idx] > 0) != On) WR_SwitchTime[idx] = WR_LastSwitchTime = rtcSAM3X8.unixtime();
		WR_LoadRun[idx] = On ? WR.LoadPower[idx] : 0;
		if(GETBIT(WR.Flags, WR_fLog)) journal.jprintf_time("WR: R%d=>%d\n", idx + 1, On);
	}
}

void WR_Change_Load_PWM(uint8_t idx, int16_t delta)
{
	int n = WR_LoadRun[idx] + delta;
	if(n <= 0) n = 0; else if(n > WR.LoadPower[idx]) n = WR.LoadPower[idx];
	uint32_t t = rtcSAM3X8.unixtime();
	if(WR.PWM_FullPowerTime) {
		if(n > 0) {
			int max = int(WR.LoadPower[idx]) * WR.PWM_FullPowerLimit / 100;
			if(n > max) {
				if(WR_LoadRun[idx] <= max) {
					if(t - WR_SwitchTime[idx] <= WR.PWM_FullPowerTime * 60) n = max; // Включаемся, но еще не остыли
					else WR_SwitchTime[idx] = t;
				} else if(WR_SwitchTime[idx] && t - WR_SwitchTime[idx] > WR.PWM_FullPowerTime * 60) n = max; // Перегрелись
			} else if(n < max && WR_LoadRun[idx] > max) WR_SwitchTime[idx] = t;
		} else if(WR_LoadRun[idx]) WR_SwitchTime[idx] = t;
	} else if(WR_LoadRun[idx] != n) WR_SwitchTime[idx] = t;
	if(n != WR_LoadRun[idx] || GETBIT(WR_Refresh, idx)) {
		WR_LoadRun[idx] = n;
		if(GETBIT(WR.Flags, WR_fLogFull)) journal.jprintf_time("WR: P%d=%d\n", idx + 1, n);
#ifdef PWM_ACCURATE_POWER
		n = n * (sizeof(PWM_POWER_ARRAY)/sizeof(PWM_POWER_ARRAY[0])-1)*10 / WR.LoadPower[idx]; // 0..100
		int r = n % 10;
		int val = PWM_POWER_ARRAY[n /= 10];
		if(n < (int)(sizeof(PWM_POWER_ARRAY)/sizeof(PWM_POWER_ARRAY[0]))-1) val += (PWM_POWER_ARRAY[n + 1] - val) * r / 10;
		PWM_Write(WR_Load_pins[idx], val);
#else
		PWM_Write(WR_Load_pins[idx], ((1<<PWM_WRITE_OUT_RESOLUTION)-1) - n * ((1<<PWM_WRITE_OUT_RESOLUTION)-1) / WR.LoadPower[idx]);
#endif
	}
}

inline int16_t WR_Adjust_PWM_delta(uint8_t idx, int16_t delta)
{
	if(delta != 0) {
		int16_t m = WR.LoadPower[idx] >> PWM_WRITE_OUT_RESOLUTION;
		if(delta < 0) {
			m = -m;
			if(m < delta) return m;
		} else if(m > delta) return m;
	}
	return delta;
}

#ifdef HTTP_MAP_Read_MPPT
// Проверка наличия свободного солнца
// 0 - Oшибка, 1 - Нет свободной энергии, 2 - Нужна пауза, 3 - Есть свободная энергия
uint8_t WR_Check_MPPT(void)
{
	int err = Send_HTTP_Request(HTTP_MAP_Server, HTTP_MAP_Read_MPPT, 1);
	if(err) {
		if(GETBIT(WR.Flags, WR_fLog)) journal.jprintf("WR: MPPT request Error %d\n", err);
		return 0;
	}
	char *fld = strstr(Socket[MAIN_WEB_TASK].outBuf, HTTP_MAP_JSON_Mode);
	if(!fld) return 0;
	if(*(fld + sizeof(HTTP_MAP_JSON_Mode) + 1) == 'S') return 2;
	fld = strstr(fld, HTTP_MAP_JSON_Sign);
	if(fld && *(fld + sizeof(HTTP_MAP_JSON_Sign) + 1) == '-') return 3;
	return 1;
}
#endif

#ifdef PWM_CALC_POWER_ARRAY
// Вычисление массива точного расчета мощности
#define PWM_fCalcNow			1
#define PWM_fCalcRelax			2
#define PWM_fCalc_WR_Active		3
uint8_t PWM_CalcFlags = 0;
int8_t  PWM_CalcLoadIdx;
int32_t PWM_AverageSum;
uint8_t PWM_AverageCnt; // +1
int32_t PWM_StandbyPower;
int16_t *PWM_CalcArray;
uint16_t PWM_CalcIdx;

// power - 0.1W
void WR_Calc_Power_Array_NewMeter(int32_t power)
{
	if(GETBIT(PWM_CalcFlags, PWM_fCalcNow)) {
#ifdef WR_CurrentSensor_4_20mA
		HP.sADC[IWR].Read();
#ifndef TEST_BOARD
		if(PWM_AverageCnt++) PWM_AverageSum += HP.sADC[IWR].get_Value() * (int)HP.dSDM.get_Voltage();
#else
		if(PWM_AverageCnt++) {
			if(HP.dSDM.get_Voltage() != 0) PWM_AverageSum += HP.sADC[IWR].get_Value() * (int)HP.dSDM.get_Voltage();
			else {
				PWM_AverageSum += 10 + (GETBIT(PWM_CalcFlags, PWM_fCalcRelax) ? 0 : PWM_CalcIdx * 9) + (rand() & 0x3);
			}
		}
#endif
#else
		if(PWM_AverageCnt++) PWM_AverageSum += round_div_int32(power, 10); // skip first
#endif
		if(GETBIT(PWM_CalcFlags, PWM_fCalcRelax)) {
			if(PWM_AverageCnt >= 5) { // 10 sec
				PWM_AverageSum = PWM_AverageSum / (PWM_AverageCnt - 1);
				if(GETBIT(WR.Flags, WR_fLogFull)) journal.jprintf("Relax: %d\n", PWM_AverageSum);
				PWM_AverageSum -= PWM_CalcArray[0];
				if(abs(PWM_AverageSum) > 5) journal.jprintf("Non stable Relax power: %s%d\n", PWM_AverageSum > 0 ? "+" : "", PWM_AverageSum);
				PWM_Write(WR_Load_pins[PWM_CalcLoadIdx], ((1<<PWM_WRITE_OUT_RESOLUTION)-1) - PWM_CalcIdx);
				PWM_CalcFlags &= ~(1<<PWM_fCalcRelax);
				PWM_AverageSum = PWM_AverageCnt = 0;
			}
		} else if(PWM_AverageCnt >= 6) { // 12 sec
			PWM_AverageSum /= (PWM_AverageCnt - 1);
			if(PWM_CalcIdx) PWM_AverageSum -= PWM_CalcArray[0];
			PWM_CalcArray[PWM_CalcIdx] = PWM_AverageSum;
			if(GETBIT(WR.Flags, WR_fLogFull)) journal.jprintf("Power[%d]: %d\n", PWM_CalcIdx, PWM_AverageSum);
			PWM_Write(WR_Load_pins[PWM_CalcLoadIdx], ((1<<PWM_WRITE_OUT_RESOLUTION)-1));
			if(PWM_CalcIdx++ == ((1<<PWM_WRITE_OUT_RESOLUTION)-1)) {
				WR.LoadPower[PWM_CalcLoadIdx] = PWM_AverageSum;
				TaskSuspendAll();
				journal.jprintf("\nPWM Calc Ok\nPower[%d] = ", (1<<PWM_WRITE_OUT_RESOLUTION));
				for(uint16_t i = 0; i < (1<<PWM_WRITE_OUT_RESOLUTION); i++) {
					journal.jprintf("%d,", PWM_CalcArray[i]);
				}
				journal.jprintf("\nPWM[%d] = %d,", sizeof(PWM_POWER_ARRAY)/sizeof(PWM_POWER_ARRAY[0]), ((1<<PWM_WRITE_OUT_RESOLUTION)-1));
				uint16_t last_idx = 1;
				for(uint16_t i = 1; i < sizeof(PWM_POWER_ARRAY)/sizeof(PWM_POWER_ARRAY[0]) - 2; i++) {
					int n = PWM_AverageSum * i / (sizeof(PWM_POWER_ARRAY)/sizeof(PWM_POWER_ARRAY[0])-1);
					for(; last_idx < (1<<PWM_WRITE_OUT_RESOLUTION) - 1; last_idx++) {
						if(n >= PWM_CalcArray[last_idx] && n <= PWM_CalcArray[last_idx + 1]) {
							journal.jprintf("%d,", ((1<<PWM_WRITE_OUT_RESOLUTION)-1) - (abs(n - PWM_CalcArray[last_idx]) <= abs(n - PWM_CalcArray[last_idx + 1]) ? last_idx : ++last_idx));
							break;
						}
					}
				}
				journal.jprintf("0, 0\n\n");
				xTaskResumeAll();
				free(PWM_CalcArray);
				WR.Flags |= (GETBIT(PWM_CalcFlags, PWM_fCalc_WR_Active)<<WR_fActive);
				PWM_CalcFlags = 0;
			} else {
				PWM_CalcFlags |= (1<<PWM_fCalcRelax);
				PWM_AverageSum = PWM_AverageCnt = 0;
			}
		}
	}
}

void WR_Calc_Power_Array_Start(int8_t load_idx)
{
#if !defined(WR_CurrentSensor_4_20mA) && !defined(WR_PowerMeter_Modbus)
	journal.jprintf("PWM Calc not supported!\n");
#else
	journal.jprintf("\nPWM Calc begin:\n");
	PWM_CalcLoadIdx = load_idx;
	PWM_AverageSum = PWM_AverageCnt = PWM_StandbyPower = PWM_CalcIdx = 0;
	PWM_CalcArray = (int16_t*) malloc(sizeof(int16_t) * (1<<PWM_WRITE_OUT_RESOLUTION));
	if(PWM_CalcArray == NULL) {
		journal.jprintf("Memory low!\n");
		return;
	}
	PWM_Write(WR_Load_pins[PWM_CalcLoadIdx], ((1<<PWM_WRITE_OUT_RESOLUTION)-1));
	PWM_CalcFlags = (PWM_CalcFlags & ~(1<<PWM_fCalc_WR_Active)) | (GETBIT(WR.Flags, WR_fActive)<<PWM_fCalc_WR_Active) | (1<<PWM_fCalcNow);
	WR.Flags &= ~(1<<WR_fActive);
#endif
}
#endif

#endif

