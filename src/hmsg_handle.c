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
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stddef.h>
#include <fcntl.h>
#include <time.h>

#include "dlog.h"
#include "hmsg.h"
#include "hash.h"

#define QLEN    10
#define OPEN_MAX_GUESS  256
#define MSG_PATH        "/tmp/.hmsg_path"
#define NALLOC  10      /* # client structs to alloc/realloc for */
#define MAXLINE 4096
#define STALE   30  /* client's name can't be older than this (sec) */

#define CLI_PATH    "/tmp/"     /* +5 for pid = 14 chars */
#define CLI_PERM    S_IRWXU         /* rwx for user only */


Client   *client;        /* ptr to malloc'ed array */
int       client_size;   /* # entries in client[] array */

static long open_max(void)
{
	int openmax = 0;

	errno = 0;
	if ((openmax = sysconf(_SC_OPEN_MAX)) < 0) {
	if (errno == 0)
		openmax = OPEN_MAX_GUESS;
	else
		dlog(L_ERR, "sysconf error for _SC_OPEN_MAX");
	}

	return(openmax);
}

/* alloc more entries in the client[] array */
static void client_alloc(void)
{
    int  i;

    if (client == NULL)
        client = malloc(NALLOC * sizeof(Client));
    else
        client = realloc(client, (client_size+NALLOC)*sizeof(Client));

    if (client == NULL)
        dlog(L_ERR, "can't alloc for client array");

    /* initialize the new entries */
    for (i = client_size; i < client_size + NALLOC; i++)
        client[i].fd = -1;  /* fd of -1 means entry available */

    client_size += NALLOC;
}

/*
* Called by loop() when connection request from a new client arrives.
*/
static int client_add(int fd, uid_t uid)
{
	int i;

	if (client == NULL)     /* first time we're called */
		client_alloc();
again:
	for (i = 0; i < client_size; i++) {
		if (client[i].fd == -1) {   /* find an available entry */
			client[i].fd = fd;
			client[i].uid = uid;
			return(i);  /* return index in client[] array */
		}
	}

	/* client array full, time to realloc for more */
	client_alloc();
	goto again;     /* and search again (will work this time) */
}

/*
* Called by loop() when we're done with a client.
*/
static void client_del(int fd)
{
	int i;

	for (i = 0; i < client_size; i++) {
		if (client[i].fd == fd) {
			client[i].fd = -1;
			return;
		}
	}
	dlog(L_INFO, "can't find client entry for fd %d", fd);
}


#define MAXARGC     50  /* max number of arguments in buf */
#define WHITE   " \t\n" /* white space for tokenizing arguments */

/*
 * buf[] contains white-space-separated arguments.  We convert it to an
 * argv-style array of pointers, and call the user's function (optfunc)
 * to process the array.  We return -1 if there's a problem parsing buf,
 * else we return whatever optfunc() returns.  Note that user's buf[]
 * array is modified (nulls placed after each token).
 */
static int buf_args(char *buf, int clifd, int (*optfunc)(int, int, char **))
{
	char *ptr, *argv[MAXARGC];
	int argc;

	if (strtok(buf, WHITE) == NULL)     /* an argv[0] is required */
		return(-1);
	argv[argc = 0] = buf;
	while ((ptr = strtok(NULL, WHITE)) != NULL) {
		if (++argc >= MAXARGC-1)    /* -1 for room for NULL at end */
		return(-1);
		argv[argc] = ptr;
	}   
	argv[++argc] = NULL;

	/*  
	* Since argv[] pointers point into the user's buf[],
	* user's function can just copy the pointers, even
	* though argv[] array will disappear on return.
	*/
	return((*optfunc)(clifd, argc, argv));
}

static int serv_send_ack(int fd, int errcode)
{
	char ackbuf[sizeof(serv_msg_head) + 4];
    serv_msg_head *pmsg_hdr;
	int ack_len = sizeof(serv_msg_head) + 4;
	int *pmsg_data;

	pmsg_hdr = (serv_msg_head *)&ackbuf[0];
	pmsg_hdr->type = T_ACK;
	pmsg_hdr->len = 4;
	pmsg_data = (int *)&(ackbuf[sizeof(serv_msg_head)]);
	*pmsg_data = errcode;

	if (write(fd, &ackbuf, ack_len) != ack_len) {    /* send the error message */
		dlog(L_ERR, "send ack failed\n");
		return(-1);
	}

	return(0);
}

