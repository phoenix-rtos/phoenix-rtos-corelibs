/*
 * Phoenix-RTOS
 *
 * VirtIO-GPU driver
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/interrupt.h>
#include <sys/mman.h>
#include <sys/threads.h>
#include <sys/time.h>
#include <sys/types.h>

#include <libvirtio.h>

#include "graph.h"


/* Use polling on RISCV64 (interrupts trigger memory protection exception) */
#ifdef TARGET_RISCV64
#define USE_POLLING
#endif


typedef struct {
	uint32_t x;                     /* Horizontal coordinate */
	uint32_t y;                     /* Vertical coordinate */
	uint32_t w;                     /* Rectangle width */
	uint32_t h;                     /* Rectangle height */
} __attribute__((packed)) virtiogpu_rect_t;


typedef struct {
	struct {
		virtiogpu_rect_t r;         /* Display rectangle */
		uint32_t enabled;           /* Is enabled? */
		uint32_t flags;             /* Display flags */
	} pmodes[16];                   /* Displays info */
} __attribute__((packed)) virtiogpu_info_t;


typedef struct {
	/* Request buffers (accessible by device) */
	struct {
		/* Request header (device readable/writable) */
		struct {
			uint32_t type;          /* Request/Response type */
			uint32_t flags;         /* Request flags */
			uint64_t fence;         /* Request fence ID */
			uint32_t ctx;           /* Rendering context */
			uint32_t pad;           /* Padding */
		} hdr;

		/* Request data (device access depends on request type) */
		union {
			/* Displays info */
			volatile virtiogpu_info_t info;

			/* EDID info */
			volatile struct {
				uint32_t sid;       /* Scanout ID/data size */
				uint32_t pad;       /* Padding */
				uint8_t data[1024]; /* EDID data */
			} edid;

			/* Create resource */
			struct {
				uint32_t rid;       /* Resource ID */
				uint32_t fmt;       /* Resource format */
				uint32_t w;         /* Resource width */
				uint32_t h;         /* Resource height */
			} create;

			/* Destroy resource */
			struct {
				uint32_t rid;       /* Resource ID */
				uint32_t pad;       /* Padding */
			} destroy;

			/* Set scanout resource */
			struct {
				virtiogpu_rect_t r; /* Scanout rectangle */
				uint32_t sid;       /* Scanout ID */
				uint32_t rid;       /* Resource ID */
			} set;

			/* Flush scanout resource */
			struct {
				virtiogpu_rect_t r; /* Scanout rectangle */
				uint32_t rid;       /* Resource ID */
				uint32_t pad;       /* Padding */
			} flush;

			/* Transfer resource */
			struct {
				virtiogpu_rect_t r; /* Buffer rectangle */
				uint64_t offs;      /* Resource offset */
				uint32_t rid;       /* Resource ID */
				uint32_t pad;       /* Padding */
			} transfer;

			/* Attach resource buffers */
			struct {
				uint32_t rid;       /* Resource ID */
				uint32_t n;         /* Number of attached buffers */
				uint64_t addr;      /* Buffer address */
				uint32_t len;       /* Buffer length */
				uint32_t pad;       /* Padding */
			} attach;

			/* Detach resource buffers */
			struct {
				uint32_t rid;       /* Resource ID */
				uint32_t pad;       /* Padding */
			} detach;

			/* Update cursor */
			struct {
				struct {
					uint32_t sid;   /* Scanout ID */
					uint32_t x;     /* Horizontal coordinate */
					uint32_t y;     /* Vertical coordinate */
					uint32_t pad;   /* Padding */
				} pos;
				uint32_t rid;       /* Resource ID */
				uint32_t hx;        /* Hotspot horizontal coordinate */
				uint32_t hy;        /* Hotspot vertical coordinate */
				uint32_t pad;       /* Padding */
			} update;
		};
	} __attribute__((packed));

	/* VirtIO request segments */
	virtio_seg_t rseg;              /* Device readable segment */
	virtio_seg_t wseg;              /* Device writeable segment */
	virtio_req_t vreq;              /* VirtIO request */

	/* Custom helper fields */
	volatile int done;              /* Indicates request completion */
	handle_t lock;                  /* Request mutex */
	handle_t cond;                  /* Request condition variable */
} virtiogpu_req_t;


