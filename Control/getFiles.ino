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
// Герерация различных файлов для выгрузки
#include "HeatPump.h"
#include "NoSDpage.h"
#define STR_END   strcat(Socket[thread].outBuf,"\r\n")      // Макрос на конец строки
extern uint16_t sendPacketRTOS(uint8_t thread, const uint8_t * buf, uint16_t len,uint16_t pause);

// Генерация заголовка
void get_Header(uint8_t thread,char *name_file)
{
  // journal.jprintf("$Generate file %s\n",name_file);
    strcpy(Socket[thread].outBuf, WEB_HEADER_OK_CT);
    strcat(Socket[thread].outBuf, WEB_HEADER_TEXT_ATTACH);
    strcat(Socket[thread].outBuf, name_file);
    strcat(Socket[thread].outBuf, "\"");
    strcat(Socket[thread].outBuf, WEB_HEADER_END);
	sendPacketRTOS(thread, (byte*)Socket[thread].outBuf, strlen(Socket[thread].outBuf), 0);
	sendPrintfRTOS(thread, " ------ Народный контроллер теплового насоса ver. %s  сборка %s %s ------\r\nКонфигурация: %s: %s\r\nСоздание файла: %s %s \r\n\r\n", VERSION,__DATE__,__TIME__,CONFIG_NAME,CONFIG_NOTE,NowTimeToStr(),NowDateToStr());
}

