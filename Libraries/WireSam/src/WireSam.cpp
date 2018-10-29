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

extern "C" {
#include <string.h>
}

#include "WireSam.h"

//extern void _delay(int ms); // RTOS external delay - 1 ms
//#define RTOS_delay(ms) _delay(ms)
#include "FreeRTOS_ARM.h"
#define RTOS_delay(ms) { if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) vTaskDelay(ms/portTICK_PERIOD_MS); else delay(ms); }

TwoWire::TwoWire(Twi *_twi, void(*_beginCb)(void), void(*_endCb)(void)) :
	twi(_twi), txAddress(0), _BufferIndex(0), _BufferLength(0),
	Buffer(NULL), BufferMax(0), BufferIndex(0), BufferLength(0),
	ReceiveBuffer(NULL), ReceiveLength(0), Error(0), twiClock(TWI_CLOCK), InterruptOccured(0), status(UNINITIALIZED),
	onBeginCallback(_beginCb), onEndCallback(_endCb) {
}

void TwoWire::begin(void) {
	// TwoWire interrupt run at lower priority than other arduino interrupts
	if(NVIC_GetPriorityGrouping() == 0) {
		NVIC_SetPriorityGrouping(3); // Groups 16, Sub 1
	}
	if(onBeginCallback) onBeginCallback();

 //   if(status == MASTER_IDLE) return; // skip multiply init

	// Disable PDC channel
	twi->TWI_PTCR = TWI_PTCR_RXTDIS | TWI_PTCR_TXTDIS;

	TWI_ConfigureMaster(twi, twiClock, VARIANT_MCK);
	status = MASTER_IDLE;
}

/* Slave not need yet
void TwoWire::begin(uint8_t address) {
	if(onBeginCallback) onBeginCallback();
	// Disable PDC channel
	twi->TWI_PTCR = TWI_PTCR_RXTDIS | TWI_PTCR_TXTDIS;

	TWI_ConfigureSlave(twi, address);
	status = SLAVE_IDLE;
	TWI_EnableIt(twi, TWI_IER_SVACC);
	//| TWI_IER_RXRDY | TWI_IER_TXRDY	| TWI_IER_TXCOMP);
}

void TwoWire::begin(int address) {
	begin((uint8_t) address);
}
*/

void TwoWire::end(void) {
	TWI_Disable(twi);
	// Enable PDC channel
	twi->TWI_PTCR &= ~(TWI_PTCR_RXTDIS | TWI_PTCR_TXTDIS);
	if(onEndCallback) onEndCallback();
}

// in Hz
void TwoWire::setClock(uint32_t frequency) {
	twiClock = frequency;
	TWI_SetClock(twi, twiClock, VARIANT_MCK);
}

// 0 - transmitting, 1 - transmitted ok,
// 2 - got NACK on address transmit, 3 - got NACK during data transmmit, 4 - got NACK when finishing
uint8_t TwoWire::TransmissionStatus(void) {
	return status == MASTER_IDLE ? Error ? Error : 1 : 0;
}

// Return 0 if success, otherwise error
__attribute__((always_inline)) inline uint8_t TwoWire::WaitTransmission(uint8_t use_RTOS_delay)
{
	uint8_t ret;
	uint16_t _timeout;
	InterruptOccured = 1;
	while((ret = TransmissionStatus()) == 0) {
		if(use_RTOS_delay) { RTOS_delay(1); }
		if(InterruptOccured) {
			_timeout = use_RTOS_delay ? I2C_TIMEOUT_MS : I2C_TIMEOUT;
			InterruptOccured = 0;
		}
		if(--_timeout == 0) {
			Stop();
			ret = 5;
			break;
		}
	}
	if(ret == 1) ret = 0;
	return ret;
}

void TwoWire::requestFrom_start(void) {
    TWI_EnableIt(twi, TWI_IER_RXRDY);
	TWI_StartRead(twi, txAddress, 0, 0); // iaddress = 0, isize = 0
	if(BufferMax == 1) TWI_SendSTOPCondition(twi); // Stop condition must be set during the reception of last byte
}

