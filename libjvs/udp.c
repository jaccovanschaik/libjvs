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

#include "tcp.h"
#include "debug.h"

static struct linger linger = { 1, 5 }; /* 5 second linger */

static int one = 1;

/* Create a socket */

static int udp_socket(void)
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

/* Bind a socket to <port> and <host>. If <host> is NULL, the socket
 * will be bound to INADDR_ANY. If port is 0, it will be bound to a random port. */

static int udp_bind(int socket, const char *host, int port)
{
    struct sockaddr_in myaddr_in;      /* for local socket address */
    struct hostent *host_ptr;

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

static int udp_listen(int socket)
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

/* Make a connection to <port> on <host> and return the corresponding
 * file descriptor. */

int udpConnect(const char *host, int port)
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

    sd = udp_socket();

    if (sd == -1) {
        dbgError(stderr, "udp_socket failed");
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
