/* nx.c: Description
 *
 * Copyright:	(c) 2012 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>

#include "debug.h"
#include "buffer.h"
#include "list.h"
#include "defs.h"
#include "net.h"
#include "nx.h"

typedef struct {
    ListNode _node;
    double t;
    void *udata;
    void (*handler)(NX *nx, double t, void *udata);
} NX_Timeout;

struct NX_Conn {
    NX *nx;
    int fd;
    Buffer *incoming, *outgoing;
};

struct NX {
    NX_Conn *connection[FD_SETSIZE];
    int listen_fd;
    int nfds;
    List timeouts;
    void (*on_connect)(NX_Conn *conn);
    void (*on_disconnect)(NX_Conn *conn);
    void (*on_error)(NX_Conn *conn, int err);
    void (*on_data)(NX_Conn *conn);
};

static int nx_compare_timeouts(const void *p1, const void *p2)
{
    const NX_Timeout *t1 = p1;
    const NX_Timeout *t2 = p2;

    if (t1->t > t2->t)
        return 1;
    else if (t1->t < t2->t)
        return -1;
    else
        return 0;
}

/* Return the timeval in <tv>, converted to a double. */

static double nx_timeval_to_double(const struct timeval *tv)
{
    return tv->tv_sec + tv->tv_usec / 1000000.0;
}

/* Convert double <time> and write it to the timeval at <tv>. */

static void nx_double_to_timeval(double time, struct timeval *tv)
{
    tv->tv_sec = (int) time;
    tv->tv_usec = 1000000 * fmod(time, 1.0);
}

static void nx_count_fds(NX *nx)
{
    int i;

    nx->nfds = nx->listen_fd + 1;

    for (i = 0; i < FD_SETSIZE; i++) {
        if (nx->connection[i] != NULL) nx->nfds = i + 1;
    }
}

/* Destroy connection <conn>. */

static void nx_destroy_connection(NX_Conn *conn)
{
    if (conn->incoming != NULL) bufDestroy(conn->incoming);
    if (conn->outgoing != NULL) bufDestroy(conn->outgoing);

    conn->nx->connection[conn->fd] = NULL;

    nx_count_fds(conn->nx);

    free(conn);
}

/* Create a connection for fd <fd>. */

static NX_Conn *nx_create_connection(NX *nx, int fd)
{
    NX_Conn *conn = calloc(1, sizeof(NX_Conn));

    conn->fd = fd;
    conn->nx = nx;

    conn->incoming = bufCreate();
    conn->outgoing = bufCreate();

    if (nx->connection[fd] != NULL) {
        nx_destroy_connection(nx->connection[fd]);
    }
    else if (fd >= nx->nfds) {
        nx->nfds = fd + 1;
    }

    nx->connection[fd] = conn;

    return conn;
}

/* Call the NX callback <handler> for connection <conn>. */

static void nx_callback(NX_Conn *conn, void (*handler)(NX_Conn *conn))
{
    if (handler != NULL) handler(conn);
}

/* Create a Network Exchange listening on <host> and <port>. <host> may
 * be NULL, in which case the Exchange will listen on all interfaces.
 * <port> may be -1, in which case the system will choose a port number.
 * Use nxListenPort to find out which port was chosen. */

NX *nxCreate(const char *host, int port)
{
    NX *nx = calloc(1, sizeof(NX));

    nx->listen_fd = netOpenPort(host, port);

    if (nx->listen_fd == -1) {
        dbgError(stderr, "netOpenPort failed");
        free(nx);
        return NULL;
    }

    return nx;
}

/* Close down this Network Exchange. Closes the listen port and all
 * other connections and cancels all timeouts. The nxRun() mainloop will
 * then exit. */

void nxClose(NX *nx)
{
    int fd;

    NX_Timeout *tm;

    close(nx->listen_fd);

    nx->listen_fd = -1;

    for (fd = 0; fd < nx->nfds; fd++) {
        if (nx->connection[fd] != NULL) {
            nx_destroy_connection(nx->connection[fd]);
        }
    }

    while ((tm = listRemoveHead(&nx->timeouts)) != NULL) {
        free(tm);
    }
}

/* Return the port that <nx> listens on. */

int nxListenPort(NX *nx)
{
    return netLocalPort(nx->listen_fd);
}

/* Return the hostname that <nx> listens on. May be "0.0.0.0", in which
 * case <nx> listens on all interfaces. */

const char *nxListenHost(NX *nx)
{
    return netLocalHost(nx->listen_fd);
}

/* Return the local port for <conn>. */

int nxLocalPort(NX_Conn *conn)
{
    return netLocalPort(conn->fd);
}

/* Return the local hostname for <conn>. */

const char *nxLocalHost(NX_Conn *conn)
{
    return netLocalHost(conn->fd);
}

/* Return the remote port for <conn>. */

