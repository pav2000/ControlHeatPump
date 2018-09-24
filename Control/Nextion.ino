/*
 * Copyright (c) 2016-2018 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav, by vad711 (vad7@yahoo.com)
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
#include "Nextion.h"

// Команды и строки дисплея (экономим место)
// В NEXTION надо посылать сроки в кодировке iso8859-5 перевод http://codepage-encoding.online-domain-tools.com/
const char *_YES_8859 = { "\xB4\xB0" };                                             // ДА
const char *_NO_8859 = { "\xBD\xB5\xC2" };                                         // НЕТ
const char *_HP_OFF_8859 = { "\xc2\xbd\x20\xd2\xeb\xda\xdb\xee\xe7\xd5\xdd\x0d\x0a" }; // ТН выключен
const char *_xB0 = { "\xB0" }; // strcat(ftoa(temp,(float)HP.sTemp[TEVAOUTG].get_Temp()/100.0,1),"\xB0"); setComponentText("t4", temp);
const char comm_end[3] = { 0xFF, 0xFF, 0xFF };
char 	buffer[128];

// первоначальная инициализация - страница 0
boolean Nextion::init()
{
	DataAvaliable = 0;
	refresh_flags = 0;
	NEXTION_PORT.begin(9600);
	// Поднятие скорости обмена
	//  sendCommand("baud=115200");
	//  _delay(100);
	//  NEXTION_PORT.begin(115200);
	sendCommand("rest");
	_delay(100);
	if(check_incoming()) {
		NEXTION_PORT.flush();
		DataAvaliable = 0;
	} else {
		journal.jprintf(" No response!\n");
		return false;
	}

	sendCommand("bkcmd=0");     // Ответов нет от дисплея
	_delay(10);
	sendCommand("sendxy=0");
	sendCommand("thup=1");
	//sendCommand("sleep=0");
	sendCommand("page 0");
	PageID = 0;
	fPageID = false;
	StartON();
	return true;
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
	if(check_incoming()) return false;
	NEXTION_PORT.print(cmd);
	if(check_incoming()) return false;
	NEXTION_PORT.write(0xFF);
	NEXTION_PORT.write(0xFF);
	NEXTION_PORT.write(0xFF);
	if(check_incoming()) return false;
#ifdef NEXTION_DEBUG
	journal.jprintf("NXTTX: %s\n",cmd);
#endif
	return true;
}

boolean Nextion::ack(void)
{
	uint8_t bytes[4] = { 0 };
	NEXTION_PORT.setTimeout(20);
	if(sizeof(bytes) != NEXTION_PORT.readBytes((char *) bytes, sizeof(bytes))) {
		return false;
	}
	if((bytes[1] == 0xFF) && (bytes[2] == 0xFF) && (bytes[3] == 0xFF)) {
		switch(bytes[0]) {
		case 0x00:
			return false;
			break;
		case 0x01:
			return true;
			break;
		default:
			return false;
		}
	}
	return false;
}

boolean Nextion::setComponentText(char* component, char* txt)
{
	m_snprintf(buffer, sizeof(buffer), "%s.txt=\"%s\"", component, txt);
	return sendCommand(buffer);
}

// Обработка входящей команды (только одна - первая)
void Nextion::readCommand()
{
	if(!check_incoming() && DataAvaliable == 0) return;
	uint8_t buffer_idx = 0;
	while(NEXTION_PORT.available()/*&& buffer_idx < sizeof(buffer)*/) buffer[buffer_idx++] = NEXTION_PORT.read();
	DataAvaliable = NEXTION_PORT.available();
	char *p = (char *)memmem(buffer, buffer_idx, comm_end, sizeof(comm_end));
	if(p == NULL) {
		//buffer_idx = 0;
		return;
	}
	uint8_t len = p - buffer;
#ifdef NEXTION_DEBUG
	journal.jprintf("NXTRX: ");
	for(uint8_t i = 0; i < len; i++) journal.jprintf("%02x", buffer[i]);
	journal.jprintf("\n");
