/*
 * Copyright (c) 2016-2020 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
 * &                       by Vadim Kulakov vad7@yahoo.com, vad711
 * "Народный контроллер" для тепловых насосов.
 * Данное програмное обеспечение предназначено для управления
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
#include "Nextion.h"

// Команды и строки дисплея (экономим место)
// В NEXTION надо посылать сроки в кодировке iso8859-5 перевод http://codepage-encoding.online-domain-tools.com/
const char *_YES_8859 = { "\xB4\xB0" };      // ДА
const char *_NO_8859 = { "\xBD\xB5\xC2" };   // НЕТ
const char *_HP_OFF_8859 = { "\xc2\xbd\x20\xd2\xeb\xda\xdb\xee\xe7\xd5\xdd\x0d\x0a" }; // ТН выключен
const char *_xB0 = { "\xB0" }; // °
#define COMM_END_B 0xFF
const char comm_end[3] = { COMM_END_B, COMM_END_B, COMM_END_B };

#define NXTID_PAGE_MAIN		0x00
#define NXTID_PAGE_MENU		0x01
#define NXTID_PAGE_NETWORK	0x02
#define NXTID_PAGE_SYSTEM	0x03
#define NXTID_PAGE_SCHEME	0x04
#define NXTID_PAGE_HEAT		0x05
#define NXTID_PAGE_BOILER	0x06
#define NXTID_PAGE_INFO		0x07
#define NXTID_PAGE_PROFILE	0x08
// Page Main
#define NXTID_MAIN_ONOFF	0x02
#define NXTID_BOILER_URGENT	0x13
#define NXTID_GENERATOR	    0x16
// Page Heat
#define NXTID_TEMP_PLUS		0x17
#define NXTID_TEMP_MINUS	0x18
#define NXTID_NEXT_MODE		0x1A
// Page Boiler
#define NXTID_BOILER_ONOFF	0x14
#define NXTID_BOILER_PLUS	0x0D
#define NXTID_BOILER_MINUS	0x0E
// Page Profile
#define NXTID_SCHEDULER_OFF	0x80
#define NXTID_SCHEDULER_ON	0x81
#define NXTID_PROFILE_ONOFF	0x25


#define fSleep 1
#define NEXTION_INPUT_DELAY	10		// * NEXTION_READ ms

char buffer[64];
#define ntemp buffer

// первоначальная инициализация - страница 0
boolean Nextion::init()
{
	DataAvaliable = 0;
	flags = 0;
	StatusCrc = 0;
	fUpdate = 0;
	input_delay = 0;
	PageID = NXTID_PAGE_MAIN;
	NEXTION_PORT.begin(NEXTION_PORT_SPEED);
//	sendCommand("rest");
//	sendCommand("cls 0");
    sendCommand("sleep=0");
    _delay(NEXTION_BOOT_TIME);
	sendCommand("sendme");
	uint16_t timeout = NEXTION_BOOT_TIME * 2; // ~ms
	while(--timeout) {
		_delay(1);
		if(check_incoming()) break;
	}
	if(timeout) {
		while(NEXTION_PORT.available()) NEXTION_PORT.read();
		DataAvaliable = 0;
		fUpdate = 1;
		journal.jprintf("OK\n");
	} else {
		journal.jprintf("No response!\n");
		return false;
	}
	init_display();
	return true;
}

void Nextion::init_display()
{
	// bkcmd:
	//– Level 0 is Off – no pass/fail will be returned
	//– Level 1 is OnSuccess,  only when last serial command successful.
	//– Level 2 is OnFailure, only when last serial command failed
	//– Level 3 is Always, returns 0x00 to 0x23 result of serial command.
	sendCommand("bkcmd=2");
	//_delay(10);
	sendCommand("sendxy=0");
	sendCommand("thup=1");
	if(HP.Option.sleep>0 && HP.get_errcode()==OK && !HP.get_BackupPower())   // установлено засыпание дисплея, при условии! нет ошибок...
	{
		strcpy(ntemp, "thsp=");
		_itoa(HP.Option.sleep * 60, ntemp); // секунды
		sendCommand(ntemp);
	} else {
		sendCommand("thsp=0");   // sleep режим выключен  - ЭТО  РАБОТАЕТ
		/*
		 sendCommand("rest");         // Запретить режим сна получается только через сброс экрана
		 _delay(50);
		 sendCommand("page 0");
		 sendCommand("bkcmd=0");     // Ответов нет от дисплея
		 sendCommand("sendxy=0");
		 sendCommand("thup=1");      // sleep режим активировать
		 */
	}
	sendCommand("page 0");
}

