#ifndef LIBJVS_UDP_H
#define LIBJVS_UDP_H

/*
 * Provides a simplified interface to UDP networking.
 *
 * Part of libjvs.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/*
 * Create an unbound UDP socket.
 */
int udpSocket(void);

/*
 * Send <data> with size <size> via <fd> to <host>, <port>. Note that this function does a hostname
 * lookup for every call, which can be slow. If possible, use netConnect() to set a default address,
 * after which you can simply write() to the socket without incurring this overhead.
 */
int udpSend(int fd, const char *host, uint16_t port, const char *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif
