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

#include "virtio.h"
#include "virtiolow.h"


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


/* Selects virtqueue with given index */
static void virtqueue_select(virtio_dev_t *vdev, unsigned int idx)
{
	if (vdev->info.type == vdevPCI) {
		if (virtio_legacy(vdev)) {
			virtio_write16(vdev, vdev->info.base.addr, 0x0e, idx);
			virtio_mb();
			return;
		}

		virtio_write16(vdev, vdev->info.base.addr, 0x16, idx);
		virtio_mb();
		return;
	}

	virtio_write32(vdev, vdev->info.base.addr, 0x30, idx);
	virtio_mb();
}


/* Checks if virtqueue is available and validates its size */
static int virtqueue_enable(virtio_dev_t *vdev, unsigned int *size)
{
	unsigned int maxsz;

	if (vdev->info.type == vdevPCI) {
		if (virtio_legacy(vdev)) {
			if (!(maxsz = virtio_read16(vdev, vdev->info.base.addr, 0x0c)) || virtio_read32(vdev, vdev->info.base.addr, 0x08))
				return -ENOENT;

			*size = maxsz;
			return EOK;
		}

		if (!(maxsz = virtio_read16(vdev, vdev->info.base.addr, 0x18)) || virtio_read16(vdev, vdev->info.base.addr, 0x1c))
			return -ENOENT;

		if (*size > maxsz)
			*size = maxsz;

		return EOK;
	}

	if (!(maxsz = virtio_read32(vdev, vdev->info.base.addr, 0x34)) || virtio_read32(vdev, vdev->info.base.addr, virtio_legacy(vdev) ? 0x40 : 0x44))
		return -ENOENT;

	if (*size > maxsz)
		*size = maxsz;

	return EOK;
}


/* Activates virtqueue */
static int virtqueue_activate(virtio_dev_t *vdev, virtqueue_t *vq)
{
	uint64_t addr;

	if (vdev->info.type == vdevPCI) {
		if (virtio_legacy(vdev)) {
			if ((addr = va2pa((void *)vq->desc) / _PAGE_SIZE) >> 32)
				return -EFAULT;

			virtio_write32(vdev, vdev->info.base.addr, 0x08, addr);
			return EOK;
		}

		vq->noffs = virtio_read16(vdev, vdev->info.base.addr, 0x1e);
		virtio_write16(vdev, vdev->info.base.addr, 0x18, vq->size);
		virtio_write64(vdev, vdev->info.base.addr, 0x20, va2pa((void *)vq->desc));
		virtio_write64(vdev, vdev->info.base.addr, 0x28, va2pa((void *)vq->avail));
		virtio_write64(vdev, vdev->info.base.addr, 0x30, va2pa((void *)vq->used));

		virtio_mb();
		virtio_write16(vdev, vdev->info.base.addr, 0x1c, 1);
		return EOK;
	}

	virtio_write32(vdev, vdev->info.base.addr, 0x38, vq->size);

	if (virtio_legacy(vdev)) {
		if ((addr = va2pa((void *)vq->desc) / _PAGE_SIZE) >> 32)
			return -EFAULT;

		virtio_write32(vdev, vdev->info.base.addr, 0x3c, _PAGE_SIZE);
		virtio_write32(vdev, vdev->info.base.addr, 0x40, addr);
		return EOK;
	}

	addr = va2pa((void *)vq->desc);
	virtio_write32(vdev, vdev->info.base.addr, 0x80, addr);
	virtio_write32(vdev, vdev->info.base.addr, 0x84, addr >> 32);

	addr = va2pa((void *)vq->avail);
	virtio_write32(vdev, vdev->info.base.addr, 0x90, addr);
	virtio_write32(vdev, vdev->info.base.addr, 0x94, addr >> 32);

	addr = va2pa((void *)vq->used);
	virtio_write32(vdev, vdev->info.base.addr, 0xa0, addr);
	virtio_write32(vdev, vdev->info.base.addr, 0xa4, addr >> 32);

	virtio_mb();
	virtio_write32(vdev, vdev->info.base.addr, 0x44, 1);
	return EOK;
}


void virtqueue_enableIRQ(virtio_dev_t *vdev, virtqueue_t *vq)
{
	virtqueue_write16(vdev, &vq->avail->flags, virtqueue_read16(vdev, &vq->avail->flags) & ~0x1);
	virtio_mb();
}


