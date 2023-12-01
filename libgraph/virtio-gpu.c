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
#include <string.h>
#include <unistd.h>

#include <sys/interrupt.h>
#include <sys/mman.h>
#include <sys/threads.h>
#include <sys/types.h>

#include <virtio.h>

#include "graph.h"


/* Use polling on RISCV64 (interrupts trigger memory protection exception) */
#ifdef __TARGET_RISCV64
#define USE_POLLING
#endif

/* Default graphics mode index */
#define DEFMODE 15 /* 1024x768x32 */


typedef struct {
	uint32_t x; /* Horizontal coordinate */
	uint32_t y; /* Vertical coordinate */
	uint32_t w; /* Rectangle width */
	uint32_t h; /* Rectangle height */
} __attribute__((packed)) virtiogpu_rect_t;


typedef struct {
	struct {
		virtiogpu_rect_t r; /* Display rectangle */
		uint32_t enabled;   /* Display enabled? */
		uint32_t flags;     /* Display flags */
	} pmodes[16];
} __attribute__((packed)) virtiogpu_info_t;


typedef struct {
	/* Request buffers (accessible by device) */
	struct {
		/* Request header (device readable/writable) */
		struct {
			uint32_t type;  /* Request/Response type */
			uint32_t flags; /* Request flags */
			uint64_t fence; /* Request fence ID */
			uint32_t ctx;   /* Rendering context */
			uint32_t pad;   /* Padding */
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

			/* Allocate resource */
			struct {
				uint32_t rid; /* Resource ID */
				uint32_t fmt; /* Resource format */
				uint32_t w;   /* Resource width */
				uint32_t h;   /* Resource height */
			} alloc;

			/* Free resource */
			struct {
				uint32_t rid; /* Resource ID */
				uint32_t pad; /* Padding */
			} free;

			/* Attach resource buffers */
			struct {
				uint32_t rid;  /* Resource ID */
				uint32_t n;    /* Number of attached buffers */
				uint64_t addr; /* Buffer address */
				uint32_t len;  /* Buffer length */
				uint32_t pad;  /* Padding */
			} attach;

			/* Detach resource buffers */
			struct {
				uint32_t rid; /* Resource ID */
				uint32_t pad; /* Padding */
			} detach;

			/* Set scanout resource */
			struct {
				virtiogpu_rect_t r; /* Scanout rectangle */
				uint32_t sid;       /* Scanout ID */
				uint32_t rid;       /* Resource ID */
			} scanout;

			/* Transfer resource */
			struct {
				virtiogpu_rect_t r; /* Buffer rectangle */
				uint64_t offs;      /* Resource offset */
				uint32_t rid;       /* Resource ID */
				uint32_t pad;       /* Padding */
			} transfer;

			/* Flush scanout resource */
			struct {
				virtiogpu_rect_t r; /* Scanout rectangle */
				uint32_t rid;       /* Resource ID */
				uint32_t pad;       /* Padding */
			} flush;

			/* Update cursor */
			struct {
				struct {
					uint32_t sid; /* Scanout ID */
					uint32_t x;   /* Horizontal coordinate */
					uint32_t y;   /* Vertical coordinate */
					uint32_t pad; /* Padding */
				} pos;
				uint32_t rid; /* Resource ID */
				uint32_t hx;  /* Hotspot horizontal coordinate */
				uint32_t hy;  /* Hotspot vertical coordinate */
				uint32_t pad; /* Padding */
			} cursor;
		};
	} __attribute__((packed));

	/* VirtIO request segments */
	virtio_seg_t rseg; /* Device readable segment */
	virtio_seg_t wseg; /* Device writeable segment */
	virtio_req_t vreq; /* VirtIO request */

	/* Custom helper fields */
	volatile int done; /* Indicates request completion */
	handle_t lock;     /* Request mutex */
	handle_t cond;     /* Request condition variable */
} virtiogpu_req_t;


typedef struct {
	void *buff;       /* Buffer */
	unsigned int len; /* Buffer length */
	unsigned int rid; /* Resource ID */
} virtiogpu_resource_t;


