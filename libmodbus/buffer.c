/*
 * Phoenix-RTOS
 *
 * Modbus buffer helpers
 *
 * Copyright 2025 Phoenix Systems
 * Author: Mateusz Kobak
 *
 * %LICENSE%
 */

#include <time.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>

#include "buffer.h"


static uint64_t getTimeMonoMs(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / (1000 * 1000);
}


/*
 * Returns the number of milliseconds remaining till timeout,
 * or 0, when already timed out.
 */
static uint32_t checkTimeout(modbus_t *ctx)
{
	uint64_t nowMs = getTimeMonoMs();

	if (ctx->readStartMs + ctx->readTimeoutMs <= nowMs) {
		return 0;
	}

	uint32_t ret = (uint32_t)(ctx->readStartMs + ctx->readTimeoutMs - nowMs);
	assert(ret <= ctx->readTimeoutMs);
	return ret;
}


/* CRC-16/MODBUS: POLY=0x8005, INIT=0xFFFF, RESULT_XOR=0x0000 */
static uint16_t computeCrc(uint8_t *buf, size_t len)
{
	uint16_t crc = 0xffff;

	for (int pos = 0; pos < len; pos++) {
		crc ^= (uint16_t)buf[pos];

		for (int i = 8; i != 0; i--) {
			if ((crc & 0x0001) != 0) {
				crc >>= 1;
				crc ^= 0xa001;
			}
			else {
				crc >>= 1;
			}
		}
	}

	return (crc << 8) | (crc >> 8);
}


void modbus_buffer_startRead(modbus_t *ctx)
{
	ctx->readStartMs = getTimeMonoMs();
}


modbus_status_t modbus_buffer_flush(modbus_t *ctx)
{
	int ret;
	uint8_t temp[8];
	size_t bytesRead = 0;

	do {
		ret = ctx->cb.read(temp, sizeof(temp), 1, 0, ctx->cb.userArgs);
		if (ret > 0) {
			bytesRead += ret;
		}
		if (bytesRead > MODBUS_BUFFER_SIZE) {
			return modbus_statusCommunicationError;
		}
	} while (ret > 0);

	return modbus_statusOk;
}


static modbus_status_t bufferRecv(modbus_t *ctx, size_t len)
{
	uint32_t timeRemaining = checkTimeout(ctx);
	if (timeRemaining == 0) {
		return modbus_statusTimedOut;
	}

	int ret = ctx->cb.read(ctx->buf.buf + ctx->buf.woffs, sizeof(ctx->buf.buf) - ctx->buf.woffs, len, timeRemaining, ctx->cb.userArgs);
	if (ret < 0) {
		return modbus_statusCommunicationError;
	}

	ctx->buf.woffs += ret;

	if (ret < len) {
		return modbus_statusTimedOut;
	}

	return modbus_statusOk;
}


modbus_status_t modbus_buffer_send(modbus_t *ctx)
{
	int ret = ctx->cb.write(ctx->buf.buf, ctx->buf.woffs, ctx->writeTimeoutMs, ctx->cb.userArgs);
	if (ret < 0) {
		return modbus_statusCommunicationError;
	}
	else if (ret < ctx->buf.woffs) {
		return modbus_statusTimedOut;
	}
	else {
		return modbus_statusOk;
	}
}


static modbus_status_t ensureRead(modbus_t *ctx, uint8_t len)
{
	uint8_t bytesAvailable = ctx->buf.woffs - ctx->buf.roffs;
	if (bytesAvailable >= len) {
		return modbus_statusOk;
	}

	uint8_t bytesNeeded = len - bytesAvailable;
	return bufferRecv(ctx, bytesNeeded);
}


modbus_status_t modbus_buffer_getU8(modbus_t *ctx, uint8_t *out)
{
	modbus_status_t ret = ensureRead(ctx, 1);
	if (ret < 0) {
		return ret;
	}

	*out = ctx->buf.buf[ctx->buf.roffs];
	ctx->buf.roffs += 1;
	return modbus_statusOk;
}


modbus_status_t modbus_buffer_getU16(modbus_t *ctx, uint16_t *out)
{
	modbus_status_t ret = ensureRead(ctx, 2);
	if (ret < 0) {
		return ret;
	}

	*out = ((uint16_t)ctx->buf.buf[ctx->buf.roffs] << 8) + ctx->buf.buf[ctx->buf.roffs + 1];
	ctx->buf.roffs += 2;
	return modbus_statusOk;
}


modbus_status_t modbus_buffer_putU8(modbus_t *ctx, uint8_t val)
{
	if (MODBUS_BUFFER_SIZE - ctx->buf.woffs < 1) {
		return modbus_statusOtherError;
	}

	ctx->buf.buf[ctx->buf.woffs] = val;
	ctx->buf.woffs += 1;
	return modbus_statusOk;
}


modbus_status_t modbus_buffer_putU16(modbus_t *ctx, uint16_t val)
{
	if (MODBUS_BUFFER_SIZE - ctx->buf.woffs < 2) {
		return modbus_statusOtherError;
	}

	/* Modbus treats each 16-bit register as big-endian */
	ctx->buf.buf[ctx->buf.woffs] = (val >> 8) & 0xff;
	ctx->buf.buf[ctx->buf.woffs + 1] = val & 0xff;
	ctx->buf.woffs += 2;
	return modbus_statusOk;
}


modbus_status_t modbus_buffer_putCRC(modbus_t *ctx)
{
	uint16_t crc = computeCrc(ctx->buf.buf, ctx->buf.woffs);
	return modbus_buffer_putU16(ctx, crc);
}


modbus_status_t modbus_buffer_checkCRC(modbus_t *ctx)
{
	uint16_t calcCrc = computeCrc(ctx->buf.buf, ctx->buf.roffs);
	uint16_t recvCrc;

	modbus_status_t ret = modbus_buffer_getU16(ctx, &recvCrc);
	if (ret < 0) {
		return ret;
	}

	return calcCrc == recvCrc ? modbus_statusOk : modbus_statusWrongCrc;
}


void modbus_buffer_clear(modbus_t *ctx)
{
	memset(ctx->buf.buf, 0x0, MODBUS_BUFFER_SIZE);
	ctx->buf.roffs = 0;
	ctx->buf.woffs = 0;
	ctx->readStartMs = 0;
}