void Nextion::set_dim(uint8_t dim)
{
	strcpy(ntemp, "dims=");
	_itoa(dim, ntemp);
	sendCommand(ntemp);
}

// Проверка на начало получения данных из дисплея и ожидание
boolean Nextion::check_incoming(void)
{
	boolean ret = false;
	while(DataAvaliable != NEXTION_PORT.available()) {
		DataAvaliable = NEXTION_PORT.available();
		ret = true;
		_delay(1); // для скорости 9600
	}
	return ret;
}

// Возвращает false, если данные начали поступать из дисплея
boolean Nextion::sendCommand(const char* cmd)
{
	check_incoming();
	NEXTION_PORT.write(cmd);
	NEXTION_PORT.write(COMM_END_B);
	NEXTION_PORT.write(COMM_END_B);
	NEXTION_PORT.write(COMM_END_B);
#ifdef NEXTION_DEBUG
	journal.jprintf("NXTTX: %s\n",cmd);
#endif
	if(check_incoming()) return false;
	return true;
}

boolean Nextion::setComponentValIdx(const char* component, uint8_t idx, int8_t val)
{
	check_incoming();
	NEXTION_PORT.write(component);
	NEXTION_PORT.print(idx);
	NEXTION_PORT.write(".val=");
	NEXTION_PORT.print(val);
	NEXTION_PORT.write(COMM_END_B);
	NEXTION_PORT.write(COMM_END_B);
	NEXTION_PORT.write(COMM_END_B);
#ifdef NEXTION_DEBUG
	journal.jprintf("NXTTX: %s%d=%d\n", component, idx, val);
#endif
	return !check_incoming();
}

// cmd with space
boolean Nextion::sendCommandToComponentIdx(const char* component, const char* cmd, uint8_t idx, int8_t val)
{
	check_incoming();
	NEXTION_PORT.write(cmd);
	NEXTION_PORT.write(component);
	NEXTION_PORT.print(idx);
	NEXTION_PORT.write(",");
	NEXTION_PORT.print(val);
	NEXTION_PORT.write(COMM_END_B);
	NEXTION_PORT.write(COMM_END_B);
	NEXTION_PORT.write(COMM_END_B);
#ifdef NEXTION_DEBUG
	journal.jprintf("NXTTX: %s%s%d,%d\n", cmd, component, idx, val);
#endif
	return !check_incoming();
}

boolean Nextion::setComponentText(const char* component, char* txt)
{
	check_incoming();
	NEXTION_PORT.write(component);
	NEXTION_PORT.write(".txt=\"");
	NEXTION_PORT.write(txt);
	NEXTION_PORT.write("\"");
	NEXTION_PORT.write(COMM_END_B);
	NEXTION_PORT.write(COMM_END_B);
	NEXTION_PORT.write(COMM_END_B);
#ifdef NEXTION_DEBUG
	journal.jprintf("NXTTX: %s=%s\n", component, txt);
#endif
	return !check_incoming();
}

boolean Nextion::setComponentIdxText(const char* component, uint8_t idx, char* txt)
{
	check_incoming();
	NEXTION_PORT.write(component);
	NEXTION_PORT.print(idx);
	NEXTION_PORT.write(".txt=\"");
	NEXTION_PORT.write(txt);
	NEXTION_PORT.write("\"");
	NEXTION_PORT.write(COMM_END_B);
	NEXTION_PORT.write(COMM_END_B);
	NEXTION_PORT.write(COMM_END_B);
#ifdef NEXTION_DEBUG
	journal.jprintf("NXTTX: %s%d=%s\n", component, idx, txt);
#endif
	return !check_incoming();
}