typedef struct {
	/* Device info */
	virtio_dev_t vdev;          /* VirtIO device */
	virtqueue_t ctlq;           /* Control virtqueue */
	virtqueue_t curq;           /* Cursor virtqueue */
	unsigned int rbmp;          /* Resource bitmap */
	virtiogpu_req_t *req;       /* Request context */
	volatile unsigned int done; /* Destroy device? */

	/* Device resources */
	virtiogpu_resource_t fb;  /* Framebuffer resource */
	virtiogpu_resource_t cur; /* Cursor resource */
	unsigned char curst;      /* Cursor state */
	unsigned int curx;        /* Cursor horizontal coordinate */
	unsigned int cury;        /* Cursor vertical coordinate */

	/* Interrupt/polling thread */
	volatile unsigned int isr; /* Interrupt status */
	handle_t lock;             /* Interrupt mutex */
	handle_t cond;             /* Interrupt condition variable */
	handle_t inth;             /* Interrupt handle */
	char istack[2048] __attribute__((aligned(8)));
} virtiogpu_dev_t;


typedef struct {
	graph_mode_t mode;   /* Graphics mode */
	unsigned int width;  /* Screen width */
	unsigned int height; /* Screen height */
	unsigned char depth; /* Screen color depth */
} virtiogpu_mode_t;


/* VirtIO GPU device descriptors */
static const virtio_devinfo_t info[] = {
	{ .type = vdevPCI, .id = 0x1050 },
#ifdef __TARGET_RISCV64
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

/* clang-format off */
/* Graphics modes table (32-bit color only) */
static const virtiogpu_mode_t modes[] = {
	/* Power management modes */
	{ GRAPH_ON },                          /*  0 */
	{ GRAPH_OFF },                         /*  1 */
	{ GRAPH_STANDBY },                     /*  2 */
	{ GRAPH_SUSPEND },                     /*  3 */
	/* 32-bit color (8:8:8:8) */
	{ GRAPH_640x480x32,   640,  480,  4 }, /*  4 */
	{ GRAPH_720x480x32,   720,  480,  4 }, /*  5 */
	{ GRAPH_720x576x32,   720,  576,  4 }, /*  6 */
	{ GRAPH_800x600x32,   800,  600,  4 }, /*  7 */
	{ GRAPH_832x624x32,   832,  624,  4 }, /*  8 */
	{ GRAPH_896x672x32,   896,  672,  4 }, /*  9 */
	{ GRAPH_928x696x32,   928,  696,  4 }, /* 10 */
	{ GRAPH_960x540x32,   960,  540,  4 }, /* 11 */
	{ GRAPH_960x600x32,   960,  600,  4 }, /* 12 */
	{ GRAPH_960x720x32,   960,  720,  4 }, /* 13 */
	{ GRAPH_1024x576x32,  1024, 576,  4 }, /* 14 */
	{ GRAPH_1024x768x32,  1024, 768,  4 }, /* 15 */
	{ GRAPH_1152x864x32,  1152, 864,  4 }, /* 16 */
	{ GRAPH_1280x720x32,  1280, 720,  4 }, /* 17 */
	{ GRAPH_1280x800x32,  1280, 800,  4 }, /* 18 */
	{ GRAPH_1280x960x32,  1280, 960,  4 }, /* 19 */
	{ GRAPH_1280x1024x32, 1280, 1024, 4 }, /* 20 */
	{ GRAPH_1360x768x32,  1360, 768,  4 }, /* 21 */
	{ GRAPH_1368x768x32,  1368, 768,  4 }, /* 22 */
	{ GRAPH_1400x900x32,  1400, 900,  4 }, /* 23 */
	{ GRAPH_1400x1050x32, 1400, 1050, 4 }, /* 24 */
	{ GRAPH_1440x240x32,  1440, 240,  4 }, /* 25 */
	{ GRAPH_1440x288x32,  1440, 288,  4 }, /* 26 */
	{ GRAPH_1440x576x32,  1440, 576,  4 }, /* 27 */
	{ GRAPH_1440x810x32,  1440, 810,  4 }, /* 28 */
	{ GRAPH_1440x900x32,  1440, 900,  4 }, /* 29 */
	{ GRAPH_1600x900x32,  1600, 900,  4 }, /* 30 */
	{ GRAPH_1600x1024x32, 1600, 1024, 4 }, /* 31 */
	{ GRAPH_1650x750x32,  1650, 750,  4 }, /* 32 */
	{ GRAPH_1680x720x32,  1680, 720,  4 }, /* 33 */
	{ GRAPH_1680x1050x32, 1680, 1050, 4 }, /* 34 */
	{ GRAPH_1920x540x32,  1920, 540,  4 }, /* 35 */
	{ GRAPH_1920x1080x32, 1920, 1080, 4 }, /* 36 */
	/* No mode */
	{ 0 }
};
/* clang-format on */

struct {
	virtio_ctx_t vctx; /* Device detection context */
	unsigned int desc; /* Processed descriptors */
} virtiogpu_common;


/* Schedules and executes tasks */
extern int graph_schedule(graph_t *graph);


/* Returns host resource format (XRGB) */
static inline int virtiogpu_xrgb(void)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return 2;
#elif __BYTE_ORDER == __BIG_ENDIAN
	return 4;
#else
#error "Unsupported byte order"
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

	if (resp && (virtio_vtog32(vdev, *(volatile uint32_t *)(&req->hdr.type)) != resp))
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


/* Returns request context */
static virtiogpu_req_t *virtiogpu_get(void)
{
	virtiogpu_req_t *req;

	if ((req = mmap(NULL, (sizeof(virtiogpu_req_t) + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1), PROT_READ | PROT_WRITE, MAP_UNCACHED | MAP_ANONYMOUS | MAP_CONTIGUOUS, -1, 0)) == MAP_FAILED)
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


/* Returns display info */
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
			info->pmodes[i].r.w = virtio_vtog32(vdev, req->info.pmodes[i].r.w);
			info->pmodes[i].r.h = virtio_vtog32(vdev, req->info.pmodes[i].r.h);
			info->pmodes[i].enabled = virtio_vtog32(vdev, req->info.pmodes[i].enabled);
			info->pmodes[i].flags = virtio_vtog32(vdev, req->info.pmodes[i].flags);
		}
	} while (0);

	mutexUnlock(req->lock);

	return ret;
}


