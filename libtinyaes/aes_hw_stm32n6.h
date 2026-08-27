/*
 * Phoenix-RTOS
 *
 * STM32L4 AES driver API
 *
 * Copyright 2020 Phoenix Systems
 * Author: Daniel Sawka
 *
 * %LICENSE%
 */


#ifndef LIBCRYP_H_
#define LIBCRYP_H_


enum {
	aes_128 = 0,
	aes_256 = 1,
	aes_192 = 2
};


enum {
	aes_ecb = 0,
	aes_cbc = 1,
	aes_ctr = 2
};


enum {
	aes_encrypt = 0,
	aes_decrypt = 2
};


int libcryp_tmp(unsigned char *key, unsigned char *iv, const unsigned char *in, unsigned char *out);


void libcryp_enable(void);


void libcryp_disable(void);


int libcryp_setKey(const unsigned char *key, int keylen);


void libcryp_getKey(unsigned char *key, int keylen);


void libcryp_setIv(const unsigned char *iv);


void libcryp_getIv(unsigned char *iv);


void libcryp_deriveDecryptionKey(void);


void libcryp_prepare(int mode, int dir);


void libcryp_unprepare(void);


void libcryp_processBlock(const unsigned char *in, unsigned char *out);


int libcryp_init(void);


#endif
