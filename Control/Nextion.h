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
#ifndef __NEXTION_H__
#define __NEXTION_H__

#include <Arduino.h>
#include "Constant.h"                       // Вся конфигурация и константы проекта Должен быть первым !!!!

class Nextion{
 public:
  boolean init();
  void Update();
  void StatusLine();  
  void set_need_refresh() { fPageID = true; };    // установить необходимость обновления страницы
  void Listen();
  boolean setComponentText(const char* component, char* txt);
  void    readCommand();
  boolean sendCommand(const char* cmd);
  boolean check_incoming(void);
  void    refresh(uint8_t flags) { refresh_flags = flags; };	// Обновить нужные настройки
  void    Encode_UTF8_to_ISO8859_5(char* outstr, const char* instr, uint16_t outsize);
 private:
  int8_t  PageID;                               // Текущая страница
  boolean fPageID;                              // Признак смены страницы true
  uint8_t DataAvaliable;
  uint16_t StatusCrc;                            // Состояние ТН
  uint8_t refresh_flags;
  void StartON();
  void getTargetTemp(char *rstr);               // Получить целевую температуру
};
#endif






