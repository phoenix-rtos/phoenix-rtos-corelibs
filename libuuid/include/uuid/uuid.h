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

#ifndef _UUID_H_
#define _UUID_H_

#include <sys/select.h>
#include <time.h>
#include <unistd.h>


typedef unsigned char uuid_t[16];


/* Sets the uuid variable to the NULL value */
extern void uuid_clear(uuid_t uu);


/* Compares the two supplied uuid variables to each other */
extern int uuid_compare(const uuid_t uu1, const uuid_t uu2);


/* Copies the UUID variable src to dst */
extern void uuid_copy(uuid_t dst, const uuid_t src);


/* Creates a new universally unique identifier (UUID) */
extern void uuid_generate(uuid_t out);


/* Creates UUID in all-random format */
extern void uuid_generate_random(uuid_t out);


/* Creates UUID using the current time and the local ethernet MAC address (if available) */
extern void uuid_generate_time(uuid_t out);


/* Same as uuid_generate_time(), except that returns a value 
which denotes whether any of the synchronization mechanisms has been used */
extern int uuid_generate_time_safe(uuid_t out);


/* Creates an MD5 hashed UUID */
extern void uuid_generate_md5(uuid_t out, const uuid_t ns, const char *name, size_t len);


/* Creates an SHA1 hashed UUID */
extern void uuid_generate_sha1(uuid_t out, const uuid_t ns, const char *name, size_t len);


/* Compares the value of the supplied UUID variable to the NULL value */
extern int uuid_is_null(const uuid_t uu);


/* Converts the UUID string into the binary representation */
extern int uuid_parse(const char *in, uuid_t uu);


/* Converts the input string in specified range into the UUID binary representation */
extern int uuid_parse_range(const char *in_start, const char *in_end, uuid_t uu);


/* Converts the supplied UUID from the binary representation into a 36-byte string */
extern void uuid_unparse(const uuid_t uu, char *out);


/* Same as uuid_unparse(), except that the output string is always lowercase */
extern void uuid_unparse_lower(const uuid_t uu, char *out);


/* Same as uuid_unparse(), except that the output string is always uppercase */
extern void uuid_unparse_upper(const uuid_t uu, char *out);


/* Extracts the time at which the supplied time-based UUID was created */
extern time_t uuid_time(const uuid_t uu, struct timeval *ret_tv);


/* Extracts the UUID version */
extern int uuid_type(const uuid_t uu);


/* Extracts the UUID variant */
extern int uuid_variant(const uuid_t uu);


/* Gets name-based UUID for the specified alias */
extern const uuid_t *uuid_get_template(const char *alias);

#endif