// Returns read bytes (quantity less or equal BUFFER_LENGTH)
uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity) {
	return requestFrom(address, NULL, quantity, 0);
}

// Returns read bytes (quantity less or equal BUFFER_LENGTH)
uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop) {
	return requestFrom(address, NULL, quantity, 0);
}

// Returns read bytes
// use_RTOS_delay: 1 - RTOS_delay(1), 2 - exit, non-blocking, use TransmissionStatus() and available().
size_t TwoWire::requestFrom(uint8_t address, uint8_t *buffer, size_t quantity, uint8_t use_RTOS_delay) {
	if(buffer == NULL) { // use local buffer
		if (quantity > BUFFER_LENGTH) quantity = BUFFER_LENGTH;
		Buffer = _Buffer;
	} else Buffer = buffer;

	txAddress = address;
	BufferMax = quantity;
	BufferIndex = 0;
	BufferLength = 0;
	Error = 0;
	status = MASTER_RECV;

	requestFrom_start();

	if(use_RTOS_delay == 2) return 0; // non-blocking - exiting

	// WaitTransmission(use_RTOS_delay);
	// use inline function:
	uint8_t ret;
	uint16_t _timeout = 0;
	InterruptOccured = 1;
	while((ret = TransmissionStatus()) == 0) {
		if(use_RTOS_delay) { RTOS_delay(1); }
		if(InterruptOccured) {
			_timeout = use_RTOS_delay ? I2C_TIMEOUT_MS : I2C_TIMEOUT;
			InterruptOccured = 0;
		}
		if(--_timeout == 0) {
			Stop();
			break;
		}
	}
	// use inline function.
	return BufferLength;
}

// Prepare transmission
void TwoWire::beginTransmission(uint8_t address) {
	txAddress = address;
	_BufferIndex = 0;
	_BufferLength = 0;
	Buffer = NULL;
	BufferMax = 0;
	BufferIndex = 0;
	BufferLength = 0;
	ReceiveBuffer = NULL;
	ReceiveLength = 0;
	Error = 0;
	status = MASTER_SEND;
}

// Prepare transmission
// Send buffer[quantity]
void TwoWire::beginTransmission(uint8_t address, uint8_t *buffer, size_t quantity) {
	beginTransmission(address);
	if(buffer) { // use external buffer
		Buffer = buffer;
		BufferLength = quantity;
	}
}

/*uint8_t TwoWire::endTransmission(uint8_t sendStop) { // not recommended for use
	return endTransmissionReceive(0);
}*/

// Start transmission, return 0 when success
uint8_t TwoWire::endTransmission(void) {
	return endTransmissionReceive(0);
}

// Start transmission
uint8_t TwoWire::endTransmission(uint8_t use_RTOS_delay) {
	return endTransmissionReceive(use_RTOS_delay);
}

// Start transmission and receiving after, return 0 when success
// use_RTOS_delay: 1 - RTOS_delay(1), 2 - exit, non-blocking, use TransmissionStatus() and available().
uint8_t TwoWire::endTransmissionReceive(uint8_t *buffer, size_t quantity, uint8_t use_RTOS_delay)
{
	if(buffer == NULL) {
		ReceiveBuffer = _Buffer;
		if (quantity > BUFFER_LENGTH) quantity = BUFFER_LENGTH;
	} else ReceiveBuffer = buffer;
	ReceiveLength = quantity;

	return endTransmissionReceive(use_RTOS_delay);
}

