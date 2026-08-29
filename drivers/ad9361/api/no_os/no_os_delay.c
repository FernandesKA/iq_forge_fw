/*******************************************************************************
 *   @file   no_os_delay.c
 *   @brief  Implementation of delay functions for Linux userspace.
*******************************************************************************/

#include "no_os_delay.h"

#include <time.h>
#include <unistd.h>

void no_os_udelay(uint32_t usecs)
{
	usleep(usecs);
}

void no_os_mdelay(uint32_t msecs)
{
	usleep(msecs * 1000);
}

struct no_os_time no_os_get_time(void)
{
	struct timespec ts;
	struct no_os_time time = {0, 0};

	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
		time.s = (unsigned int)ts.tv_sec;
		time.us = (unsigned int)(ts.tv_nsec / 1000);
	}

	return time;
}
