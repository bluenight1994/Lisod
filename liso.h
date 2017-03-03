#ifndef _LISO_H_
#define _LISO_H_

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include "log.h"
#include "parse.h"

#define STAGE_INIT             1000
#define STAGE_ERROR            1001
#define STAGE_CLOSE            1002

#define REQ_VALID               1
#define REQ_INVALID             0
#define REQ_PIPE                2

#define DATE_SIZE     35
#define FILETYPE_SIZE 15
#define BUFF_SIZE     8192
#define MIN_LINE      64

typedef struct {
    int maxfd;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    fd_set read_set;
    fd_set ready_set;
	char *www;
} pool;

#endif

