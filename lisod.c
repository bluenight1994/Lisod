/********************************************************************************
 *    Documentation:                                                            *
 *                                                                              *
 *    select() give the server the power to monitor several sockets             *
 *    at the same time.                                                         *
 *    will tell which ones are ready for reading, which are ready for           * 
 *    writing and which sockets have raised exceptions.                         *        
 *                                                                              *
 *    int select(int numdfs, fd_set *readfds, fd_set *writefds,                 *
 *			fd_set *exceptfds, struct timeval *timeout);                        *
 *                                                                              *
 *    When select() returns, readfds will be modified to reflect                *
 *    which of the file descriptors you selected which is ready for reading.    *
 *                                                                              *
 *                                                                              *
 *                                                                              *
 ********************************************************************************/
 
#include "lisod.h"

#define ARG_NUMBER 3
#define LISTENQ 1024

void usage();
int  open_listen_socket(int port);
int  close_socket(int sock);
void init_pool(int listenfd, pool *p);
void add_client(int newfd, pool *p, struct sockaddr_in *cli_addr, int port);
void handle_clients(int listen, pool *p);
void process_request(int i, pool *p);
int  is_valid_method(char *method);
int  parse_header(int clientfd, pool *p, HTTPContext *context);
void get_time(char *date);
int  parse_uri(pool *p, char *uri, char* filename);
void serve_static(Buff *buff, char *filename, struct stat sbuf);
void serve_error(int client_fd, char *errnum, char *shortmsg, char *longmsg);
void serve_get(int client_fd, HTTPContext *context);
void serve_head(int client_fd, HTTPContext *context);
int  serve_body(int client_fd, HTTPContext *context);
void serve_post(int client_fd, HTTPContext *context);

void get_filetype(char *filename, char *filetype);

// I/O helper method
void rio_readinitb(rio_t *rp, int fd);
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n);

FILE *fp;

int main(int argc, char* argv[]) 
{
    int listen_sock;
    int newfd;
    struct sockaddr cli_addr;
    socklen_t addrlen;
    char* logfile;
    char* lockfile;

    int http_port;

    if (argc != ARG_NUMBER + 1) {
        usage();
    }

    http_port = atoi(argv[1]);
    logfile = argv[2];
//	lockfile = argv[3];

    fp = open_log(logfile);

    static pool pool;
    fprintf(stdout, "------ Echo Server ------\n");
    Log(fp, "Start Lisod Server\n");

    listen_sock = open_listen_socket(http_port);
    init_pool(listen_sock, &pool);

    pool.www = argv[3];

    while (1) {
        pool.ready_set = pool.read_set;
        pool.nready = select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);
        if (pool.nready == -1) {
            fprintf(stderr, "select error\n");
            return EXIT_FAILURE;
        }
        /* listen discriptor ready */
        if (FD_ISSET(listen_sock, &pool.ready_set)) {
            addrlen = sizeof cli_addr;
            newfd = accept(listen_sock, (struct sockaddr *)&cli_addr, &addrlen);

            if (newfd == -1) {
                fprintf(stderr, "establishing new connection error\n");
                break;
            } else {
                /* add new client to pool */
                fcntl(newfd, F_SETFL, O_NONBLOCK);
                add_client(newfd, &pool, (struct sockaddr_in *) &cli_addr, http_port);
            }
        }
        handle_clients(listen_sock, &pool);
    }

    close(listen_sock);

    return EXIT_SUCCESS;
}

void usage(void) {
    fprintf(stderr, "usage: ./lisod <HTTP port> <HTTPS port> <log file> "
        "<lock file> <www folder> <CGI script path> <private key file> "
        "<certificate file>\n");
    exit(EXIT_FAILURE);
}

