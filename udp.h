#ifndef LIBJVS_UDP_H
#define LIBJVS_UDP_H

/*
 * udp.h: provides a simplified interface to UDP networking.
 *
 * udp.h is part of libjvs.
 *
 * Copyright:   (c) 2007-2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: udp.h 406 2020-12-19 23:33:04Z jacco $
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
 * Send <data> with size <size> via <fd> to <host>, <port>. Note that this
 * function does a hostname lookup for every call, which can be slow. If
 * possible, use netConnect() to set a default destination address, after
 * which you can simply write() to the socket without incurring this overhead.
 */
int udpSend(int fd, const char *host, uint16_t port,
        const char *data, size_t size);

/*
 * Add the socket given by <fd> to the multicast group given by <group> (a
 * dotted-quad ip address).
 */
int udpMulticastJoin(int fd, const char *group);

/*
 * Allow (if <allow_loop> is 1) or disallow (if it is 0) multicast packets to be
 * looped back to the sending network interface. The default is that packets do
 * loop back.
 */
int udpMulticastLoop(int fd, int allow_loop);

/*
 * Set the outgoing UDP Multicast interface for <fd> to the one associated with
 * <address>.
 */
int udpMulticastInterface(int fd, const char *address);

/*
 * Remove the socket given by <fd> from the multicast group given by <group> (a
 * dotted-quad ip address).
 */
int udpMulticastLeave(int fd, const char *group);

#ifdef __cplusplus
}
#endif

#endif
