/*
 * Phoenix-RTOS
 *
 * AES Cipher-based Message Authentication Code (AES-CMAC)
 *
 * Copyright 2020 Phoenix Systems
 * Author: Daniel Sawka
 *
 * %LICENSE%
 */

#ifndef CMAC_H
#define CMAC_H

#include <stddef.h>

#include <tinyaes/aes.h>


struct CMAC_ctx {
	struct AES_ctx aes;
	uint8_t mac[AES_BLOCKLEN];
	uint8_t buf[AES_BLOCKLEN];
	size_t outstanding_len;
};


void CMAC_init_ctx(struct CMAC_ctx *ctx, const uint8_t key[AES_KEYLEN]);


/* stage counter used in CMAC/OMAC1 */
void CMAC_stage(struct CMAC_ctx *ctx, uint8_t ctr);


void CMAC_append(struct CMAC_ctx *ctx, const uint8_t data[], size_t len);


void CMAC_calculate(struct CMAC_ctx *ctx, uint8_t mac[AES_KEYLEN]);


/* Internal */


void CMAC_generate_subkey_k1(struct AES_ctx *ctx, uint8_t k1[AES_KEYLEN]);


void CMAC_generate_subkey_k1_k2(struct AES_ctx *ctx, uint8_t k1[AES_KEYLEN], uint8_t k2[AES_KEYLEN]);

#endif /* CMAC_H */