#endif

	switch(buffer[0]) {
	case 0x65:   //   	Touch Event
		if(len == 4 && buffer[3] == 0) { // event: release
			uint8_t cmd1 = buffer[1];
			uint8_t cmd2 = buffer[2];
			if(cmd1 == 0x00 && cmd2 == 0x02) {  // событие нажатие кнопки вкл/выкл ТН
				if((HP.get_State() != pSTARTING_HP) || (HP.get_State() != pSTOPING_HP)) {
					if(HP.get_State() == pOFF_HP) HP.sendCommand(pSTART);
					else HP.sendCommand(pSTOP);
				}
			} else if(cmd1 == 0x05) { // Изменение целевой температуры СО шаг изменения сотые градуса
				if(cmd2 == 0x18 || cmd2 == 0x19) {
					setComponentText((char*) "tust", ftoa(temp, (float) HP.setTargetTemp(cmd2 == 0x18 ? 20 : -20) / 100.0, 1));
				} else if(cmd2 == 0x1B) { // Переключение режимов отопления ТОЛЬКО если насос выключен
					if(HP.get_State() == pOFF_HP) {
						HP.set_nextMode();  // выбрать следующий режим
						switch((MODE_HP) HP.get_modeHouse()) {
						case pOFF:
							sendCommand((char*) "vis hotin,0");
							sendCommand((char*) "vis hotout,1");
							sendCommand((char*) "vis coolin,0");
							sendCommand((char*) "vis coolout,1");
							sendCommand((char*) "vis coin,1");
							sendCommand((char*) "vis coout,0");
							break;
						case pHEAT:
							sendCommand((char*) "vis hotin,1");
							sendCommand((char*) "vis hotout,0");
							sendCommand((char*) "vis coolin,0");
							sendCommand((char*) "vis coolout,1");
							sendCommand((char*) "vis coin,0");
							sendCommand((char*) "vis coout,1");
							break;
						case pCOOL:
							sendCommand((char*) "vis hotin,0");
							sendCommand((char*) "vis hotout,1");
							sendCommand((char*) "vis coolin,1");
							sendCommand((char*) "vis coolout,0");
							sendCommand((char*) "vis coin,0");
							sendCommand((char*) "vis coout,1");
							break;
						default:
							break;
						}
					}
				}
			} else if(cmd1 == 0x06 && cmd2 == 0x15) {                       // событие нажатие кнопки вкл/выкл ГВС
				if(HP.get_BoilerON()) HP.set_BoilerOFF();
				else HP.set_BoilerON();
			} else if(cmd1 == 0x06) { // Изменение целевой температуры ГВС шаг изменения сотые градуса
				if(cmd2 == 0x0E || cmd2 == 0x0F) {
					setComponentText((char*) "tustgvs", ftoa(temp, (float) HP.setTempTargetBoiler(cmd2 == 0x0E ? 100 : -100) / 100.0, 1));
				}
			}
		}
		break;
	case 0x66:  // 	Current Page    // Произошла смена страницы
		fPageID = PageID != buffer[1];
		PageID = buffer[1];
		break;
	case 0x87:   // выход из сна
		fPageID = true;
		break;
	}
//	if(buffer_idx > len + sizeof(comm_end))	{
//		memmove(buffer, p, buffer_idx - (len + sizeof(comm_end)));
//		DataAvaliable = 1;
//	}
	if(fPageID) Update();
}

static char ntemp[24];

