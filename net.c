/*
 * net.c: General networking utility functions.
 *
 * net.c is part of libjvs.
 *
 * Copyright:   (c) 2007-2021 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: net.c 438 2021-08-19 10:10:03Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <netdb.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include "defs.h"
#include "net.h"
#include "debug.h"

/*
 * Get the host name that belongs to IP address <big_endian_ip>. Returns the
 * fqdn if it can be found, otherwise an IP address in dotted-quad notation.
 */
const char *netHost(uint32_t big_endian_ip)
{
    static char text_buffer[16];
    struct hostent *ent;

    if (big_endian_ip == 0) {
        snprintf(text_buffer, sizeof(text_buffer), "0.0.0.0");

        return text_buffer;
    }
    else if ((ent = gethostbyaddr((char *) &big_endian_ip,
                    sizeof(big_endian_ip), AF_INET)) == NULL)
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
uint16_t netPort(const char *service, const char *protocol)
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
uint16_t netPeerPort(int sd)
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
uint16_t netLocalPort(int sd)
{
    struct sockaddr_in peeraddr;

    socklen_t len = sizeof(struct sockaddr_in);

    getsockname(sd, (struct sockaddr *) &peeraddr, &len);

    return ntohs(peeraddr.sin_port);
}
