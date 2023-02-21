#ifndef LIBJVS_UDP_H
#define LIBJVS_UDP_H

/*
 * udp.h: provides a simplified interface to UDP networking.
 *
 * udp.h is part of libjvs.
 *
 * Copyright:   (c) 2007-2023 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: udp.h 475 2023-02-21 08:08:11Z jacco $
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
 * Create an unbound IPv4 UDP socket.
 */
int udpSocket(void);

/*
 * Bind IPv4 UDP socket <sd> to the interface associated with host name <host>
 * and port <port>.
 */
int udpBind(int sd, const char *host, uint16_t port);

/*
 * Connect IPv4 UDP socket <sd> to port <port> on host <host>.
 */
int udpConnect(int sd, const char *host, uint16_t port);

/*
 * Send <data> with size <size> via IPv4 UDP socket <sd> to <host>, <port>.
 * Note that this function does a hostname lookup for every call, which can be
 * slow. If possible, use udpConnect() to set a default destination address,
 * after which you can simply write() to the socket without incurring this
 * overhead.
 */
int udpSend(int sd, const char *host, uint16_t port,
        const char *data, size_t size);

/*
 * Create an unbound IPv6 UDP socket.
 */
int udp6Socket(void);

/*
 * Bind IPv6 UDP socket <sd> to the interface associated with host name <host>
 * and port <port>.
 */
int udp6Bind(int sd, const char *host, uint16_t port);

/*
 * Connect IPv6 UDP socket <sd> to port <port> on host <host>.
 */
int udp6Connect(int sd, const char *host, uint16_t port);

/*
 * Send <data> with size <size> via IPv6 UDP socket <sd> to <host>, <port>.
 * Note that this function does a hostname lookup for every call, which can be
 * slow. If possible, use udpConnect() to set a default destination address,
 * after which you can simply write() to the socket without incurring this
 * overhead.
 */
int udp6Send(int sd, const char *host, uint16_t port,
        const char *data, size_t size);

/*
 * Add the socket given by <sd> to the multicast group given by <group> (a
 * dotted-quad ip address).
 */
int udpMulticastJoin(int sd, const char *group);

/*
 * Allow (if <allow_loop> is 1) or disallow (if it is 0) multicast packets to be
 * looped back to the sending network interface. The default is that packets do
 * loop back.
 */
int udpMulticastLoop(int sd, int allow_loop);

/*
 * Set the outgoing UDP Multicast interface for <sd> to the one associated with
 * <address>.
 */
int udpMulticastInterface(int sd, const char *address);

/*
 * Remove the socket given by <sd> from the multicast group given by <group> (a
 * dotted-quad ip address).
 */
int udpMulticastLeave(int sd, const char *group);

#ifdef __cplusplus
}
#endif

#endif
