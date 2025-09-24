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

#ifndef MODBUS_BUFFER_H
#define MODBUS_BUFFER_H

#include <string.h>
#include <stdint.h>

#include "modbus.h"


modbus_status_t modbus_buffer_putU8(modbus_t *ctx, uint8_t val);


modbus_status_t modbus_buffer_putU16(modbus_t *ctx, uint16_t val);


modbus_status_t modbus_buffer_putCRC(modbus_t *ctx);


modbus_status_t modbus_buffer_getU8(modbus_t *ctx, uint8_t *out);


modbus_status_t modbus_buffer_getU16(modbus_t *ctx, uint16_t *out);


modbus_status_t modbus_buffer_checkCRC(modbus_t *ctx);


modbus_status_t modbus_buffer_send(modbus_t *ctx);


void modbus_buffer_startRead(modbus_t *ctx);


modbus_status_t modbus_buffer_flush(modbus_t *ctx);


void modbus_buffer_clear(modbus_t *ctx);


#endif /* MODBUS_BUFFER_H */