static int serv_send_value(int fd, char *value)
{
	char *sendbuf;
	int sendbuf_len = 0;
	serv_msg_head *pmsg_hdr;
	char *pmsg_data;

    sendbuf = malloc(sizeof(serv_msg_head) + strlen(value) + 1);
	if(!sendbuf){
		dlog(L_ERR, "serv_send_value: can not alloc send buffer\n");
		return -ENOMEM;
	}
	
	pmsg_hdr = (serv_msg_head *)sendbuf;
	pmsg_hdr->type = T_VALUE;
	pmsg_hdr->len = strlen(value) + 1;
	pmsg_data = &sendbuf[sizeof(serv_msg_head)];
	strcpy(pmsg_data, value);

	sendbuf_len = pmsg_hdr->len + sizeof(serv_msg_head);

	dlog(L_DBG, "send value type %d len %d value %s\n",pmsg_hdr->type, pmsg_hdr->len, pmsg_data);

	if (write(fd, sendbuf, sendbuf_len) != sendbuf_len) {    /* send the error message */
		dlog(L_ERR, "send ack failed\n");
		return(-1);
	}

	return(0);
}


static int msg_arg_handle(int clifd, int argc, char *argv[])
{
    char *name;
    char *eq;
    char *value;
	char valbuf[MAX_STRLEN];
    int rval;

	dlog(L_INFO, "get msg argc %d\n", argc);
	
	if (1 == argc) {
		if (!strcmp(argv[0], "commit")) {
			return str_hash_commit(HASH_FILE);
		}
	} else if (2 == argc) {
		name = argv[1];
		if (!strcmp(argv[0], "add")) {
			if (!(eq = strchr(name, '=')))
				return -2;
			*eq = '\0';
			value = eq + 1;
			rval = str_hash_add(name, value);
		} else if (!strcmp(argv[0], "del")) {
			rval = str_hash_del(name);
		} else if (!strcmp(argv[0], "get")) {
			rval = str_hash_get(name, valbuf, MAX_STRLEN);
			if (serv_send_value(clifd, valbuf)) {
				dlog(L_ERR, "send value %s of %s error", valbuf, name);
				return -1;
			}
		}
	} else {
		rval = -3;
	}

	return rval;
}

static void msg_parse_arg(char *buf, int nread, int clifd, uid_t uid)
{
    int newfd;
    int res;

    if (buf[nread-1] != 0) {
        dlog(L_ERR, "request from uid %d not null terminated: %*.*s\n",
          uid, nread, nread, buf);
        serv_send_ack(clifd, -1);
        return;
    }

    dlog(L_INFO, "request: %s, from uid %d\n", buf, uid);

    /* parse the arguments, set options */
    if ((res = buf_args(buf, clifd, msg_arg_handle)) < 0) {
        serv_send_ack(clifd, res);
		dlog(L_ERR, "parse arg from uid %d error\n", uid);
        return;
    }

	serv_send_ack(clifd, 0);

}

static int msg_listen(const char *name)
{
	int fd, len, err, rval;
	struct sockaddr_un  un;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return(-1);

	unlink(name);

	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, name);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(name);

	if (bind(fd, (struct sockaddr *)&un, len) < 0) {
		rval = -2;
		goto errout;
	}

	if (listen(fd, QLEN) < 0) {
		rval = -3;
		goto errout;
	}
	return(fd);

errout:
	err = errno;
	close(fd);
	errno = err;
	return(rval);
}

/*
 * Wait for a client connection to arrive, and accept it.
 * We also obtain the client's user ID from the pathname
 * that it must bind before calling us.
 * Returns new fd if all OK, <0 on error
 */
