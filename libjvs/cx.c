/*
 * cx.c: Communications Exchange.
 *
 * Copyright:	(c) 2012 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <assert.h>

#include "buffer.h"
#include "list.h"
#include "tcp.h"
#include "defs.h"
#include "debug.h"

#include "cx.h"

typedef enum {
    CX_CT_NONE,
    CX_CT_FILE,
    CX_CT_LISTEN,
    CX_CT_SOCKET,
    CX_CT_COUNT
} CX_ConnectionType;

typedef struct {
    CX_ConnectionType type;
    Buffer incoming, outgoing;
    void *on_file_data_udata;
    void (*on_file_data)(CX *cx, int fd, void *udata);
} CX_Connection;

typedef struct {
    ListNode _node;
    double t;
    void *on_time_udata;
    void (*on_time)(CX *cx, double t, void *udata);
} CX_Timeout;

struct CX {
    int num_connections;
    CX_Connection **connection;
    void *on_socket_data_udata;
    void (*on_socket_data)(CX *cx, int fd, const char *data, size_t size, void *udata);
    void *on_error_udata;
    void (*on_error)(CX *cx, int fd, int error, void *udata);
    void *on_connect_udata;
    void (*on_connect)(CX *cx, int fd, void *udata);
    void *on_disconnect_udata;
    void (*on_disconnect)(CX *cx, int fd, void *udata);
    List timeouts;
};

static int cx_compare_timeouts(const void *p1, const void *p2)
{
    const CX_Timeout *t1 = p1;
    const CX_Timeout *t2 = p2;

    if (t1->t > t2->t)
        return 1;
    else if (t1->t < t2->t)
        return -1;
    else
        return 0;
}

/*
 * Convert double <time> and write it to the timeval at <tv>.
 */
static void cx_double_to_timeval(double time, struct timeval *tv)
{
    tv->tv_sec = (int) time;
    tv->tv_usec = 1000000 * fmod(time, 1.0);
}

static void cx_add_file(CX *cx, int fd, CX_ConnectionType type)
{
    int new_num_connections = fd + 1;

    if (new_num_connections > cx->num_connections) {
        cx->connection = realloc(cx->connection, new_num_connections * sizeof(CX_Connection *));

        memset(cx->connection + cx->num_connections, 0,
                sizeof(CX_Connection *) * (new_num_connections - cx->num_connections));

        cx->num_connections = new_num_connections;
    }

    if (cx->connection[fd] == NULL) {
        cx->connection[fd] = calloc(1, sizeof(CX_Connection));
    }

    cx->connection[fd]->type = type;
}

/*
 * Create a communications exchange.
 */
CX *cxCreate(void)
{
    return calloc(1, sizeof(CX));
}

/*
 * Subscribe to input. When data is available on <fd>, <on_file_data> will be called with the given
 * <cx>, <fd> and <udata>. Only one handler per file descriptor can be set, subsequent calls will
 * override earlier ones.
 */
void cxOnFileData(CX *cx, int fd, void (*on_file_data)(CX *cx, int fd, void *udata), void *udata)
{
    cx_add_file(cx, fd, CX_CT_FILE);

    cx->connection[fd]->on_file_data_udata = udata;
    cx->connection[fd]->on_file_data = on_file_data;
}

/*
 * Drop subscription to fd <fd>.
 */
void cxDropFile(CX *cx, int fd)
{
    int new_num_connections;

    if (fd < cx->num_connections) cx->connection[fd] = NULL;

    for (fd = cx->num_connections - 1; fd >= 0; fd--) {
        if (cx->connection[fd] != NULL) break;
    }

    new_num_connections = fd + 1;

    if (new_num_connections < cx->num_connections) {
        cx->connection = realloc(cx->connection, new_num_connections * sizeof(CX_Connection *));

        cx->num_connections = new_num_connections;
    }
}

/*
 * Return the current UTC time (number of seconds since 1970-01-01/00:00:00 UTC) as a double.
 */
