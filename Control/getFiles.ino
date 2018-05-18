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
// Герерация различных файлов для выгрузки
#include "HeatPump.h"
#include "NoSDpage.h"
#define STR_END   strcat(Socket[thread].outBuf,"\r\n")      // Макрос на конец строки
extern uint16_t sendPacketRTOS(uint8_t thread, const uint8_t * buf, uint16_t len,uint16_t pause);

// Генерация заголовка
void get_Header(uint8_t thread,char *name_file)
{
  // journal.jprintf("$Generate file %s\n",name_file);
   sendPrintfRTOS(thread, "HTTP/1.1 200 OK\r\nContent-Type:text/plain\r\nContent-Disposition: attachment; filename=\"%s\"\r\nAccess-Control-Allow-Origin: *\r\n\r\n", name_file);
   sendPrintfRTOS(thread, " ------ Народный контроллер теплового насоса ver. %s  ------\r\nКонфигурация: %s: %s\r\nСоздание файла: %s %s \r\n\r\n", VERSION,CONFIG_NAME,CONFIG_NOTE,NowTimeToStr(),NowDateToStr());
}

// Получить состояние теплового насоса
// header - заголовок файла ставить или нет
void get_txtState(uint8_t thread, boolean header)
{   uint8_t i,j; 
    int16_t x; 
     if (header) get_Header(thread,(char*)"state.txt");   // Если нужен заголовок
     sendPrintfRTOS(thread, "\n  1. Тепловой насос\r\nСостояниеТН: %s\r\nПоследняя ошибка: %d - %s\r\n", HP.StateToStr(),HP.get_errcode(),HP.get_lastErr());
     
     // Состояние ТН
      sendPrintfRTOS(thread, "Режим работы: %s\r\nПоследняя перезагрузка: %s\r\nВремя с последней перезагрузки: %s\r\nПричина последней перезагрузки: %s%s\r\n",\
      HP.TestToStr(),DecodeTimeDate(HP.get_startDT()),TimeIntervalToStr(HP.get_uptime()),ResetCause());
       
     strcpy(Socket[thread].outBuf,"\n  2. Датчики температуры\r\n");
     for(i=0;i<TNUMBER;i++)   // Информация по  датчикам температуры
         {
              strcat(Socket[thread].outBuf,HP.sTemp[i].get_name()); if((x=8-strlen(HP.sTemp[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(Socket[thread].outBuf," "); }
              strcat(Socket[thread].outBuf,"["); strcat(Socket[thread].outBuf,addressToHex(HP.sTemp[i].get_address())); strcat(Socket[thread].outBuf,"] "); 
              strcat(Socket[thread].outBuf,HP.sTemp[i].get_note());  strcat(Socket[thread].outBuf,": "); 
              if (HP.sTemp[i].get_present())
                {
                  ftoa(Socket[thread].outBuf + strlen(Socket[thread].outBuf),(float)HP.sTemp[i].get_Temp()/100.0,2);
                  if (HP.sTemp[i].get_lastErr()!=OK) { strcat(Socket[thread].outBuf," error:"); strcat(Socket[thread].outBuf,int2str(HP.sTemp[i].get_lastErr())); } 
                  STR_END; 
                }
                else strcat(Socket[thread].outBuf," absent\r\n"); 
         }
      if (HP.get_mode()==pCOOL) strcat(Socket[thread].outBuf,"Внимание! ТН в режиме охлаждения, изменены назначения датчиков TEVAIN->TCONOUT TEVAOUT->TCONIN TCONIN->TEVAOUT TCONOUT->TEVAIN");
      sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));
      
      strcpy(Socket[thread].outBuf,"\n  3. Аналоговые датчики\r\n"); // Новый пакет
      for(i=0;i<ANUMBER;i++)   // Информация по  датчикам температуры
         {   
            strcat(Socket[thread].outBuf,HP.sADC[i].get_name()); if((x=8-strlen(HP.sADC[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(Socket[thread].outBuf," "); }
            strcat(Socket[thread].outBuf,HP.sADC[i].get_note());  strcat(Socket[thread].outBuf,": "); 
            if (HP.sADC[i].get_present()) 
                { 
                  ftoa(Socket[thread].outBuf + strlen(Socket[thread].outBuf),(float)HP.sADC[i].get_Press()/100.0,2); if (HP.sADC[i].get_lastErr()!=OK ) { strcat(Socket[thread].outBuf," error:"); strcat(Socket[thread].outBuf,int2str(HP.sADC[i].get_lastErr())); }
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
                  strcat(Socket[thread].outBuf,int2str(HP.sInput[i].get_Input())); 
                  strcat(Socket[thread].outBuf," alarm:"); strcat(Socket[thread].outBuf,int2str(HP.sInput[i].get_alarmInput())); STR_END;   
                }
                else strcat(Socket[thread].outBuf," absent\r\n");      
           }
       
              
       strcat(Socket[thread].outBuf,"\n  5. Реле\r\n");
       for(i=0;i<RNUMBER;i++)  
           {
            strcat(Socket[thread].outBuf,HP.dRelay[i].get_name()); if((x=8-strlen(HP.dRelay[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(Socket[thread].outBuf," "); }
            strcat(Socket[thread].outBuf,HP.dRelay[i].get_note());  strcat(Socket[thread].outBuf,": "); 
      
            if (HP.dRelay[i].get_present()) strcat(Socket[thread].outBuf,int2str(HP.dRelay[i].get_Relay())); 
            else strcat(Socket[thread].outBuf," absent"); 
            STR_END;         
           }
       #ifdef EEV_DEF
        strcat(Socket[thread].outBuf,"\n  6. ЭРВ - ");  strcat(Socket[thread].outBuf,HP.dEEV.get_note()); STR_END;
        if (HP.dEEV.get_present()) 
         {
              strcat(Socket[thread].outBuf,"Минимальное положение (шаги): ");  strcat(Socket[thread].outBuf,int2str(HP.dEEV.get_minEEV())); STR_END;
              strcat(Socket[thread].outBuf,"Полное открыте (шаги):");  strcat(Socket[thread].outBuf,int2str(HP.dEEV.get_maxEEV()));STR_END;
              strcat(Socket[thread].outBuf,"Правило управления ЭРВ: ");
              switch ((int)HP.dEEV.get_ruleEEV())
                   {
                    case TEVAOUT_TEVAIN:    strcat(Socket[thread].outBuf,"TEVAOUT_TEVAIN\r\n");    break;
                    case TRTOOUT_TEVAIN:    strcat(Socket[thread].outBuf,"TRTOOUT_TEVAIN\r\n");    break;
                    case TEVAOUT_PEVA:      strcat(Socket[thread].outBuf,"TEVAOUT_PEVA\r\n");      break;
                    case TRTOOUT_PEVA:      strcat(Socket[thread].outBuf,"TRTOOUT_PEVA\r\n");      break;
                    case TABLE:             strcat(Socket[thread].outBuf,"TABLE\r\n");             break;
                    case MANUAL:            strcat(Socket[thread].outBuf,"MANUAL\r\n");            break;
                    default:                strcat(Socket[thread].outBuf,"error\r\n");             break;    
                   }         
             strcat(Socket[thread].outBuf,"Текущее положение (шаги): ");  strcat(Socket[thread].outBuf,int2str(HP.dEEV.get_EEV())); STR_END;
       //      strcat(Socket[thread].outBuf,"Формула перегрева: ");
       //      if (HP.sADC[PEVA].get_present()) strcat(Socket[thread].outBuf,"TEVAOUT-T[PEVA]\r\n"); else strcat(Socket[thread].outBuf,"TEVAOUT-TEVAIN\r\n"); 
             strcat(Socket[thread].outBuf,"Текущий перегрев (градусы): ");  ftoa(Socket[thread].outBuf + strlen(Socket[thread].outBuf),(float)HP.dEEV.get_Overheat()/100.0,2); STR_END;
         }  
        #else
        strcat(Socket[thread].outBuf,"\n  6. EEV absent\r\n");    
        #endif
         
       strcat(Socket[thread].outBuf,"\n  7. Частотный преобразователь\r\n");
       if (HP.dFC.get_present()) 
         {
              strcat(Socket[thread].outBuf,"Целевая частота [Гц]: ");    ftoa(Socket[thread].outBuf + strlen(Socket[thread].outBuf),(float)HP.dFC.get_targetFreq()/100.0,2); STR_END;
              strcat(Socket[thread].outBuf,"Текущая частота [Гц]: ");    ftoa(Socket[thread].outBuf + strlen(Socket[thread].outBuf),(float)HP.dFC.get_freqFC()/100.0,2); STR_END;
              strcat(Socket[thread].outBuf,"Текущая мощность [кВт]: ");  ftoa(Socket[thread].outBuf + strlen(Socket[thread].outBuf),(float)HP.dFC.get_power()/10.0,1); STR_END;
              strcat(Socket[thread].outBuf,"ЦАП дискреты: ");            strcat(Socket[thread].outBuf,int2str(HP.dFC.get_DAC())); STR_END;
         } 
        else strcat(Socket[thread].outBuf,"FC absent\r\n");    

        strcat(Socket[thread].outBuf,"\n  8. Электросчетчик SDM120\r\n");
         #ifdef USE_ELECTROMETER_SDM
  //         strcat(Socket[thread].outBuf,"MAX напряжение при контроле входного напряжения [В]: ");   HP.dSDM.get_paramSDM((char*)sdm_MAX_VOLTAGE,Socket[thread].outBuf); STR_END;
  //         strcat(Socket[thread].outBuf,"MIN напряжение при контроле входного напряжения [В]: ");   HP.dSDM.get_paramSDM((char*)sdm_MIN_VOLTAGE,Socket[thread].outBuf); STR_END;
  //         strcat(Socket[thread].outBuf,"MIN максимальаня мощность при контроле мощности [Вт]: ");  HP.dSDM.get_paramSDM((char*)sdm_MAX_POWER,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Текущее входное напряжение [В]: ");                          HP.dSDM.get_paramSDM((char*)sdm_VOLTAGE,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Текущий потребляемый ток ТН [А]: ");                         HP.dSDM.get_paramSDM((char*)sdm_CURRENT,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Текущая потребляемая реактивная мощность ТН [Вт]: ");        HP.dSDM.get_paramSDM((char*)sdm_REPOWER,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Текущая потребляемая активная мощность ТН [Вт]: ");          HP.dSDM.get_paramSDM((char*)sdm_ACPOWER,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Текущая потребляемая суммараная мощность ТН [Вт]: ");        HP.dSDM.get_paramSDM((char*)sdm_POWER,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Коэффициент мощности: ");                                    HP.dSDM.get_paramSDM((char*)sdm_POW_FACTOR,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Угол фазы (градусы): ");                                     HP.dSDM.get_paramSDM((char*)sdm_PHASE,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Суммараная активная энергия [кВт/ч]: ");                     HP.dSDM.get_paramSDM((char*)sdm_ACENERGY,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Суммараная реактивная энергия [кВт/ч]: ");                   HP.dSDM.get_paramSDM((char*)sdm_REENERGY,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Суммараная потребленная энергия [кВт/ч]: ");                 HP.dSDM.get_paramSDM((char*)sdm_ENERGY,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"Cостояние связи со счетчиком: ");                            HP.dSDM.get_paramSDM((char*)sdm_LINK,Socket[thread].outBuf); STR_END;
         #else
           strcat(Socket[thread].outBuf,"SDM120 absent\r\n");    
         #endif  
   
   strcat(Socket[thread].outBuf,"\n  9. Частотные датчики потока\r\n");
     for(i=0;i<FNUMBER;i++)  
           {
            strcat(Socket[thread].outBuf,HP.sFrequency[i].get_name()); if((x=8-strlen(HP.sFrequency[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(Socket[thread].outBuf," "); }
            strcat(Socket[thread].outBuf,HP.sFrequency[i].get_note());  strcat(Socket[thread].outBuf,": "); 
            if (HP.sFrequency[i].get_present()) ftoa(Socket[thread].outBuf + strlen(Socket[thread].outBuf),(float)HP.sFrequency[i].get_Value()/1000.0,3);
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
     char temp[10];
       
     get_Header(thread,(char*)"settings.txt");
     strcpy(Socket[thread].outBuf,"\n  1. Общие настройки\r\n");
     strcat(Socket[thread].outBuf,"Режим работы :");
     switch ((MODE_HP)HP.get_mode())   // режим работы отопления
                   {
                    case pOFF:  strcat(Socket[thread].outBuf,"Выключено\r\n"); break;
                    case pHEAT: strcat(Socket[thread].outBuf,"Отопление\r\n"); break;
                    case pCOOL: strcat(Socket[thread].outBuf,"Охлаждение\r\n"); break;
                    default:    strcat(Socket[thread].outBuf,"Выключено\r\n"); break;
                   }  
     strcat(Socket[thread].outBuf,"Бойлер:"); HP.Prof.get_boiler((char*)boil_BOILER_ON,Socket[thread].outBuf); STR_END;
 
     strcat(Socket[thread].outBuf,"\n  1.1 Отопление\r\n");
     strcat(Socket[thread].outBuf,"Алгоритм работы отопления: "); strcat(Socket[thread].outBuf,HP.Prof.get_paramHeatHP(pRULE,HP.dFC.get_present())); STR_END;
     strcat(Socket[thread].outBuf,"Целевая температура дома: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramHeatHP(pTEMP1,HP.dFC.get_present()));STR_END;
     strcat(Socket[thread].outBuf,"Целевая температура обратки:");strcat(Socket[thread].outBuf,HP.Prof.get_paramHeatHP(pTEMP2,HP.dFC.get_present()));STR_END;
     strcat(Socket[thread].outBuf,"Что является целью: ");  strcat(Socket[thread].outBuf,HP.Prof.get_paramHeatHP(pTARGET,HP.dFC.get_present()));STR_END;
     strcat(Socket[thread].outBuf,"Гистерезис целевой температуры: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramHeatHP(pDTEMP,HP.dFC.get_present()));STR_END;
     if (HP.get_ruleHeat()!=pHYSTERESIS)
         {
         strcat(Socket[thread].outBuf,"Целевая температура подачи ПИД ТН: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramHeatHP(pTEMP_PID,HP.dFC.get_present()));STR_END;
         strcat(Socket[thread].outBuf,"Постоянная интегрирования времени в секундах ПИД ТН: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramHeatHP(pHP_TIME,HP.dFC.get_present()));STR_END;
         strcat(Socket[thread].outBuf,"Пропорциональная составляющая ПИД ТН: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramHeatHP(pHP_PRO,HP.dFC.get_present()));STR_END;
         strcat(Socket[thread].outBuf,"Интегральная составляющая ПИД ТН: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramHeatHP(pHP_IN,HP.dFC.get_present()));STR_END;
         strcat(Socket[thread].outBuf,"Дифференциальная составляющая ПИД ТН: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramHeatHP(pHP_DIF,HP.dFC.get_present()));STR_END;
         strcat(Socket[thread].outBuf,"Использование погодозависимости: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramHeatHP(pWEATHER,HP.dFC.get_present()));STR_END;
         strcat(Socket[thread].outBuf,"Коэффициент погодозависимости: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramHeatHP(pK_WEATHER,HP.dFC.get_present()));STR_END;
         }
     strcat(Socket[thread].outBuf," Защиты\r\n");
     strcat(Socket[thread].outBuf,"Tемпература подачи максимальная: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramHeatHP(pTEMP_IN,HP.dFC.get_present()));STR_END;
     strcat(Socket[thread].outBuf,"Tемпература обратки минимальная: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramHeatHP(pTEMP_OUT,HP.dFC.get_present()));STR_END;
     strcat(Socket[thread].outBuf,"Минимальное время простоя компрессора в минутах: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramHeatHP(pPAUSE,HP.dFC.get_present()));STR_END;
     strcat(Socket[thread].outBuf,"Максимальная разность температур конденсатора: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramHeatHP(pD_TEMP,HP.dFC.get_present()));STR_END;
 
     strcat(Socket[thread].outBuf,"\n  1.2 Охлаждение\r\n");
     strcat(Socket[thread].outBuf,"Алгоритм работы отопления: "); strcat(Socket[thread].outBuf,HP.Prof.get_paramCoolHP(pRULE,HP.dFC.get_present()));STR_END;
     strcat(Socket[thread].outBuf,"Целевая температура дома: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramCoolHP(pTEMP1,HP.dFC.get_present()));STR_END;
     strcat(Socket[thread].outBuf,"Целевая температура обратки:");strcat(Socket[thread].outBuf,HP.Prof.get_paramCoolHP(pTEMP2,HP.dFC.get_present()));STR_END;
     strcat(Socket[thread].outBuf,"Что является целью: ");  strcat(Socket[thread].outBuf,HP.Prof.get_paramCoolHP(pTARGET,HP.dFC.get_present()));STR_END;
     strcat(Socket[thread].outBuf,"Гистерезис целевой температуры: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramCoolHP(pDTEMP,HP.dFC.get_present()));STR_END;
     if (HP.get_ruleCool()!=pHYSTERESIS)
         {
         strcat(Socket[thread].outBuf,"Целевая температура подачи ПИД ТН: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramCoolHP(pTEMP_PID,HP.dFC.get_present()));STR_END;
         strcat(Socket[thread].outBuf,"Постоянная интегрирования времени в секундах ПИД ТН: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramCoolHP(pHP_TIME,HP.dFC.get_present()));STR_END;
         strcat(Socket[thread].outBuf,"Пропорциональная составляющая ПИД ТН: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramCoolHP(pHP_PRO,HP.dFC.get_present()));STR_END;
         strcat(Socket[thread].outBuf,"Интегральная составляющая ПИД ТН: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramCoolHP(pHP_IN,HP.dFC.get_present()));STR_END;
         strcat(Socket[thread].outBuf,"Дифференциальная составляющая ПИД ТН: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramCoolHP(pHP_DIF,HP.dFC.get_present()));STR_END;
         strcat(Socket[thread].outBuf,"Использование погодозависимости: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramCoolHP(pWEATHER,HP.dFC.get_present()));STR_END;
         strcat(Socket[thread].outBuf,"Коэффициент погодозависимости: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramCoolHP(pK_WEATHER,HP.dFC.get_present()));STR_END;
         }
     strcat(Socket[thread].outBuf," Защиты\r\n");
     strcat(Socket[thread].outBuf,"Tемпература подачи максимальная: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramCoolHP(pTEMP_IN,HP.dFC.get_present()));STR_END;
     strcat(Socket[thread].outBuf,"Tемпература обратки минимальная: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramCoolHP(pTEMP_OUT,HP.dFC.get_present()));STR_END;
     strcat(Socket[thread].outBuf,"Минимальное время простоя компрессора в минутах: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramCoolHP(pPAUSE,HP.dFC.get_present()));STR_END;
     strcat(Socket[thread].outBuf,"Максимальная разность температур конденсатора: ");strcat(Socket[thread].outBuf,HP.Prof.get_paramCoolHP(pD_TEMP,HP.dFC.get_present()));STR_END;

     sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));   
     
     strcpy(Socket[thread].outBuf,"\n  1.3 ГВС\r\n");
     strcat(Socket[thread].outBuf,"Целевая температура бойлера: ");HP.Prof.get_boiler((char*)boil_TEMP_TARGET,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Гистерезис целевой температуры: ");HP.Prof.get_boiler((char*)boil_DTARGET,Socket[thread].outBuf);STR_END;
     
     strcat(Socket[thread].outBuf," Встроенный ТЭН\r\n");
     strcat(Socket[thread].outBuf,"Ускоренный нагрев ГВС (одновременное использование ТЭНа и ТН для нагрева): ");HP.Prof.get_boiler((char*)boil_TURBO_BOILER,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Использование ТЭНа для догрева бойлера до высоких температур: ");HP.Prof.get_boiler((char*)boil_ADD_HEATING,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Значение температуры для включения догрева бойлера: "); HP.Prof.get_boiler((char*)boil_TEMP_RBOILER,Socket[thread].outBuf) ;STR_END;
     
     if (HP.dFC.get_present()) // Только для инвертора
         {
         strcat(Socket[thread].outBuf," ПИД\r\n");
         strcat(Socket[thread].outBuf,"Целевая температура подачи: ");HP.Prof.get_boiler((char*)boil_BOIL_TEMP,Socket[thread].outBuf);STR_END;
         strcat(Socket[thread].outBuf,"Постоянная интегрирования времени (секунды): ");HP.Prof.get_boiler((char*)boil_BOIL_TIME,Socket[thread].outBuf);STR_END;
         strcat(Socket[thread].outBuf,"Пропорциональная составляющая: ");HP.Prof.get_boiler((char*)boil_BOIL_PRO,Socket[thread].outBuf);STR_END;
         strcat(Socket[thread].outBuf,"Интегральная составляющая: ");HP.Prof.get_boiler((char*)boil_BOIL_IN,Socket[thread].outBuf);STR_END;
         strcat(Socket[thread].outBuf,"Дифференциальная составляющая: ");HP.Prof.get_boiler((char*)boil_BOIL_DIF,Socket[thread].outBuf);STR_END;
         }
 
     strcat(Socket[thread].outBuf," Опции\r\n");
     strcat(Socket[thread].outBuf,"Использование расписания: ");HP.Prof.get_boiler((char*)boil_SCHEDULER_ON,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Борьба с сальмонелой: ");HP.Prof.get_boiler((char*)boil_SALLMONELA,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Управления циркуляционным насосом ГВС: ");HP.Prof.get_boiler((char*)boil_CIRCULATION,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Время работы насоса ГВС минуты: ");HP.Prof.get_boiler((char*)boil_CIRCUL_WORK,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Пауза в работе насоса ГВС минуты: ");HP.Prof.get_boiler((char*)boil_CIRCUL_PAUSE,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"При нагреве бойлера сбрасывать ""избыточное"" тепло систему отопления: ");HP.Prof.get_boiler((char*)boil_RESET_HEAT,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Время сброса тепла (минуты): ");HP.Prof.get_boiler((char*)boil_RESET_TIME,Socket[thread].outBuf);  STR_END;
     
     strcat(Socket[thread].outBuf," Защиты при работе теплового насоса\r\n");
     strcat(Socket[thread].outBuf,"Tемпература подачи максимальная: ");HP.Prof.get_boiler((char*)boil_TEMP_MAX,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Минимальное время простоя компрессора в минутах: ");HP.Prof.get_boiler((char*)boil_PAUSE1,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf," Расписание работы\r\n");
     HP.Prof.get_boiler((char*)boil_SCHEDULER,Socket[thread].outBuf);STR_END;
     
     strcat(Socket[thread].outBuf,"\n  1.4 Опции ТН\r\n");
     strcat(Socket[thread].outBuf,"Сохранение состояния ТН в ЕЕПРОМ, для восстановления его после сброса: "); if(!strcmp(HP.get_optionHP(pSAVE_ON),cOne))  strcat(Socket[thread].outBuf,cYes); else  strcat(Socket[thread].outBuf,cNo);STR_END;
     strcat(Socket[thread].outBuf,"Период накопления графиков (список): "); strcat(Socket[thread].outBuf,HP.get_optionHP(pTIME_CHART));STR_END;
     strcat(Socket[thread].outBuf,"Запись графиков на карту памяти: "); if(!strcmp(HP.get_optionHP(pSD_CARD),cOne))  strcat(Socket[thread].outBuf,cYes); else  strcat(Socket[thread].outBuf,cNo);STR_END;
     strcat(Socket[thread].outBuf,"Время работы конденсатора  насоса при выключенном компрессоре МИНУТЫ: "); strcat(Socket[thread].outBuf,HP.get_optionHP(pPUMP_PAUSE));STR_END;
     strcat(Socket[thread].outBuf,"Время паузы конденсатора насоса при выключенном компрессоре МИНУТЫ: "); strcat(Socket[thread].outBuf,HP.get_optionHP(pPUMP_WORK));STR_END;
     strcat(Socket[thread].outBuf,"Число попыток пуска компрессора :"); strcat(Socket[thread].outBuf,HP.get_optionHP(pATTEMPT));STR_END;
     strcat(Socket[thread].outBuf,"Использование дополнительного ТЭНа отопления: "); if(!strcmp(HP.get_optionHP(pADD_HEAT),cOne))  strcat(Socket[thread].outBuf,cYes); else  strcat(Socket[thread].outBuf,cNo);STR_END;
     strcat(Socket[thread].outBuf,"Значение температуры для управления дополнительным ТЭНом: "); strcat(Socket[thread].outBuf,HP.get_optionHP(pTEMP_RHEAT)) ;STR_END;
     strcat(Socket[thread].outBuf,"Закрытие ЭРВ при выключении компрессора: "); if(!strcmp(HP.get_optionHP(pEEV_CLOSE),cOne))  strcat(Socket[thread].outBuf,cYes); else  strcat(Socket[thread].outBuf,cNo);STR_END;
     strcat(Socket[thread].outBuf,"Всегда начинать работу ЭРВ со стратовой позиции: "); if(!strcmp(HP.get_optionHP(pEEV_START),cOne))  strcat(Socket[thread].outBuf,cYes); else  strcat(Socket[thread].outBuf,cNo);STR_END;
     strcat(Socket[thread].outBuf,"Использование спецальную позицию ЭРВ при пуске компрессора: "); if(!strcmp(HP.get_optionHP(pEEV_LIGHT_START),cOne))  strcat(Socket[thread].outBuf,cYes); else  strcat(Socket[thread].outBuf,cNo);STR_END;
     strcat(Socket[thread].outBuf,"Использование звука: "); if(!strcmp(HP.get_optionHP(pBEEP),cOne))  strcat(Socket[thread].outBuf,cYes); else  strcat(Socket[thread].outBuf,cNo);STR_END;
     strcat(Socket[thread].outBuf,"Использование Nextion дисплея: "); if(!strcmp(HP.get_optionHP(pNEXTION),cOne))  strcat(Socket[thread].outBuf,cYes); else  strcat(Socket[thread].outBuf,cNo);STR_END;
     strcat(Socket[thread].outBuf,"Время засыпания дисплея Nextion (минуты): "); strcat(Socket[thread].outBuf,HP.get_optionHP(pNEXT_SLEEP));STR_END;
     strcat(Socket[thread].outBuf,"Яркость дисплея Nextion в %: "); strcat(Socket[thread].outBuf,HP.get_optionHP(pNEXT_DIM));STR_END;

     sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));  
    
     strcpy(Socket[thread].outBuf,"\n  1.5 Сетевые настройки\r\n");
     strcat(Socket[thread].outBuf,"Использование DHCP: "); HP.get_network((char*)net_DHSP,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"IP адрес контроллера: "); HP.get_network((char*)net_IP,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"DNS сервер: "); HP.get_network((char*)net_SDNS,Socket[thread].outBuf); STR_END;
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

     
     strcat(Socket[thread].outBuf,"\n  1.6 Настройки даты и времени\r\n");
     strcat(Socket[thread].outBuf,"Установленная дата: "); HP.get_datetime((char*)time_DATE,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Установленное время: "); HP.get_datetime((char*)time_TIME,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Адрес NTP сервера: "); HP.get_datetime((char*)time_NTP,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Часовой пояс (часы): "); HP.get_datetime((char*)time_TIMEZONE,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Синхронизация времени по NTP раз в сутки: "); HP.get_datetime((char*)time_UPDATE,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Синхронизация раз в час с I2C часами: "); HP.get_datetime((char*)time_UPDATE_I2C,Socket[thread].outBuf);STR_END;
  
     sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));  
      
     strcpy(Socket[thread].outBuf,"\n  1.7 Уведомления\r\n");
     strcat(Socket[thread].outBuf,"Сброс контроллера: "); HP.message.get_messageSetting((char*)mess_MESS_RESET,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Возникновение ошибок: "); HP.message.get_messageSetting((char*)mess_MESS_ERROR,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Сигнал «жизни» (ежедневно в 12-00): "); HP.message.get_messageSetting((char*)mess_MESS_LIFE,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Достижение граничной температуры: "); HP.message.get_messageSetting((char*)mess_MESS_TEMP,Socket[thread].outBuf);  STR_END;
     strcat(Socket[thread].outBuf," Граничная температура в доме (если меньше то посылается уведомление): "); HP.message.get_messageSetting((char*)mess_MESS_TIN,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf," Граничная температура бойлера (если меньше то посылается уведомление): "); HP.message.get_messageSetting((char*)mess_MESS_TBOILER,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf," Граничная температура компрессора (если больше то посылается уведомление): "); HP.message.get_messageSetting((char*)mess_MESS_TCOMP,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Проблемы с SD картой: "); HP.message.get_messageSetting((char*)mess_MESS_SD,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Прочие уведомления: "); HP.message.get_messageSetting((char*)mess_MESS_WARNING,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf," Настройка отправки почты\r\n");
     strcat(Socket[thread].outBuf,"Посылать уведомления по почте: "); HP.message.get_messageSetting((char*)mess_MAIL,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Адрес smtp сервера: "); HP.message.get_messageSetting((char*)mess_SMTP_SERVER,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Порт smtp сервера: "); HP.message.get_messageSetting((char*)mess_SMTP_PORT,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Необходимость авторизации на сервере: ");HP.message.get_messageSetting((char*)mess_MAIL_AUTH,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Логин для входа: "); HP.message.get_messageSetting((char*)mess_SMTP_LOGIN,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Пароль для входа: "); HP.message.get_messageSetting((char*)mess_SMTP_PASS,Socket[thread].outBuf); STR_END;
     strcat(Socket[thread].outBuf,"Адрес отправителя: "); HP.message.get_messageSetting((char*)mess_SMTP_MAILTO,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Адрес получателя: "); HP.message.get_messageSetting((char*)mess_SMTP_RCPTTO,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Добавлять в уведомление информацию о состоянии ТН: "); HP.message.get_messageSetting((char*)mess_MAIL_INFO,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf," Настройка отправки SMS\r\n");
     strcat(Socket[thread].outBuf,"Посылать уведомления по SMS: "); HP.message.get_messageSetting((char*)mess_SMS,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Телефон получателя: "); HP.message.get_messageSetting((char*)mess_SMS_PHONE,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Сервис отправки SMS: ");HP.message.get_messageSetting((char*)mess_SMS_SERVICE,Socket[thread].outBuf);STR_END;
     strcat(Socket[thread].outBuf,"Адрес сервиса отправки SMS: ");HP.message.get_messageSetting((char*)mess_SMS_IP,Socket[thread].outBuf);STR_END;
     HP.message.get_messageSetting((char*)mess_SMS_NAMEP1,Socket[thread].outBuf);strcat(Socket[thread].outBuf,": ");HP.message.get_messageSetting((char*)mess_SMS_P1,Socket[thread].outBuf);STR_END;
     HP.message.get_messageSetting((char*)mess_SMS_NAMEP2,Socket[thread].outBuf);strcat(Socket[thread].outBuf,": ");HP.message.get_messageSetting((char*)mess_SMS_P2,Socket[thread].outBuf);STR_END;
     sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf)); 
      
      // MQTT
     strcpy(Socket[thread].outBuf,"\n  1.8 Настройка MQTT\r\n");
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
     strcat(Socket[thread].outBuf," Сервис <Народный мониторинг>\r\n");
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
                  strcat(Socket[thread].outBuf," T=");      strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.sTemp[i].get_Temp()/100.0,2));
                  strcat(Socket[thread].outBuf,", Tmin=");  strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.sTemp[i].get_minTemp()/100.0,2));
                  strcat(Socket[thread].outBuf,", Tmax=");  strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.sTemp[i].get_maxTemp()/100.0,2));
                  strcat(Socket[thread].outBuf,", Terr=");  strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.sTemp[i].get_errTemp()/100.0,2));
                  strcat(Socket[thread].outBuf,", Ttest="); strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.sTemp[i].get_testTemp()/100.0,2));
                  if (HP.sTemp[i].get_lastErr()!=OK) { strcat(Socket[thread].outBuf," error:"); strcat(Socket[thread].outBuf,int2str(HP.sTemp[i].get_lastErr())); }  STR_END;
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
                  strcat(Socket[thread].outBuf," P=");    strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.sADC[i].get_Press()/100.0,2));
                  strcat(Socket[thread].outBuf," Pmin="); strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.sADC[i].get_minPress()/100.0,2));
                  strcat(Socket[thread].outBuf," Pmax="); strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.sADC[i].get_maxPress()/100.0,2));
                  strcat(Socket[thread].outBuf," Ptest=");strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.sADC[i].get_testPress()/100.0,2));
                  strcat(Socket[thread].outBuf," Zero="); strcat(Socket[thread].outBuf,int2str(HP.sADC[i].get_zeroPress()));
                  strcat(Socket[thread].outBuf," Kof=");  strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.sADC[i].get_transADC(),3));
                  strcat(Socket[thread].outBuf," Pin=AD");strcat(Socket[thread].outBuf,int2str(HP.sADC[i].get_pinA())); 
                  if (HP.sADC[i].get_lastErr()!=OK ) { strcat(Socket[thread].outBuf," error:"); strcat(Socket[thread].outBuf,int2str(HP.sADC[i].get_lastErr())); }  STR_END;
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
                 strcat(Socket[thread].outBuf," In=");       strcat(Socket[thread].outBuf,int2str(HP.sInput[i].get_Input())); 
                 strcat(Socket[thread].outBuf," Alarm=");    strcat(Socket[thread].outBuf,int2str(HP.sInput[i].get_alarmInput())); 
                 strcat(Socket[thread].outBuf," Test=");     strcat(Socket[thread].outBuf,int2str(HP.sInput[i].get_testInput())); 
                 strcat(Socket[thread].outBuf," Pin=");      strcat(Socket[thread].outBuf,int2str(HP.sInput[i].get_pinD()));  
                 strcat(Socket[thread].outBuf," Type=");
                 switch((int)HP.sInput[i].get_typeInput())
                   {
                    case pALARM: strcat(Socket[thread].outBuf,"Alarm"); break;                // 0 Аварийный датчик, его срабатываение приводит к аварии и останове Тн
                    case pSENSOR:strcat(Socket[thread].outBuf,"Work");  break;                // 1 Обычный датчик, его значение используется в алгоритмах ТН
                    case pPULSE: strcat(Socket[thread].outBuf,"Pulse"); break;                // 2 Импульсный висит на прерывании и считает частоты - выходная величина ЧАСТОТА
                    default:     strcat(Socket[thread].outBuf,"err_type"); break;             // Ошибка??
                   }
                  if (HP.sInput[i].get_lastErr()!=OK) { strcat(Socket[thread].outBuf," error:"); strcat(Socket[thread].outBuf,int2str(HP.sInput[i].get_lastErr()));}   STR_END;
                }
                else strcat(Socket[thread].outBuf," absent \r\n");      
           } 

     sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));   
          
        strcpy(Socket[thread].outBuf,"\n  3.1 Реле\r\n");
        for(i=0;i<RNUMBER;i++)  
           {
            strcat(Socket[thread].outBuf,HP.dRelay[i].get_name()); if((x=8-strlen(HP.dRelay[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(Socket[thread].outBuf," "); }
            strcat(Socket[thread].outBuf,HP.dRelay[i].get_note()); 
            strcat(Socket[thread].outBuf," Pin:"); strcat(Socket[thread].outBuf,int2str(HP.dRelay[i].get_pinD())); strcat(Socket[thread].outBuf," Status: "); 
      
            if (HP.dRelay[i].get_present()) { strcat(Socket[thread].outBuf,int2str(HP.dRelay[i].get_Relay())); STR_END;}
            else strcat(Socket[thread].outBuf," absent \r\n");      
           }       

       #ifdef EEV_DEF
       strcat(Socket[thread].outBuf,"\n  3.2 ЭРВ - ");  strcat(Socket[thread].outBuf,HP.dEEV.get_note()); STR_END;
        if (HP.dEEV.get_present()) 
         {
              strcat(Socket[thread].outBuf,"Ноги куда присоединен ЭРВ: ");
              strcat(Socket[thread].outBuf,"D"); strcat(Socket[thread].outBuf,int2str(PIN_EEV1_D24));
              strcat(Socket[thread].outBuf," D");strcat(Socket[thread].outBuf,int2str(PIN_EEV2_D25));
              strcat(Socket[thread].outBuf," D");strcat(Socket[thread].outBuf,int2str(PIN_EEV3_D26));
              strcat(Socket[thread].outBuf," D");strcat(Socket[thread].outBuf,int2str(PIN_EEV4_D27));STR_END;
                  
              strcat(Socket[thread].outBuf,"Минимальное положение (шаги): ");  strcat(Socket[thread].outBuf,int2str(HP.dEEV.get_minEEV())); STR_END;
              strcat(Socket[thread].outBuf,"Полное открыте (шаги):");  strcat(Socket[thread].outBuf,int2str(HP.dEEV.get_maxEEV())); STR_END;
              strcat(Socket[thread].outBuf,"Формула перегрева: ");
              switch ((int)HP.dEEV.get_ruleEEV())
                   {
                    case TEVAOUT_TEVAIN:    strcat(Socket[thread].outBuf,"TEVAOUT_TEVAIN\r\n");    break;
                    case TRTOOUT_TEVAIN:    strcat(Socket[thread].outBuf,"TRTOOUT_TEVAIN\r\n");    break;
                    case TEVAOUT_PEVA:      strcat(Socket[thread].outBuf,"TEVAOUT_PEVA\r\n");      break;
                    case TRTOOUT_PEVA:      strcat(Socket[thread].outBuf,"TRTOOUT_PEVA\r\n");      break;
                    case TABLE:             strcat(Socket[thread].outBuf,"TABLE\r\n");             break;
                    case MANUAL:            strcat(Socket[thread].outBuf,"MANUAL\r\n");            break;
                    default:                strcat(Socket[thread].outBuf,"Error type rule EEV\r\n");break;    
                   }   
             strcat(Socket[thread].outBuf,"Целевой перегрев: ");  strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.dEEV.get_tOverheat()/100.0,2));   STR_END;
             strcat(Socket[thread].outBuf,"Постоянная интегрирования времени (сек): ");  strcat(Socket[thread].outBuf,int2str(HP.dEEV.get_timeIn()));  STR_END;
             strcat(Socket[thread].outBuf,"Пропорциональная составляющая: ");  strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.dEEV.get_Kpro()/100.0,2));STR_END;
             strcat(Socket[thread].outBuf,"Интегральная составляющая: ");  strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.dEEV.get_Kint()/10.0,1)); STR_END;
             strcat(Socket[thread].outBuf,"Дифференциальная составляющая: ");  strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.dEEV.get_Kdif()/10.0,1)); STR_END;
             strcat(Socket[thread].outBuf,"Ручное управление, открытие ЭРВ (шаги): ");  strcat(Socket[thread].outBuf,int2str(HP.dEEV.get_manualStep()));STR_END;
             strcat(Socket[thread].outBuf,"Поправка (градусы): ");  strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.dEEV.get_Correction()/100.0,1));  STR_END;
             strcat(Socket[thread].outBuf,"Используемый фреон: ");
             switch ((int)HP.dEEV.get_typeFreon())
                   {
                    case R22:               strcat(Socket[thread].outBuf,"R22\r\n");               break;
                    case R410A:             strcat(Socket[thread].outBuf,"R410A\r\n");             break;
                    case R600A:             strcat(Socket[thread].outBuf,"R600A\r\n");             break;
                    case R134A:             strcat(Socket[thread].outBuf,"R134A\r\n");             break;
                    case R407C:             strcat(Socket[thread].outBuf,"R407C\r\n");             break;
                    case R12:               strcat(Socket[thread].outBuf,"R12\r\n");               break;
                    case R290:              strcat(Socket[thread].outBuf,"R290\r\n");              break;
                    case R404A:             strcat(Socket[thread].outBuf,"R404A\r\n");             break;
                    case R717:              strcat(Socket[thread].outBuf,"R717\r\n");              break;
                    default:                strcat(Socket[thread].outBuf,"error\r\n");             break;    
                   }    
                        
             strcat(Socket[thread].outBuf,"Текущее положение (шаги): ");     strcat(Socket[thread].outBuf,int2str(HP.dEEV.get_EEV())); STR_END;
             strcat(Socket[thread].outBuf,"Текущий перегрев (градусы): ");   strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.dEEV.get_Overheat()/100.0,2));
             STR_END;
         }
        #else 
            strcat(Socket[thread].outBuf,"\n  3.2 EEV absent \r\n");    
        #endif

       strcat(Socket[thread].outBuf,"\n  3.3 Частотный преобразователь\r\n");
       if (HP.dFC.get_present()) 
         {
              strcat(Socket[thread].outBuf,"Текущее положение %: ");  strcat(Socket[thread].outBuf,int2str(HP.dFC.get_targetFreq())); STR_END;
              strcat(Socket[thread].outBuf,"ЦАП дискреты: ");         strcat(Socket[thread].outBuf,int2str(HP.dFC.get_DAC())); STR_END;
            
         } 
        else strcat(Socket[thread].outBuf,"FC absent \r\n");    

       strcat(Socket[thread].outBuf,"\n  8. Электросчетчик SDM120\r\n");
         #ifdef USE_ELECTROMETER_SDM
           strcat(Socket[thread].outBuf,"MAX напряжение при контроле входного напряжения [В]: ");    HP.dSDM.get_paramSDM((char*)sdm_MAX_VOLTAGE,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"MIN напряжение при контроле входного напряжения [В]: ");    HP.dSDM.get_paramSDM((char*)sdm_MIN_VOLTAGE,Socket[thread].outBuf); STR_END;
           strcat(Socket[thread].outBuf,"MAX максимальная мощность при контроле мощности [Вт]: ");   HP.dSDM.get_paramSDM((char*)sdm_MAX_POWER,Socket[thread].outBuf); STR_END;
        #else
           strcat(Socket[thread].outBuf,"SDM120 absent\r\n");    
         #endif  

        strcat(Socket[thread].outBuf,"\n  9. Частотные датчики потока\r\n");
        for(i=0;i<FNUMBER;i++)  
           {
            strcat(Socket[thread].outBuf,HP.sFrequency[i].get_name()); if((x=8-strlen(HP.sFrequency[i].get_name()))>0) { for(j=0;j<x;j++)  strcat(Socket[thread].outBuf," "); }
            strcat(Socket[thread].outBuf,HP.sFrequency[i].get_note());  strcat(Socket[thread].outBuf,": "); 
            if (HP.sFrequency[i].get_present()) 
            {
             strcat(Socket[thread].outBuf," Минимальный поток"); strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.sFrequency[i].get_minValue()/1000.0,3));
             strcat(Socket[thread].outBuf," Коэффициент");       strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.sFrequency[i].get_kfValue(),3));
            }
            else strcat(Socket[thread].outBuf," absent"); 
            STR_END;         
           }

     sendBufferRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf));   

}

