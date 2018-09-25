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
#define COMM_END_B 0xFF
const char comm_end[3] = { COMM_END_B, COMM_END_B, COMM_END_B };
char 	buffer[64];

// первоначальная инициализация - страница 0
boolean Nextion::init()
{
	DataAvaliable = 0;
	refresh_flags = 0;
	StatusCrc = 0;
	PageID = 0;
	fPageID = false;
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

boolean Nextion::setComponentText(const char* component, char* txt)
{
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

// Обработка входящей команды (только одна - первая)
void Nextion::readCommand()
{
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
						setComponentText("tust", ftoa(temp, (float) HP.setTargetTemp(cmd2 == 0x18 ? 20 : -20) / 100.0, 1));
					} else if(cmd2 == 0x1B) { // Переключение режимов отопления ТОЛЬКО если насос выключен
						if(HP.IsWorkingNow()) {
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
				} else if(cmd1 == 0x06 && cmd2 == 0x15) {                       // событие нажатие кнопки вкл/выкл ГВС
					if(HP.get_BoilerON()) HP.set_BoilerOFF(); else HP.set_BoilerON();
				} else if(cmd1 == 0x06) { // Изменение целевой температуры ГВС шаг изменения сотые градуса
					if(cmd2 == 0x0E || cmd2 == 0x0F) {
						setComponentText("tustgvs", ftoa(temp, (float) HP.setTempTargetBoiler(cmd2 == 0x0E ? 100 : -100) / 100.0, 1));
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
//		if(buffer_idx > len + sizeof(comm_end))	{
//			memmove(buffer, p, buffer_idx - (len + sizeof(comm_end)));
//			DataAvaliable = 1;
//		}
	}
	if(fPageID) Update();
}

static char ntemp[24];

// Обновление информации на дисплее вызывается в цикле
void Nextion::Update()
{
	if(!setComponentText("time", NowTimeToStr1())) return;  // Обновить время
	// 2. Вывод в зависмости от страницы
	if(PageID == 0)  // Обновление данных 0 страницы "Главный экран"
	{
		strcat(ftoa(ntemp, (float) HP.sTemp[TIN].get_Temp() / 100.0, 1), _xB0);
		setComponentText("t0", ntemp);
		getTargetTemp(ntemp); strcat(ntemp, _xB0);
		setComponentText("t1", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TOUT].get_Temp() / 100.0, 1), _xB0);
		setComponentText("t2", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TBOILER].get_Temp() / 100.0, 1), _xB0);
		setComponentText("t3", ntemp);
		//          strcat(ftoa(ntemp,(float)HP.sTemp[TEVAOUTG].get_Temp()/100.0,1),_xB0); setComponentText("t4", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TEVAING].get_Temp() / 100.0, 1), _xB0);
		setComponentText("t4", ntemp);
		//          strcat(ftoa(ntemp,(float)HP.RET/100.0,1),_xB0); setComponentText("t5", ntemp);
		strcat(ftoa(ntemp, (float) HP.FEED/100.0,1),_xB0);
		setComponentText("t5", ntemp);
		if(HP.IsWorkingNow()) sendCommand("bt0.val=0");    // Кнопка включения в положение ВКЛ
		else sendCommand("bt0.val=1");    // Кнопка включения в положение ВЫКЛ
	} else if(PageID == 2)  // Обновление данных первой страницы "СЕТЬ"
	{
		/*
		 Использовать DHCP сервер  -web1
		 IP адрес контролера  -web2
		 Маска подсети - web3
		 Адрес шлюза  - web4
		 Адрес DNS сервера - web5
		 Аппаратный mac адрес - web6 */
		setComponentText("web1", (char*) (HP.get_DHCP() ? _YES_8859 : _NO_8859));
		setComponentText("web2", HP.get_network((char*) net_IP, ntemp));
		setComponentText("web3", HP.get_network((char*) net_SUBNET, ntemp));
		setComponentText("web4", HP.get_network((char*) net_GATEWAY, ntemp));
		setComponentText("web5", HP.get_network((char*) net_DNS, ntemp));
		setComponentText("web6", HP.get_network((char*) net_MAC, ntemp));
		/*
		 Использование паролей - pas1
		 Имя - pas2 пароль - pas3
		 Имя - pas4 пароль - pas5 */
		setComponentText("pas1", (char*) (HP.get_fPass() ? _YES_8859 : _NO_8859));
		setComponentText("pas2", (char*) NAME_USER);
		setComponentText("pas3", HP.get_network((char*) net_PASSUSER, ntemp));
		setComponentText("pas4", (char*) NAME_ADMIN);
		setComponentText("pas5", HP.get_network((char*) net_PASSADMIN, ntemp));
	} else if(PageID == 3)  // Обновление данных 3 страницы "Система"
	{
		setComponentText("syst1", (char*) VERSION);
		setComponentText("syst2", TimeIntervalToStr(HP.get_uptime(), ntemp));
		setComponentText("syst3", ResetCause());
		setComponentText("syst4", HP.IsWorkingNow() ? itoa(HP.num_repeat, ntemp, 10) : (char*) _HP_OFF_8859);
		setComponentText("syst5", ftoa(ntemp, (float) HP.get_motoHourH2() / 60.0, 1));
		setComponentText("syst6", ftoa(ntemp, (float) HP.get_motoHourC2() / 60.0, 1));
		setComponentText("syst7", itoa(100 - HP.CPU_IDLE, ntemp, 10));
		setComponentText("syst8", itoa(HP.get_errcode(), ntemp, 10));
		Encode_UTF8_to_ISO8859_5(buffer, noteError[HP.get_errcode()], sizeof(buffer));
		setComponentText("terr", buffer);

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
		strcat(ftoa(ntemp, (float) HP.sTemp[TOUT].get_Temp() / 100.0, 1), _xB0);
		setComponentText("tout", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TIN].get_Temp() / 100.0, 1), _xB0);
		setComponentText("tin", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TCOMP].get_Temp() / 100.0, 1), _xB0);
		setComponentText("tcomp", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TEVAOUTG].get_Temp() / 100.0, 1), _xB0);
		setComponentText("tevaing", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TEVAING].get_Temp() / 100.0, 1), _xB0);
		setComponentText("tevaoutg", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TCONOUTG].get_Temp() / 100.0, 1), _xB0);
		setComponentText("tconing", ntemp);
		strcat(ftoa(ntemp, (float) HP.sTemp[TCONING].get_Temp() / 100.0, 1), _xB0);
		setComponentText("tconoutg", ntemp);