// Обработка входящей команды (только одна - первая)
void Nextion::readCommand()
{
	if(!GETBIT(HP.Option.flags, fNextion)) return;
	if(input_delay) input_delay--;
	while(check_incoming() || DataAvaliable) {
		uint8_t buffer_idx = 0;
		char *p = NULL;
		while(NEXTION_PORT.available() && buffer_idx < sizeof(buffer)) {
			if((buffer[buffer_idx++] = NEXTION_PORT.read()) == COMM_END_B && buffer_idx > 3 && buffer[buffer_idx - 2] == COMM_END_B && buffer[buffer_idx - 3] == COMM_END_B) {
				p = buffer + buffer_idx - 3;
				break;
			}
		}
		DataAvaliable = NEXTION_PORT.available();
		if(p == NULL) break;
		uint8_t len = p - buffer;
#ifdef NEXTION_DEBUG
		journal.jprintf("NXTRX: ");
		for(uint8_t i = 0; i < len + 3; i++) journal.jprintf("%02x", buffer[i]);
		journal.jprintf("\n");
#endif
		switch(buffer[0]) {
		case 0x65:   //   	Touch Event
			if(input_delay) break;
			if((len == 4 && buffer[3] == 0) || len == 3) { // event: release
				uint8_t cmd1 = buffer[1];
				uint8_t cmd2 = buffer[2];
				if(cmd1 == NXTID_PAGE_MAIN) {
					if(cmd2 == NXTID_BOILER_URGENT) {
						HP.set_HeatBoilerUrgently(!HP.HeatBoilerUrgently);
						fUpdate = 2;
						input_delay = NEXTION_INPUT_DELAY;
					} else if(cmd2 == NXTID_MAIN_ONOFF) {  // событие нажатие кнопки вкл/выкл ТН
						if((HP.get_State() != pSTARTING_HP) || (HP.get_State() != pSTOPING_HP)) {
							if(HP.get_State() == pOFF_HP) HP.sendCommand(pSTART); else HP.sendCommand(pSTOP);
							input_delay = NEXTION_INPUT_DELAY * 2;
							return;
						}
					} else if(cmd2 == NXTID_GENERATOR) { // Прочитать команду работа от генератора
				        HP.set_BackupPower(!HP.get_BackupPower());
				        input_delay = NEXTION_INPUT_DELAY * 2;
					}
				} else if(cmd1 == NXTID_PAGE_HEAT) { // Изменение целевой температуры СО шаг изменения десятые градуса
					if(cmd2 == NXTID_TEMP_PLUS || cmd2 == NXTID_TEMP_MINUS) {
						HP.setTargetTemp(cmd2 == NXTID_TEMP_PLUS ? 10 : -10);
						HP.getTargetTempStr(ntemp);
						setComponentText("tust", ntemp);
					} else if(cmd2 == NXTID_NEXT_MODE) { // Переключение режимов отопления ТОЛЬКО если насос выключен
						if(!HP.IsWorkingNow()) {
							HP.set_nextMode();  // выбрать следующий режим
							switch((MODE_HP) HP.get_modeHouse()) {
							case pOFF:
								sendCommand("vis hotin,0");
								sendCommand("vis hotout,1");
								sendCommand("vis coolin,0");
								sendCommand("vis coolout,1");
								sendCommand("vis coin,1");
								sendCommand("vis coout,0");
								break;
							case pHEAT:
								sendCommand("vis hotin,1");
								sendCommand("vis hotout,0");
								sendCommand("vis coolin,0");
								sendCommand("vis coolout,1");
								sendCommand("vis coin,0");
								sendCommand("vis coout,1");
								break;
							case pCOOL:
								sendCommand("vis hotin,0");
								sendCommand("vis hotout,1");
								sendCommand("vis coolin,1");
								sendCommand("vis coolout,0");
								sendCommand("vis coin,0");
								sendCommand("vis coout,1");
								break;
							default:
								break;
							}
						}
					}
				} else if(cmd1 == NXTID_PAGE_BOILER) {
					if(cmd2 == NXTID_BOILER_ONOFF) {                       // событие нажатие кнопки вкл/выкл ГВС
						if(HP.get_BoilerON()) HP.set_BoilerOFF(); else HP.set_BoilerON();
					} else if(cmd2 == NXTID_BOILER_PLUS || cmd2 == NXTID_BOILER_MINUS) {  // Изменение целевой температуры ГВС шаг изменения сотые градуса
						setComponentText("tustgvs", ftoa(ntemp, (float) HP.setTempTargetBoiler(cmd2 == NXTID_BOILER_PLUS ? 100 : -100) / 100.0, 1));
					}
				} else if(cmd1 == NXTID_PAGE_PROFILE) {
					if(cmd2 == NXTID_SCHEDULER_OFF) {
						HP.Schdlr.web_set_param((char*)WEB_SCH_On, (char*)"0");
					} else if(cmd2 == NXTID_SCHEDULER_ON) {
						HP.Schdlr.web_set_param((char*)WEB_SCH_On, (char*)"1");
					} else if(cmd2 < I2C_PROFIL_NUM) {
						HP.Prof.set_list(cmd2);
					} else if(cmd2 == NXTID_PROFILE_ONOFF) {  // событие нажатие кнопки вкл/выкл ТН
						if((HP.get_State() != pSTARTING_HP) || (HP.get_State() != pSTOPING_HP)) {
							if(HP.get_State() == pOFF_HP) HP.sendCommand(pSTART); else HP.sendCommand(pSTOP);
							input_delay = NEXTION_INPUT_DELAY * 2;
							return;
						}
					}
				}
			}
			break;
		case 0x66:  // 	Current Page    // Произошла смена страницы
			fUpdate = 2;
			PageID = buffer[1];
			break;
		case 0x67:  // Touch Coordinate (awake)
		case 0x68:  // Touch Coordinate (sleep)
			break;
		case 0x86:  // Auto Entered Sleep Mode
			if(GETBIT(HP.Option.flags, fNextionOnWhileWork)) flags &= ~((HP.is_compressor_on() || HP.get_errcode() || HP.get_BackupPower())<<fSleep);
			fUpdate = 0;
			break;
		case 0x87:   // выход из сна
			fUpdate = 2;
			break;
		case 0x88:   // Power on
			init_display();
			break;
		default: // 0x00 - 	Invalid Instruction, 0x03 - Invalid Page ID, 0x1A,0x1B - Invalid Variable, 0x1E - Invalid Quantity of Parameters, 0x1F - IO Operation failed
			sendCommand("sendme");
#ifdef NEXTION_DEBUG
			journal.jprintf("Nextion(%d) RX: %02X\n", PageID, buffer[0]);
#endif
			_delay(10);
		}
	}
	if(fUpdate >= 2) Update();
}

