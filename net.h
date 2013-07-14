#ifndef NET_H
#define NET_H

/*
 * net.h: General networking utility functions.
 *
 * Author:	Jacco van Schaik (jacco.van.schaik@dnw.aero)
 * Copyright:	(c) 2013 DNW German-Dutch Windtunnels
 * Created:	2013-06-05
 * Version:	$Id$
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