/* Returns scanout EDID */
static int virtiogpu_edid(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, unsigned int sid, unsigned char *edid, unsigned int *len)
{
	virtio_dev_t *vdev = &vgpu->vdev;
	unsigned int l;
	int i, ret;

	/* Check EDID support */
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

		l = virtio_vtog32(vdev, req->edid.sid);
		if (len != NULL)
			*len = l;

		for (i = 0; i < l; i++)
			edid[i] = req->edid.data[i];
	} while (0);

	mutexUnlock(req->lock);

	return ret;
}


/* Allocates host resource */
static int virtiogpu_alloc(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, unsigned int format, unsigned int width, unsigned int height)
{
	virtio_dev_t *vdev = &vgpu->vdev;
	int ret;

	mutexLock(req->lock);

	req->rseg.buff = &req->hdr;
	req->rseg.len = sizeof(req->hdr) + sizeof(req->alloc);
	req->wseg.buff = &req->hdr;
	req->wseg.len = sizeof(req->hdr);

	req->hdr.type = virtio_gtov32(vdev, 0x101);
	req->hdr.flags = virtio_gtov32(vdev, 1 << 0);
	req->alloc.fmt = virtio_gtov32(vdev, format);
	req->alloc.w = virtio_gtov32(vdev, width);
	req->alloc.h = virtio_gtov32(vdev, height);
	req->alloc.rid = virtio_gtov32(vdev, 1 << (__builtin_ffsl(vgpu->rbmp) - 1));

	do {
		if ((ret = _virtiogpu_send(vgpu, &vgpu->ctlq, req, 0x1100)) < 0)
			break;

		ret = virtio_vtog32(vdev, req->alloc.rid);
		vgpu->rbmp &= ~ret;
	} while (0);

	mutexUnlock(req->lock);

	return ret;
}


/* Releases host resource */
static int virtiogpu_free(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, unsigned int rid)
{
	virtio_dev_t *vdev = &vgpu->vdev;
	int ret;

	mutexLock(req->lock);

	req->rseg.buff = &req->hdr;
	req->rseg.len = sizeof(req->hdr) + sizeof(req->free);
	req->wseg.buff = &req->hdr;
	req->wseg.len = sizeof(req->hdr);

	req->hdr.type = virtio_gtov32(vdev, 0x102);
	req->hdr.flags = virtio_gtov32(vdev, 1 << 0);
	req->free.rid = virtio_gtov32(vdev, rid);

	do {
		if ((ret = _virtiogpu_send(vgpu, &vgpu->ctlq, req, 0x1100)) < 0)
			break;

		vgpu->rbmp |= rid;
	} while (0);

	mutexUnlock(req->lock);

	return ret;
}


