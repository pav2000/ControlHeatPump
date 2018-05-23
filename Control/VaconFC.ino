/*
 * Частотный преобразователь Vacon 10
 * Адаптировано vad711, vad7@yahoo.com
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

 /*   
    flags = 0x00; // флаги  0 - наличие FC
	Uptime				= DEF_Uptime            ;
    PidFreqStep		= DEF_PidFreqStep     ;
    PidStop				= DEF_PidStop          ;
    dtCompTemp			= DEF_dtCompTemp      ;
    FC_DT_CON_PRESS			= DEF_FC_DT_CON_PRESS      ;
    startFreq			= DEF_startFreq        ;
    startFreq_BOILER	= DEF_startFreq_BOILER ;
    minFreq				= DEF_minFreq          ;
    minFreqCool		= DEF_minFreqCool     ;
    minFreqBoiler		= DEF_minFreqBoiler   ;
    minFreqUser		= DEF_minFreqUser     ;
    maxFreq				= DEF_maxFreq          ;
    maxFreqCool		= DEF_maxFreqCool     ;
    maxFreqBoiler		= DEF_maxFreqBoiler   ;
    maxFreqUser		= DEF_maxFreqUser     ;
    stepFreq			= DEF_stepFreq         ;
    stepFreqBoiler		= DEF_stepFreqBoiler  ;
    dtTemp				= DEF_dtTemp           ;
    dtTemp_BOILER		= DEF_dtTemp_BOILER    ;
    FC_MAX_POWER			= DEF_FC_MAX_POWER         ;
    FC_MAX_POWER_BOILER		= DEF_FC_MAX_POWER_BOILER  ;
    FC_MAX_CURRENT			= DEF_FC_MAX_CURRENT       ;
    FC_MAX_CURRENT_BOILER	= DEF_FC_MAX_CURRENT_BOILER;
    FC_ACCEL_TIME			= DEF_FC_ACCEL_TIME        ;
    FC_DEACCEL_TIME			= DEF_FC_DEACCEL_TIME      ;

#ifdef FC_ANALOG_CONTROL // Аналоговое управление
    pin = PIN_DEVICE_FC; // Ножка куда прицеплено FC
    dac = 0; // Текущее значение ЦАП
    analogWriteResolution(12); // разрешение ЦАП 12 бит;
    analogWrite(pin, dac);
    level0 = 0; // Отсчеты ЦАП соответсвующие 0   мощности
    level100 = 4096; // Отсчеты ЦАП соответсвующие 100 мощности
    levelOff = 10; // Минимальная мощность при котором частотник отключается (ограничение минимальной мощности)
//  SETBIT0(flags,fAnalog);           // Нет аналогового выхода
#endif
*/
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
	  _data.flags=0x00;                                // флаги  0 - наличие FC
	 

    SETBIT0(_data.flags, fAuto); // По умолчанию старт-стоп
    if(!Modbus.get_present()) // modbus отсутствует
    {
        SETBIT0(_data.flags, fFC); // Инвертор не работает
        journal.jprintf("%s, modbus not found, block.\n", name);
        err = ERR_NO_MODBUS;
        return err;
    } else if(DEVICEFC == true)
        SETBIT1(_data.flags, fFC); // наличие частотника в текушей конфигурации

    if(get_present())
        journal.jprintf("Invertor %s: present config\r\n", name);
    else {
        journal.jprintf("Invertor %s: none config\r\n", name);
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
    check_blockFC();
    if(err != OK) return err; // связи нет выходим
    journal.jprintf("Test link Modbus %s: OK\r\n", name); // Тест пройден

    uint8_t i = 3;
    while(state >= 0 && (state & FC_S_RUN)) // Если частотник работает то остановить его
    {
    	if(i-- == 0) break;
        stop_FC();
        journal.jprintf("Wait stop %s...\r\n", name);
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
#endif // #ifndef FC_ANALOG_CONTROL
    return err;
}

// Возвращает состояние, или ERR_LINK_FC, если нет связи
int16_t devVaconFC::CheckLinkStatus(void)
{
    //    if((!get_present())||(GETBIT(_data.flags,fErrFC))) return 0;                  // выходим если нет инвертора или он заблокирован по ошибке
    if(testMode == NORMAL || testMode == HARD_TEST){
		for (uint8_t i = 0; i < 3; i++) // Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
		{
			err = Modbus.readHoldingRegisters16(FC_MODBUS_ADR, FC_STATUS - 1, (uint16_t *)&state); // Послать запрос, Нумерация регистров с НУЛЯ!!!!
			if(err == OK) break; // Прочитали удачно
			_delay(FC_DELAY_READ);
		}
		check_blockFC(); // проверить необходимость блокировки
		if(err != OK) state = ERR_LINK_FC;
    } else {
    	state = 0;
    }
    return state;
}

// Прочитать (внутренние переменные обновляются) состояние Инвертора, возвращает или ОК или ошибку
// Вызывается из задачи чтения датчиков период FC_TIME_READ
int8_t devVaconFC::get_readState()
{
    uint8_t i;
    if((!get_present()) || (GETBIT(_data.flags, fErrFC))) return err; // выходим если нет инвертора или он заблокирован по ошибке
    err = OK;
    if(testMode != NORMAL && testMode != HARD_TEST){
    	state = 0;
    	FC_curr = FC;
    	return OK;
    }

#ifndef FC_ANALOG_CONTROL // Не аналоговое управление
    // Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
    for (i = 0; i < FC_NUM_READ; i++) // делаем FC_NUM_READ попыток чтения
    {
        state = read_0x03_16(FC_STATUS); // прочитать состояние
        err = Modbus.get_err(); // Скопировать ошибку
        if(err == OK) // Прочитано верно
        {
            if((GETBIT(_data.flags, fOnOff)) && (state != 3))
                continue;
            else
                break; // ТН включил компрессор а инвертор не имеет правильного состяния пытаемся прочитать еще один раз в проитвном случае все ок выходим
        }
        _delay(FC_DELAY_REPEAT);
        journal.jprintf(cErrorRS485, name, __FUNCTION__, err); // Выводим сообщение о повторном чтении
        numErr++; // число ошибок чтение по модбасу
        //journal.jprintf(pP_TIME,cErrorRS485,name,err);     // Вывод кода ошибки в журнал
    }
    if(err != OK) // Ошибка модбаса
    {
        state = ERR_LINK_FC; // признак потери связи с инвертором
        SETBIT1(_data.flags, fErrFC); // Блок инвертора
        set_Error(err, name); // генерация ошибки
        return err; // Возврат
    }
    // Состояние прочитали и оно правильное все остальное читаем
    _delay(FC_DELAY_READ);
    FC_curr = read_0x03_16(FC_SPEED); // прочитать текущую частоту
    err = Modbus.get_err(); // Скопировать ошибку
    if(err != OK) {
        state = ERR_LINK_FC;
    } // Ошибка выходим

    _delay(FC_DELAY_READ);
    power = read_0x03_16(FC_POWER); // прочитать мощность
    err = Modbus.get_err(); // Скопировать ошибку
    if(err != OK) {
        state = ERR_LINK_FC;
    } // Ошибка выходим

    _delay(FC_DELAY_READ);
    current = read_0x03_16(FC_CURRENT); // прочитать ток
    err = Modbus.get_err(); // Скопировать ошибку
    if(err != OK) {
        state = ERR_LINK_FC;
    } // Ошибка выходим
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
    err = OK;
#ifdef DEMO
    if((x >= _min) && (x <= _max)) // Проверка диапазона разрешенных частот
    {
        FC = x;
        if(show) journal.jprintf(" Set %s: %.2f\r\n", name, FC / 100.0);
        return err;
    } // установка частоты OK  - вывод сообщения если надо
    else {
        journal.jprintf("%s: Wrong frequency %.2f\n", name, x / 100.0);
        return WARNING_VALUE;
    }
#else // Боевой вариант
    uint8_t i;
    if((!get_present()) || (GETBIT(_data.flags, fErrFC))) return err; // выходим если нет инвертора или он заблокирован по ошибке
    if((x >= _min) && (x <= _max)) // Проверка диапазона разрешенных частот
    {
#ifndef FC_ANALOG_CONTROL // Не аналоговое управление
        // Запись в регистры инвертора установленной частоты
        if(testMode == NORMAL || testMode == HARD_TEST){
			for (i = 0; i < FC_NUM_READ; i++) // Делаем FC_NUM_READ попыток
			{
				err = write_0x06_16((uint16_t)FC_SET_SPEED, x);
				if(err == OK) break; // Команда выполнена
				_delay(100);
				journal.jprintf("%s: repeat set speed\n", name); // Выводим сообщение о повторной команде
			}
        }
        if(err == OK) {
            FC = x;
            if(show) journal.jprintf(" Set %s: %.2f %%\r\n", name, FC / 100.0);
            return err;
        } // установка частоты OK  - вывод сообщения если надо
        else {
            state = ERR_LINK_FC;
            SETBIT1(_data.flags, fErrFC);
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
            journal.jprintf(" Set %s: %.2f %%\r\n", name, FC / 100.0); // установка частоты OK  - вывод сообщения если надо
#endif
        return err;
    } // if((x>=_min)&&(x<=_max))
    else {
        journal.jprintf("%s: Wrong speed %.2f\n", name, x / 100.0);
        return WARNING_VALUE;
    }
#endif // DEMO
}

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

// Записать настройки в eeprom i2c на входе адрес с какого, на выходе конечный адрес, если число меньше 0 это код ошибки
int32_t devVaconFC::save(int32_t adr)
{
/*	
    if(writeEEPROM_I2C(adr, (byte*)&Uptime, (byte*)&setup_flags - (byte*)&Uptime + sizeof(setup_flags))) {
        set_Error(ERR_SAVE_EEPROM, name);
        return ERR_SAVE_EEPROM;
    }
    adr += (byte*)&setup_flags - (byte*)&Uptime + sizeof(setup_flags);
 */
 	if (writeEEPROM_I2C(adr, (byte*)&_data, sizeof(_data))) {set_Error(ERR_SAVE_EEPROM,(char*)name); return ERR_SAVE_EEPROM;}  adr=adr+sizeof(_data);     
    return adr;
}

// Считать настройки из eeprom i2c на входе адрес с какого, на выходе конечный адрес, если число меньше 0 это код ошибки
int32_t devVaconFC::load(int32_t adr)
{
/*	
    if(readEEPROM_I2C(adr, (byte*)&Uptime, (byte*)&setup_flags - (byte*)&Uptime + sizeof(setup_flags))) {
        set_Error(ERR_LOAD_EEPROM, name);
        return ERR_SAVE_EEPROM;
    }
    adr += (byte*)&setup_flags - (byte*)&Uptime + sizeof(setup_flags);
 */
   if (readEEPROM_I2C(adr, (byte*)&_data, sizeof(_data))) { set_Error(ERR_LOAD_EEPROM,(char*)name); return ERR_LOAD_EEPROM;}  adr=adr+sizeof(_data);              
  // SETBIT1(_data.flags, fPresent); 							   
    return adr;
}
// Считать настройки из буфера на входе адрес с какого, на выходе конечный адрес, число меньше 0 это код ошибки
int32_t devVaconFC::loadFromBuf(int32_t adr, byte* buf)
{
 //   memcpy((byte*)&Uptime, buf + adr, (byte*)&setup_flags - (byte*)&Uptime + sizeof(setup_flags));
 //   adr += (byte*)&setup_flags - (byte*)&Uptime + sizeof(setup_flags);
 memcpy((byte*)&_data,buf+adr,sizeof(_data)); adr=adr+sizeof(_data); 	
    return adr;
}
// Рассчитать контрольную сумму для данных на входе входная сумма на выходе новая
uint16_t devVaconFC::get_crc16(uint16_t crc)
{
	uint16_t i;
//	for(i = 0; i < (byte*)&setup_flags - (byte*)&Uptime + sizeof(setup_flags); i++)
//		crc = _crc16(crc,*((byte*)&Uptime + i));  // CRC16 структуры
    for(i=0;i<sizeof(_data);i++) crc=_crc16(crc,*((byte*)&_data+i));   
	return crc;
}

// Установить запрет на использование инвертора если лимит ошибок исчерпан
void devVaconFC::check_blockFC()
{
#ifndef FC_ANALOG_CONTROL // Не аналоговое управление
    if((xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) && (err != OK)) // если не запущена free rtos то блокируем с первого раза
    {
        SETBIT1(_data.flags, fErrFC); // Установить флаг
        note = (char*)noteFC_NO;
        set_Error(err, (char*)name); // Подъем ошибки на верх и останов ТН
        return;
    }
    if(err != OK)
        number_err++;
    else {
    	SETBIT0(_data.flags, fErrFC);
        number_err = 0;
        note = (char*)noteFC_OK; // Описание инвертора есть
        return;
    } // Увеличить счетчик ошибок
    if(number_err > FC_NUM_READ) // если привышено число ошибок то блокировка
    {
        //SemaphoreGive(xModbusSemaphore); // разблокировать семафор
        SETBIT1(_data.flags, fErrFC); // Установить флаг
        note = (char*)noteFC_NO;
        set_Error(err, (char*)name); // Подъем ошибки на верх и останов ТН
    }
#endif
}

// Получить флаг блокировки инвертора
boolean devVaconFC::get_blockFC()
{
	return GETBIT(_data.flags, fErrFC);
}

// Установить значение текущий режим работы
void  devVaconFC::set_testMode(TEST_MODE t)
{
	testMode = t;
	if(t == SAFE_TEST || t == TEST) {
		SETBIT0(_data.flags, fErrFC);
		err = OK;
	} else {
	    //CheckLinkStatus(); // проверка связи с инвертором
	}
}

// Команда ход на инвертор (целевая скорость НЕ ВЫСТАВЛЯЕТСЯ)
// Может быть подана команда через реле и через модбас в зависимости от ключей компиляции
int8_t devVaconFC::start_FC()
{
    if(((testMode == NORMAL) || (testMode == HARD_TEST))) {
    	if(!get_present() || GETBIT(_data.flags, fErrFC)) return err; // выходим если нет инвертора или он заблокирован по ошибке
    	if(FC < _data.minFreq || FC > _data.maxFreq) {
    		journal.jprintf(" %s: Wrong frequency, ignore start\n", name);
    		return err;
    	}
    } // проверка частоты не в режиме теста
    err = OK;
#ifndef FC_ANALOG_CONTROL // Не аналоговое управление
#ifdef DEMO
#ifdef FC_USE_RCOMP // Использовать отдельный провод для команды ход/стоп
    HP.dRelay[RCOMP].set_ON(); // ПЛОХО через глобальную переменную
#endif // FC_USE_RCOMP
    if(err == OK) {
        SETBIT1(_data.flags, fOnOff);
        startCompressor = rtcSAM3X8.unixtime();
        journal.jprintf(" %s ON\n", name);
    }
    else {
        state = ERR_LINK_FC;
        SETBIT1(_data.flags, fErrFC);
        set_Error(err, name);
    } // генерация ошибки
#else // DEMO
    // Боевая часть
    // set_targetFreq(FC_START_FC,true);  // Запись в регистр инвертора стартовой частоты  НЕ всегда скорость стартовая - супербойлер
    if((testMode == NORMAL) || (testMode == HARD_TEST)) //   Режим работа и хард тест, все включаем,
    {
#ifdef FC_USE_RCOMP // Использовать отдельный провод для команды ход/стоп
        HP.dRelay[RCOMP].set_ON(); // ПЛОХО через глобальную переменную
#endif
        err = write_0x06_16(FC_CONTROL, FC_C_RUN); // Команда Ход
    }
    if(err == OK) {
        SETBIT1(_data.flags, fOnOff);
        startCompressor = rtcSAM3X8.unixtime();
        journal.jprintf(" %s ON\n", name);
    } else {
        state = ERR_LINK_FC;
        SETBIT1(_data.flags, fErrFC);
        set_Error(err, name);
    } // генерация ошибки
#endif
#else //  FC_ANALOG_CONTROL
#ifdef DEMO
#ifdef FC_USE_RCOMP // Использовать отдельный провод для команды ход/стоп
    HP.dRelay[RCOMP].set_ON(); // ПЛОХО через глобальную переменную
#endif // FC_USE_RCOMP
    SETBIT1(_data.flags, fOnOff);
    startCompressor = rtcSAM3X8.unixtime();
    journal.jprintf(" %s ON\n", name);
#else // DEMO
    // Боевая часть
    if((testMode == NORMAL) || (testMode == HARD_TEST)) //   Режим работа и хард тест, все включаем,
    {
#ifdef FC_USE_RCOMP // Использовать отдельный провод для команды ход/стоп
        HP.dRelay[RCOMP].set_ON(); // ПЛОХО через глобальную переменную
#endif
        state = ERR_LINK_FC;
        err = ERR_FC_CONF_ANALOG;
        SETBIT1(_data.flags, fErrFC);
        set_Error(err, name); // Ошибка конфигурации
    }
    SETBIT1(_data.flags, fOnOff);
    startCompressor = rtcSAM3X8.unixtime();
    journal.jprintf(" %s ON\n", name);
#endif
#endif
    return err;
}

// Команда стоп на инвертор Обратно код ошибки
int8_t devVaconFC::stop_FC()
{
    if(((testMode == NORMAL) || (testMode == HARD_TEST)) && (((!get_present()) || (GETBIT(_data.flags, fErrFC)))))
        return err; // выходим если нет инвертора или он заблокирован по ошибке
    err = OK;
#ifndef FC_ANALOG_CONTROL // Не аналоговое управление
#ifdef DEMO
#ifdef FC_USE_RCOMP // Использовать отдельный провод для команды ход/стоп
    HP.dRelay[RCOMP].set_OFF(); // ПЛОХО через глобальную переменную
#endif // FC_USE_RCOMP
    if(err == OK) {
        SETBIT0(_data.flags, fOnOff);
        startCompressor = 0;
        journal.jprintf(" %s OFF\n", name);
    }
    else {
        state = ERR_LINK_FC;
        SETBIT1(_data.flags, fErrFC);
        set_Error(err, name);
    } // генерация ошибки
#else // DEMO
    if((testMode == NORMAL) || (testMode == HARD_TEST)) // Режим работа и хард тест, все включаем,
    {
#ifdef FC_USE_RCOMP // Использовать отдельный провод для команды ход/стоп
        HP.dRelay[RCOMP].set_OFF(); // ПЛОХО через глобальную переменную
#endif // подать команду ход/стоп через модбас
        err = write_0x06_16(FC_CONTROL, FC_C_STOP); // Команда стоп
    }
    if(err == OK) {
        SETBIT0(_data.flags, fOnOff);
        startCompressor = 0;
        journal.jprintf(" %s OFF\n", name);
    }
    else {
        state = ERR_LINK_FC;
        SETBIT1(_data.flags, fErrFC);
        set_Error(err, name);
    } // генерация ошибки
#endif
#else // FC_ANALOG_CONTROL
#ifdef DEMO
#ifdef FC_USE_RCOMP // Использовать отдельный провод для команды ход/стоп
    HP.dRelay[RCOMP].set_OFF(); // ПЛОХО через глобальную переменную
#endif // FC_USE_RCOMP
    SETBIT0(_data.flags, fOnOff);
    startCompressor = 0;
    journal.jprintf(" %s OFF\n", name);
#else // DEMO
    if((testMode == NORMAL) || (testMode == HARD_TEST)) // Режим работа и хард тест, все включаем,
    {
#ifdef FC_USE_RCOMP // Использовать отдельный провод для команды ход/стоп
        HP.dRelay[RCOMP].set_OFF(); // ПЛОХО через глобальную переменную
#else // подать команду ход/стоп через модбас
        state = ERR_LINK_FC;
        err = ERR_FC_CONF_ANALOG;
        SETBIT1(_data.flags, fErrFC);
        set_Error(err, name); // Ошибка конфигурации
#endif
    }
    SETBIT0(_data.flags, fOnOff);
    startCompressor = 0;
    journal.jprintf(" %s OFF\n", name);
#endif
#endif // FC_ANALOG_CONTROL
    return err;
}


// Получить параметр инвертора в виде строки, результат ДОБАВЛЯЕТСЯ в ret
char* devVaconFC::get_paramFC(char *var,char *ret)
{
static char temp[12];
    if(strcmp(var,fc_ON_OFF)==0)                { if (GETBIT(_data.flags,fOnOff)) return strcat(ret,(char*)cOne);else return strcat(ret,(char*)cZero); } else 
    if(strcmp(var,fc_INFO)==0)                  {
    	                                        #ifndef FC_ANALOG_CONTROL  
    	                                        get_infoFC(ret); return ret;
    	                                        #else
                                                return strcat(ret, "|Данные не доступны, работа через аналоговый вход|;") ;
                                                #endif              
    	                                        } else
    if(strcmp(var,fc_NAME)==0)                  { return strcat(ret,name);             } else
    if(strcmp(var,fc_NOTE)==0)                  { return strcat(ret,note);             } else
    if(strcmp(var,fc_PIN)==0)                   { return strcat(ret,int2str(pin));     } else 
    if(strcmp(var,fc_PRESENT)==0)               { if (GETBIT(_data.flags,fFC)) return strcat(ret,(char*)cOne);else return strcat(ret,(char*)cZero); } else 
    if(strcmp(var,fc_STATE)==0)                 { return strcat(ret,int2str(state));   } else 
    if(strcmp(var,fc_FC)==0)                    { return strcat(ret,ftoa(temp,(float)FC/100.0,2)); } else
    if(strcmp(var,fc_cFC)==0)                   { return strcat(ret,ftoa(temp,(float)FC_curr/100.0,2)); } else
    if(strcmp(var,fc_cPOWER)==0)                { return strcat(ret,ftoa(temp,(float)power/10.0,1)); } else 
    if(strcmp(var,fc_cCURRENT)==0)              { return strcat(ret,ftoa(temp,(float)current/100.0,2)); } else
    if(strcmp(var,fc_AUTO)==0)                  { if (GETBIT(_data.flags,fAuto)) return strcat(ret,(char*)cOne);else return strcat(ret,(char*)cZero); } else
    if(strcmp(var,fc_ANALOG)==0)                { // Флаг аналогового управления
		                                        #ifdef FC_ANALOG_CONTROL                                                    
		                                        return strcat(ret,(char*)cOne);
		                                        #else
		                                        return strcat(ret,(char*)cZero);
		                                        #endif
                                                } else
    if(strcmp(var,fc_DAC)==0)                   { return strcat(ret,int2str(dac));          } else 
    #ifdef FC_ANALOG_CONTROL
    if(strcmp(var,fc_LEVEL0)==0)                { return strcat(ret,int2str(level0));       } else 
    if(strcmp(var,fc_LEVEL100)==0)              { return strcat(ret,int2str(level100));     } else 
    if(strcmp(var,fc_LEVELOFF)==0)              { return strcat(ret,int2str(levelOff));     } else 
    #endif
    if(strcmp(var,fc_BLOCK)==0)                 { if (GETBIT(_data.flags,fErrFC)) return strcat(ret,(char*)cOne);else return strcat(ret,(char*)cZero); } else
    if(strcmp(var,fc_ERROR)==0)                 { return strcat(ret,int2str(err));          } else                                    
    if(strcmp(var,fc_UPTIME)==0)                { return strcat(ret,int2str(_data.Uptime)); } else   // вывод в секундах
    if(strcmp(var,fc_PID_FREQ_STEP)==0)         { return strcat(ret,ftoa(temp,(float)_data.PidFreqStep/100.0,2)); } else // Гц
    if(strcmp(var,fc_PID_STOP)==0)              { return strcat(ret,int2str(_data.PidStop));          } else 
    if(strcmp(var,fc_DT_COMP_TEMP)==0)          { return strcat(ret,ftoa(temp,(float)_data.dtCompTemp/100.0,2)); } else // градусы
    if(strcmp(var,fc_START_FREQ)==0)            { return strcat(ret,ftoa(temp,(float)_data.startFreq/100.0,2)); } else // Гц
    if(strcmp(var,fc_START_FREQ_BOILER)==0)     { return strcat(ret,ftoa(temp,(float)_data.startFreqBoiler/100.0,2)); } else // Гц
    if(strcmp(var,fc_MIN_FREQ)==0)              { return strcat(ret,ftoa(temp,(float)_data.minFreq/100.0,2)); } else // Гц
    if(strcmp(var,fc_MIN_FREQ_COOL)==0)         { return strcat(ret,ftoa(temp,(float)_data.minFreqCool/100.0,2)); } else // Гц
    if(strcmp(var,fc_MIN_FREQ_BOILER)==0)       { return strcat(ret,ftoa(temp,(float)_data.minFreqBoiler/100.0,2)); } else // Гц
    if(strcmp(var,fc_MIN_FREQ_USER)==0)         { return strcat(ret,ftoa(temp,(float)_data.minFreqUser/100.0,2)); } else // Гц
    if(strcmp(var,fc_MAX_FREQ)==0)              { return strcat(ret,ftoa(temp,(float)_data.maxFreq/100.0,2)); } else // Гц 
    if(strcmp(var,fc_MAX_FREQ_COOL)==0)         { return strcat(ret,ftoa(temp,(float)_data.maxFreqCool/100.0,2)); } else // Гц 
    if(strcmp(var,fc_MAX_FREQ_BOILER)==0)       { return strcat(ret,ftoa(temp,(float)_data.maxFreqBoiler/100.0,2)); } else // Гц 
    if(strcmp(var,fc_MAX_FREQ_USER)==0)         { return strcat(ret,ftoa(temp,(float)_data.maxFreqUser/100.0,2)); } else // Гц 
    if(strcmp(var,fc_STEP_FREQ)==0)             { return strcat(ret,ftoa(temp,(float)_data.stepFreq/100.0,2)); } else // Гц 
    if(strcmp(var,fc_STEP_FREQ_BOILER)==0)      { return strcat(ret,ftoa(temp,(float)_data.stepFreqBoiler/100.0,2)); } else // Гц 
    if(strcmp(var,fc_DT_TEMP)==0)               { return strcat(ret,ftoa(temp,(float)_data.dtTemp/100.0,2)); } else // градусы
    if(strcmp(var,fc_DT_TEMP_BOILER)==0)        { return strcat(ret,ftoa(temp,(float)_data.dtTempBoiler/100.0,2)); } else // градусы
    return strcat(ret,(char*)cInvalid);  
}
/*
// Получить параметр инвертора в виде строки
char* devVaconFC::get_paramFC(TYPE_PARAM_FC p)
{
    static char temp[12];
    switch (p) {
    case pON_OFF:
        if(GETBIT(flags, fOnOff))
            return (char*)cOne;
        else
            return (char*)cZero;
        break; // Флаг включения выключения
    case pMIN_FC:
        return ftoa(temp, (float)minFreq / 100.0, 2);
        break; // Только чтение минимальная % работы
    case pMAX_FC:
        return ftoa(temp, (float)maxFreq / 100.0, 2);
        break; // Только чтение максимальная % работы
    case pSTART_FC:
        return ftoa(temp, (float)startFreq / 100.0, 2);
        break; // Только чтение стартовая % работы
    case pMAX_POWER:
        return ftoa(temp, (float)FC_MAX_POWER / 10.0, 1);
        break; // Только чтение максимальная мощность
    case pSTATE:
        return int2str(state);
        break; // Только чтение Состояние ПЧ
    case pFC:
        return ftoa(temp, (float)FC / 100.0, 2);
        break; // текущая скорость ПЧ
    case pPOWER:
        return ftoa(temp, (float)power / 10.0, 1);
        break; // Текущая мощность
    case pAUTO_FC:
        if(GETBIT(flags, fAuto))
            return (char*)cOne;
        else
            return (char*)cZero;
        break; // Флаг автоматической подбора частоты
    //    case   pANALOG:      if(GETBIT(flags,fAnalog)) return(char*)cOne;else return(char*)cZero;break; // Флаг аналогового управления
    case pANALOG: // Флаг аналогового управления
#ifdef FC_ANALOG_CONTROL
        return (char*)cOne;
#else
        return (char*)cZero;
#endif
        break;
#ifdef FC_ANALOG_CONTROL // Аналоговое управление
    case pLEVEL0:
        return int2str(level0);
        break; // Уровень частоты 0 в отсчетах ЦАП
    case pLEVEL100:
        return int2str(level100);
        break; // Уровень частоты 100% в  отсчетах ЦАП
    case pLEVELOFF:
        return int2str(levelOff);
        break; // Уровень частоты в % при отключении
#endif
    case pSTOP_FC:
        if(GETBIT(flags, fErrFC))
            return (char*)"Block";
        else
            return (char*)"No";
        break; // флаг глобальная ошибка инвертора - работа инвертора запрещена
    case pERROR_FC:
        return int2str(err);
        break; // Получить код ошибки
    default:
        return (char*)cInvalid;
        break;
    }
    return (char*)cInvalid;
}
*/

// Установить параметр инвертора из строки
boolean devVaconFC::set_paramFC(char *var, float x)
{
    if(strcmp(var,fc_ON_OFF)==0)                { if (x==0) stop_FC();else start_FC();return true;  } else 
    if(strcmp(var,fc_INFO)==0)                  { return true;                         } else  // только чтение
    if(strcmp(var,fc_NAME)==0)                  { return true;                         } else  // только чтение
    if(strcmp(var,fc_NOTE)==0)                  { return true;                         } else  // только чтение
    if(strcmp(var,fc_PIN)==0)                   { return true;                         } else  // только чтение
    if(strcmp(var,fc_PRESENT)==0)               { return true;                         } else  // только чтение
    if(strcmp(var,fc_STATE)==0)                 { return true;                         } else  // только чтение
    if(strcmp(var,fc_FC)==0)                    { if((x*100>=_data.minFreqUser)&&(x*100<=_data.maxFreqUser)){set_targetFreq(x*100,true, _data.minFreqUser, _data.maxFreqUser); return true; } else return false; } else
    if(strcmp(var,fc_cFC)==0)                   { return true;                         } else  // только чтение
    if(strcmp(var,fc_cPOWER)==0)                { return true;                         } else  // только чтение
    if(strcmp(var,fc_cCURRENT)==0)              { return true;                         } else  // только чтение
    if(strcmp(var,fc_AUTO)==0)                  { if (x==0) SETBIT0(_data.flags,fAuto);else SETBIT1(_data.flags,fAuto);return true;  } else
    if(strcmp(var,fc_ANALOG)==0)                { return true;                         } else  // только чтение
    if(strcmp(var,fc_DAC)==0)                   { return true;                         } else  // только чтение
    #ifdef FC_ANALOG_CONTROL
    if(strcmp(var,fc_LEVEL0)==0)                { if ((x>=0)&&(x<=4096)) { level0=x; return true;} else return false;      } else 
    if(strcmp(var,fc_LEVEL100)==0)              { if ((x>=0)&&(x<=4096)) { level100=x; return true;} else return false;    } else 
    if(strcmp(var,fc_LEVELOFF)==0)              { if ((x>=0)&&(x<=4096)) { levelOff=x; return true;} else return false;    } else 
    #endif
    if(strcmp(var,fc_BLOCK)==0)                 { SemaphoreGive(xModbusSemaphore); // отдать семафор ВСЕГДА  
                                                if (x==0) { SETBIT0(_data.flags,fErrFC); note=(char*)noteFC_OK; }
                                                else      { SETBIT1(_data.flags,fErrFC); note=(char*)noteFC_NO; }
                                                return true;            
                                                } else  // только чтение
    if(strcmp(var,fc_ERROR)==0)                 { return true;                         } else  // только чтение                                      
    if(strcmp(var,fc_UPTIME)==0)                { if((x>=3)&&(x<65)){_data.Uptime=x;return true; } else return false; } else   // хранение в сек
    if(strcmp(var,fc_PID_FREQ_STEP)==0)         { if((x>0)&&(x<5)){_data.PidFreqStep=x*100;return true; } else return false; } else // Гц
    if(strcmp(var,fc_PID_STOP)==0)              { if((x>50)&&(x<100)){_data.PidStop=x;return true; } else return false;  } else 
    if(strcmp(var,fc_DT_COMP_TEMP)==0)          { if((x>1)&&(x<25)){_data.dtCompTemp=x*100;return true; } else return false; } else // градусы
    if(strcmp(var,fc_START_FREQ)==0)            { if((x>20)&&(x<120)){_data.startFreq=x*100;return true; } else return false; } else // Гц
    if(strcmp(var,fc_START_FREQ_BOILER)==0)     { if((x>20)&&(x<140)){_data.startFreqBoiler=x*100;return true; } else return false; } else // Гц
    if(strcmp(var,fc_MIN_FREQ)==0)              { if((x>20)&&(x<80)){_data.minFreq=x*100;return true; } else return false; } else // Гц
    if(strcmp(var,fc_MIN_FREQ_COOL)==0)         { if((x>20)&&(x<80)){_data.minFreqCool=x*100;return true; } else return false; } else // Гц
    if(strcmp(var,fc_MIN_FREQ_BOILER)==0)       { if((x>20)&&(x<80)){_data.minFreqBoiler=x*100;return true; } else return false; } else // Гц
    if(strcmp(var,fc_MIN_FREQ_USER)==0)         { if((x>20)&&(x<80)){_data.minFreqUser=x*100;return true; } else return false; } else // Гц
    if(strcmp(var,fc_MAX_FREQ)==0)              { if((x>40)&&(x<200)){_data.maxFreq=x*100;return true; } else return false; } else // Гц 
    if(strcmp(var,fc_MAX_FREQ_COOL)==0)         { if((x>40)&&(x<200)){_data.maxFreqCool=x*100;return true; } else return false; } else // Гц 
    if(strcmp(var,fc_MAX_FREQ_BOILER)==0)       { if((x>40)&&(x<200)){_data.maxFreqBoiler=x*100;return true; } else return false; } else // Гц 
    if(strcmp(var,fc_MAX_FREQ_USER)==0)         { if((x>40)&&(x<200)){_data.maxFreqUser=x*100;return true; } else return false; } else // Гц 
    if(strcmp(var,fc_STEP_FREQ)==0)             { if((x>0.2)&&(x<10)){_data.stepFreq=x*100;return true; } else return false; } else // Гц 
    if(strcmp(var,fc_STEP_FREQ_BOILER)==0)      { if((x>0.2)&&(x<10)){_data.stepFreqBoiler=x*100;return true; } else return false; } else // Гц
    if(strcmp(var,fc_DT_TEMP)==0)               { if((x>0)&&(x<10)){_data.dtTemp=x*100;return true; } else return false; } else // градусы
    if(strcmp(var,fc_DT_TEMP_BOILER)==0)        { if((x>0)&&(x<10)){_data.dtTempBoiler=x*100;return true; } else return false; } else // градусы
    return false;
}
/*
// Установить параметр инвертора из строки
boolean devVaconFC::set_paramFC(TYPE_PARAM_FC p, float x)
{
    switch (p) {
    case pON_OFF:
        if(x == 0)
            stop_FC();
        else
            start_FC();
        return true;
        break; // Флаг включения выключения Включение-вуключение инвертора
    case pMIN_FC:
        return true;
        break; // Только чтение минимальная скорость работы
    case pMAX_FC:
        return true;
        break; // Только чтение максимальная скорость работы
    case pSTART_FC:
        return true;
        break; // Только чтение стартовая скорость работы
    case pMAX_POWER:
        return true;
        break; // Только чтение максимальная мощность
    case pSTATE:
        return true;
        break; // Только чтение Состояние ПЧ
    case pFC:
    	return set_targetFreq(x * 100 + 0.005, true, minFreqUser, maxFreqUser) == OK;
    case pPOWER:
        return true;
        break; // Только чтение Текущая мощность
    case pAUTO_FC:
        if(x == 0)
            SETBIT0(flags, fAuto);
        else
            SETBIT1(flags, fAuto);
        return true;
        break; // Флаг автоматической подбора скорости
    //  case   pANALOG:      if(x==0) SETBIT0(flags,fAnalog);else SETBIT1(flags,fAnalog);return true;break; // Флаг аналогового управления
    case pANALOG:
        return true;
        break; // ТОЛЬКО ЧТЕНИЕ Флаг аналогового управления
#ifdef FC_ANALOG_CONTROL // Аналоговое управление
        case pLEVEL0:
        if((x >= 0) && (x <= 4096)) {
            level0 = x;
            return true;
        } // Только правильные значения
        return false;
        break; // Уровень скорости 0 в отсчетах ЦАП
    case pLEVEL100:
        if((x >= 0) && (x <= 4096)) {
            level100 = x;
            return true;
        } // Только правильные значения
        return false;
        break; // Уровень скорости 100% в  отсчетах ЦАП
    case pLEVELOFF:
        if((x >= 0) && (x <= 4096)) {
            levelOff = x;
            return true;
        } // Только правильные значения
        return false;
        break; // Уровень скорости в % при отключении
#endif
    case pSTOP_FC:
        SemaphoreGive(xModbusSemaphore); // отдать семафор ВСЕГДА
        if(x == 0) {
            SETBIT0(flags, fErrFC);
            note = (char*)noteFC_OK;
        }
        else {
            SETBIT1(flags, fErrFC);
            note = (char*)noteFC_NO;
        }
        return true;
        break; // флаг глобальная ошибка инвертора - работа инвертора запрещена
    case pERROR_FC:
        return true;
        break; // Код ошибки тлько чтение
    default:
        return false;
        break;
    }
    return false;
}
*/
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
		if(HP.dFC.get_blockFC()) {   // Инвертор заблокирован
			strcat(buf, "|Данные не доступны (инвертор заблокирован)|;");
		} else {
			uint32_t i;
			strcat(buf, "2101|Состояние инвертора: ");
			get_infoFC_status(buf + m_strlen(buf), i = read_0x03_16(FC_STATUS));
			buf += m_snprintf(buf += m_strlen(buf), 256, "|%Xh;", i);
			if(err == OK) {
				_delay(FC_DELAY_READ);
				i = read_0x03_32(FC_SPEED); // +FC_FREQ (low word)
				buf += m_snprintf(buf, 256, "2103|Выходная скорость (%%)|%.2f;V1.1 (2104)|Выходная частота (Гц)|%.2f;", (float)(i >> 16) / 100.0, (float)(i & 0xFFFF) / 100.0);
				_delay(FC_DELAY_READ);
				i = read_0x03_32(FC_RPM); // +FC_CURRENT (low word)
				buf += m_snprintf(buf, 256, "V1.3 (2105)|Обороты (об/м)|%d;V1.4 (2106)|Выходной ток (А)|%.2f;", i >> 16, (float)(i & 0xFFFF) / 100.0);
				_delay(FC_DELAY_READ);
				buf += m_snprintf(buf, 256, "V1.5 (2107)|Крутящий момент (%%)|%.1f;", (float)read_0x03_16(FC_TORQUE) / 10.0);
				_delay(FC_DELAY_READ);
				i = read_0x03_32(FC_VOLTAGE); // +FC_VOLTATE_DC (low word)
				buf += m_snprintf(buf, 256, "V1.7 (2109)|Выходное напряжение (В)|%.1f;V1.8 (2110)|Напряжения шины постоянного тока (В)|%d;", (float)(i >> 16) / 10.0, i & 0xFFFF);
				_delay(FC_DELAY_READ);
				buf += m_snprintf(buf, 256, "V1.9 (8)|Температура радиатора (°С)|%d;", read_tempFC());
				_delay(FC_DELAY_READ);
				i = read_0x03_32(FC_POWER_DAYS); // +FC_POWER_HOURS (low word)
				buf += m_snprintf(buf, 256, "V3.3 (828)|Время включения (дней:часов)|%d:%d;", i >> 16, i & 0xFFFF);
				_delay(FC_DELAY_READ);
				i = read_0x03_32(FC_RUN_DAYS); // +FC_RUN_HOURS (low word)
				buf += m_snprintf(buf, 256, "V3.5 (840)|Время работы компрессора (дней:часов)|%d:%d;", i >> 16, i & 0xFFFF);
				_delay(FC_DELAY_READ);
				buf += m_snprintf(buf, 256, "V3.6 (842)|Счетчик аварийных отключений|%d;", read_0x03_16(FC_NUM_FAULTS));
				_delay(FC_DELAY_READ);

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
		m_snprintf(buf, 256, "-|Режим тестирования...|;");
	}
}
// Сброс ошибок инвертора по модбасу
boolean devVaconFC::reset_errorFC()
{
#ifndef FC_ANALOG_CONTROL // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
    if(write_0x06_16(FC_CONTROL, FC_C_RST)) { // задать режим иннициализации - стирание ошибок
        journal.jprintf("Error reset %s\r\n", name);
    }
#endif
    return err == OK;
}

// Сброс состояния связи инвертора через модбас
boolean devVaconFC::reset_FC()
{
#ifndef FC_ANALOG_CONTROL // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
    if(write_0x06_16(FC_COMM_ST_RESET, 1))
        journal.jprintf("Error reset comm. %s\r\n", name);
    else
        journal.jprintf("Reset FC comm. status %s\r\n", name);
#endif
    return err == OK;
}

// Текущее состояние инвертора
int16_t devVaconFC::read_stateFC()
{
#ifndef FC_ANALOG_CONTROL // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ
    return read_0x03_16(FC_STATUS);
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
    err = OK;
    if((!get_present()) || (GETBIT(_data.flags, fErrFC))) return 0; // выходим если нет инвертора или он заблокирован по ошибке

    for (i = 0; i < FC_NUM_READ; i++) // делаем FC_NUM_READ попыток чтения Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
    {
        err = Modbus.readHoldingRegisters16(FC_MODBUS_ADR, cmd - 1, (uint16_t *)&result); // Послать запрос, Нумерация регистров с НУЛЯ!!!!
        if(err == OK) break; // Прочитали удачно
        _delay(FC_DELAY_REPEAT);
        journal.jprintf("Modbus reg #%d - ", cmd);
        journal.jprintf(pP_TIME, cErrorRS485, name, __FUNCTION__, err); // Выводим сообщение о повторном чтении
        numErr++; // число ошибок чтение по модбасу
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
    err = OK;
    if((!get_present()) || (GETBIT(_data.flags, fErrFC))) return 0; // выходим если нет инвертора или он заблокирован по ошибке
    for (i = 0; i < FC_NUM_READ; i++) // делаем FC_NUM_READ попыток чтения Чтение состояния инвертора, при ошибке генерация общей ошибки ТН и останов
    {
        err = Modbus.readHoldingRegisters32(FC_MODBUS_ADR, cmd - 1, (uint32_t *)&result); // Послать запрос, Нумерация регистров с НУЛЯ!!!!
        if(err == OK) break; // Прочитали удачно
        _delay(FC_DELAY_REPEAT);
        journal.jprintf(pP_TIME, cErrorRS485, name, __FUNCTION__, err); // Выводим сообщение о повторном чтении
        numErr++; // число ошибок чтение по модбасу
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
    err = OK;
    if((!get_present()) || (GETBIT(_data.flags, fErrFC))) return err; // выходим если нет инвертора или он заблокирован по ошибке
    for (i = 0; i < FC_NUM_READ; i++) // делаем FC_NUM_READ попыток записи
    {
        err = Modbus.writeHoldingRegisters16(FC_MODBUS_ADR, cmd - 1, data); // послать запрос, Нумерация регистров с НУЛЯ!!!!
        if(err == OK) break; // Записали удачно
        _delay(FC_DELAY_REPEAT);
        journal.jprintf(pP_TIME, cErrorRS485, name, __FUNCTION__, err); // Выводим сообщение о повторном чтении
        numErr++; // число ошибок чтение по модбасу
        //        journal.jprintf(pP_TIME,cErrorRS485,name,err);                     // Вывод кода ошибки в журнал
    }
    check_blockFC(); // проверить необходимость блокировки
    return err;
}
#endif // FC_ANALOG_CONTROL    // НЕ АНАЛОГОВОЕ УПРАВЛЕНИЕ

// Возвращает текст ошибки по FT коду
const char* devVaconFC::get_fault_str(uint8_t fault)
{
    uint8_t i = 0;
    for (; i < sizeof(FC_Faults_code); i++) {
        if(FC_Faults_code[i] == fault) break;
    }
    return FC_Faults_str[i];
}