int nxRemotePort(NX_Conn *conn)
{
    return netPeerPort(conn->fd);
}

/* Return the remote hostname for <conn>. */

const char *nxRemoteHost(NX_Conn *conn)
{
    return netPeerHost(conn->fd);
}

/* Queue the first <len> bytes from <data> to be sent over connection
 * <conn>. Returns the number of bytes queued (which is always <len>).
 */

int nxQueue(NX_Conn *conn, const Buffer *data)
{
    bufCat(conn->outgoing, data);

    return bufLen(data);
}

/* Put <len> bytes received from <conn> into <data>. Returns the actual
 * number of bytes put in <data>, which may be less than <len> (even 0).
 */

int nxGet(NX_Conn *conn, Buffer *data)
{
    int len = bufLen(conn->incoming);

    bufCat(data, conn->incoming);

    bufClear(conn->incoming);

    return len;
}

/* Make and return a connection to port <port> on host <host>. */

NX_Conn *nxConnect(NX *nx, const char *host, int port)
{
    int fd = netConnect(host, port);

    NX_Conn *conn = nx_create_connection(nx, fd);

    return conn;
}

/* Disconnect connection <conn>. */

void nxDisconnect(NX_Conn *conn)
{
    shutdown(conn->fd, SHUT_RDWR);

    nx_destroy_connection(conn);
}

/* Set <handler> as the function to be called when a new connection is
 * made. Only one function can be set; subsequent calls will overwrite
 * previous ones. <handler> will *not* be called for connections users
 * create themselves (using nxConnect()). */

void nxOnConnect(NX *nx, void (*handler)(NX_Conn *conn))
{
    nx->on_connect = handler;
}

/* Set <handler> as the function to be called when a connection is
 * dropped. Only one function can be set; subsequent calls will
 * overwrite previous ones. <handler> will *not* be called for
 * connections users drop themselves (using nxDisconnect()). */

void nxOnDisconnect(NX *nx, void (*handler)(NX_Conn *conn))
{
    nx->on_disconnect = handler;
}

/* Set <handler> as the function to be called when new data comes in.
 * Only one function can be set; subsequent calls will overwrite
 * previous ones. */

void nxOnData(NX *nx, void (*handler)(NX_Conn *conn))
{
    nx->on_data = handler;
}

/* Set <handler> as the function to be called when an error occurs. The
 * connection on which it occurred and the errno code will be passed to
 * the handler. */

void nxOnError(NX *nx, void (*handler)(NX_Conn *conn, int err))
{
    nx->on_error = handler;
}

/* Return the current *UTC* time (number of seconds since
 * 1970-01-01/00:00:00 UTC) as a double. */

double nxNow(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return nx_timeval_to_double(&tv);
}

/* Add a timeout at UTC time t, calling <handler> with <nx>, time <t>
 * and <udata>. */

void nxAddTimeout(NX *nx, double t, void *udata,
        void (*handler)(NX *nx, double t, void *udata))
{
    NX_Timeout *tm = calloc(1, sizeof(NX_Timeout));

    tm->t = t;
    tm->handler = handler;
    tm->udata = udata;

    listAppendTail(&nx->timeouts, tm);

    listSort(&nx->timeouts, nx_compare_timeouts);
}

/* Return the NX for <conn>. */

NX *nxFor(NX_Conn *conn)
{
    return conn->nx;
}

/* Return an array of file descriptor that <nx> wants to listen on. The number of returned file
 * descriptors is returned through <count>. */

int *nxFdsForReading(NX *nx, int *count)
{
    int fd;

    static int nfds = 0, *fds = NULL;

    if (fds == NULL || nfds < nx->nfds) {
        nfds = nx->nfds;
        fds = calloc(nfds, sizeof(int));
    }

    *count = 0;

    for (fd = 0; fd < nx->nfds; fd++) {
        if (nx->connection[fd]) {
            fds[*count] = fd;
            (*count)++;
        }
    }

    return fds;
}

/* Return an array of file descriptor that <nx> wants to write to. The number of returned file
 * descriptors is returned through <count>. */

int *nxFdsForWriting(NX *nx, int *count)
{
    int fd;

    static int nfds = 0, *fds = NULL;

    if (fds == NULL || nfds < nx->nfds) {
        nfds = nx->nfds;
        fds = calloc(nfds, sizeof(int));
    }

    *count = 0;

    for (fd = 0; fd < nx->nfds; fd++) {
        if (nx->connection[fd] && bufLen(nx->connection[fd]->outgoing) > 0) {
            fds[*count] = fd;
            (*count)++;
        }
    }

    return fds;
}

/* Prepare arguments for a call to select() on behalf of <nx>. Returned are the nfds, rfds and wfds
 * parameters and a pointer to a struct timeval pointer. */

