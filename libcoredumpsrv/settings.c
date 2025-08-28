/*
 * Phoenix-RTOS
 *
 * Coredump Server Library
 *
 * Configuration functions
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jakub Klimek
 *
 * %LICENSE%
 */

#include "coredump.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/msg.h>


static int settings_lookupDev(oid_t *oid)
{
	if (lookup(COREDUMP_SETTINGS_DEV, NULL, oid) < 0) {
		printf("Failed to lookup settings device '%s'\n", COREDUMP_SETTINGS_DEV);
		return -ENOENT;
	}
	return 0;
}


static int settings_nameToAttr(const char *name)
{
	if (strcmp(name, "MAX_THREADS") == 0) {
		return COREDUMP_ATTR_MAX_THREADS;
	}
	else if (strcmp(name, "MAX_STACK_SIZE") == 0) {
		return COREDUMP_ATTR_MAX_STACK_SIZE;
	}
	else if (strcmp(name, "MEM_SCOPE") == 0) {
		return COREDUMP_ATTR_MEM_SCOPE;
	}
	else if (strcmp(name, "FP_CONTEXT") == 0) {
		return COREDUMP_ATTR_FP_CONTEXT;
	}
	else if (strcmp(name, "PRINT") == 0) {
		return COREDUMP_ATTR_PRINT;
	}
	else if (strcmp(name, "PRINT_SLEEP") == 0) {
		return COREDUMP_ATTR_PRINT_SLEEP;
	}
	else if (strcmp(name, "PATH") == 0) {
		return COREDUMP_ATTR_PATH;
	}
	else if (strcmp(name, "MAX_FILES") == 0) {
		return COREDUMP_ATTR_MAX_FILES;
	}
	return -EINVAL;
}


static char *settings_memscopeName(int scope)
{
	switch (scope) {
		case COREDUMP_MEM_NONE:
			return "none";
		case COREDUMP_MEM_EXC_STACK:
			return "exception thread stack";
		case COREDUMP_MEM_ALL_STACKS:
			return "all threads stacks";
		case COREDUMP_MEM_ALL:
			return "all memory";
		default:
			return "invalid";
	}
}


static void settings_read(void)
{
	static const int SAVEPATH_MAX = 128;
	static const size_t OUT_SIZE = sizeof(coredump_opts_t) + SAVEPATH_MAX;
	oid_t oid;
	if (settings_lookupDev(&oid) != 0) {
		return;
	}

	msg_t msg;
	msg.oid = oid;
	msg.type = mtGetAttrAll;
	msg.i.size = 0;
	msg.i.data = NULL;
	msg.o.size = OUT_SIZE;
	msg.o.data = malloc(OUT_SIZE);
	int ret = msgSend(oid.port, &msg);
	if (ret != 0) {
		printf("Failed to read settings: '%s'\n", strerror(-ret));
		free(msg.o.data);
		return;
	}
	if (msg.o.err != 0) {
		printf("Failed to read settings: '%s'\n", strerror(msg.o.err));
		free(msg.o.data);
		return;
	}

	coredump_opts_t *opts = (coredump_opts_t *)msg.o.data;
	if (msg.o.attr.val == sizeof(coredump_opts_t)) {
		opts->savepath = NULL;
	}
	else {
		opts->savepath = (char *)(opts + 1);
	}

	printf("Current settings:\n");
	printf("  Max Threads: %zu\n", opts->maxThreads);
	printf("  Max Stack Size: 0x%zx\n", opts->maxStackSize);
	printf("  Memory Scope: %s (%d)\n", settings_memscopeName(opts->memScope), opts->memScope);
	printf("  FP Context: %s\n", opts->fpContext ? "Enabled" : "Disabled");
	printf("  Max Memory Chunk: %zu\n", opts->maxMemChunk);
	printf("  Print: %s\n", opts->print ? "Enabled" : "Disabled");
	printf("  Print Sleep: %u us\n", opts->printSleep);
	if (opts->savepath == NULL) {
		printf("  Save Path: Disabled\n");
	}
	else {
		printf("  Save Path: %.*s%s\n", SAVEPATH_MAX, opts->savepath, msg.o.attr.val > OUT_SIZE ? "..." : "");
	}
	printf("  Max Files: %zu\n", opts->maxFiles);
	printf("\n");

	free(msg.o.data);
}


static int settings_setOpts(char *opt, char *val, coredump_opts_t *opts)
{
	switch (settings_nameToAttr(opt)) {
		case COREDUMP_ATTR_MAX_THREADS:
			opts->maxThreads = atoi(val);
			break;
		case COREDUMP_ATTR_MAX_STACK_SIZE:
			opts->maxStackSize = atoi(val);
			break;
		case COREDUMP_ATTR_MEM_SCOPE:
			opts->memScope = atoi(val);
			break;
		case COREDUMP_ATTR_FP_CONTEXT:
			opts->fpContext = atoi(val);
			break;
		case COREDUMP_ATTR_PRINT:
			opts->print = atoi(val);
			break;
		case COREDUMP_ATTR_PRINT_SLEEP:
			opts->printSleep = atoi(val);
			break;
		case COREDUMP_ATTR_PATH:
			opts->savepath = val;
			break;
		case COREDUMP_ATTR_MAX_FILES:
			opts->maxFiles = atoi(val);
			break;
		default:
			printf("Unknown setting '%s'\n", opt);
			return -EINVAL;
	}
	return 0;
}


