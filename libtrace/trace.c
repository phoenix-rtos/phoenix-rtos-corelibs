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


#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include <sys/perf.h>
#include <sys/stat.h>

#include <board_config.h>

#include "trace.h"


#define MS_BETWEEN(ts0, ts1) (((ts1).tv_sec - (ts0).tv_sec) * 1000 + ((ts1).tv_nsec - (ts0).tv_nsec) / (1000 * 1000))


#define LOG_TAG "trace: "

/* clang-format off */
#define log_info(fmt, ...) do { if (!ctx->silent) { fprintf(stderr, LOG_TAG fmt "\n", ##__VA_ARGS__); } } while (0)
#define log_warning(fmt, ...) do { log_info("warning: " fmt, ##__VA_ARGS__); } while (0)
#define log_error(fmt, ...) do { log_info("error: " fmt, ##__VA_ARGS__); } while (0)
/* clang-format on */


#ifndef PERF_RTT_ENABLED
#define PERF_RTT_ENABLED 0
#endif


static void done(trace_ctx_t *ctx)
{
	if (ctx->chans != NULL) {
		for (size_t i = 0; i < ctx->nchans; i++) {
			if (ctx->chans[i].dest != NULL && fclose(ctx->chans[i].dest) < 0) {
				log_error("fclose on '%s' failed", ctx->chans[i].name);
			}
		}

		free(ctx->chans);
		ctx->chans = NULL;
	}
	ctx->nchans = 0;
	ctx->bufsize = 0;

	if (ctx->buf != NULL) {
		free(ctx->buf);
		ctx->buf = NULL;
	}

	if (ctx->warnReadTooSlow) {
		log_warning("read buffer was fully utilized during perf_read - read rate may be too slow");
		ctx->warnReadTooSlow = false;
	}

	if (ctx->state != trace_state_initialized) {
		int ret = perf_finish(perf_mode_trace);
		if (ret < 0) {
			log_error("perf_finish failed: %d", ret);
		}
		else {
			ctx->state = trace_state_initialized;
			log_info("finished");
		}
	}

	ctx->rolling = false;
}


#if PERF_RTT_ENABLED
static int initChannels(trace_ctx_t *ctx, size_t bufsize, const char *outputDir)
{
	return EOK;
}


static size_t readChannels(trace_ctx_t *ctx)
{
	return 0;
}
#else
static int initChannels(trace_ctx_t *ctx, size_t bufsize, const char *outputDir)
{
	int res;
	char path[PATH_MAX];
	struct stat sb;

	if (bufsize == 0 || outputDir == NULL) {
		return -EINVAL;
	}

	if (ctx->chans != NULL) {
		/* already initialized */
		return EOK;
	}

	ctx->chans = calloc(ctx->nchans, sizeof(trace_chan_t));
	if (ctx->chans == NULL) {
		log_error("malloc failed");
		return -ENOMEM;
	}

	ctx->buf = malloc(bufsize);
	if (ctx->buf == NULL) {
		log_error("malloc failed");
		return -ENOMEM;
	}
	ctx->bufsize = bufsize;

	res = stat(outputDir, &sb);
	if (res == 0 && !S_ISDIR(sb.st_mode)) {
		log_error("%s is not a directory", outputDir);
		return -ENOENT;
	}

	if (res < 0) {
		if (mkdir(outputDir, 0777) < 0) {
			log_error("mkdir failed: %s", strerror(errno));
			return -EIO;
		}
	}

	for (size_t i = 0; i < ctx->nchans / trace_channel_count; i++) {
		for (size_t j = 0; j < trace_channel_count; j++) {
			trace_chan_t *chan = &ctx->chans[j + trace_channel_count * i];
			res = snprintf(chan->name, sizeof(chan->name), "%s%zu",
					j == trace_channel_meta ? "channel_meta" : "channel_event", i);
			if (res < 0 || res >= sizeof(chan->name)) {
				log_error("snprintf failed");
				return -ENOMEM;
			}
		}
	}

	for (size_t i = 0; i < ctx->nchans; i++) {
		trace_chan_t *chan = &ctx->chans[i];

		res = snprintf(path, sizeof(path), "%s/%s", outputDir, chan->name);
		if (res < 0 || res >= sizeof(path)) {
			log_error("snprintf failed");
			return -ENOMEM;
		}

		FILE *destFile = fopen(path, "wb");
		if (destFile == NULL) {
			log_error("failed to open '%s': %s", path, strerror(errno));
			return -EIO;
		}

		chan->dest = destFile;
	}

	return EOK;
}


