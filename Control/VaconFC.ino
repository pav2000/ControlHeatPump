/*
 * Частотный преобразователь Vacon 10
 * Автор vad711, vad7@yahoo.com
 *
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
#include "VaconFC.h"

int8_t devVaconFC::initFC()
{
    err = OK; // ошибка частотника (работа) при ошибке останов ТН
    numErr = 0; // число ошибок чтение по модбасу для статистики
    number_err = 0; // Число ошибок связи при превышении FC_NUM_READ блокировка инвертора
    FC = 0; // Целевая скорость частотика
    FC_curr = 0; // текущая скорость инвертора
    power = 0; // Тееущая мощность частотника
    current = 0; // Текуший ток частотника
    startCompressor = 0; // время старта компрессора
    state = ERR_LINK_FC; // Состояние - нет связи с частотником

    testMode = NORMAL; // Значение режима тестирования
    name = (char*)FC_VACON_NAME; // Имя
    note = (char*)noteFC_NONE; // Описание инвертора   типа нет его

	  _data.Uptime=DEF_FC_UPTIME;				       // Время обновления алгоритма пид регулятора (мсек) Основной цикл управления
	  _data.PidFreqStep=DEF_FC_PID_FREQ_STEP;          // Максимальный шаг (на увеличение) изменения частоты при ПИД регулировании в 0.01 Гц Необходимо что бы ЭРВ успевал
	  _data.PidStop=DEF_FC_PID_STOP;				   // Проценты от уровня защит (мощность, ток, давление, темпеартура) при которой происходит блокировка роста частоты пидом
	  _data.dtCompTemp=DEF_FC_DT_COMP_TEMP;    		   // Защита по температуре компрессора - сколько градусов не доходит до максимальной (TCOMP) и при этом происходит уменьшение частоты
	  _data.startFreq=DEF_FC_START_FREQ;               // Стартовая скорость инвертора (см компрессор) в 0.01 
	  _data.startFreqBoiler=DEF_FC_START_FREQ_BOILER;  // Стартовая скорость инвертора (см компрессор) в 0.01 ГВС
	  _data.minFreq=DEF_FC_MIN_FREQ;                   // Минимальная  скорость инвертора (см компрессор) в 0.01 
	  _data.minFreqCool=DEF_FC_MIN_FREQ_COOL;          // Минимальная  скорость инвертора при охлаждении в 0.01 
	  _data.minFreqBoiler=DEF_FC_MIN_FREQ_BOILER;      // Минимальная  скорость инвертора при нагреве ГВС в 0.01
	  _data.minFreqUser=DEF_FC_MIN_FREQ_USER;          // Минимальная  скорость инвертора РУЧНОЙ РЕЖИМ (см компрессор) в 0.01
	  _data.maxFreq=DEF_FC_MAX_FREQ;                   // Максимальная скорость инвертора (см компрессор) в 0.01 
	  _data.maxFreqCool=DEF_FC_MAX_FREQ_COOL;          // Максимальная скорость инвертора в режиме охлаждения  в 0.01 
	  _data.maxFreqBoiler=DEF_FC_MAX_FREQ_BOILER;      // Максимальная скорость инвертора в режиме ГВС в 0.01 Гц поглощение бойлера обычно меньше чем СО
	  _data.maxFreqUser=DEF_FC_MAX_FREQ_USER;          // Максимальная скорость инвертора РУЧНОЙ РЕЖИМ (см компрессор) в 0.01 
	  _data.stepFreq=DEF_FC_STEP_FREQ ;                // Шаг уменьшения инвертора при достижении максимальной температуры, мощности и тока (см компрессор) в 0.01 
	  _data.stepFreqBoiler=DEF_FC_STEP_FREQ_BOILER;    // Шаг уменьшения инвертора при достижении максимальной температуры, мощности и тока ГВС в 0.01
	  _data.dtTemp=DEF_FC_DT_TEMP;                     // Привышение температуры от уставок (подача) при которой срабатыват защита (уменьшается частота) в сотых градуса
	  _data.dtTempBoiler=DEF_FC_DT_TEMP_BOILER;        // Привышение температуры от уставок (подача) при которой срабатыват защита ГВС в сотых градуса
	 #ifdef FC_ANALOG_CONTROL
	  _data.level0=0;                                  // Отсчеты ЦАП соответсвующие 0   мощности
	  _data.level100=4096;                             // Отсчеты ЦАП соответсвующие 100 мощности
	  _data.levelOff=10;                               // Минимальная мощность при котором частотник отключается (ограничение минимальной мощности)
	  #endif
	  flags=0x00;                                // флаги  0 - наличие FC
	  _data.setup_flags = 0;
	 
    if(!Modbus.get_present()) // modbus отсутствует
    {
        SETBIT0(flags, fFC); // Инвертор не работает
        journal.jprintf("%s, modbus not found, block.\n", name);
        err = ERR_NO_MODBUS;
        return err;
    } else if(DEVICEFC == true)
        SETBIT1(flags, fFC); // наличие частотника в текушей конфигурации

    if(get_present())
        journal.jprintf("Invertor %s: present config\n", name);
    else {
        journal.jprintf("Invertor %s: none config\n", name);
        return err;
    } // выходим если нет инвертора

    ChartFC.init(get_present()); // инициалазация графика
#ifdef FC_ANALOG_CONTROL // Аналоговое управление графики не нужны
    ChartPower.init(false); // инициалазация графика
    ChartCurrent.init(false); // инициалазация графика
#else
    ChartPower.init(get_present()); // инициалазация графика
    ChartCurrent.init(get_present()); // инициалазация графика
#endif

#ifndef FC_ANALOG_CONTROL // НЕ Аналоговое управление
    CheckLinkStatus(); // проверка связи с инвертором
    if(err != OK) return err; // связи нет выходим
    journal.jprintf("Test link Modbus %s: OK\n", name); // Тест пройден

    uint8_t i = 3;
    while((state & FC_S_RUN)) // Если частотник работает то остановить его
    {
    	if(i-- == 0) break;
        stop_FC();
        journal.jprintf("Wait stop %s...\n", name);
        _delay(3000);
        CheckLinkStatus(); //  Получить состояние частотника
    }
    if(i == 0) { // Не останавливается
        err = ERR_MODBUS_STATE;
        set_Error(err, name);
    }
    if(err == OK) {
        // 10.Установить стартовую частоту
        set_targetFreq(_data.startFreq, true, _data.minFreqUser, _data.maxFreqUser); // режим н знаем по этому границы развигаем
    }
    // Вычисление номинальной мощности двигателя компрессора = U*sqrt(3)*I*cos, W
   	//nominal_power = (uint32_t) (400) * (700) / 100 * (75) / 100; // W
    nominal_power = (uint32_t)read_0x03_16(FC_MOTOR_NVOLT) * 173 / 100 * read_0x03_16(FC_MOTOR_NA) / 100 * read_0x03_16(FC_MOTOR_NCOS) / 100;
    journal.jprintf(" Nominal: %d W\n", nominal_power);
#endif // #ifndef FC_ANALOG_CONTROL
    return err;
}

// Возвращает состояние, или ERR_LINK_FC, если нет связи
int16_t devVaconFC::CheckLinkStatus(void)
{
    if(testMode == NORMAL || testMode == HARD_TEST){
		for (uint8_t i = 0; i < FC_NUM_READ+1; i++) // Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
		{
			err = Modbus.readHoldingRegisters16(FC_MODBUS_ADR, FC_STATUS - 1, (uint16_t *)&state); // Послать запрос, Нумерация регистров с НУЛЯ!!!!
			if(err == OK) break; // Прочитали удачно
			check_blockFC(); // проверить необходимость блокировки
			if(GETBIT(flags, fErrFC)) break; // превысили кол-во ошибок
			_delay(FC_DELAY_REPEAT);
		}
		if(err != OK) state = ERR_LINK_FC;
    } else {
    	state = FC_S_RDY;
    	err = OK;
    }
    return state;
}

// Прочитать (внутренние переменные обновляются) состояние Инвертора, возвращает или ОК или ошибку
// Вызывается из задачи чтения датчиков период FC_TIME_READ
int8_t devVaconFC::get_readState()
{
    if(testMode != NORMAL && testMode != HARD_TEST){
    	state = FC_S_RDY;
    	FC_curr = FC;
    	return OK;
    }
    if(!get_present() || state == ERR_LINK_FC) return err; // выходим если нет инвертора или нет связи
    err = OK;
#ifndef FC_ANALOG_CONTROL // Не аналоговое управление
    // Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
    state = read_0x03_16(FC_STATUS); // прочитать состояние
    if(err) // Ошибка
    {
        state = ERR_LINK_FC; // признак потери связи с инвертором
        SETBIT1(flags, fErrFC); // Блок инвертора
        set_Error(err, name); // генерация ошибки
        return err; // Возврат
    }
    if(GETBIT(flags, fOnOff)) { // ТН включил компрессор, проверяем состояние инвертора
		if(state & FC_S_FLT) { // Действующий отказ
			err = ERR_FC_FAULT;
		} else if(rtcSAM3X8.unixtime() - get_startTime()>FC_ACCEL_TIME/100 && ((state & (FC_S_RDY | FC_S_RUN | FC_S_DIR)) != (FC_S_RDY | FC_S_RUN))){
			err = ERR_MODBUS_STATE;
		}
		if(err) {
			set_Error(err, name); // генерация ошибки
		}
	} else if(state & FC_S_RUN) { // Привод работает, а не должен - останавливаем через модбас
		if(rtcSAM3X8.unixtime() - HP.get_stopCompressor() > FC_DEACCEL_TIME/100) {
			journal.jprintf(pP_TIME, "Compressor running - stop via Modbus!\n");
	        err = write_0x06_16(FC_CONTROL, FC_C_STOP); // подать команду ход/стоп через модбас
	        if(err) return err;
		}
	}
    // Состояние прочитали и оно правильное все остальное читаем
    uint32_t reg32 = read_0x03_32(FC_SPEED); // +FC_FREQ(low word). прочитать текущую скорость и частоту
    if(err == OK) {
        FC_curr = reg32 >> 16;
        FC_curr_freq = reg32 & 0xFFFF;
		power = read_0x03_16(FC_POWER); // прочитать мощность
		if(err == OK) {
		    current = read_0x03_16(FC_CURRENT); // прочитать ток
		    err = Modbus.get_err(); // Скопировать ошибку
		}
		if(GETBIT(_data.setup_flags,fLogWork) && GETBIT(flags, fOnOff)) {
			journal.jprintf(pP_TIME, "FC: %Xh,%.2f%%,%.2fHz,%.2fA,%.2f%%=%.3f\n", state, (float)FC_curr/100.0, (float)FC_curr_freq/100.0, (float)current/100.0, (float)power/100.0, (float)get_power()/1000.0);
		}
    }
#else // Аналоговое управление
    FC_curr = FC;
    power = 0;
    current = 0;
#endif
    return err;
}

// Установить целевую скорость в %
// show - показывать сообщение сообщение или нет, два оставшихся параметра границы
int8_t devVaconFC::set_targetFreq(int16_t x, boolean show, int16_t _min, int16_t _max)
{
#ifdef DEMO
    if((x >= _min) && (x <= _max)) // Проверка диапазона разрешенных частот
    {
        FC = x;
        if(show) journal.jprintf(" Set %s: %.2f\n", name, (float)FC / 100);
        return err;
    } // установка частоты OK  - вывод сообщения если надо
    else {
    	err = ERR_FC_ERROR;
        journal.jprintf("%s: Wrong frequency %.2f\n", name, (float)x / 100);
        return WARNING_VALUE;
    }
#else // Боевой вариант
    if((!get_present()) || (GETBIT(flags, fErrFC))) return err; // выходим если нет инвертора или он заблокирован по ошибке
    if((x >= _min) && (x <= _max)) // Проверка диапазона разрешенных частот
    {
#ifndef FC_ANALOG_CONTROL // Не аналоговое управление
        // Запись в регистры инвертора установленной частоты
    	err = OK;
        if(testMode == NORMAL || testMode == HARD_TEST){
			err = write_0x06_16((uint16_t)FC_SET_SPEED, x);
        }
        if(err == OK) {
            FC = x;
            if(show) journal.jprintf(" Set %s: %.2f%%\n", name, (float)FC / 100);
            return err;
        } // установка частоты OK  - вывод сообщения если надо
        else {
            SETBIT1(flags, fErrFC);
            set_Error(err, name);
            return err;
        } // генерация ошибки
#else // Аналоговое управление
        FC = x;
        dac = ((level100 - level0) * FC - 0 * level100) / (100 - 0);
        switch (testMode) // РЕАЛЬНЫЕ Действия в зависимости от режима
        {
        case NORMAL:
            analogWrite(pin, dac);
            break; //  Режим работа не тест, все включаем
        case SAFE_TEST:
            break; //  Ничего не включаем
        case TEST:
            break; //  Включаем все кроме компрессора
        case HARD_TEST:
            analogWrite(pin, dac);
            break; //  Все включаем и компрессор тоже
        }
        if(show)
            journal.jprintf(" Set %s: %.2f%%\n", name, (float)FC / 100); // установка частоты OK  - вывод сообщения если надо
#endif
        return err;
    } // if((x>=_min)&&(x<=_max))
    else {
        journal.jprintf("%s: Wrong speed %.2f\n", name, (float)x / 100);
        return WARNING_VALUE;
    }
#endif // DEMO
}

// Установить запрет на использование инвертора если лимит ошибок исчерпан
void devVaconFC::check_blockFC()
{
#ifndef FC_ANALOG_CONTROL // Не аналоговое управление
    if((xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) && (err != OK)) // если не запущена free rtos то блокируем с первого раза
    {
        SETBIT1(flags, fErrFC); // Установить флаг
        note = (char*)noteFC_NO;
        set_Error(err, (char*)name); // Подъем ошибки на верх и останов ТН
        return;
    }
    if(err != OK)
        number_err++;
    else {
    	SETBIT0(flags, fErrFC);
        number_err = 0;
        note = (char*)noteFC_OK; // Описание инвертора есть
        return;
    } // Увеличить счетчик ошибок
    if(number_err > FC_NUM_READ) // если превышено число ошибок то блокировка
    {
        //SemaphoreGive(xModbusSemaphore); // разблокировать семафор
        SETBIT1(flags, fErrFC); // Установить флаг
        note = (char*)noteFC_NO;
        set_Error(err, (char*)name); // Подъем ошибки на верх и останов ТН
    }
#endif
}

// Установить значение текущий режим работы
void  devVaconFC::set_testMode(TEST_MODE t)
{
	testMode = t;
	if(t == SAFE_TEST || t == TEST) {
		SETBIT0(flags, fErrFC);
		err = OK;
	} else {
	    //CheckLinkStatus(); // проверка связи с инвертором
	}
}

// Команда ход на инвертор (целевая скорость НЕ ВЫСТАВЛЯЕТСЯ)
// Может быть подана команда через реле и через модбас в зависимости от ключей компиляции
int8_t devVaconFC::start_FC()
{
    if(testMode != NORMAL && testMode != HARD_TEST) {
        err = OK;
    	goto xStarted;
    }
	if(!get_present() || GETBIT(flags, fErrFC)) return err = ERR_MODBUS_BLOCK; // выходим если нет инвертора или он заблокирован по ошибке
	if(FC < _data.minFreq || FC > _data.maxFreq) {
		err = ERR_FC_ERROR;
		journal.jprintf(" %s: Wrong frequency, ignore start\n", name);
		return err;
	}
    err = OK;
#ifndef FC_ANALOG_CONTROL // Не аналоговое управление
#ifdef DEMO
#ifdef FC_USE_RCOMP // Использовать отдельный провод для команды ход/стоп
    HP.dRelay[RCOMP].set_ON(); // ПЛОХО через глобальную переменную
#endif // FC_USE_RCOMP
    if(err == OK) {
        SETBIT1(flags, fOnOff);
        startCompressor = rtcSAM3X8.unixtime();
        journal.jprintf(" %s ON\n", name);
    }
    else {
        SETBIT1(flags, fErrFC);
        set_Error(err, name);
    } // генерация ошибки
#else // DEMO
    // Боевая часть
    // set_targetFreq(FC_START_FC,true);  // Запись в регистр инвертора стартовой частоты  НЕ всегда скорость стартовая - супербойлер
	if((state & FC_S_FLT)) { // Действующий отказ
		uint16_t reg = read_0x03_16(FC_ERROR);
		journal.jprintf("%s, Fault: %s(%d) - ", name, err ? cError : get_fault_str(reg), err ? err : reg);
		if(GETBIT(_data.setup_flags, fAutoResetFault)) { // Автосброс не критичного сбоя инвертора
			if(!err) { // код считан успешно
				uint8_t i = 0;
				for(; i < sizeof(FC_NonCriticalFaults); i++) if(FC_NonCriticalFaults[i] == reg) break;
				if(i < sizeof(FC_NonCriticalFaults)) {
					if(reset_errorFC()) {
						if((state & FC_S_FLT)) err = ERR_FC_FAULT; else journal.jprintf("Reseted.\n");
					}
					if(err) journal.jprintf(" reset failed!\n");
				} else {
					journal.jprintf("Critical!\n");
					err = ERR_FC_FAULT;
				}
			}
		} else {
			journal.jprintf("skip start!\n");
			err = ERR_FC_FAULT;
		}
	}
	if(err == OK) {
#ifdef FC_USE_RCOMP // Использовать отдельный провод для команды ход/стоп
		HP.dRelay[RCOMP].set_ON();
#else
		err = write_0x06_16(FC_CONTROL, FC_C_RUN); // Команда Ход
#endif
	}
#endif // DEMO	
    if(err == OK) {
xStarted:
    	SETBIT1(flags, fOnOff);
        startCompressor = rtcSAM3X8.unixtime();
        journal.jprintf(" %s ON\n", name);
    } else {
        SETBIT1(flags, fErrFC);
        set_Error(err, name);
    } // генерация ошибки
#else //  FC_ANALOG_CONTROL
#ifdef DEMO
#ifdef FC_USE_RCOMP // Использовать отдельный провод для команды ход/стоп
    HP.dRelay[RCOMP].set_ON(); // ПЛОХО через глобальную переменную
#endif // FC_USE_RCOMP
    SETBIT1(flags, fOnOff);
    startCompressor = rtcSAM3X8.unixtime();
    journal.jprintf(" %s ON\n", name);
#else // DEMO
    // Боевая часть
    if((testMode == NORMAL) || (testMode == HARD_TEST)) //   Режим работа и хард тест, все включаем,
    {
#ifdef FC_USE_RCOMP // Использовать отдельный провод для команды ход/стоп
        HP.dRelay[RCOMP].set_ON(); // ПЛОХО через глобальную переменную
#endif
        err = ERR_FC_CONF_ANALOG;
        SETBIT1(flags, fErrFC);
        set_Error(err, name); // Ошибка конфигурации
    }
    SETBIT1(flags, fOnOff);
    startCompressor = rtcSAM3X8.unixtime();
    journal.jprintf(" %s ON\n", name);
#endif
#endif
    return err;
}

// Команда стоп на инвертор Обратно код ошибки
int8_t devVaconFC::stop_FC()
{
    if((testMode == NORMAL || testMode == HARD_TEST) && (!get_present() || state == ERR_LINK_FC))
        return err; // выходим если нет инвертора или нет связи
    err = OK;
#ifndef FC_ANALOG_CONTROL // Не аналоговое управление
#ifdef DEMO
#ifdef FC_USE_RCOMP // Использовать отдельный провод для команды ход/стоп
    HP.dRelay[RCOMP].set_OFF(); // ПЛОХО через глобальную переменную
#endif // FC_USE_RCOMP
    if(err == OK) {
        SETBIT0(flags, fOnOff);
        startCompressor = 0;
        journal.jprintf(" %s OFF\n", name);
    }
    else {
        SETBIT1(flags, fErrFC);
        set_Error(err, name);
    } // генерация ошибки
#else // DEMO
    if((testMode == NORMAL) || (testMode == HARD_TEST)) // Режим работа и хард тест, все включаем,
    {
#ifdef FC_USE_RCOMP // Использовать отдельный провод для команды ход/стоп
        HP.dRelay[RCOMP].set_OFF();
#else
        err = write_0x06_16(FC_CONTROL, FC_C_STOP); // подать команду ход/стоп через модбас
#endif
    }
    if(err == OK) {
        SETBIT0(flags, fOnOff);
        startCompressor = 0;
        journal.jprintf(" %s OFF\n", name);
    }
    else {
        SETBIT1(flags, fErrFC);
        set_Error(err, name);
    } // генерация ошибки
#endif
#else // FC_ANALOG_CONTROL
#ifdef DEMO
#ifdef FC_USE_RCOMP // Использовать отдельный провод для команды ход/стоп
    HP.dRelay[RCOMP].set_OFF(); // ПЛОХО через глобальную переменную
#endif // FC_USE_RCOMP
    SETBIT0(flags, fOnOff);
    startCompressor = 0;
    journal.jprintf(" %s OFF\n", name);
#else // DEMO
    if((testMode == NORMAL) || (testMode == HARD_TEST)) // Режим работа и хард тест, все включаем,
    {
#ifdef FC_USE_RCOMP // Использовать отдельный провод для команды ход/стоп
        HP.dRelay[RCOMP].set_OFF(); // ПЛОХО через глобальную переменную
#else // подать команду ход/стоп через модбас
        err = ERR_FC_CONF_ANALOG;
        SETBIT1(flags, fErrFC);
        set_Error(err, name); // Ошибка конфигурации
#endif
    }
    SETBIT0(flags, fOnOff);
    startCompressor = 0;
    journal.jprintf(" %s OFF\n", name);
#endif
#endif // FC_ANALOG_CONTROL
    return err;
}

// Получить параметр инвертора в виде строки, результат ДОБАВЛЯЕТСЯ в ret
void devVaconFC::get_paramFC(char *var,char *ret)
{

	ret += m_strlen(ret);
    if(strcmp(var,fc_ON_OFF)==0)                { if (GETBIT(flags,fOnOff))  strcat(ret,(char*)cOne);else  strcat(ret,(char*)cZero); } else
    if(strcmp(var,fc_INFO)==0)                  {
    	                                        #ifndef FC_ANALOG_CONTROL  
    	                                        get_infoFC(ret);
    	                                        #else
                                                 strcat(ret, "|Данные не доступны, работа через аналоговый вход|;") ;
                                                #endif              
    	                                        } else
    if(strcmp(var,fc_NAME)==0)                  {  strcat(ret,name);             } else
    if(strcmp(var,fc_NOTE)==0)                  {  strcat(ret,note);             } else
    if(strcmp(var,fc_PRESENT)==0)               { if (GETBIT(flags,fFC))  strcat(ret,(char*)cOne);else  strcat(ret,(char*)cZero); } else
    if(strcmp(var,fc_STATE)==0)                 {  _itoa(state,ret);   } else
    if(strcmp(var,fc_FC)==0)                    {  _ftoa(ret,(float)FC/100.0,2); strcat(ret, "%"); } else
    if(strcmp(var,fc_cFC)==0)                   {  _ftoa(ret,(float)FC_curr/100.0,2); strcat(ret, "%"); } else
    if(strcmp(var,fc_cPOWER)==0)                {  _itoa(get_power(), ret); } else
    if(strcmp(var,fc_INFO1)==0)                 {  _ftoa(ret,(float)FC_curr_freq/100.0,2);  strcat(ret, " Гц"); } else // Текущая частота!
    if(strcmp(var,fc_cCURRENT)==0)              {  _ftoa(ret,(float)current/100.0,2); } else
    if(strcmp(var,fc_AUTO_RESET_FAULT)==0)      {  strcat(ret,(char*)(GETBIT(_data.setup_flags,fAutoResetFault) ? cOne : cZero)); } else
    if(strcmp(var,fc_LogWork)==0)      			{  strcat(ret,(char*)(GETBIT(_data.setup_flags,fLogWork) ? cOne : cZero)); } else
    if(strcmp(var,fc_ANALOG)==0)                { // Флаг аналогового управления
		                                        #ifdef FC_ANALOG_CONTROL                                                    
		                                         strcat(ret,(char*)cOne);
		                                        #else
		                                         strcat(ret,(char*)cZero);
		                                        #endif
                                                } else
	#ifdef FC_ANALOG_CONTROL
    if(strcmp(var,fc_PIN)==0)                   {  _itoa(pin,ret);     	    } else
    if(strcmp(var,fc_DAC)==0)                   {  _itoa(dac,ret);          } else
    if(strcmp(var,fc_LEVEL0)==0)                {  _itoa(level0,ret);       } else
    if(strcmp(var,fc_LEVEL100)==0)              {  _itoa(level100,ret);     } else
    if(strcmp(var,fc_LEVELOFF)==0)              {  _itoa(levelOff,ret);     } else
    #endif
    if(strcmp(var,fc_BLOCK)==0)                 { if (GETBIT(flags,fErrFC))  strcat(ret,(char*)cYes); } else
    if(strcmp(var,fc_ERROR)==0)                 {  _itoa(err,ret);          } else
    if(strcmp(var,fc_UPTIME)==0)                {  _itoa(_data.Uptime,ret); } else   // вывод в секундах
    if(strcmp(var,fc_PID_STOP)==0)              {  _itoa(_data.PidStop,ret);          } else
    if(strcmp(var,fc_DT_COMP_TEMP)==0)          {  _ftoa(ret,(float)_data.dtCompTemp/100.0,2); } else // градусы

	if(strcmp(var,fc_PID_FREQ_STEP)==0)         {  _ftoa(ret,(float)_data.PidFreqStep/100.0,2); } else // %
	if(strcmp(var,fc_START_FREQ)==0)            {  _ftoa(ret,(float)_data.startFreq/100.0,2); } else // %
	if(strcmp(var,fc_START_FREQ_BOILER)==0)     {  _ftoa(ret,(float)_data.startFreqBoiler/100.0,2); } else // %
	if(strcmp(var,fc_MIN_FREQ)==0)              {  _ftoa(ret,(float)_data.minFreq/100.0,2); } else // %
	if(strcmp(var,fc_MIN_FREQ_COOL)==0)         {  _ftoa(ret,(float)_data.minFreqCool/100.0,2); } else // %
	if(strcmp(var,fc_MIN_FREQ_BOILER)==0)       {  _ftoa(ret,(float)_data.minFreqBoiler/100.0,2); } else // %
	if(strcmp(var,fc_MIN_FREQ_USER)==0)         {  _ftoa(ret,(float)_data.minFreqUser/100.0,2); } else // %
	if(strcmp(var,fc_MAX_FREQ)==0)              {  _ftoa(ret,(float)_data.maxFreq/100.0,2); } else // %
	if(strcmp(var,fc_MAX_FREQ_COOL)==0)         {  _ftoa(ret,(float)_data.maxFreqCool/100.0,2); } else // %
	if(strcmp(var,fc_MAX_FREQ_BOILER)==0)       {  _ftoa(ret,(float)_data.maxFreqBoiler/100.0,2); } else // %
	if(strcmp(var,fc_MAX_FREQ_USER)==0)         {  _ftoa(ret,(float)_data.maxFreqUser/100.0,2); } else // %
	if(strcmp(var,fc_STEP_FREQ)==0)             {  _ftoa(ret,(float)_data.stepFreq/100.0,2); } else // %
	if(strcmp(var,fc_STEP_FREQ_BOILER)==0)      {  _ftoa(ret,(float)_data.stepFreqBoiler/100.0,2); } else // %

    if(strcmp(var,fc_DT_TEMP)==0)               {  _ftoa(ret,(float)_data.dtTemp/100.0,2); } else // градусы
    if(strcmp(var,fc_DT_TEMP_BOILER)==0)        {  _ftoa(ret,(float)_data.dtTempBoiler/100.0,2); } else // градусы
    if(strcmp(var,fc_MB_ERR)==0)        		{  _itoa(numErr, ret); } else
    strcat(ret,(char*)cInvalid);
}

// Установить параметр инвертора из строки
boolean devVaconFC::set_paramFC(char *var, float f)
{
	int16_t x = f;
    if(strcmp(var,fc_ON_OFF)==0)                { if (x==0) stop_FC();else start_FC();return true;  } else 
    if(strcmp(var,fc_AUTO_RESET_FAULT)==0)      { _data.setup_flags = (_data.setup_flags & ~(1<<fAutoResetFault)) | ((x!=0)<<fAutoResetFault); return true;  } else
    if(strcmp(var,fc_LogWork)==0)               { _data.setup_flags = (_data.setup_flags & ~(1<<fLogWork)) | ((x!=0)<<fLogWork); return true;  } else
	#ifdef FC_ANALOG_CONTROL
    if(strcmp(var,fc_LEVEL0)==0)                { if ((x>=0)&&(x<=4096)) { level0=x; return true;} else return false;      } else 
    if(strcmp(var,fc_LEVEL100)==0)              { if ((x>=0)&&(x<=4096)) { level100=x; return true;} else return false;    } else 
    if(strcmp(var,fc_LEVELOFF)==0)              { if ((x>=0)&&(x<=4096)) { levelOff=x; return true;} else return false;    } else 
    #endif
    if(strcmp(var,fc_BLOCK)==0)                 { SemaphoreGive(xModbusSemaphore); // отдать семафор ВСЕГДА  
                                                if(x==0) { if(reset_FC()) note=(char*)noteFC_OK; }
                                                else     { SETBIT1(flags,fErrFC); note=(char*)noteFC_NO; }
                                                return true;            
                                                } else  // только чтение
    if(strcmp(var,fc_UPTIME)==0)                { if((x>=1)&&(x<65)){_data.Uptime=x;return true; } else return false; } else   // хранение в сек
    if(strcmp(var,fc_PID_STOP)==0)              { if((x>50)&&(x<=100)){_data.PidStop=x;return true; } else return false;  } else
   
		x = rd(f, 100);
    	if(strcmp(var,fc_DT_COMP_TEMP)==0)          { if((x>0)&&(x<2500)){_data.dtCompTemp=x;return true; } else return false; } else // градусы
		if(strcmp(var,fc_FC)==0)                    { if((x>=_data.minFreqUser)&&(x<=_data.maxFreqUser)){set_targetFreq(x,true, _data.minFreqUser, _data.maxFreqUser); return true; } } else
		if(strcmp(var,fc_DT_TEMP)==0)               { if((x>0)&&(x<1000)){_data.dtTemp=x;return true; } } else // градусы
		if(strcmp(var,fc_DT_TEMP_BOILER)==0)        { if((x>0)&&(x<1000)){_data.dtTempBoiler=x;return true; } } else // градусы

		if(strcmp(var,fc_START_FREQ)==0)            { if((x>0)&&(x<=10000)){_data.startFreq=x;return true; } } else // %
		if(strcmp(var,fc_START_FREQ_BOILER)==0)     { if((x>0)&&(x<=10000)){_data.startFreqBoiler=x;return true; } } else // %
		if(strcmp(var,fc_MIN_FREQ)==0)              { if((x>0)&&(x<=10000)){_data.minFreq=x;return true; } } else // %
		if(strcmp(var,fc_MIN_FREQ_COOL)==0)         { if((x>0)&&(x<8000)){_data.minFreqCool=x;return true; } } else // %
		if(strcmp(var,fc_MIN_FREQ_BOILER)==0)       { if((x>0)&&(x<8000)){_data.minFreqBoiler=x;return true; } } else // %
		if(strcmp(var,fc_MIN_FREQ_USER)==0)         { if((x>0)&&(x<8000)){_data.minFreqUser=x;return true; } } else // %
		if(strcmp(var,fc_MAX_FREQ)==0)              { if((x>0)&&(x<20000)){_data.maxFreq=x;return true; } } else // %
		if(strcmp(var,fc_MAX_FREQ_COOL)==0)         { if((x>0)&&(x<20000)){_data.maxFreqCool=x;return true; } } else // %
		if(strcmp(var,fc_MAX_FREQ_BOILER)==0)       { if((x>0)&&(x<20000)){_data.maxFreqBoiler=x;return true; } } else // %
		if(strcmp(var,fc_MAX_FREQ_USER)==0)         { if((x>0)&&(x<20000)){_data.maxFreqUser=x;return true; } } else // %
		if(strcmp(var,fc_PID_FREQ_STEP)==0)         { if((x>0)&&(x<10000)){_data.PidFreqStep=x;return true; } } else // %
		if(strcmp(var,fc_STEP_FREQ)==0)             { if((x>0)&&(x<10000)){_data.stepFreq=x;return true; } } else // %
		if(strcmp(var,fc_STEP_FREQ_BOILER)==0)      { if((x>0)&&(x<10000)){_data.stepFreqBoiler=x;return true; } } // %
 
    return false;
}

// Вывести в buffer строковый статус.
void devVaconFC::get_infoFC_status(char *buffer, uint16_t st)
{
	if(st & FC_S_RDY) 	strcat(buffer, FC_S_RDY_str);
	if(st & FC_S_RUN) 	{
		strcat(buffer, FC_S_RUN_str);
		if((st & FC_S_AREF)==0)	strcat(buffer, FC_S_AREF_str0);
	}
	if(st & FC_S_DIR) 	strcat(buffer, FC_S_DIR_str);
	if(st & FC_S_FLT) 	strcat(buffer, FC_S_FLT_str);
	if(st & FC_S_W) 	strcat(buffer, FC_S_W_str);
	if(st & FC_S_Z) 	strcat(buffer, FC_S_Z_str);
	if(st) *(buffer + m_strlen(buffer) - 1) = '\0'; // skip last ','
}

// Получить информацию о частотнике
void devVaconFC::get_infoFC(char* buf)
{
	buf += m_strlen(buf);
	if(testMode == NORMAL || testMode == HARD_TEST) {
#ifndef FC_ANALOG_CONTROL // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
		if(state == ERR_LINK_FC) {   // Нет связи
			strcat(buf, "|Данные не доступны - нет связи|;");
		} else {
			uint32_t i;
			strcat(buf, "2101|Состояние инвертора: ");
			get_infoFC_status(buf + m_strlen(buf), state);
			buf += m_snprintf(buf += m_strlen(buf), 256, "|%Xh;", state);
			if(err == OK) {
				buf += m_snprintf(buf, 256, "2103|Фактическая скорость|%.2f%%;2108 (V1.1)|Выходная мощность: %.1f%% (кВт)|%.3f;", (float)read_0x03_16(FC_SPEED) / 100.0, (float)power / 10.0, (float)get_power()/1000.0);
				buf += m_snprintf(buf, 256, "2105 (V1.3)|Обороты (об/м)|%d;", (int16_t)read_0x03_16(FC_RPM));
				buf += m_snprintf(buf, 256, "2107 (V1.5)|Крутящий момент|%.1f%%;", (float)(int16_t)read_0x03_16(FC_TORQUE) / 10.0);
				i = read_0x03_32(FC_VOLTAGE); // +FC_VOLTATE_DC (low word)
				buf += m_snprintf(buf, 256, "2109 (V1.7)|Выходное напряжение (В)|%.1f;2110 (V1.8)|Напряжение шины постоянного тока (В)|%d;", (float)(i >> 16) / 10.0, i & 0xFFFF);
				buf += m_snprintf(buf, 256, "0008 (V1.9)|Температура радиатора (°С)|%d;", read_tempFC());
				i = read_0x03_32(FC_POWER_DAYS); // +FC_POWER_HOURS (low word)
				buf += m_snprintf(buf, 256, "0828 (V3.3)|Время включения инвертора (дней:часов)|%d:%d;", i >> 16, i & 0xFFFF);
				i = read_0x03_32(FC_RUN_DAYS); // +FC_RUN_HOURS (low word)
				buf += m_snprintf(buf, 256, "0840 (V3.5)|Время работы компрессора (дней:часов)|%d:%d;", i >> 16, i & 0xFFFF);
				buf += m_snprintf(buf, 256, "0842 (V3.6)|Счетчик аварийных отключений|%d;", read_0x03_16(FC_NUM_FAULTS));

				i = read_0x03_16(FC_ERROR);
				if(err == OK) {
					m_snprintf(buf, 256, "2111|Активная ошибка|%s (%d);", get_fault_str(i), i);
				} else {
					m_snprintf(buf, 256, "-|Ошибка Modbus|%d;", err);
				}
			}
		}
#endif
	} else {
		m_snprintf(buf, 256, "|Режим тестирования!|;");
	}
}
// Сброс сбоя инвертора
boolean devVaconFC::reset_errorFC()
{
#ifndef FC_ANALOG_CONTROL // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
	if((state & FC_S_FLT)) {
		if(write_0x06_16(FC_CONTROL, FC_C_RST)) { // сброс отказа
			journal.jprintf("%s: Error reset fault!\n", name);
		}
	}
    read_stateFC();
#endif
    return err == OK;
}

// Сброс состояния связи инвертора через модбас
boolean devVaconFC::reset_FC()
{
    number_err = 0;
    CheckLinkStatus();
    if(!err && state == ERR_LINK_FC) err = ERR_RESET_FC;
    return err == OK;
}

// Текущее состояние инвертора
inline int16_t devVaconFC::read_stateFC()
{
#ifndef FC_ANALOG_CONTROL // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
    return state = read_0x03_16(FC_STATUS);
#else
    return 0;
#endif
}

// Tемпература радиатора
int16_t devVaconFC::read_tempFC()
{
#ifndef FC_ANALOG_CONTROL // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
    return read_0x03_16(FC_TEMP);
#else
    return 0;
#endif
}
// Функции общения по модбас инвертора Чтение регистров
#ifndef FC_ANALOG_CONTROL // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
// Функция 0х03 прочитать 2 байта, возвращает значение, ошибка обновляется
// Реализовано FC_NUM_READ попыток чтения/записи в инвертор
int16_t devVaconFC::read_0x03_16(uint16_t cmd)
{
    uint8_t i;
    int16_t result;
    if((!get_present()) || state == ERR_LINK_FC) return 0; // выходим если нет инвертора или он заблокирован по ошибке
    for (i = 0; i < FC_NUM_READ; i++) // делаем FC_NUM_READ попыток чтения Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
    {
        err = Modbus.readHoldingRegisters16(FC_MODBUS_ADR, cmd - 1, (uint16_t *)&result); // Послать запрос, Нумерация регистров с НУЛЯ!!!!
        if(err == OK) break; // Прочитали удачно
        numErr++; // число ошибок чтение по модбасу
        journal.jprintf("Modbus reg #%d - ", cmd);
        journal.jprintf(pP_TIME, cErrorRS485, name, __FUNCTION__, err); // Выводим сообщение о повторном чтении
        _delay(FC_DELAY_REPEAT);
        //         journal.jprintf(pP_TIME,cErrorRS485,name,err);                     // Вывод кода ошибки в журнал
    }
    check_blockFC(); // проверить необходимость блокировки
    return result;
}

// Функция 0х03 прочитать 2 байта, возвращает значение, ошибка обновляется
// Реализовано FC_NUM_READ попыток чтения/записи в инвертор
uint32_t devVaconFC::read_0x03_32(uint16_t cmd)
{
    uint8_t i;
    uint32_t result;
    if(!get_present() || state == ERR_LINK_FC) return 0; // выходим если нет инвертора или он заблокирован по ошибке
    for (i = 0; i < FC_NUM_READ; i++) // делаем FC_NUM_READ попыток чтения Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
    {
        err = Modbus.readHoldingRegisters32(FC_MODBUS_ADR, cmd - 1, (uint32_t *)&result); // Послать запрос, Нумерация регистров с НУЛЯ!!!!
        if(err == OK) break; // Прочитали удачно
        numErr++; // число ошибок чтение по модбасу
        journal.jprintf(pP_TIME, cErrorRS485, name, __FUNCTION__, err); // Выводим сообщение о повторном чтении
        _delay(FC_DELAY_REPEAT);
        //         journal.jprintf(pP_TIME,cErrorRS485,name,err);                     // Вывод кода ошибки в журнал
    }
    check_blockFC(); // проверить необходимость блокировки
    return result;
}

// Запись данных (2 байта) в регистр cmd возвращает код ошибки
// Реализовано FC_NUM_READ попыток чтения/записи в инвертор
int8_t devVaconFC::write_0x06_16(uint16_t cmd, uint16_t data)
{
    uint8_t i;
    if(!get_present() || state == ERR_LINK_FC) return err; // выходим если нет инвертора или он заблокирован по ошибке
    for (i = 0; i < FC_NUM_READ; i++) // делаем FC_NUM_READ попыток записи
    {
        err = Modbus.writeHoldingRegisters16(FC_MODBUS_ADR, cmd - 1, data); // послать запрос, Нумерация регистров с НУЛЯ!!!!
        if(err == OK) break; // Записали удачно
        numErr++; // число ошибок чтение по модбасу
        journal.jprintf(pP_TIME, cErrorRS485, name, __FUNCTION__, err); // Выводим сообщение о повторном чтении
        _delay(FC_DELAY_REPEAT);
        //        journal.jprintf(pP_TIME,cErrorRS485,name,err);                     // Вывод кода ошибки в журнал
    }
    check_blockFC(); // проверить необходимость блокировки
    return err;
}
#endif // FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ

#ifdef FC_ANALOG_CONTROL // Аналоговое управление
// Установить Отсчеты ЦАП соответсвующие 0   мощности
int8_t devVaconFC::set_level0(int16_t x)
{
    if((x >= 0) && (x <= 4096)) {
        level0 = x;
        return OK;
    } // Только правильные значения
    return WARNING_VALUE;
}
// Установить Отсчеты ЦАП соответсвующие 100 мощности
int8_t devVaconFC::set_level100(int16_t x)
{
    if((x >= 0) && (x <= 4096)) {
        level100 = x;
        return OK;
    } // Только правильные значения
    return WARNING_VALUE;
}
// Установить Минимальная мощность при котором частотник отключается (ограничение минимальной мощности)
int8_t devVaconFC::set_levelOff(int16_t x)
{
    if((x >= 0) && (x <= 100)) {
        levelOff = x;
        return OK;
    } // Только правильные значения
    return WARNING_VALUE;
}
#endif

// Возвращает текст ошибки по FT коду
const char* devVaconFC::get_fault_str(uint8_t fault)
{
    uint8_t i = 0;
    for (; i < sizeof(FC_Faults_code); i++) {
        if(FC_Faults_code[i] == fault) break;
    }
    return FC_Faults_str[i];
}