static void settings_setDev(char *opt, char *val)
{
	oid_t oid;
	if (settings_lookupDev(&oid) != 0) {
		return;
	}

	int attr = settings_nameToAttr(opt);
	if (attr < 0) {
		printf("Unknown setting '%s'\n", opt);
		return;
	}

	int value = atoi(val);

	msg_t msg;
	msg.oid = oid;
	msg.type = mtSetAttr;
	msg.i.attr.type = attr;
	if (attr == COREDUMP_ATTR_PATH && *val != '0') {
		msg.i.attr.val = 1;
		msg.i.size = strlen(val) + 1;
		msg.i.data = val;
	}
	else {
		msg.i.attr.val = value;
		msg.i.size = 0;
		msg.i.data = NULL;
	}
	msg.o.size = 0;
	msg.o.data = NULL;
	int ret = msgSend(oid.port, &msg);
	if (ret != 0) {
		printf("Failed to set setting '%s': %s\n", opt, strerror(-ret));
		return;
	}
	if (msg.o.err != 0) {
		printf("Failed to set setting '%s': %s\n", opt, strerror(msg.o.err));
		return;
	}

	if (attr == COREDUMP_ATTR_PATH) {
		printf("Changed '%s' to '%s'\n", opt, val);
	}
	else if (attr == COREDUMP_ATTR_MEM_SCOPE) {
		printf("Changed '%s' to '%s' (%d)\n", opt, settings_memscopeName(value), value);
	}
	else {
		printf("Changed '%s' to '%d'\n", opt, value);
	}
}


static void printHelp(char *cmd, bool config)
{
	if (!config) {
		printf("Usage: %s [config] [options]\n", cmd);
		printf("  %s\tStart coredump daemon with default options\n", cmd);
		printf("  config\tDon't start coredump daemon, just configure running server\n");
	}
	printf("Options:\n");
	printf("  -h, --help\t\tShow this help message\n");
	printf("  -s, --set <name> <value>\tSet coredump server setting\n");
	printf("  -g, --get\tGet current coredump server settings\n");
	printf("  -p, --print-elf <path>\tPrint ELF file information\n");
	printf("Settings:\n");
	printf("  MAX_THREADS, MAX_STACK_SIZE, MEM_SCOPE, FP_CONTEXT, PRINT, PRINT_SLEEP, PATH, MAX_FILES\n");
	printf("Example: %s -s MAX_THREADS 8\n", cmd);
	printf("         %s -s PATH 0\n", cmd);
	printf("         %s -s PATH \"/coredumps\"\n", cmd);
	printf("\n");
}


static int settings_parseOption(char *cmd, int argleft, char *arg[], coredump_opts_t *opts)
{
	if (strcmp(arg[0], "-h") == 0 || strcmp(arg[0], "--help") == 0) {
		printHelp(cmd, opts == NULL);
		return -EAGAIN;
	}

	if (strcmp(arg[0], "-g") == 0 || strcmp(arg[0], "--get") == 0) {
		if (opts != NULL) {
			printf("Error: Cannot use -g/--get when starting server\n");
			return -EINVAL;
		}
		settings_read();
		return 1;
	}

	if (strcmp(arg[0], "-s") == 0 || strcmp(arg[0], "--set") == 0) {
		if (argleft < 2) {
			printf("Error: Missing option name for -s/--set\n");
			return -EINVAL;
		}
		if (argleft < 3) {
			printf("Error: Missing value for option '%s'\n", arg[1]);
			return -EINVAL;
		}
		if (opts != NULL) {
			int ret = settings_setOpts(arg[1], arg[2], opts);
			if (ret < 0) {
				return ret;
			}
		}
		else {
			settings_setDev(arg[1], arg[2]);
		}
		return 3;
	}

	if (strcmp(arg[0], "-p") == 0 || strcmp(arg[0], "--print-elf") == 0) {
		if (argleft < 2) {
			printf("Error: Missing ELF file path for -p/--print-elf\n");
			return -EINVAL;
		}
		if (opts != NULL) {
			printf("Error: Cannot use -p/--print-elf when starting server\n");
			return -EINVAL;
		}
		int ret = coredump_printElf(arg[1]);
		if (ret < 0) {
			return ret;
		}
		return 2;
	}

	printf("Error: Unknown option '%s'\n", arg[0]);
	return -EINVAL;
}


int coredump_configure(char *cmd, int argc, char *argv[])
{
	if (argc < 1) {
		printHelp(cmd, true);
		return 0;
	}

	int i = 0;
	int ret;
	while (i < argc) {
		ret = settings_parseOption(cmd, argc - i, &argv[i], NULL);
		if (ret < 0) {
			return ret;
		}
		i += ret;
	}
	return 0;
}


int coredump_parseStartupArgs(int argc, char *argv[], coredump_opts_t *opts)
{
	int i = 1;

	int ret;
	while (i < argc) {
		ret = settings_parseOption(argv[0], argc - i, &argv[i], opts);
		if (ret < 0) {
			return ret;
		}
		i += ret;
	}
	return 0;
}
