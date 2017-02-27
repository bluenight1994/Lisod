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
 *    which of the file descriptors you selected which is ready for reading.    *                  *
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
int is_valid_method(char *method);
Requests *get_freereq(Buff *b);
void client_error(Requests *req, char *addr, char *cause, char *errnum, char*shortmsg, char*longmsg);
void fill_header(Requests *req, Request_header *rh, int cnt);
void fill_helper(Requests *req, char *key, char *value);
void get_time(char *date);
int parse_uri(pool *p, char *uri, char* filename);
void serve_static(Buff *buff, char *filename, struct stat sbuf);
void serve_send(pool *p);

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
        pool.read_ready_set = pool.read_set;
        pool.write_ready_set = pool.write_set;
        pool.nready = select(pool.maxfd+1, &pool.read_ready_set, &pool.write_ready_set, NULL, NULL);
        if (pool.nready == -1) {
            fprintf(stderr, "select error\n");
            return EXIT_FAILURE;
        }
        /* listen discriptor ready */
        if (FD_ISSET(listen_sock, &pool.read_ready_set)) {
            addrlen = sizeof cli_addr;
            newfd = accept(listen_sock, (struct sockaddr *)&cli_addr, &addrlen);

            if (newfd == -1) {
                fprintf(stderr, "establishing new connection error\n");
                break;
            } else {
                /* add new client to pool */
                add_client(newfd, &pool, (struct sockaddr_in *) &cli_addr, http_port);
            }
        }

        handle_clients(listen_sock, &pool);
        printf("fuck1\n");
        if (pool.nready)
            serve_send(&pool);
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

void init_pool(int listenfd, pool *p) {
    int i;
    p->maxi = -1;
    for (i = 0; i < FD_SETSIZE; i++) {
        p->buf[i] = NULL;
        p->clientfd[i] = -1;
    }
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

void add_client(int newfd, pool *p, struct sockaddr_in *cli_addr, int port) {
    int i;
    Buff* bufi;
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++) {
        if (p->clientfd[i] < 0) {
            p->clientfd[i] = newfd;
            if (p->buf[i] == NULL) {
                p->buf[i] = (Buff *)malloc(sizeof(Buff));
                bufi = p->buf[i];
                bufi->buf = (char *)malloc(BUFF_SIZE);
                bufi->stage = STAGE_INIT;
                bufi->request = (Requests *)malloc(sizeof(Requests));
                bufi->request->response = NULL;
                bufi->request->next = NULL;
                bufi->request->header = NULL;
                bufi->request->method = NULL;
                bufi->request->uri = NULL;
                bufi->request->version = NULL;
                bufi->request->valid = REQ_INVALID;
                bufi->request->post_body = NULL;
                bufi->cur_size = 0;
                bufi->cur_parsed = 0;
                bufi->size = BUFF_SIZE;
                bufi->fd = newfd;
                bufi->port = port;
                inet_ntop(AF_INET, &(cli_addr->sin_addr),
                          bufi->addr, INET_ADDRSTRLEN);
                Log(fp, "HTTP cient added: %s\n", bufi->addr);
            }
            FD_SET(newfd, &p->read_set);
            if (newfd > p->maxfd)
                p->maxfd = newfd;
            if (i > p->maxi)
                p->maxi = i;
            break;
        }
    }

    if (i == FD_SETSIZE)
        Log(fp, "Error when adding client, too many clients\n");
}

void handle_clients(int listen, pool *p) {
    int i, curfd;
    Buff *buff;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        curfd = p->clientfd[i];

        if ((curfd > 0) && (FD_ISSET(curfd, &p->read_ready_set))) {
            p -> nready--;
            buff = p->buf[i];
            if (buff->stage == STAGE_ERROR) continue;
            if (buff->stage == STAGE_CLOSE) continue;
            process_request(i, p);
        }
    }
}

