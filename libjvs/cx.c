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

#include "buffer.h"
#include "list.h"
#include "tcp.h"
#include "defs.h"
#include "debug.h"

#include "cx.h"

typedef struct {
    void *udata;
    int (*handler)(CX *cx, int fd, void *udata);
} CX_Connection;

typedef struct {
    ListNode _node;
    double t;
    void *udata;
    int (*handler)(CX *cx, double t, void *udata);
} CX_Timeout;

struct CX {
    int num_connections;
    CX_Connection **connection;
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

/*
 * Create a communications exchange.
 */
CX *cxCreate(void)
{
    return calloc(1, sizeof(CX));
}

/*
 * Subscribe to input. When data is available on <fd>, <handler> will be called with the given <cx>,
 * <fd> and <udata>. Only one handler per file descriptor can be set, subsequent calls will override
 * earlier ones.
 */
void cxAddFile(CX *cx, int fd, int (*handler)(CX *cx, int fd, void *udata), void *udata)
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

    cx->connection[fd]->udata = udata;
    cx->connection[fd]->handler = handler;
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
 * Set a handler to be called at time <t> (in seconds since 1970-01-01/00:00:00 UTC). <handler> will
 * be called with the given <cx>, <t> and <udata>.
 */
void cxAddTime(CX *cx, double t, int (*handler)(CX *cx, double t, void *udata), void *udata)
{
    CX_Timeout *tm = calloc(1, sizeof(CX_Timeout));

    tm->t = t;
    tm->udata = udata;
    tm->handler = handler;

    listAppendTail(&cx->timeouts, tm);

    listSort(&cx->timeouts, cx_compare_timeouts);
}

/*
 * Drop timeout at time <t>. Both <t> and <handler> must match the earlier call to cxAddTime.
 */
void cxDropTime(CX *cx, double t, int (*handler)(CX *cx, double t, void *udata))
{
    CX_Timeout *tm, *next;

    for (tm = listHead(&cx->timeouts); tm; tm = next) {
        next = listNext(tm);

        if (tm->t == t && tm->handler == handler) {
            listRemove(&cx->timeouts, tm);
            free(tm);
        }
    }
}

/*
 * Fill <rfds> with file descriptors that may have input data, and return the number of file
 * descriptors that may be set.
 */
int cxFillFDs(CX *cx, fd_set *rfds)
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
 * Prepare a call to select() for <cx>. <rfds> is filled with file descriptors that may have input
 * data and <tv> is set to a pointer to a struct timeval to be used as a timeout (which may be NULL
 * if no timeouts are pending). The number of file descriptors that may be set is returned.
 */
int cxPrepareSelect(CX *cx, fd_set *rfds, struct timeval **tv)
{
    CX_Timeout *tm;
    static struct timeval my_tv;

    cxFillFDs(cx, rfds);

    *tv = NULL;

    if ((tm = listHead(&cx->timeouts)) != NULL) {
        double dt = tm->t - cxNow();

        if (dt < 0) dt = 0;

        cx_double_to_timeval(dt, &my_tv);

        *tv = &my_tv;
    }

    return cx->num_connections;
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

        tm->handler(cx, tm->t, tm->udata);

        free(tm);
    }
    else {
        int fd;

        for (fd = 0; fd < cx->num_connections; fd++) {
            CX_Connection *conn = cx->connection[fd];

            if (conn == NULL) continue;

            if (FD_ISSET(fd, rfds)) {
                conn->handler(cx, fd, conn->udata);
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
        fd_set rfds;
        struct timeval *tv;

        int nfds = cxPrepareSelect(cx, &rfds, &tv);

P       dbgPrint(stderr, "nfds = %d, tv = %p\n", nfds, tv);

        if (nfds == 0 && tv == NULL) break;

        r = select(nfds, &rfds, NULL, NULL, tv);

        r = cxProcessSelect(cx, r, &rfds);
    }

    return r;
}

/*
 * Close down Communications Exchange <cx>. This forcibly stops <cx> from listening on any file
 * descriptor and removes all pending timeouts, which causes cxRun() to return.
 */
int cxClose(CX *cx)
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

    return 0;
}

/*
 * Free the memory occupied by <cx>. Call this outside of the cx loop, i.e. after cxRun() returns.
 * You can force cxRun() to return by calling cxClose().
 */
int cxFree(CX *cx)
{
    cxClose(cx);    /* Should not be necessary, just to be safe. */

    free(cx);

    return 0;
}

#ifdef TEST
#include "net.h"

/*
 * -- Client --
 */

int client_handle_data(CX *cx, int fd, void *udata)
{
    char buffer[80] = "";

    int r = read(fd, buffer, sizeof(buffer));

P   dbgPrint(stderr, "Got reply (%*s), closing down.\n", r, buffer);

    cxClose(cx);

    return 0;
}

int client_handle_timeout(CX *cx, double t, void *udata)
{
    int fd = *((int *) udata);

P   dbgPrint(stderr, "Timeout, sending ping...\n");

    tcpWrite(fd, "Ping!", 5);

    return 0;
}

void client(int port)
{
    CX *cx = cxCreate();

    int fd = tcpConnect("localhost", port);

    cxAddFile(cx, fd, client_handle_data, NULL);

    cxAddTime(cx, cxNow() + 1, client_handle_timeout, &fd);

    cxRun(cx);

P   dbgPrint(stderr, "cxRun() returned\n");
}

/*
 * -- Server --
 */

int server_handle_data(CX *cx, int fd, void *udata)
{
    char buffer[80] = "";

    int r = read(fd, buffer, sizeof(buffer));

    if (r == 0) {
P       dbgPrint(stderr, "End of file on fd %d, closing down.\n", fd);

        cxDropFile(cx, fd);

        cxClose(cx);
    }
    else if (strncmp(buffer, "Ping!", r) == 0) {
P       dbgPrint(stderr, "Got ping, sending echo.\n");

        tcpWrite(fd, "Echo!", 5);
    }

    return 0;
}

int server_accept_connection(CX *cx, int fd, void *udata)
{
P   dbgPrint(stderr, "Accepting new connection\n");

    fd = tcpAccept(fd);

    cxAddFile(cx, fd, server_handle_data, NULL);

    return 0;
}

int main(int argc, char *argv[])
{
    CX *cx = cxCreate();

    int fd = tcpListen(NULL, 0);
    int port = netLocalPort(fd);

    cxAddFile(cx, fd, server_accept_connection, NULL);

    if (fork() == 0) {
        client(port);
    }
    else {
        cxRun(cx);
    }

    return 0;
}
#endif
