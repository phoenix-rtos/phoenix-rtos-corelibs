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

#ifdef WITH_AES_CMAC

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <aes.h>
#include <cmac.h>

#define CEIL_DIV(X, DIV) (((X) + (DIV) -1) / (DIV))

static void shift_left(uint8_t dst[], const uint8_t src[], size_t len)
{
    int i;
    unsigned int carry = 0;

    for (i = len - 1; i >= 0; i--) {
        dst[i] = (src[i] << 1) | carry;
        carry = !!(src[i] & 0x80);
    }
}

static void xor_symbol(uint8_t buf[], uint8_t symbol, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        buf[i] ^= symbol;
    }
}

static void xor_block(uint8_t buf1[], const uint8_t buf2[], size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        buf1[i] ^= buf2[i];
    }
}

static void pad(uint8_t dst[AES_BLOCKLEN], const uint8_t src[], size_t len)
{
    memcpy(dst, src, len);

    if (AES_BLOCKLEN - len > 0) {
        dst[len] = 0x80;
        len += 1;
    }

    memset(&dst[len], 0x0, AES_BLOCKLEN - len);
}

void CMAC_generate_subkey_k1(struct AES_ctx* ctx, uint8_t k1[AES_BLOCKLEN])
{
    uint8_t l[AES_BLOCKLEN] = { 0 };

    AES_ECB_encrypt(ctx, l);
    shift_left(k1, l, AES_BLOCKLEN);

    if ((l[0] & 0x80) != 0) {
        xor_symbol(k1, 0x00, AES_BLOCKLEN - 1);
        xor_symbol(&k1[AES_BLOCKLEN - 1], 0x87, 1);
    }
}

void CMAC_generate_subkey_k1_k2(struct AES_ctx* ctx, uint8_t k1[AES_BLOCKLEN], uint8_t k2[AES_BLOCKLEN])
{
    CMAC_generate_subkey_k1(ctx, k1);

    shift_left(k2, k1, AES_BLOCKLEN);

    if ((k1[0] & 0x80) != 0) {
        xor_symbol(k2, 0x00, AES_BLOCKLEN - 1);
        xor_symbol(&k2[AES_BLOCKLEN - 1], 0x87, 1);
    }
}

void CMAC_init_ctx(struct CMAC_ctx* ctx, const uint8_t key[AES_KEYLEN])
{
    AES_init_ctx(&ctx->aes, key);
    memset(ctx->mac, 0x0, AES_BLOCKLEN);
    ctx->outstanding_len = 0;
}

/* stage counter used in CMAC/OMAC1 */
void CMAC_stage(struct CMAC_ctx* ctx, uint8_t ctr)
{
    memset(ctx->mac, 0, AES_BLOCKLEN);
    ctx->outstanding_len = 0;
    ctx->mac[AES_BLOCKLEN - 1] = ctr;
    AES_ECB_encrypt(&ctx->aes, ctx->mac);
}

void CMAC_append(struct CMAC_ctx* ctx, const uint8_t data[], size_t len)
{
    if (len == 0) {
        return;
    }

    if (ctx->outstanding_len < AES_BLOCKLEN) {
        size_t to_append = AES_BLOCKLEN - ctx->outstanding_len;
        to_append = (to_append > len) ? len : to_append;

        memcpy(&ctx->buf[ctx->outstanding_len], data, to_append);
        ctx->outstanding_len += to_append;
        data += to_append;
        len -= to_append;

        if (len == 0) {
            return;
        }
    }

    xor_block(ctx->mac, ctx->buf, AES_BLOCKLEN);
    AES_ECB_encrypt(&ctx->aes, ctx->mac);

    while (CEIL_DIV(len, AES_BLOCKLEN) > 1) {
        xor_block(ctx->mac, data, AES_BLOCKLEN);
        AES_ECB_encrypt(&ctx->aes, ctx->mac);
        data += AES_BLOCKLEN;
        len -= AES_BLOCKLEN;
    }

    memcpy(ctx->buf, data, len);
    ctx->outstanding_len = len;
}

void CMAC_calculate(struct CMAC_ctx* ctx, uint8_t mac[AES_BLOCKLEN])
{
    uint8_t k1[AES_BLOCKLEN];
    uint8_t k2[AES_BLOCKLEN];
    uint8_t* last;
    uint8_t* padded;

    if (ctx->outstanding_len == AES_BLOCKLEN) {
        CMAC_generate_subkey_k1(&ctx->aes, k1);
        xor_block(k1, ctx->buf, AES_BLOCKLEN);
        last = k1;
    } else {
        CMAC_generate_subkey_k1_k2(&ctx->aes, k1, k2);
        padded = k1;
        pad(padded, ctx->buf, ctx->outstanding_len);
        xor_block(k2, padded, AES_BLOCKLEN);
        last = k2;
    }

    xor_block(ctx->mac, last, AES_BLOCKLEN);
    AES_ECB_encrypt(&ctx->aes, ctx->mac);
    memcpy(mac, ctx->mac, AES_BLOCKLEN);
}

#endif