static size_t readChannels(trace_ctx_t *ctx)
{
	int bcount;
	size_t total = 0;

	for (size_t i = 0; i < ctx->nchans; i++) {
		trace_chan_t *chan = &ctx->chans[i];

		bcount = perf_read(perf_mode_trace, ctx->buf, ctx->bufsize, i);
		if (bcount < 0) {
			log_error("perf_read failed: %d", bcount);
			return -EIO;
		}

		total += bcount;

		if (ctx->bufsize == bcount && !ctx->rolling) {
			ctx->warnReadTooSlow = true;
		}

		int ret = fwrite(ctx->buf, 1, bcount, chan->dest);
		if (ret < bcount) {
			log_error("failed or partial write: %d/%d", ret, bcount);
			return -EIO;
		}

		log_info("wrote %d/%zu bytes to %s", bcount, ctx->bufsize, chan->name);
	}

	return total;
}
#endif


int trace_init(trace_ctx_t *ctx, bool silent)
{
	if (ctx == NULL) {
		return -EINVAL;
	}

	ctx->outputDir = NULL;
	ctx->chans = NULL;
	ctx->nchans = 0;
	ctx->buf = NULL;
	ctx->bufsize = 0;
	ctx->rolling = false;
	ctx->warnReadTooSlow = false;
	ctx->state = trace_state_initialized;
	ctx->silent = silent;

	return EOK;
}


static int startTrace(trace_ctx_t *ctx, bool rolling)
{
	unsigned int flags = 0;

	if (ctx == NULL || ctx->state != trace_state_initialized) {
		return -EINVAL;
	}

	if (rolling) {
		ctx->rolling = true;
		flags |= PERF_TRACE_FLAG_ROLLING;
	}

	int ret = perf_start(perf_mode_trace, flags, NULL, 0);
	if (ret < 0) {
		log_error("perf_start failed: %d", ret);
		ctx->rolling = false;
		return ret;
	}

	ctx->nchans = ret;
	ctx->state = trace_state_started;

	log_info("started");

	return EOK;
}


int trace_start(trace_ctx_t *ctx)
{
	return startTrace(ctx, true);
}


int trace_stopAndGather(trace_ctx_t *ctx, size_t bufsize, const char *outputDir)
{
	/*
	 * ctx->state == trace_state_initialized is valid, as the background trace
	 * might have been initialized by another process. If it was not, the
	 * perf_stop will fail anyway.
	 */
	if (ctx == NULL || ctx->state == trace_state_stopped) {
		return -EINVAL;
	}

	int ret = perf_stop(perf_mode_trace);
	if (ret < 0) {
		log_error("perf_stop failed: %d", ret);
		done(ctx);
		return ret;
	}

	log_info("stopped");

	ctx->nchans = ret;
	ctx->state = trace_state_stopped;
	ctx->rolling = true;

	ret = initChannels(ctx, bufsize, outputDir);
	if (ret < 0) {
		done(ctx);
		return ret;
	}

	while (readChannels(ctx) > 0) { }

	log_info("nothing left to write, exiting");

	done(ctx);

	return EOK;
}


int trace_record(trace_ctx_t *ctx, uint64_t sleeptimeMs, uint64_t durationMs, size_t bufsize, const char *outputDir)
{
	struct timespec ts[2];
	int ret = startTrace(ctx, false);
	if (ret < 0) {
		return ret;
	}

	ret = initChannels(ctx, bufsize, outputDir);
	if (ret < 0) {
		done(ctx);
		return ret;
	}

	clock_gettime(CLOCK_MONOTONIC_RAW, &ts[0]);

	for (;;) {
		ret = readChannels(ctx);
		if (ret < 0) {
			break;
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &ts[1]);
		uint64_t ms = MS_BETWEEN(ts[0], ts[1]);
		if (ms >= durationMs) {
			/*
			 * exit the trace and gather all that's left - otherwise there could be garbage
			 * at the end of trace in multicore scenario
			 */
			ret = trace_stopAndGather(ctx, bufsize, outputDir);
			break;
		}

		usleep(sleeptimeMs * 1000);
	}

	done(ctx);

	return ret;
}
