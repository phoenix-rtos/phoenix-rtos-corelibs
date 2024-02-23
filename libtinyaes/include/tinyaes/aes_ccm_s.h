/*
 * Phoenix-RTOS
 *
 * Extended AES Counter with CBC-MAC, a.k.a. CCM* mode implementation
 *
 * Copyright 2020 Phoenix Systems
 * Author: Gerard Świderski
 *
 * Remarks: implementation based on RFC 3610, IEEE 802.15.4
 *
 * %LICENSE%
 */

#ifndef AES_CCM_S_H
#define AES_CCM_S_H

#include <stdint.h>

/*
 * NOTICE: to use this operation mode libtinyaes should be compiled with
 * `WITH_AES_CCM_S` defined. This makes libtinyaes compact in size.
 */

struct AES_ctx;

enum aes_ccm_s_dir_e {
	aes_ccm_s__encrypt,
	aes_ccm_s__decrypt,
};

/* setup key for crypt */
void AES_CCM_S_set_key(struct AES_ctx *ctx, const uint8_t *key);

/* authenticate with encryption or decryption (dir) */
void AES_CCM_S_crypt(struct AES_ctx *ctx,
	const uint8_t *nonce,
	const uint8_t *a, uint16_t a_len,
	uint8_t *m, uint16_t m_len,
	uint8_t *mic_ptr, uint8_t mic_len,
	enum aes_ccm_s_dir_e dir);

/* calculate/authenticate: message integrity code of length mic_len */
void AES_CCM_S_mic(struct AES_ctx *ctx,
	const uint8_t *nonce,
	const uint8_t *a, uint16_t a_len,
	const uint8_t *m, uint16_t m_len,
	uint8_t *mic_ptr, uint8_t mic_len,
	enum aes_ccm_s_dir_e priv);

/* encrypt message payload */
void AES_CCM_S_ctr(struct AES_ctx *ctx, const uint8_t *nonce, uint8_t *m, uint16_t m_len);

#endif /* end of AES_CCM_S_H */