// Start transmission, return 0 when success
uint8_t TwoWire::endTransmissionReceive(uint8_t use_RTOS_delay)
{
	TWI_StartWrite(twi, txAddress, 0, 0, _BufferLength ? _Buffer[_BufferIndex++] : BufferLength ? Buffer[BufferIndex++] : 0); // iaddress = 0, isize = 0
	TWI_EnableIt(twi, TWI_IER_TXRDY | TWI_IER_NACK);

	if(use_RTOS_delay == 2) return 0; // non-blocking - exiting

	// return WaitTransmission(use_RTOS_delay);
	// use inline function:
	uint8_t ret;
	uint16_t _timeout = 0;
	InterruptOccured = 1;
	while((ret = TransmissionStatus()) == 0) {
		if(use_RTOS_delay) { RTOS_delay(1); }
		if(InterruptOccured) {
			_timeout = use_RTOS_delay ? I2C_TIMEOUT_MS : I2C_TIMEOUT;
			InterruptOccured = 0;
		}
		if(--_timeout == 0) {
			Stop();
			ret = 5;
			break;
		}
	}
	if(ret == 1) ret = 0;
	return ret;
	// use inline function.
}

// Return number of bytes added to send buffer
size_t TwoWire::write(uint8_t data) {
//	uint8_t b = data;
//	return write(&b, 1);
// Stack optimized:
	if (status == MASTER_SEND) {
		if (_BufferLength >= BUFFER_LENGTH)	return 0;
		_Buffer[_BufferLength++] = data;
/* Not need yet
	} else {
		for (size_t i = 0; i < quantity; ++i) {
			if (srvBufferLength >= BUFFER_LENGTH)
				return i;
			srvBuffer[srvBufferLength++] = data[i];
		}
*/
	}
	return 1;
}

// Return number of bytes added to send buffer
size_t TwoWire::write(const uint8_t *data, size_t quantity) {
	if (status == MASTER_SEND) {
		for (size_t i = 0; i < quantity; ++i) {
			if (_BufferLength >= BUFFER_LENGTH) return i;
			_Buffer[_BufferLength++] = data[i];
		}
/* Not need yet
	} else {
		for (size_t i = 0; i < quantity; ++i) {
			if (srvBufferLength >= BUFFER_LENGTH) return i;
			srvBuffer[srvBufferLength++] = data[i];
		}
*/
	}
	return quantity;
}

size_t TwoWire::available(void) {
	return BufferLength - BufferIndex;
}

int TwoWire::read(void) {
	if(BufferIndex < BufferLength)
		return Buffer[BufferIndex++];
	return -1;
}

int TwoWire::peek(void) {
	if(BufferIndex < BufferLength)
		return Buffer[BufferIndex];
	return -1;
}

void TwoWire::flush(void) {
	// Do nothing, use endTransmission(..) to force
	// data transfer.
}

void TwoWire::Stop(void)
{
	TWI_DisableIt(twi, TWI_IER_TXRDY | TWI_IER_NACK | TWI_IER_TXCOMP | TWI_SR_RXRDY);
	TWI_Stop(twi);
	status = MASTER_IDLE;
}