// Обновление информации на дисплее вызывается в цикле
void Nextion::Update()
{
	if(!GETBIT(HP.Option.flags, fNextion)) return;
	if((GETBIT(HP.Option.flags, fNextionOnWhileWork) && HP.is_compressor_on()) || HP.get_errcode() || HP.get_BackupPower()) {  // При ошибке дисплей включен
		if(!GETBIT(flags, fSleep)) {
			sendCommand("sleep=0");
			_delay(NEXTION_BOOT_TIME);
			sendCommand("thsp=0");
			flags |= (1<<fSleep);
			fUpdate = 2;
		}
	} else if(GETBIT(flags, fSleep)) {
		flags &= ~(1<<fSleep);
		if(HP.Option.sleep > 0) {  // установлено засыпание дисплея
			strcpy(ntemp, "thsp=");      // sleep режим активировать
			_itoa(HP.Option.sleep * 60, ntemp); // секунды
			sendCommand(ntemp);
			sendCommand("thup=1");
		}
	}

	if(fUpdate == 0) return;
	//if(!sendCommand("ref_stop")) return;      // Остановить обновление
	// 2. Вывод в зависмости от страницы
	if(PageID == NXTID_PAGE_MAIN)  // Обновление данных 0 страницы "Главный экран"
	{
		strcat(dptoa(ntemp, HP.sTemp[TIN].get_Temp() / 10, 1), _xB0);
		setComponentText("t0", ntemp);
		strcat(dptoa(ntemp, HP.sTemp[TOUT].get_Temp() / 10, 1), _xB0);
		setComponentText("t2", ntemp);
		strcat(dptoa(ntemp, HP.sTemp[TBOILER].get_Temp() / 10, 1), _xB0);
		setComponentText("t3", ntemp);
		strcat(dptoa(ntemp, HP.sTemp[TEVAING].get_Temp() / 10, 1), _xB0);
		setComponentText("t4", ntemp);
		strcat(dptoa(ntemp, HP.FEED /10, 1),_xB0);
		setComponentText("t5", ntemp);
		HP.getTargetTempStr(ntemp);
		uint16_t newcrc = calc_crc16((uint8_t*)ntemp, 4);
		if(newcrc != Page1crc || fUpdate == 2) {
			Page1crc = newcrc;
			strcat(ntemp, _xB0);
			setComponentText("t1", ntemp);
		}
		uint8_t fl = (HP.IsWorkingNow() && HP.get_State() != pSTOPING_HP) | (HP.HeatBoilerUrgently << 1)
#ifdef USE_SUN_COLLECTOR
					| (HP.dRelay[RSUN].get_Relay() << 2)
#endif
#ifdef RBOILER
					| (HP.dRelay[RBOILER].get_Relay() << 3)
#endif
					| (HP.get_BackupPower() << 4);
		if(fUpdate == 2) Page1flags = ~fl;
		if(fl != Page1flags) {
			if((fl ^ Page1flags) & (1<<0)) {
				if(fl & (1<<0)) sendCommand("bt0.val=0");    // Кнопка включения в положение ВКЛ
				else sendCommand("bt0.val=1");    // Кнопка включения в положение ВЫКЛ
#ifndef NEXTION_SCR0_DIS_ON_OFF
				if(fUpdate == 2) sendCommand("tsw bt0,1");
#endif
			}
			if((fl ^ Page1flags) & (1<<1)) {
				if(fl & (1<<1)) sendCommand("vis g,1"); else sendCommand("vis g,0");
			}
#ifdef USE_SUN_COLLECTOR
			if((fl ^ Page1flags) & (1<<2)) {
				if(fl & (1<<2)) sendCommand("vis s,1"); else sendCommand("vis s,0");
			}
#endif
#ifdef RBOILER
			if((fl ^ Page1flags) & (1<<3)) {
				if(fl & (1<<3)) sendCommand("t3.pco=63488"); else sendCommand("t3.pco=65535");
			}
#endif
            if((fl ^ Page1flags) & (1<<4)) {
#ifdef NEXTION_GENERATOR 
            	if(fUpdate == 2) {
            		sendCommand("vis bt1,1");  // Показ кнопки работа от генератора
            		sendCommand("tsw bt1,1");
            	}
            	if(fl & (1<<4)) sendCommand("bt1.val=1"); else sendCommand("bt1.val=0");
#else
            	if(fl & (1<<4)) sendCommand("vis bt1,1"); else sendCommand("vis bt1,0");
#endif
            }
            Page1flags = fl;
		}
	} else if(PageID == NXTID_PAGE_NETWORK)  // Обновление данных первой страницы "СЕТЬ"
	{
		if(fUpdate == 2) {
			/*
			 Использовать DHCP сервер  -web1
			 IP адрес контролера  -web2
			 Маска подсети - web3
			 Адрес шлюза  - web4
			 Адрес DNS сервера - web5
			 Аппаратный mac адрес - web6 */
			setComponentText("web1", (char*) (HP.get_DHCP() ? _YES_8859 : _NO_8859)); ntemp[0] = '\0';
			setComponentText("web2", HP.get_network((char*) net_IP, ntemp)); ntemp[0] = '\0';
			setComponentText("web3", HP.get_network((char*) net_SUBNET, ntemp)); ntemp[0] = '\0';
			setComponentText("web4", HP.get_network((char*) net_GATEWAY, ntemp)); ntemp[0] = '\0';
			setComponentText("web5", HP.get_network((char*) net_DNS, ntemp)); ntemp[0] = '\0';
			setComponentText("web6", HP.get_network((char*) net_MAC, ntemp));
			/*
			 Использование паролей - pas1
			 Имя - pas2 пароль - pas3
			 Имя - pas4 пароль - pas5 */
			setComponentText("pas1", (char*) (HP.get_fPass() ? _YES_8859 : _NO_8859));
			setComponentText("pas2", (char*) NAME_USER); ntemp[0] = '\0';
			setComponentText("pas3", HP.get_network((char*) net_PASSUSER, ntemp)); ntemp[0] = '\0';
			setComponentText("pas4", (char*) NAME_ADMIN);
			setComponentText("pas5", HP.get_network((char*) net_PASSADMIN, ntemp));
		}
	} else if(PageID == NXTID_PAGE_SYSTEM)  // Обновление данных 3 страницы "Система"
	{
		if(fUpdate == 2) {
			setComponentText("syst1", (char*) VERSION);
		}
		ntemp[0] = '\0';
		setComponentText("syst2", TimeIntervalToStr(HP.get_uptime(), ntemp));
		setComponentText("syst3", ResetCause());
		setComponentText("syst4", HP.IsWorkingNow() ? itoa(HP.num_repeat, ntemp, 10) : (char*) _HP_OFF_8859);
		setComponentText("syst5", ftoa(ntemp, (float) HP.get_motoHour()->H2 / 60.0, 1));
		setComponentText("syst6", ftoa(ntemp, (float) HP.get_motoHour()->C2 / 60.0, 1));
		setComponentText("syst7", itoa(HP.CPU_LOAD, ntemp, 10));
		setComponentText("syst8", HP.get_errcode() == OK ? (char *)"-" : itoa(HP.get_errcode(), ntemp, 10));
		if(HP.get_errcode() == OK) buffer[0] = '\0';
		else Encode_UTF8_to_ISO8859_5(buffer, HP.get_lastErr(), sizeof(buffer)-1);
		setComponentText("terr", buffer);

	} else if(PageID == NXTID_PAGE_SCHEME)  // Обновление данных 4 страницы "СХЕМА ТН"
	{
		/*
		 темп на улице - tout
		 темп в доме - tin
		 компрессор - tcomp
		 перегрев - tper
		 из геоконтура - tevaoutg
		 в геоконтур - tevaing
		 из системы отопления - tconoutg
		 в систему отопления - tconing
		 */
		strcat(dptoa(ntemp, HP.sTemp[TOUT].get_Temp() / 10, 1), _xB0);
		setComponentText("tout", ntemp);
		strcat(dptoa(ntemp, HP.sTemp[TIN].get_Temp() / 10, 1), _xB0);
		setComponentText("tin", ntemp);
		strcat(dptoa(ntemp, HP.sTemp[TCOMP].get_Temp() / 10, 1), _xB0);
		setComponentText("tcomp", ntemp);
		strcat(dptoa(ntemp, HP.sTemp[TEVAOUTG].get_Temp() / 10, 1), _xB0);
		setComponentText("tevaing", ntemp);
		strcat(dptoa(ntemp, HP.sTemp[TEVAING].get_Temp() / 10, 1), _xB0);
		setComponentText("tevaoutg", ntemp);
		strcat(dptoa(ntemp, HP.sTemp[TCONOUTG].get_Temp() / 10, 1), _xB0);
		setComponentText("tconing", ntemp);
		strcat(dptoa(ntemp, HP.sTemp[TCONING].get_Temp() / 10, 1), _xB0);
		setComponentText("tconoutg", ntemp);

#ifdef EEV_DEF
		strcat(dptoa(ntemp, HP.dEEV.get_Overheat() / 10, 1), _xB0);
		setComponentText("tper", ntemp);
#else
		strcat(dptoa(ntemp, 0, 1),_xB0); setComponentText("tper", ntemp);
#endif
	} else if(PageID == NXTID_PAGE_HEAT)  // Обновление данных 5 страницы "Отопление/Охлаждение"
	{
		/*
		 установленная Т - tust
		 Т плюс - plus
		 Т минус - minus
		 Отопление вкл - hotin
		 Отопление выкл - hotout
		 Охлаждение вкл - coolin
		 Охлаждение выкл - coolout
		 СО отключено вкл - coin
		 СО отключено выкл - coout
		 Алгоритм Т в доме - alg1
		 Алгоритм Т обратки - alg2
		 */
		HP.getTargetTempStr(ntemp);
		setComponentText("tust", ntemp);
		// Состояние системы отопления
		switch(HP.get_modeHouse()) {
		case pOFF:
			sendCommand("vis hotin,0");
			sendCommand("vis hotout,1");
			sendCommand("vis coolin,0");
			sendCommand("vis coolout,1");
			sendCommand("vis coin,1");
			sendCommand("vis coout,0");
			sendCommand("vis alg1,0");
			sendCommand("vis alg2,0");  // убрать показ алгоритма
			break;
		case pHEAT:
			sendCommand("vis hotin,1");
			sendCommand("vis hotout,0");
			sendCommand("vis coolin,0");
			sendCommand("vis coolout,1");
			sendCommand("vis coin,0");
			sendCommand("vis coout,1");
			switch((RULE_HP) HP.get_ruleHeat()) {
			case pHYSTERESIS:
			case pPID:
				if(HP.get_TargetHeat()) {
					sendCommand("vis alg1,0");
					sendCommand("vis alg2,1");
				}   // цель дом
				else {
					sendCommand("vis alg1,1");
					sendCommand("vis alg2,0");
				} // цель обратка
				break;
			case pHYBRID:
				sendCommand("vis alg1,0");
				sendCommand("vis alg2,1");  // цель дом
				break;
			default:
				break;
			}  // switch((RULE_HP) HP.get_ruleHeat())
			break;
		case pCOOL:
			sendCommand("vis hotin,0");
			sendCommand("vis hotout,1");
			sendCommand("vis coolin,1");
			sendCommand("vis coolout,0");
			sendCommand("vis coin,0");
			sendCommand("vis coout,1");
			switch((RULE_HP) HP.get_ruleCool()) {
			case pHYSTERESIS:
			case pPID:
				if(HP.get_TargetCool()) {
					sendCommand("vis alg1,0");
					sendCommand("vis alg2,1");
				} else {
					sendCommand("vis alg1,1");
					sendCommand("vis alg2,0");
				}
				break;
			case pHYBRID:
				sendCommand("vis alg1,0");
				sendCommand("vis alg2,1");
				break;
			default:
				break;
			}  // switch((RULE_HP) HP.get_ruleCool())
			break;
			default:
				break;
		} // switch ((MODE_HP)HP.get_modeHouse() )

	} else if(PageID == NXTID_PAGE_BOILER)  // Обновление данных 6 страницы "ГВС"
	{
		strcat(ftoa(ntemp, (float) HP.sTemp[TBOILER].get_Temp() / 100.0, 1), _xB0);
		setComponentText("tboiler", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TCONOUTG].get_Temp() / 100.0, 1), _xB0);
		setComponentText("tconoutg", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TCONING].get_Temp() / 100.0, 1), _xB0);
		setComponentText("tconing", ntemp);
		strcat(ftoa(ntemp, (float) HP.get_boilerTempTarget() / 100.0, 1), "");
		setComponentText("tustgvs", ntemp);
		if(HP.get_BoilerON()) sendCommand("gvson.val=1");    // Кнопка включения ГВС в положение ВКЛ
		else sendCommand("gvson.val=0");                     // Кнопка включения ГВС в положение ВЫКЛ

	} else if(PageID == NXTID_PAGE_INFO) { // Обновление данных 7 страницы "О контролллере"
		if(fUpdate == 2) {
			Encode_UTF8_to_ISO8859_5(buffer, CONFIG_NAME, sizeof(buffer)-1);
			setComponentText("t1", buffer);
			Encode_UTF8_to_ISO8859_5(buffer, CONFIG_NOTE, sizeof(buffer)-1);
			setComponentText("t2", buffer);
		}
	} else if(PageID == NXTID_PAGE_PROFILE) { // Обновление данных страницы 8 "Профили"
#ifdef NEXTION_DEBUG
		journal.printf("#: %u\n", GetTickCount());
#endif
		if(HP.Schdlr.IsShedulerOn()) sendCommand("s.val=1"); else sendCommand("s.val=0");
		if((HP.IsWorkingNow() && HP.get_State() != pSTOPING_HP)) sendCommand("bt0.val=0");    // Кнопка включения в положение ВКЛ
		else sendCommand("bt0.val=1");    // Кнопка включения в положение ВЫКЛ
		if(fUpdate == 2) {
			Encode_UTF8_to_ISO8859_5(buffer, HP.Schdlr.sch_data.Names[HP.Schdlr.sch_data.Active], sizeof(buffer)-1);
			setComponentText("trn", buffer);
			char *lst = HP.Prof.list;
			int8_t i = 0;
			for(; i < I2C_PROFIL_NUM; i++) {
				char *p = strchr(lst, ':');
				if(p == NULL) break;
				memcpy(buffer, lst, p - lst);
				buffer[p - lst] = '\0';
				Encode_UTF8_to_ISO8859_5(buffer, buffer, sizeof(buffer)-1);
				setComponentIdxText("t", i, buffer);
				if(p[1] == '1') sendCommandToComponentIdx("r", "click ", i, 1);
				lst = p + 3;
			}
			sendCommand("click z,0");
		}
#ifdef NEXTION_DEBUG
		journal.printf("##: %u\n", GetTickCount());
#endif
	}
	StatusLine();
	fUpdate = 1;
}

