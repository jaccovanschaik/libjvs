/*
 * Provides a simplified interface to TCP/IP networking.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: net.h 242 2007-06-23 23:12:05Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifndef NET_H
#define NET_H

/* Open a listen port (using a port number) */
int netOpenPort(int port);

/* Connect to a port (using a port number) */
int netConnect(int port, char *server);

/* Get the port that corresponds to service <service>. */
int netPortFor(char *service);

/* Accept an incoming connection request on a listen socket */
int netAccept(int sd);

/* Get hostname of peer */
char *netGetPeerName(int sd);

/* Read until <buf> is full */
int netRead(int fd, void *buf, int len);

/* Write all of <buf> */
int netWrite(int fd, void *buf, int len);

#endif
