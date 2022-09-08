/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Memory Technology Device (MTD) Interface
 *
 * Copyright 2022 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "include/mtd/mtd.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>


int mtd_erase(struct mtd_info *mtdInfo, struct erase_info *instr)
{
	int res;
	storage_mtd_t *mtd = mtdInfo->storage->dev->mtd;

	if (mtd->ops->erase == NULL) {
		return -EOPNOTSUPP;
	}

	if (instr->addr >= mtdInfo->size || instr->len > (mtdInfo->size - instr->addr)) {
		return -EINVAL;
	}

	if ((mtdInfo->flags & MTD_WRITEABLE) == 0) {
		return -EROFS;
	}

	instr->fail_addr = MTD_FAIL_ADDR_UNKNOWN;
	instr->state = MTD_ERASING;
	if (instr->len == 0) {
		instr->state = MTD_ERASE_DONE;
	}
	else {
		instr->state = MTD_ERASE_PENDING;
		res = mtd->ops->erase(mtdInfo->storage, mtdInfo->storage->start + instr->addr, instr->len);
		if (res < 0) {
			instr->fail_addr = instr->addr;
			instr->state = MTD_ERASE_FAILED;
		}
		else {
			instr->state = MTD_ERASE_DONE;
		}
	}

	if (instr->callback != NULL) {
		instr->callback(instr);
	}

	return EOK;
}


int mtd_point(struct mtd_info *mtdInfo, off_t from, size_t len, size_t *retlen, void **virt, addr_t *phys)
{
	int ret;
	storage_mtd_t *mtd = mtdInfo->storage->dev->mtd;

	*retlen = 0;
	*virt = NULL;

	if (phys != NULL) {
		*phys = 0;
	}

	if (mtd->ops->point == NULL) {
		ret = -EOPNOTSUPP;
	}
	else if (from < 0 || from >= mtdInfo->size || len > (mtdInfo->size - from)) {
		ret = -EINVAL;
	}
	else if (len == 0) {
		ret = EOK;
	}
	else {
		ret = mtd->ops->point(mtdInfo->storage, mtdInfo->storage->start + from, len, retlen, virt, phys);
	}

	return ret;
}


int mtd_unpoint(struct mtd_info *mtdInfo, off_t from, size_t len)
{
	int ret;
	storage_mtd_t *mtd = mtdInfo->storage->dev->mtd;

	if (mtd->ops->unPoint == NULL) {
		ret = -EOPNOTSUPP;
	}
	else if (from < 0 || from >= mtdInfo->size || len > (mtdInfo->size - from)) {
		ret = -EINVAL;
	}
	else if (len == 0) {
		ret = EOK;
	}
	else {
		ret = mtd->ops->unPoint(mtdInfo->storage, mtdInfo->storage->start + from, len);
	}

	return ret;
}


unsigned long mtd_get_unmapped_area(struct mtd_info *mtdInfo, unsigned long len, unsigned long offset, unsigned long flags)
{
	size_t retlen;
	void *virt;
	int ret;

	ret = mtd_point(mtdInfo, mtdInfo->storage->start + offset, len, &retlen, &virt, NULL);
	if (ret > 0) {
		return ret;
	}

	if (retlen != len) {
		mtd_unpoint(mtdInfo, mtdInfo->storage->start + offset, retlen);
		return -ENOSYS;
	}

	return (unsigned long)virt;
}


int mtd_read(struct mtd_info *mtdInfo, off_t from, size_t len, size_t *retlen, u_char *buf)
{
	ssize_t ret;
	storage_mtd_t *mtd = mtdInfo->storage->dev->mtd;

	*retlen = 0;

	if (from < 0 || from >= mtdInfo->size || len > (mtdInfo->size - from)) {
		ret = -EINVAL;
	}
	else if (mtd->ops->read == NULL) {
		ret = -EOPNOTSUPP;
	}
	else if (len == 0) {
		ret = EOK;
	}
	else {
		ret = mtd->ops->read(mtdInfo->storage, mtdInfo->storage->start + from, buf, len, retlen);
	}

	return ret;
}


