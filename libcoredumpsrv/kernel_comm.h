/*
 * Phoenix-RTOS
 *
 * Coredump Server Library
 *
 * Kernel communication
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jakub Klimek
 *
 * %LICENSE%
 */

#ifndef _KERNEL_COMM_H_
#define _KERNEL_COMM_H_

#include <unistd.h>
#include <phoenix/coredump.h>
#include <phoenix/msg.h>

extern int coredump_waitForCrash(coredump_general_t *crashInfo);


extern int coredump_getThreadContext(int tid, coredump_thread_t *resp);


extern int coredump_getMemList(size_t bufSize, coredump_memseg_t *resp);


extern int coredump_getRelocs(size_t bufSize, coredump_reloc_t *resp);


extern int coredump_getMemory(void *startAddr, size_t len, msg_t *msg, msg_rid_t *rid);


extern void coredump_putMemory(msg_t *msg, msg_rid_t rid);


extern int coredump_closeCrash(void);

#endif /* _KERNEL_COMM_H_ */
