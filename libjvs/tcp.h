/*
 * Provides a simplified interface to TCP/IP networking.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: net.h 237 2012-02-10 14:09:38Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifndef NET_H
#define NET_H

/* Open a listen port on <host> and <port> and return the corresponding
 * file descriptor. If <host> is NULL the socket will listen on all
 * interfaces. If <port> is equal to 0, the socket will be bound to a
 * random local port (use tcpLocalPort() on the returned fd to find out
 * which). */
int tcpListen(const char *host, int port);

/* Make a connection to <port> on <host> and return the corresponding
 * file descriptor. */
int tcpConnect(const char *host, int port);

/* Get the port that corresponds to service <service>. */
int netPortFor(char *service);

/* Accept an incoming connection request on a listen socket */
int tcpAccept(int sd);

/* Get hostname of peer */
const char *netPeerHost(int sd);

/* Get port number used by peer */
int netPeerPort(int sd);

/* Get local hostname */
const char *netLocalHost(int sd);

/* Get local port number. */
int netLocalPort(int sd);

/* Read from <fd> until <buf> contains exactly <len> bytes. */
int tcpRead(int fd, void *buf, int len);

/* Write all of the <len> bytes in <buf> to <fd>. */
int tcpWrite(int fd, const void *buf, int len);

#endif
