/*
 * Phoenix-RTOS
 *
 * Non-thread safe implementation of libtinyaes API using hardware-assisted libaes
 *
 * Copyright 2020, 2022 Phoenix Systems
 * Author: Daniel Sawka
 *
 * %LICENSE%
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "tinyaes/aes.h"

/* FIXME - unfortunate compiling order, corelibs depends on devices.
 * Just provide HW AES API directly for now */
// #include <libmulti/libaes.h>
#include "aes_hw_stm32l4.h"

/**************************** WARNING ******************************/
/* This implementation uses a hardware peripheral to perform       */
/* cryptographic operations and is NOT thread safe. Access is not  */
/* synchronized due to performance (avoiding locking/msg overhead) */
/* and compatibility (retaining the same libtinyaes API) reasons.  */
/* If used in a multi-threaded environment, be sure to use         */
/* application-level locking or other means of synchronization.    */
/*******************************************************************/


#if AES_KEYLEN == 24
#error "AES key length of 192 bits is not supported"
#endif


static unsigned int is_initialized = 0;

static void AES_init(void)
{
	if (!is_initialized) {
		is_initialized = 1;
		libaes_init();
	}
}

void AES_init_ctx(struct AES_ctx *ctx, const uint8_t *key)
{
	AES_init();
	memcpy(&ctx->RoundKey[0], key, AES_KEYLEN);
	ctx->RoundKey[AES_KEYLEN] = 0x0;
}

#if (defined(CBC) && (CBC == 1)) || (defined(CTR) && (CTR == 1))

void AES_init_ctx_iv(struct AES_ctx *ctx, const uint8_t *key, const uint8_t *iv)
{
	AES_init();
	memcpy(&ctx->RoundKey[0], key, AES_KEYLEN);
	ctx->RoundKey[AES_KEYLEN] = 0x0;
	memcpy(ctx->Iv, iv, AES_BLOCKLEN);
}

void AES_ctx_set_iv(struct AES_ctx *ctx, const uint8_t *iv)
{
	AES_init();
	memcpy(ctx->Iv, iv, AES_BLOCKLEN);
}

#endif

static void process_buffer(struct AES_ctx *ctx, int mode, int dir, uint8_t *buf, uint32_t length)
{
	uintptr_t i;
	int keylen = (AES_KEYLEN == 16) ? aes_128 : aes_256;

	if (dir == aes_decrypt && mode != aes_ctr) {
		if (ctx->RoundKey[AES_KEYLEN] == 0x0) {
			libaes_setKey(&ctx->RoundKey[0], keylen);
			libaes_deriveDecryptionKey();
			libaes_getKey(&ctx->RoundKey[AES_KEYLEN + 1], keylen);
			ctx->RoundKey[AES_KEYLEN] = 0x1;
		}
		else {
			libaes_setKey(&ctx->RoundKey[AES_KEYLEN + 1], keylen);
		}
	}
	else {
		libaes_setKey(&ctx->RoundKey[0], keylen);
	}

	libaes_prepare(mode, dir);

	for (i = 0; i < length; i += AES_BLOCKLEN) {
		libaes_processBlock(buf, buf);
		buf += AES_BLOCKLEN;
	}

	libaes_unprepare();
}

#if defined(ECB) && (ECB == 1)

void AES_ECB_encrypt(struct AES_ctx *ctx, uint8_t *buf)
{
	process_buffer(ctx, aes_ecb, aes_encrypt, buf, AES_BLOCKLEN);
}

void AES_ECB_decrypt(struct AES_ctx *ctx, uint8_t *buf)
{
	process_buffer(ctx, aes_ecb, aes_decrypt, buf, AES_BLOCKLEN);
}

#endif  // #if defined(ECB) && (ECB == 1)


#if defined(CBC) && (CBC == 1)

void AES_CBC_encrypt_buffer(struct AES_ctx *ctx, uint8_t *buf, uint32_t length)
{
	libaes_setIv(ctx->Iv);
	process_buffer(ctx, aes_cbc, aes_encrypt, buf, length);
	libaes_getIv(ctx->Iv);
}

void AES_CBC_decrypt_buffer(struct AES_ctx *ctx, uint8_t *buf, uint32_t length)
{
	libaes_setIv(ctx->Iv);
	process_buffer(ctx, aes_cbc, aes_decrypt, buf, length);
	libaes_getIv(ctx->Iv);
}

#endif  // #if defined(CBC) && (CBC == 1)


#if defined(CTR) && (CTR == 1)

void AES_CTR_xcrypt_buffer(struct AES_ctx *ctx, uint8_t *buf, uint32_t length)
{
	libaes_setIv(ctx->Iv);
	process_buffer(ctx, aes_ctr, aes_encrypt, buf, length);
	libaes_getIv(ctx->Iv);
}

#endif  // #if defined(CTR) && (CTR == 1)
