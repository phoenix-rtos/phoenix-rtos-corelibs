/*
 * Phoenix-RTOS
 *
 * Universally Unique identifiers library
 *
 * Copyright 2022 Phoenix Systems
 * Author: Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "include/uuid/uuid.h"


static int init = 1;


/* Converts 4-bit unsigned char to hex ASCII code */
static inline char nibble2hex(const unsigned char nibble)
{
	return (nibble > 9) ? (nibble & 0xf) - 10 + 'a' : nibble + '0';
}


/* Converts hex ASCII code to 4-bit unsigned char */
static inline int hex2nibble(unsigned char *nibble, const char hex)
{
	char c = hex;

	if ((c >= '0') && (c <= '9')) {
		*nibble = c - '0';
		return 0;
	}
	else {
		c |= 0x20;
		if ((c >= 'a') && (c <= 'f')) {
			*nibble = c - 'a' + 10;
			return 0;
		}
	}

	*nibble = 0;
	return -1;
}


static void uuid_useRand(uuid_t out)
{
	pid_t pid;
	struct timespec ts;
	int i, n;

	/* on some targets clock interval is 10ms, the sleep is to prevent duplication */
	usleep(10 * 1000);
	clock_gettime(CLOCK_MONOTONIC, &ts);
	pid = getpid();

	if (init == 1) {
		srand((unsigned int)((ts.tv_sec & 0x0FFF) | (pid << 24)));
		init = 0;
	}

	/* to prevent same numbers when using the same seed */
	n = (int)(ts.tv_nsec / (1000 * 1000));
	for (i = 0; i < n; i++) {
		rand();
	}

	for (i = 0; i < (int)sizeof(uuid_t); i++) {
		out[i] = (unsigned char)(rand() >> (rand() % ((sizeof(int) - 1) * 8)));
	}
}


void uuid_unparse(const uuid_t uu, char *out)
{
	size_t i;
	char *temp = out;

	for (i = 0; i < sizeof(uuid_t); i++) {
		if ((i == 4) || (i == 6) || (i == 8) || (i == 10)) {
			*temp++ = '-';
		}

		*(temp++) = nibble2hex(uu[i] >> 4);
		*(temp++) = nibble2hex(uu[i] & 0xf);
	}

	*temp = '\0';
}


int uuid_parse(const char *in, uuid_t uu)
{
	int ret;
	unsigned char val;
	const char *temp;
	size_t i;

	if (strlen(in) != 36) {
		return -1;
	}

	temp = in;

	for (i = 0; i < sizeof(uuid_t); i++) {
		if ((i == 4) || (i == 6) || (i == 8) || (i == 10)) {
			if (*temp != '-') {
				ret = -1;
				break;
			}
			temp++;
		}

		ret = hex2nibble(&val, *temp++);
		if (ret < 0) {
			break;
		}
		uu[i] = val << 4;

		ret = hex2nibble(&val, *temp++);
		if (ret < 0) {
			break;
		}
		uu[i] |= val;

	}

	return ret;
}


void uuid_generate_random(uuid_t out)
{
	size_t nread, ntoread;
	const size_t size = sizeof(uuid_t);
	int useRand;
	const int readAttempts = 100;
	int cnt;

	useRand = 0;
	ntoread = (int)sizeof(uuid_t);

	/* make sure that there is a /dev/random file */
	FILE *file;
	file = fopen("/dev/random", "rb");
	if (file == NULL) {
		file = fopen("/dev/urandom", "rb");
		if (file == NULL) {
			useRand = 1;
		}
	}

	if (useRand == 0) {
		cnt = 0;

		while (ntoread > 0) {
			nread = fread((void *)&out[size - ntoread], 1, ntoread, file);
			if (nread == 0 || ferror(file)) {
				if (cnt++ > readAttempts) {
					useRand = 1;
					break;
				}

				usleep(10 * 1000);
				continue;
			}
			ntoread -= nread;
		}

		fclose(file);
	}

	if (useRand > 0) {
		uuid_useRand(out);
	}

	/* Set uuid variant to 1 - RFC 4122 */
	out[8] &= 0xDF;
	out[8] |= 0x40;

	/* Set uuid version to 4 - The randomly/pseuso-randomly generated */
	out[6] &= 0x0F;
	out[6] |= 0x40;
}


void uuid_generate(uuid_t out)
{
	/* TODO: add fallback to uuid_generate_time when implemented */
	/* (in case that /dev/random and /dev/urandom aren't available) */
	uuid_generate_random(out);
}


void uuid_clear(uuid_t uu)
{
	memset(uu, 0, sizeof(uuid_t));
}
