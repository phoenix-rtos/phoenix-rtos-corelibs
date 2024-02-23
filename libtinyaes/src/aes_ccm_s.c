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

#ifdef WITH_AES_CCM_S

#include <stddef.h>
#include <string.h>

#include <aes.h>
#include <aes_ccm_s.h>

#define CCM_S_BLOCK_SIZE 16
#define CCM_S_FLAG_SIZE  1
#define CCM_S_L_SIZE     2
#define CCM_S_NONCE_SIZE (CCM_S_BLOCK_SIZE - CCM_S_L_SIZE - CCM_S_FLAG_SIZE)

/* flags as defined in RFC */
#define CCM_S_ENCR_FLAGS           0x01
#define CCM_S_AUTH_FLAGS(adata, m) (((adata) ? (1U << 6) : 0) | ((((m) -2U) / 2U) << 3) | (CCM_S_L_SIZE - 1U))

/* clang-format off */
#if defined(AES128) && (AES128 == 1) && defined(ECB) && (ECB == 1) && (CCM_S_BLOCK_SIZE == AES_KEYLEN)
#  define AES128_crypt(ctx, blk) AES_ECB_encrypt(ctx, blk)
#else
#  error "RFC 3610 defines CCM* only for AES-128"
#  define AES128_crypt(ctx, blk) ((void)0)
#endif
/* clang-format on */

/* set initial vector: counter mode */
static inline void prep_iv(uint8_t* blk, uint8_t flags, const uint8_t* nonce, uint16_t counter)
{
    /* flags field */
    blk[0] = flags;

    /* nonce field */
    memcpy(blk + CCM_S_FLAG_SIZE, nonce, CCM_S_NONCE_SIZE);

    /* size/counter field (BE16) */
    blk[CCM_S_BLOCK_SIZE - 2] = (uint8_t)(counter >> 8);
    blk[CCM_S_BLOCK_SIZE - 1] = (uint8_t) counter;
}

static inline void ctr_next_block(struct AES_ctx* ctx,
                                  const uint8_t* nonce,
                                  uint8_t* m, uint16_t m_len,
                                  uint16_t idx, uint16_t counter)
{
    uint8_t iv[CCM_S_BLOCK_SIZE], n = 0;

    /* set flags described in RFC 3610 */
    prep_iv(iv, CCM_S_ENCR_FLAGS, nonce, counter);
    AES128_crypt(ctx, iv);

    while ((idx + n < m_len) && (n < CCM_S_BLOCK_SIZE)) {
        m[idx + n] ^= iv[n];
        n++;
    }
}

/* encrypt message payload */
void AES_CCM_S_ctr(struct AES_ctx* ctx, const uint8_t* nonce, uint8_t* m, uint16_t m_len)
{
    uint8_t counter = 1;
    uint16_t idx = 0;

    while (idx < m_len) {
        ctr_next_block(ctx, nonce, m, m_len, idx, counter);
        idx += CCM_S_BLOCK_SIZE;
        counter++;
    }
}

/* calculate/authenticate: message integrity code of length mic_len */
void AES_CCM_S_mic(struct AES_ctx* ctx,
                   const uint8_t* nonce,
                   const uint8_t* a, uint16_t a_len,
                   const uint8_t* m, uint16_t m_len,
                   uint8_t* mic_ptr, uint8_t mic_len,
                   enum aes_ccm_s_dir_e priv)
{
    uint8_t n, block[CCM_S_BLOCK_SIZE];
    uint16_t idx;

    /* set flags described in RFC 3610 */
    prep_iv(block, CCM_S_AUTH_FLAGS(a_len, mic_len), nonce, m_len);
    AES128_crypt(ctx, block);

    /* adata */
    if (a_len) {
        /* 2 octests of adata length (BE16) */
        block[0] = block[0] ^ (uint8_t)(a_len >> 8);
        block[1] = block[1] ^ (uint8_t) a_len;

        /* 14 octets of data in the first auth block */
        for (n = 2; (n - 2 < a_len) && (n < CCM_S_BLOCK_SIZE); n++) {
            block[n] ^= a[n - 2];
        }

        AES128_crypt(ctx, block);

        idx = CCM_S_FLAG_SIZE + CCM_S_NONCE_SIZE;
        while (idx < a_len) {
            for (n = 0; (idx + n < a_len) && (n < CCM_S_BLOCK_SIZE); n++) {
                block[n] ^= a[idx + n];
            }
            idx += CCM_S_BLOCK_SIZE;
            AES128_crypt(ctx, block);
        }
    }

    /* message payload */
    if (m_len) {
        idx = 0;
        while (idx < m_len) {
            for (n = 0; (idx + n < m_len) && (n < CCM_S_BLOCK_SIZE); n++) {
                block[n] ^= m[idx + n];
            }
            idx += CCM_S_BLOCK_SIZE;
            AES128_crypt(ctx, block);
        }
    }

    if (priv == aes_ccm_s__encrypt) {
        /* encrypt cbc-mac -> mic, counter=0 */
        ctr_next_block(ctx, nonce, block, CCM_S_BLOCK_SIZE, 0, 0);
    }

    memcpy(mic_ptr, block, mic_len);
}

void AES_CCM_S_set_key(struct AES_ctx* ctx, const uint8_t* key)
{
    AES_init_ctx(ctx, key);
}

/* combo: authenticate with encryption or decryption (dir) */
void AES_CCM_S_crypt(struct AES_ctx* ctx,
                     const uint8_t* nonce,
                     const uint8_t* a, uint16_t a_len,
                     uint8_t* m, uint16_t m_len,
                     uint8_t* mic_ptr, uint8_t mic_len,
                     enum aes_ccm_s_dir_e dir)
{
    if (dir == aes_ccm_s__decrypt) {
        AES_CCM_S_ctr(ctx, nonce, m, m_len);
    }

    /* mic is an encrypted cbc-mac hash */
    AES_CCM_S_mic(ctx, nonce, a, a_len, m, m_len, mic_ptr, mic_len, aes_ccm_s__encrypt);

    if (dir == aes_ccm_s__encrypt) {
        AES_CCM_S_ctr(ctx, nonce, m, m_len);
    }
}

#endif /* end of WITH_AES_CCM_S */