typedef struct {
	/* Device info */
	virtio_dev_t vdev;              /* VirtIO device */
	virtqueue_t ctlq;               /* Control virtqueue */
	virtqueue_t curq;               /* Cursor virtqueue */
	//virtiogpu_req_t *req;           /* Request context */
	unsigned int rbmp;              /* Resource bitmap */

	/* Display info */
	void *buff;                     /* Framebuffer */
	size_t size;                    /* Framebuffer size */
	unsigned int rid;               /* Framebuffer resource ID */

	/* Interrupt handling */
	volatile unsigned char done;    /* End interrupt thread? */
	volatile unsigned int isr;      /* Interrupt status */
	handle_t ilock;                 /* Interrupt mutex */
	handle_t icond;                 /* Interrupt condition variable */
	handle_t inth;                  /* Interrupt handle */
	char istack[2048] __attribute__((aligned(8)));
} virtiogpu_dev_t;


typedef struct {
	unsigned int mode;              /* Mode identifier */
	unsigned int width;             /* Screen width */
	unsigned int height;            /* Screen height */
	unsigned char depth;            /* Color depth */
} virtiogpu_mode_t;


typedef struct {
	unsigned int mode;              /* Refresh rate identifier */
	unsigned int freq;              /* Refresh rate (in us) */
} virtiogpu_freq_t;


/* Graphics modes table */
static const virtiogpu_mode_t modes[] = {
	{ GRAPH_640x480x32,   640,  480,  4 },
	{ GRAPH_720x480x32,   720,  480,  4 },
	{ GRAPH_720x576x32,   720,  576,  4 },
	{ GRAPH_800x600x32,   800,  600,  4 },
	{ GRAPH_832x624x32,   832,  624,  4 },
	{ GRAPH_896x672x32,   896,  672,  4 },
	{ GRAPH_928x696x32,   928,  696,  4 },
	{ GRAPH_960x540x32,   960,  540,  4 },
	{ GRAPH_960x600x32,   960,  600,  4 },
	{ GRAPH_960x720x32,   960,  720,  4 },
	{ GRAPH_1024x576x32,  1024, 576,  4 },
	{ GRAPH_1024x768x32,  1024, 768,  4 },
	{ GRAPH_1152x864x32,  1152, 864,  4 },
	{ GRAPH_1280x720x32,  1280, 720,  4 },
	{ GRAPH_1280x800x32,  1280, 800,  4 },
	{ GRAPH_1280x960x32,  1280, 960,  4 },
	{ GRAPH_1280x1024x32, 1280, 1024, 4 },
	{ GRAPH_1360x768x32,  1360, 768,  4 },
	{ GRAPH_1368x768x32,  1368, 768,  4 },
	{ GRAPH_1400x900x32,  1400, 900,  4 },
	{ GRAPH_1400x1050x32, 1400, 1050, 4 },
	{ GRAPH_1440x240x32,  1440, 240,  4 },
	{ GRAPH_1440x288x32,  1440, 288,  4 },
	{ GRAPH_1440x576x32,  1440, 576,  4 },
	{ GRAPH_1440x810x32,  1440, 810,  4 },
	{ GRAPH_1440x900x32,  1440, 900,  4 },
	{ GRAPH_1600x900x32,  1600, 900,  4 },
	{ GRAPH_1600x1024x32, 1600, 1024, 4 },
	{ GRAPH_1650x750x32,  1650, 750,  4 },
	{ GRAPH_1680x720x32,  1680, 720,  4 },
	{ GRAPH_1680x1050x32, 1680, 1050, 4 },
	{ GRAPH_1920x540x32,  1920, 540,  4 },
	{ GRAPH_1920x1080x32, 1920, 1080, 4 },
	{ GRAPH_NONE }
};


/* Refresh rates table */
static const virtiogpu_freq_t freqs[] = {
	{ GRAPH_56Hz,  17857 },
	{ GRAPH_60Hz,  16666 },
	{ GRAPH_70Hz,  14285 },
	{ GRAPH_72Hz,  13888 },
	{ GRAPH_75Hz,  13333 },
	{ GRAPH_80Hz,  12500 },
	{ GRAPH_87Hz,  11494 },
	{ GRAPH_90Hz,  11111 },
	{ GRAPH_120Hz, 8333 },
	{ GRAPH_144HZ, 6944 },
	{ GRAPH_165Hz, 6060 },
	{ GRAPH_240Hz, 4166 },
	{ GRAPH_300Hz, 3333 },
	{ GRAPH_360Hz, 2777 },
	{ GRAPH_NONE }
};


