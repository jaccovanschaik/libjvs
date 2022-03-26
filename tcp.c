/*
 * tcp.c: provides a simplified interface to TCP/IP networking.
 *
 * tcp.c is part of libjvs.
 *
 * Copyright:   (c) 2007-2022 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: tcp.c 460 2022-03-26 12:08:04Z jacco $
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
#include <stdint.h>

#include "net.h"
#include "tcp.h"
#include "defs.h"
#include "debug.h"

static struct linger linger = { 1, 5 }; /* 5 second linger */

/*
 * Create a socket
 */
static int tcp_socket(int family)
{
    int sd;                            /* socket descriptor */

    if ((sd = socket(family, SOCK_STREAM, 0)) == -1) {
        P dbgError(stderr, "socket failed");
    }
    else if (setsockopt(sd, SOL_SOCKET, SO_LINGER,
                        &linger, sizeof(linger)) != 0)
    {
        P dbgError(stderr, "setsockopt(LINGER) failed");
        close(sd);
        sd = -1;
    }

    return sd;
}

/*
 * Open a listen port on <host> and <port>, using address family <family>, and
 * return the corresponding file descriptor. If <host> is NULL the socket will
 * listen on all interfaces. If <port> is equal to 0, the socket will be bound
 * to a random local port (use netLocalPort() on the returned fd to find out
 * which).
 */
static int tcp_listen(const char *host, uint16_t port, int family)
{
    struct addrinfo *info = NULL, *first_info = NULL;
    struct addrinfo  hint = {
        .ai_family   = family,
        .ai_socktype = SOCK_STREAM,
        .ai_flags    = AI_PASSIVE
    };

    char port_str[6];

    snprintf(port_str, sizeof(port_str), "%hu", port);

    int r;

    if ((r = getaddrinfo(host, port_str, &hint, &first_info)) != 0) {
        dbgPrint(stderr, "getaddrinfo for %s:%hu failed: %s",
                host, port, gai_strerror(r));
        return -1;
    }

    int lsd = -1;
    int one =  1;

    for (info = first_info; lsd == -1 && info != NULL; info = info->ai_next) {
        if ((lsd = tcp_socket(info->ai_family)) == -1) {
            P dbgError(stderr, "tcp_socket() failed");
        }
        else if (family == AF_INET6 &&
                 setsockopt(lsd, IPPROTO_IPV6, IPV6_V6ONLY,
                            &one, sizeof(one)) != 0) {
            P dbgError(stderr, "setsockopt(IPV6_V6ONLY) failed");
            close(lsd);
            lsd = -1;
        }
        else if (setsockopt(lsd, SOL_SOCKET, SO_REUSEADDR,
                            &one, sizeof(one)) != 0) {
            P dbgError(stderr, "setsockopt(REUSEADDR) failed");
            close(lsd);
            lsd = -1;
        }
        else if (bind(lsd, info->ai_addr, info->ai_addrlen) < 0) {
            P dbgAbort(stderr, "bind() failed");
            close(lsd);
            lsd = -1;
        }
        else if (listen(lsd, SOMAXCONN) == -1) {
            P dbgError(stderr, "listen() failed");
            close(lsd);
            lsd = -1;
        }
    }

    freeaddrinfo(first_info);

    return lsd;
}

/*
 * Make an IPv4 connection to <port> on <host> and return the corresponding
 * file descriptor.
 */
static int tcp_connect(const char *host, uint16_t port, int family)
{
    int r, sd;

    char port_as_text[6];
    struct addrinfo *info;

    struct addrinfo hints = {
        .ai_family = family,
        .ai_socktype = SOCK_STREAM
    };

    snprintf(port_as_text, sizeof(port_as_text), "%hu", port);

    if ((r = getaddrinfo(host, port_as_text, &hints, &info)) != 0) {
        dbgPrint(stderr, "%s: getaddrinfo failed: %s",
                __func__, gai_strerror(r));

        return -1;
    }

    do {
        if ((sd = tcp_socket(info->ai_family)) == -1) {
            continue;
        }
        else if ((r = connect(sd, info->ai_addr, info->ai_addrlen)) < 0) {
            close(sd);
            continue;
        }
        else {
            break;
        }
    } while ((info = info->ai_next) != NULL);

    freeaddrinfo(info);

    if (sd == -1) {
        P dbgError(stderr, "socket() failed");
        return -1;
    }
    else if (r == -1) {
        P dbgError(stderr, "connect() failed");
        return -1;
    }

    return sd;
}

/*
 * Open an IPv4 listen port on <host> and <port> and return the corresponding
 * file descriptor. If <host> is NULL the socket will listen on all
 * interfaces. If <port> is equal to 0, the socket will be bound to a random
 * local port (use netLocalPort() on the returned fd to find out which).
 */
int tcp4Listen(const char *host, uint16_t port)
{
    return tcp_listen(host, port, AF_INET);
}

/*
 * Make an IPv4 connection to <port> on <host> and return the corresponding
 * file descriptor.
 */