int close_socket(int sock) {
    if (close(sock)) {
        fprintf(stderr, "Failed closing socket.\n");
        Log(fp, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

int  open_listen_socket(int port) {
    int sock;
    struct sockaddr_in addr;

    int yes = 1;

    /* create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        Log(fp, "Fail to create socket\n");
        return EXIT_FAILURE;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        Log(fp, "Fail to set socketopt\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr))) {
        close_socket(sock);
        Log(fp, "Fail to bing socket\n");
        return EXIT_FAILURE;
    }

    if (listen(sock, LISTENQ)) {
        close_socket(sock);
        Log(fp, "Fail to listen on socket\n");
        return EXIT_FAILURE;
    }

    return sock;
}

// init of client pool

void init_pool(int listenfd, pool *p) {
    int i;
    p->maxi = -1;
    for (i = 0; i < FD_SETSIZE; i++)
        p->clientfd[i] = -1;

    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

// add client to pool

void add_client(int newfd, pool *p, struct sockaddr_in *cli_addr, int port) {
    int i;
    p->nready--;

    for (i = 0; i < (FD_SETSIZE - 20); i++)
    {
        if (p->clientfd[i] < 0)
        {
            p->clientfd[i] = newfd;
            rio_readinitb(&p->clientrio[i], newfd);
            FD_SET(newfd, &p->read_set);
            if (newfd > p->maxfd)
                p->maxfd = newfd;
            if (i > p->maxi)
                p->maxi = i;
            break;
        }
    }
}

void handle_clients(int listen, pool *p) {
    int i, curfd;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        curfd = p->clientfd[i];

        if ((curfd > 0) && (FD_ISSET(curfd, &p->ready_set))) {
            p -> nready--;
            process_request(i, p);
        }
    }
}

void process_request(int i, pool *p) {

    HTTPContext *context = (HTTPContext *)calloc(1, sizeof(HTTPContext));

    int nbytes;
    char filename[BUFF_SIZE];
    struct stat sbuf;
    char buf[BUFF_SIZE];

    int curfd = p->clientfd[i];

    if ((nbytes = recv(curfd, buf, BUFF_SIZE, 0)) <= 0) {
        if (nbytes == 0) {
            Log(fp, "sock %d hung up\n", curfd);
            printf("sock %d hung up\n", curfd);
        }
        else
            Log(fp, "Error occurred when receiving data from client\n");
        FD_CLR(curfd, &p->read_set);
        p->clientfd[i] = -1;
        if (close_socket(curfd))
            Log(fp, "Error occurred when trying to close client socket %d\n", curfd);
        return;
    } else {
        Log(fp, "Server received %d bytes data on %d\n", (int)nbytes, curfd);
        Request *request = parse(buf, nbytes, curfd);
//        bufi->cur_request->method  = (char *)malloc(strlen(request->http_method)+1);
//        bufi->cur_request->version = (char *)malloc(strlen(request->http_version)+1);
//        bufi->cur_request->uri     = (char *)malloc(strlen(request->http_uri)+1);
        strcpy(context->method, request->http_method);
        strcpy(context->version, request->http_version);
        strcpy(context->uri, request->http_uri);

        if (!is_valid_method(request->http_method))
        {
            serve_error(curfd, "501", "Not Implemented", "The method is not implemented by the server");
            goto Done;
        }

        if (strcasecmp(context->version, "HTTP/1.1"))
        {
            serve_error(curfd, "505", "HTTP Version not supported", "HTTP/1.0 is not supported by Liso server");
            goto Done;
        }

        parse_uri(p, request->http_uri, filename);
        strcpy(context->filename, filename);

        if (stat(context->filename, &sbuf) < 0) {
            serve_error(curfd, "404", "Not Found", "File not found on Liso Server");
            goto Done;
        }

        if (!strcasecmp(context->method, "GET"))
            serve_get(curfd, context);
        if (!strcasecmp(context->method, "POST"))
            serve_post(curfd, context);
        if (!strcasecmp(context->method, "HEAD"))
            serve_head(curfd, context);

        Done:
        free(context);
        Log(fp, "Process request finished.\n");
    }
}

int is_valid_method(char *method) {
    if (!strcasecmp(method, "GET")) {
        return 1;
    } else if (!strcasecmp(method, "POST")) {
        return 1;
    } else if (!strcasecmp(method, "HEAD")) {
        return 1;
    }
    return 0;
}

void get_time(char *date) {
    time_t t;
    struct tm *tmp;
    t = time(NULL);
    tmp = localtime(&t);
    strftime(date, DATE_SIZE, "%a, %d %b %Y %T %Z", tmp);
}

int parse_uri(pool *p, char *uri, char* filename) {
    if (!strstr(uri, "/cgi/")) {
        strcpy(filename, p->www);
        strcat(filename, uri);
        if (uri[strlen(uri)-1] == '/') {
            strcat(filename, "index.html");
        }
    } else {
        // TODO: cgi
    }
}

void rio_readinitb(rio_t *rp, int fd)
{
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}


static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;

    while (rp->rio_cnt <= 0) {  /* refill if buf is empty */
        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));
        if (rp->rio_cnt < 0) {
            if (errno != EINTR) /* interrupted by sig handler return */
                return -1;
        }
        else if (rp->rio_cnt == 0)  /* EOF */
            return 0;
        else
            rp->rio_bufptr = rp->rio_buf; /* reset buffer ptr */
    }

    /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
    cnt = n;
    if (rp->rio_cnt < n)
        cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}

