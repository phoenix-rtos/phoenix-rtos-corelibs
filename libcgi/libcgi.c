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

#include "libcgi.h"


int libcgi_getRequestMethod(void)
{
	int len;
	char *method = getenv("REQUEST_METHOD");

	if (method == NULL)
		return LIBCGI_METHOD_ERROR;

	len = strlen(method);

	if (len >= 3 && !strncmp(method, "GET", len))
		return LIBCGI_METHOD_GET;
	else  if (len >= 4 && !strncmp(method, "POST", len)) {
		if (!strncasecmp("multipart/form-data", getenv("CONTENT TYPE"), strlen("multipart/form-data")))
			return LIBCGI_METHOD_MULTIPART;
		else
			return LIBCGI_METHOD_POST;
	} else
		return LIBCGI_METHOD_ERROR;
}


void libcgi_printCode(unsigned code, char *status)
{
	printf("HTTP/1.1 %u %s\n", code, status);
}

void libcgi_printHeaders(char *content_type, char *content_disposition, char *filename, char *raw_headers)
{
	/* TODO: validate? */
	if (content_disposition != NULL && filename != NULL)
		printf("Content-Disposition: %s; filename=%s\n", content_disposition, filename);
	printf("Content-Type: %s\n", content_type);
	if (raw_headers != NULL)
		printf("%s\n", raw_headers);

	printf("\n");
}


char *libcgi_getQueryString(void)
{
	return getenv("QUERY STRING");
}


libcgi_param_t *libcgi_getUrlParams(void)
{
	/* TODO: handle special characters */
	char *query = libcgi_getQueryString();
	char *urlParams, *sep;
	int len, plen;
	libcgi_param_t *head = NULL, *tail, *param;

	if (query == NULL)
		return NULL;

	urlParams = calloc(1, strlen(query) + 1);
	strcpy(urlParams, query);
	len = strlen(urlParams);

	while ((sep = strchr(urlParams, '&')) != NULL)
		sep = 0;
	while ((sep = strchr(urlParams, '=')) != NULL)
		sep = 0;

	plen = 0;
	while (plen < len) {
		param = malloc(sizeof(libcgi_param_t));
		param->next = NULL;
		if (head == NULL) {
			head = param;
			tail = head;
		}

		param->key = urlParams;
		urlParams += strlen(urlParams) + 1;
		plen += strlen(urlParams) + 1;
		param->value = urlParams;
		urlParams += strlen(urlParams) + 1;
		plen += strlen(urlParams) + 1;

		if (head != param) {
			tail->next = param;
			tail = param;
		}
	}

	return head;
}


void libcgi_freeUrlParams(libcgi_param_t *params_head)
{
	libcgi_param_t *param, *victim;

	if (params_head == NULL)
		return;

	/* only do it once since all key value pair is one buffer */
	free(params_head->key);

	param = params_head;
	while (param != NULL) {
		victim = param;
		param = param->next;
		free(victim);
	}
}


static char *libcgi_getMultipartBoundry(void)
{
	char *method = getenv("REQUEST METHOD");
	char *boundry, *str;

	str = strchr(method, '=');
	str++;
	boundry = calloc(1, strlen(str) + 3);
	boundry[0] = '-';
	boundry[1] = '-';
	strcpy(&boundry[2], str);

	return boundry;
}


libcgi_param_t *libcgi_getMultipartParams(void)
{
	libcgi_param_t *head = NULL, *tail, *param;
	char mpl[4096]; /* multipart line */
	char *boundry = libcgi_getMultipartBoundry();
	int nl, pos, blen = strlen(boundry);

	/* TODO: handle file upload */
	while (fgets(mpl, sizeof(mpl), stdin) != NULL) {

		if (!strncmp(boundry, mpl, blen)) {
			nl = strcspn(mpl, "\r\n");
			/* check multipart end boundry */
			if (nl - 2 == blen && strncmp(&mpl[blen], "--", 2))
				break;

			param = malloc(sizeof(libcgi_param_t));
			param->next = NULL;
			if (head == NULL) {
				head = param;
				tail = head;
			}

			/* TODO: check for errors */
			fgets(mpl, sizeof(mpl), stdin);
			pos = strcspn(mpl, "=\"") + 2;
			nl = strcspn(mpl + pos, "\"");
			param->key = calloc(1, nl + 1);
			memcpy(param->key, mpl + pos, nl);

			/* empty line */
			do {
				fgets(mpl, sizeof(mpl), stdin);
			} while (strcspn(mpl, "\r\n") != 0);

			/* value */
			fgets(mpl, sizeof(mpl), stdin);

			/* TODO: handle values larger than 4k */
			nl = strcspn(mpl, "\r\n");
			param->value = calloc(1, nl + 1);
			memcpy(param->value, mpl, nl);

			if (head != param) {
				tail->next = param;
				tail = param;
			}
		}
	}
	return head;
}


void libcgi_freeMultipartParams(libcgi_param_t *params_head)
{
	libcgi_param_t *param, *victim;

	if (params_head == NULL)
		return;

	param = params_head;
	while (param != NULL) {
		victim = param;
		param = param->next;

		free(victim->key);
		free(victim->value);
		free(victim);
	}

}


char *libcgi_getUrlParam(char *paramName)
{
	char *param = NULL;
	char *query = libcgi_getQueryString();
	int pos, len = strlen(query), paramlen = strlen(paramName);

	pos = strcspn(query, paramName);
	if (pos < len) {

		query = query + pos + paramlen + 1;
		len = strcspn(query, "&");
		param = calloc(1, len + 1);
		memcpy(param, query, len);
	}

	return param;
}


char *libcgi_getMultipartParam(char *paramName)
{
	char mpl[4096]; /* multipart line */
	char *boundry = libcgi_getMultipartBoundry();
	int nl, pos, blen = strlen(boundry);
	char *param;

	/* TODO: handle file upload */
	while (fgets(mpl, sizeof(mpl), stdin) != NULL) {

		if (!strncmp(boundry, mpl, blen)) {
			nl = strcspn(mpl, "\r\n");
			/* check multipart end boundry */
			if (nl - 2 == blen && strncmp(&mpl[blen], "--", 2))
				break;

			/* TODO: check for errors */
			fgets(mpl, sizeof(mpl), stdin);
			pos = strcspn(mpl, "=\"") + 2;
			if (!strncmp(paramName, mpl + pos, strlen(paramName))) {

				/* empty line */
				do {
					fgets(mpl, sizeof(mpl), stdin);
				} while (strcspn(mpl, "\r\n") != 0);

				/* value */
				fgets(mpl, sizeof(mpl), stdin);

				/* TODO: handle values larger than 4k */
				nl = strcspn(mpl, "\r\n");
				param = calloc(1, nl + 1);
				memcpy(param, mpl, nl);
				fseek(stdin, 0, SEEK_SET);

				return param;
			}
		}
	}

	fseek(stdin, 0, SEEK_SET);
	return NULL;
}


/* TODO: raw content */
libcgi_param_t *libcgi_getRawContent(void)
{
	return NULL;
}


void libcgi_freeRawContent(libcgi_param_t *params_head)
{

}
