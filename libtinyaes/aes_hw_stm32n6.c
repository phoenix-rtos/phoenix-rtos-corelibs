/*
 * Phoenix-RTOS
 *
 * Non-thread safe implementation of libtinyaes API using hardware-assisted libcryp
 *
 * Copyright 2025 Phoenix Systems
 * Author: Krzysztof Radzewicz
 *
 * %LICENSE%
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "tinyaes/aes.h"

/* FIXME - unfortunate compiling order, corelibs depends on devices.
 * Just provide HW AES API directly for now */
// #include <libmulti/libcryp.h>
#include "aes_hw_stm32n6.h"

/**************************** WARNING ******************************/
/* This implementation uses a hardware peripheral to perform       */
/* cryptographic operations and is NOT thread safe. Access is not  */
/* synchronized due to performance (avoiding locking/msg overhead) */
/* and compatibility (retaining the same libtinyaes API) reasons.  */
/* If used in a multi-threaded environment, be sure to use         */
/* application-level locking or other means of synchronization.    */
/*******************************************************************/

#if defined(AES_KEYLEN) && AES_KEYLEN == 16
#define CRYP_KEYLEN aes_128
#elif defined(AES_KEYLEN) && AES_KEYLEN == 24
#define CRYP_KEYLEN aes_192
#else
#define CRYP_KEYLEN aes_256
#endif

static unsigned int is_initialized = 0;

static void AES_init(void)
{
	if (!is_initialized) {
		is_initialized = 1;
		libcryp_init();
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

#if defined(ECB) && (ECB == 1)

void AES_ECB_encrypt(struct AES_ctx *ctx, uint8_t *buf)
{
	libcryp_prepare(aes_ecb, aes_encrypt);
	libcryp_setKey(&ctx->RoundKey[0], CRYP_KEYLEN);
	libcryp_processBlock(buf, buf);
	libcryp_unprepare();
}

void AES_ECB_decrypt(struct AES_ctx *ctx, uint8_t *buf)
{
	libcryp_deriveDecryptionKey();
	libcryp_setKey(&ctx->RoundKey[0], CRYP_KEYLEN);
	libcryp_prepare(aes_ecb, aes_decrypt);
	libcryp_enable();
	libcryp_processBlock(buf, buf);
	libcryp_unprepare();
}

#endif  // #if defined(ECB) && (ECB == 1)


#if defined(CBC) && (CBC == 1)

void AES_CBC_encrypt_buffer(struct AES_ctx *ctx, uint8_t *buf, uint32_t length)
{
	libcryp_prepare(aes_cbc, aes_encrypt);
	libcryp_setIv(&ctx->Iv[0]);
	libcryp_setKey(&ctx->RoundKey[0], CRYP_KEYLEN);

	for (int i = 0; i < length; i += AES_BLOCKLEN) {
		libcryp_processBlock(buf, buf);
		buf += AES_BLOCKLEN;
	}
	libcryp_unprepare();
}

void AES_CBC_decrypt_buffer(struct AES_ctx *ctx, uint8_t *buf, uint32_t length)
{
	libcryp_deriveDecryptionKey();
	libcryp_setKey(&ctx->RoundKey[0], CRYP_KEYLEN);
	libcryp_prepare(aes_cbc, aes_decrypt);
	libcryp_setIv(&ctx->Iv[0]);
	libcryp_enable();

	for (int i = 0; i < length; i += AES_BLOCKLEN) {
		libcryp_processBlock(buf, buf);
		buf += AES_BLOCKLEN;
	}
	libcryp_unprepare();
}


#endif  // #if defined(CBC) && (CBC == 1)


#if defined(CTR) && (CTR == 1)

void AES_CTR_xcrypt_buffer(struct AES_ctx *ctx, uint8_t *buf, uint32_t length)
{
	/* RM mentions the counter should start at 0x1, but that is wrong. It should be 0. */
	uint8_t nonce[16] = {};
	memcpy(nonce, &ctx->Iv, 12);
	libcryp_prepare(aes_ctr, aes_encrypt);
	libcryp_setIv(nonce);
	libcryp_setKey(&ctx->RoundKey[0], CRYP_KEYLEN);

	for (int i = 0; i < length; i += AES_BLOCKLEN) {
		libcryp_processBlock(buf, buf);
		buf += AES_BLOCKLEN;
	}
	libcryp_unprepare();
}

#endif  // #if defined(CTR) && (CTR == 1)
