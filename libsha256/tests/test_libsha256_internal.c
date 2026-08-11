/*
 * Phoenix-RTOS
 *
 * libsha256 unit tests - internals
 *
 * The compression function is static, so there is nothing to link against: the
 * implementation is #included instead of being linked, which puts the whole
 * translation unit - static functions and file-scope data included - in reach
 * of the test. Everything else works as in any other unit test.
 *
 * Copyright 2026 Phoenix Systems
 * Author: Marek Bialowas
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include <unity_fixture.h>

#include "../sha256.c"


/* SHA-256 of "abc" and of the empty message (FIPS 180-4) */
static const unsigned char abcDigest[SHA256_DIGEST_SIZE] = {
	0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
	0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
};

static const unsigned char emptyDigest[SHA256_DIGEST_SIZE] = {
	0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
	0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
};


/* the digest is the chaining state serialized big-endian */
static void stateToDigest(const sha256_ctx_t *ctx, unsigned char *digest)
{
	int i;

	for (i = 0; i < 8; i++) {
		STORE32H(ctx->sha256.state[i], digest + 4 * i);
	}
}


/* pad msg into a single 64-byte block the way sha256_done() would */
static void padBlock(const char *msg, unsigned char *block)
{
	size_t len = strlen(msg);

	TEST_ASSERT_LESS_OR_EQUAL_size_t(SHA256_BLOCK_SIZE - 9, len);

	memset(block, 0, SHA256_BLOCK_SIZE);
	memcpy(block, msg, len);
	block[len] = 0x80;
	STORE64H((uint64_t)len * 8, block + SHA256_BLOCK_SIZE - 8);
}


TEST_GROUP(libsha256_internal);


TEST_SETUP(libsha256_internal)
{
}


TEST_TEAR_DOWN(libsha256_internal)
{
}


/* one compression of a hand-padded block has to produce the whole digest */
TEST(libsha256_internal, compress_single_block)
{
	unsigned char block[SHA256_BLOCK_SIZE], digest[SHA256_DIGEST_SIZE];
	sha256_ctx_t ctx;

	padBlock("abc", block);

	TEST_ASSERT_EQUAL_INT(0, sha256_init(&ctx));
	TEST_ASSERT_EQUAL_INT(0, s_sha256_compress(&ctx, block));
	stateToDigest(&ctx, digest);

	TEST_ASSERT_EQUAL_HEX8_ARRAY(abcDigest, digest, sizeof(digest));
}


/* the empty message is pure padding - only the length field distinguishes it */
TEST(libsha256_internal, compress_padding_only_block)
{
	unsigned char block[SHA256_BLOCK_SIZE], digest[SHA256_DIGEST_SIZE];
	sha256_ctx_t ctx;

	padBlock("", block);

	TEST_ASSERT_EQUAL_INT(0, sha256_init(&ctx));
	TEST_ASSERT_EQUAL_INT(0, s_sha256_compress(&ctx, block));
	stateToDigest(&ctx, digest);

	TEST_ASSERT_EQUAL_HEX8_ARRAY(emptyDigest, digest, sizeof(digest));
}


/* the initial chaining value is fixed by the standard */
TEST(libsha256_internal, init_sets_the_fips_iv)
{
	static const uint32_t iv[8] = {
		0x6a09e667UL, 0xbb67ae85UL, 0x3c6ef372UL, 0xa54ff53aUL,
		0x510e527fUL, 0x9b05688cUL, 0x1f83d9abUL, 0x5be0cd19UL
	};
	sha256_ctx_t ctx;

	TEST_ASSERT_EQUAL_INT(0, sha256_init(&ctx));

	TEST_ASSERT_EQUAL_HEX32_ARRAY(iv, ctx.sha256.state, 8);
	TEST_ASSERT_EQUAL_UINT32(0, ctx.sha256.curlen);
	TEST_ASSERT_EQUAL_UINT64(0, ctx.sha256.length);
}


TEST_GROUP_RUNNER(libsha256_internal)
{
	RUN_TEST_CASE(libsha256_internal, compress_single_block);
	RUN_TEST_CASE(libsha256_internal, compress_padding_only_block);
	RUN_TEST_CASE(libsha256_internal, init_sets_the_fips_iv);
}


static void runner(void)
{
	RUN_TEST_GROUP(libsha256_internal);
}


int main(int argc, char *argv[])
{
	return (UnityMain(argc, (const char **)argv, runner) == 0) ? 0 : 1;
}
