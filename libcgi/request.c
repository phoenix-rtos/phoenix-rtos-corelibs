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

#define LIBCGI_CONTENT_TYPE_MULTIPART "multipart/form-data"


int libcgi_getRequestMethod(void)
{
	int len;
	char *method = getenv("REQUEST_METHOD");
	char *content = NULL;

	if (method == NULL)
		return LIBCGI_METHOD_ERROR;

	len = strlen(method);

	if (!strncmp(method, "POST", len)) {
		content = getenv("CONTENT_TYPE");
		if (content != NULL && !strncmp(LIBCGI_CONTENT_TYPE_MULTIPART, content, strlen(LIBCGI_CONTENT_TYPE_MULTIPART)))
			return LIBCGI_METHOD_POST_MULTIPART;
		else
			return LIBCGI_METHOD_POST;
	}
	else if (!strncmp(method, "GET", len))
		return LIBCGI_METHOD_GET;
	else if (!strncmp(method, "DELETE", len))
		return LIBCGI_METHOD_DELETE;
	else
		return LIBCGI_METHOD_ERROR;
}