int nxPrepareSelect(NX *nx, int *nfds, fd_set *rfds, fd_set *wfds, struct timeval **tvpp)
{
    int fd;

    NX_Timeout *tm;

    static struct timeval tv;

    if (nx->listen_fd >= 0) {
        FD_SET(nx->listen_fd, rfds);
    }

    for (fd = 0; fd < nx->nfds; fd++) {
        NX_Conn *conn = nx->connection[fd];

        if (conn == NULL) continue;

        FD_SET(conn->fd, rfds);

        if (bufLen(conn->outgoing) > 0) FD_SET(conn->fd, wfds);
    }

    if (nfds != NULL) *nfds = MAX(*nfds, nx->nfds);

    if ((tm = listHead(&nx->timeouts)) == NULL) {
        *tvpp = NULL;
    }
    else {
        double dt = tm->t - nxNow();

        if (dt < 0) dt = 0;

        nx_double_to_timeval(dt, &tv);

        *tvpp = &tv;
    }

    return 0;
}

/* Run the Network Exchange. New connection requests from external
 * parties will be accepted automatically, calling the on_connect
 * handler. On errors and end-of-file conditions connections will
 * automatically be closed, calling the on_error and on_disconnect
 * handlers. This function will return either when the select inside
 * fails (returning errno) or when there are no more connections and
 * timeouts left. */

int nxRun(NX *nx)
{
    int fd, r;
    fd_set rfds, wfds;
    struct timeval *tv;

    for ever {
        int i, *fds, num_fds;

        FD_ZERO(&rfds);
        FD_ZERO(&wfds);

        fds = nxFdsForReading(nx, &num_fds);
        for (i = 0; i < num_fds; i++) FD_SET(fds[i], &rfds);

        fds = nxFdsForWriting(nx, &num_fds);
        for (i = 0; i < num_fds; i++) FD_SET(fds[i], &wfds);

        nxPrepareSelect(nx, NULL, &rfds, &wfds, &tv);

        if (nx->nfds > 0) {
            r = select(nx->nfds, &rfds, &wfds, NULL, tv);
        }
        else if (tv != NULL) {
            usleep(1000000 * tv->tv_sec + tv->tv_usec);

            r = 0;
        }
        else {
            /* No connections left and no timeouts. */

            r = 0;

            break;
        }

        if (r > 0) {
            if (FD_ISSET(nx->listen_fd, &rfds)) {
                int fd = netAccept(nx->listen_fd);

                NX_Conn *conn = nx_create_connection(nx, fd);

                nx_callback(conn, nx->on_connect);
            }

            for (fd = 0; fd < nx->nfds; fd++) {
                NX_Conn *conn = nx->connection[fd];

                if (conn == NULL) continue;

                if (FD_ISSET(conn->fd, &wfds)) {
                    r = write(conn->fd, bufGet(conn->outgoing),
                            bufLen(conn->outgoing));

                    if (r == -1) {
                        if (nx->on_error) nx->on_error(conn, errno);
                        nxDisconnect(conn);
                    }
                    else {
                        bufTrim(conn->outgoing, r, 0);
                    }
                }
                if (FD_ISSET(conn->fd, &rfds)) {
                    char buffer[9000];

                    r = read(conn->fd, buffer, sizeof(buffer));

                    if (r == -1) {
                        if (nx->on_error) nx->on_error(conn, errno);
                        nxDisconnect(conn);
                    }
                    else if (r == 0) {
                        nx_callback(conn, nx->on_disconnect);
                        nxDisconnect(conn);
                    }
                    else {
                        bufAdd(conn->incoming, buffer, r);
                        nx_callback(conn, nx->on_data);
                    }
                }
            }
        }
        else if (r == 0) {
            NX_Timeout *tm = listRemoveHead(&nx->timeouts);

            tm->handler(nx, tm->t, tm->udata);

            free(tm);
        }
        else if (errno != EINTR) {
            r = errno;
            break;
        }
    }

    return r;
}


#if 0
int nxOwnsFd(NX *nx, int fd)
{
    return fd == nx->listen_fd || nx->connection[fd] != NULL;
}

double nxNextTimeout(NX *nx)
{
    NX_Timeout *timeout = listHead(&nx->timeouts);

    return timeout ? timeout->t : INFINITY;
}

