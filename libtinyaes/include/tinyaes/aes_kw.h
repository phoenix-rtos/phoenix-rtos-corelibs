/*
 * Phoenix-RTOS
 *
 * AES Key Wrapping (NIST SP 800-38F)
 *
 * Copyright 2021 Phoenix Systems
 * Author: Daniel Sawka
 *
 * %LICENSE%
 */

#ifndef AES_KW_H
#define AES_KW_H

#include <stdint.h>
#include <stddef.h>


struct AES_ctx;


#define AES_KWP_HEADER_LEN 8


/* Performs wrapping on a buffer.
Note: this is an in-place operation but the buffer must be accounted for
the KWP header, i.e. buf must have AES_KWP_HEADER_LEN bytes free before
the actual content to be wrapped. buf must be a multiple of AES block length
(16 bytes) to account for padding. */
int AES_KWP_wrap(struct AES_ctx *aes, uint8_t *buf, size_t len);


/* Performs unwrapping on a buffer.
Note: this is an in-place operation but KWP header is left as is.
Skip buf by AES_KWP_HEADER_LEN bytes to get the actual unwrapped content. */
int AES_KWP_unwrap(struct AES_ctx *aes, uint8_t *buf, size_t len);


void AES_KW_raw_wrap(struct AES_ctx *aes, uint8_t *buf, size_t len);


void AES_KW_raw_unwrap(struct AES_ctx *aes, uint8_t *buf, size_t len);


#endif