// Записать в клиента бинарный файл настроек
uint16_t get_binSettings(uint8_t thread)
{
	int16_t i, j, len;
	byte b;
	len = HP.save();   // записать настройки в еепром, а потом будем их писать и получить размер записываемых данных
	sendConstRTOS(thread, "HTTP/1.1 200 OK\r\nContent-Type:application/x-binary\r\nContent-Disposition: attachment; filename=\"settings.bin\"\r\n\r\n");
	sendConstRTOS(thread, HEADER_BIN);
	if(len <= 0) return 0;
	// запись настроек ТН
	for(i = 0; i < HP.headerEEPROM.len; i++) {
		readEEPROM_I2C(I2C_SETTING_EEPROM + i, &b, 1);
		Socket[thread].outBuf[i] = b;
	}
	// запись текущего профиля
	uint32_t addr = I2C_PROFILE_EEPROM + HP.Prof.get_idProfile() * HP.Prof.get_lenProfile();
	for(j = 0; j < HP.Prof.get_lenProfile(); j++) {
		readEEPROM_I2C(addr + j, &b, 1);
		Socket[thread].outBuf[i + j] = b;
		if((uint16_t)(i + j) > sizeof(Socket[thread].outBuf) - 1) break; // Слишком  много данных
	}
#ifdef USE_SCHEDULER
	if((uint16_t)(i + j) + HP.Schdlr.get_data_size() <= sizeof(Socket[thread].outBuf)) {
		i = HP.Schdlr.load((uint8_t *)Socket[thread].outBuf + i + j);
		if(i <= 0) return 0; // ошибка
		len += i;
	}
#endif
	if(sendPacketRTOS(thread, (byte*) Socket[thread].outBuf, len, 0) == 0) return 0; // передать пакет, при ошибке выйти
	return len;
}

