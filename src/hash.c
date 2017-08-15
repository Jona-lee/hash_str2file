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
#include <strings.h>

#include "hash.h"
#include "dlog.h"

struct hash_tuple *hash_table[NHASH];

static unsigned int _bkdr_hash(const char *str)
{
	unsigned int seed = 31; // the magic number, 31, 131, 1313, 13131, etc.. orz..
	unsigned int hash = 0;
	unsigned char *p = (unsigned char *) str;

	while (*p)
		hash = hash*seed + (*p++);

	return hash;
}

static unsigned int hash(const char *str)
{
	return _bkdr_hash(str);
}


/* return NULL pointer on failure */
struct hash_tuple *strhash_realloc(struct hash_tuple *t, const char *name, const char *value, int *value_changed)
{
	unsigned int size_to_allocate;

	size_to_allocate = sizeof(struct hash_tuple) + strlen(name)+ strlen(value) + 2;

	if (t == NULL) {
		if (!(t = malloc(size_to_allocate)))
			return NULL;
	} else {
		if (strcmp(t->name, name)){
			dlog(L_ERR, "can't override different name %s->%s\n", name, t->name);
			return NULL;
		}
		if(size_to_allocate > t->tuple_lens){
			t = realloc(t, size_to_allocate);
		}
	}

    /* set tuple lens */
	t->tuple_lens = size_to_allocate;

    /* Copy name */
	t->name = (char *) &t[1];
	strcpy(t->name, name);

    /* Copy value */
	t->value = t->name + strlen(name) + 1;
	strcpy(t->value, value);

    if (value_changed != NULL)
        *value_changed = 1;

	return t;
}

static int str_rehash(const char *fpath)
{
	struct hash_tuple *t, *next;
	FILE *hf;
	int i;
	char tmp[MAX_STRLEN];
	char *lineptr = tmp;
	size_t linelen = 0;
	char *name, *value, *eq;
	int res;
	
	/* (Re)initialize hash table, free all the nodes */
	for(i = 0; i < NHASH; i++) {
		for(t = hash_table[i]; t; t = next) {
			next = t->next;
			free(t);
		}
		hash_table[i] = NULL;
	}

	hf = fopen(fpath, "r");
	if (!hf) {
		dlog(L_ERR, "can't open or create file %s\n", fpath);
		return -2;
	}
	
	linelen = MAX_STRLEN;
	while ((res = getline(&lineptr, &linelen, hf)) != -1) {
        if (res > MAX_STRLEN)
			dlog(L_WARN, "hash strings with length %d > Max:%d\n", res, MAX_STRLEN);
		/* Parse and set "name=value\0 ... \0\0" */
		name = lineptr;
		if (!(eq = strchr(name, '=')))
			continue;
		*eq = '\0';
		value = eq + 1;
		if(*(value+strlen(value)-1) == '\n')
			*(value+strlen(value)-1) = '\0';
		str_hash_add(name, value);
		*eq = '='; /* restore it back */
	}
	fclose(hf);
	return 0;
}

unsigned int str_hash_init(const char *fpath)
{
	return str_rehash(fpath);
}

unsigned int str_hash_add(const char *name, const char *value)
{
	unsigned int i;
	struct hash_tuple *t, *u, *v, **prev;
	int offset=0;
    int istmp = 0;
    int aclset = 0;
    int value_changed = 0;
	
	if (!name)
		return -1;
		
	/* Hash the strings */
	i = hash(name) % NHASH;

	/* Find the associated tuple in the hash table */
	for (prev = &hash_table[i], t = v = *prev;
		t && strcmp(t->name, name);
		prev = &t->next, v = t, t = *prev);

	if (!value) {
		/* if the value is null, we delete it's node*/
		if(!t) {
			/* name not find in hash table.*/
			return 0;
	    }
		
		v->next = t->next;
		if (hash_table[i] == t) {
			dlog(L_DBG, "tabe index %d fist node delete...\n", i);
			hash_table[i] = NULL;
		}
		free(t);
		return 0;
	}

	/* (Re)allocate tuple, if t is NULL, we will allocate new, otherwise try to reuse it if possible */
	if (!(u = strhash_realloc(t, name, value, &value_changed)))
		return -ENOMEM;

	/* reallocated */
	if (t && t == u) /* same node used */
		return 0;

	/* Add new tuple to the hash table */
	u->next = hash_table[i];
	hash_table[i] = u;

	return 0;
}

unsigned int str_hash_get(const char *name, char *value_ptr, int buf_len)
{
	unsigned int i;
	struct hash_tuple *t, *u, *v, **prev;
	int offset=0;
    int istmp = 0;
    int aclset = 0;
    int value_changed = 0;
	
	if (!name)
		return -1;
		
	/* Hash the strings */
	i = hash(name) % NHASH;

	/* Find the associated tuple in the hash table */
	for (prev = &hash_table[i], t = v = *prev;
		t && strcmp(t->name, name);
		prev = &t->next, v = t, t = *prev);

	if (!t || !t->value) {
		dlog(L_DBG, "NULL\n");
		return -1;
	}

	if (buf_len > strlen(t->value) + 1)
		strncpy(value_ptr, t->value, strlen(t->value) + 1);
	else 
		strncpy(value_ptr, t->value, buf_len);
	
	return 0;
}

unsigned int str_hash_dump(void)
{
	struct hash_tuple *t;
	int i;

	dlog(L_DBG, "=================string hash dump begin==============\n");
	for(i = 0; i < NHASH; i++) {
		for(t = hash_table[i]; t; t = t->next) {
			fprintf(stdout, "%s=%s %s ", t->name, t->value,
				(t->next != NULL)? "-->" : "-->NULL");
		}
		if(hash_table[i])
			fprintf(stdout, "\n");
	}
	dlog(L_DBG, "=================string hash dump end==============\n");

	return 0;
}

unsigned int str_hash_del(const char *name)
{
     return str_hash_add(name, NULL);
}

unsigned int str_hash_commit(const char *fpath)
{
	struct hash_tuple *t;
	FILE *hf;
	int i;

	if(!fpath)
		return -1;

	hf = fopen(fpath, "w+");
	if (!hf) {
		dlog(L_ERR, "can't open or create file %s\n", fpath);
		return -2;
	}

	for(i = 0; i < NHASH; i++) {
		for(t = hash_table[i]; t; t = t->next) {
			fprintf(hf, "%s=%s\n", t->name, t->value);
		}
	}
	fclose(hf);
	return 0;
}