/* VirtIO GPU device descriptors */
static const virtio_devinfo_t info[] = {
	{ .type = vdevPCI, .id = 0x1050 },
#ifdef TARGET_RISCV64
	/* Direct VirtIO MMIO QEMU GPU device descriptors */
	{ .type = vdevMMIO, .id = 0x10, .irq = 8, .base = { (void *)0x10008000, 0x1000 } },
	{ .type = vdevMMIO, .id = 0x10, .irq = 7, .base = { (void *)0x10007000, 0x1000 } },
	{ .type = vdevMMIO, .id = 0x10, .irq = 6, .base = { (void *)0x10006000, 0x1000 } },
	{ .type = vdevMMIO, .id = 0x10, .irq = 5, .base = { (void *)0x10005000, 0x1000 } },
	{ .type = vdevMMIO, .id = 0x10, .irq = 4, .base = { (void *)0x10004000, 0x1000 } },
	{ .type = vdevMMIO, .id = 0x10, .irq = 3, .base = { (void *)0x10003000, 0x1000 } },
	{ .type = vdevMMIO, .id = 0x10, .irq = 2, .base = { (void *)0x10002000, 0x1000 } },
	{ .type = vdevMMIO, .id = 0x10, .irq = 1, .base = { (void *)0x10001000, 0x1000 } },
#endif
	{ .type = vdevNONE }
};


struct {
	virtio_ctx_t vctx;   /* Device detection context */
	unsigned int desc;   /* Processed descriptors */
} virtiogpu_common;


/* Schedules and executes new task */
extern int graph_schedule(graph_t *graph);


/* Returns timestamp */
static inline unsigned long long virtiogpu_timestamp(void)
{
	struct timeval time;

	gettimeofday(&time, NULL);

	return 1000000ULL * (unsigned long long)time.tv_sec + (unsigned long long)time.tv_usec;
}


/* Calculates time delta */
static inline unsigned long long virtiogpu_timediff(unsigned long long start, unsigned long long end)
{
	return (end < start) ? (unsigned long long)-1 - start + end : end - start;
}


/* Returns resource format (ABGR on little endian and RGBA on big endian) */
static inline int virtiogpu_format(void)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return 121;
#else
	return 67;
#endif
}


/* Sends request to device */
static int _virtiogpu_send(virtiogpu_dev_t *vgpu, virtqueue_t *vq, virtiogpu_req_t *req, unsigned int resp)
{
	virtio_dev_t *vdev = &vgpu->vdev;
	int err;

#ifdef USE_POLLING
	mutexLock(vgpu->lock);
#endif
	req->done = 0;
	if ((err = virtqueue_enqueue(vdev, vq, &req->vreq)) < 0)
		return err;
	virtqueue_notify(vdev, vq);
#ifdef USE_POLLING
	vgpu->isr |= (1 << 0);
	condSignal(vgpu->cond);
	mutexUnlock(vgpu->lock);
#endif

	while (!req->done)
		condWait(req->cond, req->lock, 0);

	if (virtio_vtog32(*(volatile uint32_t *)(&req->hdr.type)) != resp)
		return -EFAULT;

	return EOK;
}


/* Destroys request context */
static void virtiogpu_put(virtiogpu_req_t *req)
{
	resourceDestroy(req->cond);
	resourceDestroy(req->lock);
	munmap(req, (sizeof(virtiogpu_req_t) + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1));
}


/* Creates request context */
static virtiogpu_req_t *virtiogpu_get(void)
{
	virtiogpu_req_t *req;

	if ((req = mmap(NULL, (sizeof(virtiogpu_req_t) + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1), PROT_READ | PROT_WRITE, MAP_UNCACHED | MAP_ANONYMOUS, OID_CONTIGUOUS, 0)) == MAP_FAILED)
		return NULL;

	if (mutexCreate(&req->lock) < 0) {
		munmap(req, (sizeof(virtiogpu_req_t) + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1));
		return NULL;
	}

	if (condCreate(&req->cond) < 0) {
		resourceDestroy(req->lock);
		munmap(req, (sizeof(virtiogpu_req_t) + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1));
		return NULL;
	}

	req->rseg.prev = &req->wseg;
	req->rseg.next = &req->wseg;
	req->wseg.prev = &req->rseg;
	req->wseg.next = &req->rseg;
	req->vreq.segs = &req->rseg;
	req->vreq.rsegs = 1;
	req->vreq.wsegs = 1;

	return req;
}