void process_request(int i, pool *p) {
    int nbytes;
    Buff *bufi;
    char filename[BUFF_SIZE];
    struct stat sbuf;

    size_t buff_size;

    int curfd = p->clientfd[i];
    bufi = p->buf[i];

    if (bufi->stage == STAGE_INIT) {
        buff_size = bufi->size - bufi->cur_size;
        if ((nbytes = recv(curfd, bufi->buf + bufi->cur_size, buff_size, 0)) <= 0) {
            if (nbytes == 0) {
                Log(fp, "sock %d hung up\n", i);
                printf("sock %d hung up\n", i);
            }
            else
                Log(fp, "Error occurred when receiving data from client\n");

            FD_CLR(curfd, &p->read_set);
            p->clientfd[i] = -1;
            if (close_socket(curfd))
                Log(fp, "Error occurred when trying to close client socket %d\n", curfd);
            return;
        } else {
            Log(fp, "Server received %d bytes data on %d\n", (int)nbytes, i);
            printf("Server received %d bytes data on %d\n", (int)nbytes, i);
            bufi->cur_size += nbytes;
            bufi->cur_parsed += nbytes;
            Request *request = parse(bufi->buf + bufi -> cur_size - nbytes, nbytes, curfd);
            bufi->cur_request = get_freereq(bufi);
            bufi->cur_request->method  = (char *)malloc(strlen(request->http_method)+1);
            bufi->cur_request->version = (char *)malloc(strlen(request->http_version)+1);
            bufi->cur_request->uri     = (char *)malloc(strlen(request->http_uri)+1);
            strcpy(bufi->cur_request->method, request->http_method);
            strcpy(bufi->cur_request->version, request->http_version);
            strcpy(bufi->cur_request->uri, request->http_uri);

            fill_header(bufi->cur_request, request->headers, request->header_count);

            if (!is_valid_method(request->http_method)) {
                client_error(bufi->cur_request, bufi->addr, bufi->cur_request->method, "501", "Not Implemented", "Server hasn't implemented this method");
                bufi->stage = STAGE_ERROR;
                FD_SET(curfd, &p->write_set);
                return;
            }

            parse_uri(p, bufi->cur_request->uri, filename);

            if (stat(filename, &sbuf) < 0) {
                client_error(bufi->cur_request, bufi->addr, bufi->cur_request->method, "404", "Not found", "File not found on this server");
                bufi->stage = STAGE_ERROR;
                FD_SET(curfd, &p->write_set);
                return;
            }
            serve_static(bufi, filename, sbuf);
            FD_SET(curfd, &p->write_set);
            bufi->cur_request->valid = REQ_VALID;

            /* server provide static content according to the client request */


//			if (send(curfd, bufi->buf + bufi->cur_size - nbytes, nbytes, 0) != nbytes) {
//				printf("request error\n");
//				close_socket(curfd);
//				Log(fp, "Error occurred when sending data to client.\n");
//				// return EXIT_FAILURE;
//			}
//			Log(fp, "Server sent %d bytes data to %d\n", (int)nbytes, i);
//			printf("Server sent %d bytes data to %d\n", (int)nbytes, i);
            if (bufi->stage != STAGE_ERROR && bufi->stage != STAGE_CLOSE)
                bufi->stage = STAGE_INIT;
            bufi->cur_size = 0;
            bufi->cur_parsed = 0;
            FD_CLR(curfd, &p->read_ready_set);
        }
    }
}

int is_valid_method(char *method) {
    if (!strcasecmp(method, "GET")) {
        return 1;
    } else if (!strcasecmp(method, "POST")) {
        return 1;
    } else if (!strcasecmp(method, "head")) {
        return 1;
    }
    return 0;
}

Requests *get_freereq(Buff *b) {
    Requests *req = b->request;
    while (req->valid == REQ_VALID) {
        if (req->next == NULL)
            break;
        req = req->next;
    }

    if (req->valid == REQ_INVALID) {
        Headers *hdr = req->header;
        Headers *hdr_pre;
        while (hdr) {
            hdr_pre = hdr;
            hdr = hdr->next;
            free(hdr_pre);
        }
        req->header = NULL;

    } else {
        req->next = (Requests *)malloc(sizeof(Requests));

        req = req->next;
    }
    req->pipefd = -1;
    req->response = NULL;
    req->header = NULL;
    req->next = NULL;
    req->method = NULL;
    req->uri = NULL;
    req->version = NULL;
    req->valid = REQ_INVALID;
    req->post_body = NULL;
    return req;
}

void fill_header(Requests *req, Request_header *rh, int cnt) {
    int index;
    for (index = 0; index < cnt; index++) {
//		printf("Request Header\n");
//		printf("Header name %s Header Value %s\n", rh[index].header_name, rh[index].header_value);
        fill_helper(req, rh[index].header_name, rh[index].header_value);
    }
}

void fill_helper(Requests *req, char *key, char *value) {
    Headers *h = req->header;
    if (req->header == NULL) {
        req->header = (Headers *)malloc(sizeof(Headers));
        req->header->next = NULL;
        req->header->key = (char *)malloc(strlen(key) + 1);
        req->header->value = (char *)malloc(strlen(value) + 1);
        strcpy(req->header->key, key);
        strcpy(req->header->value, value);
        return;
    }

    while (h->next != NULL)
        h = h->next;

    h->next = (Headers *)malloc(sizeof(Headers));
    h = h->next;
    h->next = NULL;
    h->key = (char *)malloc(strlen(key) + 1);
    h->value = (char *)malloc(strlen(value) + 1);
    strcpy(h->key, key);
    strcpy(h->value, value);
}

void get_time(char *date) {
    time_t t;
    struct tm *tmp;
    t = time(NULL);
    tmp = localtime(&t);
    strftime(date, DATE_SIZE, "%a, %d %b %Y %T %Z", tmp);
}

