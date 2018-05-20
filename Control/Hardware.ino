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
// --------------------------------------------------------------------------------
// Описание базовых классов для работы c "железом"
// (датчики и исполнительные устройства) зависит от контроллера
// --------------------------------------------------------------------------------
#include "Hardware.h"

// --------------------------------------------------------------------------------
// Настройка таймера и ацп для чтения датчика давления
// Зависит от чипа
// --------------------------------------------------------------------------------
// Старт считывания АЦП
void start_ADC()
{ 
  adc_setup () ;                                       // setup ADC
  pmc_enable_periph_clk (TC_INTERFACE_ID + 0*3+0) ;    // clock the TC0 channel 0

  TcChannel * t = &(TC0->TC_CHANNEL)[0] ;              // pointer to TC0 registers for its channel 0
  t->TC_CCR = TC_CCR_CLKDIS ;                          // disable internal clocking while setup regs
  t->TC_IDR = 0xFFFFFFFF ;                             // disable interrupts
  t->TC_SR ;                                           // read int status reg to clear pending
  t->TC_CMR = TC_CMR_TCCLKS_TIMER_CLOCK1 |             // use TCLK1 (prescale by 2, = 42MHz)
              TC_CMR_WAVE |                            // waveform mode
              TC_CMR_WAVSEL_UP_RC |                    // count-up PWM using RC as threshold
              TC_CMR_EEVT_XC0 |                        // Set external events from XC0 (this setup TIOB as output)
              TC_CMR_ACPA_CLEAR | TC_CMR_ACPC_CLEAR |
              TC_CMR_BCPB_CLEAR | TC_CMR_BCPC_CLEAR ;
 
  t->TC_RC = VARIANT_MCK/2/PRESS_FREQ;        // counter resets on RC, so sets period in terms of 42MHz clock
  t->TC_RA = VARIANT_MCK/2/PRESS_FREQ/2 ;     // roughly square wave
  t->TC_CMR = (t->TC_CMR & 0xFFF0FFFF) | TC_CMR_ACPA_CLEAR | TC_CMR_ACPC_SET ;  // set clear and set from RA and RC compares
  t->TC_CCR = TC_CCR_CLKEN | TC_CCR_SWTRG ;  // re-enable local clocking and switch to hardware trigger source.

 }

// Установка АЦП
void adc_setup ()
{
  uint32_t adcMask=0x00;
   #ifdef VCC_CONTROL                                   // если разрешено чтение напряжение питания
     adcMask=adcMask|(0x1u<<PIN_ADC_VCC);               // Добавить маску контроля питания
   #endif
  //   adcMask=adcMask|(0x1u<<ADC_TEMPERATURE_SENSOR);         // добавить маску для внутреннего датчика температуры
  //   adc_enable_ts(ADC);                                     // разрешить чтение температурного датчика в регистре ADC Analog Control Register

  
// Расчет маски каналов
for (uint8_t i=0;i<ANUMBER;i++)    // по всем датчикам
      if (HP.sADC[i].get_present()) adcMask=adcMask|(0x1u <<HP.sADC[i].get_pinA());     // датчик есть в конфигурации
 
  NVIC_EnableIRQ (ADC_IRQn) ;        // enable ADC interrupt vector
  ADC->ADC_IDR = 0xFFFFFFFF ;        // disable interrupts IDR Interrupt Disable Register
  ADC->ADC_IER = adcMask;            // IER Interrupt Enable Register enable AD11 End-Of-Conv interrupt (Arduino pin A9) каналы здесь SAMX3!!
//  ADC->ADC_IER =(0x1u <<28);       // IER Interrupt Enable Register enable AD11 End-Of-Conv interrupt (Arduino pin A9) каналы здесь SAMX3!!
  ADC->ADC_CHDR = 0xFFFF ;           // Channel Disable Register CHDR disable all channels
  ADC->ADC_CHER =adcMask;            // Channel Enable Register CHER enable just A11  каналы здесь SAMX3!!
  ADC->ADC_CGR = 0x15555555 ;        // All gains set to x1 Channel Gain Register
 // ADC->ADC_CGR = 0x55555555 ;        // All gains set to x1 Channel Gain Register
  ADC->ADC_COR = 0x00000000 ;        // All offsets off Channel Offset Register
  ADC->ADC_MR = (ADC->ADC_MR & 0xFFFFFFF0) | (1 << 1) | ADC_MR_TRGEN ;  // 1 = trig source TIO from TC0
}


// Обработчик прерывания, как можно короче
#ifdef __cplusplus
extern "C"
{
#endif
void ADC_Handler (void)
{
 uint8_t i=0;

   #ifdef VCC_CONTROL  // если разрешено чтение напряжение питания
    if (ADC->ADC_ISR & (0x1u <<PIN_ADC_VCC))   // ensure there was an End-of-Conversion and we read the ISR reg
          {
           HP.AdcVcc =(unsigned int)(*(ADC->ADC_CDR+PIN_ADC_VCC));   // если готов прочитать результат
          }
    #endif
       
 for (i=0;i<ANUMBER;i++)    // по всем датчикам
       { 
         if (!HP.sADC[i].get_present()) continue;    // датчик отсутсвует в конфигурации пропускаем
         if (ADC->ADC_ISR & (0x1u << HP.sADC[i].get_pinA()))   // ensure there was an End-of-Conversion and we read the ISR reg
          {
           HP.sADC[i].adc.lastVal =(unsigned int)(*(ADC->ADC_CDR+HP.sADC[i].get_pinA())) ;                      // get conversion result
           HP.sADC[i].adc.error=OK;
          }
          else continue; 
          // Усреднение значений
          HP.sADC[i].adc.sum=HP.sADC[i].adc.sum+HP.sADC[i].adc.lastVal;                                       // Добавить новое значение
          HP.sADC[i].adc.sum=HP.sADC[i].adc.sum-HP.sADC[i].adc.p[HP.sADC[i].adc.last];                        // Убрать самое старое значение
          HP.sADC[i].adc.p[HP.sADC[i].adc.last]=HP.sADC[i].adc.lastVal;                                       // Запомить новое значение
          if (HP.sADC[i].adc.last<FILTER_SIZE-1) HP.sADC[i].adc.last++; else {HP.sADC[i].adc.last=0; HP.sADC[i].adc.flagFull=true;} // приращение счетчика
  //        if (HP.sADC[i].adc.flagFull) HP.sADC[i].adc.val=HP.sADC[i].adc.sum/FILTER_SIZE; else HP.sADC[i].adc.val=HP.sADC[i].adc.sum/HP.sADC[i].adc.last;  // расчет
       } // for
       
 // if (ADC->ADC_ISR & (0x1u <<ADC_TEMPERATURE_SENSOR))   // ensure there was an End-of-Conversion and we read the ISR reg
 //            HP.AdcTempSAM3x =(unsigned int)(*(ADC->ADC_CDR+ADC_TEMPERATURE_SENSOR));   // если готов прочитать результат
       
}

#ifdef __cplusplus
}
#endif

    
// ------------------------------------------------------------------------------------------
// Аналоговые датчики давления --------------------------------------------------------------
// Давление хранится в СОТЫХ БАРА
// Описание датчиков
const char *namePress[] =       {
                                 "PEVA",
                                 "PCON"
                                 };

void sensorADC::initSensorADC(int sensor,int pinA)
    { 

      // Инициализация структуры для хранения "сырых"данных с аналогового датчика.
      if (SENSORPRESS[sensor]==true)    // отводим память если используем датчик под сырые данные
      { 
    //    adc.p=(uint16_t*)malloc(sizeof(uint16_t)*FILTER_SIZE);
    //    if (adc.p==NULL) { set_Error(ERR_OUT_OF_MEMORY,(char*)"sensorADC");return;}  // ОШИБКА если память не выделена
        adc.sum=0;                                                                   // сумма
        adc.last=0;                                                                  // текущий индекс
        adc.flagFull=false;                                                          // буфер полный
        adc.lastVal=0;                                                               // последнее считанное значение
 //       adc.err_read=0;                                                              // счетчик ошибкок чтения
        adc.error=OK;                                                                // Последняя ошибка чтения датчика
        clearBuffer();
      }  
 
      minPress=MINPRESS[sensor];                 // минимально разрешенное давление
      maxPress=MAXPRESS[sensor];                 // максимально разрешенное давление
      testPress=TESTPRESS[sensor];               // Значение при тестировании
      testMode=NORMAL;                           // Значение режима тестирования
      zeroPress=ZEROPRESS[sensor];               // отсчеты АЦП при нуле датчика
      transADC=TRANsADC[sensor];                 // коэффициент пересчета АЦП в давление
      flags=0x00;                                // Обнулить флаги
      if (SENSORPRESS[sensor]==true) SETBIT1(flags,fPresent);  // наличие датчика в текушей конфигурации
      Chart.init(SENSORPRESS[sensor]);            // инициалазация статистики
      err=OK;                                     // ошибка датчика (работа)
      Press=0;                                    // давление датчика (обработанная)
      pin=pinA;
      note=(char*)notePress[sensor];              // присвоить наименование датчика
      name=(char*)namePress[sensor];              // присвоить имя датчика
    };
    
 // очистить буфер АЦП
 void sensorADC::clearBuffer()
 {
  uint16_t i;
      for(i=0;i<P_NUMSAMLES;i++) p[i]=0;         // обнуление буффера значений
      sum=0;
      last=0;
      SETBIT0(flags,fFull);                      // Буфер не полный
      lastPress=0;                               // последнее считанное давление по умолчанию ноль
 }

  
// чтение данных c аналогового датчика (АЦП) возвращает код ошибки, делает все преобразования
int8_t  sensorADC::Read() 
{  
       
 if(!(GETBIT(flags,fPresent)))  return err;        // датчик запрещен в конфигурации ничего не делаем
 
 if (testMode!=NORMAL) lastPress=testPress;        // В режиме теста
 else                                              // Чтение датчика
   {
  #ifdef DEMO
      lastADC=random(1350,2500);                   // В демо режиме генерим значение
   #else
       if (adc.flagFull) lastADC=adc.sum/FILTER_SIZE; else lastADC=adc.sum/adc.last;    
   #endif   
    if(adc.error!=OK)  {err=ERR_READ_PRESS;set_Error(err,name);return err;}   // Проверка на ошибку чтения ацп
    lastPress=(int)((float)lastADC*(transADC))-zeroPress;   
   }
 //  Serial.print(lastADC);  Serial.print(" ");  Serial.println(lastPress); 
    // Усреднение значений
    sum=sum+lastPress;          // Добавить новое значение
    sum=sum-p[last];            // Убрать самое старое значение
    p[last]=lastPress;          // Запомить новое значение
    if (last<P_NUMSAMLES-1) last++; else {last=0; SETBIT1(flags,fFull);}
    if (GETBIT(flags,fFull)) Press=sum/P_NUMSAMLES; else Press=sum/last; 
    
  // Проверка на ошибки именно здесь обрабатывются ошибки и передаются на верх
  // Берутся МНОВЕННЫЕ значения!!!! для увеличения реакции системы на ошибки
  // При ошибке запоминаем мговенное значение как среднее  что бы видно было
  if(lastPress<minPress)  {Press=lastPress; err=ERR_MINPRESS;set_Error(err,name);return err;}  
  if(lastPress>maxPress)  {Press=lastPress; err=ERR_MAXPRESS;set_Error(err,name);return err;}

  // Дошли до сюда значит ошибок нет
  err=OK;                                         // Новый цикл новые ошибки
  return err; 
}
// полный цикл получения данных возвращает значение давления, только тестирование!! никакие переменные класса не трогает!!
int16_t sensorADC::Test()
{
   int16_t x;
   if (adc.flagFull) x=adc.sum/FILTER_SIZE; else x=adc.sum/adc.last;    
   return (int)((float)x*(transADC))-zeroPress;
}


// Установка 0 датчика темпеартуры
int8_t sensorADC::set_zeroPress(int16_t p)
{
  if((p>=0)&&(p<=2048)) { clearBuffer(); zeroPress=p; return OK;} // Суммы обнулить надо
  else return WARNING_VALUE;
}


//Получить значение давления датчика - это то что используется
int16_t sensorADC::get_Press()
{
if (!(GETBIT(flags,fPresent))) return -100;                  // датчик запрещен в конфигурации то давление -100
return Press;    
}

// Установить значение коэффициента преобразования напряжение (отсчеты ацп)-температура
int8_t sensorADC::set_transADC(float p)
{
  if((p>=0.0)&&(p<=4.0)) { clearBuffer(); transADC=p; return OK;}  // Суммы обнулить надо
  else return WARNING_VALUE;
}


// Установить значение давления датчика в режиме теста
int8_t sensorADC::set_testPress(int16_t p)            
{
 if((p>=minPress)&&(p<=maxPress)) { testPress=p; return OK;} else return WARNING_VALUE;
}

