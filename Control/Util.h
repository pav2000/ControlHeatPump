/*
 * Copyright (c) 2016-2020 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav
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
// --------------------------------------- функции общего использования -------------------------------------------
#ifndef Util_h
#define Util_h

#include <Arduino.h>

#define sign(a) (a > 0 ? 1 : a < 0 ? -1 : 0)
#define signm(a,t) ((a > 0 ? 1 : a < 0 ? -1 : 0) * (abs(a) > t ? 2 : 1))

uint16_t calc_crc16(unsigned char * pcBlock, unsigned short len, uint16_t crc = 0xFFFF);
void int_to_dec_str(int32_t value, int32_t div, char **ret, uint8_t maxfract);
uint8_t calc_bits_in_mask(uint32_t mask);
int32_t round_div_int32(int32_t value, int16_t div);
void getIDchip(char *outstr);
char* _itoa( int value, char *string);
uint32_t i32toa(int32_t value, char *string, uint8_t zero_pad);
uint8_t _ftoa(char *outstr, float val, unsigned char precision);
void _dtoa(char *outstr, int val, int precision);
char* NowTimeToStr(char *buf = NULL);
char* NowDateToStr(char *buf = NULL);

#endif
