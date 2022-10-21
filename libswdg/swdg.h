/*
 * Phoenix-RTOS
 *
 * Software watchdog library
 *
 * Copyright 2022 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef SWDG_H_
#define SWDG_H_

#include <time.h>

typedef void (*swdg_callback_t)(int channel);


void swdg_reload(int no);


void swdg_disable(int no);


void swdg_enable(int no);


void swdg_chanConfig(int no, swdg_callback_t callback, time_t limit);


int swdg_init(size_t chanCount, int priority);

#endif
