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
#define FIELD_SIZE    4096
#define MIN_LINE      64

typedef struct headers{
    char *key;
    char *value;
    struct headers *next;
} Headers;

typedef struct requests {
    char *method;
    char *uri;
    char *version;
    Headers *header;
    int valid;
    char *response;
    char *body;
    char *post_body;
    int body_size;
    struct requests *next;
	int pipefd;
} Requests;

typedef struct buff {
    char addr[INET_ADDRSTRLEN];
    char *buf;
    int fd;
    int port;
    unsigned int cur_size;
    unsigned int cur_parsed;
    unsigned int size;
    Requests *cur_request;
    int stage;
    Requests *request;
} Buff;

typedef struct {
    int rio_fd;
    int rio_cnt;
    char *rio_bufptr;
    char rio_buf[BUFF_SIZE];
} rio_t;

typedef struct {
    int maxfd;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    fd_set read_set;
    fd_set ready_set;
    rio_t *clientrio[FD_SETSIZE];
	char *www;
} pool;

typedef struct {
    int content_len;
    char method[FIELD_SIZE];
    char version[FIELD_SIZE];
    char uri[FIELD_SIZE];
    char filename[FIELD_SIZE];
} HTTPContext;

