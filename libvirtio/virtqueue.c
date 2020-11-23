/*
 * Phoenix-RTOS
 *
 * Virtqueue interface
 *
 * Copyright 2020 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/list.h>
#include <sys/mman.h>
#include <sys/threads.h>

#include "libvirtio.h"


static inline uint8_t virtqueue_read8(virtio_dev_t *vdev, volatile void *addr)
{
	return *(volatile uint8_t *)addr;
}


static inline uint16_t virtqueue_read16(virtio_dev_t *vdev, volatile void *addr)
{
	return virtio_vtog16(vdev, *(volatile uint16_t *)addr);
}


static inline uint32_t virtqueue_read32(virtio_dev_t *vdev, volatile void *addr)
{
	return virtio_vtog32(vdev, *(volatile uint32_t *)addr);
}


static inline uint64_t virtqueue_read64(virtio_dev_t *vdev, volatile void *addr)
{
	return virtio_vtog64(vdev, *(volatile uint64_t *)addr);
}


static inline void virtqueue_write8(virtio_dev_t *vdev, volatile void *addr, uint8_t val)
{
	*(volatile uint8_t *)addr = val;
}


static inline void virtqueue_write16(virtio_dev_t *vdev, volatile void *addr, uint16_t val)
{
	*(volatile uint16_t *)addr = virtio_gtov16(vdev, val);
}


static inline void virtqueue_write32(virtio_dev_t *vdev, volatile void *addr, uint32_t val)
{
	*(volatile uint32_t *)addr = virtio_gtov32(vdev, val);
}


static inline void virtqueue_write64(virtio_dev_t *vdev, volatile void *addr, uint64_t val)
{
	*(volatile uint64_t *)addr = virtio_gtov32(vdev, val);
}


virtqueue_t *virtqueue_alloc(unsigned int idx, unsigned int size)
{
	unsigned int aoffs = size * sizeof(virtio_desc_t);
	unsigned int ueoffs = aoffs + sizeof(virtio_avail_t) + size * sizeof(uint16_t);
	unsigned int uoffs = (ueoffs + sizeof(uint16_t) + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1);
	unsigned int aeoffs = uoffs + sizeof(virtio_used_t) + size * sizeof(virtio_used_elem_t);
	unsigned int i;
	virtqueue_t *vq;

	/* Virtqueue index has to be a 2-byte value */
	if (idx > 0xffff)
		return NULL;

	/* Virtqueue size has to a be 2-byte power of 2 */
	if (!size || (size > 0xffff) || ((size - 1) & size))
		return NULL;

	if ((vq = malloc(sizeof(virtqueue_t))) == NULL)
		return NULL;

	if ((vq->buffs = malloc(size * sizeof(void *))) == NULL) {
		free(vq);
		return NULL;
	}

	vq->memsz = (aeoffs + sizeof(uint16_t) + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1);

	/* TODO: allocate contiguous physical memory below 4GB (legacy interface requires 32-bit physical address) */
	if ((vq->mem = mmap(NULL, vq->memsz, PROT_READ | PROT_WRITE, MAP_UNCACHED | MAP_ANONYMOUS, OID_NULL, 0)) == MAP_FAILED) {
		free(vq->buffs);
		free(vq);
		return NULL;
	}

	if (condCreate(&vq->dcond) < 0) {
		munmap(vq->mem, vq->memsz);
		free(vq->buffs);
		free(vq);
		return NULL;
	}

	if (mutexCreate(&vq->dlock) < 0) {
		resourceDestroy(vq->dcond);
		munmap(vq->mem, vq->memsz);
		free(vq->buffs);
		free(vq);
		return NULL;
	}

	if (mutexCreate(&vq->lock) < 0) {
		resourceDestroy(vq->dlock);
		resourceDestroy(vq->dcond);
		munmap(vq->mem, vq->memsz);
		free(vq->buffs);
		free(vq);
		return NULL;
	}

	memset(vq->mem, 0, vq->memsz);
	vq->desc = (virtio_desc_t *)vq->mem;
	vq->avail = (virtio_avail_t *)((uintptr_t)vq->mem + aoffs);
	vq->uevent = (uint16_t *)((uintptr_t)vq->mem + ueoffs);
	vq->used = (virtio_used_t *)((uintptr_t)vq->mem + uoffs);
	vq->aevent = (uint16_t *)((uintptr_t)vq->mem + aeoffs);

	vq->idx = idx;
	vq->size = size;
	vq->nfree = size;
	vq->free = 0;
	vq->last = 0;

	for (i = 0; i < size; i++) {
		vq->desc[i].next = i + 1;
		vq->buffs[i] = NULL;
	}

	return vq;
}


