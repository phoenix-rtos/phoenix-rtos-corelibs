/*
 * Phoenix-RTOS
 *
 * Trace library
 *
 * Copyright 2025 Phoenix Systems
 * Author: Adam Greloch
 *
 * %LICENSE%
 */

#ifndef _PERF_H
#define _PERF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
	char name[16];
	FILE *dest;
} chan_t;


typedef struct {
	const char *outputDir;

	chan_t *chans;
	size_t nchans;

	char *buf;
	size_t bufsize;

	bool rolling;
	bool warnReadTooSlow;

	/* clang-format off */
	enum { trace_state_init = 1, trace_state_started, trace_state_stopped } state;
	/* clang-format on */

	bool silent;
} trace_ctx_t;


/* Initializes trace context. */
int trace_init(trace_ctx_t *ctx, bool silent);


/* Records trace for a given duration. If RTT is not used, gathers trace during recording in sleeptimeMs intervals to outputDir */
int trace_record(trace_ctx_t *ctx, time_t sleeptimeMs, time_t durationMs, size_t bufsize, const char *outputDir);


/* Starts trace in rolling window mode. Trace must be completed with call to trace_stopAndGather() */
int trace_start(trace_ctx_t *ctx);


/* Ends trace started with trace_start(). If RTT is not used, gathers recorded trace to outputDir */
int trace_stopAndGather(trace_ctx_t *ctx, size_t bufsize, const char *outputDir);


#endif
