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
#include <errno.h>

#include "cgi.h"


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


static char *libcgi_getMultipartBoundary(void)
{
	char *content = getenv("CONTENT_TYPE");
	char *boundary, *str;

	if (content == NULL)
		return NULL;

	str = strchr(content, '=');
	if (str == NULL)
		return NULL;

	str++;
	boundary = calloc(1, strlen(str) + 3);
	boundary[0] = '-';
	boundary[1] = '-';
	strcpy(&boundary[2], str);

	return boundary;
}

#define CGI_BUF_SIZE (4096 * 16)

libcgi_param_t *libcgi_getMultipartParams(char *store_path)
{
	libcgi_param_t *head = NULL, *tail = NULL, *param;
	char *mp_buf; /* multipart buffer */
	char file_path[256];
	char *mp, *key;

	char *boundary = libcgi_getMultipartBoundary();
	int klen, blen = strlen(boundary);

	char *bbuf;
	int i = 0, bbuf_len, nitems, nlast = 0;

#define RET_ERR \
	do { \
		libcgi_freeMultipartParams(head); \
		free(boundary); \
		free(bbuf); \
		free(mp_buf); \
		return NULL; \
	} while (0)

	mp_buf = calloc(1, CGI_BUF_SIZE);

	bbuf = calloc(1, blen + 3);
	bbuf[0] = '\r';
	bbuf[1] = '\n';
	memcpy(&bbuf[2], boundary, blen);
	bbuf_len = blen + 2;

	if (mp_buf == NULL)
		RET_ERR;

	mp = fgets(mp_buf, CGI_BUF_SIZE, stdin);
	while (mp != NULL) {

		if (!strncmp(boundary, mp, blen)) {
			klen = strcspn(mp, "\r\n");
			/* check multipart end boundary */
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

			if ((mp = fgets(mp_buf, CGI_BUF_SIZE, stdin)) == NULL)
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

				if (key == NULL)
					RET_ERR;

				klen = strcspn(key, "\"");
				param->key = calloc(1, klen + 1);
				memcpy(param->key, key, klen);
			}

			/* ff to the content */
			while ((mp = fgets(mp_buf, CGI_BUF_SIZE, stdin)) != NULL) {
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
				param->stream = fopen(file_path, "w+");
			}

			if (param->stream == NULL)
				RET_ERR;

			mp = mp_buf;
			nitems = 0;
			i = 0;
			/* read byte by byte from stdin to detect cgi boundary */
			while ((nitems += fread(mp, 1, CGI_BUF_SIZE - nitems, stdin)) > 0) {

				if (errno == EINTR)
					continue;

				if (nlast == nitems)
					RET_ERR;

				while ((mp - mp_buf < nitems)) {
					if (*mp != bbuf[i]) {
						if (*mp != bbuf[0])
							i = 0;
						else
							i = 1;
					}
					else {
						i++;
					}
					mp++;
					if (i >= bbuf_len)
						break;
				}

				if (fwrite(mp_buf, mp - mp_buf - i, 1, param->stream) < 0)
					RET_ERR;
				memmove(mp_buf, mp - i, nitems - (mp - mp_buf - i));
				nitems = nitems - (mp - mp_buf - i);

				nlast = nitems;
				if (i >= bbuf_len) {
					mp = mp_buf + 2;
					i = 0;
					break;
				}
				else {
					mp = mp_buf + nitems;
				}
			}

			fseek(param->stream, 0, SEEK_SET);
		}
		else {
			RET_ERR;
		}
	}

	free(boundary);
	free(bbuf);
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