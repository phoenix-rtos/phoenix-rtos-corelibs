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

	if (!(n = req->rsegs + req->wsegs))
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


void virtqueue_destroy(virtio_dev_t *vdev, virtqueue_t *vq)
{
	resourceDestroy(vq->lock);
	resourceDestroy(vq->dlock);
	resourceDestroy(vq->dcond);
	munmap(vq->mem, vq->memsz);
	free(vq->buffs);
}


int virtqueue_init(virtio_dev_t *vdev, virtqueue_t *vq, unsigned int idx, unsigned int size)
{
	unsigned int aoffs = size * sizeof(virtio_desc_t);
	unsigned int ueoffs = aoffs + sizeof(virtio_avail_t) + size * sizeof(uint16_t);
	unsigned int uoffs = (ueoffs + sizeof(uint16_t) + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1);
	unsigned int aeoffs = uoffs + sizeof(virtio_used_t) + size * sizeof(virtio_used_elem_t);
	unsigned int i, err, maxsz;
	uint64_t addr;

	/* Virtqueue index has to be a 2-byte value */
	if (idx > 0xffff)
		return -EINVAL;

	/* Virtqueue size has to be a 2-byte power of 2 */
	if (!size || (size > 0xffff) || ((size - 1) & size))
		return -EINVAL;

	/* Select virtqueue slot and get max virtqueue size */
	switch (vdev->type) {
	case VIRTIO_PCI:
		if (virtio_legacy(vdev)) {
			virtio_write16(vdev, vdev->base, 0x0e, idx);

			if (!(maxsz = virtio_read16(vdev, vdev->base, 0x0c)) || virtio_read32(vdev, vdev->base, 0x08))
				return -ENOENT;

			/* Legacy VirtIO PCI devices can't control virtqueue size */
			size = maxsz;
		}
		else {
			/* Check if virtqueue index is valid */
			if (idx >= virtio_read16(vdev, vdev->base, 0x12))
				return -ENOENT;

			virtio_write16(vdev, vdev->base, 0x16, idx);

			if (!(maxsz = virtio_read16(vdev, vdev->base, 0x18)) || virtio_read16(vdev, vdev->base, 0x1c))
				return -ENOENT;
		}
		break;

	case VIRTIO_MMIO:
		virtio_write32(vdev, vdev->base, 0x30, idx);

		if (!(maxsz = virtio_read32(vdev, vdev->base, 0x34)) || virtio_read32(vdev, vdev->base, virtio_legacy(vdev) ? 0x40 : 0x44))
			return -ENOENT;
		break;
	}

	/* Initialize virtqueue memory */
	if (size > maxsz)
		size = maxsz;

	if ((vq->buffs = malloc(size * sizeof(void *))) == NULL)
		return -ENOMEM;

	vq->memsz = (aeoffs + sizeof(uint16_t) + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1);

	/* TODO: allocate contiguous physical memory below 4GB (legacy interface requires 32-bit physical address) */
	if ((vq->mem = mmap(NULL, vq->memsz, PROT_READ | PROT_WRITE, MAP_UNCACHED | MAP_ANONYMOUS, OID_NULL, 0)) == MAP_FAILED) {
		free(vq->buffs);
		return -ENOMEM;
	}

	if ((err = condCreate(&vq->dcond)) < 0) {
		munmap(vq->mem, vq->memsz);
		free(vq->buffs);
		return err;
	}

	if ((err = mutexCreate(&vq->dlock)) < 0) {
		resourceDestroy(vq->dcond);
		munmap(vq->mem, vq->memsz);
		free(vq->buffs);
		return err;
	}

	if ((err = mutexCreate(&vq->lock)) < 0) {
		resourceDestroy(vq->dlock);
		resourceDestroy(vq->dcond);
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

	/* Activate virtqueue */
	switch (vdev->type) {
	case VIRTIO_PCI:
		if (virtio_legacy(vdev)) {
			/* Legacy interface requires 32-bit descriptors page address */
			if ((addr = va2pa((void *)vq->desc) / _PAGE_SIZE) >> 32) {
				virtqueue_destroy(vdev, vq);
				return -EFAULT;
			}

			virtio_write32(vdev, vdev->base, 0x08, addr);
		}
		else {
			virtio_write16(vdev, vdev->base, 0x18, size);

			virtio_write64(vdev, vdev->base, 0x20, va2pa((void *)vq->desc));
			virtio_write64(vdev, vdev->base, 0x28, va2pa((void *)vq->avail));
			virtio_write64(vdev, vdev->base, 0x30, va2pa((void *)vq->used));
		}
		break;

	case VIRTIO_MMIO:
		virtio_write32(vdev, vdev->base, 0x38, size);

		if (virtio_legacy(vdev)) {
			/* Legacy interface requires 32-bit descriptors page address */
			if ((addr = va2pa((void *)vq->desc) / _PAGE_SIZE) >> 32) {
				virtqueue_destroy(vdev, vq);
				return -EFAULT;
			}

			virtio_write32(vdev, vdev->base, 0x3c, _PAGE_SIZE);
			virtio_write32(vdev, vdev->base, 0x40, addr);
		}
		else {
			addr = va2pa((void *)vq->desc);
			virtio_write32(vdev, vdev->base, 0x80, addr);
			virtio_write32(vdev, vdev->base, 0x84, addr >> 32);

			addr = va2pa((void *)vq->avail);
			virtio_write32(vdev, vdev->base, 0x90, addr);
			virtio_write32(vdev, vdev->base, 0x94, addr >> 32);

			addr = va2pa((void *)vq->used);
			virtio_write32(vdev, vdev->base, 0xa0, addr);
			virtio_write32(vdev, vdev->base, 0xa4, addr >> 32);

			virtio_write32(vdev, vdev->base, 0x44, 1);
		}
		break;
	}

	return EOK;
}