// Обновление информации на дисплее вызывается в цикле
void Nextion::Update()
{
	if(!setComponentText((char*) "time", NowTimeToStr1())) return;  // Обновить время
	// 2. Вывод в зависмости от страницы
	if(PageID == 0)  // Обновление данных 0 страницы "Главный экран"
	{
		strcat(ftoa(ntemp, (float) HP.sTemp[TIN].get_Temp() / 100.0, 1), (char*) _xB0);
		setComponentText((char*) "t0", ntemp);
		strcat(ftoa(ntemp, (float) getTargetTemp() / 100.0, 1), (char*) _xB0);
		setComponentText((char*) "t1", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TOUT].get_Temp() / 100.0, 1), (char*) _xB0);
		setComponentText((char*) "t2", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TBOILER].get_Temp() / 100.0, 1), (char*) _xB0);
		setComponentText((char*) "t3", ntemp);
		//          strcat(ftoa(ntemp,(float)HP.sTemp[TEVAOUTG].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"t4", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TEVAING].get_Temp() / 100.0, 1), (char*) _xB0);
		setComponentText((char*) "t4", ntemp);
		//          strcat(ftoa(ntemp,(float)HP.RET/100.0,1),(char*)_xB0); setComponentText((char*)"t5", ntemp);
		strcat(ftoa(ntemp, (float) HP.FEED/100.0,1),(char*)_xB0);
		setComponentText((char*) "t5", ntemp);
		if(((HP.get_State() == pWORK_HP) || (HP.get_State() == pSTARTING_HP))) sendCommand((char*) "bt0.val=0");    // Кнопка включения в положение ВКЛ
		else sendCommand((char*) "bt0.val=1");    // Кнопка включения в положение ВЫКЛ
	} else if(PageID == 2)  // Обновление данных первой страницы "СЕТЬ"
	{
		/*
		 Использовать DHCP сервер  -web1
		 IP адрес контролера  -web2
		 Маска подсети - web3
		 Адрес шлюза  - web4
		 Адрес DNS сервера - web5
		 Аппаратный mac адрес - web6 */
		if(HP.get_DHCP()) setComponentText((char*) "web1", (char*) _YES_8859);
		else setComponentText((char*) "web1", (char*) _NO_8859);
		setComponentText((char*) "web2", HP.get_network((char*) net_IP, ntemp));
		setComponentText((char*) "web3", HP.get_network((char*) net_SUBNET, ntemp));
		setComponentText((char*) "web4", HP.get_network((char*) net_GATEWAY, ntemp));
		setComponentText((char*) "web5", HP.get_network((char*) net_DNS, ntemp));
		setComponentText((char*) "web6", HP.get_network((char*) net_MAC, ntemp));
		/*
		 Использование паролей - pas1
		 Имя - pas2 пароль - pas3
		 Имя - pas4 пароль - pas5 */
		if(HP.get_fPass()) setComponentText((char*) "pas1", (char*) _YES_8859);
		else setComponentText((char*) "pas1", (char*) _NO_8859);
		setComponentText((char*) "pas2", (char*) NAME_USER);
		setComponentText((char*) "pas3", HP.get_network((char*) net_PASSUSER, ntemp));
		setComponentText((char*) "pas4", (char*) NAME_ADMIN);
		setComponentText((char*) "pas5", HP.get_network((char*) net_PASSADMIN, ntemp));
	} else if(PageID == 3)  // Обновление данных 3 страницы "Система"
	{
		setComponentText((char*) "syst1", (char*) VERSION);
		setComponentText((char*) "syst2", TimeIntervalToStr(HP.get_uptime(), ntemp));
		setComponentText((char*) "syst3", ResetCause());
		if(HP.get_State() == pWORK_HP) setComponentText((char*) "syst4", itoa(HP.num_repeat, ntemp, 10));
		else setComponentText((char*) "syst4", (char*) _HP_OFF_8859);
		setComponentText((char*) "syst5", ftoa(ntemp, (float) HP.get_motoHourH2() / 60.0, 1));
		setComponentText((char*) "syst6", ftoa(ntemp, (float) HP.get_motoHourC2() / 60.0, 1));
		setComponentText((char*) "syst7", itoa(100 - HP.CPU_IDLE, ntemp, 10));
		setComponentText((char*) "syst8", itoa(HP.get_errcode(), ntemp, 10));

	} else if(PageID == 4)  // Обновление данных 4 страницы "СХЕМА ТН"
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
		strcat(ftoa(ntemp, (float) HP.sTemp[TOUT].get_Temp() / 100.0, 1), (char*) _xB0);
		setComponentText((char*) "tout", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TIN].get_Temp() / 100.0, 1), (char*) _xB0);
		setComponentText((char*) "tin", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TCOMP].get_Temp() / 100.0, 1), (char*) _xB0);
		setComponentText((char*) "tcomp", ntemp);
		//    strcat(ftoa(ntemp,(float)HP.sTemp[TEVAOUTG].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tevaoutg", ntemp);
		//    strcat(ftoa(ntemp,(float)HP.sTemp[TEVAING].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tevaing", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TEVAOUTG].get_Temp() / 100.0, 1), (char*) _xB0);
		setComponentText((char*) "tevaing", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TEVAING].get_Temp() / 100.0, 1), (char*) _xB0);
		setComponentText((char*) "tevaoutg", ntemp);
		//    strcat(ftoa(ntemp,(float)HP.sTemp[TCONOUTG].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tconoutg", ntemp);
		//    strcat(ftoa(ntemp,(float)HP.sTemp[TCONING].get_Temp()/100.0,1),(char*)_xB0); setComponentText((char*)"tconing", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TCONOUTG].get_Temp() / 100.0, 1), (char*) _xB0);
		setComponentText((char*) "tconing", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TCONING].get_Temp() / 100.0, 1), (char*) _xB0);
		setComponentText((char*) "tconoutg", ntemp);

#ifdef EEV_DEF
		strcat(ftoa(ntemp, (float) HP.dEEV.get_Overheat() / 100.0, 1), (char*) _xB0);
		setComponentText((char*) "tper", ntemp);
