/*
 * Phoenix-RTOS
 *
 * Lock-free SPSC FIFO
 *
 * Copyright 2025 Phoenix Systems
 * Author: Ziemowit Leszczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef LF_FIFO_H
#define LF_FIFO_H

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdalign.h>
#include <string.h>
#include <assert.h>

#ifdef ATOMIC_UINT_LOCK_FREE
_Static_assert(ATOMIC_UINT_LOCK_FREE == 2, "atomic_uint may not be lock-free on this platform.");
#else
_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "atomic_int may not be lock-free on this platform.");
#endif

#ifndef LF_FIFO_CACHELINE
#define LF_FIFO_CACHELINE 64u
#endif

typedef struct lf_fifo_s lf_fifo_t;

/*
 * Bounded, circular FIFO queue using C11 atomics for lock-free
 * operation between one producer thread and one consumer thread.
 * Buffer size must be a power of 2 and >= 2. Requires lock-free
 * atomic_uint.
 * For non-overwriting API effective capacity is (size - 1) elements.
 * One slot is always left unused to avoid empty & full states ambiguity.
 * For overwriting API effective capacity is size elements. When full,
 * pushes discard the oldest element to make space.
 * Mixing non-overwriting and overwriting calls is undefined and not
 * supported. Use one API per FIFO instance.
 */
struct lf_fifo_s {
	atomic_uint head __attribute__((aligned(LF_FIFO_CACHELINE)));

	atomic_uint tail __attribute__((aligned(LF_FIFO_CACHELINE)));

	unsigned int size;
	unsigned int mask; /* size - 1 */
	uint8_t *data;
};


/* --------------------- Common API --------------------- */

static inline void lf_fifo_init(lf_fifo_t *f, uint8_t *data, unsigned int size)
{
	assert(size >= 2u && (size & (size - 1u)) == 0u);

	atomic_init(&f->head, 0u);
	atomic_init(&f->tail, 0u);

	f->size = size;
	f->mask = size - 1u;
	f->data = data;
}


/* Returns 1 if FIFO is empty, 0 otherwise. */
static inline bool lf_fifo_empty(const lf_fifo_t *f)
{
	unsigned int tail = atomic_load_explicit(&f->tail, memory_order_relaxed);
	unsigned int head = atomic_load_explicit(&f->head, memory_order_relaxed);

	return (head == tail);
}


/* --------------------- Non-overwriting API --------------------- */

/* Returns 1 if element has been pushed, 0 otherwise. */
static inline unsigned int lf_fifo_push(lf_fifo_t *f, uint8_t byte)
{
	unsigned int head = atomic_load_explicit(&f->head, memory_order_relaxed);
	unsigned int tail = atomic_load_explicit(&f->tail, memory_order_acquire);
	unsigned int next = (head + 1u) & f->mask;

	if (next == tail) {
		/* full */
		return 0u;
	}

	f->data[head] = byte;

	/* publish new head so consumer can see data */
	atomic_store_explicit(&f->head, next, memory_order_release);

	return 1u;
}


/* Push up to n bytes. Returns how many actually pushed. */
static inline unsigned int lf_fifo_push_many(lf_fifo_t *f, const uint8_t *src, unsigned int n)
{
	if (n == 0u) {
		return 0u;
	}

	unsigned int head = atomic_load_explicit(&f->head, memory_order_relaxed);
	unsigned int tail = atomic_load_explicit(&f->tail, memory_order_acquire);
	unsigned int free = (tail - head - 1u) & f->mask;

	if (free == 0u) {
		return 0u;
	};

	if (n > free) {
		n = free;
	}

	/* contiguous to buffer end */
	unsigned int m = f->size - head;
	if (m > n) {
		m = n;
	}

	memcpy(f->data + head, src, m);
	if (n > m) {
		memcpy(f->data, src + m, n - m);
	}

	/* publish new head so consumer can see data */
	atomic_store_explicit(&f->head, (head + n) & f->mask, memory_order_release);

	return n;
}


/* Returns 1 if element has been popped, 0 otherwise. */
static inline unsigned int lf_fifo_pop(lf_fifo_t *f, uint8_t *byte)
{
	unsigned int tail = atomic_load_explicit(&f->tail, memory_order_relaxed);
	unsigned int head = atomic_load_explicit(&f->head, memory_order_acquire);

	if (head == tail) {
		/* empty */
		return 0u;
	}

	*byte = f->data[tail];

	/* publish new tail so producer can reuse slot */
	atomic_store_explicit(&f->tail, (tail + 1u) & f->mask, memory_order_release);

	return 1u;
}


/* Pop up to n bytes. Returns how many actually popped. */
static inline unsigned int lf_fifo_pop_many(lf_fifo_t *f, uint8_t *dst, unsigned int n)
{
	if (n == 0u) {
		return 0u;
	}

	unsigned int tail = atomic_load_explicit(&f->tail, memory_order_relaxed);
	unsigned int head = atomic_load_explicit(&f->head, memory_order_acquire);
	unsigned int used = (head - tail) & f->mask;

	if (used == 0u) {
		return 0u;
	}

	if (n > used) {
		n = used;
	}

	/* contiguous to buffer end */
	unsigned int m = f->size - tail;
	if (m > n) {
		m = n;
	}

	memcpy(dst, f->data + tail, m);
	if (n > m) {
		memcpy(dst + m, f->data, n - m);
	}

	/* publish new tail so producer can reuse slot */
	atomic_store_explicit(&f->tail, (tail + n) & f->mask, memory_order_release);

	return n;
}


