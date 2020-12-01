/*
 * Phoenix-RTOS
 *
 * VirtIO device platform dependent initialization (IA32)
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

#include <phoenix/arch/ia32.h>
#include <sys/list.h>
#include <sys/mman.h>
#include <sys/platform.h>

#include "virtio.h"


/* Destroys BAR */
static void virtiopci_destroyBAR(void *bar)
{
	uintptr_t addr = (uintptr_t)bar;

	if (!(addr & 0x1))
		munmap((void *)(addr & ~(_PAGE_SIZE - 1)), _PAGE_SIZE);
}


/* Initializes BAR */
static void *virtiopci_initBAR(pci_dev_t *pdev, unsigned char idx)
{
	unsigned long base = pdev->resources[idx].base;
	unsigned long len = pdev->resources[idx].limit;
	unsigned char flags = pdev->resources[idx].flags;
	void *bar;

	if (!base || !len)
		return NULL;

	/* Check for IO space BAR */
	if (flags & 0x1)
		return (void *)(base | 0x1);

	/* Check for not supported 64-bit memory space BAR (used upper 32 bits or base + len overflow) */
	if ((flags & 0x4) && ((base > base + len) || (idx >= 5) || pdev->resources[idx + 1].base))
		return NULL;

	if ((bar = mmap(NULL, _PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_DEVICE | MAP_UNCACHED, OID_PHYSMEM, base)) == MAP_FAILED)
		return NULL;

	return bar;
}


/* Initializes and returns capability BAR */
static void *virtiopci_initCap(pci_dev_t *pdev, virtiopci_cap_t *caps, unsigned char type, unsigned int capsz)
{
	virtiopci_cap_t *cap;
	void *bar;

	if (((cap = virtiopci_getCap(caps, type)) == NULL) || (cap->size < capsz) || ((bar = virtio_initBAR(pdev, cap->bar)) == NULL))
		return NULL;

	return (void *)((uintptr_t)bar + cap->offs);
}


/* Performs VirtIO PCI device generic initialization */
static int virtiopci_initone(pci_dev_t *pdev, virtiopci_cap_t *caps, virtio_dev_t *vdev)
{
	vdev->type = vdevPCI;
	vdev->features = (pdev->revision) ? (1ULL << 32) : 0;
	vdev->irq = pdev->irq;
	vdev->prev = NULL;
	vdev->next = NULL;

	if (virtio_legacy(vdev)) {
		if ((vdev->base = virtiopci_initBAR(pdev, 0)) == NULL)
			return -EFAULT;

		vdev->ntf = (void *)((uintptr_t)vdev->base + 0x10);
		vdev->isr = (void *)((uintptr_t)vdev->base + 0x13);
		vdev->cfg = (void *)((uintptr_t)vdev->base + 0x14);

		return EOK;
	}

	if ((vdev->base = virtiopci_initCap(pdev, caps, 0x01, 0x38)) == NULL)
		return -EFAULT;

	if ((vdev->ntf = virtiopci_initCap(pdev, caps, 0x02, 0x14)) == NULL) {
		virtiopci_destroyBAR(vdev->base);
		return -EFAULT;
	}

	if ((vdev->isr = virtiopci_initCap(pdev, caps, 0x03, 0x01)) == NULL) {
		virtiopci_destroyBAR(vdev->ntf);
		virtiopci_destroyBAR(vdev->base);
		return -EFAULT;
	}

	if ((vdev->cfg = virtio_initCap(pdev, caps, 0x04, 0x00)) == NULL) {
		virtiopci_destroyBAR(vdev->isr);
		virtiopci_destroyBAR(vdev->ntf);
		virtiopci_destroyBAR(vdev->base);
		return -EFAULT;
	}

	return EOK;
}


static int virtiopci_init(unsigned int id, unsigned int vdevsz, int (*init)(void *), void **vdevs)
{
	platformctl_t pctl = { .action = pctl_get, .type = pctl_pci };
	unsigned char caps[192];
	int err = EOK, nvdevs = 0;
	virtio_dev_t *vdev;

	pctl.pci.id.vendor = 0x1af4;
	pctl.pci.id.device = id;
	pctl.pci.id.subvendor = PCI_ANY;
	pctl.pci.id.subdevice = PCI_ANY;
	pctl.pci.id.cl = PCI_ANY;
	pctl.pci.dev.bus = 0;
	pctl.pci.dev.dev = 0;
	pctl.pci.dev.func = 0;
	pctl.pci.caps = caps;

	while (!platformctl(&pctl)) {
		if ((vdev = malloc(vdevsz)) == NULL) {
			err = -ENOMEM;
			break;
		}

		if ((err = virtiopci_initone(&pctl.pci.dev, (virtiopci_cap_t *)caps, vdev)) < 0) {
			free(vdev);
			break;
		}

		virtio_reset(vdev);
		virtio_writeStatus(vdev, virtio_readStatus(vdev) | 0x03);
		vdev->features = virtio_getFeatures(vdev);

		if ((init != NULL) && (err = init(vdev)) < 0) {
			virtio_writeStatus(vdev, virtio_readStatus(vdev) | 0x80);
			virtio_destroy(vdev);
			free(vdev);
			pctl.pci.dev.func++;
			continue;
		}

		LIST_ADD(vdevs, vdev);
		nvdevs++;
		pctl.pci.dev.func++;
	}

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


static unsigned int virtiopci_legacyID(unsigned int id)
{
	switch (id) {
	/* Network card */
	case 0x01:
		return 0x1000;

	/* Block device */
	case 0x02:
		return 0x1001;

	/* Console */
	case 0x03:
		return 0x1003;

	/* Entropy source */
	case 0x04:
		return 0x1005;

	/* Memory ballooning (traditional) */
	case 0x05:
		return 0x1002;

	/* SCSI host */
	case 0x08:
		return 0x1004;

	/* 9P transport */
	case 0x09:
		return 0x1009;
	}

	return 0;
}


int virtio_init(unsigned int id, unsigned int vdevsz, int (*init)(void *), void **vdevs)
{
	int ret, nvdevs = 0;

	if ((vdevsz < sizeof(virtio_dev_t)) || (vdevs == NULL))
		return -EINVAL;

	*vdevs = NULL;

	/* Initialize modern VirtIO PCI devices */
	if ((ret = virtiopci_init(0x1040 + id, vdevsz, init, vdevs)) < 0)
		return ret;
	nvdevs += ret;

	if (!(id = virtiopci_legacyID(id)))
		return nvdevs;

	/* Initialize legacy VirtIO PCI devices */
	if ((ret = virtiopci_init(id, vdevsz, init, vdevs)) < 0)
		return ret;
	nvdevs += ret;

	return nvdevs;
}
