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

#include <stdio.h>
#include <string.h>
#include "hash.h"
#include "hmsg.h"
#include "dlog.h"

int usage()
{
	fprintf(stdout, "usage: hmsg_monitor/hs tools\n"
					" 		hmsg_monitor --start hash message monitor\n"
					"		hs [add/del] [name=value] add name and value to hash table\n"
					"		hs commit save name and value to file\n"
					"		hs get [name] get value of name\n"
	);
}

int main(int argc, char *argv[])
{
	char *cmd;
    int res;

	set_dlog_level (L_DBG);

    if ((cmd = strrchr(argv[0], '/')) == NULL)
        cmd = argv[0];
    else
        cmd++;

	if (strcmp(cmd, "hmsg_monitor") == 0) {
		str_hash_init(HASH_FILE);
		str_hash_dump();
		dlog(L_INFO, "start hmsg_monitor\n");
		hmsg_monitor();
		return 0;
	} else if (strcmp(cmd, "hs") == 0) {
		res = hs(argc, argv);
	} else {
		dlog(L_ERR, "Invalid command\n");
		usage();
		res = -1;
	}

	return res;
}
