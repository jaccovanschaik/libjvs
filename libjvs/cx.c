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
#include "net.h"
#include "defs.h"
#include "debug.h"

#include "cx.h"

typedef struct {
    ListNode _node;
    int fd;
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
    List connections;
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

/* Return the timeval in <tv>, converted to a double. */

static double cx_timeval_to_double(const struct timeval *tv)
{
    return tv->tv_sec + tv->tv_usec / 1000000.0;
}

/* Convert double <time> and write it to the timeval at <tv>. */

static void cx_double_to_timeval(double time, struct timeval *tv)
{
    tv->tv_sec = (int) time;
    tv->tv_usec = 1000000 * fmod(time, 1.0);
}

/* Convert double <time> and write it to the timeval at <tv>. */

static void cx_double_to_timespec(double time, struct timespec *ts)
{
    ts->tv_sec = (int) time;
    ts->tv_nsec = 1000000000 * fmod(time, 1.0);
}

/* Return the current *UTC* time (number of seconds since
 * 1970-01-01/00:00:00 UTC) as a double. */

static double cx_now(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return cx_timeval_to_double(&tv);
}

/* Create a communications exchange. */

CX *cxCreate(void)
{
    return calloc(1, sizeof(CX));
}

/* Subscribe to input. */

void cxAddFile(CX *cx, int fd, void *udata,
        int (*handler)(CX *cx, int fd, void *udata))
{
    CX_Connection *conn = calloc(1, sizeof(CX_Connection));

    conn->fd = fd;
    conn->udata = udata;
    conn->handler = handler;

    listAppendTail(&cx->connections, conn);
}

/* Drop subscription to fd <fd>. */

void cxDropFile(CX *cx, int fd,
        int (*handler)(CX *cx, int fd, void *udata))
{
    CX_Connection *conn, *next;

    for (conn = listHead(&cx->connections); conn; conn = next) {
        next = listNext(conn);

        if (conn->fd == fd && conn->handler == handler) {
            listRemove(&cx->connections, conn);
            free(conn);
        }
    }
}

/* Add a timeout at time <t>. */

void cxAddTime(CX *cx, double t, void *udata,
        int (*handler)(CX *cx, double t, void *udata))
{
    CX_Timeout *tm = calloc(1, sizeof(CX_Timeout));

    tm->t = t;
    tm->udata = udata;
    tm->handler = handler;

    listAppendTail(&cx->timeouts, tm);

    listSort(&cx->timeouts, cx_compare_timeouts);
}

/* Drop timeout at time <t>. */

void cxDropTime(CX *cx, double t,
        int (*handler)(CX *cx, double t, void *udata))
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

/* Run the communications exchange. */

int cxRun(CX *cx)
{
    int r, nfds;
    fd_set rfds;
    CX_Timeout *tm;
    CX_Connection *conn;

    while (1) {
        FD_ZERO(&rfds);
        nfds = 0;

        for (conn = listHead(&cx->connections);
                conn; conn = listNext(conn)) {
P           dbgPrint(stderr, "Adding fd %d\n", conn->fd);

            FD_SET(conn->fd, &rfds);

            if (conn->fd >= nfds) nfds = conn->fd + 1;
        }

        tm = listHead(&cx->timeouts);

        if (tm == NULL) {
            if (nfds == 0) {
                return 0;
            }
            else {
P               dbgPrint(stderr, "No timeouts, calling select\n");

                r = select(nfds, &rfds, NULL, NULL, NULL);

P               dbgPrint(stderr, "select returned %d\n", r);
            }
        }
        else {
            double dt = tm->t - cx_now();

            if (dt < 0) dt = 0;

            if (nfds == 0) {
                struct timespec req, rem;

                cx_double_to_timespec(dt, &req);

                while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
                    errno = 0;
                    req = rem;
                }

                r = (errno == 0) ? 0 : -1;
            }
            else {
                struct timeval tv;

                cx_double_to_timeval(dt, &tv);

                r = select(nfds, &rfds, NULL, NULL, &tv);
            }
        }

        if (r == -1) {
            if (errno != EINTR) return -1;
        }
        else if (r == 0) {
            tm = listRemoveHead(&cx->timeouts);

            tm->handler(cx, tm->t, tm->udata);

            free(tm);
        }
        else {
            CX_Connection *next;

            for (conn = listHead(&cx->connections); conn; conn = next) {
                next = listNext(conn);

                if (FD_ISSET(conn->fd, &rfds)) {
                    conn->handler(cx, conn->fd, conn->udata);
                }
            }
        }
    }
    return 0;
}

/*
 * Close down Communications Exchange <cx>.
 */
int cxClose(CX *cx)
{
    CX_Timeout *tm;
    CX_Connection *conn;

    while ((tm = listRemoveHead(&cx->timeouts)) != NULL) {
        free(tm);
    }

    while ((conn = listRemoveHead(&cx->connections)) != NULL) {
        free(conn);
    }

    return 0;
}

/*
 * Free the memory occupied by <cx>.
 */
int cxFree(CX *cx)
{
    cxClose(cx);

    free(cx);

    return 0;
}

#if 0
void cxFDSet(CX *cx, fd_set *fds)
{
    memcpy(fds, &cx->fds, sizeof(fd_set));
}

int cxOwnsFD(CX *cx, int fd)
{
    return FD_ISSET(fd, &cx->fds);
}
#endif

#ifdef TEST

int handle_data(CX *cx, int fd, void *udata)
{
    char buffer[80] = "";

    int r = read(fd, buffer, sizeof(buffer));

    if (r == 0) {
P       dbgPrint(stderr, "End of file on fd %d\n", fd);
        close(fd);
        cxDropFile(cx, fd, handle_data);
    }
    else {
P       dbgPrint(stderr, "Got %d bytes: \"%s\"\n", r, buffer);
    }

    return 0;
}

int accept_connection(CX *cx, int fd, void *udata)
{
P   dbgPrint(stderr, "accept_connection\n");

    fd = netAccept(fd);

    cxAddFile(cx, fd, NULL, handle_data);

    return 0;
}

int main(int argc, char *argv[])
{
    CX *cx = cxCreate();

    return 0;

    int fd = netListen("localhost", 1234);

    cxAddFile(cx, fd, NULL, accept_connection);

    cxRun(cx);

    return 0;
}
#endif
