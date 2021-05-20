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

#ifndef _CGI_H_
#define _CGI_H_

#include <stdio.h>

/* request method */
enum {
	LIBCGI_METHOD_POST,
	LIBCGI_METHOD_POST_MULTIPART,
	LIBCGI_METHOD_GET,
	LIBCGI_METHOD_DELETE,
	LIBCGI_METHOD_ERROR
};

/* auth modes */
enum {
	LIBCGI_AUTH_COOKIE_FILE
};


int libcgi_getRequestMethod(void);


char *libcgi_getQueryString(void);


/* output header printing */
void libcgi_printCode(unsigned code, char *status);


void libcgi_printHeaders(char *content_type, char *content_disposition, char *filename, char *raw_headers);


/* parameter structure */
typedef struct _libcgi_param {
	struct _libcgi_param *next;
	enum { LIBCGI_PARAM_DEFAULT,
		LIBCGI_PARAM_FILE } type;
	union {
		char *key;
		char *filename;
	};
	union {
		char *value;
		FILE *stream;
	};
} libcgi_param_t;


/* override me */
int libcgi_isLogged(int argc, ...);


/* url parameters */
libcgi_param_t *libcgi_getUrlParams(void);
void libcgi_freeUrlParams(libcgi_param_t *params_head);


/* multipart parameters */
libcgi_param_t *libcgi_getMultipartParams(char *store_path);
void libcgi_freeMultipartParams(libcgi_param_t *params_head);

#endif /* _CGI_H_ */