static int serv_accept(int listenfd, uid_t *uidptr)
{
	int	clifd, len, err, rval;
	time_t	staletime;
	struct sockaddr_un	un;
	struct stat	statbuf;

	len = sizeof(un);
	if ((clifd = accept(listenfd, (struct sockaddr *)&un, &len)) < 0)
		return(-1);		/* often errno=EINTR, if signal caught */

	/* obtain the client's uid from its calling address */
	len -= offsetof(struct sockaddr_un, sun_path); /* len of pathname */
	un.sun_path[len] = 0;			/* null terminate */

	if (stat(un.sun_path, &statbuf) < 0) {
		rval = -2;
		goto errout;
	}
#ifdef	S_ISSOCK	/* not defined for SVR4 */
	if (S_ISSOCK(statbuf.st_mode) == 0) {
		rval = -3;		/* not a socket */
		goto errout;
	}
#endif
	if ((statbuf.st_mode & (S_IRWXG | S_IRWXO)) ||
		(statbuf.st_mode & S_IRWXU) != S_IRWXU) {
		  rval = -4;	/* is not rwx------ */
		  goto errout;
	}

	staletime = time(NULL) - STALE;
	if (statbuf.st_atime < staletime ||
		statbuf.st_ctime < staletime ||
		statbuf.st_mtime < staletime) {
		  rval = -5;	/* i-node is too old */
		  goto errout;
	}

	if (uidptr != NULL)
		*uidptr = statbuf.st_uid;	/* return uid of caller */
	unlink(un.sun_path);		/* we're done with pathname now */
	return(clifd);

errout:
	err = errno;
	close(clifd);
	errno = err;
	return(rval);
}

/*
 * Create a client endpoint and connect to a server.
 * Returns fd if all OK, <0 on error.
 */
static int _cli_conn(const char *name)
{
    int fd, len, err, rval;
    struct sockaddr_un  un;

    /* create a UNIX domain stream socket */
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        return(-1);

    /* fill socket address structure with our address */
    memset(&un, 0, sizeof(un));
    un.sun_family = AF_UNIX;
    sprintf(un.sun_path, "%s%05d", CLI_PATH, getpid());
    len = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);

    unlink(un.sun_path);        /* in case it already exists */
    if (bind(fd, (struct sockaddr *)&un, len) < 0) {
        rval = -2;
        goto errout;
    }
    if (chmod(un.sun_path, CLI_PERM) < 0) {
        rval = -3;
        goto errout;
    }

    /* fill socket address structure with server's address */
    memset(&un, 0, sizeof(un));
    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, name);
    len = offsetof(struct sockaddr_un, sun_path) + strlen(name);
    if (connect(fd, (struct sockaddr *)&un, len) < 0) {
        rval = -4;
        goto errout;
    }
    return(fd);

errout:
    err = errno;
    close(fd);
    errno = err;
    return(rval);
}

static int cli_conn(void)
{
	return _cli_conn(MSG_PATH);
}

static int cli_disconn(int fd)
{
	close(fd);
}

static int cli_send_cmd(int fd, char *cmd)
{
	if (write(fd, cmd, strlen(cmd) + 1) != strlen(cmd) + 1)    /* send the error message */
		return(-1);

	return(0);
}

#define MAXACKLEN 512
/* timeout(us)*/
static int cli_recv_server_msg(int fd, int timeout)
{
    fd_set fds;
    int max_fd;
    struct timeval tv;  
	char buf[MAXACKLEN];
	serv_msg_head *pmsg_hdr;

    int rval = 0;
    int nread;
    int getack = 0;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    max_fd = fd;

    tv.tv_sec = 0;
    tv.tv_usec = timeout;
    rval = select(max_fd+1, &fds, NULL, NULL, &tv);
    if (rval < 0) 
		return rval;
    else if (0 == rval) {
        return rval;
    }

    if (!FD_ISSET(fd, &fds)) {   
        return 0;
    }

    if ((nread = read(fd, buf, MAXLINE)) < 0) {
		dlog(L_ERR, "cli_recv_ack:read error on fd %d", fd);
	} else if (nread == 0) {
	    if (0 == getack)
			dlog(L_ERR, "NO ack received\n");
	} else {        /* process client's request */
		if (nread < sizeof(serv_msg_head))
			dlog(L_ERR, "Invalid ack received\n");

		pmsg_hdr = (serv_msg_head *)&buf[0];
		int ii = 0;

		if ((pmsg_hdr->len > (MAXLINE - sizeof(serv_msg_head)))||
			(pmsg_hdr->type > T_VALUE) ||
			(pmsg_hdr->type < T_ACK)){
			rval = -1;
			dlog(L_ERR, "Invalid ack received\n");
		} else {
			dlog(L_DBG, "Get server message type %d, data len %d\n",
				pmsg_hdr->type, pmsg_hdr->len);
			if (pmsg_hdr->type == T_VALUE) {
				dlog(L_DBG, "GET value %s\n", &buf[sizeof(serv_msg_head)]);
				rval = 0;
			} else {
				rval = *(int *)&buf[sizeof(serv_msg_head)];
			}
		}
	}
    return rval;
}

