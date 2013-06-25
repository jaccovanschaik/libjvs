/*
 * Provides a simplified interface to TCP/IP networking.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: net.h 237 2012-02-10 14:09:38Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifndef UDP_H
#define UDP_H

/*
 * Create a UDP socket and bind it to <host> and <port>.
 */
int udpSocket(const char *host, int port);

/*
 * Create a UDP socket and "connect" it to <host> and <port> (which means that any send without an
 * address will go to that address by default).
 */
int udpConnect(const char *host, int port);

#endif