// Показ строки статуса в зависимости от состояния ТН
void Nextion::StatusLine()
{
	// Вычисление статуса
	char *tm = NowTimeToStr();
	uint16_t newcrc = calc_crc16((uint8_t*)tm, 5);
	newcrc = _crc16(newcrc, HP.get_errcode());
	newcrc = _crc16(newcrc, HP.get_modeHouse());
	newcrc = _crc16(newcrc, (HP.IsWorkingNow() << 1) | HP.get_BoilerON());
	char *ss = HP.StateToStr();
	newcrc = calc_crc16((uint8_t*)ss, strlen(ss), newcrc);
	if(newcrc != StatusCrc || fUpdate > 1) { // поменялся
		StatusCrc = newcrc;

		tm[5] = '\0';
		setComponentText("time", tm);  // Обновить время
		// Ошибки
		if(HP.get_errcode() == OK) {
			sendCommand("vis ok,1");
			sendCommand("vis er,0");
		} else {
			sendCommand("vis er,1");
			sendCommand("vis ok,0");
			if(PageID == NXTID_PAGE_MAIN) {
				Encode_UTF8_to_ISO8859_5(ntemp, "Ошибка", sizeof(ntemp)-1);
				_itoa(HP.get_errcode(), ntemp);
				setComponentText("er", ntemp);
			}
		}

		if(HP.IsWorkingNow()) {
			Encode_UTF8_to_ISO8859_5(ntemp, ss, 10); // = txt_maxl.
			setComponentText("tninc", ntemp);
			sendCommand("vis tninc,1");
			sendCommand("vis tnoff,0");
			switch((MODE_HP) HP.get_modeHouse()) {
			case pOFF:
				if(HP.get_BoilerON()) {
					sendCommand("vis gvs,0");
					sendCommand("vis og,1");
				} else {
					sendCommand("vis gvs,0");
					sendCommand("vis og,0");
				}
				break;
			case pHEAT:
				sendCommand("vis heat,1");
				sendCommand("vis cool,0");
				if(HP.get_BoilerON()) {
					sendCommand("vis gvs,1");
					sendCommand("vis og,0");
				} else {
					sendCommand("vis gvs,0");
					sendCommand("vis og,0");
				}
				break;
			case pCOOL:
				sendCommand("vis heat,0");
				sendCommand("vis cool,1");
				if(HP.get_BoilerON()) {
					sendCommand("vis gvs,1");
					sendCommand("vis og,0");
				} else {
					sendCommand("vis gvs,0");
					sendCommand("vis og,0");
				}
				break;
			default:
				break;
			} //switch ((int)HP.get_modeHouse() )
		} else { // Насос выключен
			sendCommand("vis tninc,0");
			sendCommand("vis tnoff,1");
			sendCommand("vis heat,0");
			sendCommand("vis cool,0");
			sendCommand("vis gvs,0");
			sendCommand("vis og,0");
		}
	}
}

// UTF8, 1 байт или 2 байта русские, остальные символы пропускаются
void Nextion::Encode_UTF8_to_ISO8859_5(char* outstr, const char* instr, uint16_t outstrsize)
{
	uint8_t c;
	//if(--outsize <= 0) return;
	while((c = *instr++)) {
		if(c > 0x7F) {
			uint16_t c2 = c * 256 + *instr++;
			if(c2 >= 0xD090 && c2 <= 0xD0BF) c = c2 - 0xD090 + 0xB0;
			else if(c2 >= 0xD180 && c2 <= 0xD18F) c = c2 - 0xD180 + 0xE0;
			else if(c2 == 0xE284 && *instr++ == 0x96) c = 0xF0;
			else {
				if(c > 0xE0) {
					instr++;
					if(c > 0xF0) instr++;
				}
				continue;
			}
		}
		*outstr++ = c;
		if(--outstrsize == 0) break;
	}
	*outstr = '\0';
}