// Записать настройки в eeprom i2c на входе адрес с какого, на выходе конечный адрес, если число меньше 0 это код ошибки
int32_t sensorADC::save(int32_t adr)
{
if (writeEEPROM_I2C(adr, (byte*)&zeroPress, sizeof(zeroPress)))    { set_Error(ERR_SAVE_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(zeroPress);   // !save! отсчеты АЦП при нуле датчика
if (writeEEPROM_I2C(adr, (byte*)&transADC, sizeof(transADC)))      { set_Error(ERR_SAVE_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(transADC);   // !save! коэффициент пересчета АЦП в давление
if (writeEEPROM_I2C(adr, (byte*)&testPress, sizeof(testPress)))    { set_Error(ERR_SAVE_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(testPress);  // !save! давление датчика в режиме тестирования
return adr;
}

// Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, если число меньше 0 это код ошибки
int32_t sensorADC::load(int32_t adr)
{
if (readEEPROM_I2C(adr, (byte*)&zeroPress, sizeof(zeroPress)))     { set_Error(ERR_LOAD_EEPROM,name); return ERR_LOAD_EEPROM; } adr=adr+sizeof(zeroPress);   // !save! отсчеты АЦП при нуле датчика
if (readEEPROM_I2C(adr, (byte*)&transADC, sizeof(transADC)))       { set_Error(ERR_LOAD_EEPROM,name); return ERR_LOAD_EEPROM; } adr=adr+sizeof(transADC);  // !save! коэффициент пересчета АЦП в давление
if (readEEPROM_I2C(adr, (byte*)&testPress, sizeof(testPress)))     { set_Error(ERR_LOAD_EEPROM,name); return ERR_LOAD_EEPROM; } adr=adr+sizeof(testPress);   // !save! давление датчика в режиме тестирования
return adr;
}
// Считать настройки из буфера на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t sensorADC::loadFromBuf(int32_t adr,byte *buf)
{
  memcpy((byte*)&zeroPress,buf+adr,sizeof(zeroPress)); adr=adr+sizeof(zeroPress);      // !save! отсчеты АЦП при нуле датчика
  memcpy((byte*)&transADC,buf+adr,sizeof(transADC)); adr=adr+sizeof(transADC);   // !save! коэффициент пересчета АЦП в давление
  memcpy((byte*)&testPress,buf+adr,sizeof(testPress)); adr=adr+sizeof(testPress);      // !save! давление датчика в режиме тестирования
  return adr;
}
 // Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
uint16_t sensorADC::get_crc16(uint16_t crc)
{
  uint8_t i;
  crc=_crc16(crc,lowByte(zeroPress)); crc=_crc16(crc,highByte(zeroPress));    //  отсчеты АЦП при нуле датчика
  for(i=0;i<sizeof(transADC);i++) crc=_crc16(crc,*((byte*)&transADC+i));  //  коэффициент пересчета АЦП в давление FLOAT
  crc=_crc16(crc,lowByte(testPress)); crc=_crc16(crc,highByte(testPress));    //  давление датчика в режиме тестирования
  return crc;              
}

// ------------------------------------------------------------------------------------------
// Цифровые контактные датчики (есть 2 состяния 0 и 1) --------------------------------------

// Инициализация контактного датчика
void  sensorDiditalInput::initInput(int sensor)
{
   Input=false;                    // Состояние датчика
   testInput=TESTINPUT[sensor];    // Состояние датчика в режиме теста
   testMode=NORMAL;                // Значение режима тестирования
   alarmInput=ALARMINPUT[sensor];  // Состояние датчика в режиме аварии
   err=OK;                         // ошибка датчика (работа) при ошибке останов ТН
   flags=0x00;                     // сброс флагов
   // флаги  0 - наличие датчика,  1- режим теста
   SETBIT1(flags,fPresent);        // наличие датчика в текушей конфигурации
   type=pALARM;
   type=SENSORTYPE[sensor];         // тип датчика
   pin=pinsInput[sensor];           // пин датчика
   pinMode(pin, INPUT);             // Настроить ножку на вход
   note=(char*)noteInput[sensor];   // присвоить наименование датчика
   name=(char*)nameInput[sensor];   // присвоить имя датчика
};

  // Чтение датчика возвращает ошибку или ОК
int8_t sensorDiditalInput::Read()
{
/* old
 err=OK;                                            // Ошибки сбросить
 if (testMode!=NORMAL) Input=testInput;              // В режиме теста
 else Input=digitalReadDirect(pin);                  // Прочитать состяние датчика и запомнить
 
  _delay(1);                 						 // почему то нужна задержка
  if ((Input==alarmInput)&&(type==pALARM))           // Срабатывание аварийного датчика (только его!)
            { err=ERR_DINPUT;set_Error(err,name); }  // Сработал датчик АВАРИЯ!!!!
*/
 err=OK;                                            // Ошибки сбросить
 if (testMode!=NORMAL) Input=testInput;             // В режиме теста
 else {
     boolean in = digitalReadDirect(pin);
     if(in != Input) {
         uint8_t i;
         for(i = 0; i<2; i++) {
        	 _delay(1);
             if(in != digitalReadDirect(pin)) break;
         }
         if(i == 2) Input = in;
     }
 }
 if ((Input==alarmInput) && (type==pALARM))     // Срабатывание аварийного датчика (только его!)
     { err=ERR_DINPUT;set_Error(err,name); }    // Сработал датчик АВАРИЯ!!!!
 return err;

}

    
// Установить Состояние датчика в режиме теста
int8_t sensorDiditalInput::set_testInput( int16_t i)         
{
   if(i==1)  { testInput=true; return OK;} 
    else  if (i==0)  { testInput=false; return OK;} 
     else return WARNING_VALUE;
}

 
// Установить Состояние датчика в режиме аварии
int8_t sensorDiditalInput::set_alarmInput( int16_t i)         
{
  if(i==1)  { alarmInput=true; return OK;} 
    else  if(i==0)  { alarmInput=false; return OK;} 
     else return WARNING_VALUE;
}


// Записать настройки в eeprom i2c на входе адрес с какого, на выходе конечный адрес, если число меньше 0 это код ошибки
int32_t sensorDiditalInput::save(int32_t adr)
{
if (writeEEPROM_I2C(adr, (byte*)&testInput, sizeof(testInput)))   { set_Error(ERR_SAVE_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(testInput);    // !save! Состояние датчика в режиме теста
if (writeEEPROM_I2C(adr, (byte*)&alarmInput, sizeof(alarmInput))) { set_Error(ERR_SAVE_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(alarmInput);   // !save! Состояние датчика в режиме аварии
return adr;   
}

// Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, если число меньше 0 это код ошибки
int32_t sensorDiditalInput::load(int32_t adr)
{
if (readEEPROM_I2C(adr, (byte*)&testInput, sizeof(testInput)))     { set_Error(ERR_LOAD_EEPROM,name); return ERR_LOAD_EEPROM; } adr=adr+sizeof(testInput);    // !save! Состояние датчика в режиме теста
if (readEEPROM_I2C(adr, (byte*)&alarmInput, sizeof(alarmInput)))   { set_Error(ERR_LOAD_EEPROM,name); return ERR_LOAD_EEPROM; } adr=adr+sizeof(alarmInput);   // !save! Состояние датчика в режиме аварии
return adr;
}
// Считать настройки из буфера на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t sensorDiditalInput::loadFromBuf(int32_t adr,byte *buf)
{
  memcpy((byte*)&testInput,buf+adr,sizeof(testInput)); adr=adr+sizeof(testInput);     // !save! Состояние датчика в режиме теста
  memcpy((byte*)&alarmInput,buf+adr,sizeof(alarmInput)); adr=adr+sizeof(alarmInput);  // !save! Состояние датчика в режиме аварии
  return adr;
}
 // Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
uint16_t sensorDiditalInput::get_crc16(uint16_t crc)
{
  crc=_crc16(crc,testInput); // !save! Состояние датчика в режиме теста
  crc=_crc16(crc,alarmInput);// !save! Состояние датчика в режиме аварии
  return crc;              
}


// ------------------------------------------------------------------------------------------
// Цифровые частотные датчики (значение кодируется в выходной частоте) ----------------------
// основное назначение - датчики потока
// Частота кодируется в тысячных герца
// Число импульсов рассчитывается за базовый период (BASE_TIME), т.к частоты малы период надо савить не менее 5 сек

// Обработчики прерываний для подсчета частоты
void InterruptFLOWCON()  
{
#ifdef FLOWCON  
  HP.sFrequency[FLOWCON].InterruptHandler();
#endif  
}
void InterruptFLOWEVA()  
{
#ifdef FLOWEVA   
  HP.sFrequency[FLOWEVA].InterruptHandler();
#endif  
}
#ifdef FLOWPCON  
void InterruptFLOWPCON() 
{
  HP.sFrequency[FLOWPCON].InterruptHandler();
}
#endif  

// Инициализация частотного датчика, на входе номер сенсора по порядку
void sensorFrequency::initFrequency(int sensor)                     
{
   Frequency=0;                                   // значение частоты
   Value=0;                                       // значение датчика в ТЫСЯЧНЫХ (умножать на 1000)
   Capacity=HEAT_CAPACITY;                        // значение теплоемкости теплоносителя в конутре где установлен датчик [Cp, Дж/(кг·град)]
   minValue=MINFLOW[sensor];                      // минимальное значение датчика
   testValue=TESTFLOW[sensor];                    // Состояние датчика в режиме теста
   kfValue=TRANSFLOW[sensor];                     // коэффициент пересчета частоты в значение
   testMode=NORMAL;                               // Значение режима тестирования
   count=0;                                       // число импульсов за базовый период (то что меняется в прерывании)
   err=OK;                                        // ошибка датчика (работа) при ошибке останов ТН
   flags=0x00;                                    // Обнулить флаги
   SETBIT1(flags,fPresent);                       // наличие датчика в текушей конфигурации - датчик всегда есть в концигурвции добавлено для единообразия
   pin=pinsFrequency[sensor];                     // Ножка куда прицеплен датчик
   note=(char*)noteFrequency[sensor];             // наименование датчика
   name=(char*)nameFrequency[sensor];             // Имя датчика
   Chart.init(true);                              // инициалазация статистики
   sTime=xTaskGetTickCount();                     // время начала базового периода в тиках
   // Привязывание обработчика преваний к методу конкретного класса
   //   LOW вызывает прерывание, когда на порту LOW
   //   CHANGE прерывание вызывается при смене значения на порту, с LOW на HIGH и наоборот
   //   RISING прерывание вызывается только при смене значения на порту с LOW на HIGH
   //   FALLING прерывание вызывается только при смене значения на порту с HIGH на LOW
        if (sensor==FLOWCON)  attachInterrupt(pin,InterruptFLOWCON,CHANGE); 
   else if (sensor==FLOWEVA)  attachInterrupt(pin,InterruptFLOWEVA,CHANGE); 
#ifdef FLOWPCON   
   else if (sensor==FLOWPCON) attachInterrupt(pin,InterruptFLOWPCON,CHANGE);
#endif     
   else err=ERR_NUM_FREQUENCY;
  
}

// Получить (точнее обновить) значение датчика
int8_t sensorFrequency::Read()
{

 if (testMode!=NORMAL) { Value=testValue; Frequency=Value*1000*(kfValue/3600.0); return err; }   // В режиме теста
 #ifdef DEMO
    Frequency=random(2500,6000);
    count=0;
 //   Value=60.0*Frequency/kfValue/1000.0;                  // переводим в Кубы в час  (Frequency/kfValue- литры в минуту)  watt=(Value/3.600) *4.191*dT
    Value=((float)Frequency/1000.0)/(kfValue/3600.0);       // ЛИТРЫ В ЧАС (ИЛИ ТЫСЯЧНЫЕ КУБА) частота в тысячных, и коэффициент правим
   //  journal.jprintf("Sensor %s: frequence=%.3f flow=%.3f\n",name,Frequency/(1000.0),Value/(1000.0));
    return err;      // Если демо вернуть случайное число
 #else
   tickCount=xTaskGetTickCount();            // получить текущее время
   if(tickCount<sTime)                       // произошло переполнение счетчиков тиков freeRTOS - измерение с начала
       { sTime=tickCount;  count=0; }
   else if(tickCount-sTime>BASE_TIME_READ*1000)   // если только пришло время измерения
       { 
        Frequency=(count*500.0*1000.0)/(tickCount-sTime);    // ТЫСЯЧНЫЕ ГЦ время в миллисекундах частота в тысячных герца *2 (прерывание по обоим фронтам)!!!!!!!!
        sTime=tickCount;  
        count=0;
  //   Value=60.0*Frequency/kfValue/1000.0;               // Frequency/kfValue  литры в минуту а нужны кубы
       Value=((float)Frequency/1000.0)/(kfValue/3600.0);          // ЛИТРЫ В ЧАС (ИЛИ ТЫСЯЧНЫЕ КУБА) частота в тысячных, и коэффициент правим
       }
 #endif   
 return err;    
}
// Установить Состояние датчика в режиме теста
int8_t  sensorFrequency::set_testValue(int16_t i) 
{
   if(i>minValue)  { testValue=i; return OK;} else return WARNING_VALUE;                
}
// Установить коэффициент пересчета
int8_t  sensorFrequency::set_kfValue(float f)
{
  if((f>=0.0)&&(f<=600.0)) {kfValue=f; return OK;}  
  else return WARNING_VALUE;                
}
// Установить теплоемкость больше 5000 не устанавливается
int8_t sensorFrequency::set_Capacity(uint16_t c)                      
{  
  if (c<=5000) {Capacity=c; return OK;} else return WARNING_VALUE;    
}   
// Записать настройки в eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t sensorFrequency::save(int32_t adr)
{
if (writeEEPROM_I2C(adr, (byte*)&testValue, sizeof(testValue)))   { set_Error(ERR_SAVE_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(testValue);    // Состояние датчика в режиме теста
if (writeEEPROM_I2C(adr, (byte*)&kfValue, sizeof(kfValue)))       { set_Error(ERR_SAVE_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(kfValue);      // коэффициент пересчета частоты в значение
if (writeEEPROM_I2C(adr, (byte*)&Capacity, sizeof(Capacity)))     { set_Error(ERR_SAVE_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(Capacity);     // теплоемкость теплоносителя в контуре
return adr;                                 
}

// Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t sensorFrequency::load(int32_t adr)
{
if (readEEPROM_I2C(adr, (byte*)&testValue, sizeof(testValue)))   { set_Error(ERR_LOAD_EEPROM,name); return ERR_LOAD_EEPROM; } adr=adr+sizeof(testValue);    // Состояние датчика в режиме теста
if (readEEPROM_I2C(adr, (byte*)&kfValue, sizeof(kfValue)))       { set_Error(ERR_LOAD_EEPROM,name); return ERR_LOAD_EEPROM; } adr=adr+sizeof(kfValue);      // коэффициент пересчета частоты в значение
if (readEEPROM_I2C(adr, (byte*)&Capacity, sizeof(Capacity)))     { set_Error(ERR_LOAD_EEPROM,name); return ERR_LOAD_EEPROM; } adr=adr+sizeof(Capacity);     // теплоемкость теплоносителя в контуре
return adr;                              
}
// Считать настройки из буфера на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t sensorFrequency::loadFromBuf(int32_t adr,byte *buf)
{
  memcpy((byte*)&testValue,buf+adr,sizeof(testValue)); adr=adr+sizeof(testValue);      // Состояние датчика в режиме теста
  memcpy((byte*)&kfValue,buf+adr,sizeof(kfValue)); adr=adr+sizeof(kfValue);            // коэффициент пересчета частоты в значение
  memcpy((byte*)&Capacity,buf+adr,sizeof(Capacity)); adr=adr+sizeof(Capacity);         // теплоемкость теплоносителя в контуре
  return adr;  
}
// Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
uint16_t sensorFrequency::get_crc16(uint16_t crc)
{
  uint8_t i;
  crc=_crc16(crc,lowByte(testValue)); crc=_crc16(crc,highByte(testValue));    // uint16_t Состояние датчика в режиме теста
  for(i=0;i<sizeof(kfValue);i++) crc=_crc16(crc,*((byte*)&kfValue+i));        // float    коэффициент пересчета АЦП в давление FLOAT
  crc=_crc16(crc,lowByte(Capacity)); crc=_crc16(crc,highByte(Capacity));      // uint16_t теплоемкость теплоносителя в контуре
  return crc;                      
}

// ------------------------------------------------------------------------------------------
// Исполнительное устройство РЕЛЕ (есть 2 состяния 0 и 1) --------------------------------------
// Реле активный уровень (включения) НИЗКИЙ!!!
void devRelay::initRelay(int sensor)
{
   Relay=false;                    // Состояние реле - выключено
   flags=0x00;
   err=OK;
   testMode=NORMAL;                // Значение режима тестирования
   // флаги  0 - наличие датчика,  1- режим теста
   flags=0x01;                     // наличие датчика в текушей конфигурации (отстатки прошлого, реле сейчас есть всегда)
   pin=pinsRelay[sensor];  
   pinMode(pin, OUTPUT);           // Настроить ножку на выход
   #ifdef RELAY_INVERT // признак инвертирования реле
      digitalWriteDirect(pin, Relay); 
   #else
      digitalWriteDirect(pin, !Relay);// выключить реле
   #endif
   note=(char*)noteRelay[sensor];  // присвоить описание реле
   name=(char*)nameRelay[sensor];  // присвоить имя реле
}


// Установить реле в состояние r, базовая функция все остальные функции используют ее
// Если состояния совпадают то ничего не делаем
int8_t devRelay::set_Relay(boolean r)    
{
  err=OK;
  if((flags&0x01)==0) { err=ERR_DEVICE; return err;   }  // Реле не установлено  и пытаемся его включить
  if (Relay==r) return err;                              // Ничего менять не надо выходим
   
 // if (strcmp(name,"RTRV")==0) r=!r;                                                  // Инвертировать 4-x ходовой
 #ifdef RELAY_INVERT                                                                   // инвертирование реле выходов реле
  switch (testMode) // РЕАЛЬНЫЕ Действия в зависимости от режима
         {
          case NORMAL: digitalWriteDirect(pin, r);                              break; //  Режим работа не тест, все включаем ИНВЕРТИРУЕМ для того что бы true соответсвовал включенному реле (зависит от схемы реле)
          case SAFE_TEST:                                                       break; //  Ничего не включаем
          case TEST:   if (strcmp(name,"RCOMP")!=0) digitalWriteDirect(pin, r); break; //  Включаем все кроме компрессора
          case HARD_TEST: digitalWriteDirect(pin, r);                           break; //  Все включаем и компрессор тоже
        }
  #else                                                                                // НЕ инвертирование реле выходов реле (так было раньше)
  switch (testMode) // РЕАЛЬНЫЕ Действия в зависимости от режима
         {
          case NORMAL: digitalWriteDirect(pin, !r);                             break; //  Режим работа не тест, все включаем ИНВЕРТИРУЕМ для того что бы true соответсвовал включенному реле (зависит от схемы реле)
          case SAFE_TEST:                                                       break; //  Ничего не включаем
          case TEST:   if (strcmp(name,"RCOMP")!=0) digitalWriteDirect(pin, !r);break; //  Включаем все кроме компрессора
          case HARD_TEST: digitalWriteDirect(pin, !r);                          break; //  Все включаем и компрессор тоже
        }
  
  #endif        
  Relay=r;      
  journal.jprintf(" Relay %s: ",name);if (Relay) journal.jprintf("ON\n"); else journal.jprintf("OFF\n"); 
  return err;
}

// ПЕРЕГРУЗКА Установить реле в состояние r на входе int 0 и 1
int8_t devRelay::set_Relay(int16_t r)    
{
  if (r==0) set_OFF();
  else if (r==1) set_ON();
  return OK;  // Не верный параметр на входе ничего не делаем
}
// ------------------------------------------------------------------------------------------
// ЭРВ ТОЛЬКО ОДНА ШТУКА ВСЕГДА (не массив) ---------------------------------------- --------
 #ifdef EEV_DEF     
const char *noteEEV = {"Электронно регулируемый вентиль" };// Описание
const char *nameEEV = {"EEV"} ;  //  Имя
// Инициализация ЭРВ
void devEEV::initEEV()
{
  EEV=-1;                               // шаговик в непонятном положении
  setZero=true;                         // Признакнеобходимости обнуления счетчика шагов EEV
  err=OK;                               // Ошибок нет
  Pid_start=EEV_START;                  // Откуда стартует ПИД
  Resume(EEV_START);                    // Обнулить рабочие переменные
  halfPos=(EEV_STEPS-EEV_MIN_STEPS)/2+EEV_MIN_STEPS;// Позиция шаговика - половина диапазона ЭРВ
  timeIn=10;                            // Постоянная интегрирования времени в секундах ЭРВ СЕКУНДЫ
  Overheat=0;                           // Перегрев текущий (сотые градуса)
  tOverheat= DEFAULT_OVERHEAT;          // Перегрев ЦЕЛЬ (сотые градуса)
  typeFreon = (TYPEFREON) DEFAULT_FREON_TYPE;
  ruleEEV = (RULE_EEV) DEFAULT_RULE_EEV;

  #ifdef DEFAULT_EEV_Kp
  Kp = DEFAULT_EEV_Kp;
  Ki = DEFAULT_EEV_Ki;
  Kd = DEFAULT_EEV_Kd;
  EEV_KP_ERR 		= 100;
  EEV_SPEED 		= 40;
  EEV_PSTART     	= 150;
  EEV_START      	= 100;
  EEV_MIN_STEPS  	= 10;
  EEV_HOLD_MOTOR 	= false;
  DELAY_ON_PID_EEV  = 5;
  DELAY_ON3_EEV     = 3;
  DELAY_START_POS   = 5;
  DELAY_OFF_EEV     = 3;
  #else
  Kp = 100;                             // Коэф пропорц.
  Ki = 0;                               // Коэф интегр.  для настройки Ki=0
  Kd = 100;                             // Коэф дифф.
  #endif
  Correction=0;                         // Поправка в градусах для правила работы ЭРВ «TEVAOUT-TEVAIN».
  manualStep=halfPos;                   // Число шагов открытия ЭРВ для правила работы ЭРВ «Manual»
  testMode=NORMAL;                      // Значение режима тестирования
  flags=0x00;                           // флаги  0 - наличие EEV
  SETBIT1(flags,fPresent);              // наличие датчика в текушей конфигурации
  Chart.init(true);                     // инициалазация статистики
  maxEEV=EEV_STEPS ;                    // Максимальное число шагов ЭРВ (диапазон)
  minEEV=EEV_MIN_STEPS;                 // Тчисло шагов при котором ЭРВ начинает открываться (холостой ход) может быть равным 0
  name=(char*)nameEEV;                  // Присвоить имя
  note=(char*)noteEEV;                  // Присвоить описание
  
// Инициализация шагового двигателя ЭРВ   ВАЖНО ПРАВИЛЬНОЕ ПОДКЛЮЧЕНИЕ!!!
// Рабочие комбинации для подключения шаговика на 5 вольт 28BYJ-48
// So in the end, four entries worked: (8,10,11,9), (9,11,8,10), (10,8,9,11), and (11,9,10,8).
// http://www.utopiamechanicus.com/article/arduino-stepper-motor-setup-troubleshooting/
// Подключение двигателя 28BYJ-48. Драйвер ULN-2003 схема (8,10,11,9)
// http://42bots.com/tutorials/28byj-48-stepper-motor-with-uln2003-driver-and-arduino-uno/
      // ШАГОВИК на 5 вольт 28BYJ-48 5V  -----------------
      // нога 1 синий       - В
      // нога 2 фиолетовый  + А
      // нога 3 желтый      + В
      // нога 4 оранжевый   - А
      // нога 5 красный     + общий
      // ЭРВ 12 вольт ------------------------------------
      // нога 1 оранжевый  + А
      // нога 2 красный    + В
      // нога 3 желтый     - А
      // нога 4 черный     - В
      // нога 5 синий      + общий
      // Распиновка LM9333 -------------------------------
      // D24     нога 1
      // D26     нога 2
      // D25     нога 3
      // D27     нога 4
      // Распиновка ULN2003 -------------------------------
      // D24     нога 1
      // D25     нога 2
      // D26     нога 3
      // D27     нога 4
      // Инициализация библиотеки порядок указания фаз в функции initStepMotor +A +B -A -B
    
#ifdef DEMO
  stepperEEV.initStepMotor(maxEEV, PIN_EEV3_D26,PIN_EEV2_D25,PIN_EEV4_D27,PIN_EEV1_D24);          // для тестирования  на шаговике 5 вольт вроде работает
#else  
    #ifdef DRV_EEV_L9333                                                                          // использование драйвера L9333
      #ifdef  EEV_INVERT                                                                          // Признак инвертирования движения ЭРВ
         stepperEEV.initStepMotor(maxEEV,PIN_EEV4_D27,PIN_EEV2_D25,PIN_EEV3_D26,PIN_EEV1_D24);    // на 8 фазном работает 480 шагов обратное подключение
      #else
         stepperEEV.initStepMotor(maxEEV,PIN_EEV1_D24,PIN_EEV3_D26,PIN_EEV2_D25,PIN_EEV4_D27);    // на 8 фазном работает 480 шагов прямое подключение
      #endif
    #else    
      #ifdef  EEV_INVERT                                                                          // Признак инвертирования движения ЭРВ
         stepperEEV.initStepMotor(maxEEV,PIN_EEV4_D27,PIN_EEV3_D26,PIN_EEV2_D25,PIN_EEV1_D24);    // на 8 фазном работает 480 шагов обратное подключение
      #else
         stepperEEV.initStepMotor(maxEEV,PIN_EEV1_D24,PIN_EEV2_D25,PIN_EEV3_D26,PIN_EEV4_D27);    // на 8 фазном работает 480 шагов прямое подключение
      #endif
    #endif  // DRV_EEV_L9333
#endif // DEMO   
stepperEEV.setSpeed(EEV_SPEED);   // Установить скорость движения
//journal.jprintf(" EEV init: OK\r\n"); 
}

// Восстановление слежения ЭРВ, если конечно задача запущена
void devEEV::Resume(uint16_t pos)
{
  fPause=false;                          // не пауза
  Pid_start=pos;                         // откуда стартует ПИД регулятор обновление в функции Resume
  resetPID();                            // Cброс служебных переменных ПИД
}
 
// Запустить ЭРВ - возвращает ок или код ошибки
// На стартовую позицию не выводит
 int8_t devEEV::Start()
 {
  
   Resume(EEV_START);
   EEV=0;
   err=OK;                               // Ошибок нет
   if(!GETBIT(flags,fPresent)) {journal.jprintf(" EEV not present, EEV disable\n"); return err;}  // если ЭРВ нет то ничего не делаем
   journal.jprintf(" EEV set zero\n"); 
   set_zero();                           // установить 0
 //  journal.jprintf(" EEV set EEV_START: %d\n",EEV_START); 
 //  set_EEV(EEV_START);      // Выставить положение ЭРВ - EEV_START
  return OK;                  
 }
 // Гарантированно (шагов больше чем диапазон) закрыть ЭРВ возвращает код ошибки
int8_t devEEV::set_zero()    
{
setZero=true;                                             // Признак обнуления счетчика шагов EEV  Ставить в начале!!
EEV=-1;   
if (testMode!=SAFE_TEST) stepperEEV.step(-EEV_STEPS-40);  // не  SAFE_TEST - работаем
else EEV=0;                                               // SAFE_TEST только координаты меняем
return OK;
}

 // Перейти на позицию абсолютную возвращает код ошибки
 // Отрицательные значения ЛЮБЫЕ признак установки 0
int8_t devEEV::set_EEV(int x)                  
{
  err=OK;
  if(x>EEV_STEPS)           { err=ERR_MAXERV; return err;   }    // Выход за верхнюю границу
  if(x<0)                   { err=ERR_MINERV; return err;   }    // Выход за нижнюю границу
  if (!(GETBIT(flags,fPresent)))  { err=ERR_DEVICE; return err;   }    // ЭРВ не установлен
  if (testMode!=SAFE_TEST) stepperEEV.step(x);                   // не  SAFE_TEST - работаем
  else EEV=x;                                                    // SAFE_TEST только координаты меняем
 return err;  
}
// Вычислить текущий перегрев, вычисляется каждое измерение (опрос датчиков)
// На входе текущие параметры датчиков, для всех вариантов на входе  TEVAOUT,TEVAIN, Press
// Если датчик давления отсутствует до давление будет -1000, и по этому опредяляем его наличие в конфигурации и как определять перегрев
// Если ЭРВ запрещена в конфигурации, то перегрев не вычисляется =0 и сразу выходим
 int16_t devEEV::set_Overheat(int16_t rto,int16_t out, int16_t in, int16_t p) 
 {
 if (!get_present()) {Overheat=0; err=OK; return Overheat;} // ЭРВ в конфиге нет
// вычисляется в зависимости от алгоритма
//   Serial.print(rto);Serial.print("-");Serial.print(out);Serial.print("-");Serial.print(in);Serial.print("-"); Serial.println(p);
if (testMode!=NORMAL)   // если режим тестирования приоритет выше чем у демо !!!
  {                                                                      
       switch (ruleEEV)  // определение доступности элемента
        {
          case  TEVAOUT_TEVAIN: if ((HP.sTemp[TEVAOUT].get_present())&&(HP.sTemp[TEVAIN].get_present())) Overheat=out-in+Correction; else {err=ERR_TYPE_OVERHEAT;set_Error(err,name);} break;
          case  TRTOOUT_TEVAIN: if ((HP.sTemp[TRTOOUT].get_present())&&(HP.sTemp[TEVAIN].get_present())) Overheat=rto-in+Correction; else { err=ERR_TYPE_OVERHEAT;set_Error(err,name);} break;
          case  TEVAOUT_PEVA:   if ((HP.sTemp[TEVAOUT].get_present())&&(HP.sADC[PEVA].get_present()))  Overheat=out-PressToTemp(p,typeFreon)+Correction; else {err=ERR_TYPE_OVERHEAT;set_Error(err,name);} break;
          case  TRTOOUT_PEVA:   if ((HP.sTemp[TRTOOUT].get_present())&&(HP.sADC[PEVA].get_present()))  Overheat=rto-PressToTemp(p,typeFreon)+Correction; else {err=ERR_TYPE_OVERHEAT;set_Error(err,name);} break;
          case  TABLE:         // По умолчанию
          case  MANUAL:         
          default:             if ((HP.sTemp[TRTOOUT].get_present())&&(HP.sADC[PEVA].get_present()))       Overheat=rto-PressToTemp(p,typeFreon)+Correction; 
                               else if((HP.sTemp[TEVAOUT].get_present())&&(HP.sADC[PEVA].get_present()))   Overheat=out-PressToTemp(p,typeFreon)+Correction;
                               else if((HP.sTemp[TRTOOUT].get_present())&&(HP.sTemp[TEVAIN].get_present()))  Overheat=rto-in+Correction; 
                               else if ((HP.sTemp[TEVAOUT].get_present())&&(HP.sTemp[TEVAIN].get_present())) Overheat=out-in+Correction; 
                               else {err=ERR_TYPE_OVERHEAT;set_Error(err,name);} break;
        }
    
  }
 else // нормальный режим
 {
    #ifdef DEMO
      Overheat=abs(out-in);              // вычислить перегрев для демки всегда больше 0
    #else 

             switch (ruleEEV)  // определение доступности элемента
              {
                case  TEVAOUT_TEVAIN: if ((HP.sTemp[TEVAOUT].get_present())&&(HP.sTemp[TEVAIN].get_present())) Overheat=out-in+Correction; else {err=ERR_TYPE_OVERHEAT;set_Error(err,name);} break;
                case  TRTOOUT_TEVAIN: if ((HP.sTemp[TRTOOUT].get_present())&&(HP.sTemp[TEVAIN].get_present())) Overheat=rto-in+Correction; else {err=ERR_TYPE_OVERHEAT;set_Error(err,name);} break;
                case  TEVAOUT_PEVA:   if ((HP.sTemp[TEVAOUT].get_present())&&(HP.sADC[PEVA].get_present()))  Overheat=out-PressToTemp(p,typeFreon)+Correction; else {err=ERR_TYPE_OVERHEAT;set_Error(err,name);} break;
                case  TRTOOUT_PEVA:   if ((HP.sTemp[TRTOOUT].get_present())&&(HP.sADC[PEVA].get_present()))  Overheat=rto-PressToTemp(p,typeFreon)+Correction; else {err=ERR_TYPE_OVERHEAT;set_Error(err,name);} break;
                case  TABLE:         // По умолчанию
                case  MANUAL:         
                default:             if ((HP.sTemp[TRTOOUT].get_present())&&(HP.sADC[PEVA].get_present()))       Overheat=rto-PressToTemp(p,typeFreon)+Correction; 
                                     else if((HP.sTemp[TEVAOUT].get_present())&&(HP.sADC[PEVA].get_present()))   Overheat=out-PressToTemp(p,typeFreon)+Correction;
                                     else if((HP.sTemp[TRTOOUT].get_present())&&(HP.sTemp[TEVAIN].get_present()))  Overheat=rto-in+Correction; 
                                     else if ((HP.sTemp[TEVAOUT].get_present())&&(HP.sTemp[TEVAIN].get_present())) Overheat=out-in+Correction; 
                                     else {err=ERR_TYPE_OVERHEAT;set_Error(err,name);} break;
              }
    #endif 
    
 }
  err=OK;
 // if (Overheat<-100) err=set_Error(ERR_OVERHEAT,name);   // Отрицательный перегрев????? даем запас -1 градуса
 return Overheat;  
 }



// УСТАНОВКА Установить целевой перегрев
int8_t devEEV::set_tOverheat(int16_t x)
{
  err=OK;
  if ((x>0)&&(x<=1500)) { if(tOverheat!=x) resetPID(); tOverheat=x;return OK;}
  else err=WARNING_VALUE;
  return  err;                        
}   

// УСТАНОВКА Установить постоянную интегрирования времени в секундах ЭРВ
int8_t devEEV::set_timeIn(int16_t x)
{
  err=OK;
  if ((x>=5)&&(x<=600)) { if(timeIn!=x) resetPID(); timeIn=x; return OK;}
  else err=WARNING_VALUE;
  return  err;                        
}  

// УСТАНОВКА Установить пропорциональную составляющую (хранится в СОТЫХ)
int8_t devEEV::set_Kpro(int16_t x)
{
  err=OK;
  if ((x>=0)&&(x<=5000)) { if(Kp!=x) resetPID(); Kp=x;return OK;}
  else err=WARNING_VALUE;
  return  err;                      
}  

// УСТАНОВКА Установить интегральнаяую составляющую (хранится в СОТЫХ)
int8_t devEEV::set_Kint(int16_t x)
{
  err=OK;
  if ((x>=0)&&(x<=1000)) { if(Ki!=x) resetPID(); Ki=x; return OK;}
  else err=WARNING_VALUE;
  return  err;                        
} 

// УСТАНОВКА Установить дифференциальную составляющую (хранится в СОТЫХ)
int8_t devEEV::set_Kdif(int16_t x)
{
  err=OK;
  if ((x>=0)&&(x<=1000)) { if(Kd!=x) resetPID(); Kd=x;return OK;}
  else err=WARNING_VALUE;
  return  err;               
} 

// УСТАНОВКА Установить поправку в градусах для правила работы ЭРВ «TEVAOUT-TEVAIN».
// Допустимый диапазон поправок -5 +5 градусов
int8_t  devEEV::set_Correction(int16_t x)
{
  err=OK;
  if ((x>=-500)&&(x<=500)) {if(Correction!=x)  Correction=x; return OK;}
  else err=WARNING_VALUE;
  return  err;
}
// установить число шагов открытия ЭРВ для правила работы ЭРВ «Manual»
int8_t  devEEV::set_manualStep(int16_t x)
{
  err=OK;
  if ((x>=minEEV)&&(x<=maxEEV)){ manualStep=x; return OK;}
  else err=WARNING_VALUE;
  return  err;
}

// Обновление ЭРВ - одна итерация алгоритма отслеживания
// на входе две температуры, используется для алгоритма table
int8_t devEEV::Update(int16_t teva, int16_t tcon)
{
  int16_t newEEV;               // Изменение положения ЭРВ
  #ifdef EEV_INT_PID            // использование ПИДА в целочисленной арифметике
   int32_t u,work_int, u_dif, u_int, u_pro; 
  #else
   float u, u_dif, u_int, u_pro; 
  #endif 
  
  if(!GETBIT(flags,fPresent)) {return err;}  // если ЭРВ нет то ничего не делаем
  if (fPause)  return err;      // если пауза то выходим
  newEEV=EEV;                   // в начале равно старому
  
  switch (ruleEEV)     // В зависмости от правила вычисления перегрева
  {
  case TEVAOUT_TEVAIN:
  case TRTOOUT_TEVAIN:
  case TEVAOUT_PEVA:
  case TRTOOUT_PEVA:
    {// ПИД регулятор для двух режимов
     // Уравнение ПИД регулятора в конечных разностях.
     // Cp, Ci, Cd – коэффициенты дискретного ПИД регулятора;
     // u(t) = P (t) + I (t) + D (t);
     // P (t) = Kp * e (t);
     // I (t) = I (t — 1) + Ki * e (t);
     // D (t) = Kd * {e (t) — e (t — 1)};
     // T – период дискретизации(период, с которым вызывается ПИД регулятор).
      #ifdef EEV_INT_PID                                            // использование ПИДА в целочисленной арифметике
         #define EEV_PID_SCALE     (int32_t)(100*100)               // ДЕСЯТИТЫСЯЧНЫЕ Масштаб расчета пид ЭРВ = СОТЫЕ для градусов * СОТЫЕ коэффициенты
         #define EEV_INT_MAX_STEP  (int32_t)(5*EEV_PID_SCALE)       // максимальное воздействие от интегральной составляющей в шагах
         #define EEV_PID_MAX_STEP  (int32_t)(50*EEV_PID_SCALE)      // максимальное изменение на одной итерации ПИД
         
         errPID=Overheat-tOverheat;                                 // Текущая ошибка, в СОТЫХ градуса (+ это привышение цели, перегрев больше и ЭРВ надо открывать для его уменьшения)
         if (Ki>0)                                                  // Расчет интегральной составляющей
         {
          temp_int=temp_int+Ki*errPID;                              // Интегральная составляющая, с накоплением, в ДЕСЯТИТЫСЯЧНЫХ (градусы 100 и интегральный коэффициент 100)
          // Ограничение диапзона изменения EEV_INT_MAX_STEP шагов за одну итерацию ПИД
          if (temp_int>EEV_INT_MAX_STEP)   temp_int=EEV_INT_MAX_STEP; 
          if (temp_int<-EEV_INT_MAX_STEP)  temp_int=-EEV_INT_MAX_STEP; 
         }
         else temp_int=0;                                            // если Кi равен 0 то интегрирование не используем
         u_int=temp_int;
        
         // Дифференцальная составляющая
         u_dif=Kd*(errPID-pre_errPID);                               // ДЕСЯТИТЫСЯЧНЫЕ Положительная составляющая - ошибка растет (воздействие надо увеличиить)  Отрицательная составляющая - ошибка уменьшается (воздействие надо уменьшить)
         
         // Пропорциональная составляющая
         u_pro=Kp*errPID;                                            // ДЕСЯТИТЫСЯЧНЫЕ
         
         // Общее воздействие
         u=u_pro+u_int+u_dif;                                       // В  градусы 100 коэффициенты 100 ДЕСЯТИТЫСЯЧНЫЕ
   //      Serial.print("u="); Serial.println(u);

   //      if (abs(errPID)<EEV_KP_ERR) u=((abs(errPID*100/EEV_KP_ERR))*u)/100;       // В близи уменьшить воздействие
   
         if (u>EEV_PID_MAX_STEP)   u=EEV_PID_MAX_STEP;              // ограничение одной итерации 50 шагами
         if (u<-EEV_PID_MAX_STEP)  u=-EEV_PID_MAX_STEP;
         newEEV=round(u/EEV_PID_SCALE)+EEV;                         // Округление и добавление предудущего значения
   //      Serial.print("newEEV="); Serial.println(newEEV);

         pre_errPID=errPID;                                         // запомнить предыдущую ошибку
    #else   // использование флоат, работатет
         errPID=((float)(Overheat-tOverheat))/100.0;                // Текущая ошибка, переводим в градусы (+ это привышение цели, перегрев больше и ЭРВ надо открывать для его уменьшения)
         if (Ki>0)                                                  // Расчет интегральной составляющей
         {
          temp_int=temp_int+((float)Ki*errPID)/100.0;               // Интегральная составляющая, с накоплением делить на 100
          // Ограничение диапзона изменения 20 шагов за один шаг ПИД
          #define EEV_MAX_STEP  5
          if (temp_int>EEV_MAX_STEP)  temp_int=EEV_MAX_STEP; 
          if (temp_int<-1.0*EEV_MAX_STEP)  temp_int=-1.0*EEV_MAX_STEP; 
         }
         else temp_int=0;                                           // если Кi равен 0 то интегрирование не используем
         u_int=temp_int;
        
         // Дифференцальная составляющая
         u_dif=((float)Kd*(errPID-pre_errPID))/100.0;               // Положительная составляющая - ошибка растет (воздействие надо увеличиить)  Отрицательная составляющая - ошибка уменьшается (воздействие надо уменьшить)
         
         // Пропорциональная составляющая
         u_pro=(float)Kp*errPID/100.0;
         
         // Общее воздействие
         u=u_pro+u_int+u_dif;

         if (abs(errPID)<(EEV_KP_ERR/100.0)) u_pro=(abs((errPID*100.0)/EEV_KP_ERR))*u_pro;            // В близи уменьшить воздействие
         newEEV=round(u)+EEV;                                        // Округление и добавление предудущего значения
         pre_errPID=errPID;                                          // запомнить предыдущую ошибку
    #endif   // EEV_INT_PID
     
        if (newEEV>=maxEEV)   newEEV=maxEEV;                         // ограничение диапазона
        if (newEEV<=minEEV)   newEEV=minEEV;
  //      Serial.print("errPID="); Serial.print(errPID,4);Serial.print(" newEEV=");Serial.print(newEEV);Serial.print(" EEV=");Serial.println(EEV);
    } break;
  case TABLE:  newEEV = TempToEEV(teva,tcon); break;
  case MANUAL: newEEV = manualStep; break;
 }

 //  Передвинуть шаговик ЭРВ в позицию (абсолютную) EEV если есть изменения
if (fStart)           {fStart=false;  journal.jprintf(" Included tracking PID EEV . . .\n");return err;}   // Первая итерация пида - пропуск движения и сброс флага первой итерации
else if (newEEV!=EEV) { set_EEV(newEEV); return err;}                                                      // Не первая итерация - движение EEV
return err;
}

static int16_t OverHeatCor_period = 0; // Только один ЭРВ.
void   devEEV::CorrectOverheat(void)
{
	if(!GETBIT(flags, fCorrectOverHeat)) return;
	if(rtcSAM3X8.unixtime() - HP.get_startCompressor() > OverHeatCor.Delay && ++OverHeatCor_period > OverHeatCor.Period) {
		OverHeatCor_period = 0;
		uint16_t delta = HP.sTemp[TCOMP].get_Temp() - HP.get_temp_condensing();
		if(delta > OverHeatCor.TDIS_TCON + OverHeatCor.TDIS_TCON_Thr) {
			delta = OverHeatCor.TDIS_TCON + OverHeatCor.TDIS_TCON_Thr - delta;	// Перегрев большой - уменьшаем
		} else if(delta < OverHeatCor.TDIS_TCON - OverHeatCor.TDIS_TCON_Thr) {
			delta = OverHeatCor.TDIS_TCON - OverHeatCor.TDIS_TCON_Thr - delta;	// Перегрев маленький - увеличиваем
		}
		tOverheat += (int32_t) delta * OverHeatCor.K / 1000;
		if(tOverheat > OverHeatCor.OverHeatMax) tOverheat = OverHeatCor.OverHeatMax;
		else if(tOverheat < OverHeatCor.OverHeatMin) tOverheat = OverHeatCor.OverHeatMin;
	}
}

// Записать настройки в eeprom i2c на входе адрес с какого, на выходе конечный адрес, если число меньше 0 это код ошибки
int32_t devEEV::save(int32_t adr)
{
    if (writeEEPROM_I2C(adr, (byte*)&timeIn, (byte*)&flags - (byte*)&timeIn + sizeof(flags))) {
    	set_Error(ERR_SAVE_EEPROM,name);
    	return ERR_SAVE_EEPROM;
    }
    adr += (byte*)&flags - (byte*)&timeIn + sizeof(flags);
    return adr;   
}

// Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, если число меньше 0 это код ошибки
int32_t devEEV::load(int32_t adr)
{
	if (readEEPROM_I2C(adr, (byte*)&timeIn, (byte*)&flags - (byte*)&timeIn + sizeof(flags))) {
		set_Error(ERR_LOAD_EEPROM,name);
		return ERR_LOAD_EEPROM;
	}
	adr += (byte*)&flags - (byte*)&timeIn + sizeof(flags);
	SETBIT1(flags, fPresent); 									// ЭРВ всегда есть!!!
	return adr;
}

// Считать настройки из буфера на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t devEEV::loadFromBuf(int32_t adr,byte *buf) 
{
	memcpy((byte*)&timeIn, buf+adr, (byte*)&flags - (byte*)&timeIn + sizeof(flags));
	adr += (byte*)&flags - (byte*)&timeIn + sizeof(flags);
	SETBIT1(flags, fPresent); 									// ЭРВ всегда есть!!!
	return adr;
}
// Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
uint16_t devEEV::get_crc16(uint16_t crc) 
{
	uint16_t i;
	for(i = 0; i < (byte*)&flags - (byte*)&timeIn + sizeof(flags); i++)
		crc = _crc16(crc,*((byte*)&timeIn + i));  // CRC16 структуры
	return crc;
}

// Сброс пид регулятора
void devEEV::resetPID()
{
  temp_int = 0;                          // Служебная переменная интегрирования
  errPID=0;                              // Текущая ошибка ПИД регулятора
  pre_errPID=0;                          // Предыдущая ошибка ПИД регулятора
  tmpTime=timeIn;                        // ТЕКУЩАЯ постоянная интегрирования времени в секундах ЭРВ
  fStart=true;                           // Признак работы пид с начала (пропуск первой итерации)
}

void devEEV::variable(uint8_t getset, char *ret, char *var, float value)
{
	if(strcmp(var, "cCOR")==0) {
    	if(getset) flags = (flags & ~(1<<fCorrectOverHeat)) | (value == 0 ? 0 : (1<<fCorrectOverHeat));
    	itoa((flags & (1<<fCorrectOverHeat)) != 0, ret, 10);
	} else if(strcmp(var, "cDELAY")==0) {
    	if(getset) OverHeatCor.Delay = value;
    	itoa(OverHeatCor.Delay, ret, 10);
    } else if(strcmp(var, "cPERIOD")==0) {
    	if(getset) OverHeatCor.Period = value;
    	itoa(OverHeatCor.Period, ret, 10);
    } else if(strcmp(var, "cDELTA")==0) {
    	if(getset) OverHeatCor.TDIS_TCON = value * 100.0 +0.005;
    	ftoa(ret, (float)OverHeatCor.TDIS_TCON / 100.0, 2);
    } else if(strcmp(var, "cDELTAT")==0) {
    	if(getset) OverHeatCor.TDIS_TCON_Thr = value * 100.0  +0.005;
    	ftoa(ret, (float)OverHeatCor.TDIS_TCON_Thr / 100.0, 2);
    } else if(strcmp(var, "cKF")==0) {
    	if(getset) OverHeatCor.K = value * 1000.0 + 0.0005;
    	ftoa(ret, (float)OverHeatCor.K / 1000.0, 3);
    } else if(strcmp(var, "cOH_MIN")==0) {
    	if(getset) OverHeatCor.OverHeatMin = value * 100.0 +0.005;
    	ftoa(ret, (float)OverHeatCor.OverHeatMin / 100.0, 2);
    } else if(strcmp(var, "cOH_MAX")==0) {
    	if(getset) OverHeatCor.OverHeatMax = value * 100.0 +0.005;
    	ftoa(ret, (float)OverHeatCor.OverHeatMax / 100.0, 2);
    }
}

 // Получить параметр ЭРВ в виде строки
 // var - строка с параметром ret-выходная строка, ответ ДОБАВЛЯЕТСЯ
char* devEEV::get_paramEEV(char *var, char *ret)
{
char temp[10+1];	
	if(strcmp(var, eev_POS)==0) {
      if(stepperEEV.isBuzy())  strcat(ret,"<<");  // признак движения		
	  strcat(ret,itoa(EEV,temp,10)); 
	  if (stepperEEV.isBuzy())  strcat(ret,">>");  // признак движения
	} else if(strcmp(var, eev_POSp)==0){
	  if(stepperEEV.isBuzy())  strcat(ret,"<<");  // признак движения		
	  strcat(ret,itoa((int32_t) EEV * 100 / maxEEV,temp,10)); 	
	  strcat(ret,"%");
 	  if (stepperEEV.isBuzy())  strcat(ret,">>");  // признак движения 
	} else if(strcmp(var, eev_POSpp)==0){
      if(stepperEEV.isBuzy())  strcat(ret,"<<");  // признак движения			
	  strcat(ret,itoa(EEV,temp,10));
	  strcat(ret,"(");
	  strcat(ret,itoa((int32_t) EEV * 100 / maxEEV,temp,10)); 
	  strcat(ret,"%)");	
	  if (stepperEEV.isBuzy())  strcat(ret,">>");  // признак движения
	} else if(strcmp(var, eev_OVERHEAT)==0){
	  strcat(ret,ftoa(temp,round(Overheat*100.0+0.5)/100.0,2)); 	
	} else if(strcmp(var, eev_ERROR)==0){
	   strcat(ret,itoa(err,temp,10)); 
	} else if(strcmp(var, eev_MIN)==0){
	   strcat(ret,itoa(minEEV,temp,10)); 
	} else if(strcmp(var, eev_MAX)==0){
	   strcat(ret,itoa(maxEEV,temp,10)); 
	} else if(strcmp(var, eev_TIME)==0){
	   strcat(ret,itoa(timeIn,temp,10)); 
	} else if(strcmp(var, eev_TARGET)==0){
	   strcat(ret,ftoa(temp,(float)(tOverheat/100.0),2));
	} else if(strcmp(var, eev_KP)==0){
	   strcat(ret,ftoa(temp,(float)(Kp/100.0),3)); 
	} else if(strcmp(var, eev_KI)==0){
	    strcat(ret,ftoa(temp,(float)(Ki/100.0),3)); 
	} else if(strcmp(var, eev_KD)==0){
	   strcat(ret,ftoa(temp,(float)(Kd/100.0),3)); 
	} else if(strcmp(var, eev_CONST)==0){
	    strcat(ret,ftoa(temp,(float)(Correction/100.0),2)); 
	} else if(strcmp(var, eev_MANUAL)==0){
	     strcat(ret,itoa(manualStep,temp,10)); 
	} else if(strcmp(var, eev_FREON)==0){
         for(uint8_t i=0;i<=R717;i++) // Формирование списка фреонов
            { strcat(ret,noteFreon[i]); strcat(ret,":"); if(i==get_typeFreon()) strcat(ret,cOne); else strcat(ret,cZero); strcat(ret,";");  }
	}   else if(strcmp(var, eev_RULE)==0){
         for(uint8_t i=TEVAOUT_TEVAIN;i<=MANUAL;i++) // Формирование списка фреонов
            { strcat(ret,noteRuleEEV[i]); strcat(ret,":"); if(i==get_ruleEEV()) strcat(ret,cOne); else strcat(ret,cZero); strcat(ret,";");  }
	} else if(strcmp(var, eev_NAME)==0){
	     strcat(ret,name); 
	} else if(strcmp(var, eev_NOTE)==0){
	     strcat(ret,note); 
	} else if(strcmp(var, eev_REMARK)==0){
	     strcat(ret,noteRemarkEEV[get_ruleEEV()]); 
	} else if(strcmp(var, eev_PINS)==0){
	      strcat(ret,"D");  strcat(ret,itoa(PIN_EEV1_D24,temp,10));
	      strcat(ret," D"); strcat(ret,itoa(PIN_EEV2_D25,temp,10));
	      strcat(ret," D"); strcat(ret,itoa(PIN_EEV3_D26,temp,10));
	      strcat(ret," D"); strcat(ret,itoa(PIN_EEV4_D27,temp,10));
	} else if(strcmp(var, eev_cCORRECT)==0){
    	strcat(ret,itoa((flags & (1<<fCorrectOverHeat)) != 0, temp, 10));
	} else if(strcmp(var, eev_cDELAY)==0){
    	strcat(ret,itoa(OverHeatCor.Delay, temp, 10));
    } else if(strcmp(var, eev_cPERIOD)==0){
    	strcat(ret, itoa(OverHeatCor.Period, temp, 10));
    } else if(strcmp(var, eev_cDELTA)==0){
    	strcat(ret, ftoa(temp, (float)(OverHeatCor.TDIS_TCON/100.0), 2));
    } else if(strcmp(var, eev_cDELTAT)==0){
    	strcat(ret,ftoa(temp, (float)(OverHeatCor.TDIS_TCON_Thr/100.0), 2));
    } else if(strcmp(var, eev_cKF)==0){
       	strcat(ret, ftoa(temp, (float)(OverHeatCor.K/1000.0), 3));
    } else if(strcmp(var, eev_cOH_MIN)==0){
    	strcat(ret,ftoa(temp, (float)(OverHeatCor.OverHeatMin/100.0), 2));
    } else if(strcmp(var, eev_cOH_MAX)==0){
    	strcat(ret,ftoa(temp, (float)(OverHeatCor.OverHeatMax/100.0), 2));
    } else strcat(ret,"E10");
   
  return ret;              
}
// Установить параметр ЭРВ из флоат параметр var
// в случае успеха возврщает true
boolean devEEV::set_paramEEV(char *var,float x)
{
float temp;	
    if(strcmp(var, eev_POS)==0) {
	  set_EEV((int)x); return true;
	} else if(strcmp(var, eev_POSp)==0){
      temp=x*EEV/maxEEV; 		
      set_EEV((int)temp); return true;
	} else if(strcmp(var, eev_POSpp)==0){
	  return true;  // не имеет смысла - только чтение
	} else if(strcmp(var, eev_OVERHEAT)==0){
	  return true;  // не имеет смысла - только чтение
	} else if(strcmp(var, eev_ERROR)==0){
	  return true;  // не имеет смысла - только чтение
	} else if(strcmp(var, eev_MIN)==0){
	  return true;  // не имеет смысла - только чтение
	} else if(strcmp(var, eev_MAX)==0){
	  return true;  // не имеет смысла - только чтение
	} else if(strcmp(var, eev_TIME)==0){
	  if ((x>=5)&&(x<=600)) { if(timeIn!=x) resetPID(); timeIn=x; return true;} else false;	// секунды
	} else if(strcmp(var, eev_TARGET)==0){ 
	  if ((x>0.0)&&(x<=15.0)) { x=x*100;if(tOverheat!=x) resetPID(); tOverheat=(int)x; ;return true;}  else false;	// сотые градуса
	} else if(strcmp(var, eev_KP)==0){
	   if ((x>=0)&&(x<=50.0)) {x=x*100.0; if(Kp!=x) resetPID(); Kp=(int)x;return true;} else false;	// сотые
	} else if(strcmp(var, eev_KI)==0){
	   if ((x>=0)&&(x<=10.0)) {x=x*100.0; if(Ki!=x) resetPID(); Ki=(int)x; return true;} else false; // сотые	
	} else if(strcmp(var, eev_KD)==0){
	   if ((x>=0)&&(x<=10.0)) {x=x*100.0; if(Kd!=x) resetPID(); Kd=(int)x;return true;} else false;	// сотые
	} else if(strcmp(var, eev_CONST)==0){
	   if ((x>=-5.0)&&(x<=5.0)) {x=x*100.0; if(Correction!=x) resetPID(); Correction=(int)x; return true;}else false;	// сотые градуса
	} else if(strcmp(var, eev_MANUAL)==0){
	   if ((x>=minEEV)&&(x<=maxEEV)==0){ manualStep=x; return true;} else false;	// шаги
	} else if(strcmp(var, eev_FREON)==0){
        if ((x>0)&&(x<=R717)){ typeFreon=(TYPEFREON)x; return true;} else false;	// перечисляемый тип  
	}   else if(strcmp(var, eev_RULE)==0){
		if ((x>TEVAOUT_TEVAIN)&&(x<=MANUAL)){ ruleEEV=(RULE_EEV)x; return true;} else false;	// перечисляемый тип  
 	} else if(strcmp(var, eev_NAME)==0){
	    return true;  // не имеет смысла - только чтение
	} else if(strcmp(var, eev_NOTE)==0){
	    return true;  // не имеет смысла - только чтение
	} else if(strcmp(var, eev_REMARK)==0){
	    return true;  // не имеет смысла - только чтение
	} else if(strcmp(var, eev_PINS)==0){
	    return true;  // не имеет смысла - только чтение
	} else if(strcmp(var, eev_cCORRECT)==0){
    	if (x==0) SETBIT0(flags, fCorrectOverHeat); else SETBIT1(flags, fCorrectOverHeat);
	} else if(strcmp(var, eev_cDELAY)==0){
		if ((x>=0)&&(x<=900)) { if(OverHeatCor.Delay!=x) OverHeatCor.Delay=x; return true;} else false;	// секунды
    } else if(strcmp(var, eev_cPERIOD)==0){
		if ((x>=1)&&(x<=20)) { if(OverHeatCor.Period!=x) resetPID(); OverHeatCor.Period=x; return true;} else false;	// циклы ЭРВ
    } else if(strcmp(var, eev_cDELTA)==0){
        if ((x>=0.0)&&(x<=25.0)) {OverHeatCor.TDIS_TCON=(int)(x*100.0); return true;}else false;	// сотые градуса
    } else if(strcmp(var, eev_cDELTAT)==0){
        if ((x>=0.0)&&(x<=10.0)) {OverHeatCor.TDIS_TCON_Thr=(int)(x*100.0); return true;}else false;	// сотые градуса
    } else if(strcmp(var, eev_cKF)==0){
    	if ((x>=0.0)&&(x<=10.0)) {OverHeatCor.K=(int)(x*1000.0); return true;}else false;	// тысячные
    } else if(strcmp(var, eev_cOH_MIN)==0){
        if ((x>=0.0)&&(x<=50.0)) {OverHeatCor.OverHeatMin=(int)(x*100.0); return true;}else false;	// сотые градуса
    } else if(strcmp(var, eev_cOH_MAX)==0){
        if ((x>=0.0)&&(x<=50.0)) {OverHeatCor.OverHeatMax=(int)(x*100.0); return true;}else false;	// сотые градуса
    } else return false;
}

#endif

#ifndef FC_VACON
// ------------------------------------------------------------------------------------------
// ЧАСТОТНЫЙ ПРЕОБРАЗОВАТЕЛЬ ТОЛЬКО ОДНА ШТУКА ВСЕГДА (не массив) ---------------------------
//#define IS_ANALOG       (GETBIT(flags,fAnalog)?true:false)                       // Проверка на требуемый тип управления  если аналоговый то выдает TRUE
                                                   
int8_t devOmronMX2::initFC()
{ 
 // uint16_t hWord,lWord;
  
  err=OK;                           // ошибка частотника (работа) при ошибке останов ТН
  numErr=0;                           // число ошибок чтение по модбасу для статистики
  number_err=0;                     // Число ошибок связи при превышении FC_NUM_READ блокировка инвертора
  FC=0;                             // Целевая частота частотика
  freqFC=0;                          // текущая частота инвертора
  power=0;                          // Тееущая мощность частотника
  current=0;                        // Текуший ток частотника
  startCompressor=0;                // время старта компрессора
  state=ERR_LINK_FC;               // Состояние - нет связи с частотником
  dac=0;                            // Текущее значение ЦАП
  level0=0;                         // Отсчеты ЦАП соответсвующие 0   мощности
  level100=4096;                    // Отсчеты ЦАП соответсвующие 100 мощности
  levelOff=10;                      // Минимальная мощность при котором частотник отключается (ограничение минимальной мощности)
  testMode=NORMAL;                  // Значение режима тестирования
  name=(char*)nameOmron;               // Имя
  note=(char*)noteFC_NONE;          // Описание инвертора   типа нет его
  
  flags=0x00;                       // флаги  0 - наличие FC
  SETBIT0(flags,fAuto);             // По умолчанию старт-стоп
//  SETBIT0(flags,fAnalog);           // Нет аналогового выхода
  if(!Modbus.get_present())         // modbus отсутствует
      {
       SETBIT0(flags,fFC);          // Инвертор не рабоатет
       journal.jprintf("%s, modbus not found, block.\n",name); 
       err=ERR_NO_MODBUS;
       return err; 
      }
  else if (DEVICEFC==true) SETBIT1(flags,fFC); // наличие частотника в текушей конфигурации
               
  if(get_present())  journal.jprintf("Invertor %s: present config\r\n",name); 
  else {journal.jprintf("Invertor %s: none config\r\n",name);return err;  }  // выходим если нет инвертора

  note=(char*)noteFC_OK;             // Описание инвертора есть
  ChartFC.init(get_present());               // инициалазация графика
  #ifdef FC_ANALOG_CONTROL                      // Аналоговое управление графики не нужны
  	  pin = PIN_DEVICE_FC;                // Ножка куда прицеплено FC
  	  analogWriteResolution(12);        // разрешение ЦАП 12 бит;
  	  analogWrite(pin,dac);
  	  ChartPower.init(false);                 // инициалазация графика
  	  ChartCurrent.init(false);               // инициалазация графика
  #else									// НЕ Аналоговое управление
      ChartPower.init(get_present());            // инициалазация графика
      ChartCurrent.init(get_present());          // инициалазация графика
      err=Modbus.LinkTestOmronMX2();     // проверка связи с инвертором  xModbusSemaphore не используем так как в один поток
      check_blockFC();   
      if (err!=OK)  return err;          // связи нет выходим
      journal.jprintf("Test link Modbus %s: OK\r\n",name);   // Тест пройден

      // Если частотник работает то остановить его
      get_readState(); //  Получить состояние частотника
      switch (state)   // В зависимости от состояния
      {
        case 0:
        case 2: break;                                                         // ОСТАНОВКА ничего не делаем
        case 3: stop_FC();                                                      // ВРАЩЕНИЕ Послать команду стоп и ждать остановки
                while ((state!=2)||(state!=4)) { get_readState();journal.jprintf("Wait stop %s . . .\r\n",name); _delay(3000); } 
                break;
        case 4:                                                                // ОСТАНОВКА С ВЫБЕГОМ  ждать остановки
                break;
        case 5:stop_FC();                                                      // ТОЛЧОВЫЙ ХОД Послать команду стоп и ждать остановки
                while ((state!=2)||(state!=4)) { get_readState();journal.jprintf("Wait stop %s . . .\r\n",name); _delay(3000); } 
                break;
        case 6:                                                                // ТОРМОЖЕНИЕ ПОСТОЯННЫМ ТОКОМ
        case 7: err=ERR_MODBUS_STATE;set_Error(err,name);  break;             // ВЫПОЛНЕНИЕ ПОВТОРНОЙ ПОПЫТКИ Подъем ошибки на верх и останов ТН
        case 8: break;                                                         // АВАРИЙНОЕ ОТКЛЮЧЕНИЕ
        case 9: break;                                                         // ПОНИЖЕНОЕ ПИТАНИЕ
        case -1:break;
        default:err=ERR_MODBUS_STATE;set_Error(err,name);  break;             // Подъем ошибки на верх и останов ТН
      }
      if (err!=OK) return err;

       // Программирование инвертора Запись различных переменных
      # ifdef  FC_FULL_INIT            // Полная инициализация инвертора
         journal.jprintf(" Full init %s . . .\r\n",name);
         // 1. Источник команды  A001=03  по умолчанию 01
        if ((err=write_0x06_16(MX2_SOURCE_FR, 0x03))==OK)           journal.jprintf(" Setting frequency source (A001) %s: 0x03\r\n",name);
        else                                                        journal.jprintf(" Error setting frequency source (A001) %s code %d\r\n",name,err);
        // 2. Источник задания частоты A002=03  по умолчанию 01
        if ((err=write_0x06_16(MX2_SOURCE_CMD, 0x03))==OK)          journal.jprintf(" Setting run command source (A002) %s: 0x03\r\n",name);
        else                                                        journal.jprintf(" Error setting run command source (A002) %s code %d\r\n",name,err);
        // 3. A003=60  основная частота
        if ((err=write_0x06_16(MX2_BASE_FR, FC_BASE_FREQ/10))==OK) journal.jprintf(" Setting base frequency (A003) %s: %.2f [Hz]\r\n",name,FC_BASE_FREQ);
        else                                                        journal.jprintf(" Error setting base frequency (A003) %s code %d\r\n",name,err);
         // 4. установка максимальной частоты
        if ((err=write_0x06_16(MX2_MAX_FR, FC_MAX_FREQ/10))==OK)   journal.jprintf(" Setting maximum frequency (A004) %s: %.2f [Hz]\r\n",name,FC_MAX_FREQ/100.0);
        else                                                        journal.jprintf(" Error setting maximum frequency (A004) %s code %d\r\n",name,err);
        // 5. Время разгона
        if( write_0x10_32(MX2_ACCEL_TIME,FC_ACCEL_TIME)==OK)          journal.jprintf(" Setting acceleration time (F002) %s: %.2f [sec]\r\n",name,FC_ACCEL_TIME/100.0);
        else                                                        journal.jprintf(" Error setting acceleration time (F002) %s code %d\r\n",name,err);
        // 6. Торможения разгона
        if( write_0x10_32(MX2_DEACCEL_TIME,FC_DEACCEL_TIME)==OK)        journal.jprintf(" Setting deacceleration time (F003) %s: %.2f [sec]\r\n",name,FC_DEACCEL_TIME/100.0);
        else                                                        journal.jprintf(" Error setting deacceleration time (F003) %s code %d\r\n",name,err);
        // 7.  Разрешение торможения постоянным током A051=0
      //  if ((err=write_0x06_16(MX2_DC_BRAKING, 0x01))==OK)          journal.jprintf(" Setting DC braking enable (A051) %s: 0x01\r\n",name);
      //  else                                                        journal.jprintf(" Error setting DC braking enable (A051) %s code %d\r\n",name,err); 
        // 8. Выбор режима ПЧ b171=03
      //  if ((err=write_0x06_16(MX2_MODE, 0x03))==OK)                journal.jprintf(" Setting inverter mode selection (b171) %s: 0x03\r\n",name);
      //  else                                                        journal.jprintf(" Error setting inverter mode selection (b171) %s code %d\r\n",name,err); 
        // 9. B091=01 Выбор способа остановки
        if ((err=write_0x06_16(MX2_STOP_MODE, 0x01))==OK)           journal.jprintf(" Setting stop mode selection (b091) %s: 0x01\r\n",name);
        else                                                        journal.jprintf(" Error setting stop mode selection (b091) %s code %d\r\n",name,err);   
      #endif

#endif  // #ifndef FC_ANALOG_CONTROL
  // 10.Установить стартовую частоту
  set_targetFreq(FC_START_FREQ,true,FC_MIN_FREQ_USER ,FC_MAX_FREQ_USER);       // режим н знаем по этому границы развигаем
  return err;                       
}

// Установить целевую частоту
// параметр показывать сообщение сообщение или нет, два оставшихся параметра границы
int8_t  devOmronMX2::set_targetFreq(int16_t x,boolean show, int16_t _min, int16_t _max)
{ 
  err=OK;
  #ifdef DEMO
    if ((x>=_min)&&(x<=_max))                     // Проверка диапазона разрешенных частот
    {FC=x; if(show) journal.jprintf(" Set %s: %.2f [Hz]\r\n",name,FC/100.0);   return err;} // установка частоты OK  - вывод сообщения если надо
     else { journal.jprintf("%s: Wrong frequency %.2f\n",name,x/100.0); return WARNING_VALUE; } 
  #else   // Боевой вариант
  uint16_t hWord,lWord;
  uint8_t i;
  if ((!get_present())||(GETBIT(flags,fErrFC))) return err;    // выходим если нет инвертора или он заблокирован по ошибке
  if ((x>=_min)&&(x<=_max))                     // Проверка диапазона разрешенных частот
   {
  #ifndef FC_ANALOG_CONTROL                                    // Не аналоговое управление
          // Запись в регистры инвертора установленной частоты
          for(i=0;i<FC_NUM_READ;i++)  // Делаем FC_NUM_READ попыток
            {
              err=write_0x10_32(MX2_TARGET_FR,x);
              if (err==OK) break;                     // Команда выполнена
              _delay(100);
              journal.jprintf("%s: repeat set frequency\n",name);  // Выводим сообщение о повторной команде
            }
            
          if(err==OK)  { FC=x; if(show) journal.jprintf(" Set %s: %.2f [Hz]\r\n",name,FC/100.0);return err;} // установка частоты OK  - вывод сообщения если надо
          else {state=ERR_LINK_FC; SETBIT1(flags,fErrFC); set_Error(err,name);return err;}                 // генерация ошибки
   #else  // Аналоговое управление
         FC=x;
         dac=((level100-level0)*FC-0*level100)/(100-0);
         switch (testMode)  // РЕАЛЬНЫЕ Действия в зависимости от режима
             {
              case NORMAL:     analogWrite(pin,dac);  break; //  Режим работа не тест, все включаем
              case SAFE_TEST:                         break; //  Ничего не включаем
              case TEST:                              break; //  Включаем все кроме компрессора
              case HARD_TEST:  analogWrite(pin,dac);  break; //  Все включаем и компрессор тоже
             }
          if(show) journal.jprintf(" Set %s: %.2f [Hz]\r\n",name,FC/100.0); // установка частоты OK  - вывод сообщения если надо
   #endif 
    return err;
   }  // if ((x>=_min)&&(x<=_max)) 
   else { journal.jprintf("%s: Wrong frequency %.2f\n",name,x/100.0); return WARNING_VALUE; }  
  #endif  // DEMO
}

// Установить Отсчеты ЦАП соответсвующие 0   мощности
int8_t devOmronMX2::set_level0(int16_t x)
{
   if ((x>=0)&&(x<=4096)) { level0=x; return OK;} // Только правильные значения
   return WARNING_VALUE;
}
// Установить Отсчеты ЦАП соответсвующие 100 мощности
int8_t devOmronMX2::set_level100(int16_t x)
{
  if ((x>=0)&&(x<=4096)) { level100=x; return OK;} // Только правильные значения
  return WARNING_VALUE;
}
// Установить Минимальная мощность при котором частотник отключается (ограничение минимальной мощности)
int8_t devOmronMX2::set_levelOff(int16_t x)
{
  if ((x>=0)&&(x<=100))  { levelOff=x;  return OK;} // Только правильные значения
  return WARNING_VALUE;
}

// Записать настройки в eeprom i2c на входе адрес с какого, на выходе конечный адрес, если число меньше 0 это код ошибки
int32_t devOmronMX2::save(int32_t adr)
{
byte f=0;   
if (writeEEPROM_I2C(adr, (byte*)&level0, sizeof(level0)))   { set_Error(ERR_SAVE_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(level0);       // !save! Отсчеты ЦАП соответсвующие 0   мощности
if (writeEEPROM_I2C(adr, (byte*)&level100, sizeof(level100))) { set_Error(ERR_SAVE_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(level100);   // !save! Отсчеты ЦАП соответсвующие 100 мощности
if (writeEEPROM_I2C(adr, (byte*)&levelOff, sizeof(levelOff))) { set_Error(ERR_SAVE_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(levelOff);   // !save! Минимальная мощность при котором частотник отключается (ограничение минимальной мощности)
//if(GETBIT(flags,fAnalog)) f=f+(1<<fAnalog);                                                                                                      // Взять только флаги настроек
if (writeEEPROM_I2C(adr, (byte*)&f, sizeof(f))) { set_Error(ERR_SAVE_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(f);                        // !save! Флаги
return adr;   
}

// Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, если число меньше 0 это код ошибки
int32_t devOmronMX2::load(int32_t adr)
{
byte f;  
if (readEEPROM_I2C(adr, (byte*)&level0, sizeof(level0)))     { set_Error(ERR_LOAD_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(level0);     // !load! Отсчеты ЦАП соответсвующие 0   мощности
if (readEEPROM_I2C(adr, (byte*)&level100, sizeof(level100))) { set_Error(ERR_LOAD_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(level100);   // !load! Отсчеты ЦАП соответсвующие 100 мощности
if (readEEPROM_I2C(adr, (byte*)&levelOff, sizeof(levelOff))) { set_Error(ERR_LOAD_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(levelOff);   // !load! Минимальная мощность при котором частотник отключается (ограничение минимальной мощности)
if (readEEPROM_I2C(adr, (byte*)&f,sizeof(f)))                { set_Error(ERR_LOAD_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(f);          // !load! флаги
// проблема при чтении некоторые флаги не настройки? по этому устанавливаем их отдельно побитно
//if (GETBIT(f,fAnalog)) SETBIT1(flags,fAnalog); else SETBIT0(flags,fAnalog);  
return adr;
}
// Считать настройки из буфера на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t devOmronMX2::loadFromBuf(int32_t adr,byte *buf)
{
  byte f;  
  memcpy((byte*)&level0,buf+adr,sizeof(level0)); adr=adr+sizeof(level0);                  // Отсчеты ЦАП соответсвующие 0   мощности
  memcpy((byte*)&level100,buf+adr,sizeof(level100)); adr=adr+sizeof(level100);            // Отсчеты ЦАП соответсвующие 100 мощности
  memcpy((byte*)&levelOff,buf+adr,sizeof(levelOff)); adr=adr+sizeof(levelOff);            // Минимальная мощность при котором частотник отключается (ограничение минимальной мощности)
  memcpy((byte*)&f,buf+adr,sizeof(f)); adr=adr+sizeof(f);                                 // флаги
// if (GETBIT(f,fAnalog)) SETBIT1(flags,fAnalog); else SETBIT0(flags,fAnalog);              // проблема при чтении некоторые флаги не настройки? по этому устанавливаем их отдельно побитно
  return adr;      
}
// Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
uint16_t devOmronMX2::get_crc16(uint16_t crc)         
{
   byte f=0;
   crc=_crc16(crc,lowByte(level0)); crc=_crc16(crc,highByte(level0));              //   Отсчеты ЦАП соответсвующие 0   мощности
   crc=_crc16(crc,lowByte(level100)); crc=_crc16(crc,highByte(level100));          //   Отсчеты ЦАП соответсвующие 100 мощности
   crc=_crc16(crc,lowByte(levelOff)); crc=_crc16(crc,highByte(levelOff));          //   Минимальная мощность при котором частотник отключается (ограничение минимальной мощности)
 //  if(GETBIT(flags,fAnalog)) f=f+(1<<fAnalog);
   crc=_crc16(crc,f);                                                              //   Только флаги насроек
   return crc;
}

// Установить запрет на использование инвертора если лимит ошибок исчерпан
void  devOmronMX2::check_blockFC() 
{
   #ifndef FC_ANALOG_CONTROL                                    // Не аналоговое управление
    if((xTaskGetSchedulerState()==taskSCHEDULER_NOT_STARTED)&&(err!=OK))   // если не запущена free rtos то блокируем с первого раза
       {
        SETBIT1(flags,fErrFC);                                                  // Установить флаг
        note=(char*)noteFC_NO;
        set_Error(err,(char*)name);                                        // Подъем ошибки на верх и останов ТН
        return; 
       }
        
    if (err!=OK) number_err++;else { number_err=0; return;}       // Увеличить счетчик ошибок
    if (number_err>FC_NUM_READ)                                // если привышено число ошибок то блокировка
      {
       SemaphoreGive(xModbusSemaphore); // разблокировать семафор
       SETBIT1(flags,fErrFC);                                         // Установить флаг
       note=(char*)noteFC_NO;
       set_Error(err,(char*)name);                        // Подъем ошибки на верх и останов ТН
      }
    #endif 
} 

// Прочитать (внутренние переменные обновляются) состояние Инвертора, возвращает или ОК или ошибку
// Вызывается из задачи чтения датчиков период FC_TIME_READ
int8_t devOmronMX2::get_readState()
{
uint8_t i;
if ((!get_present())||(GETBIT(flags,fErrFC))) return err;  // выходим если нет инвертора или он заблокирован по ошибке
err=OK;
#ifndef FC_ANALOG_CONTROL                                    // Не аналоговое управление
  // Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
  for(i=0;i<FC_NUM_READ;i++)   // делаем FC_NUM_READ попыток чтения
      { 
        state=read_0x03_16(MX2_STATE);                     // прочитать состояние
        err=Modbus.get_err();                              // Скопировать ошибку
        if (err==OK)                                       // Прочитано верно
         {
          if ((GETBIT(flags,fOnOff))&&(state!=3)) continue; else break;  // ТН включил компрессор а инвертор не имеет правильного состяния пытаемся прочитать еще один раз в проитвном случае все ок выходим
         } 
        _delay(FC_DELAY_REPEAT); 
        journal.jprintf(cErrorRS485,name,__FUNCTION__,err);// Выводим сообщение о повторном чтении
        numErr++;                                          // число ошибок чтение по модбасу
 //       journal.jprintf(pP_TIME,cErrorRS485,name,err);     // Вывод кода ошибки в журнал
      }
  if (err!=OK)                                              // Ошибка модбаса
      {
       state=ERR_LINK_FC;                                  // признак потери связи с инвертором
       SETBIT1(flags,fErrFC);                              // Блок инвертора
       set_Error(err,name);                                 // генерация ошибки
       return err;                                          // Возврат
      }
//  else  if ((testMode==NORMAL)||(testMode==HARD_TEST))     //   Режим работа и хард тест, анализируем состояние,
//        if ((GETBIT(flags,fOnOff))&&(state!=3))                  // Не верное состояние
//         {
//         err=ERR_MODBUS_STATE;                            // Ошибка не верное состояние инвертора
//         journal.jprintf(" %s:Compressor ON and wrong read state: %d \n",name,state); 
//         set_Error(err,name);  
//         return err;                                        // Возврат
//         }
    // Состояние прочитали и оно правильное все остальное читаем
    _delay(FC_DELAY_READ);
    freqFC=read_0x03_32(MX2_CURRENT_FR);               // прочитать текущую частоту
    err=Modbus.get_err();                             // Скопировать ошибку
    if (err!=OK) {state=ERR_LINK_FC; }               // Ошибка выходим
    
    _delay(FC_DELAY_READ);
    power=read_0x03_16(MX2_POWER);                    // прочитать мощность
    err=Modbus.get_err();                             // Скопировать ошибку
    if (err!=OK) {state=ERR_LINK_FC;}                // Ошибка выходим
    
    _delay(FC_DELAY_READ);
    current=read_0x03_16(MX2_AMPERAGE);               // прочитать ток
    err=Modbus.get_err();                             // Скопировать ошибку
    if (err!=OK) {state=ERR_LINK_FC;}                // Ошибка выходим
#else // Аналоговое управление
    freqFC=FC;
    power=0;
    current=0;
#endif
return err;                            
}

// Команда ход на инвертор (целевая частота НЕ ВЫСТАВЛЯЕТСЯ)
// Может быть подана команда через реле и через модбас в зависимости от ключей компиляции
int8_t devOmronMX2::start_FC()
{
if (((testMode==NORMAL)||(testMode==HARD_TEST))&&(((FC<FC_MIN_FREQ)||(FC>FC_MAX_FREQ)))) {journal.jprintf(" %s: Wrong frequency, ignore start\n",name);  return err;} // проверка частоты не в режиме теста
err=OK;
 #ifndef FC_ANALOG_CONTROL                                    // Не аналоговое управление
      #ifdef DEMO
             #ifdef FC_USE_RCOMP   // Использовать отдельный провод для команды ход/стоп
                 HP.dRelay[RCOMP].set_ON();                // ПЛОХО через глобальную переменную
             #endif // FC_USE_RCOMP   
            if (err==OK) {SETBIT1(flags,fOnOff);startCompressor=rtcSAM3X8.unixtime(); journal.jprintf(" %s ON\n",name);}
            else {state=ERR_LINK_FC; SETBIT1(flags,fErrFC); set_Error(err,name);}               // генерация ошибки
      #else // DEMO
             // Боевая часть
            if (((testMode==NORMAL)||(testMode==HARD_TEST))&&(((!get_present())||(GETBIT(flags,fErrFC))))) return err;         // выходим если нет инвертора или он заблокирован по ошибке
          
           // set_targetFreq(FC_START_FREQ,true);  // Запись в регистр инвертора стартовой частоты  НЕ всегда частота стартовая - супербойлер
           
            err=OK;
            if ((testMode==NORMAL)||(testMode==HARD_TEST))  //   Режим работа и хард тест, все включаем,
            {  
            #ifdef FC_USE_RCOMP   // Использовать отдельный провод для команды ход/стоп
                HP.dRelay[RCOMP].set_ON();                // ПЛОХО через глобальную переменную
            #else                 // подать команду ход/стоп через модбас
                err= write_0x05_bit(MX2_START, true);   // Команда Ход
            #endif    
            }
            if (err==OK) {SETBIT1(flags,fOnOff);startCompressor=rtcSAM3X8.unixtime(); journal.jprintf(" %s ON\n",name);}
            else {state=ERR_LINK_FC; SETBIT1(flags,fErrFC); set_Error(err,name);}               // генерация ошибки
      #endif
  #else  //  FC_ANALOG_CONTROL
       #ifdef DEMO
            #ifdef FC_USE_RCOMP   // Использовать отдельный провод для команды ход/стоп
                 HP.dRelay[RCOMP].set_ON();                // ПЛОХО через глобальную переменную
            #endif // FC_USE_RCOMP   
            SETBIT1(flags,fOnOff);startCompressor=rtcSAM3X8.unixtime(); journal.jprintf(" %s ON\n",name);
        #else // DEMO
             // Боевая часть
            if (((testMode==NORMAL)||(testMode==HARD_TEST))&&(((!get_present())||(GETBIT(flags,fErrFC))))) return err;   // выходим если нет инвертора или он заблокирован по ошибке
            err=OK;
            if ((testMode==NORMAL)||(testMode==HARD_TEST))  //   Режим работа и хард тест, все включаем,
            {  
            #ifdef FC_USE_RCOMP   // Использовать отдельный провод для команды ход/стоп
                HP.dRelay[RCOMP].set_ON();                // ПЛОХО через глобальную переменную
            #else               
                state=ERR_LINK_FC; err=ERR_FC_CONF_ANALOG; SETBIT1(flags,fErrFC); set_Error(err,name);// Ошибка конфигурации
            #endif    
            }
            SETBIT1(flags,fOnOff);startCompressor=rtcSAM3X8.unixtime(); journal.jprintf(" %s ON\n",name);
      #endif 
  #endif    
return err;
}

// Команда стоп на инвертор Обратно код ошибки
int8_t devOmronMX2::stop_FC()
{
err=OK;     
 #ifndef FC_ANALOG_CONTROL                                    // Не аналоговое управление
      #ifdef DEMO
            #ifdef FC_USE_RCOMP   // Использовать отдельный провод для команды ход/стоп
               HP.dRelay[RCOMP].set_OFF();                // ПЛОХО через глобальную переменную
            #endif // FC_USE_RCOMP   
          if (err==OK) {SETBIT0(flags,fOnOff);startCompressor=0; journal.jprintf(" %s OFF\n",name);}
          else {state=ERR_LINK_FC; SETBIT1(flags,fErrFC); set_Error(err,name);}               // генерация ошибки
      #else // DEMO
          if  (((testMode==NORMAL)||(testMode==HARD_TEST))&&(((!get_present())||(GETBIT(flags,fErrFC))))) return err;    // выходим если нет инвертора или он заблокирован по ошибке
          err=OK;   
          if ((testMode==NORMAL)||(testMode==HARD_TEST))      // Режим работа и хард тест, все включаем,
          {  
          #ifdef FC_USE_RCOMP   // Использовать отдельный провод для команды ход/стоп
              HP.dRelay[RCOMP].set_OFF();                    // ПЛОХО через глобальную переменную
          #else                  // подать команду ход/стоп через модбас
              err=write_0x05_bit(MX2_START, false);   // Команда стоп
          #endif   
           }
          if (err==OK) {SETBIT0(flags,fOnOff);startCompressor=0; journal.jprintf(" %s OFF\n",name);}
          else {state=ERR_LINK_FC; SETBIT1(flags,fErrFC); set_Error(err,name);}                          // генерация ошибки
      #endif
 #else  // FC_ANALOG_CONTROL 
      #ifdef DEMO
            #ifdef FC_USE_RCOMP   // Использовать отдельный провод для команды ход/стоп
               HP.dRelay[RCOMP].set_OFF();                // ПЛОХО через глобальную переменную
            #endif // FC_USE_RCOMP   
            SETBIT0(flags,fOnOff);startCompressor=0; journal.jprintf(" %s OFF\n",name);
      #else // DEMO
          if  (((testMode==NORMAL)||(testMode==HARD_TEST))&&(((!get_present())||(GETBIT(flags,fErrFC))))) return err;    // выходим если нет инвертора или он заблокирован по ошибке
          if ((testMode==NORMAL)||(testMode==HARD_TEST))      // Режим работа и хард тест, все включаем,
          {  
          #ifdef FC_USE_RCOMP   // Использовать отдельный провод для команды ход/стоп
              HP.dRelay[RCOMP].set_OFF();                    // ПЛОХО через глобальную переменную
          #else                  // подать команду ход/стоп через модбас
              state=ERR_LINK_FC; err=ERR_FC_CONF_ANALOG; SETBIT1(flags,fErrFC); set_Error(err,name);// Ошибка конфигурации
          #endif   
           }
          SETBIT0(flags,fOnOff);startCompressor=0; journal.jprintf(" %s OFF\n",name);
      #endif 
 #endif // FC_ANALOG_CONTROL 
return err;
}

// Получить параметр инвертора в виде строки
char* devOmronMX2::get_paramFC(TYPE_PARAM_FC p)
{
static char temp[10];
  switch (p)
   {
    case   pON_OFF:      if (GETBIT(flags,fOnOff)) return(char*)cOne;else return(char*)cZero; break; // Флаг включения выключения
    case   pMIN_FC:      return ftoa(temp,(float)FC_MIN_FREQ/100.0,2);              break;       // Только чтение минимальная частота работы
    case   pMAX_FC:      return ftoa(temp,(float)FC_MAX_FREQ/100.0,2);              break;       // Только чтение максимальная частота работы
    case   pSTART_FC:    return ftoa(temp,(float)FC_START_FREQ/100.0,2);            break;       // Только чтение стартовая частота работы
    case   pMAX_POWER:   return ftoa(temp,(float)FC_MAX_POWER/10.0,1);            break;       // Только чтение максимальная мощность
    case   pSTATE:       return int2str(state);                                      break;       // Только чтение Состояние ПЧ
    case   pFC:          return ftoa(temp,(float)freqFC/100.0,2);                     break;       // текущая частота ПЧ
    case   pPOWER:       return ftoa(temp,(float)power/10.0,1);                      break;       // Текущая мощность
    case   pAUTO_FC:     if (GETBIT(flags,fAuto)) return(char*)cOne;else return(char*)cZero;  break; // Флаг автоматической подбора частоты
//    case   pANALOG:      if (GETBIT(flags,fAnalog)) return(char*)cOne;else return(char*)cZero;break; // Флаг аналогового управления
    case   pANALOG:      // Флаг аналогового управления
                         #ifdef FC_ANALOG_CONTROL                                                    
                           return(char*)cOne;
                         #else
                           return(char*)cZero;
                         #endif
                         break;
    case   pLEVEL0:      return int2str(level0);                                     break;       // Уровень частоты 0 в отсчетах ЦАП
    case   pLEVEL100:    return int2str(level100);                                   break;       // Уровень частоты 100% в  отсчетах ЦАП
    case   pLEVELOFF:    return int2str(levelOff);                                   break;       // Уровень частоты в % при отключении
    case   pSTOP_FC:    if (GETBIT(flags,fErrFC)) return(char*)"Block";else return(char*)"No";break;  // флаг глобальная ошибка инвертора - работа инвертора запрещена
    case   pERROR_FC:   return int2str(err);                                        break;       // Получить код ошибки
    default:             return  (char*)cInvalid;                                   break;  
   }
 return (char*)cInvalid;
}
// Установить параметр инвертора из строки
boolean devOmronMX2::set_paramFC(TYPE_PARAM_FC p, float x)
{
 switch (p)
   {
    case   pON_OFF:      if (x==0) stop_FC();else start_FC();return true;               break;       // Флаг включения выключения Включение-вуключение инвертора
    case   pMIN_FC:      return true;                                                 break;       // Только чтение минимальная частота работы
    case   pMAX_FC:      return true;                                                 break;       // Только чтение максимальная частота работы
    case   pSTART_FC:    return true;                                                 break;       // Только чтение стартовая частота работы
    case   pMAX_POWER:   return true;                                                 break;       // Только чтение максимальная мощность
    case   pSTATE:       return true;                                                 break;       // Только чтение Состояние ПЧ
    case   pFC:          return true;                                                 break;       // Только чтение текущая частота ПЧ
    case   pPOWER:       return true;                                                 break;       // Только чтение Текущая мощность
    case   pAUTO_FC:     if (x==0) SETBIT0(flags,fAuto);else SETBIT1(flags,fAuto);return true;    break; // Флаг автоматической подбора частоты
  //  case   pANALOG:      if (x==0) SETBIT0(flags,fAnalog);else SETBIT1(flags,fAnalog);return true;break; // Флаг аналогового управления
    case   pANALOG:      return true;break;                                                        // ТОЛЬКО ЧТЕНИЕ Флаг аналогового управления
    case   pLEVEL0:      if ((x>=0)&&(x<=4096)) { level0=x; return true;}                          // Только правильные значения
                         return false;                                                break;       // Уровень частоты 0 в отсчетах ЦАП
    case   pLEVEL100:    if ((x>=0)&&(x<=4096)) { level100=x; return true;}                        // Только правильные значения
                         return false;                                                break;       // Уровень частоты 100% в  отсчетах ЦАП
    case   pLEVELOFF:    if ((x>=0)&&(x<=4096)) { levelOff=x; return true;}                        // Только правильные значения
                         return false;                                                break;       // Уровень частоты в % при отключении
    case   pSTOP_FC:    SemaphoreGive(xModbusSemaphore); // отдать семафор ВСЕГДА
                         if (x==0) { SETBIT0(flags,fErrFC); note=(char*)noteFC_OK; }
                         else      { SETBIT1(flags,fErrFC); note=(char*)noteFC_NO; }
                         return true;                                                 break;       // флаг глобальная ошибка инвертора - работа инвертора запрещена
    case   pERROR_FC:   return true;                                                 break;       // Код ошибки тлько чтение
    default:             return false;                                                break;  
   }
 return false;     
}
 // Получить информацию о частотнике
char * devOmronMX2::get_infoFC(char *buf)
{
#ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
  if(!HP.dFC.get_present()) { strcat(buf,"|Данные не доступны (нет инвертора)|;&"); return buf;}          // Инвертора нет в конфигурации
  if(HP.dFC.get_blockFC())  { strcat(buf,"|Данные не доступны (нет связи по Modbus, инвертор заблокирован)|;&"); return buf;}  // Инвертор заблокирован
  int8_t i;  
  char temp[10];  
       strcat(buf,"-|Состояние инвертора [0:Начальное состояние, 2:Остановка 3:Вращение 4:Остановка с выбегом 5:Толчковый ход 6:Торможение  постоянным током ");strcat(buf,"7:Выполнение  повторной попытки 8:Аварийное  отключение 9:Пониженное напряжение -1:Блокировка]|");strcat(buf,int2str(read_0x03_16(MX2_STATE)));strcat(buf,";");
  //     strcat(buf,"-|Направление вращения [1:Обратное вращение, 0:Вращение в прямом направлении]|");strcat(buf,int2str(HP.dFC.read_0x01_bit(MX2_DIRECTION)));strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d001|Контроль выходной частоты (Гц)|");strcat(buf,ftoa(temp,(float)read_0x03_32(MX2_CURRENT_FR)/100.0,2));strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d003|Контроль выходного тока (А)|");strcat(buf,ftoa(temp,(float)read_0x03_16(MX2_AMPERAGE)/100.0,2));strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d014|Контроль мощности (кВт)|");strcat(buf,ftoa(temp,(float)read_0x03_16(MX2_POWER)/10.0,1));strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d013|Контроль выходного напряжения (В)|");strcat(buf,ftoa(temp,(float)read_0x03_16(MX2_VOLTAGE)/10.0,1));strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d015|Контроль ватт-часов (кВт/ч)|");strcat(buf,ftoa(temp,(float)read_0x03_32(MX2_POWER_HOUR)/10.0,1));strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d016|Контроль времени наработки в режиме \"Ход\" (ч)|");strcat(buf,int2str(read_0x03_32(MX2_HOUR)));strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d017|Контроль времени наработки при включенном питании (ч)|");strcat(buf,int2str(read_0x03_32(MX2_HOUR1)));strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d018|Контроль температуры радиатора (°С)|");strcat(buf,ftoa(temp,(float)read_0x03_16(MX2_TEMP)/10.0,2));strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d102|Контроль напряжения  постоянного тока (В)|");strcat(buf,ftoa(temp,(float)read_0x03_16(MX2_VOLTAGE_DC)/10.0,1));strcat(buf,";");
       _delay(FC_DELAY_READ);
       strcat(buf,"d080|Счетчик аварийных отключений (Шт)|");strcat(buf,int2str(read_0x03_16(MX2_NUM_ERR)));strcat(buf,";");
       for(i=0;i<6;i++)  // Скан по ошибкам
          {
          strcat(buf,"d0");strcat(buf,int2str(81+i));strcat(buf,"|Состояние в момент ошибки ");
          read_0x03_error(MX2_ERROR1+i*0x0a);
          // Формирование ответа в строке
   //       strcat(buf,"[S:");  strcat(buf,int2str(HP.dFC.get_errorFC().MX2.status));
          strcat(buf,"[F:");  strcat(buf,ftoa(temp,(float)error.MX2.fr/100.0,2));
          strcat(buf," I:");  strcat(buf,ftoa(temp,(float)error.MX2.cur/100.0,2));
          strcat(buf," V:");  strcat(buf,ftoa(temp,(float)error.MX2.vol/10.0,2));
          strcat(buf," T1:"); strcat(buf,int2str(error.MX2.time1));
          strcat(buf," T2:"); strcat(buf,int2str(error.MX2.time2));
          strcat(buf,"] Код ошибки:|");
          if(error.MX2.code<10) strcat(buf,"E0"); else strcat(buf,"E");
          strcat(buf,int2str(error.MX2.code));strcat(buf,"."); strcat(buf,int2str(error.MX2.status));
          strcat(buf,";");   
          }  
#endif          
return buf;                              
}
// Сброс ошибок инвертора по модбасу
boolean devOmronMX2::reset_errorFC()                   
{
#ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
  write_0x06_16(MX2_INIT_DEF, 0x01);       // задать режим иннициализации - стирание ошибок
  _delay(FC_DELAY_READ);
  if((read_0x03_16(MX2_INIT_DEF)==0x01)&&(err=OK))   // подать команду на инициализацию, если записано стирание только ошибок
    {
    write_0x06_16(MX2_INIT_RUN, 0x01);       
    journal.jprintf("Reset error %s\r\n",name);
    }
  else journal.jprintf("$WARNING: bad read from MX2_INIT_DEF, no reset error\r\n");
#endif
if (err==OK) return true;  else return false;   
}

// Сброс инвертора через модбас
boolean devOmronMX2::reset_FC() 
{
#ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
  write_0x05_bit(MX2_RESET, true);               // подать команду на сброс по модбас
  journal.jprintf("Reset %s use Modbus\r\n",name);  
#endif
if (err==OK) return true;  else return false;                          
}

// Текущее состояние инвертора
int16_t devOmronMX2::read_stateFC()
{
#ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
  return read_0x03_16(MX2_STATE);
#else
  return 0;
#endif  
} 

// Tемпература радиатора
int16_t devOmronMX2::read_tempFC()
{
#ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
  return read_0x03_16(MX2_TEMP);
#else
  return 0;
#endif  
} 
// Функции общения по модбас инвертора  Чтение регистров
#ifndef FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
    // Чтение отдельного бита в регистр cmd возвращает код ,  ошибка обновляется
    // Реалезовано FC_NUM_READ попыток чтения/записи в инвертор
    boolean devOmronMX2::read_0x01_bit(uint16_t cmd)
    { uint8_t i;
      boolean result;
      err=OK;  
      if ((!get_present())||(GETBIT(flags,fErrFC))) return false;             // выходим если нет инвертора или он заблокирован по ошибке
      for(i=0;i<FC_NUM_READ;i++)   // делаем FC_NUM_READ попыток чтения Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
         { 
         err=Modbus.readCoil(FC_MODBUS_ADR,cmd-1, &result);              // послать запрос, Нумерация регистров MX2 с НУЛЯ!!!!
         if (err==OK) break;                                                // Прочитали удачно
         _delay(FC_DELAY_REPEAT);
         journal.jprintf(cErrorRS485,name,__FUNCTION__,err);                // Выводим сообщение о повторном чтении
         numErr++;                                                          // число ошибок чтение по модбасу
 //        journal.jprintf(pP_TIME,cErrorRS485,name,err);                     // Вывод кода ошибки в журнал
         }
    
      check_blockFC();                                                     // проверить необходимость блокировки
      return result;  
    }
    // Функция 0х03 прочитать 2 байта, возвращает значение, ошибка обновляется
    // Реализовано FC_NUM_READ попыток чтения/записи в инвертор
    int16_t  devOmronMX2::read_0x03_16(uint16_t cmd)
    {   uint8_t i;
        uint16_t result;  
        err=OK;
        if ((!get_present())||(GETBIT(flags,fErrFC))) return 0;                  // выходим если нет инвертора или он заблокирован по ошибке
    
        for(i=0;i<FC_NUM_READ;i++)   // делаем FC_NUM_READ попыток чтения Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
            { 
            err=Modbus.readHoldingRegisters16(FC_MODBUS_ADR,cmd-1,&result);  // Послать запрос, Нумерация регистров MX2 с НУЛЯ!!!!
            if (err==OK) break;                                                 // Прочитали удачно
            _delay(FC_DELAY_REPEAT);
             journal.jprintf(cErrorRS485,name,__FUNCTION__,err);                // Выводим сообщение о повторном чтении
             numErr++;                                                          // число ошибок чтение по модбасу
   //         journal.jprintf(pP_TIME,cErrorRS485,name,err);                     // Вывод кода ошибки в журнал
            }
            check_blockFC();                                                   // проверить необходимость блокировки
            return result;
             
    }
    
    // Функция 0х03 прочитать 4 байта
    // Реализовано FC_NUM_READ попыток чтения/записи в инвертор
    uint32_t devOmronMX2::read_0x03_32(uint16_t cmd)
    {
        uint8_t i;
        uint32_t result;  
        err=OK;
        if ((!get_present())||(GETBIT(flags,fErrFC))) return 0;            // выходим если нет инвертора или он заблокирован по ошибке
        for(i=0;i<FC_NUM_READ;i++)   // делаем FC_NUM_READ попыток чтения Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
          { 
           err=Modbus.readHoldingRegisters32(FC_MODBUS_ADR,cmd-1,&result);  // послать запрос, Нумерация регистров MX2 с НУЛЯ!!!!
           if (err==OK) break;                                                 // Прочитали удачно
          _delay(FC_DELAY_REPEAT);
          journal.jprintf(cErrorRS485,name,__FUNCTION__,err);                 // Выводим сообщение о повторном чтении
          numErr++;                                                           // число ошибок чтение по модбасу
    //       journal.jprintf(pP_TIME,cErrorRS485,name,err);                   // Вывод кода ошибки в журнал
          }
        check_blockFC();                                                      // проверить необходимость блокировки
        return result;
    }
    
    // Функция Modbus 0х03 описание ошибки num НУМЕРАЦИЯ с 0 (общая длина данных 10 слов по 2 байта)
    // Возвращает код ошибки и в буфер кладет описание
    // Реализовано FC_NUM_READ попыток чтения/записи в инвертор
    int16_t devOmronMX2::read_0x03_error(uint16_t cmd)  
    { uint8_t i;
      uint16_t tmp;
      err=OK;
      if ((!get_present())||(GETBIT(flags,fErrFC))) return err;              // выходим если нет инвертора или он заблокирован по ошибке
      for(i=0;i<0x0a;i++) error.inputBuf[i]=0;
      for(i=0;i<FC_NUM_READ;i++)   // делаем FC_NUM_READ попыток чтения Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
         { 
         err = Modbus.readHoldingRegistersNN(FC_MODBUS_ADR,cmd-1,0x0a,error.inputBuf);  // послать запрос, Нумерация регистров с НУЛЯ!!!!
         if (err==OK) break;                                                 // Прочитали удачно
         _delay(FC_DELAY_REPEAT);
         journal.jprintf(cErrorRS485,name,__FUNCTION__,err);                 // Выводим сообщение о повторном чтении
          numErr++;                                                          // число ошибок чтение по модбасу
  //        journal.jprintf(pP_TIME,cErrorRS485,name,err);                     // Вывод кода ошибки в журнал
         }
      if (err==OK) // Для времен переставить местами слова (2 байта) т.е сначала идет старшие 2 байта потом младшие
      {
        tmp=error.inputBuf[6]; error.inputBuf[6]=error.inputBuf[7]; error.inputBuf[7]=tmp; // Общее время наработки в режиме «Ход» к моменту отключения
        tmp=error.inputBuf[8]; error.inputBuf[8]=error.inputBuf[9]; error.inputBuf[9]=tmp; // Общее время работы ПЧ при включенном питании в момент отключения выхода
      }
      check_blockFC();                                                   // проверить необходимость блокировки
      return err;
    }
    
    // Запись отдельного бита в регистр cmd возвращает код ошибки
    // Реализовано FC_NUM_READ попыток чтения/записи в инвертор
    int8_t devOmronMX2::write_0x05_bit(uint16_t cmd, boolean f)
    { uint8_t i;
      err=OK;
      if ((!get_present())||(GETBIT(flags,fErrFC))) return err;     // выходим если нет инвертора или он заблокирован по ошибке
      for(i=0;i<FC_NUM_READ;i++)   // делаем FC_NUM_READ попыток записи
         {   
            if (f) err=Modbus.writeSingleCoil(FC_MODBUS_ADR,cmd-1,1);   // послать запрос, Нумерация регистров с НУЛЯ!!!!
            else   err=Modbus.writeSingleCoil(FC_MODBUS_ADR,cmd-1,0);
            if (err==OK) break;                                            // Записали удачно
            _delay(FC_DELAY_REPEAT);
           journal.jprintf(cErrorRS485,name,__FUNCTION__,err);             // Выводим сообщение о повторном чтении
           numErr++;                                                       // число ошибок чтение по модбасу
  //         journal.jprintf(pP_TIME,cErrorRS485,name,err);                  // Вывод кода ошибки в журнал
         }
      check_blockFC();                                                    // проверить необходимость блокировки
      return err;
    }
    // Запись данных (2 байта) в регистр cmd возвращает код ошибки
    // Реализовано FC_NUM_READ попыток чтения/записи в инвертор
    int8_t devOmronMX2::write_0x06_16(uint16_t cmd, uint16_t data)
    { uint8_t i;
      err=OK;
      if ((!get_present())||(GETBIT(flags,fErrFC))) return err;              // выходим если нет инвертора или он заблокирован по ошибке
       for(i=0;i<FC_NUM_READ;i++)                                          // делаем FC_NUM_READ попыток записи
         {
           err=Modbus.writeHoldingRegisters16(FC_MODBUS_ADR,cmd-1,data);  // послать запрос, Нумерация регистров с НУЛЯ!!!!
           if (err==OK) break;                                               // Записали удачно
           _delay(FC_DELAY_REPEAT);
           journal.jprintf(cErrorRS485,name,__FUNCTION__,err);                // Выводим сообщение о повторном чтении
           numErr++;                                                          // число ошибок чтение по модбасу
   //        journal.jprintf(pP_TIME,cErrorRS485,name,err);                     // Вывод кода ошибки в журнал
         }
      check_blockFC();                                                      // проверить необходимость блокировки
      return err;
      
    }
    // Запись данных (4 байта) в регистр cmd возвращает код ошибки
    int8_t devOmronMX2::write_0x10_32(uint16_t cmd, uint32_t data)
    { uint8_t i;
      err=OK;
      if ((!get_present())||(GETBIT(flags,fErrFC))) return err;             // выходим если нет инвертора или он заблокирован по ошибке
      for(i=0;i<FC_NUM_READ;i++)                                          // делаем FC_NUM_READ попыток записи
         {  
           err=Modbus.writeHoldingRegisters32(FC_MODBUS_ADR, cmd-1, data);// послать запрос, Нумерация регистров с НУЛЯ!!!!
           if (err==OK) break;                                               // Записали удачно
           _delay(FC_DELAY_REPEAT);
           journal.jprintf(cErrorRS485,name,__FUNCTION__,err);                // Выводим сообщение о повторном чтении
           numErr++;                                                          // число ошибок чтение по модбасу
  //         journal.jprintf(pP_TIME,cErrorRS485,name,err);                     // Вывод кода ошибки в журнал
         }
      check_blockFC();                                                       // проверить необходимость блокировки
      return err; 
    }
#endif  // FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ

#endif // НЕ FC_VACON

// ------------------------ Счетчик SDM ---------------------------------------
// Инициализация счетчика и проверка и если надо программирование
int8_t devSDM::initSDM()
{
  err=OK;                                        // Ошибок нет
  numErr=0;                                      // счетчик 0
  Voltage=0.0;                                   // Напряжение
  Current=0.0;                                   // Ток
  AcPower=0.0;                                   // активная мощность
  RePower=0.0;                                   // Реактивная мощность
  Power=0.0;                                     // Полная мощность
  PowerFactor=0.0;                               // Коэффициент мощности
  Phase=0.0;                                     // угол фазы (градусы)
  iAcEnergy=0.0;                                 // Потребленная активная энергия
  eAcEnergy=0.0;                                 // Переданная активная энергия
  iReEnergy=0.0;                                 // Потребленная реактивная энергия
  eReEnergy=0.0;                                 // Переданная реактивная энергия
  AcEnergy=0.0;                                  // Суммараная активная энергия
  ReEnergy=0.0;                                  // Суммараная реактивная энергия
  Energy=0.0;                                    // Суммараная энергия
  flags=0x00;
  // Настройки
  settingSDM.maxVoltage=SDM_MAX_VOLTAGE;         // максимальное напряжение (вольты) иначе ошибка если 0 то не работает
  settingSDM.minVoltage=SDM_MIN_VOLTAGE;         // минимальное напряжение (вольты) иначе ПРЕДУПРЕЖДЕНИЕ если 0 то не работает
  settingSDM.maxPower=SDM_MAX_POWER;             // максимальная мощность (ватты) напряжение иначе ошибка если 0 то не работает
  name=(char*)nameSDM;
  note=(char*)noteSDM_NONE;  
      
  #ifdef USE_ELECTROMETER_SDM
      if(!Modbus.get_present())                  // modbus отсутствует
      {
      journal.jprintf("%s: modbus not found, block.\n",name); 
      SETBIT0(flags,fSDM);                           // счетчик не представлен
      SETBIT0(flags,fLink);
      err=ERR_NO_MODBUS;
      }
      else
      {
      SETBIT1(flags,fSDM);                           // счетчик представлен
      note=(char*)noteSDM;
      uplinkSDM();                                  // проверить связь со счетчиком
      }
  #else
      SETBIT0(flags,fSDM);                           // счетчик не представлен
      SETBIT0(flags,fLink);
      note=(char*)noteSDM_NONE;
  #endif
   // инициализация статистики
  ChartVoltage.init(GETBIT(flags,fSDM));               // Статистика по напряжению
  ChartCurrent.init(GETBIT(flags,fSDM));               // Статистика по току
//  sAcPower.init(GETBIT(flags,fSDM));               // Статистика по активная мощность
//  sRePower.init(GETBIT(flags,fSDM));               // Статистика по Реактивная мощность
  ChartPower.init(GETBIT(flags,fSDM));                 // Статистика по Полная мощность
 // ChartPowerFactor.init(GETBIT(flags,fSDM));           // Статистика по Коэффициент мощности
 return err;
}


// Проверить связь со счетчиком предполагается что модбас уже нинициализирован читаем из регистра скорость
// Выводит сообщеиня в журнал и устанавливает флаг связи
boolean  devSDM::uplinkSDM() 
{
  float band;
  int8_t i;
  for(i=0;i<SDM_NUM_READ;i++)   // делаем SDM_NUM_READ попыток чтения
    {
     if ((Modbus.readHoldingRegistersFloat(SDM_MODBUS_ADR,SDM_BAUD_RATE,&band)==OK)&&(band==SDM_SPEED)) {SETBIT1(flags,fLink); journal.jprintf("%s, found, link OK, band rate:%.0f modbus address:%d\n",name,band,SDM_MODBUS_ADR);  return true;}
     else { SETBIT0(flags,fLink); journal.jprintf("%s, no connect.\n",name);}
     _delay(SDM_DELAY_REPEAD);
      WDT_Restart(WDT);                                                            // Сбросить вачдог
    }
 SETBIT0(flags,fLink);                                                             // связи нет
 return false;   
}

// перепрограммировать счетчик на требуемые параметры связи SDM_SPEED SDM_MODBUS_ADR c DEFAULT_SDM_SPEED DEFAULT_SDM_MODBUS_ADR
// При программировании параметры сразу начинают рабоать
boolean  devSDM::progConnect()
{
  float band; 
  journal.jprintf("%s: Setting band rate and modbus address.\n",name); 
  // 1. Проверка
  if ((Modbus.readHoldingRegistersFloat(SDM_MODBUS_ADR,SDM_BAUD_RATE,&band)==OK)&&(band==SDM_SPEED)) {SETBIT1(flags,fLink); journal.jprintf(" Setting %s are correct, programming is not required\n",name);  return true;} 
  
  // 2. Установка скорости по умолчанию
  
  MODBUS_PORT_NUM.begin(DEFAULT_SDM_SPEED,MODBUS_PORT_CONFIG);            // SERIAL_8N1 - настройки порта для счетчика из коробки
  err=OK;
  
  // 3. На DEFAULT_SDM_SPEED скорости установить правильный адрес SDM_MODBUS_ADR
  if( (err=Modbus.writeHoldingRegistersFloat(DEFAULT_SDM_MODBUS_ADR,SDM_ADR_MODBUS,SDM_MODBUS_ADR))==OK)  journal.jprintf("%s: Set address  %d.\n",name,SDM_MODBUS_ADR);  // установить требуемемый адрес
  else  journal.jprintf("%s: Setting address error, code %d.\n",name,err); 
  
  // 4. На правильный адрес SDM_MODBUS_ADR установит желаемую скорость SDM_SPEED
  if( (err=Modbus.writeHoldingRegistersFloat(SDM_MODBUS_ADR, SDM_BAUD_RATE,SDM_SPEED))==OK)  journal.jprintf("%s: Set band rate (0=2400bps 1=4800bps 2=9600bps 5=1200bps) %d.\n",name,SDM_SPEED);  // установить требуемую скорость
  else  journal.jprintf("%s: Setting band rate error, code %d.\n",name,err);
  
  // 5. Порт обратно в требуемые настройки
  MODBUS_PORT_NUM.begin(MODBUS_PORT_SPEED,MODBUS_PORT_CONFIG);                 // SERIAL_8N1 - настройки по умолчанию

  // 6. Вывод результатов
  if (err==OK) { journal.jprintf("%s: Programming is Ok\n",name); uplinkSDM(); return true;}  // Надо сбросить счетчик
  else { journal.jprintf("%s: Programming is wrong, no link\n",name); return false; }
}                           

// Прочитать инфо с счетчика
 int8_t devSDM::get_readState()              
 {
 static float tmp; 
 int8_t i;
 if((!GETBIT(flags,fSDM))||(!GETBIT(flags,fLink))) return err;  // Если нет счетчика или нет связи выходим
 err=OK;
      // Чтение состояния счетчика,
      for(i=0;i<SDM_NUM_READ;i++)   // делаем SDM_NUM_READ попыток чтения
      {
        // Читаем значения счетчика
        _delay(SDM_DELAY_READ); 
        err=Modbus.readInputRegistersFloat(SDM_MODBUS_ADR,SDM_VOLTAGE,&tmp);                    if(err==OK) { Voltage=tmp;_delay(SDM_DELAY_READ);     }            // Напряжение
        if(err==OK) {err=Modbus.readInputRegistersFloat(SDM_MODBUS_ADR,SDM_CURRENT,&tmp);       if(err==OK) Current=tmp;_delay(SDM_DELAY_READ);       }            // Ток
        if(err==OK) {err=Modbus.readInputRegistersFloat(SDM_MODBUS_ADR,SDM_AC_POWER,&tmp);      if(err==OK) AcPower=tmp;_delay(SDM_DELAY_READ);       }            // Активная мощность
        if(err==OK) {err=Modbus.readInputRegistersFloat(SDM_MODBUS_ADR,SDM_RE_POWER,&tmp);      if(err==OK) RePower=tmp;_delay(SDM_DELAY_READ);       }            // Реактивная мощность
        if(err==OK) {err=Modbus.readInputRegistersFloat(SDM_MODBUS_ADR,SDM_POWER,&tmp);         if(err==OK) Power=tmp;_delay(SDM_DELAY_READ);         }            // Полная мощность
        if(err==OK) {err=Modbus.readInputRegistersFloat(SDM_MODBUS_ADR,SDM_POW_FACTOR,&tmp);    if(err==OK) PowerFactor=tmp;_delay(SDM_DELAY_READ);   }            // Коэффициент мощности
        if(err==OK) {err=Modbus.readInputRegistersFloat(SDM_MODBUS_ADR,SDM_PHASE,&tmp);         if(err==OK) Phase=tmp;_delay(SDM_DELAY_READ);         }            // Угол фазы (градусы)
        if(err==OK) {err=Modbus.readInputRegistersFloat(SDM_MODBUS_ADR,SDM_AC_ENERGY,&tmp);     if(err==OK) AcEnergy=tmp;_delay(SDM_DELAY_READ);      }            // Суммараная активная энергия
        if(err==OK) {err=Modbus.readInputRegistersFloat(SDM_MODBUS_ADR,SDM_RE_ENERGY,&tmp);     if(err==OK) ReEnergy=tmp;_delay(SDM_DELAY_READ);      }            // Суммараная реактивная энергия
      //  if(err==OK) {err=Modbus.readInputRegistersFloat(SDM_MODBUS_ADR,SDM_ENERGY,&tmp);        if(err==OK) Energy=tmp;_delay(SDM_DELAY_READ);      }            // Суммараная энергия
        Energy=AcEnergy+ReEnergy;
        if (err==OK) break;
        numErr++;                  // число ошибок чтение по модбасу
        journal.jprintf(pP_TIME, cErrorRS485,name,__FUNCTION__,err);      // Выводим сообщение о повторном чтении
        WDT_Restart(WDT);          // Сбросить вачдог
        _delay(SDM_DELAY_REPEAD);  // Чтение не удачно, делаем паузу
      }
 if (err==OK)
 {
 // Serial.println((int)(Voltage*100));
  if ((settingSDM.maxVoltage>1)&&(settingSDM.maxVoltage< Voltage)) {err=ERR_MAX_VOLTAGE;set_Error(err,name);return err; }       // Контроль входного напряжения
  if ((settingSDM.maxPower>1)&&(settingSDM.maxPower< Power))       {err=ERR_MAX_POWER;set_Error(err,name);return err; }         // Контроль мощности потребления
  if ((settingSDM.minVoltage>1)&&(settingSDM.minVoltage>Voltage) ) {HP.message.setMessage(pMESSAGE_WARNING,(char*)"Напряжение сети ниже нормы",(int)Voltage);return err; } // сформировать уведомление о низком напряжени
  return err;                       // все прочиталось, выходим
 } 
 SETBIT0(flags,fLink);             // связь со счетчиком потеряна
// set_Error(err,name);              // генерация ошибки    НЕТ счетчик не критичен
 return err;    
 }

// Получить параметр счетчика в виде строки
char* devSDM::get_paramSDM(char *var, char *ret)           
 {
    char temp[12];
   if(strcmp(var,sdm_NAME)==0){         return strcat(ret,(char*)name);                                                   }else      // Имя счетчика
   if(strcmp(var,sdm_NOTE)==0){         return strcat(ret,(char*)note);                                                   }else      // Описание счетчика
   if(strcmp(var,sdm_MAX_VOLTAGE)==0){  return strcat(ret,int2str(settingSDM.maxVoltage));                                 }else      // мах напряжение контроля напряжения
   if(strcmp(var,sdm_MIN_VOLTAGE)==0){  return strcat(ret,int2str(settingSDM.minVoltage));                                 }else      // min напряжение контроля напряжения
   if(strcmp(var,sdm_MAX_POWER)==0){    return strcat(ret,int2str(settingSDM.maxPower));                                   }else      // максимальаня мощность контроля мощности
   if(strcmp(var,sdm_VOLTAGE)==0){      return strcat(ret,ftoa(temp,(float)Voltage,2));                                    }else      // Напряжение
   if(strcmp(var,sdm_CURRENT)==0){      return strcat(ret,ftoa(temp,(float)Current,2));                                    }else      // Ток
   if(strcmp(var,sdm_REPOWER)==0){      return strcat(ret,ftoa(temp,(float)RePower,2));                                    }else      // Реактивная мощность
   if(strcmp(var,sdm_ACPOWER)==0){      return strcat(ret,ftoa(temp,(float)AcPower,2));                                    }else      // Активная мощность
   if(strcmp(var,sdm_POWER)==0){        return strcat(ret,ftoa(temp,(float)Power,2));                                      }else      // Полная мощность
   if(strcmp(var,sdm_POW_FACTOR)==0){   return strcat(ret,ftoa(temp,(float)PowerFactor,2));                                }else      // Коэффициент мощности
   if(strcmp(var,sdm_PHASE)==0){        return strcat(ret,ftoa(temp,(float)Phase,2));                                      }else      // Угол фазы (градусы)
   if(strcmp(var,sdm_IACENERGY)==0){    return strcat(ret,ftoa(temp,(float)iAcEnergy,2));                                  }else      // Потребленная активная энергия
   if(strcmp(var,sdm_EACENERGY)==0){    return strcat(ret,ftoa(temp,(float)eAcEnergy,2));                                  }else      // Переданная активная энергия
   if(strcmp(var,sdm_IREENERGY)==0){    return strcat(ret,ftoa(temp,(float)iReEnergy,2));                                  }else      // Потребленная реактивная энергия
   if(strcmp(var,sdm_EREENERGY)==0){    return strcat(ret,ftoa(temp,(float)eReEnergy,2));                                  }else      // Переданная реактивная энергия
   if(strcmp(var,sdm_ACENERGY)==0){     return strcat(ret,ftoa(temp,(float)AcEnergy,2));                                   }else      // Суммараная активная энергия
   if(strcmp(var,sdm_REENERGY)==0){     return strcat(ret,ftoa(temp,(float)ReEnergy,2));                                   }else      // Суммараная реактивная энергия
   if(strcmp(var,sdm_ENERGY)==0){       return strcat(ret,ftoa(temp,(float)Energy,2));                                     }else      // Суммараная  энергия
   if(strcmp(var,sdm_LINK)==0){         if (GETBIT(flags,fLink)) return strcat(ret,(char*)"Ok");else return strcat(ret,(char*)"none");}else      // Cостояние связи со счетчиком
   return strcat(ret,(char*)cInvalid);
 }

// Установить параметр счетчика в виде строки
boolean devSDM::set_paramSDM(char *var,char *c)        
 {
  int16_t x=atoi(c);
   if(strcmp(var,sdm_NAME)==0){          return  true;                                    }else      // Имя счетчика
   if(strcmp(var,sdm_NOTE)==0){          return  true;                                    }else      // Описание счетчика
   if(strcmp(var,sdm_MAX_VOLTAGE)==0){   if ((x>=0)&&(x<=400)) {settingSDM.maxVoltage=(uint16_t)x;return true;} else  return false; }else      // мах напряжение контроля напряжения
   if(strcmp(var,sdm_MIN_VOLTAGE)==0){   if ((x>=0)&&(x<=400)) {settingSDM.minVoltage=(uint16_t)x;return true;} else  return false; }else      // min напряжение контроля напряжения
   if(strcmp(var,sdm_MAX_POWER)==0){     if ((x>=0)&&(x<=15000)){settingSDM.maxPower=(uint16_t)x;  return true;} else  return false;}else      // максимальаня мощность контроля мощности
   if(strcmp(var,sdm_VOLTAGE)==0){       return true;                                     }else      // Напряжение
   if(strcmp(var,sdm_CURRENT)==0){       return true;                                     }else      // Ток
   if(strcmp(var,sdm_REPOWER)==0){       return true;                                     }else      // Реактивная мощность
   if(strcmp(var,sdm_ACPOWER)==0){       return true;                                     }else      // Активная мощность
   if(strcmp(var,sdm_POWER)==0){         return true;                                     }else      // Полная мощность
   if(strcmp(var,sdm_POW_FACTOR)==0){    return true;                                     }else      // Коэффициент мощности
   if(strcmp(var,sdm_PHASE)==0){         return true;                                     }else      // Угол фазы (градусы)
   if(strcmp(var,sdm_IACENERGY)==0){     return true;                                     }else      // Потребленная активная энергия
   if(strcmp(var,sdm_EACENERGY)==0){     return true;                                     }else      // Переданная активная энергия
   if(strcmp(var,sdm_IREENERGY)==0){     return true;                                     }else      // Потребленная реактивная энергия
   if(strcmp(var,sdm_EREENERGY)==0){     return true;                                     }else      // Переданная реактивная энергия
   if(strcmp(var,sdm_ACENERGY)==0){      return true;                                     }else      // Суммараная активная энергия
   if(strcmp(var,sdm_REENERGY)==0){      return true;                                     }else      // Суммараная реактивная энергия
   if(strcmp(var,sdm_ENERGY)==0){        return true;                                     }else      // Суммараная энергия
   if(strcmp(var,sdm_LINK)==0){          return true;                                     }else      // Cостояние связи со счетчиком
   return false;
 }

// Записать настройки в eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t devSDM::save(int32_t adr)
{
if (writeEEPROM_I2C(adr, (byte*)&settingSDM, sizeof(settingSDM)))       { set_Error(ERR_SAVE_EEPROM,name); return ERR_SAVE_EEPROM; } adr=adr+sizeof(settingSDM);      // Вся структура настроек
return adr;                                 
}

// Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t devSDM::load(int32_t adr)
{
if (readEEPROM_I2C(adr, (byte*)&settingSDM, sizeof(settingSDM)))       { set_Error(ERR_LOAD_EEPROM,name); return ERR_LOAD_EEPROM; } adr=adr+sizeof(settingSDM);      // вся струткра настроек
return adr;                              
}
// Считать настройки из буфера на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t devSDM::loadFromBuf(int32_t adr,byte *buf)
{
  memcpy((byte*)&settingSDM,buf+adr,sizeof(settingSDM)); adr=adr+sizeof(settingSDM);     
  return adr;  
}
// Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
uint16_t devSDM::get_crc16(uint16_t crc)
{
  uint8_t i;
  for(i=0;i<sizeof(settingSDM);i++) crc=_crc16(crc,*((byte*)&settingSDM+i));  // CRC16 структуры  settingSDM
  return crc;                      
}

// МОДБАС Устройство ----------------------------------------------------------
// функции обратного вызова
static uint8_t Modbus_Entered_Critical = 0;
static inline void idle() // задержка между чтениями отдельных байт по Modbus
    {
//      _delay(1);  // задержка чтения отдельного символа из Modbus
		delay(1); //
    }
static inline void preTransmission() // Функция вызываемая ПЕРЕД началом передачи
    {
      #ifdef PIN_MODBUS_RSE
      digitalWriteDirect(PIN_MODBUS_RSE, HIGH);
      #endif
      Modbus_Entered_Critical = TaskSuspendAll(); // Запрет других задач во время передачи по Modbus
    }
static inline void postTransmission() // Функция вызываемая ПОСЛЕ окончания передачи
    {
	if(Modbus_Entered_Critical) {
		xTaskResumeAll();
		Modbus_Entered_Critical = 0;
	}
    _delay(MODBUS_TIME_TRANSMISION);// Минимальная пауза между командой и ответом 3.5 символа
    #ifdef PIN_MODBUS_RSE
    digitalWriteDirect(PIN_MODBUS_RSE, LOW);
    #endif
}

// Инициализация Modbus без проверки связи связи
int8_t devModbus::initModbus()    
     {
      #ifdef PIN_MODBUS_RSE
        flags=0x00;
        SETBIT1(flags,fModbus);                                                      // модбас присутствует
        pinMode(PIN_MODBUS_RSE , OUTPUT);                                            // Подготовка управлением полудуплексом
        digitalWriteDirect(PIN_MODBUS_RSE , LOW);
        MODBUS_PORT_NUM.begin(MODBUS_PORT_SPEED,MODBUS_PORT_CONFIG);                 // SERIAL_8N1 - настройки по умолчанию
        RS485.begin(1,MODBUS_PORT_NUM);                                              // Привязать к сериал
        // Назначение функций обратного вызова
        RS485.preTransmission(preTransmission);
        RS485.postTransmission(postTransmission);
        RS485.idle(idle);
        err=OK;                                                                      // Связь есть
       #else
        flags=0x00;
        SETBIT0(flags,fModbus);                                                     // модбас отсутвует
        err=ERR_NO_MODBUS;
       #endif 
        return err;                                                                 
     }
     
// ФУНКЦИИ ЧТЕНИЯ ----------------------------------------------------------------------------------------------
// Получить значение 2-x (Modbus function 0x04 Read Input Registers) регистров (4 байта) в виде float возвращает код ошибки данные кладутся в ret
int8_t devModbus::readInputRegistersFloat(uint8_t id, uint16_t cmd, float *ret)
{
	uint8_t result;
	// Если шедулер запущен то захватываем семафор
	if(SemaphoreTake(xModbusSemaphore, (MODBUS_TIME_WAIT / portTICK_PERIOD_MS)) == pdFALSE) // Захват мютекса потока или ОЖИДАНИНЕ MODBUS_TIME_WAIT
	{
		journal.jprintf((char*) cErrorMutex, __FUNCTION__, MutexModbusBuzy);
		err = ERR_485_BUZY;
		return err;
	}
	RS485.set_slave(id);
	result = RS485.readInputRegisters(cmd, 2);                                               // послать запрос,
	if(result == RS485.ku8MBSuccess) {
		err = OK;
		*ret = fromInt16ToFloat(RS485.getResponseBuffer(0), RS485.getResponseBuffer(1));
	} else {
		err = translateErr(result);
		journal.jprintf("Modbus reg #%d - ", cmd);
		*ret = 0;
	}
	SemaphoreGive (xModbusSemaphore);
	return err;
}

// Получить значение регистра (2 байта) МХ2 в виде целого  числа возвращает код ошибки данные кладутся в ret
int8_t   devModbus::readHoldingRegisters16(uint8_t id, uint16_t cmd, uint16_t *ret)
    {
    uint8_t result;
     // Если шедулер запущен то захватываем семафор
    if(SemaphoreTake(xModbusSemaphore,(MODBUS_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE)            // Захват мютекса потока или ОЖИДАНИНЕ MODBUS_TIME_WAIT
    { journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexModbusBuzy);err=ERR_485_BUZY; return err;}                                                    
      RS485.set_slave(id);
      result = RS485.readHoldingRegisters(cmd,1);                                                   // послать запрос,
      if (result == RS485.ku8MBSuccess) { err=OK; *ret=RS485.getResponseBuffer(0); }  
      else                              { err=translateErr(result); *ret=0;}
     SemaphoreGive(xModbusSemaphore); return err;
    }
    
// Получить значение 2-x регистров (4 байта) МХ2 в виде целого  числа возвращает код ошибки данные кладутся в ret
int8_t devModbus::readHoldingRegisters32(uint8_t id, uint16_t cmd, uint32_t *ret)             
    {
    uint8_t result;
    // Если шедулер запущен то захватываем семафор
    if(SemaphoreTake(xModbusSemaphore,(MODBUS_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE)      // Захват мютекса потока или ОЖИДАНИНЕ MODBUS_TIME_WAIT
    { journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexModbusBuzy);err=ERR_485_BUZY; return err;}                                                     
      RS485.set_slave(id);
      result = RS485.readHoldingRegisters(cmd,2);                                             // послать запрос,
      if (result == RS485.ku8MBSuccess) {err=OK; *ret=(RS485.getResponseBuffer(0)<<16) | RS485.getResponseBuffer(1);}
      else                              {err=translateErr(result); *ret=0;}
    SemaphoreGive(xModbusSemaphore);
    return err;  
    } 
      
// Получить значение 2-x регистров (4 байта) в виде float возвращает код ошибки данные кладутся в ret
int8_t devModbus::readHoldingRegistersFloat(uint8_t id, uint16_t cmd, float *ret)             
    {
    uint8_t result;
    // Если шедулер запущен то захватываем семафор
    if(SemaphoreTake(xModbusSemaphore,(MODBUS_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE)      // Захват мютекса потока или ОЖИДАНИНЕ MODBUS_TIME_WAIT
    { journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexModbusBuzy);err=ERR_485_BUZY; return err;}                                                     
      RS485.set_slave(id);
      result = RS485.readHoldingRegisters(cmd,2);                                             // послать запрос,
      if (result == RS485.ku8MBSuccess)  {err=OK;*ret=fromInt16ToFloat(RS485.getResponseBuffer(0),RS485.getResponseBuffer(1)); }  
      else                               {err=translateErr(result); *ret=0;}
    SemaphoreGive(xModbusSemaphore);
    return err;  
    }   


// Получить значение N регистров c cmd (2*N байта) МХ2 в виде целого  числа (uint16_t *buf) при ошибке возвращает err
int8_t devModbus::readHoldingRegistersNN(uint8_t id,uint16_t cmd, uint16_t num, uint16_t *buf) 
{
    uint8_t result;
    int16_t i;
    // Если шедулер запущен то захватываем семафор
    if(SemaphoreTake(xModbusSemaphore,(MODBUS_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE)     // Захват мютекса потока или ОЖИДАНИНЕ MODBUS_TIME_WAIT
    { journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexModbusBuzy);err=ERR_485_BUZY; return err;}                                                    
      RS485.set_slave(id);
      result = RS485.readHoldingRegisters(cmd,num);                                           // послать запрос,
      if (result == RS485.ku8MBSuccess) 
      { 
        for (i=0;i<num;i++)   buf[i]=RS485.getResponseBuffer(i);
        err=OK; 
        SemaphoreGive(xModbusSemaphore);
        return err; 
       }  
      else {err=translateErr(result); SemaphoreGive(xModbusSemaphore); return err;}
}

// прочитать отдельный бит возвращает ошибку Modbus function 0x01 Read Coils.
int8_t  devModbus::readCoil(uint8_t id,uint16_t cmd, boolean *ret)                    
{
uint8_t result;
// Если шедулер запущен то захватываем семафор
if(SemaphoreTake(xModbusSemaphore,(MODBUS_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE)            // Захват мютекса потока или ОЖИДАНИНЕ MODBUS_TIME_WAIT
{ journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexModbusBuzy);err=ERR_485_BUZY; return err;}                                                    
  RS485.set_slave(id); 
  result = RS485.readCoils(cmd,1);                                                              // послать запрос, Нумерация регистров с НУЛЯ!!!!
  if (result == RS485.ku8MBSuccess) { err=OK; SemaphoreGive(xModbusSemaphore); if (RS485.getResponseBuffer(0)) *ret=true; else *ret=false; return err;}
  else                              { err=translateErr(result); SemaphoreGive(xModbusSemaphore); return err;}
  }


// ФУНКЦИИ ЗАПИСИ ----------------------------------------------------------------------------------------------
// установить битовый вход функция Modbus function 0x05 Write Single Coil.
int8_t devModbus::writeSingleCoil(uint8_t id,uint16_t cmd, uint8_t u8State)
{
    uint8_t result;
    // Если шедулер запущен то захватываем семафор
    if(SemaphoreTake(xModbusSemaphore,(MODBUS_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE)     // Захват мютекса потока или ОЖИДАНИНЕ MODBUS_TIME_WAIT
    { journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexModbusBuzy);err=ERR_485_BUZY; return err;}                                                    
      RS485.set_slave(id);
      result = RS485.writeSingleCoil(cmd,u8State);                                         // послать запрос,
      if (result == RS485.ku8MBSuccess) 
      { 
        err=OK; 
        SemaphoreGive(xModbusSemaphore);
        return err; 
       }  
      else {err=translateErr(result); SemaphoreGive(xModbusSemaphore); return err;}
}
// Установить значение регистра (2 байта) МХ2 в виде целого  числа возвращает код ошибки данные data
int8_t   devModbus::writeHoldingRegisters16(uint8_t id, uint16_t cmd, uint16_t data)
{
   uint8_t result;
    // Если шедулер запущен то захватываем семафор
    if(SemaphoreTake(xModbusSemaphore,(MODBUS_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE)            // Захват мютекса потока или ОЖИДАНИНЕ MODBUS_TIME_WAIT
    { journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexModbusBuzy);err=ERR_485_BUZY; return err;}                                                    

      RS485.set_slave(id);
      result = RS485.writeSingleRegister(cmd,data);                                               // послать запрос,
      err=translateErr(result);
      SemaphoreGive(xModbusSemaphore);
      return err; 
  
}

// Записать 2 регистра подряд возвращает код ошибки
int8_t devModbus::writeHoldingRegisters32(uint8_t id, uint16_t cmd, uint32_t data)
{
   uint8_t result;
    // Если шедулер запущен то захватываем семафор
    if(SemaphoreTake(xModbusSemaphore,(MODBUS_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE)            // Захват мютекса потока или ОЖИДАНИНЕ MODBUS_TIME_WAIT
    { journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexModbusBuzy);err=ERR_485_BUZY; return err;}                                                    

      RS485.set_slave(id);
      RS485.setTransmitBuffer(0, data >> 16);
      RS485.setTransmitBuffer(1, data & 0xFFFF);
      result = RS485.writeMultipleRegisters(cmd,2);                                                 // послать запрос,
      
      err=translateErr(result);
      SemaphoreGive(xModbusSemaphore);
      return err;                          
}

// Записать float как 2 регистра числа возвращает код ошибки данные dat
int8_t   devModbus::writeHoldingRegistersFloat(uint8_t id, uint16_t cmd, float dat)
{
  union  {
          float f;
          uint16_t i[2];
         } float_map = { .f = dat }; 
    uint8_t result;
    // Если шедулер запущен то захватываем семафор
    if(SemaphoreTake(xModbusSemaphore,(MODBUS_TIME_WAIT/portTICK_PERIOD_MS))==pdFALSE)            // Захват мютекса потока или ОЖИДАНИНЕ MODBUS_TIME_WAIT
    { journal.jprintf((char*)cErrorMutex,__FUNCTION__,MutexModbusBuzy);err=ERR_485_BUZY; return err;}                                                    

      RS485.set_slave(id);
      RS485.setTransmitBuffer(0,float_map.i[1]);
      RS485.setTransmitBuffer(1,float_map.i[0]);
     result = RS485.writeMultipleRegisters(cmd,2);
   //   result = RS485.writeSingleRegister(cmd,dat);                                               // послать запрос,
      err=translateErr(result);
      SemaphoreGive(xModbusSemaphore);
      return err; 
  
}

// Тестирование связи возвращает код ошибки
#ifndef FC_VACON
int8_t devModbus::LinkTestOmronMX2()
{
  uint16_t result, ret;
  err=OK;
  RS485.set_slave(FC_MODBUS_ADR);
  result = RS485.LinkTestOmronMX2Only(TEST_NUMBER);                                              // Послать команду проверки связи
  if (result == RS485.ku8MBSuccess) ret=RS485.getResponseBuffer(0);                              // Получить данные с ответа
  else return err=ERR_485_INIT;                                                                  // Ошибка инициализации
  if (TEST_NUMBER!=ret) return err=ERR_MODBUS_MX2_0x05;  // Контрольные данные не совпали
  return err;                                                    
}
#endif
// Перевод ошибки протокола Модбас (что дает либа) в ошибки ТН, учитывается спицифика Инверторов
// коды ошибок у инверторов могут отличаться
int8_t devModbus::translateErr(uint8_t result)
{
 switch (result)
    {
    // Сдандартные ошибки протокола modbus  едины для всех устройств на модбасе
    case 0x00:      return OK;                  break;  
    case 0x01:      return ERR_MODBUS_0x01;     break;
    case 0x02:      return ERR_MODBUS_0x02;     break;
    case 0x03:      return ERR_MODBUS_0x03;     break;
    case 0x04:      return ERR_MODBUS_0x04;     break;
    case 0xe0:      return ERR_MODBUS_0xe0;     break;
    case 0xe1:      return ERR_MODBUS_0xe1;     break;
    case 0xe2:      return ERR_MODBUS_0xe2;     break;
    case 0xe3:      return ERR_MODBUS_0xe3;     break;    
    #ifdef FC_VACON
      case 0x05:      return ERR_MODBUS_VACON_0x05;break;
      case 0x06:      return ERR_MODBUS_VACON_0x06;break;
      case 0x07:      return ERR_MODBUS_VACON_0x07;break;
      case 0x08:      return ERR_MODBUS_VACON_0x08;break;
    #else
      case 0x05:      return ERR_MODBUS_MX2_0x05; break;
      case 0x08+0x01: return ERR_MODBUS_MX2_0x01; break;
      case 0x08+0x02: return ERR_MODBUS_MX2_0x02; break;
      case 0x08+0x03: return ERR_MODBUS_MX2_0x03; break;
      case 0x08+0x21: return ERR_MODBUS_MX2_0x21; break;
      case 0x08+0x22: return ERR_MODBUS_MX2_0x22; break;
      case 0x08+0x23: return ERR_MODBUS_MX2_0x23; break;
    #endif
    default  :      return ERR_MODBUS_UNKNOW;   break;
    }
 
}