int tcp4Connect(const char *host, uint16_t port)
{
    return tcp_connect(host, port, AF_INET);
}

/*
 * Open an IPv6 listen port on <host> and <port> and return the corresponding
 * file descriptor. If <host> is NULL the socket will listen on all
 * interfaces. If <port> is equal to 0, the socket will be bound to a random
 * local port (use netLocalPort() on the returned fd to find out which).
 */
int tcp6Listen(const char *host, uint16_t port)
{
    return tcp_listen(host, port, AF_INET6);
}

/*
 * Make an IPv6 TCP connection to <port> on <host> and return the corresponding
 * file descriptor.
 */
int tcp6Connect(const char *host, uint16_t port)
{
    return tcp_connect(host, port, AF_INET6);
}

/*
 * Open a listen socket on <host> and <port> and return the corresponding file
 * descriptor. The socket will be IPv4 or IPv6 depending on the first usable
 * addrinfo struct returned by getaddrinfo. If <host> is NULL the socket will
 * listen on all interfaces. If <port> is equal to 0, the socket will be bound
 * to a random local port (use netLocalPort() on the returned fd to find out
 * which).
 */
int tcpListen(const char *host, uint16_t port)
{
    return tcp_listen(host, port, AF_UNSPEC);
}

/*
 * Make a TCP connection to <port> on <host> and return the corresponding file
 * descriptor. The connection will be IPv4 or IPv6 depending on the first
 * usable addrinfo returned by getaddrinfo.
 */
int tcpConnect(const char *host, uint16_t port)
{
    return tcp_connect(host, port, AF_UNSPEC);
}

/*
 * Accept an incoming (IPv4 or IPv6) connection request on a listen socket.
 */
int tcpAccept(int sd)
{
    int csd;

    do {
        csd = accept(sd, NULL, NULL);
    } while (csd == -1 && errno == EINTR);

    if (csd == -1) {
        P dbgError(stderr, "accept failed");
    }

    return csd;
}

/*
 * Read from <fd> until <buf> contains exactly <len> bytes.
 */
int tcpRead(int fd, void *buf, int len)
{
    int res, n = 0;

    while (1) {
        if ((res = read(fd, (char *) buf + n, len - n)) == 0) {
            break;
        }
        else if (res > 0 && (n += res) == len) {
            break;
        }
        else if (res < 0 && errno == EINTR) {
            errno = 0;
            continue;
        }
        else if (res < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            errno = 0;
            usleep(100000);
            continue;
        }
    }

    if (res == -1) {
        P dbgError(stderr, "read failed");
        return -1;
    }
    else {
        return n;
    }
}

/*
 * Write all of the <len> bytes in <buf> to <fd>.
 */
int tcpWrite(int fd, const void *buf, int len)
{
    int res, n = 0;

    do {
        res = write(fd, (const char *) buf + n, len - n);
    } while ((res > 0 || errno == EINTR) && (n += res) < len);

    if (res == -1) {
        P dbgError(stderr, "write failed");
        return -1;
    }
    else {
        return n;
    }
}

#ifdef TEST
#include "net.h"

static int errors = 0;

static void test_msg(int send_fd, int recv_fd, char *msg)
{
    int r;
    char buf[32];

    if ((r = tcpWrite(send_fd, msg, strlen(msg))) != (int) strlen(msg)) {
        fprintf(stderr, "tcpWrite to send_fd failed.\n");
        errors++;
    }
    else if ((r = tcpRead(recv_fd, buf, strlen(msg))) != (int) strlen(msg)) {
        fprintf(stderr, "tcpRead from recv_fd failed.\n");
        errors++;
    }
    else if (strncmp(buf, msg, strlen(msg)) != 0) {
        fprintf(stderr, "buf = \"%s\", msg = \"%s\"\n", buf, msg);
        errors++;
    }
}

int main(void)
{
    char *client_msg = "Hello client!";
    char *server_msg = "Hello server!";

    int listen_fd, client_fd, server_fd;

    if ((listen_fd = tcpListen("localhost", 54321)) == -1) {
        dbgError(stderr, "tcpListen() failed");
        return 1;
    }
    else if ((client_fd = tcpConnect("localhost", 54321)) == -1) {
        dbgError(stderr, "tcpConnect() failed");
        return 1;
    }
    else if ((server_fd = tcpAccept(listen_fd)) == -1) {
        dbgError(stderr, "tcpAccept() failed");
        return 1;
    }

    test_msg(client_fd, server_fd, server_msg);
    test_msg(server_fd, client_fd, client_msg);

    close(listen_fd);
    close(client_fd);
    close(server_fd);

    listen_fd = tcpListen(NULL, 0);

    uint16_t port = netLocalPort(listen_fd);

    client_fd = tcpConnect("localhost", port);
    server_fd = tcpAccept(listen_fd);

    test_msg(client_fd, server_fd, server_msg);
    test_msg(server_fd, client_fd, client_msg);

    close(listen_fd);
    close(client_fd);
    close(server_fd);

    return errors;
}

#endif
