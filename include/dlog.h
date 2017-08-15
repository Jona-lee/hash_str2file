/*
 *
 *  Copyright (c) 2013 javenly@gmail.com
 * 
 *  Mostly socket code based on original code from apue.2e
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __DLOG_H_
#define __DLOG_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#define DEBUG

enum l_level
{
    L_ERR,
    L_WARN,
    L_INFO,
    L_DBG,
};
typedef enum l_level llevel_t;

extern int dlog(llevel_t level, const char *fmt, ...);
extern int set_dlog_level (int level);
#endif

