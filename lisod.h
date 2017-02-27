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
#include "log.h"
#include "parse.h"

#define STAGE_INIT             1000
#define STAGE_ERROR            1001
#define STAGE_CLOSE            1002

#define REQ_VALID               1
#define REQ_INVALID             0
#define REQ_PIPE                2

#define DATE_SIZE     35
#define BUFF_SIZE     8192
#define FILETYPE_SIZE 15

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
	int maxfd;
	fd_set read_set;	/* the set of fd that liso server is looking at before recv */
	fd_set read_ready_set;	/* the set of fd that is ready to recv */
	fd_set write_set;
	fd_set write_ready_set;
	int nready;		/* the num of fd that is ready to recv or send */
	int maxi;		/* the max index of fd*/
	int clientfd[FD_SETSIZE];
    Buff *buf[FD_SETSIZE];
	char* www;
} pool;