#else
		strcat(ftoa(ntemp,0.0/100.0,1),(char*)_xB0); setComponentText((char*)"tper", ntemp);
#endif
	} else if(PageID == 5)  // Обновление данных 5 страницы "Отопление/Охлаждение"
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
		setComponentText((char*) "tust", ftoa(ntemp, (float) getTargetTemp() / 100.0, 1));
		// Состояние системы отопления
		switch((MODE_HP) HP.get_modeHouse()) {
		case pOFF:
			sendCommand((char*) "vis hotin,0");
			sendCommand((char*) "vis hotout,1");
			sendCommand((char*) "vis coolin,0");
			sendCommand((char*) "vis coolout,1");
			sendCommand((char*) "vis coin,1");
			sendCommand((char*) "vis coout,0");
			sendCommand((char*) "vis alg1,0");
			sendCommand((char*) "vis alg2,0");  // убрать показ алгоритма
			break;
		case pHEAT:
			sendCommand((char*) "vis hotin,1");
			sendCommand((char*) "vis hotout,0");
			sendCommand((char*) "vis coolin,0");
			sendCommand((char*) "vis coolout,1");
			sendCommand((char*) "vis coin,0");
			sendCommand((char*) "vis coout,1");
			switch((RULE_HP) HP.get_ruleHeat()) {
			case pHYSTERESIS:
			case pPID:
				if(HP.get_TargetHeat()) {
					sendCommand((char*) "vis alg1,0");
					sendCommand((char*) "vis alg2,1");
				}   // цель дом
				else {
					sendCommand((char*) "vis alg1,1");
					sendCommand((char*) "vis alg2,0");
				} // цель обратка
				break;
			case pHYBRID:
				sendCommand((char*) "vis alg1,0");
				sendCommand((char*) "vis alg2,1");  // цель дом
				break;
			default:
				break;
			}  // switch((RULE_HP) HP.get_ruleHeat())
			break;
			case pCOOL:
				sendCommand((char*) "vis hotin,0");
				sendCommand((char*) "vis hotout,1");
				sendCommand((char*) "vis coolin,1");
				sendCommand((char*) "vis coolout,0");
				sendCommand((char*) "vis coin,0");
				sendCommand((char*) "vis coout,1");
				switch((RULE_HP) HP.get_ruleCool()) {
				case pHYSTERESIS:
				case pPID:
					if(HP.get_TargetCool()) {
						sendCommand((char*) "vis alg1,0");
						sendCommand((char*) "vis alg2,1");
					} else {
						sendCommand((char*) "vis alg1,1");
						sendCommand((char*) "vis alg2,0");
					}
					break;
				case pHYBRID:
					sendCommand((char*) "vis alg1,0");
					sendCommand((char*) "vis alg2,1");
					break;
				default:
					break;
				}  // switch((RULE_HP) HP.get_ruleCool())
				break;
				default:
					break;
		} // switch ((MODE_HP)HP.get_modeHouse() )

	} else if(PageID == 6)  // Обновление данных 6 страницы "ГВС"
	{
		strcat(ftoa(ntemp, (float) HP.sTemp[TBOILER].get_Temp() / 100.0, 1), (char*) _xB0);
		setComponentText((char*) "tboiler", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TCONOUTG].get_Temp() / 100.0, 1), (char*) _xB0);
		setComponentText((char*) "tconoutg", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TCONING].get_Temp() / 100.0, 1), (char*) _xB0);
		setComponentText((char*) "tconing", ntemp);
		strcat(ftoa(ntemp, (float) HP.get_boilerTempTarget() / 100.0, 1), "");
		setComponentText((char*) "tustgvs", ntemp);
		if(HP.get_BoilerON()) sendCommand((char*) "gvson.val=1");    // Кнопка включения ГВС в положение ВКЛ
		else sendCommand((char*) "bt0.val=0");                     // Кнопка включения ГВС в положение ВЫКЛ

	}
	// обновление статуса (если сменилась страница или изменилось состояниие ТН)
	// Определение текущего состояния ТН
	uint8_t tempS;
	// Вычисление статуса
	if(HP.get_errcode() == OK) tempS = 1;
	else tempS = 0;
	if(HP.get_State() == pWORK_HP) tempS = tempS + 2;
	if(HP.get_BoilerON()) tempS = tempS + 4;
	switch((int) HP.get_modeHouse()) {
	case pOFF:
		tempS = tempS + 8;
		break;
	case pHEAT:
		tempS = tempS + 9;
		break;
	case pCOOL:
		tempS = tempS + 10;
		break;
	}

	//   Serial.print("-"); Serial.println(temp,BIN);
	if((tempS == Status) && (fPageID == false)) return;     // Обновлять нечего уходим
	else {
		Status = tempS;
		fPageID = false;
		StatusLine();

	}

}