void serve_error(int client_fd, char *errnum, char *shortmsg, char *longmsg) {
    struct tm tm;
    time_t now;
    char buf[BUFF_SIZE], body[BUFF_SIZE], dbuf[BUFF_SIZE];

    now = time(0);
    tm = *gmtime(&now);
    strftime(dbuf, DATE_SIZE, "%a, %d %b %Y %H:%M:%S %Z", &tm);

    // build HTTP response body
    sprintf(body, "<html><title>Lisod Error</title>");
    sprintf(body, "%s<body>\r\n", body);
    sprintf(body, "%sError %s -- %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<br><p>%s</p></body></html>\r\n", body, longmsg);

    // print HTTP response
    sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    sprintf(buf, "%sDate: %s\r\n", buf, dbuf);
    sprintf(buf, "%sServer: Liso/1.0\r\n", buf);
    if (1) sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-type: text/html\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n\r\n", buf, (int)strlen(body));
    send(client_fd, buf, strlen(buf), 0);
    send(client_fd, body, strlen(body), 0);
}


void serve_get(int client_fd, HTTPContext *context)
{
    serve_head(client_fd, context);
    serve_body(client_fd, context);
}

void serve_head(int client_fd, HTTPContext *context)
{
    struct tm tm;
    struct stat sbuf;
    time_t now;
    char buf[BUFF_SIZE];
    char filetype[MIN_LINE];
    char tbuf[DATE_SIZE];
    char dbuf[DATE_SIZE];


    get_filetype(context->filename, filetype);
    printf("filetype: %s\n", filetype);

    // get time string
    tm = *gmtime(&sbuf.st_mtime);
    strftime(tbuf, DATE_SIZE, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    now = time(0);
    tm = *gmtime(&now);
    strftime(dbuf, DATE_SIZE, "%a, %d %b %Y %H:%M:%S %Z", &tm);

    stat(context->filename, &sbuf);

    // send response headers to client
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    sprintf(buf, "%sDate: %s\r\n", buf, dbuf);
    sprintf(buf, "%sServer: Liso/1.0\r\n", buf);
    if (1) sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-Length: %ld\r\n", buf, sbuf.st_size);
    sprintf(buf, "%sContent-Type: %s\r\n", buf, filetype);
    sprintf(buf, "%sLast-Modified: %s\r\n\r\n", buf, tbuf);
    send(client_fd, buf, strlen(buf), 0);
}

void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".css"))
        strcpy(filetype, "text/css");
    else if (strstr(filename, ".js"))
        strcpy(filetype, "application/javascript");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}

int serve_body(int client_fd, HTTPContext *context)
{
    int fd, filesize;
    char *ptr;
    struct stat sbuf;
    if ((fd = open(context->filename, O_RDONLY, 0)) < 0)
    {
        Log(fp, "Error: Cann't open file \n");
        return -1; ///TODO what error code here should be?
    }
    stat(context->filename, &sbuf);
    filesize = sbuf.st_size;
    ptr = mmap(0, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    int nbytes;
    nbytes = send(client_fd, ptr, filesize, 0);
    printf("ddd server send %d data\n", nbytes);
    munmap(ptr, filesize);
    return 0;
}

void serve_post(int client_fd, HTTPContext *context)
{
    return;
}
