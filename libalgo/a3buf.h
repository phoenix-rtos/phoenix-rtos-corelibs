/*
 * Phoenix-RTOS
 *
 * a3buf - Atomic Triple Buffer
 *
 * Generic, lock-free, SPSC triple buffering abstraction
 *
 * Copyright 2026 Phoenix Systems
 * Author: Jakub Smolaga
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _LIBALGO_A3BUF_H_
#define _LIBALGO_A3BUF_H_


#include <stdalign.h>
#include <stdatomic.h>
#include <stdbool.h>


#if !defined(ATOMIC_INT_LOCK_FREE) || (ATOMIC_INT_LOCK_FREE != 2)
#error "a3buf: atomic_int/uint are not lock free on this platform"
#endif

/*
 * Reader, writer and shared state are not in separate cache lines by default
 * since the memory overhead is large (12B -> 192B), and not only is it not
 * needed on non-SMP targets, but even on SMP targets, the false-sharing
 * overhead only matters under high contention which will very often not be the
 * case.
 * If you decide to set this macro there are two things to be careful about:
 * - If you define this macro in one module but not in another, sharing this
 *   structure across those modules will result in ABI breakage, since the
 *   structure layout will be different across those modules
 * - If you use a3buf_t somewhere in malloced memory then it will probably not
 *   be aligned
 */
#if defined(A3BUF_CACHE_LINE_SIZE) && (A3BUF_CACHE_LINE_SIZE > 0)
#define A3BUF_ALIGN (A3BUF_CACHE_LINE_SIZE)
#else
#define A3BUF_ALIGN (alignof(atomic_uint))
#endif

_Static_assert(A3BUF_ALIGN >= alignof(atomic_uint), "a3buf: alignment must not be lowered");
_Static_assert(A3BUF_ALIGN >= alignof(unsigned int), "a3buf: alignment must not be lowered");

/*
 * Number of buffers needed for triple buffering
 * Convenience macro which can make it more obvious where a3buf is used
 */
#define A3BUF_COUNT (3U)

/* Mask for the index value in the shared state */
#define A3BUF_MASK (0x3U)

/* Dirty flag in the shared state */
#define A3BUF_DIRTY (1U << 2U)

/*
 * Lock-Free, SPSC, triple buffering management structure. Operates purely on
 * indices to avoid creating unnecessary restrictions for possible usecases.
 * It manages three indices:
 * - Reader buffer index
 * - Writer buffer index
 * - Shared buffer index
 * None of these three indices ever overlap. The basic operations are:
 * - The writer can write to their buffer without synchronization (a3buf_wrIdx)
 * - The writer can submit their buffer to the shared pool (a3buf_submit)
 * - The reader can fetch a buffer from the shared pool (a3buf_rdFetch)
 * - The reader can read their buffer without synchronization (a3buf_rdIdx)
 *
 * Example of a data structure that can be built on top of this primitive:
 *
 * typedef struct { float x, y, z; } thing_t;
 * typedef struct { thing_t slots[A3BUF_COUNT]; a3buf_t a3; } myBuf_t;
 *
 * void myBuf_init(myBuf_t *b)
 * {
 * 	memset(b->slots, 0, sizeof(b->slots));
 * 	a3buf_init(&b->a3);
 * }
 *
 * bool myBuf_read(myBuf_t *b, thing_t *result)
 * {
 * 	bool ret = a3buf_rdFetch(&b->a3);
 * 	*result = b->slots[a3buf_rdIdx(&b->a3)];
 * 	return ret;
 * }
 *
 * bool myBuf_publish(myBuf_t *b, const thing_t *thing)
 * {
 * 	b->slots[a3buf_wrIdx(&b->a3)] = *thing;
 * 	return a3buf_submit(&b->a3);
 * }
 */
typedef struct {
	/*
	 * Shared state - combines the index of a shared buffer and a dirty flag
	 */
	alignas(A3BUF_ALIGN) atomic_uint sh;

	/*
	 * Index of the current reader buffer, doesn't need to be atomic because
	 * it is only used by the reader
	 */
	alignas(A3BUF_ALIGN) unsigned int rd;

	/*
	 * Index of the current writer buffer, doesn't need to be atomic because
	 * it is only used by the writer
	 */
	alignas(A3BUF_ALIGN) unsigned int wr;
} a3buf_t;


/*
 * Initialize the a3buf structure
 * Must be called before reader and writer threads start performing other
 * operations on the structure
 * Must not be called once reader and writer threads start accessing the structure
 */
static inline void a3buf_init(a3buf_t *a3)
{
	atomic_init(&a3->sh, 0U);
	a3->rd = 1U;
	a3->wr = 2U;
}


/*
 * Checks whether a new buffer was published since the last reader fetch
 * This has two usecases:
 * - Readers might wish to skip some operations if there is no new data
 * - Some writers might not wish to overwrite unread data
 * Please do not ever use this for synchronization, if you ever find yourself
 * thinking about spinning on this condition, then what you want is a double
 * buffer, triple buffers are explicitly about unsynchronized data exchange
 */
static inline bool a3buf_dirty(const a3buf_t *a3)
{
	/* Can be relaxed because it is not used for synchronization */
	return ((atomic_load_explicit(&a3->sh, memory_order_relaxed) & A3BUF_DIRTY) != 0U);
}


/*
 * Refresh the reader buffer
 * Returns true if new buffer was fetched and false otherwise
 */
static inline bool a3buf_rdFetch(a3buf_t *a3)
{
	bool dirty = false;

	/* check if there is a new buffer to fetch */
	if (a3buf_dirty(a3)) {
		/*
		 * swap the reader buffer with the shared buffer, requires both
		 * acquire and release because all reads from the old buffer
		 * need to happen before the swap and all the reads from the
		 * new buffer need to happen after the swap
		 */
		a3->rd = atomic_exchange_explicit(&a3->sh, a3->rd, memory_order_acq_rel) & A3BUF_MASK;
		dirty = true;
	}

	return dirty;
}


/*
 * Get the current index of the reader buffer
 * Can only be called by the reader thread
 * Does not update the reader index
 */
static inline unsigned int a3buf_rdIdx(const a3buf_t *a3)
{
	return a3->rd;
}


/*
 * Returns the index of the current writer buffer - this buffer can be safely
 * written to and later submitted to the reader using a3buf_submit
 * Can only be called by the writer thread
 */
static inline unsigned int a3buf_wrIdx(const a3buf_t *a3)
{
	return a3->wr;
}


/*
 * Submit the current writer buffer, this makes it visible to the reader
 * Changes the current writer buffer, a3buf_wrIdx must be called to return the
 * new writer buffer index
 * Writers should not make assumptions about the contents of their new buffer
 * after they call this function
 * Can only be called by the writer thread
 * Returns true if the submission overwrote unread data and false otherwise
 */
static inline bool a3buf_submit(a3buf_t *a3)
{
	/*
	 * Exchange the writer buffer and the shared buffer, needs both acquire
	 * and release because all writes to the old buffer need to happen
	 * before the swap and all the writes to the new buffer need to happen
	 * after the swap
	 */
	unsigned int sh = atomic_exchange_explicit(&a3->sh, a3->wr | A3BUF_DIRTY, memory_order_acq_rel);
	a3->wr = sh & A3BUF_MASK;
	return ((sh & A3BUF_DIRTY) != 0U);
}


#endif