void virtqueue_disableIRQ(virtio_dev_t *vdev, virtqueue_t *vq)
{
	virtqueue_write16(vdev, &vq->avail->flags, virtqueue_read16(vdev, &vq->avail->flags) | 0x1);
}


int virtqueue_enqueue(virtio_dev_t *vdev, virtqueue_t *vq, virtio_req_t *req)
{
	volatile virtio_desc_t *desc;
	virtio_seg_t *seg;
	unsigned int i, n;
	uint16_t id, idx;

	if (!(n = req->rsegs + req->wsegs))
		return -EINVAL;
	else if (n > vq->size)
		return -ENOSPC;

	mutexLock(vq->lock);

	/* Wait for n free descriptors */
	while (vq->nfree < n)
		condWait(vq->cond, vq->lock, 0);

	/* Fill out request descriptors */
	id = vq->free;
	seg = req->segs;
	for (i = 0; i < n; i++) {
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

	return EOK;
}


void virtqueue_notify(virtio_dev_t *vdev, virtqueue_t *vq)
{
	/* Ensure avail index is visible to device */
	virtio_mb();

	if (virtqueue_read16(vdev, &vq->used->flags) & 0x01)
		return;

	if (vdev->info.type == vdevPCI) {
		if (virtio_legacy(vdev)) {
			virtio_write16(vdev, vdev->info.base.addr, 0x10, vq->idx);
			return;
		}

		virtio_write16(vdev, vdev->info.ntf.addr, vq->noffs * vdev->info.xntf, vq->idx);
		return;
	}

	virtio_write32(vdev, vdev->info.base.addr, 0x50, vq->idx);
}


void *virtqueue_dequeue(virtio_dev_t *vdev, virtqueue_t *vq, unsigned int *len)
{
	volatile virtio_used_elem_t *used;
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
	condSignal(vq->cond);

	mutexUnlock(vq->lock);

	return buff;
}


void virtqueue_destroy(virtio_dev_t *vdev, virtqueue_t *vq)
{
	resourceDestroy(vq->lock);
	resourceDestroy(vq->cond);
	munmap(vq->mem, vq->memsz);
	free(vq->buffs);
}


int virtqueue_init(virtio_dev_t *vdev, virtqueue_t *vq, unsigned int idx, unsigned int size)
{
	unsigned int i, aoffs, ueoffs, uoffs, aeoffs;
	int err;

	/* Virtqueue index has to be a 2-byte value */
	if (idx > 0xffff)
		return -EINVAL;

	/* Virtqueue size has to be a 2-byte power of 2 */
	if (!size || (size > 0xffff) || ((size - 1) & size))
		return -EINVAL;

	/* Select virtqueue and negotiate its size */
	virtqueue_select(vdev, idx);
	if ((err = virtqueue_enable(vdev, &size)) < 0)
		return err;

	/* Calculate offsets */
	aoffs = size * sizeof(virtio_desc_t);
	ueoffs = aoffs + sizeof(virtio_avail_t) + size * sizeof(uint16_t);
	uoffs = (ueoffs + sizeof(uint16_t) + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1);
	aeoffs = uoffs + sizeof(virtio_used_t) + size * sizeof(virtio_used_elem_t);

	/* Initialize virtqueue memory */
	if ((vq->buffs = malloc(size * sizeof(void *))) == NULL)
		return -ENOMEM;

	vq->memsz = (aeoffs + sizeof(uint16_t) + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1);

	/* TODO: allocate physcial memory below 4GB (legacy interface requires 32-bit physical address) */
	if ((vq->mem = mmap(NULL, vq->memsz, PROT_READ | PROT_WRITE, MAP_UNCACHED | MAP_ANONYMOUS | MAP_CONTIGUOUS, -1, 0)) == MAP_FAILED) {
		free(vq->buffs);
		return -ENOMEM;
	}

	if ((err = mutexCreate(&vq->lock)) < 0) {
		munmap(vq->mem, vq->memsz);
		free(vq->buffs);
		return err;
	}

	if ((err = condCreate(&vq->cond)) < 0) {
		resourceDestroy(vq->lock);
		munmap(vq->mem, vq->memsz);
		free(vq->buffs);
		return err;
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

	if ((err = virtqueue_activate(vdev, vq)) < 0) {
		virtqueue_destroy(vdev, vq);
		return err;
	}

	return EOK;
}
