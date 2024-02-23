/*
 * Phoenix-RTOS
 *
 * AES128-GCM encryption, decryption and signing
 *
 * Copyright 2023 Phoenix Systems
 * Author: Jan Wiśniewski
 *
 * %LICENSE%
 */

#ifndef AES_GCM_H
#define AES_GCM_H

#include <stdint.h>
#include <stddef.h>

#include <tinyaes/aes.h>


/* AES context initiailization
 *
 * This calls AES_init_ctx so aes_ctx can be uninitialized here.
 *
 * @param[in]     key              16 byte (128 bit) aes key
 */
void AES_GCM_init(struct AES_ctx *aes_ctx, const uint8_t *key);


/* Calculate GCM MAC
 *
 * @param[in]  aad,aad_len      additional data (authenticated not encrypted)
 * @param[in]  ctext,ctext_len  ciphertext (authenticated and encrypted)
 * @param[out] tag_out          16 byte tag
 */
void AES_GCM_mac(struct AES_ctx *aes_ctx, const uint8_t *iv12, const uint8_t *aad, size_t aad_len, const uint8_t *ctext, size_t ctext_len, uint8_t *tag_out);


/* Encrypt / Decrypt data inplace
 *
 * Maximum xtext length supported by this implementations is
 * (2^32 - 2) * 16 bytes
 *
 * @param[in]     iv12             12 byte (96 bit) initialization vector
 * @param[in,out] xtext,xtext_len  signed ciphertext
 */
void AES_GCM_crypt(struct AES_ctx *aes_ctx, const uint8_t *iv12, uint8_t *xtext, size_t xtext_len);


#endif /* end of AES_GCM_H */
