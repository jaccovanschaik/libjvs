/*
 * net.c: General networking utility functions.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <netdb.h>
#include <stdio.h>

#include "defs.h"
#include "net.h"
#include "debug.h"

/*
 * Get the host name that belongs to IP address <big_endian_ip>. Returns the fqdn if it can be
 * found, otherwise an IP address in dotted-quad notation.
 */
const char *netHost(uint32_t big_endian_ip)
{
    static char text_buffer[16];
    struct hostent *ent;

    if (big_endian_ip == 0) {
        snprintf(text_buffer, sizeof(text_buffer), "0.0.0.0");

        return text_buffer;
    }
    else if ((ent = gethostbyaddr((char *) &big_endian_ip, sizeof(big_endian_ip), AF_INET)) == NULL)
    {
        snprintf(text_buffer, sizeof(text_buffer), "%d.%d.%d.%d",
                 (big_endian_ip & 0xFF000000) >> 24,
                 (big_endian_ip & 0x00FF0000) >> 16,
                 (big_endian_ip & 0x0000FF00) >> 8,
                 (big_endian_ip & 0x000000FF));

        return text_buffer;
    }
    else {
        return ent->h_name;
    }
}

/*
 * Get the port that corresponds to service <service>.
 */
int netPort(const char *service, const char *protocol)
{
    struct servent *serv_ptr;          /* pointer to service information */

    serv_ptr = getservbyname(service, protocol);

    if (serv_ptr == NULL) {
P       dbgError(stderr, "getservbyname(%s) failed", service);
        return -1;
    }

    return serv_ptr->s_port;
}

/*
 * Bind a socket to <port> and <host>. If <host> is NULL, the socket
 * will be bound to INADDR_ANY. If port is 0, it will be bound to a random port.
 */
int netBind(int socket, const char *host, int port)
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
P       dbgError(stderr, "gethostbyname(%s) failed", host);
        return -1;
    }

    myaddr_in.sin_port = htons(port);
    myaddr_in.sin_family = AF_INET;

    if (bind(socket, (struct sockaddr *) &myaddr_in, sizeof(struct sockaddr_in)) != 0) {
P       dbgError(stderr, "bind failed");
        return -1;
    }

    return 0;
}

/*
 * Connect an existing socket <fd> to <host> and <port>. Returns 0 on success or -1 if an error
 * occurs.
 */
int netConnect(int fd, const char *host, int port)
{
    struct hostent *host_ptr;               /* pointer to host info for remote host */

    struct sockaddr_in peeraddr_in = { 0 }; /* for peer socket address */

    peeraddr_in.sin_family = AF_INET;
    peeraddr_in.sin_port = htons(port);

    if ((host_ptr = gethostbyname(host)) == NULL) {
P       dbgError(stderr, "gethostbyname(%s) failed", host);
        return -1;
    }

    peeraddr_in.sin_addr.s_addr =
        ((struct in_addr *) (host_ptr->h_addr))->s_addr;

    if (connect(fd, (struct sockaddr *) &peeraddr_in, sizeof(peeraddr_in)) != 0) {
P       dbgError(stderr, "connect to %s:%d failed", host, port);
        return -1;
    }

    return 0;
}

/*
 * Get hostname of peer.
 */
const char *netPeerHost(int sd)
{
    struct sockaddr_in peeraddr;

    socklen_t len = sizeof(struct sockaddr_in);

    getpeername(sd, (struct sockaddr *) &peeraddr, &len);

    return netHost(peeraddr.sin_addr.s_addr);
}

/*
 * Get port number used by peer.
 */
int netPeerPort(int sd)
{
    struct sockaddr_in peeraddr;

    socklen_t len = sizeof(struct sockaddr_in);

    getpeername(sd, (struct sockaddr *) &peeraddr, &len);

    return peeraddr.sin_port;
}

/*
 * Get local hostname.
 */
const char *netLocalHost(int sd)
{
    struct sockaddr_in sockaddr;

    socklen_t len = sizeof(struct sockaddr_in);

    getsockname(sd, (struct sockaddr *) &sockaddr, &len);

    return netHost(sockaddr.sin_addr.s_addr);
}

/*
 * Get local port number.
 */
int netLocalPort(int sd)
{
    struct sockaddr_in peeraddr;

    socklen_t len = sizeof(struct sockaddr_in);

    getsockname(sd, (struct sockaddr *) &peeraddr, &len);

    return ntohs(peeraddr.sin_port);
}