/* Attaches buffer to host resource */
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

	ret = _virtiogpu_send(vgpu, &vgpu->ctlq, req, 0x1100);

	mutexUnlock(req->lock);

	return ret;
}


/* Detaches buffer from host resource */
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

	ret = _virtiogpu_send(vgpu, &vgpu->ctlq, req, 0x1100);

	mutexUnlock(req->lock);

	return ret;
}


/* Sets scanout host resource */
static int virtiogpu_scanout(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int sid, unsigned int rid)
{
	virtio_dev_t *vdev = &vgpu->vdev;
	int ret;

	mutexLock(req->lock);

	req->rseg.buff = &req->hdr;
	req->rseg.len = sizeof(req->hdr) + sizeof(req->scanout);
	req->wseg.buff = &req->hdr;
	req->wseg.len = sizeof(req->hdr);

	req->hdr.type = virtio_gtov32(vdev, 0x103);
	req->hdr.flags = virtio_gtov32(vdev, 1 << 0);
	req->scanout.r.x = virtio_gtov32(vdev, x);
	req->scanout.r.y = virtio_gtov32(vdev, y);
	req->scanout.r.w = virtio_gtov32(vdev, width);
	req->scanout.r.h = virtio_gtov32(vdev, height);
	req->scanout.sid = virtio_gtov32(vdev, sid);
	req->scanout.rid = virtio_gtov32(vdev, rid);

	ret = _virtiogpu_send(vgpu, &vgpu->ctlq, req, 0x1100);

	mutexUnlock(req->lock);

	return ret;
}


/* Transfers data to host resource */
static int virtiogpu_transfer(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int offs, unsigned int rid)
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
	req->transfer.r.w = virtio_gtov32(vdev, width);
	req->transfer.r.h = virtio_gtov32(vdev, height);
	req->transfer.offs = virtio_gtov64(vdev, offs);
	req->transfer.rid = virtio_gtov32(vdev, rid);

	ret = _virtiogpu_send(vgpu, &vgpu->ctlq, req, 0x1100);

	mutexUnlock(req->lock);

	return ret;
}


/* Flushes host resource to connected scanouts displays */
static int virtiogpu_flush(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int rid)
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
	req->flush.r.w = virtio_gtov32(vdev, width);
	req->flush.r.h = virtio_gtov32(vdev, height);
	req->flush.rid = virtio_gtov32(vdev, rid);

	ret = _virtiogpu_send(vgpu, &vgpu->ctlq, req, 0x1100);

	mutexUnlock(req->lock);

	return ret;
}


/* Moves cursor */
static int virtiogpu_movecursor(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, unsigned int x, unsigned int y, unsigned int sid)
{
	virtio_dev_t *vdev = &vgpu->vdev;
	int ret;

	mutexLock(req->lock);

	req->rseg.buff = &req->hdr;
	req->rseg.len = sizeof(req->hdr) + sizeof(req->cursor);
	req->wseg.buff = &req->hdr;
	req->wseg.len = sizeof(req->hdr);

	req->hdr.type = virtio_gtov32(vdev, 0x301);
	req->hdr.flags = virtio_gtov32(vdev, 1 << 0);
	req->cursor.pos.sid = virtio_gtov32(vdev, sid);
	req->cursor.pos.x = virtio_gtov32(vdev, x);
	req->cursor.pos.y = virtio_gtov32(vdev, y);

	ret = _virtiogpu_send(vgpu, &vgpu->curq, req, 0);

	mutexUnlock(req->lock);

	return ret;
}