/* Retrieves connected displays info */
static int virtiogpu_info(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, virtiogpu_info_t *info)
{
	virtio_dev_t *vdev = &vgpu->vdev;
	int i, ret;

	mutexLock(req->lock);

	req->rseg.buff = &req->hdr;
	req->rseg.len = sizeof(req->hdr);
	req->wseg.buff = &req->hdr;
	req->wseg.len = sizeof(req->hdr) + sizeof(req->info);

	req->hdr.type = virtio_gtov32(vdev, 0x100);
	req->hdr.flags = virtio_gtov32(vdev, 1 << 0);

	do {
		if ((ret = _virtiogpu_send(vgpu, &vgpu->ctlq, req, 0x1101)) < 0)
			break;

		for (i = 0; i < sizeof(info->pmodes) / sizeof(info->pmodes[0]); i++) {
			info->pmodes[i].r.x = virtio_vtog32(vdev, req->info.pmodes[i].r.x);
			info->pmodes[i].r.y = virtio_vtog32(vdev, req->info.pmodes[i].r.y);
			info->pmodes[i].r.width = virtio_vtog32(vdev, req->info.pmodes[i].r.width);
			info->pmodes[i].r.height = virtio_vtog32(vdev, req->info.pmodes[i].r.height);
			info->pmodes[i].enabled = virtio_vtog32(vdev, req->info.pmodes[i].enabled);
			info->pmodes[i].flags = virtio_vtog32(vdev, req->info.pmodes[i].flags);
		}
	} while(0);

	mutexUnlock(req->lock);

	return ret;
}


/* Retrieves display EDID */
static int virtiogpu_edid(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, unsigned int sid, unsigned char *edid, unsigned int *len)
{
	virtio_dev_t *vdev = &vgpu->vdev;
	int i, ret;

	if (!(virtio_readFeatures(vdev) & (1ULL << 1)))
		return -ENOTSUP;

	mutexLock(req->lock);

	req->rseg.buff = &req->hdr;
	req->rseg.len = sizeof(req->hdr) + sizeof(req->edid) - sizeof(req->edid.data);
	req->wseg.buff = &req->hdr;
	req->wseg.len = sizeof(req->hdr) + sizeof(req->edid);

	req->hdr.type = virtio_gtov32(vdev, 0x10a);
	req->hdr.flags = virtio_gtov32(vdev, 1 << 0);
	req->edid.sid = virtio_gtov32(vdev, sid);

	do {
		if ((ret = _virtiogpu_send(vgpu, &vgpu->ctlq, req, 0x1104)) < 0)
			break;

		*len = virtio_vtog32(vdev, req->edid.sid);
		for (i = 0; i < *len; i++)
			edid[i] = req->edid.data[i];
	} while (0);

	mutexUnlock(req->lock);

	return ret;
}


/* Creates host resource */
static int virtiogpu_create(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, unsigned int fmt, unsigned int w, unsigned int h)
{
	virtio_dev_t *vdev = &vgpu->vdev;
	int ret;

	mutexLock(req->lock);

	req->rseg.buff = &req->hdr;
	req->rseg.len = sizeof(req->hdr) + sizeof(req->create);
	req->wseg.buff = &req->hdr;
	req->wseg.len = sizeof(req->hdr);

	req->hdr.type = virtio_gtov32(vdev, 0x101);
	req->hdr.flags = virtio_gtov32(vdev, 1 << 0);
	req->create.fmt = virtio_gtov32(vdev, fmt);
	req->create.w = virtio_gtov32(vdev, w);
	req->create.h = virtio_gtov32(vdev, h);
	req->create.rid = virtio_gtov32(vdev, 1 << (__builtin_ffsl(vgpu->rbmp) - 1));

	do {
		if ((ret = _virtiogpu_send(vgpu, &vgpu->ctlq, req, 0x1100)) < 0)
			break;

		ret = virtio_vtog32(req->create.rid);
		vgpu->rbmp &= ~ret;
	} while(0);

	mutexUnlock(req->lock);

	return ret;
}


/* Destroys host resource */
static int virtiogpu_destroy(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, unsigned int rid)
{
	virtio_dev_t *vdev = &vgpu->vdev;
	int ret;

	mutexLock(req->lock);

	req->rseg.buff = &req->hdr;
	req->rseg.len = sizeof(req->hdr) + sizeof(req->destroy);
	req->wseg.buff = &req->hdr;
	req->wseg.len = sizeof(req->hdr);

	req->hdr.type = virtio_gtov32(vdev, 0x102);
	req->hdr.flags = virtio_gtov32(vdev, 1 << 0);
	req->destroy.rid = virtio_gtov32(vdev, rid);

	do {
		if ((ret = _virtiogpu_send(vgpu, &vgpu->ctlq, req, 0x1100)) < 0)
			break;

		vgpu->rbmp |= rid;
	} while(0);

	mutexUnlock(req->lock);

	return ret;
}


