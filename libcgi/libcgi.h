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

#ifndef _LIBCGI_H_
#define _LIBCGI_H_

/* request method */
enum {
	LIBCGI_METHOD_POST,
	LIBCGI_METHOD_MULTIPART,
	LIBCGI_METHOD_GET,
	LIBCGI_METHOD_ERROR
};



extern int libcgi_getRequestMethod(void);


extern char *libcgi_getQueryString(void);

/* output header printing */
extern void libcgi_printCode(unsigned code, char *status);


extern void libcgi_printHeaders(char *content_type, char *content_disposition, char *filename, char *raw_headers);


/* parameter structure */
typedef struct _libcgi_param {
	struct _libcgi_param *next;
	int type; /* to recognize param type - not used yet */
	char *key;
	char *value;
	char *filename; /* for file types - not used yet*/
} libcgi_param_t;


/* url parameters */
extern libcgi_param_t *libcgi_getUrlParams(void);
extern void libcgi_freeUrlParams(libcgi_param_t *params_head);


/* multipart parameters */
extern libcgi_param_t *libcgi_getMultipartParams(void);
extern void libcgi_freeMultipartParams(libcgi_param_t *params_head);

#endif /* _LIBCGI_H_ */
