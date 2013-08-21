/*
 * Provides a simplified interface to TCP/IP networking.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifndef UDP_H
#define UDP_H

/*
 * Create an unbound UDP socket.
 */
int udpSocket(void);

/*
 * Send <data> with size <size> via <fd> to <host>, <port>. Note that this function does a hostname
 * lookup for every call, which can be slow. If possible, use netConnect() to set a default address,
 * after which you can simply write() to the socket without incurring this overhead.
 */
int udpSend(int fd, const char *host, int port, const char *data, size_t size);

#endif
