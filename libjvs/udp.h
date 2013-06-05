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

/* Make a connection to <port> on <host> and return the corresponding
 * file descriptor. */
int udpConnect(const char *host, int port);

#endif
