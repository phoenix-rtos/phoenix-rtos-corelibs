/*
 * Phoenix-RTOS
 *
 * VirtIO device platform dependent initialization (RISCV64)
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

#include <sys/list.h>
#include <sys/mman.h>

#include "virtio.h"


static int virtiommio_initone(void *base, unsigned int irq, virtio_dev_t *vdev)
{
	vdev->type = vdevMMIO;
	vdev->features = 0;
	vdev->irq = irq;
	vdev->prev = NULL;
	vdev->next = NULL;

	if ((vdev->base = mmap(NULL, _PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_DEVICE | MAP_UNCACHED, OID_PHYSMEM, base)) == MAP_FAILED)
		return -ENOMEM;

	vdev->ntf = (void *)((uintptr_t)vdev->base + 0x50);
	vdev->isr = (void *)((uintptr_t)vdev->base + 0x60);
	vdev->cfg = (void *)((uintptr_t)vdev->base + 0x100);

	if (virtio_read32(vdev, vdev->base, 0x00) != 0x74726976) {
		vdev->features = (1ULL << 32);
		if (virtio_read32(vdev, vdev->base, 0x00) != 0x74726976)
			return -ENXIO;
	}

	switch (virtio_read32(vdev, vdev->base, 0x04)) {
	case 1:
		vdev->features = 0;
		break;

	case 2:
		vdev->features = (1ULL << 32);
		break;

	default:
		return -ENOTSUP;
	}

	if (!virtio_read32(vdev, vdev->base, 0x08))
		return -ENXIO;

	return EOK;
}


static int virtiommio_init(unsigned int id, unsigned int vdevsz, int (*init)(void *), void **vdevs)
{
	int err = EOK, nvdevs = 0;
	virtio_dev_t *vdev;

	/* TODO: parse DTB for VirtIO devices */
	do {
		if (id != 0x02)
			return 0;

		if ((vdev = malloc(vdevsz)) == NULL) {
			err = -ENOMEM;
			break;
		}

		if ((err = virtiommio_initone((void *)0x10008000, 8, vdev)) < 0) {
			free(vdev);
			break;
		}

		virtio_reset(vdev);
		virtio_writeStatus(vdev, virtio_readStatus(vdev) | 0x03);
		vdev->features = virtio_getFeatures(vdev);

		if (virtio_legacy(vdev)) {
			if (virtio_read32(vdev, vdev->base, 0x04) == 2) {
				virtio_writeStatus(vdev, virtio_readStatus(vdev) | 0x80);
				free(vdev);
				break;
			}
			virtio_write32(vdev, vdev->base, 0x28, _PAGE_SIZE);
		}

		if ((init != NULL) && (err = init(vdev)) < 0) {
			virtio_writeStatus(vdev, virtio_readStatus(vdev) | 0x80);
			free(vdev);
			break;
		}

		LIST_ADD(vdevs, vdev);
		nvdevs++;
	} while (0);

	if (err < 0) {
		while ((vdev = *vdevs) != NULL) {
			LIST_REMOVE(vdevs, vdev);
			virtio_destroy(vdev);
			free(vdev);
		}

		return err;
	}

	return nvdevs;
}


int virtio_init(unsigned int id, unsigned int vdevsz, int (*init)(void *), void **vdevs)
{
	int ret, nvdevs = 0;

	if ((vdevsz < sizeof(virtio_dev_t)) || (vdevs == NULL))
		return -EINVAL;

	*vdevs = NULL;

	/* Initialize VirtIO MMIO devices */
	if ((ret = virtiommio_init(id, vdevsz, init, vdevs)) < 0)
		return ret;
	nvdevs += ret;

	return nvdevs;
}
