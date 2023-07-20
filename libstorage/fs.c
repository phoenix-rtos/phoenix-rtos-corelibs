/*
 * Phoenix-RTOS
 *
 * Filesystem interface
 *
 * Copyright 2022, 2023 Phoenix Systems
 * Author: Hubert Buczynski, Lukasz Kosinski, Hubert Badocha
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "include/storage/fs.h"

#include <errno.h>
#include <sys/file.h>


void storage_fsHandler(void *data, msg_t *msg)
{
	storage_fs_t *fs;

	if (data == NULL) {
		msg->o.io.err = -EINVAL;
		return;
	}

	fs = (storage_fs_t *)data;
	if (fs->ops == NULL) {
		msg->o.io.err = -EINVAL;
		return;
	}

	switch (msg->type) {
		case mtOpen:
			if (fs->ops->open == NULL) {
				msg->o.io.err = -ENOSYS;
				break;
			}
			msg->o.io.err = fs->ops->open(fs->info, &msg->i.openclose.oid);
			break;

		case mtClose:
			if (fs->ops->close == NULL) {
				msg->o.io.err = -ENOSYS;
				break;
			}
			msg->o.io.err = fs->ops->close(fs->info, &msg->i.openclose.oid);
			break;

		case mtRead:
			if (fs->ops->read == NULL) {
				msg->o.io.err = -ENOSYS;
				break;
			}
			msg->o.io.err = fs->ops->read(fs->info, &msg->i.io.oid, msg->i.io.offs, msg->o.data, msg->o.size);
			break;

		case mtWrite:
			if (fs->ops->write == NULL) {
				msg->o.io.err = -ENOSYS;
				break;
			}
			msg->o.io.err = fs->ops->write(fs->info, &msg->i.io.oid, msg->i.io.offs, msg->i.data, msg->i.size);
			break;

		case mtTruncate:
			if (fs->ops->truncate == NULL) {
				msg->o.io.err = -ENOSYS;
				break;
			}
			msg->o.io.err = fs->ops->truncate(fs->info, &msg->i.io.oid, msg->i.io.len);
			break;

		case mtDevCtl:
			if (fs->ops->devctl == NULL) {
				/* FIXME this error passing works by accident on ioctl(),
				 * there's no dedicated error field for devctl. */
				msg->o.io.err = -ENOTTY; /* To return valid errno on ioctl() */
				break;
			}
			fs->ops->devctl(fs->info, &msg->i.io.oid, msg->i.raw, msg->o.raw);
			break;

		case mtCreate:
			if (fs->ops->create == NULL) {
				msg->o.io.err = -ENOSYS;
				break;
			}
			msg->o.create.err = fs->ops->create(fs->info, &msg->i.create.dir, msg->i.data, &msg->o.create.oid, msg->i.create.mode, msg->i.create.type, &msg->i.create.dev);
			break;

		case mtDestroy:
			if (fs->ops->destroy == NULL) {
				msg->o.io.err = -ENOSYS;
				break;
			}
			msg->o.io.err = fs->ops->destroy(fs->info, &msg->i.destroy.oid);
			break;

		case mtSetAttr:
			if (fs->ops->setattr == NULL) {
				msg->o.io.err = -ENOSYS;
				break;
			}
			msg->o.attr.err = fs->ops->setattr(fs->info, &msg->i.attr.oid, msg->i.attr.type, msg->i.attr.val, msg->i.data, msg->i.size);
			break;

		case mtGetAttr:
			if (fs->ops->getattr == NULL) {
				msg->o.io.err = -ENOSYS;
				break;
			}
			msg->o.attr.err = fs->ops->getattr(fs->info, &msg->i.attr.oid, msg->i.attr.type, &msg->o.attr.val);
			break;

		case mtLookup:
			if (fs->ops->lookup == NULL) {
				msg->o.io.err = -ENOSYS;
				break;
			}
			msg->o.lookup.err = fs->ops->lookup(fs->info, &msg->i.lookup.dir, msg->i.data, &msg->o.lookup.fil, &msg->o.lookup.dev, msg->o.data, msg->o.size);
			break;

		case mtLink:
			if (fs->ops->link == NULL) {
				msg->o.io.err = -ENOSYS;
				break;
			}
			msg->o.io.err = fs->ops->link(fs->info, &msg->i.ln.dir, msg->i.data, &msg->i.ln.oid);
			break;

		case mtUnlink:
			if (fs->ops->unlink == NULL) {
				msg->o.io.err = -ENOSYS;
				break;
			}
			msg->o.io.err = fs->ops->unlink(fs->info, &msg->i.ln.dir, msg->i.data);
			break;

		case mtReaddir:
			if (fs->ops->readdir == NULL) {
				msg->o.io.err = -ENOSYS;
				break;
			}
			msg->o.io.err = fs->ops->readdir(fs->info, &msg->i.readdir.dir, msg->i.readdir.offs, msg->o.data, msg->o.size);
			break;

		case mtStat:
			if (fs->ops->statfs == NULL) {
				msg->o.io.err = -ENOSYS;
				break;
			}
			msg->o.io.err = fs->ops->statfs(fs->info, msg->o.data, msg->o.size);
			break;

		case mtSync:
			if (fs->ops->sync == NULL) {
				msg->o.io.err = -ENOSYS;
				break;
			}
			fs->ops->sync(fs->info, &msg->i.io.oid);
			break;

		default:
			break;
	}
}
