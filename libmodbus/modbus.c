/*
 * Phoenix-RTOS
 *
 * Modbus RTU master communication
 *
 * Copyright 2025 Phoenix Systems
 * Author: Mateusz Kobak
 *
 * %LICENSE%
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "buffer.h"
#include "modbus.h"


/*
 * Protocol function codes.
 * The code of the function that the server (slave) should execute.
 */
#define MODBUS_FUNC_TYPE_GET_HOLDING_REGISTERS (0x03)
#define MODBUS_FUNC_TYPE_GET_INPUT_REGISTERS   (0x04)
#define MODBUS_FUNC_TYPE_SET_SINGLE_REGISTER   (0x06)
#define MODBUS_FUNC_TYPE_SET_MULTI_REGISTERS   (0x10)

#define MODBUS_EXCEPTION_FLAG (0x80)


#define TRY_RAISE(expr__) \
	do { \
		modbus_status_t ret__ = expr__; \
		if (ret__ < 0) { \
			return ret__; \
		} \
	} while (0)


#define CHECK_RESPONSE(expr__) \
	do { \
		if (!(expr__)) { \
			return modbus_statusBadResponse; \
		} \
	} while (0)


static modbus_status_t parseException(modbus_t *ctx)
{
	uint8_t u8;
	TRY_RAISE(modbus_buffer_getU8(ctx, &u8));
	TRY_RAISE(modbus_buffer_checkCRC(ctx));
	ctx->exception = (modbus_exception_t)u8;
	return modbus_statusServerException;
}


static modbus_status_t getRegisters(modbus_t *ctx, uint8_t devAddr, uint16_t firstReg, uint8_t regNum, uint16_t *vals, uint8_t funType)
{
	TRY_RAISE(modbus_buffer_flush(ctx));
	modbus_buffer_clear(ctx);

	TRY_RAISE(modbus_buffer_putU8(ctx, devAddr));
	TRY_RAISE(modbus_buffer_putU8(ctx, funType));
	TRY_RAISE(modbus_buffer_putU16(ctx, firstReg));
	TRY_RAISE(modbus_buffer_putU16(ctx, regNum));
	TRY_RAISE(modbus_buffer_putCRC(ctx));

	TRY_RAISE(modbus_buffer_send(ctx));

	uint8_t u8;
	modbus_buffer_clear(ctx);
	modbus_buffer_startRead(ctx);

	TRY_RAISE(modbus_buffer_getU8(ctx, &u8));
	CHECK_RESPONSE(u8 == devAddr);

	TRY_RAISE(modbus_buffer_getU8(ctx, &u8));
	if (u8 == (MODBUS_EXCEPTION_FLAG | funType)) {
		return parseException(ctx);
	}
	CHECK_RESPONSE(u8 == funType);

	TRY_RAISE(modbus_buffer_getU8(ctx, &u8));
	CHECK_RESPONSE(u8 == 2 * regNum); /* Number of bytes in RX payload */

	for (uint8_t i = 0; i < regNum; i++) {
		TRY_RAISE(modbus_buffer_getU16(ctx, &vals[i]));
	}

	TRY_RAISE(modbus_buffer_checkCRC(ctx));

	return modbus_statusOk;
}


modbus_status_t modbus_readHoldingRegisters(modbus_t *ctx, uint8_t devAddr, uint16_t firstReg, uint8_t regNum, uint16_t *vals)
{
	return getRegisters(ctx, devAddr, firstReg, regNum, vals, MODBUS_FUNC_TYPE_GET_HOLDING_REGISTERS);
}


modbus_status_t modbus_readInputRegisters(modbus_t *ctx, uint8_t devAddr, uint16_t firstReg, uint8_t regNum, uint16_t *vals)
{
	return getRegisters(ctx, devAddr, firstReg, regNum, vals, MODBUS_FUNC_TYPE_GET_INPUT_REGISTERS);
}