// Получить состояние теплового насоса
// header - заголовок файла ставить или нет
void get_txtState(uint8_t thread, boolean header)
{   uint8_t i,j; 
    int16_t x; 
     if (header) get_Header(thread,(char*)"state.txt");   // Если нужен заголовок
     sendPrintfRTOS(thread, "\n  1. Тепловой насос\r\nСостояниеТН: %s\r\nПоследняя ошибка: %d - %s\r\n", HP.StateToStr(),HP.get_errcode(),HP.get_lastErr());
     
     // Состояние ТН
     strcpy(Socket[thread].outBuf,"Режим работы:");strcat(Socket[thread].outBuf,HP.TestToStr()); 
     strcat(Socket[thread].outBuf,"\r\nПоследняя перезагрузка: ");DecodeTimeDate(HP.get_startDT(),Socket[thread].outBuf); 
     strcat(Socket[thread].outBuf,"\r\nВремя с последней перезагрузки: "); TimeIntervalToStr(HP.get_uptime(),Socket[thread].outBuf);  
     strcat(Socket[thread].outBuf,"\r\nПричина последней перезагрузки: ");strcat(Socket[thread].outBuf,ResetCause()); 
       
     strcat(Socket[thread].outBuf,"\r\n\r\n  2. Датчики температуры\r\n");
     for(i=0;i<TNUMBER;i++)   // Информация по  датчикам температуры
         {
              strcat(Socket[thread].outBuf,HP.sTemp[i].get_name()); if((x=8-strlen(HP.sTemp[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(Socket[thread].outBuf," "); }
              strcat(Socket[thread].outBuf,"["); strcat(Socket[thread].outBuf,addressToHex(HP.sTemp[i].get_address())); strcat(Socket[thread].outBuf,"] "); 
              strcat(Socket[thread].outBuf,HP.sTemp[i].get_note());  strcat(Socket[thread].outBuf,": "); 
              if (HP.sTemp[i].get_present())
                {
                  _ftoa(Socket[thread].outBuf,(float)HP.sTemp[i].get_Temp()/100.0,2);
                  if (HP.sTemp[i].get_lastErr()!=OK) { strcat(Socket[thread].outBuf," error:"); _itoa(HP.sTemp[i].get_lastErr(),Socket[thread].outBuf); } 
                  STR_END; 
                }
                else strcat(Socket[thread].outBuf," absent\r\n"); 
         }
      if (HP.get_modeHouse() ==pCOOL) strcat(Socket[thread].outBuf,"Внимание! ТН в режиме охлаждения роли испарителя и конденсатора меняются местами");
      sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));
      
      strcpy(Socket[thread].outBuf,"\n  3. Аналоговые датчики\r\n"); // Новый пакет
      for(i=0;i<ANUMBER;i++)   // Информация по  датчикам температуры
         {   
            strcat(Socket[thread].outBuf,HP.sADC[i].get_name()); if((x=8-strlen(HP.sADC[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(Socket[thread].outBuf," "); }
            strcat(Socket[thread].outBuf,HP.sADC[i].get_note());  strcat(Socket[thread].outBuf,": "); 
            if (HP.sADC[i].get_present()) 
                { 
                  _ftoa(Socket[thread].outBuf,(float)HP.sADC[i].get_Press()/100.0,2); if (HP.sADC[i].get_lastErr()!=OK ) { strcat(Socket[thread].outBuf," error:"); _itoa(HP.sADC[i].get_lastErr(),Socket[thread].outBuf); }
                  STR_END; 
      
                } 
            else strcat(Socket[thread].outBuf," absent\r\n"); 
         }
   
       strcat(Socket[thread].outBuf,"\n  4. Датчики 'сухой контакт'\r\n");
       for(i=0;i<INUMBER;i++)  
           {
            strcat(Socket[thread].outBuf,HP.sInput[i].get_name()); if((x=8-strlen(HP.sInput[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(Socket[thread].outBuf," "); }
            strcat(Socket[thread].outBuf,HP.sInput[i].get_note());  strcat(Socket[thread].outBuf,": "); 
      
            if (HP.sInput[i].get_present()) 
                { 
                  _itoa(HP.sInput[i].get_Input(),Socket[thread].outBuf); 
                  strcat(Socket[thread].outBuf," alarm:"); _itoa(HP.sInput[i].get_alarmInput(),Socket[thread].outBuf); STR_END;   
                }
                else strcat(Socket[thread].outBuf," absent\r\n");      
           }
       
              
       strcat(Socket[thread].outBuf,"\n  5. Реле\r\n");
       for(i=0;i<RNUMBER;i++)  
           {
            strcat(Socket[thread].outBuf,HP.dRelay[i].get_name()); if((x=8-strlen(HP.dRelay[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(Socket[thread].outBuf," "); }
            strcat(Socket[thread].outBuf,HP.dRelay[i].get_note());  strcat(Socket[thread].outBuf,": "); 
      
            if (HP.dRelay[i].get_present()) _itoa(HP.dRelay[i].get_Relay(),Socket[thread].outBuf); 
            else strcat(Socket[thread].outBuf," absent"); 
            STR_END;         
           }
       #ifdef EEV_DEF
        strcat(Socket[thread].outBuf,"\n  6. ЭРВ - ");  strcat(Socket[thread].outBuf,HP.dEEV.get_note()); STR_END;
        if (HP.dEEV.get_present()) 
         {
             strcat(Socket[thread].outBuf,"Минимальное положение (шаги): ");  _itoa(HP.dEEV.get_minEEV(),Socket[thread].outBuf); STR_END;
             strcat(Socket[thread].outBuf,"Полное открыте (шаги):");  _itoa(HP.dEEV.get_maxEEV(),Socket[thread].outBuf);STR_END;
             strcat(Socket[thread].outBuf,"Правило управления ЭРВ: ");
             HP.dEEV.get_ruleEEVtext(Socket[thread].outBuf); STR_END;
             strcat(Socket[thread].outBuf,"\nТекущее положение (шаги): ");  _itoa(HP.dEEV.get_EEV(),Socket[thread].outBuf); STR_END;
       //      strcat(Socket[thread].outBuf,"Формула перегрева: ");
       //      if (HP.sADC[PEVA].get_present()) strcat(Socket[thread].outBuf,"TEVAOUT-T[PEVA]\r\n"); else strcat(Socket[thread].outBuf,"TEVAOUT-TEVAIN\r\n"); 
             strcat(Socket[thread].outBuf,"Текущий перегрев (градусы): ");  _ftoa(Socket[thread].outBuf,(float)HP.dEEV.get_Overheat()/100.0,2); STR_END;
         }  
        #else
        strcat(Socket[thread].outBuf,"\n  6. EEV absent\r\n");    
        #endif
         
       strcat(Socket[thread].outBuf,"\n  7. Частотный преобразователь\r\n");
       if (HP.dFC.get_present()) 
         {
#ifdef FC_VACON
             strcat(Socket[thread].outBuf,"Целевая скорость [%]: ");    _ftoa(Socket[thread].outBuf,(float)HP.dFC.get_target()/100.0,2); STR_END;
             strcat(Socket[thread].outBuf,"Текущая частота [Гц]: ");    _ftoa(Socket[thread].outBuf,(float)HP.dFC.get_frequency()/100.0,2); STR_END;
#else
              strcat(Socket[thread].outBuf,"Целевая частота [Гц]: ");    _ftoa(Socket[thread].outBuf,(float)HP.dFC.get_target()/100.0,2); STR_END;
              strcat(Socket[thread].outBuf,"Текущая частота [Гц]: ");    _ftoa(Socket[thread].outBuf,(float)HP.dFC.get_frequency()/100.0,2); STR_END;
#endif
              strcat(Socket[thread].outBuf,"Текущая мощность [кВт]: ");  _ftoa(Socket[thread].outBuf,(float)HP.dFC.get_power()/1000.0,3); STR_END;
#ifdef FC_ANALOG_CONTROL // Аналоговое управление
              strcat(Socket[thread].outBuf,"ЦАП дискреты: ");            _itoa(HP.dFC.get_DAC(),Socket[thread].outBuf); STR_END;
#endif
              }
        else strcat(Socket[thread].outBuf,"FC absent\r\n");    

        strcat(Socket[thread].outBuf,"\n  8. Электросчетчик SDM\r\n");
         #ifdef USE_ELECTROMETER_SDM
  //         strcat(Socket[thread].outBuf,"MAX напряжение при контроле входного напряжения [В]: ");   HP.dSDM.get_paramSDM((char*)sdm_MAX_VOLTAGE,Socket[thread].outBuf); STR_END;
  //         strcat(Socket[thread].outBuf,"MIN напряжение при контроле входного напряжения [В]: ");   HP.dSDM.get_paramSDM((char*)sdm_MIN_VOLTAGE,Socket[thread].outBuf); STR_END;
  //         strcat(Socket[thread].outBuf,"MIN максимальаня мощность при контроле мощности [Вт]: ");  HP.dSDM.get_paramSDM((char*)sdm_MAX_POWER,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Текущее входное напряжение [В]: ");                          HP.dSDM.get_paramSDM((char*)sdm_VOLTAGE,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Текущий потребляемый ток ТН [А]: ");                         HP.dSDM.get_paramSDM((char*)sdm_CURRENT,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Текущая потребляемая активная мощность ТН [Вт]: ");          HP.dSDM.get_paramSDM((char*)sdm_ACPOWER,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Текущая потребляемая суммарная мощность ТН [ВА]: ");        HP.dSDM.get_paramSDM((char*)sdm_POWER,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Коэффициент мощности: ");                                    HP.dSDM.get_paramSDM((char*)sdm_POW_FACTOR,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Угол фазы (градусы): ");                                     HP.dSDM.get_paramSDM((char*)sdm_PHASE,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Суммарная активная энергия [кВт/ч]: ");                     HP.dSDM.get_paramSDM((char*)sdm_ACENERGY,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Cостояние связи со счетчиком: ");                            HP.dSDM.get_paramSDM((char*)sdm_LINK,Socket[thread].outBuf); STR_END;
         #else
           strcat(Socket[thread].outBuf,"SDM absent\r\n");
         #endif  
   
   strcat(Socket[thread].outBuf,"\n  9. Частотные датчики потока\r\n");
     for(i=0;i<FNUMBER;i++)  
           {
            strcat(Socket[thread].outBuf,HP.sFrequency[i].get_name()); if((x=8-strlen(HP.sFrequency[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(Socket[thread].outBuf," "); }
            strcat(Socket[thread].outBuf,HP.sFrequency[i].get_note());  strcat(Socket[thread].outBuf,": "); 
            if (HP.sFrequency[i].get_present()) _ftoa(Socket[thread].outBuf,(float)HP.sFrequency[i].get_Value()/1000.0,3);
            else strcat(Socket[thread].outBuf," absent"); 
            STR_END;         
           }
   
        
  sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));   
}


// Получить настройки в текстовом виде
void get_txtSettings(uint8_t thread)
{
     uint8_t i,j;
     int16_t x; 
     
     get_Header(thread,(char*)"settings.txt");
     sendPrintfRTOS(thread, "Состояние ТН: %s\r\nПоследняя ошибка: %d - %s\r\n", HP.StateToStr(),HP.get_errcode(),HP.get_lastErr());
     strcpy(Socket[thread].outBuf,"\n  1. Общие настройки\r\n");
     strcat(Socket[thread].outBuf,"Режим работы :");
     switch ((MODE_HP)HP.get_modeHouse() )   // режим работы отопления
                   {
                    case pOFF:  strcat(Socket[thread].outBuf,"Выключено\r\n"); break;
                    case pHEAT: strcat(Socket[thread].outBuf,"Отопление\r\n"); break;
                    case pCOOL: strcat(Socket[thread].outBuf,"Охлаждение\r\n"); break;
                    default:    strcat(Socket[thread].outBuf,"Выключено\r\n"); break;
                   }  
     strcat(Socket[thread].outBuf,"Бойлер:"); HP.Prof.get_boiler((char*)boil_BOILER_ON,Socket[thread].outBuf); STR_END;
     
 
     strcat(Socket[thread].outBuf,"\n  1.1 Отопление\r\n");
     strcat(Socket[thread].outBuf,"Алгоритм работы отопления: "); HP.Prof.get_paramHeatHP((char*)hp_RULE,Socket[thread].outBuf,HP.dFC.get_present()); STR_END;
     strcat(Socket[thread].outBuf,"Целевая температура дома (C°): ");HP.Prof.get_paramHeatHP((char*)hp_TEMP1,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Целевая температура обратки (C°):");HP.Prof.get_paramHeatHP((char*)hp_TEMP2,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Что является целью: ");  HP.Prof.get_paramHeatHP((char*)hp_TARGET,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Гистерезис целевой температуры (C°): ");HP.Prof.get_paramHeatHP((char*)hp_DTEMP,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
    
     strcat(Socket[thread].outBuf,"  - Использование ночного тарифа -\r\n");
     strcat(Socket[thread].outBuf,"Добавка к целевой температуре по часам (C°): ");HP.Prof.get_paramHeatHP((char*)ADD_DELTA_TEMP,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Начальный час добавки к целевой температуре: ");HP.Prof.get_paramHeatHP((char*)ADD_DELTA_HOUR,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Конечный час добавки к целевой температуре: ");HP.Prof.get_paramHeatHP((char*)ADD_DELTA_END_HOUR,Socket[thread].outBuf,HP.dFC.get_present());STR_END;  
       
     if (HP.get_ruleHeat()!=pHYSTERESIS)
         {
         strcat(Socket[thread].outBuf," - ПИД -\r\n");
         strcat(Socket[thread].outBuf,"Целевая температура подачи ПИД ТН (C°): ");HP.Prof.get_paramHeatHP((char*)hp_TEMP_PID,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
         strcat(Socket[thread].outBuf,"Постоянная интегрирования времени в секундах ПИД ТН: ");HP.Prof.get_paramHeatHP((char*)hp_HP_TIME,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
         strcat(Socket[thread].outBuf,"Пропорциональная составляющая ПИД ТН: ");HP.Prof.get_paramHeatHP((char*)hp_HP_PRO,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
         strcat(Socket[thread].outBuf,"Интегральная составляющая ПИД ТН: ");HP.Prof.get_paramHeatHP((char*)hp_HP_IN,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
         strcat(Socket[thread].outBuf,"Дифференциальная составляющая ПИД ТН: ");HP.Prof.get_paramHeatHP((char*)hp_HP_DIF,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
         }
         
     strcat(Socket[thread].outBuf,"  - Опции -\r\n"); 
     strcat(Socket[thread].outBuf,"Теплый пол: ");HP.Prof.get_paramHeatHP((char*)hp_HEAT_FLOOR,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Использовать солнечный коллектор (TSUN*+2°>TEVAOUTG): ");HP.Prof.get_paramHeatHP((char*)hp_SUN,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Использование погодозависимости: ");HP.Prof.get_paramHeatHP((char*)hp_WEATHER,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Коэффициент погодозависимости: ");HP.Prof.get_paramHeatHP((char*)hp_K_WEATHER,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
        
     strcat(Socket[thread].outBuf," - Защиты - \r\n");
     strcat(Socket[thread].outBuf,"Tемпература подачи максимальная (C°): ");HP.Prof.get_paramHeatHP((char*)hp_TEMP_IN,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Tемпература обратки минимальная (C°): ");HP.Prof.get_paramHeatHP((char*)hp_TEMP_OUT,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Максимальная разность температур конденсатора (C°): ");HP.Prof.get_paramHeatHP((char*)hp_D_TEMP,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf)); 
 
     strcpy(Socket[thread].outBuf,"\n  1.2 Охлаждение\r\n");
     strcat(Socket[thread].outBuf,"Алгоритм работы отопления: "); HP.Prof.get_paramCoolHP((char*)hp_RULE,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Целевая температура дома (C°): ");HP.Prof.get_paramCoolHP((char*)hp_TEMP1,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Целевая температура обратки (C°):");HP.Prof.get_paramCoolHP((char*)hp_TEMP2,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Что является целью: ");  HP.Prof.get_paramCoolHP((char*)hp_TARGET,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Гистерезис целевой температуры (C°): ");HP.Prof.get_paramCoolHP((char*)hp_DTEMP,Socket[thread].outBuf,HP.dFC.get_present());STR_END;

     if (HP.get_ruleCool()!=pHYSTERESIS)
         {
         strcat(Socket[thread].outBuf," - ПИД -\r\n");	
         strcat(Socket[thread].outBuf,"Целевая температура подачи ПИД ТН (C°): ");HP.Prof.get_paramCoolHP((char*)hp_TEMP_PID,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
         strcat(Socket[thread].outBuf,"Постоянная интегрирования времени в секундах ПИД ТН: ");HP.Prof.get_paramCoolHP((char*)hp_HP_TIME,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
         strcat(Socket[thread].outBuf,"Пропорциональная составляющая ПИД ТН: ");HP.Prof.get_paramCoolHP((char*)hp_HP_PRO,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
         strcat(Socket[thread].outBuf,"Интегральная составляющая ПИД ТН: ");HP.Prof.get_paramCoolHP((char*)hp_HP_IN,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
         strcat(Socket[thread].outBuf,"Дифференциальная составляющая ПИД ТН: ");HP.Prof.get_paramCoolHP((char*)hp_HP_DIF,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
         }
     strcat(Socket[thread].outBuf," - Опции -\r\n"); 
     strcat(Socket[thread].outBuf,"Теплый пол: ");HP.Prof.get_paramCoolHP((char*)hp_HEAT_FLOOR,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Использовать солнечный коллектор (TSUN*+2°>TEVAOUTG): ");HP.Prof.get_paramCoolHP((char*)hp_SUN,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Использование погодозависимости: ");HP.Prof.get_paramCoolHP((char*)hp_WEATHER,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Коэффициент погодозависимости: ");HP.Prof.get_paramCoolHP((char*)hp_K_WEATHER,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
         
     strcat(Socket[thread].outBuf," - Защиты -\r\n");
     strcat(Socket[thread].outBuf,"Tемпература подачи максимальная (C°): ");HP.Prof.get_paramCoolHP((char*)hp_TEMP_IN,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Tемпература обратки минимальная (C°): ");HP.Prof.get_paramCoolHP((char*)hp_TEMP_OUT,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     strcat(Socket[thread].outBuf,"Максимальная разность температур конденсатора (C°): ");HP.Prof.get_paramCoolHP((char*)hp_D_TEMP,Socket[thread].outBuf,HP.dFC.get_present());STR_END;
     sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));   
     
     strcpy(Socket[thread].outBuf,"\n  1.3 ГВС\r\n");
     strcat(Socket[thread].outBuf,"Целевая температура бойлера (C°): ");HP.Prof.get_boiler((char*)boil_TEMP_TARGET,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Гистерезис целевой температуры (C°): ");HP.Prof.get_boiler((char*)boil_DTARGET,Socket[thread].outBuf);STR_END;
     
     strcat(Socket[thread].outBuf," - Использование ночного тарифа - \r\n");
     strcat(Socket[thread].outBuf,"Добавка к целевой температуре по часам (C°): ");HP.Prof.get_boiler((char*)ADD_DELTA_TEMP,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Начальный час добавки к целевой температуре: ");HP.Prof.get_boiler((char*)ADD_DELTA_HOUR,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Конечный час добавки к целевой температуре: ");HP.Prof.get_boiler((char*)ADD_DELTA_END_HOUR,Socket[thread].outBuf);STR_END;  
     
     strcat(Socket[thread].outBuf," - Встроенный ТЭН -\r\n");
     strcat(Socket[thread].outBuf,"Ускоренный нагрев ГВС (одновременное использование ТЭНа и ТН для нагрева): ");HP.Prof.get_boiler((char*)boil_TURBO_BOILER,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Использование ТЭНа для догрева бойлера до высоких температур: ");HP.Prof.get_boiler((char*)boil_ADD_HEATING,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Значение температуры для включения догрева бойлера (C°): "); HP.Prof.get_boiler((char*)boil_TEMP_RBOILER,Socket[thread].outBuf) ;STR_END;
     
     if (HP.dFC.get_present()) // Только для инвертора
         {
         strcat(Socket[thread].outBuf," - ПИД -\r\n");
         strcat(Socket[thread].outBuf,"Целевая температура подачи (C°): ");HP.Prof.get_boiler((char*)boil_BOIL_TEMP,Socket[thread].outBuf);STR_END;
         strcat(Socket[thread].outBuf,"Постоянная интегрирования времени (сек.): ");HP.Prof.get_boiler((char*)boil_BOIL_TIME,Socket[thread].outBuf);STR_END;
         strcat(Socket[thread].outBuf,"Пропорциональная составляющая: ");HP.Prof.get_boiler((char*)boil_BOIL_PRO,Socket[thread].outBuf);STR_END;
         strcat(Socket[thread].outBuf,"Интегральная составляющая: ");HP.Prof.get_boiler((char*)boil_BOIL_IN,Socket[thread].outBuf);STR_END;
         strcat(Socket[thread].outBuf,"Дифференциальная составляющая: ");HP.Prof.get_boiler((char*)boil_BOIL_DIF,Socket[thread].outBuf);STR_END;
         }

     strcat(Socket[thread].outBuf," - Опции -\r\n");
     strcat(Socket[thread].outBuf,"Использование расписания: ");HP.Prof.get_boiler((char*)boil_SCHEDULER_ON,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Работа по расписанию только для ТЭНа: ");HP.Prof.get_boiler((char*)boil_SCHEDULER_ADDHEAT,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Борьба с сальмонелой: ");HP.Prof.get_boiler((char*)boil_SALLMONELA,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Управления циркуляционным насосом ГВС: ");HP.Prof.get_boiler((char*)boil_CIRCULATION,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Время работы насоса ГВС (мин.): ");HP.Prof.get_boiler((char*)boil_CIRCUL_WORK,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Пауза в работе насоса ГВС (мин.): ");HP.Prof.get_boiler((char*)boil_CIRCUL_PAUSE,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"При нагреве бойлера сбрасывать ""избыточное"" тепло систему отопления: ");HP.Prof.get_boiler((char*)boil_RESET_HEAT,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Время сброса тепла (мин.): ");HP.Prof.get_boiler((char*)boil_RESET_TIME,Socket[thread].outBuf);  STR_END;
     
     strcat(Socket[thread].outBuf," - Защиты при работе теплового насоса -\r\n");
     strcat(Socket[thread].outBuf,"Tемпература подачи максимальная (C°): ");HP.Prof.get_boiler((char*)boil_TEMP_MAX,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf," - Расписание работы -\r\n");
     HP.Prof.get_boiler((char*)boil_SCHEDULER,Socket[thread].outBuf);STR_END;
     sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));  

     strcpy(Socket[thread].outBuf,"\n  1.4 Текущий профиль ТН\r\n");  
     strcat(Socket[thread].outBuf,"Номер профиля: "); HP.Prof.get_paramProfile((char*)prof_ID_PROFILE,Socket[thread].outBuf);STR_END; 
     strcat(Socket[thread].outBuf,"Имя профиля: "); HP.Prof.get_paramProfile((char*)prof_NAME_PROFILE,Socket[thread].outBuf);STR_END; 
     strcat(Socket[thread].outBuf,"Описание профиля: "); HP.Prof.get_paramProfile((char*)prof_NOTE_PROFILE,Socket[thread].outBuf);STR_END; 
     strcat(Socket[thread].outBuf,"Дата изменения профиля: "); HP.Prof.get_paramProfile((char*)prof_DATE_PROFILE,Socket[thread].outBuf);STR_END; 
     strcat(Socket[thread].outBuf,"CRC16 профиля: "); HP.Prof.get_paramProfile((char*)prof_CRC16_PROFILE,Socket[thread].outBuf);STR_END; 
     sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));  
     
     strcpy(Socket[thread].outBuf,"\n  1.5 Опции ТН\r\n");
     strcat(Socket[thread].outBuf,"Расположение файлов веб-сервера на SPI Flash (иначе на SD карте): "); HP.get_optionHP((char*)option_WebOnSPIFlash,Socket[thread].outBuf);STR_END; 
     strcat(Socket[thread].outBuf,"Сохранение состояния ТН в ЕЕПРОМ, для восстановления его после сброса: "); HP.get_optionHP((char*)option_SAVE_ON,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Число попыток пуска компрессора: "); HP.get_optionHP((char*)option_ATTEMPT,Socket[thread].outBuf);STR_END;
    
     strcat(Socket[thread].outBuf," - Настройка встроенных графиков -\r\n");
     strcat(Socket[thread].outBuf,"Период накопления графиков (сек): "); HP.get_optionHP((char*)option_TIME_CHART,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Запись графиков на карту памяти: "); HP.get_optionHP((char*)option_History,Socket[thread].outBuf);STR_END;

     strcat(Socket[thread].outBuf," - Настройки контура отопления -\r\n");
     strcat(Socket[thread].outBuf,"Использование дополнительного ТЭНа отопления: "); HP.get_optionHP((char*)option_ADD_HEAT,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Температура для управления дополнительным ТЭНом (C°): ");HP.get_optionHP((char*)option_TEMP_RHEAT,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Время работы насоса отопления при выключенном компрессоре (сек): "); HP.get_optionHP((char*)option_PUMP_WORK,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Время паузы насоса отопления при выключенном компрессоре (сек): "); HP.get_optionHP((char*)option_PUMP_PAUSE,Socket[thread].outBuf);STR_END;
     
     // Солнечный коллектор
     strcat(Socket[thread].outBuf," - Настройки солнечного коллектора -\r\n");
     strcat(Socket[thread].outBuf,"Использовать солнечный коллектор для регенерации геоконтура в простое: "); HP.get_optionHP((char*)option_SunRegGeo,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Температура начала регенерации геоконтура с помощью СК (+Δ) (°C): "); HP.get_optionHP((char*)option_SunRegGeoTemp,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Разница (Δ) температур для включения СК (°C): "); HP.get_optionHP((char*)option_SunTDelta,Socket[thread].outBuf);STR_END;

     strcat(Socket[thread].outBuf," - Времена и задержки -\r\n");
     strcat(Socket[thread].outBuf,"Задержка включения компрессора после включения насосов (сек): "); HP.get_optionHP((char*)option_DELAY_ON_PUMP,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Задержка выключения насосов после выключения компрессора (сек): "); HP.get_optionHP((char*)option_DELAY_OFF_PUMP,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Задержка включения ТН после внезапного сброса контроллера (сек): "); HP.get_optionHP((char*)option_DELAY_START_RES,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Задержка перед повторным включением ТН при ошибке (попытки пуска) (сек): "); HP.get_optionHP((char*)option_DELAY_REPEAD_START,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Задержка после срабатывания датчика перед включением разморозки (сек): "); HP.get_optionHP((char*)option_DELAY_DEFROST_ON,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Задержка перед выключением разморозки (сек): "); HP.get_optionHP((char*)option_DELAY_DEFROST_OFF,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Задержка между переключением реверсивного клапана и включением компрессора (сек): "); HP.get_optionHP((char*)option_DELAY_R4WAY,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Пауза после переключение ГВС (сек): "); HP.get_optionHP((char*)option_DELAY_BOILER_SW,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Время на сколько блокируются защиты при переходе с ГВС (сек): "); HP.get_optionHP((char*)option_DELAY_BOILER_OFF,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Минимальное время простоя компрессора (мин): "); HP.get_optionHP((char*)option_PAUSE,Socket[thread].outBuf);STR_END;

     strcat(Socket[thread].outBuf," - Дополнительное оборудование -\r\n");
     strcat(Socket[thread].outBuf,"Логировать не критичные ошибки электро-счетчика SDM: ");  HP.get_optionHP((char*)option_SDM_LOG_ERR,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Логировать обмен между беспроводными датчиками: "); HP.get_optionHP((char*)option_LogWirelessSensors,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Генерация звукового сигнала: "); HP.get_optionHP((char*)option_BEEP,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Использование Nextion дисплея: "); HP.get_optionHP((char*)option_NEXTION,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Включать дисплей при пуске компрессора: "); HP.get_optionHP((char*)option_NEXTION_WORK,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Время засыпания дисплея Nextion (мин.): "); HP.get_optionHP((char*)option_NEXT_SLEEP,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Яркость дисплея Nextion в %: "); HP.get_optionHP((char*)option_NEXT_DIM,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"На шинах 1-Wire(DS2482) только один датчик: "); if((HP.get_flags() & (1<<f1Wire2TSngl))) strcat(Socket[thread].outBuf, "2 "); if((HP.get_flags() & (1<<f1Wire3TSngl))) strcat(Socket[thread].outBuf, "3 "); if((HP.get_flags() & (1<<f1Wire4TSngl))) strcat(Socket[thread].outBuf, "4");
     sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));  
    
     strcpy(Socket[thread].outBuf,"\n\n  1.6 Сетевые настройки\r\n");
     strcat(Socket[thread].outBuf,"Использование DHCP: "); HP.get_network((char*)net_DHCP,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"IP адрес контроллера: "); HP.get_network((char*)net_IP,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"DNS сервер: "); HP.get_network((char*)net_DNS,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Шлюз: "); HP.get_network((char*)net_GATEWAY,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Маска подсети: "); HP.get_network((char*)net_SUBNET,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"MAC адрес контроллера: "); HP.get_network((char*)net_MAC,Socket[thread].outBuf);STR_END;

     strcat(Socket[thread].outBuf,"Запрет пинга контроллера: "); HP.get_network((char*)net_NO_PING,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Использование паролей: ");   HP.get_network((char*)net_PASS,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Пароль пользователя (user): "); HP.get_network((char*)net_PASSUSER,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Пароль администратора (admin): "); HP.get_network((char*)net_PASSADMIN,Socket[thread].outBuf); STR_END;

     strcat(Socket[thread].outBuf,"Проверка ping до сервера: "); HP.get_network((char*)net_PING_TIME,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Адрес пингуемого сервера: "); HP.get_network((char*)net_PING_ADR,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Ежеминутный контроль связи с Wiznet W5xxx: "); HP.get_network((char*)net_INIT_W5200,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Время очистки сокетов: "); HP.get_network((char*)net_RES_SOCKET,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Время сброса чипа: "); strcat(Socket[thread].outBuf,nameWiznet);strcat(Socket[thread].outBuf,": "); HP.get_network((char*)net_RES_W5200,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Размер пакета для отправки в байтах: "); HP.get_network((char*)net_SIZE_PACKET,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Не ожидать получения ACK при посылке следующего пакета: "); HP.get_network((char*)net_NO_ACK,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Пауза перед отправкой следующего пакета, если нет ожидания ACK. (мсек): "); HP.get_network((char*)net_DELAY_ACK,Socket[thread].outBuf);STR_END;

     
     strcat(Socket[thread].outBuf,"\n  1.7 Настройки даты и времени\r\n");
     strcat(Socket[thread].outBuf,"Установленная дата: "); HP.get_datetime((char*)time_DATE,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Установленное время: "); HP.get_datetime((char*)time_TIME,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Адрес NTP сервера: "); HP.get_datetime((char*)time_NTP,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Часовой пояс (часы): "); HP.get_datetime((char*)time_TIMEZONE,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Синхронизация времени по NTP раз в сутки: "); HP.get_datetime((char*)time_UPDATE,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Синхронизация раз в час с I2C часами: "); HP.get_datetime((char*)time_UPDATE_I2C,Socket[thread].outBuf);STR_END;
  
     sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));  
      
     strcpy(Socket[thread].outBuf,"\n  1.8 Уведомления\r\n");
     strcat(Socket[thread].outBuf,"Сброс контроллера: "); HP.message.get_messageSetting((char*)mess_MESS_RESET,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Возникновение ошибок: "); HP.message.get_messageSetting((char*)mess_MESS_ERROR,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Сигнал «жизни» (ежедневно в 12-00): "); HP.message.get_messageSetting((char*)mess_MESS_LIFE,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Достижение граничной температуры: "); HP.message.get_messageSetting((char*)mess_MESS_TEMP,Socket[thread].outBuf);  STR_END;
     strcat(Socket[thread].outBuf," Граничная температура в доме (если меньше то посылается уведомление): "); HP.message.get_messageSetting((char*)mess_MESS_TIN,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf," Граничная температура бойлера (если меньше то посылается уведомление): "); HP.message.get_messageSetting((char*)mess_MESS_TBOILER,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf," Граничная температура компрессора (если больше то посылается уведомление): "); HP.message.get_messageSetting((char*)mess_MESS_TCOMP,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Проблемы с SD картой и SPI flash: "); HP.message.get_messageSetting((char*)mess_MESS_SD,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Прочие уведомления: "); HP.message.get_messageSetting((char*)mess_MESS_WARNING,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf," - Настройка отправки почты -\r\n");
     strcat(Socket[thread].outBuf,"Посылать уведомления по почте: "); HP.message.get_messageSetting((char*)mess_MAIL,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Адрес smtp сервера: "); HP.message.get_messageSetting((char*)mess_SMTP_SERVER,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Порт smtp сервера: "); HP.message.get_messageSetting((char*)mess_SMTP_PORT,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Необходимость авторизации на сервере: ");HP.message.get_messageSetting((char*)mess_MAIL_AUTH,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Логин для входа: "); HP.message.get_messageSetting((char*)mess_SMTP_LOGIN,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Пароль для входа: "); HP.message.get_messageSetting((char*)mess_SMTP_PASS,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Адрес отправителя: "); HP.message.get_messageSetting((char*)mess_SMTP_MAILTO,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Адрес получателя: "); HP.message.get_messageSetting((char*)mess_SMTP_RCPTTO,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Добавлять в уведомление информацию о состоянии ТН: "); HP.message.get_messageSetting((char*)mess_MAIL_INFO,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf," - Настройка отправки SMS -\r\n");
     strcat(Socket[thread].outBuf,"Посылать уведомления по SMS: "); HP.message.get_messageSetting((char*)mess_SMS,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Телефон получателя: "); HP.message.get_messageSetting((char*)mess_SMS_PHONE,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Сервис отправки SMS: ");HP.message.get_messageSetting((char*)mess_SMS_SERVICE,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Адрес сервиса отправки SMS: ");HP.message.get_messageSetting((char*)mess_SMS_IP,Socket[thread].outBuf);STR_END;
     HP.message.get_messageSetting((char*)mess_SMS_NAMEP1,Socket[thread].outBuf);strcat(Socket[thread].outBuf,": ");HP.message.get_messageSetting((char*)mess_SMS_P1,Socket[thread].outBuf);STR_END;
     HP.message.get_messageSetting((char*)mess_SMS_NAMEP2,Socket[thread].outBuf);strcat(Socket[thread].outBuf,": ");HP.message.get_messageSetting((char*)mess_SMS_P2,Socket[thread].outBuf);STR_END;
     sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf)); 
      
      // MQTT
     strcpy(Socket[thread].outBuf,"\n  1.9 Настройка MQTT\r\n");
      #ifdef MQTT
     strcat(Socket[thread].outBuf,"Включить отправку на сервер MQTT: ");HP.clMQTT.get_paramMQTT((char*)mqtt_USE_MQTT,Socket[thread].outBuf);  STR_END;
     strcat(Socket[thread].outBuf,"Отправка на сервер ThingSpeak: "); HP.clMQTT.get_paramMQTT((char*)mqtt_USE_TS,Socket[thread].outBuf);  STR_END;
     strcat(Socket[thread].outBuf,"Включить отсылку дополнительных данных: "); HP.clMQTT.get_paramMQTT((char*)mqtt_BIG_MQTT,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Включить передачу данных с электросчетчика SDM120: "); HP.clMQTT.get_paramMQTT((char*)mqtt_SDM_MQTT,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Включить передачу данных об инверторе: ");HP.clMQTT.get_paramMQTT((char*)mqtt_FC_MQTT,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Включить передачу данных об эффективности ТН: "); HP.clMQTT.get_paramMQTT((char*)mqtt_COP_MQTT,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Интервал передачи данных [1...1000] (минут): "); HP.clMQTT.get_paramMQTT((char*)mqtt_TIME_MQTT,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Адрес MQTT сервера: ");HP.clMQTT.get_paramMQTT((char*)mqtt_ADR_MQTT,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Порт MQTT сервера: "); HP.clMQTT.get_paramMQTT((char*)mqtt_PORT_MQTT,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Логин для входа: "); HP.clMQTT.get_paramMQTT((char*)mqtt_LOGIN_MQTT,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Пароль для входа: "); HP.clMQTT.get_paramMQTT((char*)mqtt_PASSWORD_MQTT,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Идентификатор клиента: "); HP.clMQTT.get_paramMQTT((char*)mqtt_ID_MQTT,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf," - Сервис <Народный мониторинг> -\r\n");
     strcat(Socket[thread].outBuf,"Включить передачу данных: "); HP.clMQTT.get_paramMQTT((char*)mqtt_USE_NARMON,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Посылать расширенный набор данных: "); HP.clMQTT.get_paramMQTT((char*)mqtt_BIG_NARMON,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Адрес сервиса: ");HP.clMQTT.get_paramMQTT((char*)mqtt_ADR_NARMON,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Порт сервиса: "); HP.clMQTT.get_paramMQTT((char*)mqtt_PORT_NARMON,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Логин для входа (получается при регистрации): "); HP.clMQTT.get_paramMQTT((char*)mqtt_LOGIN_NARMON,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Личный код для передачи (смотреть в разделе API MQTT сервиса): "); HP.clMQTT.get_paramMQTT((char*)mqtt_PASSWORD_NARMON,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Имя устройства (корень всех топиков): "); HP.clMQTT.get_paramMQTT((char*)mqtt_ID_NARMON,Socket[thread].outBuf);STR_END;
     #else
      strcat(Socket[thread].outBuf,"Не поддерживается, нет в прошивке");
     #endif
     
     strcat(Socket[thread].outBuf,"\n  2.1 Датчики температуры\r\n");
     for(i=0;i<TNUMBER;i++)   // Информация по  датчикам температуры
         {
              strcat(Socket[thread].outBuf,HP.sTemp[i].get_name()); if((x=8-strlen(HP.sTemp[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(Socket[thread].outBuf," "); } 
              strcat(Socket[thread].outBuf,"["); strcat(Socket[thread].outBuf,addressToHex(HP.sTemp[i].get_address())); strcat(Socket[thread].outBuf,"] ");
              strcat(Socket[thread].outBuf,HP.sTemp[i].get_note());  strcat(Socket[thread].outBuf,": "); 
        
              if (HP.sTemp[i].get_present())
                {
                  strcat(Socket[thread].outBuf," T=");      _ftoa(Socket[thread].outBuf,(float)HP.sTemp[i].get_Temp()/100.0,2);
                  strcat(Socket[thread].outBuf,", Tmin=");  _ftoa(Socket[thread].outBuf,(float)HP.sTemp[i].get_minTemp()/100.0,2);
                  strcat(Socket[thread].outBuf,", Tmax=");  _ftoa(Socket[thread].outBuf,(float)HP.sTemp[i].get_maxTemp()/100.0,2);
                  strcat(Socket[thread].outBuf,", Terr=");  _ftoa(Socket[thread].outBuf,(float)HP.sTemp[i].get_errTemp()/100.0,2);
                  strcat(Socket[thread].outBuf,", Ttest="); _ftoa(Socket[thread].outBuf,(float)HP.sTemp[i].get_testTemp()/100.0,2);
                  if (HP.sTemp[i].get_lastErr()!=OK) { strcat(Socket[thread].outBuf," error:"); _itoa(HP.sTemp[i].get_lastErr(),Socket[thread].outBuf); }  STR_END;
                }
                else strcat(Socket[thread].outBuf," absent \r\n"); 
         }   

     sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));   
    
          
      strcpy(Socket[thread].outBuf,"\n  2.2 Аналоговые датчики\r\n");
      for(i=0;i<ANUMBER;i++)   // Информация по  аналоговым датчикм - например давление
         {   
            strcat(Socket[thread].outBuf,HP.sADC[i].get_name()); if((x=8-strlen(HP.sADC[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(Socket[thread].outBuf," "); }
            strcat(Socket[thread].outBuf,HP.sADC[i].get_note());  strcat(Socket[thread].outBuf,": "); 
            if (HP.sADC[i].get_present()) 
                { 
                  strcat(Socket[thread].outBuf," P=");    _ftoa(Socket[thread].outBuf,(float)HP.sADC[i].get_Press()/100.0,2);
                  strcat(Socket[thread].outBuf," Pmin="); _ftoa(Socket[thread].outBuf,(float)HP.sADC[i].get_minPress()/100.0,2);
                  strcat(Socket[thread].outBuf," Pmax="); _ftoa(Socket[thread].outBuf,(float)HP.sADC[i].get_maxPress()/100.0,2);
                  strcat(Socket[thread].outBuf," Ptest=");_ftoa(Socket[thread].outBuf,(float)HP.sADC[i].get_testPress()/100.0,2);
                  strcat(Socket[thread].outBuf," Zero="); _itoa(HP.sADC[i].get_zeroPress(),Socket[thread].outBuf);
                  strcat(Socket[thread].outBuf," Kof=");  _ftoa(Socket[thread].outBuf,(float)HP.sADC[i].get_transADC(),3);
                  strcat(Socket[thread].outBuf," Pin=AD");_itoa(HP.sADC[i].get_pinA(),Socket[thread].outBuf); 
                  if (HP.sADC[i].get_lastErr()!=OK ) { strcat(Socket[thread].outBuf," error:"); _itoa(HP.sADC[i].get_lastErr(),Socket[thread].outBuf); }  STR_END;
                } 
            else strcat(Socket[thread].outBuf," absent \r\n"); 
         }  
         
  
       strcat(Socket[thread].outBuf,"\n  2.3 Датчики сухой контакт\r\n");
       for(i=0;i<INUMBER;i++)  
           {
            strcat(Socket[thread].outBuf,HP.sInput[i].get_name()); if((x=8-strlen(HP.sInput[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(Socket[thread].outBuf," "); }
            strcat(Socket[thread].outBuf,HP.sInput[i].get_note());  strcat(Socket[thread].outBuf,": "); 
      
            if (HP.sInput[i].get_present()) 
                { 
                 strcat(Socket[thread].outBuf," In=");       _itoa(HP.sInput[i].get_Input(),Socket[thread].outBuf); 
                 strcat(Socket[thread].outBuf," Alarm=");    _itoa(HP.sInput[i].get_alarmInput(),Socket[thread].outBuf); 
                 strcat(Socket[thread].outBuf," Test=");     _itoa(HP.sInput[i].get_testInput(),Socket[thread].outBuf); 
                 strcat(Socket[thread].outBuf," Pin=");      _itoa(HP.sInput[i].get_pinD(),Socket[thread].outBuf);  
                 strcat(Socket[thread].outBuf," Type=");
                 switch((int)HP.sInput[i].get_typeInput())
                   {
                    case pALARM: strcat(Socket[thread].outBuf,"Alarm"); break;                // 0 Аварийный датчик, его срабатываение приводит к аварии и останове Тн
                    case pSENSOR:strcat(Socket[thread].outBuf,"Work");  break;                // 1 Обычный датчик, его значение используется в алгоритмах ТН
                    case pPULSE: strcat(Socket[thread].outBuf,"Pulse"); break;                // 2 Импульсный висит на прерывании и считает частоты - выходная величина ЧАСТОТА
                    default:     strcat(Socket[thread].outBuf,"err_type"); break;             // Ошибка??
                   }
                  if (HP.sInput[i].get_lastErr()!=OK) { strcat(Socket[thread].outBuf," error:"); _itoa(HP.sInput[i].get_lastErr(),Socket[thread].outBuf);}   STR_END;
                }
                else strcat(Socket[thread].outBuf," absent \r\n");      
           } 

     sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));   
          
        strcpy(Socket[thread].outBuf,"\n  3.1 Реле\r\n");
        for(i=0;i<RNUMBER;i++)  
           {
            strcat(Socket[thread].outBuf,HP.dRelay[i].get_name()); if((x=8-strlen(HP.dRelay[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(Socket[thread].outBuf," "); }
            strcat(Socket[thread].outBuf,HP.dRelay[i].get_note()); 
            strcat(Socket[thread].outBuf," Pin:"); _itoa(HP.dRelay[i].get_pinD(),Socket[thread].outBuf); strcat(Socket[thread].outBuf," Status: "); 
      
            if (HP.dRelay[i].get_present()) { _itoa(HP.dRelay[i].get_Relay(),Socket[thread].outBuf); STR_END;}
            else strcat(Socket[thread].outBuf," absent \r\n");      
           }       

       #ifdef EEV_DEF
       strcat(Socket[thread].outBuf,"\n  3.2 ЭРВ - ");  strcat(Socket[thread].outBuf,HP.dEEV.get_note()); STR_END;
        if (HP.dEEV.get_present()) 
         {
			 strcat(Socket[thread].outBuf,"Ноги куда присоединен ЭРВ: ");
			 strcat(Socket[thread].outBuf,"D"); _itoa(PIN_EEV1_D24,Socket[thread].outBuf);
			 strcat(Socket[thread].outBuf," D");_itoa(PIN_EEV2_D25,Socket[thread].outBuf);
			 strcat(Socket[thread].outBuf," D");_itoa(PIN_EEV3_D26,Socket[thread].outBuf);
			 strcat(Socket[thread].outBuf," D");_itoa(PIN_EEV4_D27,Socket[thread].outBuf);STR_END;

			 strcat(Socket[thread].outBuf,"Минимальное положение (шаги): ");  HP.dEEV.get_paramEEV((char*)eev_MIN, Socket[thread].outBuf);  STR_END;
			 strcat(Socket[thread].outBuf,"Полное открыте (шаги):");  HP.dEEV.get_paramEEV((char*)eev_MAX, Socket[thread].outBuf);  STR_END;
			 strcat(Socket[thread].outBuf,"Формула перегрева: ");
 			 HP.dEEV.get_ruleEEVtext(Socket[thread].outBuf); STR_END;
             strcat(Socket[thread].outBuf,"Алгоритм ПИД ЭРВ: ");
             #ifdef PID_FORMULA2
             strcat(Socket[thread].outBuf,"PID_FORMULA2");STR_END;
             #else
             strcat(Socket[thread].outBuf,"PID_FORMULA1");STR_END;
             #endif 
             strcat(Socket[thread].outBuf,"Целевой перегрев (C°): "); HP.dEEV.get_paramEEV((char*)eev_TARGET, Socket[thread].outBuf);  STR_END;
             strcat(Socket[thread].outBuf,"ПИД, период (сек): ");  HP.dEEV.get_paramEEV((char*)eev_TIME, Socket[thread].outBuf);  STR_END;
             strcat(Socket[thread].outBuf,"Пропорциональная составляющая: ");  HP.dEEV.get_paramEEV((char*)eev_KP, Socket[thread].outBuf); STR_END;
             strcat(Socket[thread].outBuf,"Интегральная составляющая: ");  HP.dEEV.get_paramEEV((char*)eev_KI, Socket[thread].outBuf); STR_END;
             strcat(Socket[thread].outBuf,"Дифференциальная составляющая: ");  HP.dEEV.get_paramEEV((char*)eev_KD, Socket[thread].outBuf); STR_END;
             strcat(Socket[thread].outBuf,"Ручное управление, открытие ЭРВ (шаги): ");  HP.dEEV.get_paramEEV((char*)eev_MANUAL, Socket[thread].outBuf); STR_END;
             strcat(Socket[thread].outBuf,"Поправка (C°): ");  HP.dEEV.get_paramEEV((char*)eev_CONST, Socket[thread].outBuf); STR_END;
             strcat(Socket[thread].outBuf,"Используемый фреон: "); STR_END;
             strcat(Socket[thread].outBuf, noteFreon[HP.dEEV.get_typeFreon()]);
             strcat(Socket[thread].outBuf,"Текущее положение (шаги): ");     _itoa(HP.dEEV.get_EEV(),Socket[thread].outBuf); STR_END;
             strcat(Socket[thread].outBuf,"Текущий перегрев (градусы): ");   _ftoa(Socket[thread].outBuf,(float)HP.dEEV.get_Overheat()/100.0,2); STR_END;
             strcat(Socket[thread].outBuf," - Корректировка перегрева -\r\n");
             strcat(Socket[thread].outBuf,"Флаг включения корректировки перегрева от разности температур конденсатора и испарителя: ");  HP.dEEV.get_paramEEV((char*)eev_cCORRECT,Socket[thread].outBuf);STR_END;
             strcat(Socket[thread].outBuf,"Задержка после старта компрессора (сек): ");  HP.dEEV.get_paramEEV((char*)eev_cDELAY,Socket[thread].outBuf);STR_END; 
             strcat(Socket[thread].outBuf,"Период в циклах ЭРВ, сколько пропустить (циклы): ");  HP.dEEV.get_paramEEV((char*)eev_cPERIOD,Socket[thread].outBuf);STR_END; 
             strcat(Socket[thread].outBuf,"Температура нагнетания - конденсации (C°): ");  HP.dEEV.get_paramEEV((char*)eev_cDELTA,Socket[thread].outBuf);STR_END; 
             strcat(Socket[thread].outBuf,"Стартовый перегрев (C°): ");  HP.dEEV.get_paramEEV((char*)eev_cOH_START,Socket[thread].outBuf);STR_END;
             strcat(Socket[thread].outBuf,"Минимальный перегрев (C°): ");  HP.dEEV.get_paramEEV((char*)eev_cOH_MIN,Socket[thread].outBuf);STR_END;
             strcat(Socket[thread].outBuf,"Максимальный перегрев (C°): ");  HP.dEEV.get_paramEEV((char*)eev_cOH_MAX,Socket[thread].outBuf);STR_END; 

             strcat(Socket[thread].outBuf," - Глобальные настройки -\r\n");
             strcat(Socket[thread].outBuf,"Ошибка при которой происходит уменьшение ПИД ЭРВ (C°): ");  HP.dEEV.get_paramEEV((char*)eev_PID2_delta,Socket[thread].outBuf);STR_END;
             strcat(Socket[thread].outBuf,"Скорость шагового двигателя ЭРВ (импульсы в сек.): ");  HP.dEEV.get_paramEEV((char*)eev_SPEED,Socket[thread].outBuf);STR_END; 
             strcat(Socket[thread].outBuf,"ПУСКОВАЯ позиция ЭРВ - то что при старте компрессора, при раскрутке (шаги): ");  HP.dEEV.get_paramEEV((char*)eev_PRE_START_POS,Socket[thread].outBuf);STR_END; 
             strcat(Socket[thread].outBuf,"СТАРТОВАЯ позиция ЭРВ после раскрутки компрессора т.е. позиция скоторой начинается работа ПИД (шаги): ");  HP.dEEV.get_paramEEV((char*)eev_START_POS,Socket[thread].outBuf);STR_END; 
             strcat(Socket[thread].outBuf,"Задержка включения ЭРВ после включения компрессора (сек.): ");  HP.dEEV.get_paramEEV((char*)eev_DELAY_ON_PID,Socket[thread].outBuf);STR_END; 
             strcat(Socket[thread].outBuf,"Время после старта компрессора когда ЭРВ выходит на стартовую позицию - облегчение пуска вначале ЭРВ (сек.): ");  HP.dEEV.get_paramEEV((char*)eev_DELAY_START_POS,Socket[thread].outBuf);STR_END; 
             strcat(Socket[thread].outBuf,"Задержка закрытия ЭРВ после выключения насосов (се.к): ");  HP.dEEV.get_paramEEV((char*)eev_DELAY_OFF,Socket[thread].outBuf);STR_END; 
             strcat(Socket[thread].outBuf,"Задержка между открытием ЭРВ и включением компрессора, для выравнивания давлений (сек.): ");  HP.dEEV.get_paramEEV((char*)eev_DELAY_ON,Socket[thread].outBuf);STR_END; 
             strcat(Socket[thread].outBuf,"Флаг удержания мотора шагового двигателя ЭРВ: ");  HP.dEEV.get_paramEEV((char*)eev_HOLD_MOTOR,Socket[thread].outBuf);STR_END; 
             strcat(Socket[thread].outBuf,"Закрытие ЭРВ при выключении компрессора: "); HP.dEEV.get_paramEEV((char*)eev_CLOSE,Socket[thread].outBuf);STR_END;
             strcat(Socket[thread].outBuf,"Всегда начинать работу ЭРВ со стратовой позиции: "); HP.dEEV.get_paramEEV((char*)eev_START,Socket[thread].outBuf);STR_END;
             strcat(Socket[thread].outBuf,"Использование спецальную позицию ЭРВ при пуске компрессора: "); HP.dEEV.get_paramEEV((char*)eev_LIGHT_START,Socket[thread].outBuf);STR_END;
         }
        #else 
            strcat(Socket[thread].outBuf,"\n  3.2 EEV absent \r\n");    
        #endif
       sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));   
        
       strcpy(Socket[thread].outBuf,"\n  3.3 Частотный преобразователь\r\n");
       if (HP.dFC.get_present()) 
         {
         strcat(Socket[thread].outBuf,"Текущая частота (гц): ");  HP.dFC.get_paramFC((char*)fc_cFC,Socket[thread].outBuf);STR_END; 
         #ifdef FC_ANALOG_CONTROL // Аналоговое управление
              strcat(Socket[thread].outBuf," - Аналоговое управление -\r\n");
              strcat(Socket[thread].outBuf,"Отсчеты ЦАП соответсвующие частоте 0 гц (отсчеты): ");  HP.dFC.get_paramFC((char*)fc_LEVEL0,Socket[thread].outBuf);STR_END; 
              strcat(Socket[thread].outBuf,"Отсчеты ЦАП соответсвующие максимальной частоте (отсчеты): ");  HP.dFC.get_paramFC((char*)fc_LEVEL100,Socket[thread].outBuf);STR_END; 
              strcat(Socket[thread].outBuf,"Частота отключения инвертора  (отсчеты): ");  HP.dFC.get_paramFC((char*)fc_LEVELOFF,Socket[thread].outBuf);STR_END; 
          #else
             strcat(Socket[thread].outBuf," - Управление идет через Modbus - \r\n"); 
             strcat(Socket[thread].outBuf,"Блокировка инвертора: ");  HP.dFC.get_paramFC((char*)fc_BLOCK,Socket[thread].outBuf);STR_END; 
             strcat(Socket[thread].outBuf,"Флаг автоматического сброса не критичной ошибки инвертора: ");  HP.dFC.get_paramFC((char*)fc_AUTO_RESET_FAULT,Socket[thread].outBuf);STR_END; 
          #endif
	       strcat(Socket[thread].outBuf,"Время обновления алгоритма пид регулятора (сек): ");  HP.dFC.get_paramFC((char*)fc_UPTIME,Socket[thread].outBuf);STR_END; 
	       strcat(Socket[thread].outBuf,"Проценты от уровня защит (мощность, ток, давление, температура) при которой происходит блокировка роста частоты (%): ");  HP.dFC.get_paramFC((char*)fc_PID_STOP,Socket[thread].outBuf);STR_END; 
	       
           strcat(Socket[thread].outBuf," - Граничные температуры - \r\n");
	       strcat(Socket[thread].outBuf,"Защита по температуре нагнетания - сколько градусов не доходит до максимальной и при этом происходит уменьшение частоты (C°): ");  HP.dFC.get_paramFC((char*)fc_DT_COMP_TEMP,Socket[thread].outBuf);STR_END; 
	       strcat(Socket[thread].outBuf,"Превышение температуры от уставок (подача) при которой срабатыват защита, уменьшается частота ((C°): ");  HP.dFC.get_paramFC((char*)fc_DT_TEMP,Socket[thread].outBuf);STR_END; 
	       strcat(Socket[thread].outBuf,"Превышение температуры от уставок (подача) при которой срабатыват защита ГВС, уменьшается частота (C°): ");  HP.dFC.get_paramFC((char*)fc_DT_TEMP_BOILER,Socket[thread].outBuf);STR_END; 
	
	       strcat(Socket[thread].outBuf," - Стартовые частоты - \r\n");
	       strcat(Socket[thread].outBuf,"Стартовая частота инвертора (гц): ");  HP.dFC.get_paramFC((char*)fc_START_FREQ,Socket[thread].outBuf);STR_END; 
	       strcat(Socket[thread].outBuf,"Стартовая частота инвертора ГВС (гц): ");  HP.dFC.get_paramFC((char*)fc_START_FREQ_BOILER,Socket[thread].outBuf);STR_END; 
	       
	       strcat(Socket[thread].outBuf," - Минимальные частоты - \r\n");
	       strcat(Socket[thread].outBuf,"Минимальная  частота инвертора (гц): ");  HP.dFC.get_paramFC((char*)fc_MIN_FREQ,Socket[thread].outBuf);STR_END; 
	       strcat(Socket[thread].outBuf,"Mинимальная  частота инвертора при охлаждении (гц): ");  HP.dFC.get_paramFC((char*)fc_MIN_FREQ_COOL,Socket[thread].outBuf);STR_END; 
	       strcat(Socket[thread].outBuf,"Минимальная  частота инвертора при нагреве ГВС (гц): ");  HP.dFC.get_paramFC((char*)fc_MIN_FREQ_BOILER,Socket[thread].outBuf);STR_END; 
	       strcat(Socket[thread].outBuf,"Минимальная  частота инвертора РУЧНОЙ РЕЖИМ (гц): ");  HP.dFC.get_paramFC((char*)fc_MIN_FREQ_USER,Socket[thread].outBuf);STR_END; 
	      
	       strcat(Socket[thread].outBuf," - Максимальные частоты - \r\n");
	       strcat(Socket[thread].outBuf,"Максимальная частота инвертора (гц): ");  HP.dFC.get_paramFC((char*)fc_MAX_FREQ,Socket[thread].outBuf);STR_END; 
	       strcat(Socket[thread].outBuf,"Максимальная частота инвертора в режиме охлаждения (гц): ");  HP.dFC.get_paramFC((char*)fc_MAX_FREQ_COOL,Socket[thread].outBuf);STR_END; 
	       strcat(Socket[thread].outBuf,"Максимальная частота инвертора в режиме ГВС (гц): ");  HP.dFC.get_paramFC((char*)fc_MAX_FREQ_BOILER,Socket[thread].outBuf);STR_END; 
	       strcat(Socket[thread].outBuf,"Максимальная частота инвертора РУЧНОЙ РЕЖИМ (гц): ");  HP.dFC.get_paramFC((char*)fc_MAX_FREQ_USER,Socket[thread].outBuf);STR_END; 
	
           strcat(Socket[thread].outBuf," - Ограничение ПИД регулятора - \r\n");	    
	       strcat(Socket[thread].outBuf,"Максимальный шаг (на увеличение) изменения частоты при ПИД регулировании (Гц): ");  HP.dFC.get_paramFC((char*)fc_PID_FREQ_STEP,Socket[thread].outBuf);STR_END; 
	       strcat(Socket[thread].outBuf,"Шаг уменьшения инвертора при достижении максимальной температуры, мощности и тока (гц): ");  HP.dFC.get_paramFC((char*)fc_STEP_FREQ,Socket[thread].outBuf);STR_END; 
	       strcat(Socket[thread].outBuf,"Шаг уменьшения инвертора при достижении максимальной температуры, мощности и тока ГВС (гц): ");  HP.dFC.get_paramFC((char*)fc_STEP_FREQ_BOILER,Socket[thread].outBuf);STR_END; 
	     } 
        else strcat(Socket[thread].outBuf,"FC absent \r\n");    
        sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));  
        
       strcpy(Socket[thread].outBuf,"\n  3.4. Электросчетчик SDM120\r\n");
         #ifdef USE_ELECTROMETER_SDM
           strcat(Socket[thread].outBuf,"MAX напряжение при контроле входного напряжения (В): ");    HP.dSDM.get_paramSDM((char*)sdm_MAX_VOLTAGE,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"MIN напряжение при контроле входного напряжения (В): ");    HP.dSDM.get_paramSDM((char*)sdm_MIN_VOLTAGE,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"MAX максимальная мощность при контроле мощности (Вт): ");   HP.dSDM.get_paramSDM((char*)sdm_MAX_POWER,Socket[thread].outBuf); STR_END;
        #else
           strcat(Socket[thread].outBuf,"SDM120 absent\r\n");    
         #endif  

        strcat(Socket[thread].outBuf,"\n  3.5. Частотные датчики потока\r\n");
        for(i=0;i<FNUMBER;i++)  
           {
            strcat(Socket[thread].outBuf,HP.sFrequency[i].get_name()); if((x=8-strlen(HP.sFrequency[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(Socket[thread].outBuf," "); }
            strcat(Socket[thread].outBuf,HP.sFrequency[i].get_note());  strcat(Socket[thread].outBuf,": "); 
            if (HP.sFrequency[i].get_present()) 
            {
             strcat(Socket[thread].outBuf," Контроль мин. потока: ");  if (HP.sFrequency[i].get_checkFlow())  strcat(Socket[thread].outBuf,(char*)cOne);else strcat(Socket[thread].outBuf,(char*)cZero);
             strcat(Socket[thread].outBuf,", Минимальный поток (куб/ч)="); _ftoa(Socket[thread].outBuf,(float)HP.sFrequency[i].get_minValue()/1000.0,3);
             strcat(Socket[thread].outBuf,", Коэффициент (имп*л)=");       _ftoa(Socket[thread].outBuf,(float)HP.sFrequency[i].get_kfValue()/100.0,3);
             strcat(Socket[thread].outBuf,", Теплоемкость, Дж/(кг*град)="); _ftoa(Socket[thread].outBuf,(float)HP.sFrequency[i].get_Capacity(),3);
             strcat(Socket[thread].outBuf,", Тест (куб/ч)="); _ftoa(Socket[thread].outBuf,(float)HP.sFrequency[i].get_testValue()/1000,3);
             
            }
            else strcat(Socket[thread].outBuf," absent"); 
            STR_END;         
           }

     sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));   

}

// Записать в клиента бинарный файл настроек
uint16_t get_binSettings(uint8_t thread)
{
	uint16_t i, len;
	byte b;
	// 1. Заголовок
    strcpy(Socket[thread].outBuf, WEB_HEADER_OK_CT);
    strcat(Socket[thread].outBuf, WEB_HEADER_BIN_ATTACH);
    strcat(Socket[thread].outBuf, "settings.bin\"\r\n\r\n");
	sendPacketRTOS(thread, (byte*)Socket[thread].outBuf, strlen(Socket[thread].outBuf), 0);
	sendConstRTOS(thread, HEADER_BIN);
	
	// 2. Запись настроек ТН
	if((len = HP.save())<= 0) return 0; // записать настройки в еепром, а потом будем их писать и получить размер записываемых данных
	for(i = 0; i < len; i++) {  // Копирование в буффер
		readEEPROM_I2C(I2C_SETTING_EEPROM + i, &b, 1);
		Socket[thread].outBuf[i] = b;
	}
	if(sendPacketRTOS(thread, (byte*) Socket[thread].outBuf, len, 0) == 0) return 0; // передать пакет, при ошибке выйти

	// 3. Запись текущего профиля
	if(HP.Prof.save(HP.Prof.get_idProfile()) <= 0) return 0;
	len = HP.Prof.get_lenProfile();
	uint32_t addr = I2C_PROFILE_EEPROM + HP.Prof.get_idProfile() * len;
	for(i = 0; i < len; i++) {
		readEEPROM_I2C(addr + i, &b, 1);
		Socket[thread].outBuf[i] = b;
		if((uint16_t)(i) > sizeof(Socket[thread].outBuf) - 1) return 0; // Слишком  много данных
	}
	if(sendPacketRTOS(thread, (byte*) Socket[thread].outBuf, len, 0) == 0) return 0; // передать пакет, при ошибке выйти
	
    // 4. Расписание
	if((uint16_t)len + HP.Schdlr.get_data_size() > sizeof(Socket[thread].outBuf)) return 0;
	len = HP.Schdlr.load((uint8_t *)Socket[thread].outBuf);
	if(len <= 0) return 0; // ошибка
	if(sendPacketRTOS(thread, (byte*) Socket[thread].outBuf, len, 0) == 0) return 0; // передать пакет, при ошибке выйти
	return len;
}

// Получить файл со графиками возвращает число отправленных байт
uint16_t get_csvChart(uint8_t thread)
{ 
	int16_t i,j;
	uint32_t sum=0,s=0;

	// заголовок
	strcpy(Socket[thread].outBuf, WEB_HEADER_OK_CT);
	strcat(Socket[thread].outBuf, WEB_HEADER_TEXT_ATTACH);
	strcat(Socket[thread].outBuf, "chart.csv\"\r\n\r\n");
	sendPacketRTOS(thread, (byte*)Socket[thread].outBuf, strlen(Socket[thread].outBuf), 0);
	strcpy(Socket[thread].outBuf,"Point;");
	//    strcpy(Socket[thread].outBuf,""); _itoa(HP.sTemp[TIN].Chart.get_num(),Socket[thread].outBuf);strcat(Socket[thread].outBuf,";");// вывести число точек
	for(i=0;i<TNUMBER;i++) if(HP.sTemp[i].Chart.get_present())      {strcat(Socket[thread].outBuf,HP.sTemp[i].get_name()); strcat(Socket[thread].outBuf,";");}
#ifndef MIN_RAM_CHARTS
	for(i=0;i<ANUMBER;i++)
#else
	for(i=PCON+1;i<ANUMBER;i++)
#endif
		if(HP.sADC[i].Chart.get_present())        { strcat(Socket[thread].outBuf,HP.sADC[i].get_name()); strcat(Socket[thread].outBuf,";");}
	for(i=0;i<FNUMBER;i++) if(HP.sFrequency[i].Chart.get_present()) { strcat(Socket[thread].outBuf,HP.sFrequency[i].get_name()); strcat(Socket[thread].outBuf,";");}
#ifdef EEV_DEF
	if(HP.dEEV.Chart.get_present())     strcat(Socket[thread].outBuf,"posEEV;");
	if(HP.ChartOVERHEAT.get_present())  strcat(Socket[thread].outBuf,"OverHeat;");
	if(HP.ChartOVERHEAT2.get_present())  strcat(Socket[thread].outBuf,"OverHeat2;");
	if(HP.ChartTPEVA.get_present())     strcat(Socket[thread].outBuf,"T[PEVA];");
	if(HP.ChartTPCON.get_present())     strcat(Socket[thread].outBuf,"T[PCON];");
#endif
	if(HP.dFC.ChartFC.get_present())       strcat(Socket[thread].outBuf,"FrequencyFC;");
	if(HP.dFC.ChartPower.get_present())    strcat(Socket[thread].outBuf,"PowerFC;");
#ifndef MIN_RAM_CHARTS
	if(HP.dFC.ChartCurrent.get_present())     strcat(Socket[thread].outBuf,"CurrentFC;");
#endif
	if(HP.ChartRCOMP.get_present())     strcat(Socket[thread].outBuf,"RCOMP;");

	if ((HP.sTemp[TCONOUTG].Chart.get_present())&&(HP.sTemp[TCONING].Chart.get_present())) strcat(Socket[thread].outBuf,"dCO;");
	if ((HP.sTemp[TEVAING].Chart.get_present())&&(HP.sTemp[TEVAOUTG].Chart.get_present())) strcat(Socket[thread].outBuf,"dGEO;");


#ifdef FLOWCON
	if((HP.sTemp[TCONOUTG].Chart.get_present())&&(HP.sTemp[TCONING].Chart.get_present())) strcat(Socket[thread].outBuf,"PowerCO;");
#endif
#ifdef FLOWEVA
	if((HP.sTemp[TEVAOUTG].Chart.get_present())&&(HP.sTemp[TEVAING].Chart.get_present())) strcat(Socket[thread].outBuf,"PowerGEO;");
#endif

	if(HP.ChartCOP.get_present())       strcat(Socket[thread].outBuf,"COP;");

#ifdef USE_ELECTROMETER_SDM
  #ifndef MIN_RAM_CHARTS
	if(HP.dSDM.ChartVoltage.get_present())    strcat(Socket[thread].outBuf,"VOLTAGE;");
	if(HP.dSDM.ChartCurrent.get_present())    strcat(Socket[thread].outBuf,"CURRENT;");
  #endif
	//    if(HP.dSDM.sAcPower.get_present())    strcat(Socket[thread].outBuf,"acPOWER;");
	//    if(HP.dSDM.sRePower.get_present())    strcat(Socket[thread].outBuf,"rePOWER;");
	if(HP.dSDM.ChartPower.get_present())      strcat(Socket[thread].outBuf,"fullPOWER;");
	//    if(HP.dSDM.ChartPowerFactor.get_present())strcat(Socket[thread].outBuf,"kPOWER;");
	if(HP.ChartFullCOP.get_present())      strcat(Socket[thread].outBuf,"fullCOP;");
#endif

	STR_END;
	s=strlen(Socket[thread].outBuf);
	sum=s;

	if(sendPacketRTOS(thread,(byte*)Socket[thread].outBuf,s,0)==0) return 0 ;                          // передать пакет, при ошибке выйти
	for(i=0;i<HP.sTemp[TIN].Chart.get_num();i++)  // По всем точкам датичк TIN всегда существует
	{
		//сформировать одну строку
		strcpy(Socket[thread].outBuf,"#"); _itoa(i+1,Socket[thread].outBuf); strcat(Socket[thread].outBuf,";");
		for(j=0;j<TNUMBER;j++)  if(HP.sTemp[j].Chart.get_present())   { _ftoa(Socket[thread].outBuf,(float)HP.sTemp[j].Chart.get_Point(i)/100,2); strcat(Socket[thread].outBuf,";"); }
		for(j=0;j<ANUMBER;j++)  if(HP.sADC[j].Chart.get_present())  { _ftoa(Socket[thread].outBuf,(float)HP.sADC[j].Chart.get_Point(i)/100.0,2); strcat(Socket[thread].outBuf,";"); }
		for(j=0;j<FNUMBER;j++)  if(HP.sFrequency[j].Chart.get_present())  { _ftoa(Socket[thread].outBuf,(float)HP.sFrequency[j].Chart.get_Point(i)/1000.0,3); strcat(Socket[thread].outBuf,";"); }
#ifdef EEV_DEF
		if(HP.dEEV.Chart.get_present())    { _itoa(HP.dEEV.Chart.get_Point(i),Socket[thread].outBuf); strcat(Socket[thread].outBuf,";"); }
		if(HP.ChartOVERHEAT.get_present()) { _ftoa(Socket[thread].outBuf,(float)HP.ChartOVERHEAT.get_Point(i)/100.0,2); strcat(Socket[thread].outBuf,";"); }
		if(HP.ChartOVERHEAT2.get_present()) { _ftoa(Socket[thread].outBuf,(float)HP.ChartOVERHEAT2.get_Point(i)/100.0,2); strcat(Socket[thread].outBuf,";"); }
		if(HP.ChartTPEVA.get_present())    { _ftoa(Socket[thread].outBuf,(float)HP.ChartTPEVA.get_Point(i)/100.0,2); strcat(Socket[thread].outBuf,";"); }
		if(HP.ChartTPCON.get_present())    { _ftoa(Socket[thread].outBuf,(float)HP.ChartTPCON.get_Point(i)/100.0,2); strcat(Socket[thread].outBuf,";"); }
#endif
		if(HP.dFC.ChartFC.get_present())       { _itoa(HP.dFC.ChartFC.get_Point(i),Socket[thread].outBuf); strcat(Socket[thread].outBuf,";"); }
		if(HP.dFC.ChartPower.get_present())    { _itoa(HP.dFC.ChartPower.get_Point(i),Socket[thread].outBuf); strcat(Socket[thread].outBuf,";"); }
#ifndef MIN_RAM_CHARTS
		if(HP.dFC.ChartCurrent.get_present())  { _itoa(HP.dFC.ChartCurrent.get_Point(i),Socket[thread].outBuf); strcat(Socket[thread].outBuf,";"); }
#endif
		if(HP.ChartRCOMP.get_present())    { _itoa(HP.ChartRCOMP.get_Point(i),Socket[thread].outBuf); strcat(Socket[thread].outBuf,";"); }
		if ((HP.sTemp[TCONOUTG].Chart.get_present())&&(HP.sTemp[TCONING].Chart.get_present())) // считаем на лету экономим оперативку
		{_ftoa(Socket[thread].outBuf,((float)HP.sTemp[TCONOUTG].Chart.get_Point(i)-(float)HP.sTemp[TCONING].Chart.get_Point(i))/100, 2); strcat(Socket[thread].outBuf,(char*)";"); }
		if ((HP.sTemp[TEVAING].Chart.get_present())&&(HP.sTemp[TEVAOUTG].Chart.get_present())) // считаем на лету экономим оперативку
		{_ftoa(Socket[thread].outBuf,((float)HP.sTemp[TEVAOUTG].Chart.get_Point(i)-(float)HP.sTemp[TCONING].Chart.get_Point(i))/100, 2); strcat(Socket[thread].outBuf,(char*)";"); }

#ifdef FLOWCON // Мощности расчет на лету
		if((HP.sTemp[TCONOUTG].Chart.get_present())&&(HP.sTemp[TCONING].Chart.get_present()))
		{_ftoa(Socket[thread].outBuf,float(abs(HP.sTemp[TCONOUTG].Chart.get_Point(i)-HP.sTemp[TCONING].Chart.get_Point(i))*HP.sFrequency[FLOWCON].Chart.get_Point(i))/HP.sFrequency[FLOWCON].get_kfCapacity()/1000, 2); strcat(Socket[thread].outBuf,(char*)";");}
#endif
#ifdef FLOWEVA
		if((HP.sTemp[TEVAOUTG].Chart.get_present())&&(HP.sTemp[TEVAING].Chart.get_present()))
		{_ftoa(Socket[thread].outBuf,float(abs(HP.sTemp[TEVAOUTG].Chart.get_Point(i)-HP.sTemp[TEVAING].Chart.get_Point(i))*HP.sFrequency[FLOWEVA].Chart.get_Point(i))/HP.sFrequency[FLOWEVA].get_kfCapacity()/1000, 2); strcat(Socket[thread].outBuf,(char*)";");}
#endif


		if(HP.ChartCOP.get_present())   { _ftoa(Socket[thread].outBuf,(float)HP.ChartCOP.get_Point(i)/100.0,2); strcat(Socket[thread].outBuf,";"); }

#ifdef USE_ELECTROMETER_SDM
	#ifndef MIN_RAM_CHARTS
		if(HP.dSDM.ChartVoltage.get_present())     { _ftoa(Socket[thread].outBuf,(float)HP.dSDM.ChartVoltage.get_Point(i)/100.0,2); strcat(Socket[thread].outBuf,";"); }
		if(HP.dSDM.ChartCurrent.get_present())     { _ftoa(Socket[thread].outBuf,(float)HP.dSDM.ChartCurrent.get_Point(i)/100.0,2); strcat(Socket[thread].outBuf,";"); }
	#endif
		//   if(HP.dSDM.sAcPower.get_present())     { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.dSDM.sAcPower.get_Point(i),2)); strcat(Socket[thread].outBuf,";"); }
		//   if(HP.dSDM.sRePower.get_present())     { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.dSDM.sRePower.get_Point(i),2)); strcat(Socket[thread].outBuf,";"); }
		if(HP.dSDM.ChartPower.get_present())       { _ftoa(Socket[thread].outBuf,(float)HP.dSDM.ChartPower.get_Point(i),2); strcat(Socket[thread].outBuf,";"); }
		//   if(HP.dSDM.ChartPowerFactor.get_present()) { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.dSDM.ChartPowerFactor.get_Point(i)/100.0,2)); strcat(Socket[thread].outBuf,";"); }
		if(HP.ChartFullCOP.get_present())       { _ftoa(Socket[thread].outBuf,(float)HP.ChartFullCOP.get_Point(i)/100.0,2); strcat(Socket[thread].outBuf,";"); }
#endif

		STR_END;
		s=strlen(Socket[thread].outBuf);
		if(sendPacketRTOS(thread,(byte*)Socket[thread].outBuf,s,0)==0) return 0 ;                          // передать пакет, при ошибке выйти
		sum=sum+s;

	}  // for
	return sum;
}

/*
// файл статистики на карте отсутсвует 
void noCsvStatistic(uint8_t thread)
{
   get_Header(thread,(char*)FILE_STATISTIC);
   sendPrintfRTOS(thread, "Файл статистики за выбранный год не найден на карте памяти.\r\n");
}
*/ 
   
// Получить индексный файл при отсутвии SD карты
// выдача массива index_noSD в index_noSD
int16_t get_indexNoSD(uint8_t thread)
{
    uint16_t n,i=0;
     #ifdef LOG 
       journal.jprintf("$Read: indexNoSD from memory\n");  
     #endif
   	strcpy(Socket[thread].outBuf, WEB_HEADER_OK_CT);
   	strcat(Socket[thread].outBuf, WEB_HEADER_TXT_KEEP);
   	strcat(Socket[thread].outBuf, WEB_HEADER_END);
   	sendPacketRTOS(thread, (byte*)Socket[thread].outBuf, strlen(Socket[thread].outBuf), 0);
    n=sizeof(index_noSD);              // сколько надо передать байт
    while (n>0)                        // Пока есть не отправленные данные
      {
       if(n>=W5200_MAX_LEN)
           {
            memcpy(Socket[thread].outBuf,index_noSD+i,W5200_MAX_LEN);
            if(sendPacketRTOS(thread,(byte*)Socket[thread].outBuf,W5200_MAX_LEN,0)==0) return 0 ;                      // не последний пакет
            i=i+W5200_MAX_LEN;n=n-W5200_MAX_LEN;
           }
       else 
           {
            memcpy(Socket[thread].outBuf,(index_noSD+i),n);
            if(sendPacketRTOS(thread,(byte*)Socket[thread].outBuf,n,0)==0) return 0; else return sizeof(index_noSD);  // последний пакет
           }
     } // while (n>0)
  return n;
}   

// Выдать файл журнала в виде файла
void get_txtJournal(uint8_t thread)
{
   get_Header(thread,(char*)"journal.txt");
   sendPrintfRTOS(thread, "  Системный журнал (размер %d из %d байт)\r\n", journal.available(),JOURNAL_LEN);
   journal.send_Data(thread);
}

// Сгенерировать тестовый файл
#define SIZE_TEST 2*1024*1024  // размер теста в байтах, реально передается округление до целого числа sizeof(Socket[thread].outBuf)
void get_datTest(uint8_t thread)
{
   uint16_t j;
   unsigned long startTick;
   // Заголовок
   strcpy(Socket[thread].outBuf, WEB_HEADER_OK_CT);
   strcat(Socket[thread].outBuf, "application/x-binary\r\nContent-Length:");
   _itoa((SIZE_TEST/sizeof(Socket[thread].outBuf))*sizeof(Socket[thread].outBuf),Socket[thread].outBuf);  //реальный размер передачи
   strcat(Socket[thread].outBuf,"\r\nContent-Disposition: attachment; filename=\"test.dat\"\r\n\r\n");
   sendPacketRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf),0);
   
   // Генерация файла используется выходной буфер
   memset(Socket[thread].outBuf,0x55,sizeof(Socket[thread].outBuf));   // Заполнение буфера
   startTick=millis();                                      // Запомнить время старта
   for(j=0;j<SIZE_TEST/sizeof(Socket[thread].outBuf);j++)  
   {
    if (sendBufferRTOS(thread,(byte*)(Socket[thread].outBuf),sizeof(Socket[thread].outBuf))==0) break;
   }
   startTick=millis()-startTick;
   journal.jprintf("Download test.dat speed:%d bytes/sec\n",SIZE_TEST*1000/startTick);

}

// Получить состояние теплового насоса для отсылки по почте
// посылается в клиента используя tempBuf
void get_mailState(EthernetClient client,char *tempBuf)
{
uint8_t i,j;
int16_t x; 

       strcpy(tempBuf,"\r\n -----  С О С Т О Я Н И Е  -----");
       strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
      
       strcpy(tempBuf,"\n  1. Тепловой насос");
       strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
    
       strcpy(tempBuf,"СостояниеТН: "); strcat(tempBuf,HP.StateToStr());
       strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));  
      
       strcpy(tempBuf,"Последняя ошибка: ");  _itoa(HP.get_errcode(),tempBuf); strcat(tempBuf," - "); strcat(tempBuf,HP.get_lastErr());
       strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
   
       strcpy(tempBuf,"Режим работы: ");strcat(tempBuf,HP.TestToStr());
       strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf)); 
           
      strcpy(tempBuf,"Последняя перезагрузка: "); DecodeTimeDate(HP.get_startDT(),tempBuf);
      strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));  
     
      strcpy(tempBuf,"Время с последней перезагрузки: "); TimeIntervalToStr(HP.get_uptime(),tempBuf);
      strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));  
     
      strcpy(tempBuf,"Причина последней перезагрузки: "); strcat(tempBuf,ResetCause());
      strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));  

      strcpy(tempBuf,"\n  2. Датчики температуры");
      strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
      for(i=0;i<TNUMBER;i++)   // Информация по  датчикам температуры
         {
            if (HP.sTemp[i].get_present())  // только присутсвующие датчики
            {
              strcpy(tempBuf,HP.sTemp[i].get_name()); if((x=8-strlen(HP.sTemp[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(tempBuf," "); }
              strcat(tempBuf,"["); strcat(tempBuf,addressToHex(HP.sTemp[i].get_address())); strcat(tempBuf,"] "); 
              strcat(tempBuf,HP.sTemp[i].get_note());  strcat(tempBuf,": "); 
              _ftoa(tempBuf,(float)HP.sTemp[i].get_Temp()/100.0,2);
              if (HP.sTemp[i].get_lastErr()!=OK) { strcat(tempBuf," error:"); _itoa(HP.sTemp[i].get_lastErr(),tempBuf); } 
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
            }  //   if (HP.sTemp[i].get_present())
         }
      strcpy(tempBuf,"\n  3. Аналоговые датчики");
      strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
      for(i=0;i<ANUMBER;i++)   // Информация по  аналоговым датчикам
         {   
           if (HP.sADC[i].get_present()) // только присутсвующие датчики
            {          
            strcpy(tempBuf,HP.sADC[i].get_name()); if((x=8-strlen(HP.sADC[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(tempBuf," "); }
            strcat(tempBuf,HP.sADC[i].get_note());  strcat(tempBuf,": "); 
            _ftoa(tempBuf,(float)HP.sADC[i].get_Press()/100.0,2); 
            if (HP.sADC[i].get_lastErr()!=OK ) { strcat(tempBuf," error:"); _itoa(HP.sADC[i].get_lastErr(),tempBuf); }
            strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
            }
         }
   
       strcpy(tempBuf,"\n  4. Датчики 'сухой контакт'");
       strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
       for(i=0;i<INUMBER;i++)  
           {
           if (HP.sInput[i].get_present()) // только присутсвующие датчики
              { 
              strcpy(tempBuf,HP.sInput[i].get_name()); if((x=8-strlen(HP.sInput[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(tempBuf," "); }
              strcat(tempBuf,HP.sInput[i].get_note());  strcat(tempBuf,": "); 
              _itoa(HP.sInput[i].get_Input(),tempBuf); 
              strcat(tempBuf," alarm:"); _itoa(HP.sInput[i].get_alarmInput(),tempBuf); 
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
              }     
           }

       strcpy(tempBuf,"\n  5. Реле");
       strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf)); 
       for(i=0;i<RNUMBER;i++)  
           {
            if (HP.dRelay[i].get_present()) // только присутсвующие датчики
              {
              strcpy(tempBuf,HP.dRelay[i].get_name()); if((x=8-strlen(HP.dRelay[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(tempBuf," "); }
              strcat(tempBuf,HP.dRelay[i].get_note());  strcat(tempBuf,": "); 
              _itoa(HP.dRelay[i].get_Relay(),tempBuf); 
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf)); 
              }         
           }
       
         #ifdef EEV_DEF
			strcpy(tempBuf,"\n  6. ЭРВ - ");  strcat(tempBuf,HP.dEEV.get_note());  strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
			strcpy(tempBuf,"Минимальное положение (шаги): ");  _itoa(HP.dEEV.get_minEEV(),tempBuf);
			strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
			strcpy(tempBuf,"Полное открыте (шаги):");  _itoa(HP.dEEV.get_maxEEV(),tempBuf);
			strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
			strcpy(tempBuf,"Правило управления ЭРВ: ");
			HP.dEEV.get_ruleEEVtext(tempBuf);
			strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
			strcpy(tempBuf,"Текущее положение (шаги): ");  _itoa(HP.dEEV.get_EEV(),tempBuf);
			strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
			strcpy(tempBuf,"Текущий перегрев (градусы): ");  _ftoa(tempBuf,(float)HP.dEEV.get_Overheat()/100.0,2);
			strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
        #else 
            strcpy(tempBuf,"EEV absent");    strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
        #endif
         
       strcpy(tempBuf,"\n  7. Инвертор ");strcat(tempBuf,HP.dFC.get_name());
       strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));   
       if (HP.dFC.get_present()) 
         {    strcpy(tempBuf,"Текущее состояние инвертора: "); _itoa(HP.dFC.read_stateFC(),tempBuf);
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf)); 
              strcpy(tempBuf,"Целевая частота [Гц]: ");    _ftoa(tempBuf,(float)HP.dFC.get_target()/100.0,2);
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));  
              strcpy(tempBuf,"Текущая частота [Гц]: ");    _ftoa(tempBuf,(float)HP.dFC.get_frequency()/100.0,2);
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));  
              strcpy(tempBuf,"Текущая мощность [кВт]: ");  _ftoa(tempBuf,(float)HP.dFC.get_power()/1000.0,3);
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
              strcpy(tempBuf,"Tемпература радиатора [°С]: ");  _ftoa(tempBuf,(float)HP.dFC.read_tempFC()/10.0,2);
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
#ifdef FC_ANALOG_CONTROL // Аналоговое управление
              strcpy(tempBuf,"ЦАП дискреты: ");            _itoa(HP.dFC.get_DAC(),tempBuf);
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));  
#endif              
        } 
        else {strcpy(tempBuf,"FC absent");  strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));}   

       strcpy(tempBuf,"\n  8. Электросчетчик SDM120"); strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
         #ifdef USE_ELECTROMETER_SDM
           strcpy(tempBuf,"Текущее входное напряжение [В]: ");                          HP.dSDM.get_paramSDM((char*)sdm_VOLTAGE,tempBuf);strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
           strcpy(tempBuf,"Текущий потребляемый ток ТН [А]: ");                         HP.dSDM.get_paramSDM((char*)sdm_CURRENT,tempBuf);strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
           strcpy(tempBuf,"Текущая потребляемая реактивная мощность ТН [Вт]: ");        HP.dSDM.get_paramSDM((char*)sdm_REPOWER,tempBuf);strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
           strcpy(tempBuf,"Текущая потребляемая активная мощность ТН [Вт]: ");          HP.dSDM.get_paramSDM((char*)sdm_ACPOWER,tempBuf);strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
           strcpy(tempBuf,"Текущая потребляемая суммараная мощность ТН [Вт]: ");        HP.dSDM.get_paramSDM((char*)sdm_POWER,tempBuf); strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
           strcpy(tempBuf,"Коэффициент мощности: ");                                    HP.dSDM.get_paramSDM((char*)sdm_POW_FACTOR,tempBuf);strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
           strcpy(tempBuf,"Угол фазы (градусы): ");                                     HP.dSDM.get_paramSDM((char*)sdm_PHASE,tempBuf); strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
           strcpy(tempBuf,"Суммараная активная энергия [кВт*ч]: ");                     HP.dSDM.get_paramSDM((char*)sdm_ACENERGY,tempBuf); strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
           strcpy(tempBuf,"Cостояние связи со счетчиком: ");                            HP.dSDM.get_paramSDM((char*)sdm_LINK,tempBuf);strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
         #else
           strcpy(tempBuf,"SDM120 absent");
           strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));     
         #endif  

        strcpy(tempBuf,"\n  9. Частотные датчики потока");
        strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf)); 
        for(i=0;i<FNUMBER;i++)  
           {
              if (HP.sFrequency[i].get_present())
                {
                strcpy(tempBuf,HP.sFrequency[i].get_name()); if((x=8-strlen(HP.sFrequency[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(tempBuf," "); }
                strcat(tempBuf,HP.sFrequency[i].get_note());  strcat(tempBuf,": "); 
                _ftoa(tempBuf,(float)HP.sFrequency[i].get_Value()/1000.0,3); 
                strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));    
                }      
           }        
       
}