// Получить файл со графиками возвращает число отправленных байт
uint16_t get_csvChart(uint8_t thread)
{ 
   int16_t i,j; 
   char temp[10];
   uint32_t sum=0,s=0;

   // заголовок
   sendConstRTOS(thread,"HTTP/1.1 200 OK\r\nContent-Type:text/plain\r\nContent-Disposition: attachment; filename=\"chart.csv\"\r\n\r\n");
   //     strcpy(Socket[thread].outBuf,"time;");
     strcpy(Socket[thread].outBuf,int2str(HP.sTemp[TIN].Chart.get_num()));strcat(Socket[thread].outBuf,";");
   //  strcpy(Socket[thread].outBuf,int2str(HP.dEEV.Chart.get_num()));strcat(Socket[thread].outBuf,";");
     for(i=0;i<TNUMBER;i++) if(HP.sTemp[i].Chart.get_present())      {strcat(Socket[thread].outBuf,HP.sTemp[i].get_name()); strcat(Socket[thread].outBuf,";");}
     for(i=0;i<ANUMBER;i++)if(HP.sADC[i].Chart.get_present())        { strcat(Socket[thread].outBuf,HP.sADC[i].get_name()); strcat(Socket[thread].outBuf,";");} 
     for(i=0;i<FNUMBER;i++) if(HP.sFrequency[i].Chart.get_present()) { strcat(Socket[thread].outBuf,HP.sFrequency[i].get_name()); strcat(Socket[thread].outBuf,";");}  
     #ifdef EEV_DEF
     if(HP.dEEV.Chart.get_present())     strcat(Socket[thread].outBuf,"posEEV;");
     if(HP.ChartOVERHEAT.get_present())  strcat(Socket[thread].outBuf,"OVERHEAT;");
     if(HP.ChartTPEVA.get_present())     strcat(Socket[thread].outBuf,"T[PEVA];");
     if(HP.ChartTPCON.get_present())     strcat(Socket[thread].outBuf,"T[PCON];");  
     #endif
     if(HP.dFC.ChartFC.get_present())       strcat(Socket[thread].outBuf,"FrequencyFC;");
     if(HP.dFC.ChartPower.get_present())    strcat(Socket[thread].outBuf,"PowerFC;");
     if(HP.dFC.ChartCurrent.get_present())     strcat(Socket[thread].outBuf,"CurrentFC;");
           
     if(HP.ChartRCOMP.get_present())     strcat(Socket[thread].outBuf,"RCOMP;");
     
     if ((HP.sTemp[TCONOUTG].Chart.get_present())&&(HP.sTemp[TCONING].Chart.get_present())) strcat(Socket[thread].outBuf,"dCO;");
     if ((HP.sTemp[TEVAING].Chart.get_present())&&(HP.sTemp[TEVAOUTG].Chart.get_present())) strcat(Socket[thread].outBuf,"dGEO;");
     
     if(HP.ChartPowerCO.get_present())   strcat(Socket[thread].outBuf,"PowerCO;");
     if(HP.ChartPowerGEO.get_present())  strcat(Socket[thread].outBuf,"PowerGEO;");
     if(HP.ChartCOP.get_present())       strcat(Socket[thread].outBuf,"COP;"); 

     #ifdef USE_ELECTROMETER_SDM 
     if(HP.dSDM.ChartVoltage.get_present())    strcat(Socket[thread].outBuf,"VOLTAGE;");
     if(HP.dSDM.ChartCurrent.get_present())    strcat(Socket[thread].outBuf,"CURRENT;");
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

   for(i=0;i<HP.ChartRCOMP.get_num();i++)  // По всем точкам
   { 
      //сформировать одну строку
      strcpy(Socket[thread].outBuf,"#"); strcat(Socket[thread].outBuf,int2str(i+1)); strcat(Socket[thread].outBuf,";");
      for(j=0;j<TNUMBER;j++)  if(HP.sTemp[j].Chart.get_present())   { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.sTemp[j].Chart.get_Point(i)/100,2)); strcat(Socket[thread].outBuf,";"); } 
      for(j=0;j<ANUMBER;j++)  if(HP.sADC[j].Chart.get_present())  { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.sADC[j].Chart.get_Point(i)/100.0,2)); strcat(Socket[thread].outBuf,";"); }
      for(j=0;j<FNUMBER;j++)  if(HP.sFrequency[j].Chart.get_present())  { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.sFrequency[j].Chart.get_Point(i)/1000.0,3)); strcat(Socket[thread].outBuf,";"); }
      #ifdef EEV_DEF
      if(HP.dEEV.Chart.get_present())    { strcat(Socket[thread].outBuf,int2str(HP.dEEV.Chart.get_Point(i))); strcat(Socket[thread].outBuf,";"); }
      if(HP.ChartOVERHEAT.get_present()) { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.ChartOVERHEAT.get_Point(i)/100.0,2)); strcat(Socket[thread].outBuf,";"); }
      if(HP.ChartTPEVA.get_present())    { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.ChartTPEVA.get_Point(i)/100.0,2)); strcat(Socket[thread].outBuf,";"); }
      if(HP.ChartTPCON.get_present())    { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.ChartTPCON.get_Point(i)/100.0,2)); strcat(Socket[thread].outBuf,";"); }
      #endif
      if(HP.dFC.ChartFC.get_present())       { strcat(Socket[thread].outBuf,int2str(HP.dFC.ChartFC.get_Point(i))); strcat(Socket[thread].outBuf,";"); }
      if(HP.dFC.ChartPower.get_present())    { strcat(Socket[thread].outBuf,int2str(HP.dFC.ChartPower.get_Point(i))); strcat(Socket[thread].outBuf,";"); }
      if(HP.dFC.ChartCurrent.get_present())  { strcat(Socket[thread].outBuf,int2str(HP.dFC.ChartCurrent.get_Point(i))); strcat(Socket[thread].outBuf,";"); }
      
      if(HP.ChartRCOMP.get_present())    { strcat(Socket[thread].outBuf,int2str(HP.ChartRCOMP.get_Point(i))); strcat(Socket[thread].outBuf,";"); }
  //    if(HP.ChartdCO.get_present())      { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.ChartdCO.get_Point(i)/100.0,2)); strcat(Socket[thread].outBuf,";"); }
      if ((HP.sTemp[TCONOUTG].Chart.get_present())&&(HP.sTemp[TCONING].Chart.get_present())) // считаем на лету экономим оперативку
                    { strcat(Socket[thread].outBuf,ftoa(temp,((float)HP.sTemp[TCONOUTG].Chart.get_Point(i)-(float)HP.sTemp[TCONING].Chart.get_Point(i))/100, 2)); strcat(Socket[thread].outBuf,(char*)";"); } 
 //      if(HP.ChartdGEO.get_present())     { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.ChartdGEO.get_Point(i)/100.0,2)); strcat(Socket[thread].outBuf,";"); }
       if ((HP.sTemp[TEVAING].Chart.get_present())&&(HP.sTemp[TEVAOUTG].Chart.get_present())) // считаем на лету экономим оперативку
                    { strcat(Socket[thread].outBuf,ftoa(temp,((float)HP.sTemp[TEVAOUTG].Chart.get_Point(i)-(float)HP.sTemp[TCONING].Chart.get_Point(i))/100, 2)); strcat(Socket[thread].outBuf,(char*)";"); } 
      
      if(HP.ChartPowerCO.get_present())  { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.ChartPowerCO.get_Point(i)/1000.0,3)); strcat(Socket[thread].outBuf,";"); } 
      if(HP.ChartPowerGEO.get_present()) { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.ChartPowerGEO.get_Point(i)/1000.0,3)); strcat(Socket[thread].outBuf,";"); }
      if(HP.ChartCOP.get_present())      { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.ChartCOP.get_Point(i)/100.0,2)); strcat(Socket[thread].outBuf,";"); }    

     #ifdef USE_ELECTROMETER_SDM 
     if(HP.dSDM.ChartVoltage.get_present())     { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.dSDM.ChartVoltage.get_Point(i)/100.0,2)); strcat(Socket[thread].outBuf,";"); } 
     if(HP.dSDM.ChartCurrent.get_present())     { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.dSDM.ChartCurrent.get_Point(i)/100.0,2)); strcat(Socket[thread].outBuf,";"); } 
  //   if(HP.dSDM.sAcPower.get_present())     { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.dSDM.sAcPower.get_Point(i),2)); strcat(Socket[thread].outBuf,";"); } 
  //   if(HP.dSDM.sRePower.get_present())     { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.dSDM.sRePower.get_Point(i),2)); strcat(Socket[thread].outBuf,";"); } 
     if(HP.dSDM.ChartPower.get_present())       { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.dSDM.ChartPower.get_Point(i),2)); strcat(Socket[thread].outBuf,";"); } 
  //   if(HP.dSDM.ChartPowerFactor.get_present()) { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.dSDM.ChartPowerFactor.get_Point(i)/100.0,2)); strcat(Socket[thread].outBuf,";"); } 
     if(HP.ChartFullCOP.get_present())       { strcat(Socket[thread].outBuf,ftoa(temp,(float)HP.ChartFullCOP.get_Point(i)/100.0,2)); strcat(Socket[thread].outBuf,";"); } 
     #endif
      
      STR_END;
      s=strlen(Socket[thread].outBuf);
      if(sendPacketRTOS(thread,(byte*)Socket[thread].outBuf,s,0)==0) return 0 ;                          // передать пакет, при ошибке выйти
      sum=sum+s;
     
   }  // for
  return sum; 
}


