/*
 * net.c: Description
 *
 * Author:	Jacco van Schaik (jacco.van.schaik@dnw.aero)
 * Copyright:	(c) 2013 DNW German-Dutch Windtunnels
 * Created:	2013-06-05
 * Version:	$Id$
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
    struct hostent *ent = gethostbyaddr((char *) &big_endian_ip, sizeof(big_endian_ip), AF_INET);

    if (ent == NULL) {
        static char text_buffer[16];

        snprintf(text_buffer, sizeof(text_buffer), "%d.%d.%d.%d",
                 (big_endian_ip & 0xFF000000) >> 24,
                 (big_endian_ip & 0x00FF0000) >> 16,
                 (big_endian_ip & 0x0000FF00) >> 8,
                 (big_endian_ip & 0x000000FF));

        return text_buffer;
    }

    return ent->h_name;
}

/*
 * Get the port that corresponds to service <service>.
 */
int netPort(char *service)
{
    struct servent *serv_ptr;          /* pointer to service information */

    serv_ptr = getservbyname(service, "tcp");

    if (serv_ptr == NULL) {
        dbgError(stderr, "getservbyname(%s) failed", service);
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
