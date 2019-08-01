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

#define CGI_BUF_SIZE 4096

libcgi_param_t *libcgi_getMultipartParams(char *store_path)
{
	libcgi_param_t *head = NULL, *tail = NULL, *param;
	char *mp_buf; /* multipart buffer */
	char file_path[256];
	char *mp, *key;

	char *boundry = libcgi_getMultipartBoundry();
	int klen, blen = strlen(boundry), i = 0;

#define RET_ERR  	do { \
						libcgi_freeMultipartParams(head); \
						free(mp_buf); \
						return NULL; \
					} while (0)

	mp_buf = calloc(1, CGI_BUF_SIZE);

	if (mp_buf == NULL)
		RET_ERR;

	mp = fgets(mp_buf, CGI_BUF_SIZE, stdin);
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
			/* read byte by byte from stdin to detect cgi boundry */
			while (fread(mp, 1, 1, stdin) > 0) {

				/*check for boundry start */
				if (*mp == '\r') {
					mp++;
					/* check for possible buffer overflow */
					if (mp - mp_buf >= CGI_BUF_SIZE) {
						fwrite(mp_buf, 1, CGI_BUF_SIZE - 1, param->stream);
						mp = mp_buf;
						*mp++ = '\r';
					}
					/* check second character */
					if (fread(mp, 1 ,1, stdin) < 1)
						RET_ERR;

					if (*mp == '\n') {
						/* check if we have space in buffer for whole boundry */
						if (mp - mp_buf > (CGI_BUF_SIZE - blen - 4)) {
							fwrite(mp_buf, 1, mp - mp_buf - 1, param->stream);
							mp = mp_buf;
							*mp++ = '\r';
							*mp++ = '\n';
						}
						else
							mp++;
						/* boundry check */
						while (i < blen && fread(mp, 1 , 1, stdin) > 0) {
							*(mp + 1) = 0;
							if (*mp != boundry[i]) {
								mp++;
								i = 0;
								break;
							}
							mp++;
							i++;
						}
						/* boundry found. flush buffer and get to the next element */
						if (i == blen) {
							fwrite(mp_buf, 1, mp - mp_buf - blen - 2, param->stream);
							if (fread(mp, 2 , 1, stdin) < 1)
								RET_ERR;

							mp[2] = 0;
							mp = mp - blen;
							i = 0;
							break;
						}
					}
					else {
						mp++;
					}
				}
				else {
					mp++;
				}
				/* check if buffer is full */
				if (mp - mp_buf >= CGI_BUF_SIZE) {
					fwrite(mp_buf, 1, CGI_BUF_SIZE, param->stream);
					mp = mp_buf;
				}
			}
			fseek(param->stream, 0, SEEK_SET);
		}
		else {
			RET_ERR;
		}
	}

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