int mtd_write(struct mtd_info *mtdInfo, off_t to, size_t len, size_t *retlen, const u_char *buf)
{
	ssize_t ret;
	storage_mtd_t *mtd = mtdInfo->storage->dev->mtd;

	*retlen = 0;
	if (to < 0 || to >= mtdInfo->size || len > (mtdInfo->size - to)) {
		ret = -EINVAL;
	}
	else if (mtd->ops->write == NULL || ((mtdInfo->flags & MTD_WRITEABLE) == 0)) {
		ret = -EROFS;
	}
	else if (len == 0) {
		ret = EOK;
	}
	else {
		ret = mtd->ops->write(mtdInfo->storage, mtdInfo->storage->start + to, buf, len, retlen);
	}

	return ret;
}


static int mtd_check_oob_ops(struct mtd_info *mtd, off_t offs, struct mtd_oob_ops *ops)
{
	int ret = EOK;

	if (ops->datbuf == NULL) {
		ops->len = 0;
	}

	if (ops->oobbuf == NULL) {
		ops->ooblen = 0;
	}

	if (offs < 0 || offs + ops->len > mtd->size) {
		ret = -EINVAL;
	}

	return ret;
}


int mtd_read_oob(struct mtd_info *mtdInfo, off_t from, struct mtd_oob_ops *ops)
{
	ssize_t ret;
	storage_mtd_t *mtd = mtdInfo->storage->dev->mtd;

	ops->retlen = ops->oobretlen = 0;
	if (mtd->ops->meta_read == NULL) {
		ret = -EOPNOTSUPP;
	}
	else {
		ret = mtd_check_oob_ops(mtdInfo, from, ops);
	}

	if (ret >= 0) {
		ret = mtd->ops->meta_read(mtdInfo->storage, mtdInfo->storage->start + from, ops->oobbuf, ops->ooblen, &ops->oobretlen);
	}

	return ret;
}


int mtd_write_oob(struct mtd_info *mtdInfo, off_t to, struct mtd_oob_ops *ops)
{
	int ret;
	storage_mtd_t *mtd = mtdInfo->storage->dev->mtd;

	ops->retlen = ops->oobretlen = 0;
	if (mtd->ops->meta_write == NULL) {
		ret = -EOPNOTSUPP;
	}
	else if ((mtdInfo->flags & MTD_WRITEABLE) == 0) {
		ret = -EROFS;
	}
	else {
		ret = mtd_check_oob_ops(mtdInfo, to, ops);
	}

	if (ret >= 0) {
		ret = mtd->ops->meta_write(mtdInfo->storage, mtdInfo->storage->start + to, ops->oobbuf, ops->ooblen, &ops->oobretlen);
	}

	return ret;
}


int mtd_writev(struct mtd_info *mtdInfo, const struct kvec *vecs, unsigned long count, off_t to, size_t *retlen)
{
	int ret = 0;
	unsigned long i;
	size_t totlen = 0, thislen;

	*retlen = 0;
	if ((mtdInfo->flags & MTD_WRITEABLE) == 0) {
		return -EROFS;
	}

	for (i = 0; i < count; i++) {
		if (vecs[i].iov_len == 0) {
			continue;
		}

		ret = mtd_write(mtdInfo, to, vecs[i].iov_len, &thislen, vecs[i].iov_base);
		totlen += thislen;

		if (ret || thislen != vecs[i].iov_len) {
			break;
		}

		to += vecs[i].iov_len;
	}

	*retlen = totlen;

	return ret;
}


void mtd_sync(struct mtd_info *mtdInfo)
{
	storage_mtd_t *mtd = mtdInfo->storage->dev->mtd;

	if (mtd->ops->sync != NULL) {
		mtd->ops->sync(mtdInfo->storage);
	}
}


int mtd_lock(struct mtd_info *mtdInfo, off_t ofs, uint64_t len)
{
	int ret;
	storage_mtd_t *mtd = mtdInfo->storage->dev->mtd;

	if (ofs < 0 || ofs >= mtdInfo->size || len > (mtdInfo->size - ofs)) {
		ret = -EINVAL;
	}
	else if (mtd->ops->lock == NULL) {
		ret = -EOPNOTSUPP;
	}
	else if (len == 0) {
		ret = EOK;
	}
	else {
		ret = mtd->ops->lock(mtdInfo->storage, mtdInfo->storage->start + ofs, len);
	}

	return ret;
}