/* Attaches buffer to resource */
static int virtiogpu_attach(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, unsigned int rid, void *buff, unsigned int len)
{
	virtio_dev_t *vdev = &vgpu->vdev;
	int ret;

	mutexLock(req->lock);

	req->rseg.buff = &req->hdr;
	req->rseg.len = sizeof(req->hdr) + sizeof(req->attach);
	req->wseg.buff = &req->hdr;
	req->wseg.len = sizeof(req->hdr);

	req->hdr.type = virtio_gtov32(vdev, 0x106);
	req->hdr.flags = virtio_gtov32(vdev, 1 << 0);
	req->attach.rid = virtio_gtov32(vdev, rid);
	req->attach.n = virtio_gtov32(vdev, 1);
	req->attach.addr = virtio_gtov64(vdev, va2pa(buff));
	req->attach.len = virtio_gtov32(vdev, len);

	ret = _virtiogpu_send(vgpu, &vgpu->ctlq, req, 0x1100));

	mutexUnlock(req->lock);

	return ret;
}


/* Detaches buffer from resource */
static int virtiogpu_detach(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, unsigned int rid)
{
	virtio_dev_t *vdev = &vgpu->vdev;
	int ret;

	mutexLock(req->lock);

	req->rseg.buff = &req->hdr;
	req->rseg.len = sizeof(req->hdr) + sizeof(req->detach);
	req->wseg.buff = &req->hdr;
	req->wseg.len = sizeof(req->hdr);

	req->hdr.type = virtio_gtov32(vdev, 0x107);
	req->hdr.flags = virtio_gtov32(vdev, 1 << 0);
	req->detach.rid = virtio_gtov32(vdev, rid);

	ret = _virtiogpu_send(vgpu, &vgpu->ctlq, req, 0x1100));

	mutexUnlock(req->lock);

	return ret;
}


/* Sets scanout resource */
static int virtiogpu_set(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int sid, unsigned int rid)
{
	virtio_dev_t *vdev = &vgpu->vdev;
	int ret;

	mutexLock(req->lock);

	req->rseg.buff = &req->hdr;
	req->rseg.len = sizeof(req->hdr) + sizeof(req->set);
	req->wseg.buff = &req->hdr;
	req->wseg.len = sizeof(req->hdr);

	req->hdr.type = virtio_gtov32(vdev, 0x103);
	req->hdr.flags = virtio_gtov32(vdev, 1 << 0);
	req->set.r.x = virtio_gtov32(vdev, x);
	req->set.r.y = virtio_gtov32(vdev, y);
	req->set.r.w = virtio_gtov32(vdev, w);
	req->set.r.h = virtio_gtov32(vdev, h);
	req->set.sid = virtio_gtov32(vdev, sid);
	req->set.rid = virtio_gtov32(vdev, rid);

	ret = _virtiogpu_send(vgpu, &vgpu->ctlq, req, 0x1100));

	mutexUnlock(req->lock);

	return ret;
}


/* Transfers resource from guest to host */
static int virtiogpu_transfer(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int offs, unsigned int rid)
{
	virtio_dev_t *vdev = &vgpu->vdev;
	int ret;

	mutexLock(req->lock);

	req->rseg.buff = &req->hdr;
	req->rseg.len = sizeof(req->hdr) + sizeof(req->transfer);
	req->wseg.buff = &req->hdr;
	req->wseg.len = sizeof(req->hdr);

	req->hdr.type = virtio_gtov32(vdev, 0x105);
	req->hdr.flags = virtio_gtov32(vdev, 1 << 0);
	req->transfer.r.x = virtio_gtov32(vdev, x);
	req->transfer.r.y = virtio_gtov32(vdev, y);
	req->transfer.r.w = virtio_gtov32(vdev, w);
	req->transfer.r.h = virtio_gtov32(vdev, h);
	req->transfer.offs = virtio_gtov64(vdev, offs);
	req->transfer.rid = virtio_gtov32(vdev, rid);

	ret = _virtiogpu_send(vgpu, &vgpu->ctlq, req, 0x1100));

	mutexUnlock(req->lock);

	return ret;
}


