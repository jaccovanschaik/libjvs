#ifndef LIBJVS_NET_H
#define LIBJVS_NET_H

/*
 * net.h: General networking utility functions.
 *
 * net.h is part of libjvs.
 *
 * Copyright:   (c) 2007-2022 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: net.h 467 2022-11-20 00:05:38Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * Get the host name that belongs to IP address <big_endian_ip>. Returns the
 * fqdn if it can be found, otherwise an IP address in dotted-quad notation.
 */
const char *netHost(uint32_t big_endian_ip);

/*
 * Get the port that corresponds to service <service>.
 */
uint16_t netPort(const char *service, const char *protocol);

/*
 * Get hostname of peer.
 */
const char *netPeerHost(int sd);

/*
 * Get port number used by peer.
 */
uint16_t netPeerPort(int sd);

/*
 * Get local hostname.
 */
const char *netLocalHost(int sd);

/*
 * Get local port number.
 */
uint16_t netLocalPort(int sd);

#ifdef __cplusplus
}
#endif

#endif