#ifdef EEV_DEF
		strcat(ftoa(ntemp, (float) HP.dEEV.get_Overheat() / 100.0, 1), _xB0);
		setComponentText("tper", ntemp);
#else
		strcat(ftoa(ntemp,0.0/100.0,1),_xB0); setComponentText("tper", ntemp);
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
		getTargetTemp(ntemp);
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

	} else if(PageID == 6)  // Обновление данных 6 страницы "ГВС"
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
		else sendCommand("bt0.val=0");                     // Кнопка включения ГВС в положение ВЫКЛ

	} else if(PageID == 7) { // Обновление данных 7 страницы "О контролллере"
		Encode_UTF8_to_ISO8859_5(buffer, CONFIG_NAME, sizeof(buffer));
		setComponentText("t1", buffer);
		Encode_UTF8_to_ISO8859_5(buffer, CONFIG_NOTE, sizeof(buffer));
		setComponentText("t2", buffer);
	}
	StatusLine();
	fPageID = false;
}

// Подготовка экрана к первому показу
void Nextion::StartON()
{
	sendCommand("ref_stop");      // Остановить обновление
	sendCommand("page 0");
	sendCommand("vis tninc,0");
	sendCommand("vis tnoff,0");
	sendCommand("vis options,0");
	sendCommand("vis fault,0");
	sendCommand("vis heat,0");
	sendCommand("vis cool,0");
	sendCommand("vis gvs,0");
	sendCommand("vis onlygvs,0");
	sendCommand("bt0.val=1");    // Кнопка включения в положение выключено
	StatusLine();
}

