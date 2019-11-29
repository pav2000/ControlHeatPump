/*
 * TwoWire.h - TWI/I2C library for Arduino Due
 *
 * Unblocking master send/receive (interrupt mode)
 * Rewritten by vad7.
 * https://github.com/vad7/Arduino-DUE-WireSam
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef TwoWireSam_h
#define TwoWireSam_h

// Include Atmel CMSIS driver
#include <include/twi.h>

#include "variant.h"

#define BUFFER_LENGTH 32

#define TWI_CLOCK 			100000  // default
// Timeouts
#define I2C_TIMEOUT 		3500  // ~5ms
#define I2C_TIMEOUT_MS 		5

 // WIRE_HAS_END means Wire has end()
#define WIRE_HAS_END 1

class TwoWire
{
public:
	TwoWire(Twi *twi, void(*begin_cb)(void), void(*end_cb)(void));
	void begin();
//	void begin(uint8_t address);
	void end();
	void Stop(void);
	void setClock(uint32_t frequency); // in Hz

	void beginTransmission(uint8_t address); // use write() to add bytes to the internal buffer
	void beginTransmission(uint8_t address, uint8_t *buffer, size_t quantity); // use write() and/or external buffer[quantity]; external will be send last.
	uint8_t endTransmission(void); // send immediately at first internal buffer, after external buffer
	uint8_t endTransmission(uint8_t use_RTOS_delay);
	uint8_t endTransmissionReceive(uint8_t use_RTOS_delay);
	uint8_t endTransmissionReceive(uint8_t *buffer, size_t quantity, uint8_t use_RTOS_delay); // after send redirect to requestFrom(..., quantity, buffer, ...)

	uint8_t requestFrom(uint8_t address, uint8_t quantity);
	uint8_t requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop);
	// use_RTOS_delay: 1 - RTOS_delay(1), 2 - exit, non-blocking, use TransmissionStatus() and available.
	size_t requestFrom(uint8_t address, uint8_t *buffer, size_t quantity, uint8_t use_RTOS_delay);

	uint8_t TransmissionStatus(void); // 0 - in process, 1 - ok, >2 - error
	uint8_t WaitTransmission(uint8_t use_RTOS_delay);
//	void WaitPreviousTask(uint8_t use_RTOS_delay);

	size_t write(uint8_t);
	size_t write(const uint8_t *, size_t);
	int read(void);
	int peek(void);
	void flush(void);
	size_t available(void);

	void onService(void);

private:

	// TWI instance
	Twi *twi;
	uint8_t txAddress;
	uint8_t _Buffer[BUFFER_LENGTH];
	uint32_t _BufferIndex;
	uint32_t _BufferLength;
	uint8_t *Buffer;
	size_t BufferMax;
	size_t BufferIndex;
	size_t BufferLength;
	uint8_t *ReceiveBuffer; // if NULL internal buffer used
	size_t ReceiveLength;	// if not zero - start receive after send
	uint8_t Error;	// 2..5
	// TWI clock frequency
	uint32_t twiClock;
	volatile uint8_t InterruptOccured;

	void requestFrom_start(void);

	// Service buffer
//	uint8_t srvBuffer[BUFFER_LENGTH];
//	uint8_t srvBufferIndex;
//	uint8_t srvBufferLength;

	// TWI state
	enum TwoWireStatus {
		UNINITIALIZED,
		MASTER_IDLE,
		MASTER_SEND,
		MASTER_RECV,
		SLAVE_IDLE,
		SLAVE_RECV,
		SLAVE_SEND
	};
	volatile uint8_t status;

	// Called before initialization
	void (*onBeginCallback)(void);
	// Called after deinitialization
	void (*onEndCallback)(void);

};

#if WIRE_INTERFACES_COUNT > 0
extern TwoWire Wire;
#endif
#if WIRE_INTERFACES_COUNT > 1
extern TwoWire Wire1;
#endif

#endif
