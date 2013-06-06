/*
 * Provides a simplified interface to UDP networking.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: net.c 237 2012-02-10 14:09:38Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>

#include "net.h"
#include "tcp.h"
#include "debug.h"

static struct linger linger = { 1, 5 }; /* 5 second linger */

static int one = 1;

/*
 * Create a UDP socket.
 */
int udpSocket(void)
{
    int sd;                            /* socket descriptor */

    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        dbgError(stderr, "unable to create socket");
        return -1;
    }

    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0) {
        dbgError(stderr, "setsockopt(REUSEADDR) failed");
        return -1;
    }

    if (setsockopt(sd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) != 0) {
        dbgError(stderr, "setsockopt(LINGER) failed");
        return -1;
    }

    return sd;
}

/*
 * Create a UDP socket and "connect" it to <host> and <port> (which means that any send without an
 * address will go to that address by default).
 */
int udpConnect(const char *host, int port)
{
    int fd = udpSocket();

    if (netConnect(fd, host, port) != 0) {
        dbgError(stderr, "netConnect failed");
        close(fd);
        return -1;
    }

    return fd;
}

#ifdef TEST
#include "net.h"

static int errors = 0;

void _make_sure_that(const char *file, int line, const char *str, int val)
{
   if (!val) {
      fprintf(stderr, "%s:%d: Expression \"%s\" failed\n", file, line, str);
      errors++;
   }
}

#define make_sure_that(expr) _make_sure_that(__FILE__, __LINE__, #expr, (expr))

int main(int argc, char *argv[])
{
    int r;
    char buffer[16];

    int fd1 = udpSocket();
    int fd2 = udpSocket();

    netConnect(fd1, "localhost", 1234);
    netBind(fd2, "localhost", 1234);

    write(fd1, "Hoi!", 4);
    r = read(fd2, buffer, sizeof(buffer));

    make_sure_that(r == 4);
    make_sure_that(strncmp(buffer, "Hoi!", 4) == 0);

    return errors;
}
#endif