// Показ строки статуса в зависимости от состояния ТН
void Nextion::StatusLine()
{
	// Вычисление статуса
	uint16_t newcrc = _crc16(0xFFFF, HP.get_errcode());
	newcrc = _crc16(newcrc, (HP.IsWorkingNow() << 2) | (HP.get_BoilerON() << 1) | fPageID);
	newcrc = _crc16(newcrc, HP.get_modeHouse());
	if(newcrc != StatusCrc) { // поменялся
		StatusCrc = newcrc;

		if(!sendCommand("ref_stop")) return;      // Остановить обновление
		//   setComponentText("time", NowTimeToStr1());
		// Ошибки
		if(HP.get_errcode() == OK) {
			sendCommand("vis options,1");
			sendCommand("vis fault,0");
		} else {
			sendCommand("vis fault,1");
			sendCommand("vis options,0");
			if(PageID == 0) {
				Encode_UTF8_to_ISO8859_5(ntemp, "Ошибка ", sizeof(ntemp));
				_itoa(HP.get_errcode(), ntemp);
				setComponentText("fault", ntemp);
			}
		}

		if(PageID == 0) {
			if(HP.IsWorkingNow()) sendCommand("bt0.val=0");    // Кнопка включения в положение ВКЛ
			else sendCommand("bt0.val=1");    // Кнопка включения в положение ВЫКЛ
		} else if(PageID == 6) {
			if(HP.get_BoilerON()) sendCommand("gvson.val=1");    // Кнопка включения ГВС в положение ВКЛ
			else sendCommand("bt0.val=0");                    // Кнопка включения ГВС в положение ВЫКЛ
		}

		if(HP.IsWorkingNow()) {
			sendCommand("vis tninc,1");
			sendCommand("vis tnoff,0");
			switch((MODE_HP) HP.get_modeHouse()) {
			case pOFF:
				if(HP.get_BoilerON()) {
					sendCommand("vis gvs,0");
					sendCommand("vis onlygvs,1");
				} else {
					sendCommand("vis gvs,0");
					sendCommand("vis onlygvs,0");
				}
				break;
			case pHEAT:
				sendCommand("vis heat,1");
				sendCommand("vis cool,0");
				if(HP.get_BoilerON()) {
					sendCommand("vis gvs,1");
					sendCommand("vis onlygvs,0");
				} else {
					sendCommand("vis gvs,0");
					sendCommand("vis onlygvs,0");
				}
				break;
			case pCOOL:
				sendCommand("vis heat,0");
				sendCommand("vis cool,1");
				if(HP.get_BoilerON()) {
					sendCommand("vis gvs,1");
					sendCommand("vis onlygvs,0");
				} else {
					sendCommand("vis gvs,0");
					sendCommand("vis onlygvs,0");
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
			sendCommand("vis onlygvs,0");
		}
	}
	sendCommand("ref_star");    // Восстановить обновление

}

// Получить целевую температуру отопления
void Nextion::getTargetTemp(char *rstr)
{
	switch(HP.get_modeHouse())   // проверка отопления
	{
	case pHEAT:
		ftoa(rstr, (float) HP.get_targetTempHeat() / 100, 1);
		break;
	case pCOOL:
		ftoa(rstr, (float) HP.get_targetTempCool() / 100, 1);
		break;
	default:
		strcpy(rstr, "-.-");
	}
}

void Nextion::Encode_UTF8_to_ISO8859_5(char* outstr, const char* instr, uint16_t outsize)
{
	uint8_t c;
	while((c = *instr++)) {
		if(c > 0x7F) {
			uint16_t c2 = c * 256 + *instr++;
			if(c2 >= 0xD090 && c2 <= 0xD18F) c = c2 - 0xD090 + 0xB0;
		}
		*outstr++ = c;
		if(--outsize == 0) break;
	}
}