// файл статистики на карте отсутсвует
void noCsvChart_SD(uint8_t thread)
{
   get_Header(thread,(char*)FILE_CHART);
   sendPrintfRTOS(thread, "Файл %s  не найден на карте памяти.\r\nДля его создания в разделе ОПЦИИ ТН, установите соответсвующий флаг и включите ТН.\r\n", FILE_CHART);
}
    
// Получить индексный файл при отсутвии SD карты
// выдача массива index_noSD в index_noSD
int16_t get_indexNoSD(uint8_t thread)
{
    uint16_t n,i=0;
     #ifdef LOG 
       journal.jprintf("$Read: indexNoSD from memory\n");  
     #endif
    sendConstRTOS(thread,"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: keep-alive\r\n\r\n");
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
   strcpy(Socket[thread].outBuf,"HTTP/1.1 200 OK\r\nContent-Type:application/x-binary\r\nContent-Length:");
   strcat(Socket[thread].outBuf,int2str((SIZE_TEST/sizeof(Socket[thread].outBuf))*sizeof(Socket[thread].outBuf)));  //реальный размер передачи
   strcat(Socket[thread].outBuf,"\r\nContent-Disposition: attachment; filename=\"test.dat\"\r\n\r\n");
   sendPacketRTOS(thread,(byte*)Socket[thread].outBuf,strlen(Socket[thread].outBuf),0);
   
   // Генерация файла используется выходной буфер
   memset(Socket[thread].outBuf,0x55,sizeof(Socket[thread].outBuf));   // Заполнение буфера
   startTick=xTaskGetTickCount();                                      // Запомнить время старта
   for(j=0;j<SIZE_TEST/sizeof(Socket[thread].outBuf);j++)  
   {
    if (sendBufferRTOS(thread,(byte*)(Socket[thread].outBuf),sizeof(Socket[thread].outBuf))==0) break;
   }
   startTick=xTaskGetTickCount()-startTick;
   journal.jprintf("Download test.dat speed:%d bytes/sec\n",SIZE_TEST*1000/startTick);

}