/* Flushes scanout resource to display */
static int virtiogpu_flush(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int rid)
{
	virtio_dev_t *vdev = &vgpu->vdev;
	int ret;

	mutexLock(req->lock);

	req->rseg.buff = &req->hdr;
	req->rseg.len = sizeof(req->hdr) + sizeof(req->flush);
	req->wseg.buff = &req->hdr;
	req->wseg.len = sizeof(req->hdr);

	req->hdr.type = virtio_gtov32(vdev, 0x104);
	req->hdr.flags = virtio_gtov32(vdev, 1 << 0);
	req->flush.r.x = virtio_gtov32(vdev, x);
	req->flush.r.y = virtio_gtov32(vdev, y);
	req->flush.r.w = virtio_gtov32(vdev, w);
	req->flush.r.h = virtio_gtov32(vdev, h);
	req->flush.rid = virtio_gtov32(vdev, rid);

	ret = _virtiogpu_send(vgpu, &vgpu->ctlq, req, 0x1100));

	mutexUnlock(req->lock);

	return ret;
}


#ifndef USE_POLLING
/* Interrupt handler */
static int virtiogpu_int(unsigned int n, void *arg)
{
	virtiogpu_dev_t *vgpu = (virtiogpu_dev_t *)arg;
	virtio_dev_t *vdev = &vgpu->vdev;

	virtqueue_disableIRQ(vdev, &vgpu->ctlq);
	virtqueue_disableIRQ(vdev, &vgpu->curq);
	vgpu->isr = virtio_isr(vdev);

	return vgpu->cond;
}
#endif


/* Interrupt/polling thread */
static void virtiogpu_intthr(void *arg)
{
	virtiogpu_dev_t *vgpu = (virtiogpu_dev_t *)arg;
	virtio_dev_t *vdev = &vgpu->vdev;
	virtiogpu_req_t *req;
	unsigned int isr;

	mutexLock(vgpu->lock);
	vgpu->done = 0;
	vgpu->isr = 0;

	while (!vgpu->done) {
		while (!(isr = vgpu->isr))
			condWait(vgpu->cond, vgpu->lock, 0);
		vgpu->isr = 0;

		/* Handle processed requests */
		if (isr & (1 << 0)) {
#ifdef USE_POLLING
			/* Poll for processed request (in polling mode requests are submitted and processed synchronously) */
			while (((req = virtqueue_dequeue(vdev, &vgpu->ctlq, NULL)) == NULL) && ((req = virtqueue_dequeue(vdev, &vgpu->curq, NULL)) == NULL));

			mutexLock(req->lock);
			req->done = 1;
			condSignal(req->cond);
			mutexUnlock(req->lock);
#else
			while ((req = virtqueue_dequeue(vdev, &vgpu->ctlq, NULL)) != NULL) {
				mutexLock(req->lock);
				req->done = 1;
				condSignal(req->cond);
				mutexUnlock(req->lock);
			}

			while ((req = virtqueue_dequeue(vdev, &vgpu->curq, NULL)) != NULL) {
				mutexLock(req->lock);
				req->done = 1;
				condSignal(req->cond);
				mutexUnlock(req->lock);
			}
#endif
		}

#ifndef USE_POLLING
		virtqueue_enableIRQ(vdev, &vgpu->ctlq);
		virtqueue_enableIRQ(vdev, &vgpu->curq);

		/* Get requests that might have come after last virtqueue_dequeue() and before virtqueue_enableIRQ() */
		while ((req = virtqueue_dequeue(vdev, &vgpu->ctlq, NULL)) != NULL) {
			mutexLock(req->lock);
			req->done = 1;
			condSignal(req->cond);
			mutexUnlock(req->lock);
		}

		while ((req = virtqueue_dequeue(vdev, &vgpu->curq, NULL)) != NULL) {
			mutexLock(req->lock);
			req->done = 1;
			condSignal(req->cond);
			mutexUnlock(req->lock);
		}
#endif
	}

	endthread();
}


/* Vsync thread */
static void virtiogpu_vsyncthr(void *arg)
{
	grapt_t *graph = (graph_t *)arg;
	virtiogpu_dev_t *vgpu = (virtiogpu_dev_t *)graph->adapter;
	unsigned long long diff, time = virtiogpu_timestamp();
	virtiogpu_req_t *req;

	if ((req = virtiogpu_get()) == NULL)
		endthread();

	while (!vgpu->done) {
		if ((diff = virtiogpu_timediff(time, virtiogpu_timestamp())) < graph->freq)
			usleep(graph->freq - diff);
		time += graph->freq;

		/* Transfer and flush framebuffer */
		virtiogpu_transfer(vgpu, req, 0, 0, graph->width, graph->height, 0, vgpu->rid);
		virtiogpu_flush(vgpu, req, 0, 0, graph->width, graph->height, vgpu->rid);

		/* Update vsync counter */
		mutexLock(graph->vlock);
		graph->vsync++;
		mutexUnlock(graph->vlock);

		/* Try to reschedule */
		graph_schedule(graph);
	}

	virtiogpu_put(req);
	endthread();
}


