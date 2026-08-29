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
 *
 * For non-overwriting API effective capacity is (size - 1) elements.
 * One slot is always left unused to avoid empty & full states ambiguity.
 *
 * For overwriting API effective capacity is size elements. When full,
 * pushes discard the oldest element to make space.
 *
 * Mixing non-overwriting and overwriting calls is undefined and not
 * supported. Use one API per FIFO instance.
 */
struct lf_fifo_s {
	/* common cache line */
	unsigned int size __attribute__((aligned(LF_FIFO_CACHELINE)));
	unsigned int mask; /* size - 1 */
	uint8_t *data;

	/* producer's cache line */
	atomic_uint head __attribute__((aligned(LF_FIFO_CACHELINE)));
	/*
	 * Overwriting API only: slots the producer has announced it is about to
	 * write. Always >= head. Producer writes it, consumer reads it.
	 */
	atomic_uint claim;

	/* consumer's cache line */
	atomic_uint tail __attribute__((aligned(LF_FIFO_CACHELINE)));
	/*
	 * Overwriting API only: elements that the consumer found to be overwritten
	 * and have not yet been reported. Overruns are added to the lost count on
	 * each pop. Used by the consumer only.
	 */
	unsigned int lost;
	/*
	 * Overwriting API only: total number of elements dropped by push and
	 * observed by the consumer. Used by the consumer only.
	 */
	unsigned int droppedSeen;
	/*
	 * Overwriting API only: total number of elements dropped outright by push.
	 * Only the producer writes it and never resets it. The consumer subtracts
	 * the number it has already seen. It is placed on the consumer's cache line
	 * because the consumer reads it frequently, while the producer updates it
	 * only on a rare path.
	 */
	atomic_uint dropped;
};


/* --------------------- Common API --------------------- */

