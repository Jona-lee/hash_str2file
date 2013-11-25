/*
 *
 *  Copyright (c) 2013 javenly@gmail.com
 * 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include "dlog.h"

llevel_t dlog_level = L_ERR;

int set_dlog_level (int level)
{
	if(level > L_DBG)
		dlog_level = L_DBG;
	else if(level < L_ERR)
		dlog_level = L_ERR;
	else
		dlog_level = level;
	
	return 0;
}

int dlog(llevel_t level, const char *fmt, ...)
{
    int off = 0;
    char buf[1024] = "";
    int size = 0;
    va_list va;
    struct tm tm;
    struct timeval tv;

    if (level > dlog_level) {
        return 0;
    }

    size = sizeof(buf);

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm);

    off = snprintf(buf, size, "<%d> ", level);
    off += strftime(buf+off, size-off, "<%Y-%m-%d %H:%M:%S> ", &tm);

    va_start(va, fmt);
    vsnprintf(buf+off, size-off, fmt, va);
    va_end(va);

    fprintf(stdout, "%s", buf);
    return 0;
}