// Подготовка экрана к первому показу
void Nextion::StartON()
{
	sendCommand((char*) "ref_stop");      // Остановить обновление
	sendCommand((char*) "vis tninc,0");
	sendCommand((char*) "vis tnoff,0");
	sendCommand((char*) "vis options,0");
	sendCommand((char*) "vis fault,0");
	sendCommand((char*) "vis heat,0");
	sendCommand((char*) "vis cool,0");
	sendCommand((char*) "vis gvs,0");
	sendCommand((char*) "vis onlygvs,0");
	sendCommand((char*) "bt0.val=1");    // Кнопка включения в положение выключено
	sendCommand((char*) "ref_star");     // Восстановить обновление
	StatusLine();
}

// Показ строки статуса в зависимости от состояния ТН
void Nextion::StatusLine()
{
	if(!sendCommand((char*) "ref_stop")) return;      // Остановить обновление
	//   setComponentText("time", NowTimeToStr1());
	// Ошибки
	if(HP.get_errcode() == OK) {
		sendCommand((char*) "vis options,1");
		sendCommand((char*) "vis fault,0");
	} else {
		sendCommand((char*) "vis fault,1");
		sendCommand((char*) "vis options,0");
	}

	if(PageID == 0) {
		if(((HP.get_State() == pWORK_HP) || (HP.get_State() == pSTARTING_HP))) sendCommand((char*) "bt0.val=0");    // Кнопка включения в положение ВКЛ
		else sendCommand((char*) "bt0.val=1");    // Кнопка включения в положение ВЫКЛ
	} else if(PageID == 6) {
		if(HP.get_BoilerON()) sendCommand((char*) "gvson.val=1");    // Кнопка включения ГВС в положение ВКЛ
		else sendCommand((char*) "bt0.val=0");                    // Кнопка включения ГВС в положение ВЫКЛ
	}

	if(HP.get_State() == pOFF_HP) // Насос выключен
	{
		sendCommand((char*) "vis tninc,0");
		sendCommand((char*) "vis tnoff,1");
		sendCommand((char*) "vis heat,0");
		sendCommand((char*) "vis cool,0");
		sendCommand((char*) "vis gvs,0");
		sendCommand((char*) "vis onlygvs,0");

	} else  // насос включен
	{
		sendCommand((char*) "vis tninc,1");
		sendCommand((char*) "vis tnoff,0");
		switch((MODE_HP) HP.get_modeHouse()) {
		case pOFF:
			if(HP.get_BoilerON()) {
				sendCommand((char*) "vis gvs,0");
				sendCommand((char*) "vis onlygvs,1");
			} else {
				sendCommand((char*) "vis gvs,0");
				sendCommand((char*) "vis onlygvs,0");
			}
			break;
		case pHEAT:
			sendCommand((char*) "vis heat,1");
			sendCommand((char*) "vis cool,0");
			if(HP.get_BoilerON()) {
				sendCommand((char*) "vis gvs,1");
				sendCommand((char*) "vis onlygvs,0");
			} else {
				sendCommand((char*) "vis gvs,0");
				sendCommand((char*) "vis onlygvs,0");
			}
			break;
		case pCOOL:
			sendCommand((char*) "vis heat,0");
			sendCommand((char*) "vis cool,1");
			if(HP.get_BoilerON()) {
				sendCommand((char*) "vis gvs,1");
				sendCommand((char*) "vis onlygvs,0");
			} else {
				sendCommand((char*) "vis gvs,0");
				sendCommand((char*) "vis onlygvs,0");
			}
			break;
		default:
			break;
		} //switch ((int)HP.get_modeHouse() )
	}
	sendCommand((char*) "ref_star");    // Восстановить обновление

}
// Получить целевую температуру отопления
int16_t Nextion::getTargetTemp()
{
	switch((int) HP.get_modeHouse())   // проверка отопления
	{
	case pOFF:
		return -1;
		break;
	case pHEAT:
		return HP.get_targetTempHeat();
		break;
	case pCOOL:
		return HP.get_targetTempCool();
		break;
	default:
		break;
	}
	return -1;
}

