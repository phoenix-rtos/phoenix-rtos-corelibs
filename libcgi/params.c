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


char *libcgi_getQueryString(void)
{
	return getenv("QUERY_STRING");
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
		*sep = '=';

	sep = urlParams;
	while ((sep = strchr(sep, '=')) != NULL) {
		len--;
		*sep = 0;
		sep++;
	}

	plen = 0;
	while (plen < len) {
		param = malloc(sizeof(libcgi_param_t));
		param->next = NULL;
		if (head == NULL) {
			head = param;
			tail = head;
		}

		param->key = urlParams;
		plen += strlen(urlParams);
		urlParams += strlen(urlParams) + 1;
		param->value = urlParams;
		plen += strlen(urlParams);
		urlParams += strlen(urlParams) + 1;

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
	char *content = getenv("CONTENT_TYPE");
	char *boundry, *str;

	if (content == NULL)
		return NULL;

	str = strchr(content, '=');
	if (str == NULL)
		return NULL;

	str++;
	boundry = calloc(1, strlen(str) + 3);
	boundry[0] = '-';
	boundry[1] = '-';
	strcpy(&boundry[2], str);

	return boundry;
}


libcgi_param_t *libcgi_getMultipartParams(char *store_path)
{
	libcgi_param_t *head = NULL, *tail = NULL, *param;
	char *mp_buf, *tmp_buf; /* multipart line */
	char file_path[256];
	char *mp, *key, *tmp = NULL;

	char *boundry = libcgi_getMultipartBoundry();
	int klen, blen = strlen(boundry);

#define RET_ERR  	do { \
						libcgi_freeMultipartParams(head); \
						free(mp_buf); \
						free(tmp_buf); \
						return NULL; \
					} while (0)

	mp_buf = calloc(1, 4096);
	tmp_buf = calloc(1, 128);

	if (mp_buf == NULL || tmp_buf == NULL)
		RET_ERR;

	mp = fgets(mp_buf, 4096, stdin);
	while (mp != NULL) {

		if (!strncmp(boundry, mp, blen)) {
			klen = strcspn(mp, "\r\n");
			/* check multipart end boundry */
			if (klen - 2 == blen && !strncmp(&mp[blen], "--", 2))
				break;

			param = calloc(1, sizeof(libcgi_param_t));
			if (head == NULL) {
				head = param;
				tail = head;
			}
			else {
				tail->next = param;
				tail = param;
			}

			if ((mp = fgets(mp_buf, 4096, stdin)) == NULL)
				RET_ERR;

			key = strstr(mp, "filename=\"") + strlen("filename=\"");
			if (key != NULL) {
				klen = strcspn(key, "\"");
				param->filename = calloc(1, klen + 1);
				param->type = LIBCGI_PARAM_FILE;
				memcpy(param->filename, key, klen);
			}
			else {
				key = strstr(mp, "name=\"") + strlen("name=\"");

				if (key == NULL) RET_ERR;

				klen = strcspn(key, "\"");
				param->key = calloc(1, klen + 1);
				memcpy(param->key, key, klen);
			}

			/* ff to the content */
			while ((mp = fgets(mp_buf, 4096, stdin)) != NULL) {
				if (!strcmp(mp, "\r\n"))
					break;
			}

			if (mp == NULL)
				RET_ERR;

			if (param->type != LIBCGI_PARAM_FILE || store_path == NULL) {
				param->stream = tmpfile();
			 }
			 else {
				sprintf(file_path, "%s/%s", store_path, param->filename);
				param->stream  = fopen(file_path, "w+");
			}

			if (param->stream == NULL)
				RET_ERR;

			while ((mp = fgets(mp_buf, 4096, stdin)) != NULL) {
				if (!strncmp(boundry, mp, blen)) {
					break;
				}

				if (tmp != NULL) {
					fputs(tmp, param->stream);
					tmp = NULL;
				}

				if (!strncmp(mp, "\r\n", 2)) {
					tmp = fgets(tmp_buf, 128, stdin);
					if (tmp == NULL)
						RET_ERR;
					if (!strncmp(boundry, tmp, blen))
						break;
					else {
						fputs(mp, param->stream);
						continue;
					}
				}

				fputs(mp, param->stream);
			}
			fseek(param->stream, 0, SEEK_SET);
		}
		else
			RET_ERR;
	}

	free(tmp_buf);
	free(mp_buf);
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
		if (victim->type == LIBCGI_PARAM_DEFAULT)
			free(victim->key);
		else
			free(victim->filename);
		fclose(victim->stream);
		free(victim);
	}
}


char *libcgi_getUrlParam(char *paramName)
{
	char *param = NULL;
	char *query = libcgi_getQueryString();
	int pos, len = strlen(query), paramlen = strlen(paramName);

	if (query == NULL)
		return NULL;

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