/* Sets cursor icon */
static int virtiogpu_setcursor(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, unsigned int x, unsigned int y, unsigned int hx, unsigned int hy, unsigned int sid, unsigned int rid)
{
	virtio_dev_t *vdev = &vgpu->vdev;
	int ret;

	mutexLock(req->lock);

	req->rseg.buff = &req->hdr;
	req->rseg.len = sizeof(req->hdr) + sizeof(req->cursor);
	req->wseg.buff = &req->hdr;
	req->wseg.len = sizeof(req->hdr);

	req->hdr.type = virtio_gtov32(vdev, 0x300);
	req->hdr.flags = virtio_gtov32(vdev, 1 << 0);
	req->cursor.pos.sid = virtio_gtov32(vdev, sid);
	req->cursor.pos.x = virtio_gtov32(vdev, x);
	req->cursor.pos.y = virtio_gtov32(vdev, y);
	req->cursor.rid = virtio_gtov32(vdev, rid);
	req->cursor.hx = virtio_gtov32(vdev, hx);
	req->cursor.hy = virtio_gtov32(vdev, hy);

	ret = _virtiogpu_send(vgpu, &vgpu->curq, req, 0);

	mutexUnlock(req->lock);

	return ret;
}


/* Creates resource */
static int virtiogpu_create(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, unsigned int format, unsigned int width, unsigned int height, virtiogpu_resource_t *res)
{
	int err;

	res->len = (4 * height * width + _PAGE_SIZE - 1) & ~(_PAGE_SIZE - 1);
	if ((res->buff = mmap(NULL, res->len, PROT_READ | PROT_WRITE, MAP_UNCACHED | MAP_ANONYMOUS | MAP_CONTIGUOUS, -1, 0)) == MAP_FAILED)
		return -ENOMEM;

	if ((err = virtiogpu_alloc(vgpu, req, format, width, height)) < 0) {
		munmap(res->buff, res->len);
		return err;
	}
	res->rid = err;

	if ((err = virtiogpu_attach(vgpu, req, res->rid, res->buff, res->len)) < 0) {
		virtiogpu_free(vgpu, req, res->rid);
		munmap(res->buff, res->len);
		return err;
	}

	return EOK;
}