void TwoWire::onService(void) {
	InterruptOccured = 1;
	// Retrieve interrupt status
	uint32_t sr = TWI_GetStatus(twi);
	if(status == MASTER_SEND) {
		if(_BufferLength) { // if local buffer is not empty - use it
			if(sr & TWI_SR_TXCOMP) {
				if(sr & TWI_SR_NACK) {
					 // 2 - got NACK on address transmit, 3 - got NACK during data transmmit, 4 - got NACK when finishing
					if(_BufferIndex == 1) Error = 2;
					else if(_BufferIndex == _BufferLength) Error = 4;
					else Error = 3;
				}
				status = MASTER_IDLE;
			} else if(sr & TWI_SR_TXRDY) {
				if(_BufferIndex < _BufferLength) {
					TWI_WriteByte(twi, _Buffer[_BufferIndex++]);
					if(_BufferIndex == _BufferLength && BufferLength) {
						_BufferLength = 0; // continue transfer external buffer
					}
				} else {
					TWI_DisableIt(twi, TWI_IER_TXRDY);
					TWI_Stop(twi);
				}
				TWI_EnableIt(twi, TWI_IER_TXCOMP);
			}
		} else {
			if(sr & TWI_SR_TXCOMP) {
				if(sr & TWI_SR_NACK) {
					 // 2 - got NACK on address transmit, 3 - got NACK during data transmmit, 4 - got NACK when finishing
					if(_BufferIndex + BufferIndex <= 1U) Error = 2;
					else if(BufferIndex == BufferLength) Error = 4;
					else Error = 3;
				}
				BufferLength = 0;		// empty buffer
				status = MASTER_IDLE;
			} else if(sr & TWI_SR_TXRDY) {
				if(BufferIndex < BufferLength && Buffer) {
					TWI_WriteByte(twi, Buffer[BufferIndex++]);
				} else {
					TWI_DisableIt(twi, TWI_IER_TXRDY);
					TWI_Stop(twi);
				}
				TWI_EnableIt(twi, TWI_IER_TXCOMP);
			}
		}
		if(status == MASTER_IDLE) {
			TWI_DisableIt(twi, TWI_IER_TXRDY | TWI_IER_NACK | TWI_IER_TXCOMP | TWI_SR_RXRDY);
			_BufferLength = 0;		// empty buffer
			if(Error == 0 && ReceiveLength != 0) { // Need receive after
				status = MASTER_RECV;
				Buffer = ReceiveBuffer;
				BufferMax = ReceiveLength;
				BufferIndex = 0;
				BufferLength = 0;
				requestFrom_start();
			}
		}
    } else if(status == MASTER_RECV) {
		if(sr & TWI_SR_RXRDY) {
			if(BufferLength < BufferMax && Buffer) {
				Buffer[BufferLength++] = TWI_ReadByte(twi);
				if(BufferLength == BufferMax - 1) {
					TWI_SendSTOPCondition(twi); // Stop condition must be set during the reception of last byte
				}
				TWI_EnableIt(twi, TWI_IER_TXCOMP);
			} else {
				TWI_Stop(twi);
				status = MASTER_IDLE;
			}
		}
		if(sr & TWI_SR_TXCOMP) status = MASTER_IDLE;
	    if(status == MASTER_IDLE) TWI_DisableIt(twi, TWI_IER_TXRDY | TWI_IER_NACK | TWI_IER_TXCOMP | TWI_SR_RXRDY);
/* Slave not need yet
    } else {
		if (status == SLAVE_IDLE && (sr & TWI_SR_SVACC)) {
			TWI_DisableIt(twi, TWI_IDR_SVACC);
			TWI_EnableIt(twi, TWI_IER_RXRDY | TWI_IER_GACC | TWI_IER_NACK
					| TWI_IER_EOSACC | TWI_IER_SCL_WS | TWI_IER_TXCOMP);

			srvBufferLength = 0;
			srvBufferIndex = 0;

			// Detect if we should go into RECV or SEND status
			// SVREAD==1 means *master* reading -> SLAVE_SEND
			if (!(sr & TWI_SR_SVREAD)) {
				status = SLAVE_RECV;
			} else {
				status = SLAVE_SEND;

				// Alert calling program to generate a response ASAP
				if (onRequestCallback)
					onRequestCallback();
				else
					// create a default 1-byte response
					write((uint8_t) 0);
			}
		}

		if (status != SLAVE_IDLE && (sr & TWI_SR_EOSACC)) {
			if (status == SLAVE_RECV && onReceiveCallback) {
				// Copy data into rxBuffer
				// (allows to receive another packet while the
				// user program reads actual data)
				for (uint8_t i = 0; i < srvBufferLength; ++i)
					rxBuffer[i] = srvBuffer[i];
				rxBufferIndex = 0;
				rxBufferLength = srvBufferLength;

				// Alert calling program
				onReceiveCallback( rxBufferLength);
			}

			// Transfer completed
			TWI_EnableIt(twi, TWI_SR_SVACC);
			TWI_DisableIt(twi, TWI_IDR_RXRDY | TWI_IDR_GACC | TWI_IDR_NACK
					| TWI_IDR_EOSACC | TWI_IDR_SCL_WS | TWI_IER_TXCOMP);
			status = SLAVE_IDLE;
		}

		if (status == SLAVE_RECV) {
			if (TWI_STATUS_RXRDY(sr)) {
				if (srvBufferLength < BUFFER_LENGTH)
					srvBuffer[srvBufferLength++] = TWI_ReadByte(twi);
			}
		}

		if (status == SLAVE_SEND) {
			if (TWI_STATUS_TXRDY(sr) && !(sr & TWI_SR_NACK)) {
				uint8_t c = 'x';
				if (srvBufferIndex < srvBufferLength)
					c = srvBuffer[srvBufferIndex++];
				TWI_WriteByte(twi, c);
			}
		}
*/
    }
}

