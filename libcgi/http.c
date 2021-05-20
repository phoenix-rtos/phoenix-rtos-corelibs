/*
 * Phoenix-RTOS
 *
 * cgi helper library
 *
 * helps with cgi and related stuff
 *
 * Copyright 2019 Phoenix Systems
 * Author: Kamil Amanowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#include "cgi.h"


void libcgi_printCode(unsigned code, char *status)
{
	printf("Status: %d\n", code);
}

void libcgi_printHeaders(char *content_type, char *content_disposition, char *filename, char *raw_headers)
{
	/* TODO: validate? */
	if (content_disposition != NULL && filename != NULL)
		printf("Content-Disposition: %s; filename=%s\n", content_disposition, filename);
	printf("Content-Type: %s\n", content_type);
	if (raw_headers != NULL)
		printf("%s", raw_headers);

	printf("\n");
}