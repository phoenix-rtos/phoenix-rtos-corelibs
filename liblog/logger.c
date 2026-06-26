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


#include "logger.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>

#include <sys/threads.h>
#include <sys/time.h>

#define LOGGER_THREAD_PRIO       2
#define LOGGER_THREAD_STACK_SIZE _PAGE_SIZE

#define LOGGER_MAX_MSG_LEN   512
#define LOGGER_MAX_CALLBACKS 4

#define ANON_FD        -1
#define PAGE_MASK      (_PAGE_SIZE - 1ul)
#define PAGE_ROUND(sz) (sz & ~(PAGE_MASK))

struct logger_stats_t {
	size_t file_dump_failures;
};

struct _logger_ctx_t {
	time_t delay_milis;
	handle_t dump_thread;
	volatile bool logger_exit;

	logger_write_callback_t callbacks[LOGGER_MAX_CALLBACKS];
	void *callback_data[LOGGER_MAX_CALLBACKS];
	size_t callback_count;

	size_t len, dropped_logs;
	volatile ssize_t wr_ptr, rd_ptr;
	char *queue;
};


static size_t logger_alloc_mirrored_buffer(size_t size, void **out_buf)
{
	if (size < _PAGE_SIZE) {
		LOG_ERR("Buffer has to be at least page size");
		return 0;
	}
	if (NULL == out_buf) {
		LOG_ERR("No buffer to allocate to");
		return 0;
	}

	size = PAGE_ROUND(size);
	void *buffer = mmap(NULL, size * 2, PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_PRIVATE, ANON_FD, (off_t)0ul);

	if (MAP_FAILED == buffer) {
		LOG_ERR("Failed to mmap buffer");
	}

	/*
	 mmap will first unmap provided virtual address and after
	 that map it again to physical memory we already have.
	*/
	void *last_page = (void *)((uintptr_t)buffer + size);
	void *tmp = mmap(last_page, size, PROT_READ | PROT_WRITE,
			MAP_FIXED | MAP_PRIVATE | MAP_ANON | MAP_PHYSMEM, ANON_FD, va2pa(buffer));

	if (MAP_FAILED == tmp) {
		LOG_ERR("Failed to mirror mapping");
		munmap(buffer, size + _PAGE_SIZE);
	}

	*out_buf = buffer;
	LOG_INFO("Allocated queue 0x%lx", (uint64_t)*out_buf);
	return size;
}

static void logger_unmap_mirrored_buffer(logger_ctx_t ctx)
{
	/* Unmap mirror */
	munmap((void *)((uintptr_t)ctx->queue + ctx->len), ctx->len);
	/* Unmap buffer */
	munmap(ctx->queue, ctx->len);
}

static void logger_thread(void *arg)
{
	logger_ctx_t ctx = (logger_ctx_t)arg;
	ssize_t size;
	void *buffer;
	int i;

	*(volatile int *)&ctx->dump_thread = gettid();

	do {
		if (ctx->rd_ptr != ctx->wr_ptr) {
			size = ctx->wr_ptr - ctx->rd_ptr;
			size = size < 0 ? ctx->len + size : size;

			/* Again safe since buffer is mirrored */
			buffer = (void *)((uintptr_t)ctx->queue + ctx->rd_ptr);

			/* Callbacks */
			for (i = 0; i < ctx->callback_count; i++) {
				ctx->callbacks[i](ctx, buffer, size, ctx->callback_data[i]);
			}

			/* Cosnume data from buffer */
			ctx->rd_ptr = (ctx->rd_ptr + size) % ctx->len;
		}
		usleep(ctx->delay_milis * 1000);
	} while (!ctx->logger_exit);

	LOG_INFO("Logger thread exit");

	endthread();
}


logger_ctx_t logger_init(struct logger_options_t *options)
{
	if (NULL == options)
		return NULL;

	logger_ctx_t ctx = calloc(1, sizeof(*ctx));
	if (NULL == ctx)
		return NULL;

	ctx->len = logger_alloc_mirrored_buffer(options->queue_size, (void **)&ctx->queue);
	if (0 == ctx->len) {
		free(ctx);
		return NULL;
	}
	ctx->wr_ptr = ctx->rd_ptr = 0;
	ctx->delay_milis = options->delay_milis;

	void *stack = malloc(LOGGER_THREAD_STACK_SIZE);
	if (NULL == stack) {
	}

	void *logger_thread_stack = malloc(LOGGER_THREAD_STACK_SIZE);
	if (NULL == logger_thread_stack) {
		logger_unmap_mirrored_buffer(ctx);
		return NULL;
	}

	beginthread(logger_thread, LOGGER_THREAD_PRIO,
			(void *)logger_thread_stack, LOGGER_THREAD_STACK_SIZE, ctx);

	return ctx;
}

void logger_exit(logger_ctx_t ctx)
{
	ctx->logger_exit = true;

	LOG_INFO("Waiting for thread to finish (tid=%d)", ctx->dump_thread);
	threadJoin(ctx->dump_thread, 0);

	LOG_INFO("Trying to unmap (mirror=0x%lx), (queue=0x%lx), (size=%lu)",
			(uintptr_t)ctx->queue + ctx->len,
			(uint64_t)ctx->queue, ctx->len);
	logger_unmap_mirrored_buffer(ctx);
	LOG_INFO("Cleared resources");
}

int logger_push_callback(logger_ctx_t ctx, logger_write_callback_t callback, void *data)
{
	if (LOGGER_MAX_CALLBACKS == ctx->callback_count)
		return -ENOMEM;
	ctx->callbacks[ctx->callback_count] = callback;
	ctx->callback_data[ctx->callback_count] = data;
	return ++ctx->callback_count;
}

logger_write_callback_t logger_pop_callback(logger_ctx_t ctx)
{
	if (0 == ctx->callback_count)
		return NULL;
	return ctx->callbacks[ctx->callback_count--];
}


int logger_log(logger_ctx_t ctx, const char *fmt, ...)
{
	/* Cannot be static since shared between threads */
	char buffer[LOGGER_MAX_MSG_LEN];
	va_list args;
	ssize_t len, diff;
	volatile ssize_t wr_ptr;

	/*
	 TODO: Remove parsing from logger, instead
	 copy raw data to temporary buffer?
	*/
	va_start(args, fmt);
	len = vsnprintf(buffer, LOGGER_MAX_MSG_LEN, fmt, args);
	if (len < 0)
		return -ENOMEM;

	/* Acuquire ring buffer space */
	do {
		wr_ptr = ctx->wr_ptr;
		diff = ctx->rd_ptr - ctx->wr_ptr;
		if ((diff > 0 ? diff : ctx->len + diff) < len) {
			ctx->dropped_logs++;
			return -EAGAIN;
		}
	} while (!atomic_compare_exchange_strong(
			&ctx->wr_ptr, &wr_ptr, (wr_ptr + len) % ctx->len));

	/* Write message - mirror mapping allows alias to 1st page */
	memcpy(ctx->queue + wr_ptr, buffer, len);

	return 0;
}