static inline void lf_fifo_init(lf_fifo_t *f, uint8_t *data, unsigned int size)
{
	assert(size >= 2u && (size & (size - 1u)) == 0u);

	atomic_init(&f->head, 0u);
	atomic_init(&f->claim, 0u);
	atomic_init(&f->tail, 0u);
	atomic_init(&f->dropped, 0u);

	f->lost = 0u;
	f->droppedSeen = 0u;

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

/*
 * A push never fails: when the FIFO is full it discards the OLDEST elements to
 * make room. The producer therefore never blocks and never reads tail.
 *
 * What the consumer receives is a faithful subsequence of what was pushed - in
 * order, never duplicated, never torn. Only whole elements are ever missing.
 *
 * `head` and `tail` are free-running unmasked counters, so index `i` lives in slot
 * `i & mask` and indices `i` and `i + size` share a slot. A read of index `tail` is
 * only valid while the producer has not begun writing `tail + size`, and `head`
 * cannot establish that: the slot is written before `head` is published, so a
 * clobbered element can still look safe. The producer instead announces a slot
 * in `claim` before touching it, and the consumer validates against `claim`
 * after reading. If the producer had claimed the slot, the element is discarded
 * and the read retried further from the producer, doubling that distance each
 * time - which also bounds the retry, since the consumer gives up at most the
 * whole buffer.
 *
 * Loss is reported by lf_fifo_ow_lost() and covers both of its sources:
 * elements overwritten before the consumer reached them, and elements a push
 * longer than the FIFO never stored. Everything pushed is therefore either
 * delivered or counted, never both and never neither.
 *
 * Note that `data` is read and written without a lock, which the C11 model
 * calls a data race however it is fenced - the usual seqlock caveat. What the
 * code relies on is that the compiler neither moves a data access across
 * atomic_thread_fence() nor tears a byte-sized one.
 */

/*
 * Always succeeds. If full, overwrites oldest element.
 * Never reads tail, so it stays wait-free and off the consumer's cache line.
 * Overwritten elements are accounted for by lf_fifo_ow_lost().
 */
static inline void lf_fifo_ow_push(lf_fifo_t *f, uint8_t byte)
{
	unsigned int head = atomic_load_explicit(&f->head, memory_order_relaxed);

	/*
	 * Announce the slot before touching it. The release fence keeps this store
	 * ahead of the data write, so a consumer that sees the new byte is
	 * guaranteed to see the claim too and can reject what it read.
	 */
	atomic_store_explicit(&f->claim, head + 1u, memory_order_relaxed);
	atomic_thread_fence(memory_order_release);

	f->data[head & f->mask] = byte;

	/* publish new head so consumer can see data */
	atomic_store_explicit(&f->head, head + 1u, memory_order_release);
}


/*
 * Always succeeds. If full, overwrites oldest elements.
 * If `n` exceeds `f->size` only the newest `f->size` source elements are
 * stored. The older excess never enters the FIFO and is dropped at once,
 * accounted for by lf_fifo_ow_lost() along with overwritten elements.
 */
static inline void lf_fifo_ow_push_many(lf_fifo_t *f, const uint8_t *src, unsigned int n)
{
	if (n == 0u) {
		return;
	}

	if (n > f->size) {
		unsigned int m = n - f->size;
		unsigned int dropped = atomic_load_explicit(&f->dropped, memory_order_relaxed);

		atomic_store_explicit(&f->dropped, dropped + m, memory_order_relaxed);
		src += m;
		n = f->size;
	}

	unsigned int head = atomic_load_explicit(&f->head, memory_order_relaxed);

	/*
	 * Announce the whole sequence before touching it, see lf_fifo_ow_push().
	 * One claim covers all slots because the consumer only has to be told that
	 * its slot is in danger, not which element of the sequence reaches it.
	 */
	atomic_store_explicit(&f->claim, head + n, memory_order_relaxed);
	atomic_thread_fence(memory_order_release);

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
	unsigned int margin = 0u;

	for (;;) {
		unsigned int used = head - tail;
		unsigned int keep;
		uint8_t val;

		if (used == 0u) {
			/* empty, and tail has not moved */
			return 0u;
		}

		/*
		 * Everything below `head - size` has been overwritten, and everything
		 * below `head - keep` is additionally being given up to get clear of a
		 * producer that has already beaten us once. Either way it is gone.
		 */
		keep = f->size - margin;
		if (used > keep) {
			f->lost += used - keep;
			tail = head - keep;
			used = keep;

			if (used == 0u) {
				/* margin swallowed the buffer, publish the skip and give up */
				atomic_store_explicit(&f->tail, tail, memory_order_relaxed);
				return 0u;
			}
		}

		val = f->data[tail & f->mask];

		/*
		 * Index `tail` and index `tail + size` share a slot, so the read above
		 * is only valid if the producer has not claimed `tail + size` yet.
		 * The acquire fence keeps the data read ahead of the claim load. Checking
		 * head instead would be unsound: the slot is written before head is
		 * published, so a clobbered byte can still look safe.
		 */
		atomic_thread_fence(memory_order_acquire);
		if ((atomic_load_explicit(&f->claim, memory_order_relaxed) - tail) <= f->size) {
			*byte = val;
			atomic_store_explicit(&f->tail, tail + 1u, memory_order_relaxed);
			return 1u;
		}

		/*
		 * The producer took the slot while we were reading it. Drop what we read
		 * rather than hand over an element out of order, and retry further from
		 * the producer - one slot first, then doubling. A fixed step would keep
		 * losing the same race to a fast producer; doubling reaches a distance it
		 * cannot cross mid-read within a few turns, and bounds the loop at about
		 * log2(size) of them - at `margin == size` what is left of the buffer is
		 * given up and the FIFO reported empty.
		 */
		head = atomic_load_explicit(&f->head, memory_order_acquire);
		margin = (margin == 0u) ? 1u : (margin * 2u);
		if (margin > f->size) {
			margin = f->size;
		}
	}
}


/* Pop up to n bytes. Returns how many actually popped. */
static inline unsigned int lf_fifo_ow_pop_many(lf_fifo_t *f, uint8_t *dst, unsigned int n)
{
	if (n == 0u) {
		return 0u;
	}

	unsigned int tail = atomic_load_explicit(&f->tail, memory_order_relaxed);
	unsigned int head = atomic_load_explicit(&f->head, memory_order_acquire);
	unsigned int margin = 0u;

	for (;;) {
		unsigned int used = head - tail;
		unsigned int keep, cnt, m;

		if (used == 0u) {
			/* empty, and tail has not moved */
			return 0u;
		}

		/* overwritten, or given up for margin, see lf_fifo_ow_pop() */
		keep = f->size - margin;
		if (used > keep) {
			f->lost += used - keep;
			tail = head - keep;
			used = keep;

			if (used == 0u) {
				atomic_store_explicit(&f->tail, tail, memory_order_relaxed);
				return 0u;
			}
		}

		cnt = (n > used) ? used : n;

		/* contiguous to buffer end */
		m = f->size - (tail & f->mask);
		if (m > cnt) {
			m = cnt;
		}

		memcpy(dst, f->data + (tail & f->mask), m);
		if (cnt > m) {
			memcpy(dst + m, f->data, cnt - m);
		}

		/*
		 * `tail` is the oldest index copied and the producer writes in
		 * increasing index order, so it reaches `tail + size` before any later
		 * slot of the copy - validating tail covers the whole range, wrap
		 * included.
		 */
		atomic_thread_fence(memory_order_acquire);
		if ((atomic_load_explicit(&f->claim, memory_order_relaxed) - tail) <= f->size) {
			atomic_store_explicit(&f->tail, tail + cnt, memory_order_relaxed);
			return cnt;
		}

		/* drop the copy and retry further out, see lf_fifo_ow_pop() */
		head = atomic_load_explicit(&f->head, memory_order_acquire);
		margin = (margin == 0u) ? 1u : (margin * 2u);
		if (margin > f->size) {
			margin = f->size;
		}
	}
}


/*
 * Returns the number of elements lost since the last call and clears the
 * count, whether they were overwritten inside the FIFO or dropped by a push
 * longer than the FIFO itself.
 * Consumer-side only - use it from the same thread that pops elements.
 * Prefer using it once per drained batch rather than after every element.
 */
static inline unsigned int lf_fifo_ow_lost(lf_fifo_t *f)
{
	unsigned int lost = f->lost;
	unsigned int dropped = atomic_load_explicit(&f->dropped, memory_order_relaxed);

	f->lost = 0u;
	lost += dropped - f->droppedSeen;
	f->droppedSeen = dropped;

	return lost;
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
