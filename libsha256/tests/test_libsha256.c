/*
 * Phoenix-RTOS
 *
 * libsha256 unit tests - public API
 *
 * Covers the FIPS 180-4 examples, the message lengths either side of a padding
 * block (where a SHA-256 implementation is most likely to be wrong), the
 * equivalence of a chunked feed with a one-shot one, and the argument checks.
 *
 * Copyright 2026 Phoenix Systems
 * Author: Marek Bialowas
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include <libsha256.h>
#include <unity_fixture.h>


/* the digest of "a" repeated len times, len picked around a 64-byte block */
static const struct {
	unsigned int len;
	const char *digest;
} boundaries[] = {
	{ 55, "\x9f\x43\x90\xf8\xd3\x0c\x2d\xd9\x2e\xc9\xf0\x95\xb6\x5e\x2b\x9a\xe9\xb0\xa9\x25\xa5\x25\x8e\x24\x1c\x9f\x1e\x91\x0f\x73\x43\x18" },
	{ 56, "\xb3\x54\x39\xa4\xac\x6f\x09\x48\xb6\xd6\xf9\xe3\xc6\xaf\x0f\x5f\x59\x0c\xe2\x0f\x1b\xde\x70\x90\xef\x79\x70\x68\x6e\xc6\x73\x8a" },
	{ 57, "\xf1\x3b\x2d\x72\x46\x59\xeb\x3b\xf4\x7f\x2d\xd6\xaf\x1a\xcc\xc8\x7b\x81\xf0\x9f\x59\xf2\xb7\x5e\x5c\x0b\xed\x65\x89\xdf\xe8\xc6" },
	{ 63, "\x7d\x3e\x74\xa0\x5d\x7d\xb1\x5b\xce\x4a\xd9\xec\x06\x58\xea\x98\xe3\xf0\x6e\xee\xcf\x16\xb4\xc6\xff\xf2\xda\x45\x7d\xdc\x2f\x34" },
	{ 64, "\xff\xe0\x54\xfe\x7a\xe0\xcb\x6d\xc6\x5c\x3a\xf9\xb6\x1d\x52\x09\xf4\x39\x85\x1d\xb4\x3d\x0b\xa5\x99\x73\x37\xdf\x15\x46\x68\xeb" },
	{ 65, "\x63\x53\x61\xc4\x8b\xb9\xea\xb1\x41\x98\xe7\x6e\xa8\xab\x7f\x1a\x41\x68\x5d\x6a\xd6\x2a\xa9\x14\x6d\x30\x1d\x4f\x17\xeb\x0a\xe0" },
	{ 119, "\x31\xeb\xa5\x1c\x31\x3a\x5c\x08\x22\x6a\xdf\x18\xd4\xa3\x59\xcf\xdf\xd8\xd2\xe8\x16\xb1\x3f\x4a\xf9\x52\xf7\xea\x65\x84\xdc\xfb" },
	{ 120, "\x2f\x3d\x33\x54\x32\xc7\x0b\x58\x0a\xf0\xe8\xe1\xb3\x67\x4a\x7c\x02\x0d\x68\x3a\xa5\xf7\x3a\xaa\xed\xfd\xc5\x5a\xf9\x04\xc2\x1c" },
	{ 121, "\xe9\x61\x53\x20\x12\x8c\xc7\xa3\xd6\x07\x8e\x9a\xf0\x56\x03\x18\x8e\x5c\xcb\xf0\xd0\x7d\x8b\x73\x5d\x3d\xf5\xe8\xe0\xc1\x28\x1f" },
};

/* SHA-256 of ((i * 7 + 3) & 0xff for i in range(4096)) - see fillPattern() */
static const char patternDigest[] =
		"\x74\x86\xda\x8f\x1e\x13\x94\x3f\xae\x21\xa0\xb0\x43\xf1\xe9\x96"
		"\x40\xd7\xd8\xeb\xaf\xb2\x52\x66\x47\x8b\x5c\xdd\xae\x12\x72\xb5";


/* hash data in chunks of at most "chunk" bytes */
static void hashChunked(const void *data, size_t len, size_t chunk, unsigned char *digest)
{
	sha256_ctx_t ctx;
	const unsigned char *p = data;
	size_t n;

	TEST_ASSERT_EQUAL_INT(0, sha256_init(&ctx));

	while (len > 0) {
		n = (len < chunk) ? len : chunk;
		TEST_ASSERT_EQUAL_INT(0, sha256_process(&ctx, p, n));
		p += n;
		len -= n;
	}

	TEST_ASSERT_EQUAL_INT(0, sha256_done(&ctx, digest));
}


static void fillPattern(unsigned char *buf, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		buf[i] = (unsigned char)(i * 7 + 3);
	}
}


TEST_GROUP(libsha256);


TEST_SETUP(libsha256)
{
}


TEST_TEAR_DOWN(libsha256)
{
}


