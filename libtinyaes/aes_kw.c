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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "tinyaes/aes.h"
#include "tinyaes/aes_kw.h"

#define CEIL_DIV(X, DIV) (((X) + (DIV)-1) / (DIV))

#define SEMIBLOCK_LEN (AES_BLOCKLEN / 2)

static const uint8_t icv2[4] = "\xA6\x59\x59\xA6";

static void xor_semiblock(uint8_t sb[SEMIBLOCK_LEN], uint64_t u)
{
	size_t i;

	for (i = 0; i < SEMIBLOCK_LEN; i++) {
		sb[i] ^= (uint8_t)(u >> (sizeof(u) - i - 1) * 8);
	}
}

/* Algorithm 1: W(S)
Based on RFC 3394 index-based algorithm */
void AES_KW_raw_wrap(struct AES_ctx *aes, uint8_t *buf, size_t len)
{
	int i;
	int j;
	uint64_t t;
	/* Left semiblock = A, right semiblock = R */
	uint8_t AR[2 * SEMIBLOCK_LEN];
	uint8_t *A = &AR[0];
	uint8_t *R = &AR[SEMIBLOCK_LEN];
	int n = (len / SEMIBLOCK_LEN) - 1;

	memcpy(A, &buf[0], SEMIBLOCK_LEN);

	for (j = 0; j <= 5; j++) {
		for (i = 1; i <= n; i++) {
			memcpy(R, &buf[i * SEMIBLOCK_LEN], SEMIBLOCK_LEN);
			AES_ECB_encrypt(aes, AR);
			t = (n * j) + i;
			xor_semiblock(A, t);
			memcpy(&buf[i * SEMIBLOCK_LEN], R, SEMIBLOCK_LEN);
		}
	}

	memcpy(&buf[0], A, SEMIBLOCK_LEN);
}

/* Algorithm 2: W^-1(C)
Based on RFC 3394 index-based algorithm */
void AES_KW_raw_unwrap(struct AES_ctx *aes, uint8_t *buf, size_t len)
{
	int i;
	int j;
	uint64_t t;
	/* Left semiblock = A, right semiblock = R */
	uint8_t AR[2 * SEMIBLOCK_LEN];
	uint8_t *A = &AR[0];
	uint8_t *R = &AR[SEMIBLOCK_LEN];
	int n = (len / SEMIBLOCK_LEN) - 1;

	memcpy(A, &buf[0], SEMIBLOCK_LEN);

	for (j = 5; j >= 0; j--) {
		for (i = n; i >= 1; i--) {
			t = (n * j) + i;
			xor_semiblock(A, t);
			memcpy(R, &buf[i * SEMIBLOCK_LEN], SEMIBLOCK_LEN);
			AES_ECB_decrypt(aes, AR);
			memcpy(&buf[i * SEMIBLOCK_LEN], R, SEMIBLOCK_LEN);
		}
	}

	memcpy(&buf[0], A, SEMIBLOCK_LEN);
}

/* Algorithm 5: KWP-AE(P)  */
int AES_KWP_wrap(struct AES_ctx *aes, uint8_t *buf, size_t len)
{
	unsigned int i;
	uint32_t plen = len;
	int32_t padlen = SEMIBLOCK_LEN * CEIL_DIV(len, SEMIBLOCK_LEN) - len;

	memcpy(&buf[0], icv2, sizeof(icv2));

	for (i = 0; i < sizeof(plen); i++) {
		buf[sizeof(icv2) + sizeof(plen) - i - 1] = (uint8_t)plen;
		plen >>= 8;
	}

	memset(&buf[AES_KWP_HEADER_LEN + len], 0x0, padlen);

	if (len > 8) {
		AES_KW_raw_wrap(aes, buf, AES_KWP_HEADER_LEN + len + padlen);
	}
	else {
		AES_ECB_encrypt(aes, buf);
	}

	return AES_KWP_HEADER_LEN + len + padlen;
}

/* Algorithm 6: KWP-AD(C)  */
int AES_KWP_unwrap(struct AES_ctx *aes, uint8_t *buf, size_t len)
{
	unsigned int i;
	size_t semiblocks = len / SEMIBLOCK_LEN;
	uint32_t plen = 0;
	int32_t padlen;

	if (semiblocks > 2) {
		AES_KW_raw_unwrap(aes, buf, len);
	}
	else if (semiblocks == 2) {
		AES_ECB_decrypt(aes, buf);
	}
	else {
		return -1;
	}

	if (memcmp(buf, icv2, sizeof(icv2)) != 0) {
		return -1;
	}

	for (i = 0; i < sizeof(plen); i++) {
		plen <<= 8;
		plen |= buf[sizeof(icv2) + i];
	}

	padlen = 8 * (semiblocks - 1) - plen;

	if (padlen < 0 || padlen > 7) {
		return -1;
	}

	for (i = 0; i < (unsigned int)padlen; i++) {
		if (buf[AES_KWP_HEADER_LEN + plen + i] != 0x0) {
			return -1;
		}
	}

	return plen;
}
