/*
 * Provides a simplified interface to TCP/IP networking.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: net.c 237 2012-02-10 14:09:38Z jacco $
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
#include "debug.h"

static struct linger linger = { 1, 5 }; /* 5 second linger */

static int one = 1;

/* Create a socket */

static int net_socket(void)
{
    int sd;                            /* socket descriptor */

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
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

/* Bind a socket to <port> and <host>. If <host> is NULL, the socket
 * will be bound to INADDR_ANY. */

static int net_bind(int socket, const char *host, int port)
{
    struct sockaddr_in myaddr_in;      /* for local socket address */
    struct hostent *host_ptr;

    if (port < 0) return 0;

    memset(&myaddr_in, 0, sizeof(myaddr_in));

    if (host == NULL) {
        myaddr_in.sin_addr.s_addr = INADDR_ANY;
    }
    else if ((host_ptr = gethostbyname(host)) != NULL) {
        myaddr_in.sin_addr.s_addr = ((struct in_addr *) (host_ptr->h_addr))->s_addr;
    }
    else {
        dbgError(stderr, "gethostbyname(%s) failed", host);
        return -1;
    }

    myaddr_in.sin_port = htons(port);
    myaddr_in.sin_family = AF_INET;

    if (bind(socket, (struct sockaddr *) &myaddr_in, sizeof(struct sockaddr_in)) != 0) {
        dbgError(stderr, "bind failed");
        return -1;
    }

    return 0;
}

/* Tell a socket to be a listener */

static int net_listen(int socket)
{
    if (listen(socket, 5) == -1) {
        dbgError(stderr, "listen failed");
        return -1;
    }

    return 0;
}

static const char *net_host_name(struct sockaddr_in *peeraddr)
{
    struct hostent *ent = gethostbyaddr((char *) &peeraddr->sin_addr,
            sizeof(peeraddr->sin_addr), AF_INET);

    if (ent == NULL) {
        static char text_buffer[16];

        snprintf(text_buffer, sizeof(text_buffer), "%d.%d.%d.%d",
                 (peeraddr->sin_addr.s_addr & 0xFF000000) >> 24,
                 (peeraddr->sin_addr.s_addr & 0x00FF0000) >> 16,
                 (peeraddr->sin_addr.s_addr & 0x0000FF00) >> 8,
                 (peeraddr->sin_addr.s_addr & 0x000000FF));

        return text_buffer;
    }

    return ent->h_name;
}

/* Open a listen port on <host> and <port> and return the corresponding
 * file descriptor. If <host> is NULL the socket will listen on all
 * interfaces. If <port> is less than 0, the socket will be bound to a
 * random local port. You can use netLocalPort() afterwards to find out
 * which one. */

int netOpenPort(const char *host, int port)
{
    int lsd;

    lsd = net_socket();
    if (lsd == -1) {
        dbgError(stderr, "net_socket failed");
        return -1;
    }

    if (port >= 0 && net_bind(lsd, host, port) != 0) {
        dbgError(stderr, "net_bind failed");
        return -1;
    }

    if (net_listen(lsd) != 0) {
        dbgError(stderr, "net_listen failed");
        return -1;
    }

    return lsd;
}

/* Make a connection to <port> on <host> and return the corresponding
 * file descriptor. */

int netConnect(const char *host, int port)
{
    struct hostent *host_ptr;          /* pointer to host info for remote host */

    struct sockaddr_in peeraddr_in;    /* for peer socket address */

    int sd;                            /* socket descriptor */

    memset(&peeraddr_in, 0, sizeof(peeraddr_in));

    peeraddr_in.sin_family = AF_INET;
    peeraddr_in.sin_port = htons(port);

    if ((host_ptr = gethostbyname(host)) == NULL) {
        dbgError(stderr, "gethostbyname(%s) failed", host);
        return -1;
    }

    peeraddr_in.sin_addr.s_addr =
        ((struct in_addr *) (host_ptr->h_addr))->s_addr;

    sd = net_socket();

    if (sd == -1) {
        dbgError(stderr, "net_socket failed");
        return -1;
    }

    if (connect
        (sd, (struct sockaddr *) &peeraddr_in,
         sizeof(struct sockaddr_in)) != 0) {
        dbgError(stderr, "connect to %s:%d failed", host, port);
        close(sd);
        return -1;
    }

    return sd;
}

/* Get the port that corresponds to service <service>. */

int netPortFor(char *service)
{
    struct servent *serv_ptr;          /* pointer to service information */

    serv_ptr = getservbyname(service, "tcp");

    if (serv_ptr == NULL) {
        dbgError(stderr, "getservbyname(%s) failed", service);
        return -1;
    }

    return serv_ptr->s_port;
}

/* Accept an incoming connection request on a listen socket */

int netAccept(int sd)
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

/* Get hostname of peer */

const char *netPeerHost(int sd)
{
    struct sockaddr_in peeraddr;

    socklen_t len = sizeof(struct sockaddr_in);

    getpeername(sd, (struct sockaddr *) &peeraddr, &len);

    return net_host_name(&peeraddr);
}

/* Get port number used by peer */

int netPeerPort(int sd)
{
    struct sockaddr_in peeraddr;

    socklen_t len = sizeof(struct sockaddr_in);

    getpeername(sd, (struct sockaddr *) &peeraddr, &len);

    return peeraddr.sin_port;
}

/* Get local hostname */

const char *netLocalHost(int sd)
{
    struct sockaddr_in sockaddr;

    socklen_t len = sizeof(struct sockaddr_in);

    getsockname(sd, (struct sockaddr *) &sockaddr, &len);

    return net_host_name(&sockaddr);
}

/* Get local port number. */

int netLocalPort(int sd)
{
    struct sockaddr_in peeraddr;

    socklen_t len = sizeof(struct sockaddr_in);

    getsockname(sd, (struct sockaddr *) &peeraddr, &len);

    return ntohs(peeraddr.sin_port);
}

/* Read until <buf> is full */

int netRead(int fd, void *buf, int len)
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

/* Write all of <buf> */

int netWrite(int fd, const void *buf, int len)
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
