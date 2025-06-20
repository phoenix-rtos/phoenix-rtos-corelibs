/*
 * Phoenix-RTOS
 *
 * Modbus RTU master (client) communication
 *
 * Copyright 2025 Phoenix Systems
 * Author: Mateusz Kobak
 *
 * %LICENSE%
 */

#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H


#include <stddef.h>
#include <stdint.h>


/* NOTE: For now only Modbus RTU over serial line is supported. */


#define MODBUS_BUFFER_SIZE (256) /* Max size of a frame in Modbus RTU over serial line */


typedef enum {
	modbus_statusOk = 0,
	modbus_statusServerException = -1, /* Type of exception can be acquired by calling modbus_getLastException */
	modbus_statusCommunicationError = -2,
	modbus_statusBadResponse = -3,
	modbus_statusWrongCrc = -4,
	modbus_statusTimedOut = -5,
	modbus_statusOtherError = -6,
} modbus_status_t;


typedef enum {
	modbus_exceptionNone = 0x00,
	modbus_exceptionIllegalFunction = 0x01,
	modbus_exceptionIllegalDataAddress = 0x02,
	modbus_exceptionIllegalDataValue = 0x03,
	modbus_exceptionSeverDeviceFailure = 0x04,
	modbus_exceptionAcknowledge = 0x05,
	modbus_exceptionServerDeviceBusy = 0x06,
	modbus_exceptionNegativeAcknowledge = 0x07,
	modbus_exceptionMemoryParity = 0x08,
	modbus_exceptionGatewayPathUnavailable = 0x0a,
	modbus_exceptionGatewayTargetFailedToRespond = 0x0b,
} modbus_exception_t;


typedef struct {
	/*
	 * Tries to read at least bytesToRead bytes to the buffer.
	 * The number of bytes read could not exceed the buflen param.
	 * When function returns less then bytesToRead bytes, it is treated as timeout.
	 * If timeoutMs==0, then nonblocking read should be performed.
	 * Returns number of bytes read, or <0 on errors.
	 */
	int (*read)(uint8_t *buf, size_t buflen, size_t bytesToRead, uint32_t timeoutMs, void *args);

	int (*write)(const uint8_t *buf, size_t len, uint32_t timeoutMs, void *args);

	void *userArgs;
} modbus_callbacks_t;


typedef struct {
	uint8_t buf[MODBUS_BUFFER_SIZE];
	size_t roffs;
	size_t woffs;
} modbus_buffer_t;


typedef struct {
	modbus_buffer_t buf;
	modbus_callbacks_t cb;

	uint32_t readTimeoutMs;
	uint32_t writeTimeoutMs;
	uint64_t readStartMs;

	modbus_exception_t exception; /* Last server response exception */
} modbus_t;


modbus_status_t modbus_readHoldingRegisters(modbus_t *ctx, uint8_t devAddr, uint16_t firstReg, uint8_t regNum, uint16_t *vals);


modbus_status_t modbus_readInputRegisters(modbus_t *ctx, uint8_t devAddr, uint16_t firstReg, uint8_t regNum, uint16_t *vals);


modbus_status_t modbus_writeSingleRegister(modbus_t *ctx, uint8_t devAddr, uint16_t reg, uint16_t val);


modbus_status_t modbus_writeMultiRegister(modbus_t *ctx, uint8_t devAddr, uint16_t firstReg, uint8_t regNum, const uint16_t *vals);


void modbus_setTimeouts(modbus_t *ctx, uint32_t readTimeoutMs, uint32_t writeTimeoutMs);


modbus_exception_t modbus_getLastException(modbus_t *ctx);


void modbus_init(modbus_t *ctx, const modbus_callbacks_t *cbs);


#endif /* MODBUS_RTU_H */
