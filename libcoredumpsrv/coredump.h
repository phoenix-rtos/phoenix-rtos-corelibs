/*
 * Phoenix-RTOS
 *
 * Coredump Server Library
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jakub Klimek
 *
 * %LICENSE%
 */

#ifndef _COREDUMP_H_
#define _COREDUMP_H_

#include <sys/types.h>

#define COREDUMP_SETTINGS_DEV "/dev/coredumpctrl"


enum {
	COREDUMP_ATTR_MAX_THREADS,    /* Maximum number of threads in coredump */
	COREDUMP_ATTR_MAX_STACK_SIZE, /* Maximum stack size for each thread */
	COREDUMP_ATTR_MEM_SCOPE,      /* Memory scope for coredump */
	COREDUMP_ATTR_FP_CONTEXT,     /* Save floating point context */
	COREDUMP_ATTR_PRINT,          /* Print coredump to console */
	COREDUMP_ATTR_PRINT_SLEEP,    /* Sleep time between printing coredump chunks */
	COREDUMP_ATTR_PATH,           /* Path to save coredumps */
	COREDUMP_ATTR_MAX_FILES,      /* Maximum number of coredump files to keep */
};

enum {
	COREDUMP_MEM_NONE,
	COREDUMP_MEM_EXC_STACK,
	COREDUMP_MEM_ALL_STACKS,
	COREDUMP_MEM_ALL
};


typedef struct {
	size_t maxThreads;   /* zero for unlimited */
	size_t maxStackSize; /* zero for unlimited */
	int memScope;
	unsigned int fpContext : 1;

	size_t maxMemChunk; /* zero for unlimited */

	unsigned int print : 1;
	useconds_t printSleep;
	char *savepath;
	size_t maxFiles;
} coredump_opts_t;


extern int coredump_serverthr(coredump_opts_t *opts);


extern int coredump_configure(char *cmd, int argc, char *argv[]);


extern int coredump_parseStartupArgs(int argc, char *argv[], coredump_opts_t *opts);


extern int coredump_printElf(char *path);

#endif /* _COREDUMP_H_ */
