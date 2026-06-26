/*
 * Phoenix-RTOS
 *
 * Logging library
 *
 * Copyright 2026 Phoenix Systems
 * Author: Mikolaj Matalowski
 *
 * %LICENSE%
 */


#ifndef LOGGER_H
#define LOGGER_H

#include <stdlib.h>

#define VERBOSE

#ifdef VERBOSE
#define LOG_INFO(fmt, ...) printf("LOG (info): " fmt "\n", ##__VA_ARGS__)
#define LOG_ERR(fmt, ...)  printf("LOG (err): " fmt "\n", ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...)
#define LOG_ERR(fmt, ...)
#endif

typedef struct _logger_ctx_t *logger_ctx_t;
typedef int (*logger_write_callback_t)(logger_ctx_t, const void *, size_t, void *);

struct logger_options_t {
	/*
	 Those options have to be tuned by application based
	 on how much logs are really produced and how much
	 overhead will be introduced by running dump thread
	 periodically.
	*/
	time_t delay_milis;
	/* Queue has to be at least page size due to mirroring */
	size_t queue_size;
	/*
	 TODO: Add option to select behaviour on bigger load.
	 Currentlly if queue is full due to downstream thread
	 failing to process logs, new logs are dropped not to
	 slow down hotpath. However this might be the stuff
	 we want to monitor.
	 TODO: Add multiple loggging levels, such that under
	 load log selection can be made (drop info logs and
	 keep warningns and errors with more precise context
	 dump)
	*/
};

/* Allocate resources and start async log thread */
logger_ctx_t logger_init(struct logger_options_t *options);

/* Stop asyn log thread and clear resources */
void logger_exit(logger_ctx_t ctx);

/* Push new callback */
int logger_push_callback(logger_ctx_t ctx, logger_write_callback_t callback, void *);
/* Pop callback (organised in as stack for simplicity) */
logger_write_callback_t logger_pop_callback(logger_ctx_t ctx);

/* Place message in queue for async processing */
int logger_log(logger_ctx_t ctx, const char *fmt, ...);

#endif
