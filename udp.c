/*
 * udp.c: provides a simplified interface to UDP networking.
 *
 * udp.c is part of libjvs.
 *
 * Copyright:   (c) 2007-2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: udp.c 404 2020-12-19 14:08:20Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
 * Send <data> with size <size> via <fd> to <host>, <port>. Note that this
 * function does a hostname lookup for every call, which can be slow. If
 * possible, use netConnect() to set a default destination address, after
 * which you can simply write() to the socket without incurring this overhead.
 */
int udpSend(int fd, const char *host, uint16_t port,
        const char *data, size_t size)
{
    struct hostent *host_ptr;               /* pointer to remote host info */
    struct sockaddr_in peeraddr_in = { 0 }; /* for peer socket address */

    peeraddr_in.sin_family = AF_INET;
    peeraddr_in.sin_port = htons(port);

    if ((host_ptr = gethostbyname(host)) == NULL) {
        dbgError(stderr, "gethostbyname(%s) failed", host);
        return -1;
    }

    peeraddr_in.sin_addr.s_addr =
        ((struct in_addr *) (host_ptr->h_addr))->s_addr;

    return sendto(fd, data, size, 0, (struct sockaddr *) &peeraddr_in,
            sizeof(peeraddr_in));
}

/*
 * Add the socket given by <fd> to the multicast group given by <group> (a
 * dotted-quad ip address).
 */
int udpMulticastJoin(int fd, const char *group)
{
    struct ip_mreq mreq;

    mreq.imr_multiaddr.s_addr = inet_addr(group);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
         perror("Adding multicast membership");
         return -1;
    }

    return 0;
}

/*
 * Allow (if <allow_loop> is 1) or disallow (if it is 0) multicast packets to be
 * looped back to the sending network interface. The default is that packets do
 * loop back.
 */
int udpMulticastLoop(int fd, int allow_loop)
{
    char loop = allow_loop;

    if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *) &loop,
                sizeof(loop)) < 0)
    {
        perror("Setting IP_MULTICAST_LOOP");
        return -1;
    }

    return 0;
}

/*
 * Set the outgoing UDP Multicast interface for <fd> to the one associated with
 * <address>.
 */
int udpMulticastInterface(int fd, const char *address)
{
    struct in_addr addr;

    addr.s_addr = inet_addr(address);

    if(setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&addr,
                sizeof(addr)) < 0)
    {
        perror("Setting IP_MULTICAST_IF");
        return -1;
    }

    return 0;
}

/*
 * Remove the socket given by <fd> from the multicast group given by <group> (a
 * dotted-quad ip address).
 */
int udpMulticastLeave(int fd, const char *group)
{
    struct ip_mreq mreq;

    mreq.imr_multiaddr.s_addr = inet_addr(group);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
         perror("Removing multicast membership");
         return -1;
    }

    return 0;
}

#ifdef TEST
#include "net.h"

static int errors = 0;

int main(void)
{
    int r;
    char buffer[16];

    uint16_t recv_port = 1234;

    int recv_fd = udpSocket();
    int send_fd = udpSocket();

    // Simple test: bind receiving socket, send from sending socket.

    netBind(recv_fd, "localhost", recv_port);

    udpSend(send_fd, "localhost", recv_port, "Hallo!", 6);
    r = read(recv_fd, buffer, sizeof(buffer));

    make_sure_that(r == 6);
    make_sure_that(strncmp(buffer, "Hallo!", 6) == 0);

    close(recv_fd);
    close(send_fd);

    // Bind both sending and receiving socket, send from sending socket.

    uint16_t send_port = 1235;

    recv_fd = udpSocket();
    send_fd = udpSocket();

    netBind(send_fd, "localhost", send_port);
    netBind(recv_fd, "localhost", recv_port);

    udpSend(send_fd, "localhost", recv_port, "Hallo!", 6);
    r = read(recv_fd, buffer, sizeof(buffer));

    make_sure_that(r == 6);
    make_sure_that(strncmp(buffer, "Hallo!", 6) == 0);

    // Also try reverse: send from receive socket, receive on send socket.

    udpSend(recv_fd, "localhost", send_port, "Hallo!", 6);
    r = read(send_fd, buffer, sizeof(buffer));

    make_sure_that(r == 6);
    make_sure_that(strncmp(buffer, "Hallo!", 6) == 0);

    return errors;
}
#endif
