/*
 * Provides a simplified interface to TCP/IP networking.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id$
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

#include "net.h"
#include "tcp.h"
#include "debug.h"

static struct linger linger = { 1, 5 }; /* 5 second linger */

static int one = 1;

/* Create a socket */

static int tcp_socket(void)
{
    int sd;                            /* socket descriptor */

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        dbgError(stderr, "unable to create socket");
        return -1;
    }

    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0) {
        dbgError(stderr, "setsockopt(REUSEADDR) failed");
        return -1;
    }

    if (setsockopt(sd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) != 0) {
        dbgError(stderr, "setsockopt(LINGER) failed");
        return -1;
    }

    return sd;
}

/* Tell a socket to be a listener */

static int tcp_listen(int socket)
{
    if (listen(socket, 5) == -1) {
        dbgError(stderr, "listen failed");
        return -1;
    }

    return 0;
}

/* Open a listen port on <host> and <port> and return the corresponding
 * file descriptor. If <host> is NULL the socket will listen on all
 * interfaces. If <port> is equal to 0, the socket will be bound to a
 * random local port (use tcpLocalPort() on the returned fd to find out
 * which). */

int tcpListen(const char *host, int port)
{
    int lsd;

    if ((lsd = tcp_socket()) == -1) {
        dbgError(stderr, "tcp_socket failed");
        return -1;
    }

    if (netBind(lsd, host, port) != 0) {
        dbgError(stderr, "netBind failed");
        return -1;
    }

    if (tcp_listen(lsd) != 0) {
        dbgError(stderr, "tcp_listen failed");
        return -1;
    }

    return lsd;
}

/* Make a connection to <port> on <host> and return the corresponding
 * file descriptor. */

int tcpConnect(const char *host, int port)
{
    int fd = tcp_socket();

    if (netConnect(fd, host, port) != 0) {
        dbgError(stderr, "netConnect failed");
        close(fd);
        return -1;
    }

    return fd;
}

/* Accept an incoming connection request on a listen socket */

int tcpAccept(int sd)
{
    struct sockaddr_in peeraddr_in;    /* for peer socket address */

    socklen_t addrlen;

    int csd;

    memset(&peeraddr_in, 0, sizeof(struct sockaddr_in));

    addrlen = sizeof(struct sockaddr_in);

    do {
        csd = accept(sd, (struct sockaddr *) &peeraddr_in, &addrlen);
    } while (csd == -1 && errno == EINTR);

    if (csd == -1)
        dbgError(stderr, "accept failed");

    return csd;
}

/* Read from <fd> until <buf> contains exactly <len> bytes. */

int tcpRead(int fd, void *buf, int len)
{
    int res, n = 0;

    do {
        res = read(fd, (char *) buf + n, len - n);
    } while ((res > 0 || errno == EINTR) && (n += res) < len);

    if (res == -1) {
        dbgError(stderr, "read failed");
        return -1;
    }
    else {
        return n;
    }
}

/* Write all of the <len> bytes in <buf> to <fd>. */

int tcpWrite(int fd, const void *buf, int len)
{
    int res, n = 0;

    do {
        res = write(fd, (char *) buf + n, len - n);
    } while ((res > 0 || errno == EINTR) && (n += res) < len);

    if (res == -1) {
        dbgError(stderr, "write failed");
        return -1;
    }
    else {
        return n;
    }
}