// Получить состояние теплового насоса для отсылки по почте
// посылается в клиента используя tempBuf
void get_mailState(EthernetClient client,char *tempBuf)
{
uint8_t i,j;
int16_t x; 
char temp[10];
       strcpy(tempBuf,"\r\n -----  С О С Т О Я Н И Е  -----");
       strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
      
       strcpy(tempBuf,"\n  1. Тепловой насос");
       strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
    
       strcpy(tempBuf,"СостояниеТН: "); strcat(tempBuf,HP.StateToStr());
       strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));  
      
       strcpy(tempBuf,"Последняя ошибка: ");  strcat(tempBuf,int2str(HP.get_errcode())); strcat(tempBuf," - "); strcat(tempBuf,HP.get_lastErr());
       strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
   
       strcpy(tempBuf,"Режим работы: ");strcat(tempBuf,HP.TestToStr());
       strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf)); 
           
      strcpy(tempBuf,"Последняя перезагрузка: "); strcat(tempBuf,DecodeTimeDate(HP.get_startDT()));
      strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));  
     
      strcpy(tempBuf,"Время с последней перезагрузки: "); strcat(tempBuf,TimeIntervalToStr(HP.get_uptime()));
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
              strcat(tempBuf,ftoa(temp,(float)HP.sTemp[i].get_Temp()/100.0,2));
              if (HP.sTemp[i].get_lastErr()!=OK) { strcat(tempBuf," error:"); strcat(tempBuf,int2str(HP.sTemp[i].get_lastErr())); } 
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
            strcat(tempBuf,ftoa(temp,(float)HP.sADC[i].get_Press()/100.0,2)); 
            if (HP.sADC[i].get_lastErr()!=OK ) { strcat(tempBuf," error:"); strcat(tempBuf,int2str(HP.sADC[i].get_lastErr())); }
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
              strcat(tempBuf,int2str(HP.sInput[i].get_Input())); 
              strcat(tempBuf," alarm:"); strcat(tempBuf,int2str(HP.sInput[i].get_alarmInput())); 
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
              strcat(tempBuf,int2str(HP.dRelay[i].get_Relay())); 
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf)); 
              }         
           }
       
         #ifdef EEV_DEF
              strcpy(tempBuf,"\n  6. ЭРВ - ");  strcat(tempBuf,HP.dEEV.get_note());  strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
              strcpy(tempBuf,"Минимальное положение (шаги): ");  strcat(tempBuf,int2str(HP.dEEV.get_minEEV()));
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf)); 
              strcpy(tempBuf,"Полное открыте (шаги):");  strcat(tempBuf,int2str(HP.dEEV.get_maxEEV()));
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));    
              strcpy(tempBuf,"Правило управления ЭРВ: ");
              switch ((int)HP.dEEV.get_ruleEEV())
                   {
                    case TEVAOUT_TEVAIN:    strcat(tempBuf,"TEVAOUT_TEVAIN");    break;
                    case TRTOOUT_TEVAIN:    strcat(tempBuf,"TRTOOUT_TEVAIN");    break;
                    case TEVAOUT_PEVA:      strcat(tempBuf,"TEVAOUT_PEVA");      break;
                    case TRTOOUT_PEVA:      strcat(tempBuf,"TRTOOUT_PEVA");      break;
                    case TABLE:             strcat(tempBuf,"TABLE");             break;
                    case MANUAL:            strcat(tempBuf,"MANUAL");            break;
                    default:                strcat(tempBuf,cError);              break;    
                   }     
             strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));           
             strcpy(tempBuf,"Текущее положение (шаги): ");  strcat(tempBuf,int2str(HP.dEEV.get_EEV()));
             strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf)); 
             strcpy(tempBuf,"Текущий перегрев (градусы): ");  strcat(tempBuf,ftoa(temp,(float)HP.dEEV.get_Overheat()/100.0,2));
             strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf)); 
        #else 
             strcpy(tempBuf,"EEV absent");    strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf)); 
        #endif
         
       strcpy(tempBuf,"\n  7. Инвертор ");strcat(tempBuf,HP.dFC.get_name());
       strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));   
       if (HP.dFC.get_present()) 
         {    strcpy(tempBuf,"Текущее состояние инвертора: "); strcat(tempBuf,int2str(HP.dFC.read_stateFC()));
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf)); 
              strcpy(tempBuf,"Целевая частота [Гц]: ");    strcat(tempBuf,ftoa(temp,(float)HP.dFC.get_targetFreq()/100.0,2));
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));  
              strcpy(tempBuf,"Текущая частота [Гц]: ");    strcat(tempBuf,ftoa(temp,(float)HP.dFC.get_freqFC()/100.0,2));
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));  
              strcpy(tempBuf,"Текущая мощность [кВт]: ");  strcat(tempBuf,ftoa(temp,(float)HP.dFC.get_power()/10.0,1));
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
              strcpy(tempBuf,"Tемпература радиатора [°С]: ");  strcat(tempBuf,ftoa(temp,(float)HP.dFC.read_tempFC()/10.0,2));
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
              strcpy(tempBuf,"ЦАП дискреты: ");            strcat(tempBuf,int2str(HP.dFC.get_DAC()));
              strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));  
              
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
           strcpy(tempBuf,"Суммараная активная энергия [кВт/ч]: ");                     HP.dSDM.get_paramSDM((char*)sdm_ACENERGY,tempBuf); strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
           strcpy(tempBuf,"Суммараная реактивная энергия [кВт/ч]: ");                   HP.dSDM.get_paramSDM((char*)sdm_REENERGY,tempBuf); strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
           strcpy(tempBuf,"Суммараная потребленная энергия [кВт/ч]: ");                 HP.dSDM.get_paramSDM((char*)sdm_ENERGY,tempBuf);strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));
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
                strcat(tempBuf,ftoa(temp,(float)HP.sFrequency[i].get_Value()/1000.0,3)); 
                strcat(tempBuf,cStrEnd);  client.write(tempBuf,strlen(tempBuf));    
                }      
           }        
       
}
#ifdef I2C_EEPROM_64KB     
// Получить файл со ВСЕЙ статистикой возвращает число отправленных байт
uint16_t get_csvStatistic(uint8_t thread)
{
   int16_t i; 
   uint32_t sum=0,s=0;
   // заголовок
   sendConstRTOS(thread,"HTTP/1.1 200 OK\r\nContent-Type:text/plain\r\nContent-Disposition: attachment; filename=\"statistic.csv\"\r\n\r\n");
   strcpy(Socket[thread].outBuf,"Pos;Date;Tin;Tout;Tboiler;Hour;Hmoto;");
   #ifdef FLOWCON
   strcat(Socket[thread].outBuf,"EnergyCO;");   // выработанная энергия
   #endif
   #ifdef USE_ELECTROMETER_SDM
   strcat(Socket[thread].outBuf,"Energy220;");  // потраченная энергия
   strcat(Socket[thread].outBuf,"COP;");        // КОП
   #endif
   #ifdef FLOWCON
   strcat(Socket[thread].outBuf,"PowerCO;");    // средння мощность СО
   #endif
   #ifdef USE_ELECTROMETER_SDM
   strcat(Socket[thread].outBuf,"Power220;");   // средняя потребляемая мощность
   #endif
   STR_END;
   s=strlen(Socket[thread].outBuf);
   if(sendPacketRTOS(thread,(byte*)Socket[thread].outBuf,s,0)==0) return 0 ;                          // передать пакет, при ошибке выйти
   sum=s;
     for(i=0;i<HP.Stat.available();i++) // данные
     {
       HP.Stat.get_OneDay(Socket[thread].outBuf,i,false);  
       STR_END; 
       s=strlen(Socket[thread].outBuf);
       if(sendPacketRTOS(thread,(byte*)Socket[thread].outBuf,s,0)==0) return 0 ;                      // передать пакет, при ошибке выйти
       sum=sum+s;      
      }
    
  return sum;   
}
#endif

