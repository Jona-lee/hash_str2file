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

#ifndef __HASH_H_
#define __HASH_H_

#define HASH_FILE "hashcommit.txt"

#define NHASH 257
#define MAX_STRLEN 2048

struct hash_tuple {
	char *name;
	char *value;
	unsigned int tuple_lens;
	struct hash_tuple *next;
};

extern unsigned int str_hash_init(const char *fpath);
extern unsigned int str_hash_add(const char *name, const char *value);
extern unsigned int str_hash_del(const char *name);
extern unsigned int str_hash_commit(const char *fpath);
extern unsigned int str_hash_dump(void);
extern unsigned int str_hash_get(const char *name, char *value_ptr, int buf_len);
#endif