int virtiogpu_cursorhide(graph_t *graph)
{
	return EOK;
}


int virtiogpu_cursorshow(graph_t *graph)
{
	return EOK;
}


int virtiogpu_cursorpos(graph_t *graph, unsigned int x, unsigned int y)
{
	return EOK;
}


int virtiogpu_cursorset(graph_t *graph, char *and, char *xor, unsigned char bg, unsigned char fg)
{
	return EOK;
}


int virtiogpu_colorset(graph_t *graph, unsigned char *colors, unsigned char first, unsigned char last)
{
	return -ENOTSUP;
}


int virtiogpu_colorget(graph_t *graph, unsigned char *colors, unsigned char first, unsigned char last)
{
	return -ENOTSUP;
}


int virtiogpu_trigger(graph_t *graph)
{
	if (virtiogpu_isbusy(graph))
		return -EBUSY;

	return graph_schedule(graph);
}


static inline int _virtqueue_isbusy(virtio_dev_t *vdev, virtqueue_t *vq)
{
	return virtio_vtog16(vdev, *(volatile uint16_t *)&vq->avail->idx) != virtio_vtog16(vdev, *(volatile uint16_t *)&vq->used->idx);
}


int virtiogpu_isbusy(graph_t *graph)
{
	virtiogpu_dev_t *vgpu = (virtiogpu_dev_t *)graph->adapter;
	virtio_dev_t *vdev = &vgpu->vdev;
	int ret;

	mutexLock(vgpu->ctlq.lock);
	mutexLock(vgpu->curq.lock);

	ret = virtqueue_isbusy(vdev, &vgpu->ctlq) || virtqueue_isbusy(vdev, &vgpu->curq);

	mutexUnlock(vgpu->curq.lock);
	mutexUnlock(vgpu->ctlq.lock);

	return ret;
}


int virtiogpu_mode(graph_t *graph, unsigned int mode, unsigned int freq)
{
	virtiogpu_dev_t *vgpu = (virtiogpu_dev_t *)graph->adapter;
	unsigned int i;
	size_t size;
	void *buff;

	for (i = 0; modes[i].mode != mode; i++)
		if (modes[i].mode == GRAPH_NONE)
			return -ENOTSUP;

	if ((size = (modes[i].depth * modes[i].height * modes[i].width + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1)) > vgpu->size) {
		if ((buff = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_UNCACHED | MAP_ANONYMOUS, OID_CONTIGUOUS, 0)) == MAP_FAILED)
			return -ENOMEM;
		munmap(vgpu->buff, vgpu->size);
		vgpu->buff = buff;
		vgpu->size = size;
	}

	graph->width = modes[i].width;
	graph->height = modes[i].height;
	graph->depth = modes[i].depth;

}


void virtiogpu_close(graph_t *graph)
{
	virtiogpu_dev_t *vgpu = (virtiogpu_dev_t *)graph->adapter;
	virtio_dev_t *vdev = &vgpu->vdev;

	/* TODO: uninstall interrupt handler */
	/* End interrupt/polling and vsync threads */
	vgpu->done = 1;
	condSignal(vgpu->cond);
	threadJoin(0);

	/* Destroy device */
	virtqueue_destroy(vdev, &vgpu->ctlq);
	virtqueue_destroy(vdev, &vgpu->curq);
	virtio_destroyDev(vdev);
}


