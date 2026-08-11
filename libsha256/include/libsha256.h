/*
 * Phoenix-RTOS
 *
 * libsha256 - SHA-256 (FIPS 180-4)
 *
 * Streaming API, so a caller hashing a whole partition never has to hold it in
 * memory. The implementation is extracted from libtomcrypt, see sha256.c.
 *
 * Copyright 2026 Phoenix Systems
 * Author: Marek Bialowas
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _LIBSHA256_H_
#define _LIBSHA256_H_

#include <stddef.h>
#include <stdint.h>


#define SHA256_DIGEST_SIZE 32
#define SHA256_BLOCK_SIZE  64


/* upstream's generic hash state, reduced to the one algorithm extracted here */
typedef union {
	struct sha256_state {
		uint64_t length;
		uint32_t state[8], curlen;
		unsigned char buf[SHA256_BLOCK_SIZE];
	} sha256;
} sha256_ctx_t;


/* Initialize the hash state, returns 0 on success */
int sha256_init(sha256_ctx_t *md);


/* Process a block of memory through the hash, returns 0 on success */
int sha256_process(sha256_ctx_t *md, const unsigned char *in, unsigned long inlen);


/* Terminate the hash, writing SHA256_DIGEST_SIZE bytes to out, returns 0 on success */
int sha256_done(sha256_ctx_t *md, unsigned char *out);


/* Compare two blocks of memory for inequality in constant time.
 *
 * Only tells equal from not equal - by how much the first differing byte
 * differs is not observable, and neither is where it is. Use it wherever
 * inequality means a wrong key or a forged digest, as a plain memcmp() leaks
 * the position of the first mismatch through its execution time.
 *
 * Returns 0 when a and b are equal over len bytes and non-zero when they are
 * not: 1 for a plain difference, a different non-zero value when a or b is NULL.
 * Test the result against 0 and never against 1 - a caller writing
 * "if (mem_neq(...) == 1) reject;" would accept a NULL digest instead of
 * rejecting it.
 */
int mem_neq(const void *a, const void *b, size_t len);


#endif /* _LIBSHA256_H_ */