int nxHandleSelect(NX *nx, int return_value, fd_set *rfds, fd_set *wfds)
{
    int fd;

    NX_Conn *conn;

    if (return_value > 0) {
        if (FD_ISSET(nx->listen_fd, rfds)) {
            fd = netAccept(nx->listen_fd);

            conn = nx_create_connection(nx, fd);

            nx_callback(conn, nx->on_connect);
        }

        for (fd = 0; fd < nx->nfds; fd++) {
            conn = nx->connection[fd];

            if (conn == NULL) continue;

            if (FD_ISSET(conn->fd, wfds)) {
                return_value = write(conn->fd, bufGet(conn->outgoing), bufLen(conn->outgoing));

                if (return_value == -1) {
                    if (nx->on_error) nx->on_error(conn, errno);
                    nxDisconnect(conn);
                }
                else {
                    bufTrim(conn->outgoing, return_value, 0);
                }
            }

            if (FD_ISSET(conn->fd, rfds)) {
                char buffer[9000];

                return_value = read(conn->fd, buffer, sizeof(buffer));

                if (return_value == -1) {
                    if (nx->on_error) nx->on_error(conn, errno);
                    nxDisconnect(conn);
                }
                else if (return_value == 0) {
                    nx_callback(conn, nx->on_disconnect);
                    nxDisconnect(conn);
                }
                else {
                    bufAdd(conn->incoming, buffer, return_value);
                    nx_callback(conn, nx->on_data);
                }
            }
        }
    }
    else if (return_value == 0) {
        NX_Timeout *tm = listRemoveHead(&nx->timeouts);

        tm->handler(nx, tm->t, tm->udata);

        free(tm);
    }
    else if (errno != EINTR) {
        return_value = errno;
    }

    return 0;
}

void nxHandleRead(NX *nx, int fd)
{
    if (fd == nx->listen_fd) {
        fd = netAccept(nx->listen_fd);

        NX_Conn *conn = nx_create_connection(nx, fd);

        nx_callback(conn, nx->on_connect);
    }
    else {
        NX_Conn *conn = nx->connection[fd];

        char buffer[9000];

        int r = read(conn->fd, buffer, sizeof(buffer));

        if (r == -1) {
            if (nx->on_error) nx->on_error(conn, errno);
            nxDisconnect(conn);
        }
        else if (r == 0) {
            nx_callback(conn, nx->on_disconnect);
            nxDisconnect(conn);
        }
        else {
            bufAdd(conn->incoming, buffer, r);
            nx_callback(conn, nx->on_data);
        }
    }
}

void nxHandleWrite(NX *nx, int fd)
{
    NX_Conn *conn = nx_create_connection(nx, fd);

    int r = write(conn->fd, bufGet(conn->outgoing), bufLen(conn->outgoing));

    if (r == -1) {
        if (nx->on_error) nx->on_error(conn, errno);

        nxDisconnect(conn);
    }
    else {
        bufTrim(conn->outgoing, r, 0);
    }
}

void nxHandleTimeout(NX *nx)
{
}
#endif

#ifdef TEST
void on_connect(NX_Conn *conn)
{
    dbgPrint(stderr, "local: %s:%d, remote: %s:%d\n",
            nxLocalHost(conn), nxLocalPort(conn),
            nxRemoteHost(conn), nxRemotePort(conn));
}

void on_disconnect(NX_Conn *conn)
{
    dbgPrint(stderr, "\n");
}

void on_data(NX_Conn *conn)
{
    Buffer *buffer = bufCreate();
    int n = nxGet(conn, buffer);

    dbgPrint(stderr, "Got %d bytes: \"%.*s\"\n", n, n, bufGet(buffer));

    if (strncmp(bufGet(buffer), "Hoi!", 4) == 0) {
        bufSet(buffer, "Bye!", 4);

        nxQueue(conn, buffer);
    }
    else if (strncmp(bufGet(buffer), "Bye!", 4) == 0) {
        dbgPrint(stderr, "Calling nxClose()\n");

        nxClose(nxFor(conn));
    }
}

void on_error(NX_Conn *conn, int err)
{
    dbgPrint(stderr, "err = %d (%s)\n", err, strerror(err));
}

void on_timeout(NX *nx, double t, void *udata)
{
    Buffer *buffer = bufCreate();
    NX_Conn *conn = udata;

    dbgPrint(stderr, "timeout, sending welcome string\n");

    bufSet(buffer, "Hoi!", 4);

    nxQueue(conn, buffer);
}

int main(int argc, char *argv[])
{
    int r, listen_port;

    NX *nx;
    NX_Conn *conn;

    nx = nxCreate("localhost", 1234);

    if (nx == NULL) {
        dbgError(stderr, "nxCreate failed");
        return 1;
    }

    listen_port = nxListenPort(nx);

    dbgPrint(stderr, "Listening on host %s\n", nxListenHost(nx));
    dbgPrint(stderr, "Listening on port %d\n", listen_port);

    conn = nxConnect(nx, "localhost", listen_port);

    nxOnConnect(nx, on_connect);
    nxOnDisconnect(nx, on_disconnect);
    nxOnData(nx, on_data);
    nxOnError(nx, on_error);

    nxAddTimeout(nx, nxNow() + 1, conn, on_timeout);

    r = nxRun(nx);

    dbgPrint(stderr, "nxRun exited with %d\n", r);

    return r;
}
#endif
