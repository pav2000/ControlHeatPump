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
#ifndef __NEXTION_H__
#define __NEXTION_H__

#include <Arduino.h>
#include "Constant.h"                       // Вся конфигурация и константы проекта Должен быть первым !!!!

class Nextion{
 private:
  int8_t  PageID;                               // Текущая страница
  boolean fPageID;                              // Признак смены страницы true
  int8_t Status;                                // Состояние ТН
  void flushSerial();
  void StartON();
  int16_t getTargetTemp();                      // Получить целевую температуру
  public:
  Nextion(){};//Empty contructor
  void Update();
  void StatusLine();  
  void set_fPageID() {fPageID=true;} ;           // установить необходимость обновления страницы
  void Listen();
  boolean ack(void);
  boolean setComponentText(char* component, char* txt);
  String readCommand();
  void sendCommand(const char* cmd);
  int8_t pageId(void);
  boolean init(const char* pageId = cZero);
//  void buttonToggle(boolean &buttonState, String objName, uint8_t picDefualtId, uint8_t picPressedId);
//  uint8_t buttonOnOff(String find_component, String unknown_component, uint8_t pin, int btn_prev_state);
//  boolean setComponentValue(String component, int value);
//  unsigned int getComponentValue(String component);
//  String getComponentText(String component, uint32_t timeout = 100);
//  boolean updateProgressBar(int x, int y, int maxWidth, int maxHeight, int value, int emptyPictureID, int fullPictureID, int orientation=0);
//  String listenNextionGeneric(unsigned long timeout=100);
};
#endif