double cxNow(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

/*
 * Set a handler to be called at time <t> (in seconds since 1970-01-01/00:00:00 UTC). <on_time> will
 * be called with the given <cx>, <t> and <udata>.
 */
void cxOnTime(CX *cx, double t, void (*on_time)(CX *cx, double t, void *udata), void *udata)
{
    CX_Timeout *tm = calloc(1, sizeof(CX_Timeout));

    tm->t = t;
    tm->on_time_udata = udata;
    tm->on_time = on_time;

    listAppendTail(&cx->timeouts, tm);

    listSort(&cx->timeouts, cx_compare_timeouts);
}

/*
 * Drop timeout at time <t>. Both <t> and <on_time> must match the earlier call to cxOnTime.
 */
void cxDropTime(CX *cx, double t, void (*on_time)(CX *cx, double t, void *udata))
{
    CX_Timeout *tm, *next;

    for (tm = listHead(&cx->timeouts); tm; tm = next) {
        next = listNext(tm);

        if (tm->t == t && tm->on_time == on_time) {
            listRemove(&cx->timeouts, tm);
            free(tm);
        }
    }
}

/*
 * Call <handler> with <udata> when a new connection is made on <fd>.
 */
void cxOnConnect(CX *cx, void (*handler)(CX *cx, int fd, void *udata), void *udata)
{
    cx->on_connect = handler;
    cx->on_connect_udata = udata;
}

/*
 * Call <handler> with <udata> when the connection on <fd> is lost.
 */
void cxOnDisconnect(CX *cx, void (*handler)(CX *cx, int fd, void *udata), void *udata)
{
    cx->on_disconnect = handler;
    cx->on_disconnect_udata = udata;
}

/*
 * Call <handler> when an error has occurred on <fd>. <error> is the associated errno.
 */
void cxOnSocketError(CX *cx, void (*handler)(CX *cx, int fd, int error, void *udata), void *udata)
{
    cx->on_error = handler;
    cx->on_error_udata = udata;
}

/*
 * Call <handler> when new data on one of the connected sockets comes in.
 */
void cxOnSocketData(CX *cx,
        void (*handler)(CX *cx, int fd, const char *data, size_t size, void *udata),
        void *udata)
{
    cx->on_socket_data = handler;
    cx->on_socket_data_udata = udata;
}

/*
 * Open a listen socket bound to <host> and <port>.
 */
int cxListen(CX *cx, const char *host, int port)
{
    int fd = tcpListen(host, port);

    if (fd < 0) return -1;

    cx_add_file(cx, fd, CX_CT_LISTEN);

    return fd;
}

/*
 * Make a connection to <host> on <port>.
 */
int cxConnect(CX *cx, const char *host, int port)
{
    int fd = tcpConnect(host, port);

    if (fd < 0) return -1;

    cx_add_file(cx, fd, CX_CT_SOCKET);

    return fd;
}

/*
 * Add <data> with <size> to the output buffer of <fd>. The data will be sent when the
 * flow-of-control moves back to the main loop.
 */
void cxSend(CX *cx, int fd, const char *data, size_t size)
{
    CX_Connection *conn = cx->connection[fd];

    assert(fd >= 0 && fd < cx->num_connections);
    assert(conn != NULL);

    bufAdd(&conn->outgoing, data, size);
}

/*
 * Clear <rfds>, then fill it with the file descriptors that have been given to <cx> in a
 * cxOnFileData call. Return the number of file descriptors that may be set. <rfds> can then be
 * passed to select().
 */
int cxGetReadFDs(CX *cx, fd_set *rfds)
{
    int fd;

    FD_ZERO(rfds);

    for (fd = 0; fd < cx->num_connections; fd++) {
        if (cx->connection[fd] == NULL) continue;

        FD_SET(fd, rfds);
    }

    return cx->num_connections;
}

/*
 * Return TRUE if <fd> is handled by <cx>.
 */
int cxOwnsFD(CX *cx, int fd)
{
    return cx->connection[fd] != NULL;
}

/*
 * Get the timeout to use for a call to select. If a timeout is required it is copied to <tv>, and 1
 * is returned. Otherwise 0 is returned, and the last parameter of select should be set to NULL.
 * <tv> is not changed in that case.
 */
int cxGetTimeout(CX *cx, struct timeval *tv)
{
    CX_Timeout *tm;

    if ((tm = listHead(&cx->timeouts)) == NULL) {
        return 0;
    }
    else {
        double dt = tm->t - cxNow();

        if (dt < 0) dt = 0;

        cx_double_to_timeval(dt, tv);

        return 1;
    }
}

static void cx_handle_input(CX *cx, int fd)
{
    int r;
    char buffer[9000];

    CX_Connection *conn;

    assert(fd >= 0 && fd < cx->num_connections);
    assert(cx->connection[fd] != NULL);

    conn = cx->connection[fd];

    assert(conn->type > CX_CT_NONE && conn->type < CX_CT_COUNT);

    switch(conn->type) {
    case CX_CT_LISTEN:
        fd = tcpAccept(fd);
        cx_add_file(cx, fd, CX_CT_SOCKET);
        break;
    case CX_CT_FILE:
        dbgPrint(stderr, "CX_CT_FILE\n");
        if (conn->on_file_data) {
            conn->on_file_data(cx, fd, conn->on_file_data_udata);
        }
        break;
    case CX_CT_SOCKET:
        r = read(fd, buffer, sizeof(buffer));
        bufSet(&conn->incoming, buffer, r);
        if (cx->on_socket_data) {
            cx->on_socket_data(cx, fd, bufGet(&conn->incoming), bufLen(&conn->incoming),
                    cx->on_socket_data_udata);
        }
        bufClear(&conn->incoming);
        break;
    default:
        break;
    }
}

/*
 * Process the results from a select() call.
 */
int cxProcessSelect(CX *cx, int r, fd_set *rfds)
{
    if (r == -1) {
        if (errno != EINTR) return -1;
    }
    else if (r == 0) {
        CX_Timeout *tm = listRemoveHead(&cx->timeouts);

        tm->on_time(cx, tm->t, tm->on_time_udata);

        free(tm);
    }
    else {
        int fd;

        for (fd = 0; fd < cx->num_connections; fd++) {
            CX_Connection *conn = cx->connection[fd];

            if (conn == NULL) continue;

            if (FD_ISSET(fd, rfds)) {
                cx_handle_input(cx, fd);
                conn->on_file_data(cx, fd, conn->on_file_data_udata);
            }
        }
    }

    return 0;
}

/*
 * Run the communications exchange. This function will return when there are no more timeouts to
 * wait for and no file descriptors to listen on (which can be forced by calling cxClose()).
 */
int cxRun(CX *cx)
{
    int r = 0;

    while (r >= 0) {
        struct timeval tv;
        fd_set rfds;

        int nfds = cx->num_connections;

        cxGetReadFDs(cx, &rfds);

        r = cxGetTimeout(cx, &tv);

        dbgPrint(stderr, "nfds = %d, r = %d\n", nfds, r);

        if (nfds == 0 && r == 0) break;

        r = select(nfds, &rfds, NULL, NULL, r ? &tv : NULL);

        r = cxProcessSelect(cx, r, &rfds);
    }

    return r;
}

/*
 * Close down Communications Exchange <cx>. This forcibly stops <cx> from listening on any file
 * descriptor and removes all pending timeouts, which causes cxRun() to return.
 */
void cxClose(CX *cx)
{
    int fd;
    CX_Timeout *tm;

    while ((tm = listRemoveHead(&cx->timeouts)) != NULL) {
        free(tm);
    }

    for (fd = 0; fd < cx->num_connections; fd++) {
        if (cx->connection[fd] != NULL) {
            free(cx->connection[fd]);
            cx->connection[fd] = NULL;
        }
    }

    cx->num_connections = 0;
}

/*
 * Free the memory occupied by <cx>. Call this outside of the cx loop, i.e. after cxRun() returns.
 * You can force cxRun() to return by calling cxClose().
 */
void cxFree(CX *cx)
{
    cxClose(cx);    /* Should not be necessary, just to be safe. */

    free(cx);
}

#ifdef TEST
#include "net.h"

static char *ping = "Ping!";
static char *echo = "Echo!";

/*
 * -- Client --
 */

void client_handle_data(CX *cx, int fd, void *udata)
{
    char buffer[80] = "";

    int r = read(fd, buffer, sizeof(buffer));

    if (strncmp(buffer, echo, r) == 0) {
P       dbgPrint(stderr, "Got reply (%*s), closing down.\n", r, buffer);
    }
    else {
        dbgPrint(stderr, "Got unexpected reply (%*s).\n", r, buffer);
        exit(-1);
    }

    cxClose(cx);
}

void client_handle_timeout(CX *cx, double t, void *udata)
{
    int fd = *((int *) udata);

P   dbgPrint(stderr, "Timeout, sending ping...\n");

    tcpWrite(fd, ping, strlen(ping));
}

void client(int port)
{
    CX *cx = cxCreate();

    int fd = tcpConnect("localhost", port);

    cxOnFileData(cx, fd, client_handle_data, NULL);

    cxOnTime(cx, cxNow() + 1, client_handle_timeout, &fd);

    cxRun(cx);

P   dbgPrint(stderr, "cxRun() returned\n");
}

/*
 * -- Server --
 */

void server_handle_data(CX *cx, int fd, void *udata)
{
    char buffer[80] = "";

    int r = read(fd, buffer, sizeof(buffer));

    if (r == 0) {
P       dbgPrint(stderr, "End of file on fd %d, closing down.\n", fd);

        cxDropFile(cx, fd);

        cxClose(cx);
    }
    else if (strncmp(buffer, ping, r) == 0) {
P       dbgPrint(stderr, "Got ping, sending echo.\n");

        tcpWrite(fd, echo, strlen(echo));
    }
    else {
        dbgPrint(stderr, "Got unexpected request: \"%s\"\n", buffer);
        exit(-1);
    }
}

void server_accept_connection(CX *cx, int fd, void *udata)
{
P   dbgPrint(stderr, "Accepting new connection\n");

    fd = tcpAccept(fd);

    cxOnFileData(cx, fd, server_handle_data, NULL);
}

int main(int argc, char *argv[])
{
    CX *cx = cxCreate();

    int fd = tcpListen(NULL, 0);
    int port = netLocalPort(fd);

    if (fork() == 0) {
        client(port);
    }
    else {
        cxOnFileData(cx, fd, server_accept_connection, NULL);

        cxRun(cx);
    }

    return 0;
}
#endif