int mtd_unlock(struct mtd_info *mtdInfo, off_t ofs, uint64_t len)
{
	int ret;
	storage_mtd_t *mtd = mtdInfo->storage->dev->mtd;

	if (mtd->ops->unLock == NULL) {
		ret = -EOPNOTSUPP;
	}
	else if (ofs < 0 || ofs >= mtdInfo->size || len > (mtdInfo->size - ofs)) {
		ret = -EINVAL;
	}
	else if (len == 0) {
		ret = EOK;
	}
	else {
		ret = mtd->ops->unLock(mtdInfo->storage, mtdInfo->storage->start + ofs, len);
	}

	return ret;
}


int mtd_is_locked(struct mtd_info *mtdInfo, off_t ofs, uint64_t len)
{
	int ret;
	storage_mtd_t *mtd = mtdInfo->storage->dev->mtd;

	if (ofs < 0 || ofs >= mtdInfo->size || len > (mtdInfo->size - ofs)) {
		ret = -EINVAL;
	}
	else if (mtd->ops->isLocked == NULL) {
		ret = -EOPNOTSUPP;
	}
	else if (len == 0) {
		ret = EOK;
	}
	else {
		ret = mtd->ops->isLocked(mtdInfo->storage, mtdInfo->storage->start + ofs, len);
	}

	return ret;
}


int mtd_block_isreserved(struct mtd_info *mtdInfo, off_t ofs)
{
	int ret;
	storage_mtd_t *mtd = mtdInfo->storage->dev->mtd;

	if (ofs < 0 || ofs >= mtdInfo->size) {
		ret = -EINVAL;
	}
	else if (mtd->ops->block_isReserved == NULL) {
		ret = -EOPNOTSUPP;
	}
	else {
		ret = mtd->ops->block_isReserved(mtdInfo->storage, mtdInfo->storage->start + ofs);
	}

	return ret;
}


int mtd_block_isbad(struct mtd_info *mtdInfo, off_t ofs)
{
	int ret;
	storage_mtd_t *mtd = mtdInfo->storage->dev->mtd;

	if (ofs < 0 || ofs >= mtdInfo->size) {
		ret = -EINVAL;
	}
	else if (mtd->ops->block_isBad == NULL) {
		ret = -EOPNOTSUPP;
	}
	else {
		ret = mtd->ops->block_isBad(mtdInfo->storage, mtdInfo->storage->start + ofs);
	}

	return ret;
}


int mtd_block_markbad(struct mtd_info *mtdInfo, off_t ofs)
{
	int ret;
	storage_mtd_t *mtd = mtdInfo->storage->dev->mtd;

	if (ofs < 0 || ofs >= mtdInfo->size) {
		ret = -EINVAL;
	}
	else if (mtd->ops->block_markBad == NULL) {
		ret = EOK;
	}
	else {
		ret = mtd->ops->block_markBad(mtdInfo->storage, mtdInfo->storage->start + ofs);
	}

	return ret;
}


int mtd_suspend(struct mtd_info *mtdInfo)
{
	int ret;
	storage_mtd_t *mtd = mtdInfo->storage->dev->mtd;

	if (mtd->ops->suspend == NULL) {
		ret = -EOPNOTSUPP;
	}
	else {
		ret = mtd->ops->suspend(mtdInfo->storage);
	}

	return ret;
}


void mtd_resume(struct mtd_info *mtdInfo)
{
	storage_mtd_t *mtd = mtdInfo->storage->dev->mtd;

	if (mtd->ops->resume != NULL) {
		mtd->ops->resume(mtdInfo->storage);
	}
}


void *mtd_kmalloc_up_to(const struct mtd_info *mtdInfo, size_t *size)
{
	void *ret;

	if (*size < mtdInfo->writesize) {
		*size = mtdInfo->writesize;
	}

	ret = malloc(*size);

	return ret;
}


int mtd_is_bitflip(int err)
{
	return err == -EUCLEAN;
}
