/*
 * udp.c: provides a simplified interface to UDP networking.
 *
 * udp.c is part of libjvs.
 *
 * Copyright:   (c) 2007-2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: udp.c 411 2020-12-20 00:30:11Z jacco $
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
 * Create an unbound UDP socket using address family <family>.
 */
static int udp_socket(int family)
{
    int sd;                            /* socket descriptor */

    if ((sd = socket(family, SOCK_DGRAM, 0)) == -1) {
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
 * Send <data> with size <size> via <sd> to <port> on <host>, <port>, using
 * address family <family>. Note that this function does a hostname lookup for
 * every call, which can be slow. If possible, use udpConnect() to set a
 * default destination address, after which you can simply write() to the
 * socket without incurring this overhead.
 */
static int udp_send(int sd, const char *host, uint16_t port, int family,
        const char *data, size_t size)
{
    int r;

    char port_as_text[6];
    struct addrinfo *info;

    struct addrinfo hints = {
        .ai_family = family,
        .ai_socktype = SOCK_DGRAM
    };

    snprintf(port_as_text, sizeof(port_as_text), "%hu", port);

    if ((r = getaddrinfo(host, port_as_text, &hints, &info)) != 0) {
        dbgError(stderr, "%s: getaddrinfo failed (%s)",
                __func__, gai_strerror(r));

        r = -1;
    }

    r = sendto(sd, data, size, 0, info->ai_addr, info->ai_addrlen);

    freeaddrinfo(info);

    return r;
}

/*
 * Connect UDP socket <sd> to port <port> on host <host>, using address family
 * <family>.
 */
static int udp_connect(int sd, const char *host, uint16_t port, int family)
{
    int r;

    char port_as_text[6];
    struct addrinfo *info;

    struct addrinfo hints = {
        .ai_family = family,
        .ai_socktype = SOCK_DGRAM
    };

    snprintf(port_as_text, sizeof(port_as_text), "%hu", port);

    if ((r = getaddrinfo(host, port_as_text, &hints, &info)) != 0) {
        dbgError(stderr, "%s: getaddrinfo failed (%s)",
                __func__, gai_strerror(r));

        r = -1;
    }
    else if (connect(sd, info->ai_addr, info->ai_addrlen) != 0) {
        dbgError(stderr, "%s: connect failed", __func__);

        close(r);

        r = -1;
    }

    freeaddrinfo(info);

    return r;
}

/*
 * Bind socket <sd> to the interface associated with host name <host> and port
 * <port>, using address family <family>.
 */
static int udp_bind(int sd, const char *host, uint16_t port, int family)
{
    struct addrinfo *info = NULL;
    struct addrinfo  hint = {
        .ai_family = family,
        .ai_socktype = SOCK_DGRAM
    };

    char port_str[6];

    snprintf(port_str, sizeof(port_str), "%hu", port);

    int r;

    if ((r = getaddrinfo(host, port_str, &hint, &info)) != 0) {
        dbgError(stderr, "%s: getaddrinfo for %s:%hu failed (%s)",
                __func__, host, port, gai_strerror(r));

        r = -1;
    }
    else if ((r = bind(sd, info->ai_addr, info->ai_addrlen)) != 0) {
        dbgError(stderr, "%s: bind to %s:%hu failed: %s",
                __func__, host, port, strerror(errno));

        r = -1;
    }

    freeaddrinfo(info);

    return r;
}

/*
 * Create an unbound IPv4 UDP socket.
 */
int udpSocket(void)
{
    return udp_socket(AF_INET);
}

/*
 * Bind IPv4 UDP socket <sd> to the interface associated with host name <host>
 * and port <port>.
 */
int udpBind(int sd, const char *host, uint16_t port)
{
    return udp_bind(sd, host, port, AF_INET);
}

/*
 * Connect IPv4 UDP socket <sd> to port <port> on host <host>.
 */
int udpConnect(int sd, const char *host, uint16_t port)
{
    return udp_connect(sd, host, port, AF_INET);
}

/*
 * Send <data> with size <size> via IPv4 UDP socket <sd> to <host>, <port>.
 * Note that this function does a hostname lookup for every call, which can be
 * slow. If possible, use udpConnect() to set a default destination address,
 * after which you can simply write() to the socket without incurring this
 * overhead.
 */
int udpSend(int sd, const char *host, uint16_t port,
        const char *data, size_t size)
{
    return udp_send(sd, host, port, AF_INET, data, size);
}

/*
 * Create an unbound IPv6 UDP socket.
 */
int udp6Socket(void)
{
    return udp_socket(AF_INET6);
}

/*
 * Bind IPv6 UDP socket <sd> to the interface associated with host name <host>
 * and port <port>.
 */
int udp6Bind(int sd, const char *host, uint16_t port)
{
    return udp_bind(sd, host, port, AF_INET6);
}

/*
 * Connect IPv6 UDP socket <sd> to port <port> on host <host>.
 */
int udp6Connect(int sd, const char *host, uint16_t port)
{
    return udp_connect(sd, host, port, AF_INET6);
}

/*
 * Send <data> with size <size> via IPv6 UDP socket <sd> to <host>, <port>.
 * Note that this function does a hostname lookup for every call, which can be
 * slow. If possible, use udpConnect() to set a default destination address,
 * after which you can simply write() to the socket without incurring this
 * overhead.
 */
int udp6Send(int sd, const char *host, uint16_t port,
        const char *data, size_t size)
{
    return udp_send(sd, host, port, AF_INET6, data, size);
}

/*
 * Add the socket given by <sd> to the multicast group given by <group> (a
 * dotted-quad ip address).
 */
int udpMulticastJoin(int sd, const char *group)
{
    struct ip_mreq mreq;

    mreq.imr_multiaddr.s_addr = inet_addr(group);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
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
int udpMulticastLoop(int sd, int allow_loop)
{
    char loop = allow_loop;

    if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *) &loop,
                sizeof(loop)) < 0)
    {
        perror("Setting IP_MULTICAST_LOOP");
        return -1;
    }

    return 0;
}

/*
 * Set the outgoing UDP Multicast interface for <sd> to the one associated with
 * <address>.
 */
int udpMulticastInterface(int sd, const char *address)
{
    struct in_addr addr;

    addr.s_addr = inet_addr(address);

    if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&addr,
                sizeof(addr)) < 0)
    {
        perror("Setting IP_MULTICAST_IF");
        return -1;
    }

    return 0;
}

/*
 * Remove the socket given by <sd> from the multicast group given by <group> (a
 * dotted-quad ip address).
 */
int udpMulticastLeave(int sd, const char *group)
{
    struct ip_mreq mreq;

    mreq.imr_multiaddr.s_addr = inet_addr(group);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(sd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
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

    udpBind(recv_fd, "localhost", recv_port);

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

    udpBind(send_fd, "localhost", send_port);
    udpBind(recv_fd, "localhost", recv_port);

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