void virtqueue_free(virtqueue_t *vq)
{
	resourceDestroy(vq->lock);
	resourceDestroy(vq->dlock);
	resourceDestroy(vq->dcond);
	munmap(vq->mem, vq->memsz);
	free(vq->buffs);
	free(vq);
}


void virtqueue_enableirq(virtio_dev_t *vdev, virtqueue_t *vq)
{
	virtqueue_write16(vdev, &vq->avail->flags, virtqueue_read16(vdev, &vq->avail->flags) & ~0x1);
	virtio_mb();
}


void virtqueue_disableirq(virtio_dev_t *vdev, virtqueue_t *vq)
{
	virtqueue_write16(vdev, &vq->avail->flags, virtqueue_read16(vdev, &vq->avail->flags) | 0x1);
}


int virtqueue_enqueue(virtio_dev_t *vdev, virtqueue_t *vq, virtio_req_t *req)
{
	volatile virtio_desc_t *desc;
	virtio_seg_t *seg;
	unsigned int i, n;
	uint16_t id, idx;

	if ((req == NULL) || (req->segs == NULL))
		return -EINVAL;

	if ((n = req->rsegs + req->wsegs) < 1)
		return -EINVAL;
	else if (n > vq->size)
		return -ENOSPC;

	mutexLock(vq->dlock);

	/* Wait for n free descriptors */
	while (vq->nfree < n)
		condWait(vq->dcond, vq->dlock, 0);

	mutexLock(vq->lock);

	/* Fill out request descriptors */
	id = vq->free;
	for (i = 0, seg = req->segs; i < n; i++) {
		desc = &vq->desc[vq->free];
		vq->buffs[vq->free] = seg->buff;
		virtqueue_write64(vdev, &desc->addr, va2pa(seg->buff));
		virtqueue_write32(vdev, &desc->len, seg->len);
		virtqueue_write16(vdev, &desc->flags, ((i < n - 1) * 0x1) | ((i >= req->rsegs) * 0x2));

		if (i < n - 1)
			virtqueue_write16(vdev, &desc->next, desc->next);

		vq->free = desc->next;
		seg = seg->next;
	}
	vq->nfree -= n;

	/* Insert request to avail ring */
	idx = virtqueue_read16(vdev, &vq->avail->idx);
	virtqueue_write16(vdev, &vq->avail->ring[idx & (vq->size - 1)], id);
	virtio_mb();

	/* Update avail index */
	virtqueue_write16(vdev, &vq->avail->idx, idx + 1);

	mutexUnlock(vq->lock);
	mutexUnlock(vq->dlock);

	return EOK;
}


void virtqueue_notify(virtio_dev_t *vdev, virtqueue_t *vq)
{
	/* Ensure avail index is visible to device */
	virtio_mb();

	if (!virtqueue_read16(vdev, &vq->avail->flags))
		virtio_notify(vdev, vq);
}


void *virtqueue_dequeue(virtio_dev_t *vdev, virtqueue_t *vq, unsigned int *len)
{
	virtio_used_elem_t *used;
	uint16_t idx, next;
	void *buff;

	mutexLock(vq->lock);

	if (vq->last == virtqueue_read16(vdev, &vq->used->idx)) {
		mutexUnlock(vq->lock);
		return NULL;
	}

	/* Get processed request */
	used = &vq->used->ring[vq->last++ & (vq->size - 1)];
	virtio_mb();

	/* Get processed request descriptor chain ID and its head buffer */
	next = virtqueue_read32(vdev, &used->id);
	buff = vq->buffs[next];

	/* Get number of bytes written to request buffers */
	if (len != NULL)
		*len = virtqueue_read32(vdev, &used->len);

	/* Free descriptors */
	do {
		idx = next;
		next = virtqueue_read16(vdev, &vq->desc[idx].next);
		vq->desc[idx].next = vq->free;
		vq->buffs[idx] = NULL;
		vq->free = idx;
		vq->nfree++;
	} while (virtqueue_read16(vdev, &vq->desc[idx].flags) & 0x1);

	/* Signal free descriptors */
	condSignal(vq->dcond);

	mutexUnlock(vq->lock);

	return buff;
}