int virtiogpu_open(graph_t *graph)
{
	virtiogpu_dev_t *vgpu;
	virtio_dev_t vdev;
	int err;

	for (; info[virtiogpu_common.desc].type != vdevNONE; virtiogpu_common.desc++) {
		while ((err = virtio_find(&info[virtiogpu_common.desc], &vdev, &virtiogpu_common.vctx)) != -ENODEV) {
			if (err < 0)
				return err;

			if ((vgpu = malloc(sizeof(virtiogpu_dev_t))) == NULL)
				return = -ENOMEM;
			vgpu->vdev = vdev;

			if ((err = virtio_initDev(&vgpu->vdev)) < 0) {
				free(vgpu);
				if (err != -ENODEV)
					return err;
				continue;
			}

			do {
				/* Negotiate EDID support */
				if ((err = virtio_writeFeatures(&vgpu->vdev, (1 << 1))) < 0)
					break;

				if ((err = virtqueue_init(&vgpu->vdev, &vgpu->ctlq, 0, 64)) < 0)
					break;

				if ((err = virtqueue_init(&vgpu->vdev, &vgpu->curq, 1, 16)) < 0) {
					virtqueue_destroy(&vgpu->vdev, &vgpu->ctlq);
					break;
				}

				if ((err = mutexCreate(&vgpu->ilock)) < 0) {
					virtqueue_destroy(&vgpu->vdev, &vgpu->curq);
					virtqueue_destroy(&vgpu->vdev, &vgpu->ctlq);
					break;
				}

				if ((err = condCreate(&vgpu->icond)) < 0) {
					resourceDestroy(vgpu->ilock);
					virtqueue_destroy(&vgpu->vdev, &vgpu->curq);
					virtqueue_destroy(&vgpu->vdev, &vgpu->ctlq);
					break;
				}

				if ((err = beginthread(virtiogpu_intthr, 4, vgpu->istack, sizeof(vgpu->istack), vgpu)) < 0) {
					resourceDestroy(vgpu->icond);
					resourceDestroy(vgpu->ilock);
					virtqueue_destroy(&vgpu->vdev, &vgpu->curq);
					virtqueue_destroy(&vgpu->vdev, &vgpu->ctlq);
					break;
				}

#ifdef USE_POLLING
				virtqueue_disableIRQ(&vgpu->vdev, &vgpu->ctlq);
				virtqueue_disableIRQ(&vgpu->vdev, &vgpu->curq);
#else
				if ((err = interrupt(&vgpu->vdev->info.irq, virtiogpu_int, vgpu, vgpu->icond, &vgpu->inth)) < 0) {
					resourceDestroy(vgpu->icond);
					resourceDestroy(vgpu->ilock);
					virtqueue_destroy(vdev, &vgpu->curq);
					virtqueue_destroy(vdev, &vgpu->ctlq);
					break;
				}
#endif
				vgpu->rbmp = -1;
				virtio_writeStatus(vdev, virtio_readStatus(vdev) | (1 << 2));

				// if ((req = virtiogpu_open()) == NULL) {
				// 	err = -ENOMEM;
				// 	break;
				// }

				// /* Get default display info (QEMU doesn't report other displays info until their configuration have changed) */
				// if ((err = virtiogpu_displayInfo(vgpu, req, &info)) < 0)
				// 	break;

				// /* Initialize displays */
				// for (i = 0; i < vgpu->ndisplays; i++) {
				// 	if ((rid = virtiogpu_resource2D(vgpu, req, virtiogpu_endian() ? 121 : 3, info.pmodes[0].r.width, info.pmodes[0].r.height)) < 0) {
				// 		err = rid;
				// 		break;
				// 	}

				// 	if ((vgpu->displays[i] = mmap(NULL, (4 * info.pmodes[0].r.width * info.pmodes[0].r.height + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1), PROT_READ | PROT_WRITE, MAP_UNCACHED | MAP_ANONYMOUS, OID_CONTIGUOUS, 0)) == MAP_FAILED) {
				// 		err = -ENOMEM;
				// 		break;
				// 	}

				// 	if ((err = virtiogpu_attach(vgpu, req, rid, vgpu->displays[i], 4 * info.pmodes[0].r.width * info.pmodes[0].r.height)) < 0)
				// 		break;

				// 	if ((err = virtiogpu_scanout(vgpu, req, 0, 0, info.pmodes[0].r.width, info.pmodes[0].r.height, i, rid)) < 0)
				// 		break;
				// }

				// if (err < 0)
				// 	break;

				// virtiogpu_close(req);
				graph->adapter = vgpu;
				graph->close = virtiogpu_close;
				graph->mode = virtiogpu_mode;
				graph->isbusy = virtiogpu_isbusy;
				graph->trigger = virtiogpu_trigger;
				graph->colorset = virtiogpu_colorset;
				graph->colorget = virtiogpu_colorget;
				graph->cursorset = virtiogpu_cursorset;
				graph->cursorpos = virtiogpu_cursorpos;
				graph->cursorshow = virtiogpu_cursorshow;
				graph->cursorhide = vitriogpu_cursorhide;

				return EOK;
			} while (0);

			virtio_writeStatus(vdev, virtio_readStatus(vdev) | (1 << 7));
			virtio_destroyDev(vdev);
			free(vgpu);

			return err;
		}
		virtiogpu_common.vctx.reset = 1;
	}

	return -ENODEV;
}


void virtiogpu_done(void)
{
	virtio_done();
}


int virtiogpu_init(void)
{
	return virtio_init();
}