/* Returns 1 if FIFO is full, 0 otherwise. */
static inline bool lf_fifo_full(const lf_fifo_t *f)
{
	unsigned int head = atomic_load_explicit(&f->head, memory_order_relaxed);
	unsigned int tail = atomic_load_explicit(&f->tail, memory_order_relaxed);
	unsigned int next = (head + 1u) & f->mask;

	return (next == tail);
}


/* Returns number of used elements. */
static inline unsigned int lf_fifo_used(const lf_fifo_t *f)
{
	unsigned int tail = atomic_load_explicit(&f->tail, memory_order_relaxed);
	unsigned int head = atomic_load_explicit(&f->head, memory_order_relaxed);

	return (head - tail) & f->mask;
}

/* Returns number of free slots available. */
static inline unsigned int lf_fifo_free(const lf_fifo_t *f)
{
	unsigned int head = atomic_load_explicit(&f->head, memory_order_relaxed);
	unsigned int tail = atomic_load_explicit(&f->tail, memory_order_relaxed);

	return (tail - head - 1u) & f->mask;
}


/* --------------------- Overwriting API --------------------- */

/* Always succeeds. If full, overwrites oldest. */
static inline void lf_fifo_ow_push(lf_fifo_t *f, uint8_t byte)
{
	unsigned int head = atomic_load_explicit(&f->head, memory_order_relaxed);

	f->data[head & f->mask] = byte;

	/* publish new head so consumer can see data */
	atomic_store_explicit(&f->head, head + 1u, memory_order_release);
}


/* Always succeeds. If full, overwrites oldest. */
static inline void lf_fifo_ow_push_many(lf_fifo_t *f, const uint8_t *src, unsigned int n)
{
	if (n == 0u) {
		return;
	}

	if (n > f->size) {
		src += n - f->size;
		n = f->size;
	}

	unsigned int head = atomic_load_explicit(&f->head, memory_order_relaxed);

	/* contiguous to buffer end */
	unsigned int m = f->size - (head & f->mask);
	if (m > n) {
		m = n;
	}

	memcpy(f->data + (head & f->mask), src, m);
	if (n > m) {
		memcpy(f->data, src + m, n - m);
	}

	/* publish new head so consumer can see data */
	atomic_store_explicit(&f->head, head + n, memory_order_release);
}


/* Returns 1 if element has been popped, 0 otherwise. */
static inline unsigned int lf_fifo_ow_pop(lf_fifo_t *f, uint8_t *byte)
{
	unsigned int tail = atomic_load_explicit(&f->tail, memory_order_relaxed);
	unsigned int head = atomic_load_explicit(&f->head, memory_order_acquire);
	unsigned int used = head - tail;

	if (used == 0u) {
		/* empty */
		return 0u;
	}

	if (used > f->size) {
		/* overwrite */
		tail = head - f->size;
		used = f->size;
	}

	*byte = f->data[tail & f->mask];

	atomic_store_explicit(&f->tail, tail + 1u, memory_order_relaxed);

	return 1u;
}


/* Pop up to n bytes. Returns how many actually popped. */
static inline unsigned int lf_fifo_ow_pop_many(lf_fifo_t *f, uint8_t *dst, unsigned int n)
{
	if (n == 0u) {
		return 0u;
	}

	/* refresh tail because producer may have advanced it */
	unsigned int tail = atomic_load_explicit(&f->tail, memory_order_relaxed);
	unsigned int head = atomic_load_explicit(&f->head, memory_order_acquire);
	unsigned int used = head - tail;

	if (used == 0u) {
		/* empty */
		return 0u;
	}

	if (used > f->size) {
		/* overwrite */
		tail = head - f->size;
		used = f->size;
	}

	if (n > used) {
		n = used;
	}

	/* contiguous to buffer end */
	unsigned int m = f->size - (tail & f->mask);
	if (m > n) {
		m = n;
	}

	memcpy(dst, f->data + (tail & f->mask), m);
	if (n > m) {
		memcpy(dst + m, f->data, n - m);
	}

	/* publish new tail so producer can reuse slot */
	atomic_store_explicit(&f->tail, tail + n, memory_order_release);

	return n;
}


/* Returns number of used elements. */
static inline unsigned int lf_fifo_ow_used(const lf_fifo_t *f)
{
	unsigned int tail = atomic_load_explicit(&f->tail, memory_order_relaxed);
	unsigned int head = atomic_load_explicit(&f->head, memory_order_relaxed);
	unsigned int used = head - tail;

	if (used > f->size) {
		/* overwrite */
		used = f->size;
	}

	return used;
}

#endif
