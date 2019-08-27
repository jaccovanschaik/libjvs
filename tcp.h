#ifndef LIBJVS_TCP_H
#define LIBJVS_TCP_H

/*
 * tcp.h: provides a simplified interface to TCP/IP networking.
 *
 * tcp.h is part of libjvs.
 *
 * Copyright:   (c) 2007-2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: tcp.h 343 2019-08-27 08:39:24Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * Open a listen port on <host> and <port> and return the corresponding
 * file descriptor. If <host> is NULL the socket will listen on all
 * interfaces. If <port> is equal to 0, the socket will be bound to a
 * random local port (use netLocalPort() on the returned fd to find out
 * which).
 */
int tcpListen(const char *host, uint16_t port);

/*
 * Make a connection to <port> on <host> and return the corresponding
 * file descriptor.
 */
int tcpConnect(const char *host, uint16_t port);

/*
 * Accept an incoming connection request on a listen socket.
 */
int tcpAccept(int sd);

/*
 * Read from <fd> until <buf> contains exactly <len> bytes.
 */
int tcpRead(int fd, void *buf, int len);

/*
 * Write all of the <len> bytes in <buf> to <fd>.
 */
int tcpWrite(int fd, const void *buf, int len);

#ifdef __cplusplus
}
#endif

#endif
