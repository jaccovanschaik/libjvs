/*
 * tcp.c: provides a simplified interface to TCP/IP networking.
 *
 * tcp.c is part of libjvs.
 *
 * Copyright:   (c) 2007-2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: tcp.c 398 2020-09-08 13:09:18Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <stdint.h>

#include "net.h"
#include "tcp.h"
#include "defs.h"
#include "debug.h"

static struct linger linger = { 1, 5 }; /* 5 second linger */

static int one = 1;

/*
 * Create a socket
 */
static int tcp_socket(void)
{
    int sd;                            /* socket descriptor */

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
P       dbgError(stderr, "unable to create socket");
        return -1;
    }

    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0) {
P       dbgError(stderr, "setsockopt(REUSEADDR) failed");
        return -1;
    }

    if (setsockopt(sd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) != 0) {
P       dbgError(stderr, "setsockopt(LINGER) failed");
        return -1;
    }

    return sd;
}

/*
 * Tell a socket to be a listener
 */
static int tcp_listen(int socket)
{
    if (listen(socket, 5) == -1) {
P       dbgError(stderr, "listen failed");
        return -1;
    }

    return 0;
}

/*
 * Open a listen port on <host> and <port> and return the corresponding
 * file descriptor. If <host> is NULL the socket will listen on all
 * interfaces. If <port> is equal to 0, the socket will be bound to a
 * random local port (use netLocalPort() on the returned fd to find out
 * which).
 */
int tcpListen(const char *host, uint16_t port)
{
    int lsd;

    if ((lsd = tcp_socket()) == -1) {
P       dbgError(stderr, "tcp_socket failed");
        return -1;
    }

    if (netBind(lsd, host, port) != 0) {
P       dbgError(stderr, "netBind failed");
        return -1;
    }

    if (tcp_listen(lsd) != 0) {
P       dbgError(stderr, "tcp_listen failed");
        return -1;
    }

    return lsd;
}

/*
 * Make a connection to <port> on <host> and return the corresponding
 * file descriptor.
 */
int tcpConnect(const char *host, uint16_t port)
{
    int ret;

    char port_as_text[6];
    struct addrinfo *info;

    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM
    };

    snprintf(port_as_text, sizeof(port_as_text), "%hu", port);

    if ((ret = getaddrinfo(host, port_as_text, &hints, &info)) != 0) {
        dbgError(stderr, "tcpConnect: getaddrinfo failed (%s)", gai_strerror(ret));

        ret = -1;
    }
    else if ((ret = tcp_socket()) < 0) {
        dbgError(stderr, "tcpConnect: tcp_socket failed");

        ret = -1;
    }
    else if (connect(ret, info->ai_addr, info->ai_addrlen) != 0) {
        dbgError(stderr, "tcpConnect: connect failed");

        close(ret);

        ret = -1;
    }

    freeaddrinfo(info);

    return ret;
}

/*
 * Accept an incoming connection request on a listen socket.
 */
int tcpAccept(int sd)
{
    struct sockaddr peeraddr;    /* for peer socket address */

    socklen_t addrlen;

    int csd;

    memset(&peeraddr, 0, sizeof(struct sockaddr));

    addrlen = sizeof(struct sockaddr);

    do {
        csd = accept(sd, &peeraddr, &addrlen);
    } while (csd == -1 && errno == EINTR);

    if (csd == -1) {
P       dbgError(stderr, "accept failed");
    }

    return csd;
}

/*
 * Read from <fd> until <buf> contains exactly <len> bytes.
 */
int tcpRead(int fd, void *buf, int len)
{
    int res, n = 0;

    do {
        res = read(fd, (char *) buf + n, len - n);
    } while ((res > 0 || errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
             && (n += res) < len);

    if (res == -1) {
P       dbgError(stderr, "read failed");
        return -1;
    }
    else {
        return n;
    }
}

/*
 * Write all of the <len> bytes in <buf> to <fd>.
 */
int tcpWrite(int fd, const void *buf, int len)
{
    int res, n = 0;

    do {
        res = write(fd, (const char *) buf + n, len - n);
    } while ((res > 0 || errno == EINTR) && (n += res) < len);

    if (res == -1) {
P       dbgError(stderr, "write failed");
        return -1;
    }
    else {
        return n;
    }
}

#ifdef TEST
#include "net.h"

static int errors = 0;

int main(void)
{
    int r;

    char incoming[32];

    char *client_msg = "Hello client!";
    char *server_msg = "Hello server!";

    int listen_fd = tcpListen("localhost", 54321);
    int client_fd = tcpConnect("localhost", 54321);
    int server_fd = tcpAccept(listen_fd);

    if ((r = tcpWrite(client_fd, server_msg, strlen(server_msg))) != (int) strlen(server_msg)) {
        fprintf(stderr, "tcpWrite to client_fd failed.\n");
        errors++;
    }
    else if ((r = tcpRead(server_fd, incoming, strlen(server_msg))) != (int) strlen(server_msg)) {
        fprintf(stderr, "tcpRead from server_fd failed.\n");
        errors++;
    }
    else if (strncmp(incoming, server_msg, strlen(server_msg)) != 0) {
        fprintf(stderr, "incoming = \"%s\", server_msg = \"%s\"\n", incoming, server_msg);
        errors++;
    }

    if ((r = tcpWrite(server_fd, client_msg, strlen(client_msg))) != (int) strlen(client_msg)) {
        fprintf(stderr, "tcpWrite to server_fd failed.\n");
        errors++;
    }
    else if ((r = tcpRead(client_fd, incoming, strlen(client_msg))) != (int) strlen(client_msg)) {
        fprintf(stderr, "tcpRead from client_fd failed.\n");
        errors++;
    }
    else if (strncmp(incoming, client_msg, strlen(client_msg)) != 0) {
        fprintf(stderr, "incoming = \"%s\", client_msg = \"%s\"\n", incoming, client_msg);
        errors++;
    }

    return errors;
}

#endif