TEST(libsha256, fips_vectors)
{
	static const char *msg[] = {
		"",
		"abc",
		"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
	};
	static const char *expected[] = {
		"\xe3\xb0\xc4\x42\x98\xfc\x1c\x14\x9a\xfb\xf4\xc8\x99\x6f\xb9\x24\x27\xae\x41\xe4\x64\x9b\x93\x4c\xa4\x95\x99\x1b\x78\x52\xb8\x55",
		"\xba\x78\x16\xbf\x8f\x01\xcf\xea\x41\x41\x40\xde\x5d\xae\x22\x23\xb0\x03\x61\xa3\x96\x17\x7a\x9c\xb4\x10\xff\x61\xf2\x00\x15\xad",
		"\x24\x8d\x6a\x61\xd2\x06\x38\xb8\xe5\xc0\x26\x93\x0c\x3e\x60\x39\xa3\x3c\xe4\x59\x64\xff\x21\x67\xf6\xec\xed\xd4\x19\xdb\x06\xc1",
	};
	unsigned char digest[SHA256_DIGEST_SIZE];
	size_t i;

	for (i = 0; i < sizeof(msg) / sizeof(*msg); i++) {
		hashChunked(msg[i], strlen(msg[i]), SHA256_BLOCK_SIZE, digest);
		TEST_ASSERT_EQUAL_HEX8_ARRAY(expected[i], digest, SHA256_DIGEST_SIZE);
	}
}


TEST(libsha256, padding_boundaries)
{
	unsigned char buf[128], digest[SHA256_DIGEST_SIZE];
	size_t i;

	memset(buf, 'a', sizeof(buf));

	for (i = 0; i < sizeof(boundaries) / sizeof(*boundaries); i++) {
		hashChunked(buf, boundaries[i].len, SHA256_BLOCK_SIZE, digest);
		TEST_ASSERT_EQUAL_HEX8_ARRAY(boundaries[i].digest, digest, SHA256_DIGEST_SIZE);
	}
}


TEST(libsha256, chunked_equals_oneshot)
{
	/* chunk sizes below, at and above the block size, and unaligned ones */
	static const size_t chunks[] = { 1, 3, 63, 64, 65, 100, 4096 };
	/* static: 4 kB overflows the default thread stack of the NOMMU targets */
	static unsigned char buf[4096];
	unsigned char digest[SHA256_DIGEST_SIZE];
	size_t i;

	fillPattern(buf, sizeof(buf));

	for (i = 0; i < sizeof(chunks) / sizeof(*chunks); i++) {
		hashChunked(buf, sizeof(buf), chunks[i], digest);
		TEST_ASSERT_EQUAL_HEX8_ARRAY(patternDigest, digest, SHA256_DIGEST_SIZE);
	}
}


TEST(libsha256, rejects_invalid_args)
{
	sha256_ctx_t ctx;
	unsigned char digest[SHA256_DIGEST_SIZE];

	TEST_ASSERT_NOT_EQUAL_INT(0, sha256_init(NULL));

	TEST_ASSERT_EQUAL_INT(0, sha256_init(&ctx));
	TEST_ASSERT_NOT_EQUAL_INT(0, sha256_process(NULL, (const unsigned char *)"x", 1));
	TEST_ASSERT_NOT_EQUAL_INT(0, sha256_process(&ctx, NULL, 1));
	TEST_ASSERT_NOT_EQUAL_INT(0, sha256_done(&ctx, NULL));
	TEST_ASSERT_NOT_EQUAL_INT(0, sha256_done(NULL, digest));
}


TEST(libsha256, mem_neq_detects_any_difference)
{
	unsigned char a[SHA256_DIGEST_SIZE], b[SHA256_DIGEST_SIZE];
	size_t i;

	fillPattern(a, sizeof(a));
	memcpy(b, a, sizeof(b));

	TEST_ASSERT_EQUAL_INT(0, mem_neq(a, b, sizeof(a)));
	TEST_ASSERT_EQUAL_INT(0, mem_neq(a, b, 0));

	/* a single flipped bit has to be caught wherever it is */
	for (i = 0; i < sizeof(b); i++) {
		b[i] ^= 0x01;
		TEST_ASSERT_EQUAL_INT(1, mem_neq(a, b, sizeof(a)));
		b[i] ^= 0x01;
	}

	/* ...and not reported when it is past the compared range */
	b[sizeof(b) - 1] ^= 0x80;
	TEST_ASSERT_EQUAL_INT(0, mem_neq(a, b, sizeof(b) - 1));
}


TEST_GROUP_RUNNER(libsha256)
{
	RUN_TEST_CASE(libsha256, fips_vectors);
	RUN_TEST_CASE(libsha256, padding_boundaries);
	RUN_TEST_CASE(libsha256, chunked_equals_oneshot);
	RUN_TEST_CASE(libsha256, rejects_invalid_args);
	RUN_TEST_CASE(libsha256, mem_neq_detects_any_difference);
}


static void runner(void)
{
	RUN_TEST_GROUP(libsha256);
}


int main(int argc, char *argv[])
{
	return (UnityMain(argc, (const char **)argv, runner) == 0) ? 0 : 1;
}
