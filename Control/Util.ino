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

///////////////////////////////////////////////////////////////////////////////////
// Функции ниже использовать только в WebServer или с семафором xWebThreadSemaphore!
char _buf[18];
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

//float в *char в строку ДОБАВЛЯЕТСЯ значение экономим место и скорость и стек -----------------------------------
uint8_t _ftoa(char *outstr, float val, unsigned char precision)
{
    while(*outstr) outstr++;
	char *instr = outstr;
	
	// compute the rounding factor and fractional multiplier
	float roundingFactor = 0.5;
	unsigned long mult = 1;
	unsigned char padding = precision;
	while(precision--) {
		roundingFactor /= 10.0;
		mult *= 10;
	}
	if(val < 0.0){
		*outstr++ = '-';
		val = -val;
	}
	val += roundingFactor;
	outstr += m_itoa((long)val, outstr, 10, 0);
	if(padding > 0) {
		*(outstr++) = '.';
		outstr += m_itoa((val - (long)val) * mult, outstr, 10, padding);
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
		unsigned char digit = value % 10;
		*(pbuffer++) = (digit < 10 ? '0' + digit : 'a' + digit - 10);
		value /= 10;
	} while (value > 0);

	if (negative)
		*(pbuffer++) = '-';

	*(pbuffer) = '\0';

	/* ... now we reverse it (could do it recursively but will
	 * conserve the stack space) */
	uint8_t len = (pbuffer - string);
	for (uint8_t i = 0; i < len / 2; i++) {
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
	startSupcStatusReg = SUPC->SUPC_SR;                        // запомнить состояние при старте
	journal.jprintf("Supply Controller Status Register [SUPC_SR]: 0x%08x\n", startSupcStatusReg);

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
boolean initSD(void)
{
	boolean success = false;   // флаг успешности инициализации
	journal.jprintf("Initializing SD card... ");
#ifdef NO_SD_CONTROL                // Если реализован механизм проверки наличия карты в слоте (выключатель в слоте карты)
	pinMode(PIN_NO_SD_CARD, INPUT);     // ++ CD Программирование проверки наличия карты
	if (digitalReadDirect(PIN_NO_SD_CARD)) {journal.jprintf("No SD card!\n"); return false;}
#endif
	// 1. Инициалазация карты
	if(!card.begin(PIN_SPI_CS_SD, SD_SCK_MHZ(SD_CLOCK))) {
		journal.jprintf("Init error %d,%d!\n", card.cardErrorCode(), card.cardErrorData());
	} else success = true;  // Карта инициализирована с первого раза

	if(success)  // Запоминаем результаты
		journal.jprintf("OK\n");
	else {
		//set_Error(ERR_SD_INIT,"SD card");   // Уведомить об ошибке
		HP.message.setMessage(pMESSAGE_SD, (char*) "Ошибка инициализации SD карты", 0);    // сформировать уведомление
		SPI_switchW5200();
		return false;
	}

	// 2. Проверка наличия индексного файла
	if(!card.exists(INDEX_FILE)) {
		journal.jprintf((char*) " ERROR - Can't find %s file on SD card!\n", INDEX_FILE);
		//   set_Error(ERR_SD_INDEX,"SD card");   // Уведомить об ошибке
		HP.message.setMessage(pMESSAGE_SD, (char*) "Файл индекса не найден на SD карте", 0); // сформировать уведомление
		SPI_switchW5200();
		return false;
	} // стартовый файл не найден
	journal.jprintf((char*) " Found %s file\n", INDEX_FILE);

	// 3. Вывод инфо о карте
	cid_t cid;
	if(!card.card()->readCID(&cid)) {
		journal.jprintf((char*) "$ERROR - readCID SD card failed!\n");
		HP.message.setMessage(pMESSAGE_SD, (char*) "Ошибка чтения информации о карте (readCID)", 0); // сформировать уведомление
		SPI_switchW5200();
		return false;
	}
	// вывод информации о карте
	journal.jprintf((char*) "SD card info\n");
	journal.jprintf((char*) " Manufacturer ID: 0x%x\n", int(cid.mid));
	journal.jprintf((char*) " OEM ID: %c%c\n", cid.oid[0], cid.oid[1]);
	journal.jprintf((char*) " Serial number: 0x%x\n", int(cid.psn));
	journal.jprintf((char*) " Volume is FAT%d\n", int(card.vol()->fatType()));
	journal.jprintf((char*) " blocksPerCluster: %d\n", int(card.vol()->blocksPerCluster()));
	journal.jprintf((char*) " clusterCount: %d\n", card.vol()->clusterCount());
	uint32_t volFree = card.vol()->freeClusterCount();
	float fs = 0.000512 * volFree * card.vol()->blocksPerCluster();
	journal.jprintf((char*) " freeSpace: %.2f Mb\n", fs);
	SPI_switchW5200();
	return true;
}

// Инициализация SPI диска и проверка наличия веб морды (файл индекс), параметр true - вывод инфо в журнал false молча проверяем состояние
// Возврат true - если морда есть на карте (готово к использованию), false - диск не рабоатет или нет морды (использовать нельзя)
boolean initSpiDisk(boolean show)
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
					HP.set_WebStoreOnSPIFlash(false); //  сбрасываем в оперативке (но не на флеше) флаг загрузки из флеш диска
					if(show) journal.jprintf((char*) " ERROR - Can't find %s file on SPI flash!\n", INDEX_FILE);
					HP.message.setMessage(pMESSAGE_SD, (char*) "Файл индекса не найден на SPI флеш диске", 0); // сформировать уведомление
					return false; } 
				else if(show) journal.jprintf((char*) " Found %s file\n", INDEX_FILE);	// файл найден
			} // if HP.get_fSPIFlash()
		
		return true;
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
static byte _retEEPROM_I2C;
// Запись в eeprom, 0 - успешно
__attribute__((always_inline))  inline byte writeEEPROM_I2C(unsigned long addr, byte *values, unsigned int nBytes)
{
	if(SemaphoreTake(xI2CSemaphore, I2C_TIME_WAIT / portTICK_PERIOD_MS) == pdFALSE) {  // Если шедулер запущен то захватываем семафор
		journal.printf((char*) cErrorMutex, __FUNCTION__, MutexI2CBuzy);
		return 0;
	}
	_retEEPROM_I2C = eepromI2C.write(addr, values, nBytes);
	SemaphoreGive(xI2CSemaphore);
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
	SemaphoreGive(xI2CSemaphore);
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
	static int16_t rtc_temp = rtcI2C.temperature();
	SemaphoreGive(xI2CSemaphore);
	return (float) rtc_temp / 100.0;
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
	SemaphoreGive(xI2CSemaphore);
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
		if(writeEEPROM_I2C(addr_to++, (uint8_t *)&size, 1)) return set_Error(ERR_SAVE_EEPROM, (char*)nameHeatPump);
		crc = _crc16(crc, size);
	} else { // длина 2 байта, первый бит = 1
		size = (size << 1) | 1;
		if(writeEEPROM_I2C(addr_to, (uint8_t *)&size, 2)) return set_Error(ERR_SAVE_EEPROM, (char*)nameHeatPump);
		addr_to += 2;
		crc = _crc16(crc, size & 0xFF);
		crc = _crc16(crc, size >> 8);
	}
	size >>= 1;
	if(writeEEPROM_I2C(addr_to, addr_from, size)) {
		return set_Error(ERR_SAVE_EEPROM, (char*)nameHeatPump);
	}
	addr_to += size;
	while(size--) crc = _crc16(crc, *addr_from++);
	return OK;
}

int8_t save_2bytes(uint32_t &addr_to, uint16_t data, uint16_t &crc)
{
	if(writeEEPROM_I2C(addr_to, (uint8_t *)&data, 2)) return set_Error(ERR_SAVE_EEPROM, (char*)nameHeatPump);
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