void client_error(Requests *req, char *addr, char *cause, char *errnum, char*shortmsg, char*longmsg) {
    char date[DATE_SIZE], body[BUFF_SIZE], hdr[BUFF_SIZE];
    int len = 0;
    get_time(date);
    /* Build the HTTPS response body */
    sprintf(body, "<html><title>Liso</title>");
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Liso Web server</em>\r\n", body);

    /* Print the HTTPS response */
    sprintf(hdr, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    sprintf(hdr, "%sContent-Type: text/html\r\n",hdr);
    sprintf(hdr, "%sConnection: Close\r\n",hdr);
    sprintf(hdr, "%sDate: %s\r\n",hdr, date);
    sprintf(hdr, "%sContent-Length: %d\r\n\r\n%s",hdr,
            (int)strlen(body), body);
    len = strlen(hdr);
    req->response = (char *)malloc(len + 1);
    sprintf(req->response, "%s", hdr);
    char str[256] = {0};
    sprintf(str, "%s [%s] \"%s %s %s\" %s %d\n", addr,
            date,
            req->method,
            req->uri,
            req->version,
            errnum,
            len);
    Log(fp, str);
    req->body = NULL;
    req->valid = REQ_VALID;
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



void serve_static(Buff *buff, char *filename, struct stat sbuf) {
    int srcfd;
    int file_size = sbuf.st_size;
    int len = 0;
    char *srcp, filetype[FILETYPE_SIZE], date[DATE_SIZE], buf[BUFF_SIZE];
    Requests *req = buff->cur_request;
    char modified_time[DATE_SIZE];

    strftime(modified_time, DATE_SIZE, "%a, %d %b %Y %T %Z",
             localtime(&sbuf.st_mtime));
    get_time(date);
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".css"))
        strcpy(filetype, "text/css");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else
        strcpy(filetype, "text/plain");
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    sprintf(buf, "%sServer: Liso/1.0\r\n", buf);
    sprintf(buf, "%sDate:%s\r\n", buf, date);
    if (buff->stage == STAGE_CLOSE)
        sprintf(buf, "%sConnection: Close\r\n", buf);
    else
        sprintf(buf, "%sConnection: Keep-Alive\r\n", buf);
    sprintf(buf, "%sContent-Length: %d\r\n", buf, file_size);
    sprintf(buf, "%sLast-Modified: %s\r\n", buf, modified_time);
    sprintf(buf, "%sContent-Type: %s\r\n", buf, filetype);

    len = strlen(buf);
    req->response = (char *)malloc(len+1);
    sprintf(req->response, "%s", buf);

    if (strcmp(req->method, "HEAD")) {
        srcfd = open(filename, O_RDONLY, 0);
        srcp = mmap(0, file_size, PROT_READ, MAP_PRIVATE, srcfd, 0);
        req->body = srcp;
        req->body_size = file_size;
        close(srcfd);
        char str[256] = {0};
        sprintf(str, "%s [%s] \"%s %s %s\" %s %d\n", buff->addr,
                date,
                req->method,
                req->uri,
                req->version,
                "200",
                len + file_size);
        Log(fp, str);
    } else {
        req->body = NULL;
        char str[256] = {0};
        sprintf(str, "%s [%s] \"%s %s %s\" %s %d\n", buff->addr,
                date,
                req->method,
                req->uri,
                req->version,
                "200",
                len);
        Log(fp, str);
    }
}


void serve_send(pool *p) {
    printf("fuck2\n");
    int i;
    int client_sock;
    ssize_t sendret;
    Requests *req;
    Buff *buf;
    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        if (p->buf[i] == NULL || p->clientfd[i] == -1) continue;
        buf = p->buf[i];
        client_sock = buf->fd;
        if (FD_ISSET(client_sock, &p->write_ready_set)) {
            req = buf->request;
            while (req) {
                if (req->valid != REQ_VALID) {
                    req = req -> next;
                    continue;
                }
                if ((sendret = send(client_sock, req->response, strlen(req->response), 0)) > 0) {
                    printf("Server send header to %d\n", client_sock);
                } else {
                    close_socket(client_sock);
                    req->valid = REQ_INVALID;
                    req = req->next;
                    break;
                }

                if (req->body != NULL) {
                    if ((sendret = send(client_sock, req->body, req->body_size, 0)) > 0) {
                        printf("Server send %zd bytes to %d\n", sendret, client_sock);
                        munmap(req->body, req->body_size);
                        req->body = NULL;
                    } else {
                        close_socket(client_sock);
                        req->valid = REQ_INVALID;
                        req = req->next;
                        break;
                    }
                }

                req->valid = REQ_INVALID;
                req = req->next;
            }
            if (buf->stage == STAGE_CLOSE) close_socket(client_sock);
            if (buf->stage == STAGE_ERROR) buf->stage = STAGE_INIT;
            FD_CLR(client_sock, &p->write_set);
        }
    }
}