modbus_status_t modbus_writeSingleRegister(modbus_t *ctx, uint8_t devAddr, uint16_t reg, uint16_t val)
{
	TRY_RAISE(modbus_buffer_flush(ctx));
	modbus_buffer_clear(ctx);

	TRY_RAISE(modbus_buffer_putU8(ctx, devAddr));
	TRY_RAISE(modbus_buffer_putU8(ctx, MODBUS_FUNC_TYPE_SET_SINGLE_REGISTER));
	TRY_RAISE(modbus_buffer_putU16(ctx, reg));
	TRY_RAISE(modbus_buffer_putU16(ctx, val));
	TRY_RAISE(modbus_buffer_putCRC(ctx));

	TRY_RAISE(modbus_buffer_send(ctx));

	uint8_t u8;
	uint16_t u16;
	modbus_buffer_clear(ctx);
	modbus_buffer_startRead(ctx);

	TRY_RAISE(modbus_buffer_getU8(ctx, &u8));
	CHECK_RESPONSE(u8 == devAddr);

	TRY_RAISE(modbus_buffer_getU8(ctx, &u8));
	if (u8 == (MODBUS_EXCEPTION_FLAG | MODBUS_FUNC_TYPE_SET_SINGLE_REGISTER)) {
		return parseException(ctx);
	}
	CHECK_RESPONSE(u8 == MODBUS_FUNC_TYPE_SET_SINGLE_REGISTER);

	TRY_RAISE(modbus_buffer_getU16(ctx, &u16));
	CHECK_RESPONSE(u16 == reg);

	TRY_RAISE(modbus_buffer_getU16(ctx, &u16));
	CHECK_RESPONSE(u16 == val);

	TRY_RAISE(modbus_buffer_checkCRC(ctx));

	return modbus_statusOk;
}


modbus_status_t modbus_writeMultiRegister(modbus_t *ctx, uint8_t devAddr, uint16_t firstReg, uint8_t regNum, const uint16_t *vals)
{
	TRY_RAISE(modbus_buffer_flush(ctx));
	modbus_buffer_clear(ctx);

	TRY_RAISE(modbus_buffer_putU8(ctx, devAddr));
	TRY_RAISE(modbus_buffer_putU8(ctx, MODBUS_FUNC_TYPE_SET_MULTI_REGISTERS));
	TRY_RAISE(modbus_buffer_putU16(ctx, firstReg));
	TRY_RAISE(modbus_buffer_putU16(ctx, regNum));
	TRY_RAISE(modbus_buffer_putU8(ctx, 2 * regNum));

	for (uint8_t i = 0; i < regNum; i++) {
		TRY_RAISE(modbus_buffer_putU16(ctx, vals[i]));
	}

	TRY_RAISE(modbus_buffer_putCRC(ctx));

	TRY_RAISE(modbus_buffer_send(ctx));

	uint8_t u8;
	uint16_t u16;
	modbus_buffer_clear(ctx);
	modbus_buffer_startRead(ctx);

	TRY_RAISE(modbus_buffer_getU8(ctx, &u8));
	CHECK_RESPONSE(u8 == devAddr);

	TRY_RAISE(modbus_buffer_getU8(ctx, &u8));
	if (u8 == (MODBUS_EXCEPTION_FLAG | MODBUS_FUNC_TYPE_SET_MULTI_REGISTERS)) {
		return parseException(ctx);
	}
	CHECK_RESPONSE(u8 == MODBUS_FUNC_TYPE_SET_MULTI_REGISTERS);

	TRY_RAISE(modbus_buffer_getU16(ctx, &u16));
	CHECK_RESPONSE(u16 == firstReg);

	TRY_RAISE(modbus_buffer_getU16(ctx, &u16));
	CHECK_RESPONSE(u16 == regNum);

	TRY_RAISE(modbus_buffer_checkCRC(ctx));

	return modbus_statusOk;
}


void modbus_setTimeouts(modbus_t *ctx, uint32_t readTimeoutMs, uint32_t writeTimeoutMs)
{
	ctx->readTimeoutMs = readTimeoutMs;
	ctx->writeTimeoutMs = writeTimeoutMs;
}


modbus_exception_t modbus_getLastException(modbus_t *ctx)
{
	return ctx->exception;
}


void modbus_init(modbus_t *ctx, const modbus_callbacks_t *cbs)
{
	ctx->exception = modbus_exceptionNone;
	ctx->cb = *cbs;
}
