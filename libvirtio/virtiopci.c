/*
 * Phoenix-RTOS
 *
 * VirtIO PCI common interface
 *
 * Copyright 2020 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stddef.h>

#include "virtiopci.h"


virtiopci_cap_t *virtiopci_getCap(virtiopci_cap_t *caps, unsigned char type)
{
	virtiopci_cap_t *cap = caps;

	do {
		if ((cap->id == 0x09) && (cap->type == type))
			return cap;
	} while ((cap = (virtiopci_cap_t *)((uintptr_t)caps + cap->next)) != caps);

	return NULL;
}