#if WIRE_INTERFACES_COUNT > 0
static void Wire_Init(void) {
	pmc_enable_periph_clk(WIRE_INTERFACE_ID);
	PIO_Configure(
			g_APinDescription[PIN_WIRE_SDA].pPort,
			g_APinDescription[PIN_WIRE_SDA].ulPinType,
			g_APinDescription[PIN_WIRE_SDA].ulPin,
			g_APinDescription[PIN_WIRE_SDA].ulPinConfiguration);
	PIO_Configure(
			g_APinDescription[PIN_WIRE_SCL].pPort,
			g_APinDescription[PIN_WIRE_SCL].ulPinType,
			g_APinDescription[PIN_WIRE_SCL].ulPin,
			g_APinDescription[PIN_WIRE_SCL].ulPinConfiguration);

	NVIC_DisableIRQ(WIRE_ISR_ID);
	NVIC_ClearPendingIRQ(WIRE_ISR_ID);
	NVIC_SetPriority(WIRE_ISR_ID, 0);
	NVIC_EnableIRQ(WIRE_ISR_ID);
}

static void Wire_Deinit(void) {
	NVIC_DisableIRQ(WIRE_ISR_ID);
	NVIC_ClearPendingIRQ(WIRE_ISR_ID);

	pmc_disable_periph_clk(WIRE_INTERFACE_ID);

	// no need to undo PIO_Configure, 
	// as Peripheral A was enable by default before,
	// and pullups were not enabled
}

TwoWire Wire = TwoWire(WIRE_INTERFACE, Wire_Init, Wire_Deinit);

void WIRE_ISR_HANDLER(void) {
	Wire.onService();
}
#endif

#if WIRE_INTERFACES_COUNT > 1
static void Wire1_Init(void) {
	pmc_enable_periph_clk(WIRE1_INTERFACE_ID);
	PIO_Configure(
			g_APinDescription[PIN_WIRE1_SDA].pPort,
			g_APinDescription[PIN_WIRE1_SDA].ulPinType,
			g_APinDescription[PIN_WIRE1_SDA].ulPin,
			g_APinDescription[PIN_WIRE1_SDA].ulPinConfiguration);
	PIO_Configure(
			g_APinDescription[PIN_WIRE1_SCL].pPort,
			g_APinDescription[PIN_WIRE1_SCL].ulPinType,
			g_APinDescription[PIN_WIRE1_SCL].ulPin,
			g_APinDescription[PIN_WIRE1_SCL].ulPinConfiguration);

	NVIC_DisableIRQ(WIRE1_ISR_ID);
	NVIC_ClearPendingIRQ(WIRE1_ISR_ID);
	NVIC_SetPriority(WIRE1_ISR_ID, 0);
	NVIC_EnableIRQ(WIRE1_ISR_ID);
}

static void Wire1_Deinit(void) {
	NVIC_DisableIRQ(WIRE1_ISR_ID);
	NVIC_ClearPendingIRQ(WIRE1_ISR_ID);

	pmc_disable_periph_clk(WIRE1_INTERFACE_ID);

	// no need to undo PIO_Configure, 
	// as Peripheral A was enable by default before,
	// and pullups were not enabled
}

TwoWire Wire1 = TwoWire(WIRE1_INTERFACE, Wire1_Init, Wire1_Deinit);

void WIRE1_ISR_HANDLER(void) {
	Wire1.onService();
}
#endif
