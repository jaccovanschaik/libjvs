#ifndef LIBJVS_TCP_H
#define LIBJVS_TCP_H

/*
 * Provides a simplified interface to TCP/IP networking.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

/* Open a listen port on <host> and <port> and return the corresponding
 * file descriptor. If <host> is NULL the socket will listen on all
 * interfaces. If <port> is equal to 0, the socket will be bound to a
 * random local port (use tcpLocalPort() on the returned fd to find out
 * which). */
int tcpListen(const char *host, int port);

/* Make a connection to <port> on <host> and return the corresponding
 * file descriptor. */
int tcpConnect(const char *host, int port);

/* Accept an incoming connection request on a listen socket */
int tcpAccept(int sd);

/* Read from <fd> until <buf> contains exactly <len> bytes. */
int tcpRead(int fd, void *buf, int len);

/* Write all of the <len> bytes in <buf> to <fd>. */
int tcpWrite(int fd, const void *buf, int len);

#endif
