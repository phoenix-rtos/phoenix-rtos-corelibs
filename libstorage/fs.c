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
		msg->o.err = -EINVAL;
		return;
	}

	fs = (storage_fs_t *)data;
	if (fs->ops == NULL) {
		msg->o.err = -EINVAL;
		return;
	}

	switch (msg->type) {
		case mtOpen:
			if (fs->ops->open == NULL) {
				msg->o.err = -ENOSYS;
				break;
			}
			msg->o.err = fs->ops->open(fs->info, &msg->oid);
			break;

		case mtClose:
			if (fs->ops->close == NULL) {
				msg->o.err = -ENOSYS;
				break;
			}
			msg->o.err = fs->ops->close(fs->info, &msg->oid);
			break;

		case mtRead:
			if (fs->ops->read == NULL) {
				msg->o.err = -ENOSYS;
				break;
			}
			msg->o.err = fs->ops->read(fs->info, &msg->oid, msg->i.io.offs, msg->o.data, msg->o.size);
			break;

		case mtWrite:
			if (fs->ops->write == NULL) {
				msg->o.err = -ENOSYS;
				break;
			}
			msg->o.err = fs->ops->write(fs->info, &msg->oid, msg->i.io.offs, msg->i.data, msg->i.size);
			break;

		case mtTruncate:
			if (fs->ops->truncate == NULL) {
				msg->o.err = -ENOSYS;
				break;
			}
			msg->o.err = fs->ops->truncate(fs->info, &msg->oid, msg->i.io.len);
			break;

		case mtDevCtl:
			if (fs->ops->devctl == NULL) {
				/* FIXME this error passing works by accident on ioctl(),
				 * there's no dedicated error field for devctl. */
				msg->o.err = -ENOTTY; /* To return valid errno on ioctl() */
				break;
			}
			fs->ops->devctl(fs->info, &msg->oid, msg->i.raw, msg->o.raw);
			break;

		case mtCreate:
			if (fs->ops->create == NULL) {
				msg->o.err = -ENOSYS;
				break;
			}
			msg->o.err = fs->ops->create(fs->info, &msg->oid, msg->i.data, &msg->o.create.oid, msg->i.create.mode, msg->i.create.type, &msg->i.create.dev);
			break;

		case mtDestroy:
			if (fs->ops->destroy == NULL) {
				msg->o.err = -ENOSYS;
				break;
			}
			msg->o.err = fs->ops->destroy(fs->info, &msg->oid);
			break;

		case mtSetAttr:
			if (fs->ops->setattr == NULL) {
				msg->o.err = -ENOSYS;
				break;
			}
			msg->o.err = fs->ops->setattr(fs->info, &msg->oid, msg->i.attr.type, msg->i.attr.val, msg->i.data, msg->i.size);
			break;

		case mtGetAttr:
			if (fs->ops->getattr == NULL) {
				msg->o.err = -ENOSYS;
				break;
			}
			msg->o.err = fs->ops->getattr(fs->info, &msg->oid, msg->i.attr.type, &msg->o.attr.val);
			break;

		case mtGetAttrAll:
			if (fs->ops->getattrall == NULL) {
				msg->o.err = -ENOSYS;
				break;
			}
			if ((msg->o.size < sizeof(struct _attrAll)) || (msg->o.data == NULL)) {
				msg->o.err = -EINVAL;
				break;
			}
			msg->o.err = fs->ops->getattrall(fs->info, &msg->oid, (struct _attrAll *)msg->o.data);
			break;

		case mtLookup:
			if (fs->ops->lookup == NULL) {
				msg->o.err = -ENOSYS;
				break;
			}
			msg->o.err = fs->ops->lookup(fs->info, &msg->oid, msg->i.data, &msg->o.lookup.fil, &msg->o.lookup.dev, msg->o.data, msg->o.size);
			break;

		case mtLink:
			if (fs->ops->link == NULL) {
				msg->o.err = -ENOSYS;
				break;
			}
			msg->o.err = fs->ops->link(fs->info, &msg->oid, msg->i.data, &msg->i.ln.oid);
			break;

		case mtUnlink:
			if (fs->ops->unlink == NULL) {
				msg->o.err = -ENOSYS;
				break;
			}
			msg->o.err = fs->ops->unlink(fs->info, &msg->oid, msg->i.data);
			break;

		case mtReaddir:
			if (fs->ops->readdir == NULL) {
				msg->o.err = -ENOSYS;
				break;
			}
			msg->o.err = fs->ops->readdir(fs->info, &msg->oid, msg->i.readdir.offs, msg->o.data, msg->o.size);
			break;

		case mtStat:
			if (fs->ops->statfs == NULL) {
				msg->o.err = -ENOSYS;
				break;
			}
			msg->o.err = fs->ops->statfs(fs->info, msg->o.data, msg->o.size);
			break;

		case mtSync:
			if (fs->ops->sync == NULL) {
				msg->o.err = -ENOSYS;
				break;
			}
			fs->ops->sync(fs->info, &msg->oid);
			break;

		default:
			break;
	}
}
