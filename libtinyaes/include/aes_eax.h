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

#ifndef AES_EAX_H
#define AES_EAX_H

#include <stdint.h>
#include <aes.h>

/*
 * NOTICE: to use this operation mode libtinyaes should be compiled with
 * `WITH_AES_EAX` and `WITH_AES_CMAC` both defined.
 */

enum aes_eax_dir_e {
    aes_eax__encrypt,
    aes_eax__decrypt,
};

/* AES-EAX encryption or decryption (dir) */
int AES_EAX_crypt(
    const uint8_t key[AES_KEYLEN],
    const uint8_t* nonce, uint16_t nonce_len,
    const uint8_t* hdr, uint16_t hdr_len,
    uint8_t* m, uint16_t m_len,
    uint8_t tag_ptr[AES_BLOCKLEN],
    enum aes_eax_dir_e dir);

#endif /* end of AES_EAX_H */
