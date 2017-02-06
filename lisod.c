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

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUF_SIZE 4096
#define ARG_NUMBER 0
#define LISTENQ 1024

void usage();
int  open_listen_socket(int port);
int  close_socket(int sock);


int main(int argc, char* argv[]) 
{
	fd_set master;
	fd_set read_fds;
	int fdmax;

	int listen_sock;
	int newfd;
	struct sockaddr_storage remoteaddr;
	socklen_t addrlen;

	int http_port;

	int i;

	char buf[BUF_SIZE];
	int nbytes;

	// if (argc != ARG_NUMBER + 1) {
	// 	usage();
	// }

	http_port = atoi(argv[1]);

	fprintf(stdout, "------ Echo Server ------\n");

	listen_sock = open_listen_socket(http_port);

	FD_SET(listen_sock, &master);
	fdmax = listen_sock;

	char remoteIP[INET6_ADDRSTRLEN];

	while (1) {
		read_fds = master;
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			fprintf(stderr, "select error\n");
			return EXIT_FAILURE;
		}

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) {
				if (i == listen_sock) {
					addrlen = sizeof remoteaddr;
					newfd = accept(listen_sock, (struct sockaddr *)&remoteaddr, &addrlen);

					if (newfd == -1) {

					} else {
						FD_SET(newfd, &master);
						if (newfd > fdmax) {
							fdmax = newfd;
						}
						printf("selectserver: new connection.\n");
					}
				} else {
					if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
						if (nbytes == 0) {
							fprintf(stdout, "selectserver: socket %d hung up\n", i);
						} else {
							fprintf(stderr, "recv error");
						}
						FD_CLR(i, &master);
						if (close_socket(i)) {
							close_socket(listen_sock);
							fprintf(stderr, "error closing client socket.\n");
							return EXIT_FAILURE;
						}
					} else {
						fprintf(stdout, "Server received %d bytes data on %d\n", (int)nbytes, i);
						if (send(i, buf, nbytes, 0) != nbytes) {
							close_socket(listen_sock);
							close_socket(i);
							fprintf(stderr, "error occured when sending data to client\n");
							return EXIT_FAILURE;
						}
						fprintf(stdout, "Server sent %d bytes data to %d\n", (int)nbytes, i);
                 		memset(buf, 0, BUF_SIZE);
					}
				}
			}
		}
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
		fprintf(stderr, "Failed creating socket.\n");
		return EXIT_FAILURE;
	}

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		fprintf(stderr, "Failed set socketopt.\n");
		return EXIT_FAILURE;
	}

	addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr))) {
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }

    if (listen(sock, LISTENQ)) {
		close_socket(sock);
		fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    return sock;
}



