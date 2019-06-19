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
#ifndef __NEXTION_H__
#define __NEXTION_H__

#include <Arduino.h>
#include "Constant.h"

class Nextion {
public:
  boolean init();
  void    init_display();
  void    Update();
  void	  set_need_refresh() { fUpdate = 2; };	// обновить дисплей
  boolean check_incoming(void);
  void    readCommand();
  boolean sendCommand(const char* cmd);
  boolean setComponentText(const char* component, char* txt);
  boolean setComponentIdxText(const char* component, uint8_t idx, char* txt);
  boolean setComponentValIdx(const char* component, uint8_t idx, int8_t val);
  boolean sendCommandToComponentIdx(const char* component, const char* cmd, uint8_t idx, int8_t val);
  void    set_dim(uint8_t dim);					// Установить яркость экрана
  void    Encode_UTF8_to_ISO8859_5(char* outstr, const char* instr, uint16_t outstrsize);
private:
  int8_t   PageID;								// Текущая страница
  uint8_t  fUpdate;								// Признак обновления: 0 - нет, 1 - штатно, 2 - обновить срочно
  uint8_t  DataAvaliable;
  uint16_t StatusCrc;
  uint8_t  flags;
  uint8_t  input_delay;							// Delay for * NEXTION_READ ms
  void    getTargetTemp(char *rstr);			// Получить целевую температуру
  void    StatusLine();
};
#endif






