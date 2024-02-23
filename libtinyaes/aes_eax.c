/*
 * Phoenix-RTOS
 *
 * AES-EAX mode implementation
 *
 * Copyright 2020-2021 Phoenix Systems
 * Author: Gerard Świderski
 *
 * %LICENSE%
 */

#include <stddef.h>
#include <string.h>

#include "tinyaes/aes.h"
#include "tinyaes/cmac.h"
#include "tinyaes/aes_eax.h"

/* clang-format off */
#if !(defined(AES128) && (AES128 == 1) && defined(ECB) && (ECB == 1))
#  error "AES-EAX is supported only with 128 bit key & block size"
#endif
/* clang-format on */

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void AES_ctr_crypt(
	struct AES_ctx *ctx,
	uint8_t nonce_mac[AES_BLOCKLEN],
	uint8_t *data,
	uint32_t data_len)
{
	size_t n, len, left;
	uint8_t counter[AES_BLOCKLEN], buf[AES_BLOCKLEN];

	memcpy(counter, nonce_mac, AES_BLOCKLEN);

	for (left = data_len; left > 0; left -= len) {
		memcpy(buf, counter, AES_BLOCKLEN);
		AES_ECB_encrypt(ctx, buf);

		len = MIN(left, AES_BLOCKLEN);

		for (n = 0; n < len; n++) {
			*data++ ^= buf[n];
		}

		for (n = AES_BLOCKLEN; n-- > 0;) {
			if (++counter[n])
				break;
		}
	}
}

/* OMAC1, CMAC with counter (stage mark) */
static inline void CMAC_OMAC1_calculate(
	struct CMAC_ctx *ctx,
	uint8_t ctr,
	const uint8_t *data, size_t data_len,
	uint8_t mac[AES_BLOCKLEN])
{
	CMAC_stage(ctx, ctr);
	CMAC_append(ctx, data, data_len);
	CMAC_calculate(ctx, mac);
}

/* AES-EAX encryption or decryption (dir) */
int AES_EAX_crypt(
	const uint8_t key[AES_KEYLEN],
	const uint8_t *nonce, uint16_t nonce_len,
	const uint8_t *hdr, uint16_t hdr_len,
	uint8_t *m, uint16_t m_len,
	uint8_t tag_ptr[AES_BLOCKLEN],
	enum aes_eax_dir_e dir)
{
	unsigned n;
	uint8_t nonce_mac[AES_BLOCKLEN], hdr_mac[AES_BLOCKLEN], m_mac[AES_BLOCKLEN];

	struct CMAC_ctx cmac;
	CMAC_init_ctx(&cmac, key);

	CMAC_OMAC1_calculate(&cmac, 0, nonce, nonce_len, nonce_mac); /* Stage 0: nonce */
	CMAC_OMAC1_calculate(&cmac, 1, hdr, hdr_len, hdr_mac);       /* Stage 1: header */

	if (dir == aes_eax__decrypt) {
		CMAC_OMAC1_calculate(&cmac, 2, m, m_len, m_mac); /* Stage 2: message */

		/* authenticate */
		for (n = 0; n < AES_BLOCKLEN; n++) {
			if (tag_ptr[n] != (nonce_mac[n] ^ m_mac[n] ^ hdr_mac[n])) {
				return -1; /* failed */
			}
		}
	}

	AES_ctr_crypt(&cmac.aes, nonce_mac, m, m_len);

	if (dir == aes_eax__encrypt) {
		CMAC_OMAC1_calculate(&cmac, 2, m, m_len, m_mac); /* Stage 2: message */

		/* put tag */
		for (n = 0; n < AES_BLOCKLEN; n++) {
			tag_ptr[n] = nonce_mac[n] ^ m_mac[n] ^ hdr_mac[n];
		}
	}

	return 0;
}
