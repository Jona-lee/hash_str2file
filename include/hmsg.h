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

#ifndef __HMSG_H_
#define __HMSG_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {    /* one Client struct per connected client */
	int   fd;         /* fd, or -1 if available */
	uid_t uid;
} Client;

typedef enum {
	T_ACK,
	T_VALUE,
} serv_msg_t;

typedef struct {
	serv_msg_t   type;         
	unsigned int len;
} serv_msg_head;

extern void hmsg_monitor(void);
extern int hs(int argc, char **argv);

#endif
