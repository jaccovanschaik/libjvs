/*
 * udp.c: provides a simplified interface to UDP networking.
 *
 * udp.c is part of libjvs.
 *
 * Copyright:   (c) 2007-2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: udp.c 343 2019-08-27 08:39:24Z jacco $
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
#include <stdint.h>
#include <netdb.h>
#include <errno.h>

#include "net.h"
#include "tcp.h"
#include "debug.h"
#include "utils.h"

static struct linger linger = { 1, 5 }; /* 5 second linger */

static int one = 1;

/*
 * Create an unbound UDP socket.
 */
int udpSocket(void)
{
    int sd;                            /* socket descriptor */

    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
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

/*
 * Send <data> with size <size> via <fd> to <host>, <port>. Note that this function does a hostname
 * lookup for every call, which can be slow. If possible, use netConnect() to set a default
 * destination address, after which you can simply write() to the socket without incurring this
 * overhead.
 */
int udpSend(int fd, const char *host, uint16_t port, const char *data, size_t size)
{
    struct hostent *host_ptr;               /* pointer to host info for remote host */

    struct sockaddr_in peeraddr_in = { 0 }; /* for peer socket address */

    peeraddr_in.sin_family = AF_INET;
    peeraddr_in.sin_port = htons(port);

    if ((host_ptr = gethostbyname(host)) == NULL) {
        dbgError(stderr, "gethostbyname(%s) failed", host);
        return -1;
    }

    peeraddr_in.sin_addr.s_addr =
        ((struct in_addr *) (host_ptr->h_addr))->s_addr;

    return sendto(fd, data, size, 0, (struct sockaddr *) &peeraddr_in, sizeof(peeraddr_in));
}

#ifdef TEST
#include "net.h"

static int errors = 0;

int main(int argc, char *argv[])
{
    int r;
    char buffer[16];

    int recv_fd = udpSocket();
    int send_fd = udpSocket();

    netBind(recv_fd, "localhost", 1234);
    netConnect(send_fd, "localhost", 1234);

    r = write(send_fd, "Hoi!", 4);  /* Fixes warning about not using return code. */
    r = read(recv_fd, buffer, sizeof(buffer));

    make_sure_that(r == 4);
    make_sure_that(strncmp(buffer, "Hoi!", 4) == 0);

    udpSend(send_fd, "localhost", 1234, "Hallo!", 6);
    r = read(recv_fd, buffer, sizeof(buffer));

    make_sure_that(r == 6);
    make_sure_that(strncmp(buffer, "Hallo!", 6) == 0);

    return errors;
}
#endif
