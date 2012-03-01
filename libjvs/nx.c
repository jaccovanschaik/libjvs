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

struct NX {
    List connections;
    List timeouts;
    int listen_fd;
    int (*on_connect)(NX_Conn *conn);
    int (*on_disconnect)(NX_Conn *conn);
    int (*on_error)(NX_Conn *conn, int err);
    int (*on_data)(NX_Conn *conn);
};

typedef struct {
    ListNode _node;
    double t;
    void *udata;
    int (*handler)(NX *nx, double t, void *udata);
} NX_Timeout;

struct NX_Conn {
    ListNode _node;
    NX *nx;
    int fd;
    Buffer *incoming, *outgoing;
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

/* Create a connection for fd <fd>. */

static NX_Conn *nx_create_connection(NX *nx, int fd)
{
    NX_Conn *conn = calloc(1, sizeof(NX_Conn));

    conn->fd = fd;
    conn->nx = nx;

    conn->incoming = bufCreate();
    conn->outgoing = bufCreate();

    listAppendTail(&nx->connections, conn);

    return conn;
}

/* Destroy connection <conn>. */

static void nx_destroy_connection(NX_Conn *conn)
{
    listRemove(&conn->nx->connections, conn);

    if (conn->incoming != NULL) bufDestroy(conn->incoming);
    if (conn->outgoing != NULL) bufDestroy(conn->outgoing);

    free(conn);
}

/* Call the NX callback <handler> for connection <conn>. */

static void nx_callback(NX_Conn *conn, int (*handler)(NX_Conn *conn))
{
    if (handler != NULL) handler(conn);
}

/* Create a Network Exchange. */

NX *nxCreate(const char *host, int port)
{
    NX *nx = calloc(1, sizeof(NX));

    nx->listen_fd = netOpenPort(host, port);

    return nx;
}

/* Close down this Network Exchange. This will cause the mainloop to
 * exit when all connections are closed and no timeouts are left. */

void nxClose(NX *nx)
{
    close(nx->listen_fd);

    nx->listen_fd = -1;
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

int nxQueue(NX_Conn *conn, const char *data, int len)
{
    bufAdd(conn->outgoing, data, len);

    return len;
}

/* Put <len> bytes received from <conn> into <data>. Returns the actual
 * number of bytes put in <data>, which may be less than <len> (even 0).
 */

int nxGet(NX_Conn *conn, char *data, int len)
{
    len = MIN(len, bufLen(conn->incoming));

    memcpy(data, bufGet(conn->incoming), len);

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
 * made. Only one funcation can be set; subsequent calls will overwrite
 * previous ones. <handler> will *not* be called for connections users
 * create themselves (using nxConnect()). */

void nxOnConnect(NX *nx, int (*handler)(NX_Conn *conn))
{
    nx->on_connect = handler;
}

/* Set <handler> as the function to be called when a connection is
 * dropped. Only one funcation can be set; subsequent calls will
 * overwrite previous ones. <handler> will *not* be called for
 * connections users drop themselves (using nxDisconnect()). */

void nxOnDisconnect(NX *nx, int (*handler)(NX_Conn *conn))
{
    nx->on_disconnect = handler;
}

/* Set <handler> as the function to be called when new data comes in.
 * Only one function can be set; subsequent calls will overwrite
 * previous ones. */

void nxOnData(NX *nx, int (*handler)(NX_Conn *conn))
{
    nx->on_data = handler;
}

/* Set <handler> as the function to be called when an error occurs. The
 * connection on which it occurred and the errno code will be passed to
 * the handler. */

void nxOnError(NX *nx, int (*handler)(NX_Conn *conn, int err))
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
        int (*handler)(NX *nx, double t, void *udata))
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

/* Run the Network Exchange. New connection requests from external
 * parties will be accepted automatically, calling the on_connect
 * handler. On errors and end-of-file conditions connections will
 * automatically be closed, calling the on_error and on_disconnect
 * handlers. This function will return either when the select inside
 * fails (returning errno) or when there are no more connections and
 * timeouts left. */

int nxRun(NX *nx)
{
    int nfds, r;
    fd_set rfds, wfds;
    NX_Conn *conn;
    NX_Timeout *tm;
    struct timeval tv, *tvp;

    for ever {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);

        nfds = 0;

        if (nx->listen_fd >= 0) {
            nfds = nx->listen_fd + 1;

            FD_SET(nx->listen_fd, &rfds);
        }

        for (conn = listHead(&nx->connections); conn;
             conn = listNext(conn)) {
            if (conn->fd >= nfds) nfds = conn->fd + 1;

            FD_SET(conn->fd, &rfds);
            if (bufLen(conn->outgoing) > 0) FD_SET(conn->fd, &wfds);
        }

        if ((tm = listHead(&nx->timeouts)) == NULL) {
            tvp = NULL;
        }
        else {
            double dt = tm->t - nxNow();

            if (dt < 0) dt = 0;

            nx_double_to_timeval(dt, &tv);

            tvp = &tv;
        }

        if (nfds > 0) {
            r = select(nfds, &rfds, &wfds, NULL, tvp);
        }
        else if (tvp != NULL) {
            usleep(1000000 * tvp->tv_sec + tvp->tv_usec);

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

            for (conn = listHead(&nx->connections); conn;
                 conn = listNext(conn)) {
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
            listRemove(&nx->timeouts, tm);

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
