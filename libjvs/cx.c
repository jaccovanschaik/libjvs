/*
 * cx.c: Description
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

#define ASSERTFD(cx, fd) \
    dbgAssert(stderr, (cx)->connections[fd] != NULL, "Unknown fd %d\n", fd)

typedef struct {
    Buffer *incoming, *outgoing;
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
    int nfds;
    CX_Connection **connections;
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

/* Add file descriptor <fd>. */

void cxAddFd(CX *cx, int fd)
{
    CX_Connection *conn;

    if (fd >= cx->nfds) {
        cx->connections = realloc(cx->connections, (fd + 1) *
                sizeof(CX_Connection *));

        memset(cx->connections + cx->nfds * sizeof(CX_Connection *), 0,
                (fd + 1 - cx->nfds) * sizeof(CX_Connection *));

        cx->nfds = fd + 1;
    }

    conn = cx->connections[fd] = calloc(1, sizeof(CX_Connection));

    if (conn->incoming != NULL) bufDestroy(conn->incoming);
    if (conn->outgoing != NULL) bufDestroy(conn->outgoing);

    conn->incoming = bufCreate();
    conn->outgoing = bufCreate();
}

void cxDropFd(CX *cx, int fd)
{
    CX_Connection *conn = cx->connections[fd];

    ASSERTFD(cx, fd);

    bufDestroy(conn->incoming);
    bufDestroy(conn->outgoing);

    free(conn);

    cx->connections[fd] = NULL;
}

void cxSubscribe(CX *cx, int fd, void *udata, int (*handler)(CX *cx, int fd, void *udata))
{
    CX_Connection *conn = cx->connections[fd];

    ASSERTFD(cx, fd);

    conn->handler = handler;
    conn->udata = udata;
}

void cxQueue(CX *cx, int fd, const char *data, int len)
{
    CX_Connection *conn = cx->connections[fd];

    ASSERTFD(cx, fd);

    bufAdd(conn->outgoing, data, len);
}

const char *cxGet(CX *cx, int fd, int *len)
{
    CX_Connection *conn = cx->connections[fd];

    ASSERTFD(cx, fd);

    *len = bufLen(conn->incoming);

    return bufGet(conn->incoming);
}

void cxClear(CX *cx, int fd, int len)
{
    CX_Connection *conn = cx->connections[fd];

    ASSERTFD(cx, fd);

    bufTrim(conn->incoming, len, 0);
}

/* Add a timeout at time <t>. */

void cxAddTimeout(CX *cx, double t, void *udata, int (*handler)(CX *cx, double t, void *udata))
{
    CX_Timeout *tm = calloc(1, sizeof(CX_Timeout));

    tm->t = t;
    tm->udata = udata;
    tm->handler = handler;

    listAppendTail(&cx->timeouts, tm);

    listSort(&cx->timeouts, cx_compare_timeouts);
}

/* Run the communications exchange. */

int cxRun(CX *cx)
{
    int r, fd;
    fd_set rfds, wfds;
    CX_Timeout *tm;

    while (1) {
        CX_Connection *conn;

        FD_ZERO(&rfds);
        FD_ZERO(&wfds);

        for (fd = 0; fd < cx->nfds; fd++) {
            if (cx->connections[fd] == NULL) continue;

            FD_SET(fd, &rfds);
            if (bufLen(cx->connections[fd]->outgoing) > 0) FD_SET(fd, &wfds);
        }

        tm = listHead(&cx->timeouts);

        if (tm == NULL) {
            if (cx->nfds == 0) {
                return 0;
            }
            else {
                r = select(cx->nfds, &rfds, &wfds, NULL, NULL);
            }
        }
        else {
            double dt = tm->t - cx_now();

            if (dt < 0) dt = 0;

            if (cx->nfds == 0) {
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

                r = select(cx->nfds, &rfds, &wfds, NULL, &tv);
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
            for (fd = 0; fd < cx->nfds; fd++) {
                conn = cx->connections[fd];

                if (FD_ISSET(fd, &rfds)) {
                    char buffer[9000];

                    r = read(fd, buffer, sizeof(buffer));

                    if (r < 0) {
                    }
                    else if (r == 0) {
                        close(fd);
                        memset(conn, 0, sizeof(CX_Connection));
                    }
                    else {
                        bufAdd(cx->connections[fd]->incoming, buffer, r);
                        conn->handler(cx, fd, NULL);
                    }
                }

                if (FD_ISSET(fd, &wfds)) {
                    r = write(fd, bufGet(conn->outgoing),
                            bufLen(conn->outgoing));

                    if (r < 0) {
                    }
                    else if (r == 0) {
                    }
                    else {
                        bufTrim(conn->outgoing, r, 0);
                    }
                }
            }
        }
    }
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