/* Destroys resource */
static void virtiogpu_destroy(virtiogpu_dev_t *vgpu, virtiogpu_req_t *req, virtiogpu_resource_t *res)
{
	virtiogpu_detach(vgpu, req, res->rid);
	virtiogpu_free(vgpu, req, res->rid);
	munmap(res->buff, res->len);
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
	vgpu->isr = 0;

	while (!vgpu->done) {
		while (!(isr = vgpu->isr))
			condWait(vgpu->cond, vgpu->lock, 0);
		vgpu->isr = 0;

		/* Handle processed requests */
		if (isr & (1 << 0)) {
#ifdef USE_POLLING
			/* Poll for processed request (in polling mode requests are submitted and processed synchronously) */
			while (((req = virtqueue_dequeue(vdev, &vgpu->ctlq, NULL)) == NULL) && ((req = virtqueue_dequeue(vdev, &vgpu->curq, NULL)) == NULL))
				;

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


int virtiogpu_cursorpos(graph_t *graph, unsigned int x, unsigned int y)
{
	virtiogpu_dev_t *vgpu = (virtiogpu_dev_t *)graph->adapter;
	int err;

	if (vgpu->curst && ((err = virtiogpu_movecursor(vgpu, vgpu->req, x, y, 0)) < 0))
		return err;

	vgpu->curx = x;
	vgpu->cury = y;

	return EOK;
}


int virtiogpu_cursorset(graph_t *graph, const unsigned char *amask, const unsigned char *xmask, unsigned int bg, unsigned int fg)
{
	virtiogpu_dev_t *vgpu = (virtiogpu_dev_t *)graph->adapter;
	uint32_t *cur = vgpu->cur.buff;
	unsigned char and, xor;
	unsigned int i, j, k;
	int err;

	for (i = 0; i < 64; i++) {
		for (j = 0; j < 8; j++, amask++, xmask++) {
			and = *amask;
			xor = *xmask;
			for (k = 0; k < 8; k++, and <<= 1, xor <<= 1) {
				switch ((and&0x80) >> 6 | (xor&0x80) >> 7) {
					case 0:
						*cur++ = bg;
						break;

					case 1:
						*cur++ = fg;
						break;

					default:
						*cur++ = 0;
				}
			}
		}
	}

	if ((err = virtiogpu_transfer(vgpu, vgpu->req, 0, 0, 64, 64, 0, vgpu->cur.rid)) < 0)
		return err;

	if (vgpu->curst)
		return virtiogpu_setcursor(vgpu, vgpu->req, vgpu->curx, vgpu->cury, 0, 0, 0, vgpu->cur.rid);

	return EOK;
}


int virtiogpu_cursorhide(graph_t *graph)
{
	virtiogpu_dev_t *vgpu = (virtiogpu_dev_t *)graph->adapter;
	int err;

	if (vgpu->curst && ((err = virtiogpu_setcursor(vgpu, vgpu->req, vgpu->curx, vgpu->cury, 0, 0, 0, 0)) < 0))
		return err;
	vgpu->curst = 0;

	return EOK;
}


int virtiogpu_cursorshow(graph_t *graph)
{
	virtiogpu_dev_t *vgpu = (virtiogpu_dev_t *)graph->adapter;
	int err;

	if (!vgpu->curst && ((err = virtiogpu_setcursor(vgpu, vgpu->req, vgpu->curx, vgpu->cury, 0, 0, 0, vgpu->cur.rid)) < 0))
		return err;
	vgpu->curst = 1;

	return EOK;
}


int virtiogpu_colorset(graph_t *graph, const unsigned char *colors, unsigned char first, unsigned char last)
{
	return -ENOTSUP;
}


int virtiogpu_colorget(graph_t *graph, unsigned char *colors, unsigned char first, unsigned char last)
{
	return -ENOTSUP;
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

	ret = _virtqueue_isbusy(vdev, &vgpu->ctlq) || _virtqueue_isbusy(vdev, &vgpu->curq);

	mutexUnlock(vgpu->curq.lock);
	mutexUnlock(vgpu->ctlq.lock);

	return ret;
}


int virtiogpu_commit(graph_t *graph)
{
	virtiogpu_dev_t *vgpu = (virtiogpu_dev_t *)graph->adapter;
	int ret;

	/* Transfer and flush framebuffer */
	mutexLock(graph->lock);

	do {
		if ((ret = virtiogpu_transfer(vgpu, vgpu->req, 0, 0, graph->width, graph->height, 0, vgpu->fb.rid)) < 0)
			break;

		if ((ret = virtiogpu_flush(vgpu, vgpu->req, 0, 0, graph->width, graph->height, vgpu->fb.rid)) < 0)
			break;
	} while (0);

	mutexUnlock(graph->lock);

	/* Try to reschedule */
	graph_schedule(graph);

	return ret;
}


int virtiogpu_trigger(graph_t *graph)
{
	if (virtiogpu_isbusy(graph))
		return -EBUSY;

	return graph_schedule(graph);
}


int virtiogpu_vsync(graph_t *graph)
{
	return 1;
}


int virtiogpu_mode(graph_t *graph, graph_mode_t mode, graph_freq_t freq)
{
	virtiogpu_dev_t *vgpu = (virtiogpu_dev_t *)graph->adapter;
	virtiogpu_resource_t res;
	unsigned int i = DEFMODE;
	int err;

	if (mode != GRAPH_DEFMODE) {
		for (i = 0; modes[i].mode != mode; i++)
			if (!modes[i].mode)
				return -ENOTSUP;
	}

	/* Power management modes */
	switch (modes[i].mode) {
		case GRAPH_ON:
			return virtiogpu_scanout(vgpu, vgpu->req, 0, 0, graph->width, graph->height, 0, vgpu->fb.rid);

		case GRAPH_OFF:
		case GRAPH_STANDBY:
		case GRAPH_SUSPEND:
			return virtiogpu_scanout(vgpu, vgpu->req, 0, 0, 0, 0, 0, 0);

		default:
			break;
	}

	/* Create new framebuffer resource and set it as scanout for the display */
	if ((err = virtiogpu_create(vgpu, vgpu->req, virtiogpu_xrgb(), modes[i].width, modes[i].height, &res)) < 0)
		return err;

	if ((err = virtiogpu_scanout(vgpu, vgpu->req, 0, 0, modes[i].width, modes[i].height, 0, res.rid)) < 0) {
		virtiogpu_destroy(vgpu, vgpu->req, &res);
		return err;
	}

	/* Update graph info */
	mutexLock(graph->lock);

	graph->data = res.buff;
	graph->width = modes[i].width;
	graph->height = modes[i].height;
	graph->depth = modes[i].depth;
	virtiogpu_destroy(vgpu, vgpu->req, &vgpu->fb);
	vgpu->fb = res;

	mutexUnlock(graph->lock);

	return EOK;
}


static void virtiogpu_destroydev(virtiogpu_dev_t *vgpu)
{
	virtio_dev_t *vdev = &vgpu->vdev;

	resourceDestroy(vgpu->cond);
	resourceDestroy(vgpu->lock);
	resourceDestroy(vgpu->inth);
	virtqueue_destroy(vdev, &vgpu->ctlq);
	virtqueue_destroy(vdev, &vgpu->curq);
	virtio_destroyDev(vdev);
}


static int virtiogpu_initdev(virtiogpu_dev_t *vgpu)
{
	virtio_dev_t *vdev = &vgpu->vdev;
	int err;

	if ((err = virtio_initDev(vdev)) < 0)
		return err;

	vgpu->done = 0;
	vgpu->rbmp = -1;
	vgpu->curst = 0;
	vgpu->curx = 0;
	vgpu->cury = 0;

	do {
		/* Negotiate EDID support */
		if ((err = virtio_writeFeatures(vdev, (1 << 1))) < 0)
			break;

		if ((err = virtqueue_init(vdev, &vgpu->ctlq, 0, 64)) < 0)
			break;

		if ((err = virtqueue_init(vdev, &vgpu->curq, 1, 16)) < 0) {
			virtqueue_destroy(vdev, &vgpu->ctlq);
			break;
		}

		if ((err = mutexCreate(&vgpu->lock)) < 0) {
			virtqueue_destroy(vdev, &vgpu->curq);
			virtqueue_destroy(vdev, &vgpu->ctlq);
			break;
		}

		if ((err = condCreate(&vgpu->cond)) < 0) {
			resourceDestroy(vgpu->lock);
			virtqueue_destroy(vdev, &vgpu->curq);
			virtqueue_destroy(vdev, &vgpu->ctlq);
			break;
		}

		if ((err = beginthread(virtiogpu_intthr, 4, vgpu->istack, sizeof(vgpu->istack), vgpu)) < 0) {
			resourceDestroy(vgpu->cond);
			resourceDestroy(vgpu->lock);
			virtqueue_destroy(vdev, &vgpu->curq);
			virtqueue_destroy(vdev, &vgpu->ctlq);
			break;
		}

		/* TODO: fix race condition below */
		/* Added 100ms delay for virtiogpu_intthr() to go to sleep on vgpu->cond before first request */
		usleep(100000);

#ifdef USE_POLLING
		virtqueue_disableIRQ(vdev, &vgpu->ctlq);
		virtqueue_disableIRQ(vdev, &vgpu->curq);
#else
		if ((err = interrupt(vdev->info.irq, virtiogpu_int, vgpu, vgpu->cond, &vgpu->inth)) < 0) {
			/* End interrupt/polling thread */
			mutexLock(vgpu->lock);
			vgpu->isr |= (1 << 1);
			vgpu->done = 1;
			condSignal(vgpu->cond);
			mutexUnlock(vgpu->lock);
			while (threadJoin(-1, 0) < 0)
				;

			resourceDestroy(vgpu->cond);
			resourceDestroy(vgpu->lock);
			virtqueue_destroy(vdev, &vgpu->curq);
			virtqueue_destroy(vdev, &vgpu->ctlq);
			break;
		}
#endif
		virtio_writeStatus(vdev, virtio_readStatus(vdev) | (1 << 2));

		return EOK;
	} while (0);

	virtio_writeStatus(vdev, virtio_readStatus(vdev) | (1 << 7));
	virtio_destroyDev(vdev);

	return err;
}


void virtiogpu_close(graph_t *graph)
{
	virtiogpu_dev_t *vgpu = (virtiogpu_dev_t *)graph->adapter;

	/* Destroy resources */
	virtiogpu_destroy(vgpu, vgpu->req, &vgpu->fb);
	virtiogpu_destroy(vgpu, vgpu->req, &vgpu->cur);
	virtiogpu_put(vgpu->req);

	/* TODO: uninstall interrupt handler */
	/* End interrupt/polling thread */
	mutexLock(vgpu->lock);
	vgpu->isr |= (1 << 1);
	vgpu->done = 1;
	condSignal(vgpu->cond);
	mutexUnlock(vgpu->lock);
	while (threadJoin(-1, 0) < 0)
		;

	/* Destroy device */
	virtiogpu_destroydev(vgpu);
	free(vgpu);
}


int virtiogpu_open(graph_t *graph)
{
	unsigned char edid[1024];
	virtiogpu_info_t vinfo;
	virtiogpu_dev_t *vgpu;
	virtio_dev_t vdev;
	int err;

	for (; info[virtiogpu_common.desc].type != vdevNONE; virtiogpu_common.desc++, virtiogpu_common.vctx.reset = 1) {
		while ((err = virtio_find(&info[virtiogpu_common.desc], &vdev, &virtiogpu_common.vctx)) != -ENODEV) {
			if (err < 0)
				return err;

			if ((vgpu = malloc(sizeof(virtiogpu_dev_t))) == NULL)
				return -ENOMEM;
			vgpu->vdev = vdev;

			/* Initialize device */
			if ((err = virtiogpu_initdev(vgpu)) < 0) {
				free(vgpu);
				if (err != -ENODEV)
					return err;
				continue;
			}

			do {
				/* Create request context */
				if ((vgpu->req = virtiogpu_get()) == NULL) {
					err = -ENOMEM;
					break;
				}

				/* Get display info */
				if ((err = virtiogpu_info(vgpu, vgpu->req, &vinfo)) < 0) {
					virtiogpu_put(vgpu->req);
					break;
				}

				/* Get EDID */
				if ((virtio_readFeatures(&vgpu->vdev) & (1ULL << 1)) && ((err = virtiogpu_edid(vgpu, vgpu->req, 0, edid, NULL)) < 0)) {
					virtiogpu_put(vgpu->req);
					break;
				}

				/* Create framebuffer */
				if ((err = virtiogpu_create(vgpu, vgpu->req, virtiogpu_xrgb(), vinfo.pmodes[0].r.w, vinfo.pmodes[0].r.h, &vgpu->fb)) < 0) {
					virtiogpu_put(vgpu->req);
					break;
				}

				/* Create cursor */
				if ((err = virtiogpu_create(vgpu, vgpu->req, virtiogpu_xrgb(), 64, 64, &vgpu->cur)) < 0) {
					virtiogpu_destroy(vgpu, vgpu->req, &vgpu->fb);
					virtiogpu_put(vgpu->req);
					break;
				}

				/* Set scanout */
				if ((err = virtiogpu_scanout(vgpu, vgpu->req, 0, 0, vinfo.pmodes[0].r.w, vinfo.pmodes[0].r.h, 0, vgpu->fb.rid)) < 0) {
					virtiogpu_destroy(vgpu, vgpu->req, &vgpu->cur);
					virtiogpu_destroy(vgpu, vgpu->req, &vgpu->fb);
					virtiogpu_put(vgpu->req);
					break;
				}

				/* Initialize graph info */
				graph->adapter = vgpu;
				graph->data = vgpu->fb.buff;
				graph->width = vinfo.pmodes[0].r.w;
				graph->height = vinfo.pmodes[0].r.h;
				graph->depth = 4;

				/* Set graph functions */
				graph->close = virtiogpu_close;
				graph->mode = virtiogpu_mode;
				graph->vsync = virtiogpu_vsync;
				graph->isbusy = virtiogpu_isbusy;
				graph->trigger = virtiogpu_trigger;
				graph->commit = virtiogpu_commit;
				graph->colorset = virtiogpu_colorset;
				graph->colorget = virtiogpu_colorget;
				graph->cursorset = virtiogpu_cursorset;
				graph->cursorpos = virtiogpu_cursorpos;
				graph->cursorshow = virtiogpu_cursorshow;
				graph->cursorhide = virtiogpu_cursorhide;

				return EOK;
			} while (0);

			/* End interrupt/polling thread */
			mutexLock(vgpu->lock);
			vgpu->isr |= (1 << 1);
			vgpu->done = 1;
			condSignal(vgpu->cond);
			mutexUnlock(vgpu->lock);
			while (threadJoin(-1, 0) < 0)
				;

			/* Destroy device */
			virtiogpu_destroydev(vgpu);
			free(vgpu);

			return err;
		}
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
