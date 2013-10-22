#ifndef LIBJVS_NET_H
#define LIBJVS_NET_H

/*
 * net.h: General networking utility functions.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
*/

#include <stdint.h>

/*
 * Get the host name that belongs to IP address <big_endian_ip>. Returns the fqdn if it can be
 * found, otherwise an IP address in dotted-quad notation.
 */
const char *netHost(uint32_t big_endian_ip);

/*
 * Get the port that corresponds to service <service>.
 */
int netPort(char *service);

/*
 * Bind a socket to <port> and <host>. If <host> is NULL, the socket
 * will be bound to INADDR_ANY. If port is 0, it will be bound to a random port.
 */
int netBind(int socket, const char *host, int port);

/*
 * Connect an existing socket <fd> to <host> and <port>. Returns 0 on success or -1 if an error
 * occurs.
 */
int netConnect(int fd, const char *host, int port);

/*
 * Get hostname of peer.
 */
const char *netPeerHost(int sd);

/*
 * Get port number used by peer.
 */
int netPeerPort(int sd);

/*
 * Get local hostname.
 */
const char *netLocalHost(int sd);

/*
 * Get local port number.
 */
int netLocalPort(int sd);

#endif
