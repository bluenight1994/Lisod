/***********************************************************************************
 *    Documentation:
 *
 *    select() give the server the power to monitor several sockets at the same time.
 *    will tell which ones are ready for reading, which are ready for writing
 *    and which sockets have raised exceptions.
 *  
 *    int select(int numdfs, fd_set *readfds, fd_set *writefds, 
 *			fd_set *exceptfds, struct timeval *timeout);
 *    
 *    When select() returns, readfds will be modified to reflect which of the file
 *    descriptors you selected which is ready for reading. 
 *    
 *
 *
 ***********************************************************************************
 */
#include "lisod.h"

#define BUF_SIZE 4096
#define ARG_NUMBER 2
#define LISTENQ 1024

void usage();
int  open_listen_socket(int port);
int  close_socket(int sock);
void init_pool(int listenfd, pool *p);
void add_client(int newfd, pool *p);
void handle_clients(int listen, pool *p);

FILE *fp;

int main(int argc, char* argv[]) 
{
    int listen_sock;
    int newfd;
	struct sockaddr_storage remoteaddr;
	socklen_t addrlen;
    char* logfile;

	int http_port;

	if (argc != ARG_NUMBER + 1) {
		usage();
	}

	http_port = atoi(argv[1]);
    logfile = argv[2];
    fp = open_log(logfile);

	static pool pool;
	fprintf(stdout, "------ Echo Server ------\n");
    Log(fp, "Start Lisod Server\n");

	listen_sock = open_listen_socket(http_port);
	init_pool(listen_sock, &pool);

	// char remoteIP[INET6_ADDRSTRLEN];

	while (1) {
		pool.ready_set = pool.read_set;
		pool.nready = select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

		if (pool.nready == -1) {
			fprintf(stderr, "select error\n");
			return EXIT_FAILURE;
		}
		/* listen discriptor ready */
		if (FD_ISSET(listen_sock, &pool.ready_set)) {
			addrlen = sizeof remoteaddr;
			newfd = accept(listen_sock, (struct sockaddr *)&remoteaddr, &addrlen);

			if (newfd == -1) {
				fprintf(stderr, "establishing new connection error\n");
			} else {
				/* add new client to pool */
				add_client(newfd, &pool);
                Log(fp, "new connection from client\n");
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
    if (close(sock))
    {
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
	for (i = 0; i < FD_SETSIZE; i++)
		p->clientfd[i] = -1;
	p->maxfd = listenfd;
	FD_ZERO(&p->read_set);
	FD_SET(listenfd, &p->read_set);
}

void add_client(int newfd, pool *p) {
	int i;
	p->nready--;
	for (i = 0; i < FD_SETSIZE; i++) {
		if (p->clientfd[i] < 0) {
			p->clientfd[i] = newfd;

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
	int nbytes;
	char buf[BUF_SIZE];
	for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
		curfd = p->clientfd[i];

		if ((curfd > 0) && (FD_ISSET(curfd, &p->ready_set))) {
			p -> nready--;
			if ((nbytes = recv(curfd, buf, sizeof buf, 0)) <= 0) {
				if (nbytes == 0) {
                    Log(fp, "sock %d hung up\n", i);
				} else {
                    Log(fp, "Error occurred when receiving data from client\n");
				}
				FD_CLR(curfd, &p->read_set);
				p->clientfd[i] = -1;
				if (close_socket(curfd)) {
					close_socket(listen);
                    Log(fp, "Error occurred when trying to close client socket %d\n", curfd);
					// return EXIT_FAILURE;
				}
			} else {
                Log(fp, "Server received %d bytes data on %d\n", (int)nbytes, i);
				if (send(curfd, buf, nbytes, 0) != nbytes) {
					close_socket(listen);
					close_socket(curfd);
                    Log(fp, "Error occurred when sending data to client.\n");
					// return EXIT_FAILURE;
				}
                Log(fp, "Server sent %d bytes data to %d\n", (int)nbytes, i);
				memset(buf, 0, BUF_SIZE);
			}
		}
	}
}


