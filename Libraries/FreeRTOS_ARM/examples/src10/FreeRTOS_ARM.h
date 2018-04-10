/**
 * file
 * brief FreeRTOS for Teensy 3.x and Due
 */
#ifndef FreeRTOS_ARM_h
#define FreeRTOS_ARM_h

#define PIN_LED_ERROR  38    // pav2000 по умолчанию забивать 13

#ifndef __arm__
  #error ARM Due or Teensy 3.x required
#else  // __arm__
//------------------------------------------------------------------------------
/** FreeRTOS_ARM version YYYYMMDD */
#define FREE_RTOS_ARM_VERSION 20171128
//------------------------------------------------------------------------------
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "portmacro.h"
//#include "cmsis_os.h"
#endif  // __arm__
#endif  // FreeRTOS_ARM_h