void hmsg_monitor(void)
{
	int i, maxi, listenfd, clifd, nread;
	char buf[MAXLINE];
	uid_t uid;
	struct pollfd *pollfd;

	if ((pollfd = malloc(open_max() * sizeof(struct pollfd))) == NULL)
		dlog(L_ERR, "malloc error");

	/* obtain fd to listen for client requests on */
	if ((listenfd = msg_listen(MSG_PATH)) < 0)
		dlog(L_ERR, "msg_listen error");

	client_add(listenfd, 0);    /* we use [0] for listenfd */
	pollfd[0].fd = listenfd;
	pollfd[0].events = POLLIN;
	maxi = 0;

	for ( ; ; ) {
		if (poll(pollfd, maxi + 1, -1) < 0)
			dlog(L_ERR, "poll error");

		if (pollfd[0].revents & POLLIN) {
			/* accept new client request */
			if ((clifd = serv_accept(listenfd, &uid)) < 0)
				dlog(L_ERR, "serv_accept error: %d", clifd);

			i = client_add(clifd, uid);
			pollfd[i].fd = clifd;
			pollfd[i].events = POLLIN;
			if (i > maxi)
				maxi = i;
			dlog(L_INFO, "new connection: uid %d, fd %d\n", uid, clifd);
		}

		for (i = 1; i <= maxi; i++) {
			if ((clifd = client[i].fd) < 0)
				continue;
			if (pollfd[i].revents & POLLHUP) {
				goto hungup;
			} else if (pollfd[i].revents & POLLIN) {
			/* read argument buffer from client */
				if ((nread = read(clifd, buf, MAXLINE)) < 0) {
					dlog(L_ERR, "read error on fd %d\n", clifd);
				} else if (nread == 0) {
hungup:
					dlog(L_INFO, "closed: uid %d, fd %d\n",
						client[i].uid, clifd);
					client_del(clifd);  /* client has closed conn */
					pollfd[i].fd = -1;
					close(clifd);
				} else {        /* process client's request */
					msg_parse_arg(buf, nread, clifd, client[i].uid);
				}
			}
		}
	}
}

static int hs_usage()
{
	fprintf(stdout, "Usage: hs [action] [strings]"
					"	hs [add/del] [name=value] add name and value to hash table\n"
					"	hs commit save name and value to file\n"
					"	hs get [name] get value of name\n"
	);
}

int hs(int argc, char **argv)
{
	int serv_fd = -1;
	char *sendbuf;
	int res;
	
	serv_fd = cli_conn();
	
	if(argc < 2) {
		hs_usage();
		res = -1;
		goto failed;
	} else if ((!strcmp(argv[1], "add"))||
				(!strcmp(argv[1], "del"))||
				(!strcmp(argv[1], "get"))) {
		if(argc < 3) {
			hs_usage();
			res = -1;
			goto failed;
		}
	
		/* send cmd to hash message server */
		sendbuf = malloc(4 + strlen(argv[2]) + 1);
		if(!sendbuf) {
			dlog(L_ERR, "malloc send buffer error\n");
			res = -ENOMEM;
			goto failed;
		}
		sprintf(sendbuf, "%s ", argv[1]);
		sprintf(sendbuf + 4, "%s", argv[2]);
	
		dlog(L_DBG, "send cmd: [%s] ...\n", sendbuf);
		
		if(cli_send_cmd(serv_fd, sendbuf))
			dlog(L_ERR, "send cmd:[%s %s] error\n", argv[1], argv[2]);
		
		free(sendbuf);
	} else if ((!strcmp(argv[1], "commit"))){
		dlog(L_DBG, "send cmd: [%s] ...\n", argv[1]);
		if(cli_send_cmd(serv_fd, argv[1]))
			dlog(L_ERR, "send command: [commit] error\n");
	} else {
		dlog(L_ERR, "invalid command\n");
		res = -1;
		goto failed;
	}
	if(!strcmp(argv[1], "get")){
		res = cli_recv_server_msg(serv_fd, 100000);
	}
	res = cli_recv_server_msg(serv_fd, 100000);
	dlog(L_DBG, "get server ack %d\n", res);
	
failed:
	if(serv_fd !=-1 )
		cli_disconn(serv_fd);
	return res;
}
