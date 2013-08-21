/*
 * mx.c: Message Exchange.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: mx.c 171 2013-08-16 14:01:39Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "buffer.h"
#include "utils.h"
#include "debug.h"
#include "hash.h"
#include "tcp.h"
#include "mx.h"

typedef struct {
    void (*cb)(MX *mx, int fd, int msg_type, int version, char *payload, int size, void *udata);
    void *udata;
} MX_Subscription;

typedef enum {
    MX_ET_NONE,
    MX_ET_DATA,
    MX_ET_LISTEN,
    MX_ET_MESSAGE,
    MX_ET_TIMER,
    MX_ET_AWAIT
} MX_EventType;

typedef struct {
    MX_EventType event_type;
    Buffer incoming, outgoing;
    void (*cb)(MX *mx, int fd, void *udata);
    void *udata;
} MX_File;

typedef struct {
    ListNode _node;
    MX_EventType event_type;
    double t;
    void *udata;
    void (*cb)(MX *mx, double t, void *udata);
} MX_Timer;

typedef struct {
    ListNode _node;
    MX_EventType event_type;
    int fd;
    int msg_type;
    int version;
    int size;
    char *payload;
    double t;
} MX_Event;

struct MX {
    int num_files;
    MX_File **file;
    HashTable subs;
    List timers;

    List events;
    fd_set readable;

    void (*on_connect_cb)(MX *mx, int fd, void *udata);
    void *on_connect_udata;

    void (*on_disconnect_cb)(MX *mx, int fd, void *udata);
    void *on_disconnect_udata;

    void (*on_error_cb)(MX *mx, int fd, int error, void *udata);
    void *on_error_udata;
};

static int mx_has_fd(const MX *mx, int fd)
{
    if (fd >= mx->num_files)
        return FALSE;
    else if (mx->file[fd] == NULL)
        return FALSE;
    else
        return TRUE;
}

static void mx_add_fd(MX *mx, int fd, MX_EventType event_type)
{
    MX_File *mx_file = calloc(1, sizeof(MX_File));

    if (fd >= mx->num_files) {
        int new_num_files = fd + 1;

        mx->file = realloc(mx->file, new_num_files * sizeof(MX_File *));

        memset(mx->file + mx->num_files, 0, sizeof(MX_File *) * (new_num_files - mx->num_files));

        mx->num_files = new_num_files;
    }

    if (mx->file[fd] != NULL) free(mx->file[fd]);

    mx->file[fd] = mx_file;

    mx_file->event_type = event_type;
}

static int mx_compare_timers(const void *p1, const void *p2)
{
    const MX_Timer *t1 = p1;
    const MX_Timer *t2 = p2;

    if (t1->t > t2->t)
        return 1;
    else if (t1->t < t2->t)
        return -1;
    else
        return 0;
}

/*
 * Create a Message Exchange.
 */
MX *mxCreate(void)
{
    return calloc(1, sizeof(MX));
}

/*
 * Tell <mx> to listen on address <host> and <port> for new connection requests, and return the file
 * descriptor for the new listening socket. If <host> is NULL, <mx> will listen on all interfaces.
 * If <port> is 0, <mx> will select a random port to listen on. You can find out which port was
 * picked by passing the returned file descriptor to netLocalPort().
 */
int mxListen(MX *mx, const char *host, int port)
{
    int fd = tcpListen(host, port);

    mx_add_fd(mx, fd, MX_ET_LISTEN);

    return fd;
}

/*
 * Tell <mx> to call <cb> when a message of message type <msg_type> arrives. The file descriptor on
 * which the message was received is passed to <cb>, along with the message type, version and
 * payload contents and size of the received message. Also passed to <cb> is the user data pointer
 * <udata>. Only one callback can be installed for each message type; subsequent calls will replace
 * the installed callback with <cb>.
 */
void mxOnMessage(MX *mx, int msg_type,
        void (*cb)(MX *mx, int fd, int msg_type, int version, char *payload, int size, void *udata),
        void *udata)
{
    MX_Subscription *sub = hashGet(&mx->subs, HASH_VALUE(msg_type));

    if (sub == NULL) {
        sub = calloc(1, sizeof(MX_Subscription));

        hashAdd(&mx->subs, sub, HASH_VALUE(msg_type));
    }

    sub->cb = cb;
    sub->udata = udata;
}

/*
 * Tell <mx> to stop listening for messages with message type <msg_type>.
 */
void mxDropMessage(MX *mx, int msg_type)
{
    MX_Subscription *sub = hashGet(&mx->subs, HASH_VALUE(msg_type));

    if (sub != NULL) {
        hashDel(&mx->subs, HASH_VALUE(msg_type));
        free(sub);
    }
}

/*
 * Tell <mx> to call <cb> if data comes in on file descriptor <fd>. The file descriptor where the
 * data was received is passed to <cb>, in addition to user data pointer <udata>. Only one
 * callback can be installed for each message type; subsequent calls will replace the installed
 * callback with <cb>.
 */
void mxOnData(MX *mx, int fd, void (*cb)(MX *mx, int fd, void *udata), void *udata)
{
    if (!mx_has_fd(mx, fd)) {
        mx_add_fd(mx, fd, MX_ET_DATA);
    }

    mx->file[fd]->cb = cb;
    mx->file[fd]->udata = udata;
}

/*
 * Tell <mx> to stop listening for data on <fd>.
 */
void mxDropData(MX *mx, int fd)
{
    if (mx_has_fd(mx, fd)) {
        close(fd);
        free(mx->file[fd]);
        mx->file[fd] = NULL;
    }
}

static MX_Timer *mx_add_timer(MX *mx, double t, MX_EventType event_type,
        void (*cb)(MX *mx, double t, void *udata), void *udata)
{
    MX_Timer *tm = calloc(1, sizeof(MX_Timer));

    tm->event_type = event_type;
    tm->t = t;
    tm->udata = udata;
    tm->cb = cb;

    listAppendTail(&mx->timers, tm);

    listSort(&mx->timers, mx_compare_timers);

    return tm;
}

static void mx_drop_timer(MX *mx, MX_Timer *tm)
{
    listRemove(&mx->timers, tm);

    free(tm);
}

/*
 * Tell <mx> to call <cb> at time <t> (a timestamp in seconds since 00:00:00 UTC on 1970-01-01). The
 * <t> and <udata> that were given are passed back to <cb>.
 */
void mxOnTime(MX *mx, double t, void (*cb)(MX *mx, double t, void *udata), void *udata)
{
    dbgAssert(stderr, cb != NULL, "<cb> can not be NULL in mxOnTime()");

    mx_add_timer(mx, t, MX_ET_TIMER, cb, udata);
}

/*
 * Tell <mx> to cancel the timeout at <t>, for which callback <cb> was installed.
 */
void mxDropTime(MX *mx, double t, void (*cb)(MX *mx, double t, void *udata))
{
    MX_Timer *tm, *next;

    for (tm = listHead(&mx->timers); tm; tm = next) {
        next = listNext(tm);

        if (tm->t == t && tm->cb == cb) {
            mx_drop_timer(mx, tm);
            break;
        }
    }
}

/*
 * Tell <mx> to make a new connection to <host> on port <port>. The file descriptor of the new
 * connection is returned.
 */
int mxConnect(MX *mx, const char *host, int port)
{
    int fd = tcpConnect(host, port);

    if (fd == -1) return -1;

    mx_add_fd(mx, fd, MX_ET_MESSAGE);

    return 0;
}

/*
 * Tell <mx> to drop the connection on <fd>.
 */
int mxDisconnect(MX *mx, int fd)
{
    if (!mx_has_fd(mx, fd)) return -1;

    close(fd);

    free(mx->file[fd]);
    mx->file[fd] = NULL;

    return 0;
}

/*
 * Send a message with message type <msg_type>, version <version> and payload <payload> with size
 * <size> via <mx> to <fd>. The message is added to an outgoing buffer in <mx>, and will be sent as
 * soon as the flow of control returns to <mx>'s main loop.
 */
int mxSend(MX *mx, int fd, int msg_type, int version, const char *payload, int size)
{
    if (!mx_has_fd(mx, fd)) return -1;

    bufPack(&mx->file[fd]->outgoing,
            PACK_INT32, msg_type,
            PACK_INT32, version,
            PACK_INT32, size,
            PACK_RAW,   payload, size,
            END);

    return 0;
}

/*
 * Send a message with message type <msg_type> and version <version> via <mx> to <fd>. The message
 * payload is packed using the PACK_* syntax described in utils.h. The message is added to an
 * outgoing buffer in <mx>, and will be sent as soon as the flow of control returns to <mx>'s main
 * loop.
 */
int mxPack(MX *mx, int fd, int msg_type, int version, ...)
{
    int r;
    va_list ap;

    va_start(ap, version);
    r = mxVaPack(mx, fd, msg_type, version, ap);
    va_end(ap);

    return r;
}

/*
 * Send a message with message type <msg_type> and version <version> via <mx> to <fd>. The message
 * payload is packed using the PACK_* syntax described in utils.h. The message is added to an
 * outgoing buffer in <mx>, and will be sent as soon as the flow of control returns to <mx>'s main
 * loop.
 */
int mxVaPack(MX *mx, int fd, int msg_type, int version, va_list ap)
{
    char *str;
    size_t size = vastrpack(&str, ap);

    int r = mxSend(mx, fd, msg_type, version, str, size);

    free(str);

    return r;
}

/*
 * Call <cb> when a new connection is accepted by <mx>. The file descriptor of the new connection is
 * passed to <cb>, along with the user data pointer <udata>. This function is *not* called for
 * connections made with mxConnect().
 */
void mxOnConnect(MX *mx, void (*cb)(MX *mx, int fd, void *udata), void *udata)
{
    mx->on_connect_cb = cb;
    mx->on_connect_udata = udata;
}

/*
 * Call <cb> when a connection is lost by <mx>. The file descriptor of the lost connection is
 * passed to <cb>, along with the user data pointer <udata>. This function is *not* called for
 * connections dropped using mxDisconnect().
 */
void mxOnDisconnect(MX *mx, void (*cb)(MX *mx, int fd, void *udata), void *udata)
{
    mx->on_disconnect_cb = cb;
    mx->on_disconnect_udata = udata;
}

/*
 * Tell <mx> to call <cb> when an error occurs on the connection using file descriptor <fd>. The
 * error code is passed to <cb>, along with the user data pointer <udata>.
 */
void mxOnError(MX *mx, void (*cb)(MX *mx, int fd, int error, void *udata), void *udata)
{
    mx->on_error_cb = cb;
    mx->on_error_udata = udata;
}

static int mx_prepare_select(MX *mx, int *nfds, fd_set *rfds, fd_set *wfds, struct timeval **tvpp)
{
    return 0;
}

static int mx_process_select(MX *mx, int r, int nfds, fd_set *rfds, fd_set *wfds)
{
    return 0;
}

static int mx_collect_events(MX *mx)
{
    int r;

    fd_set rfds, wfds;
    int nfds;
    struct timeval tv, *tvp;

    mx_prepare_select(mx, &nfds, &rfds, &wfds, &tvp);

    tvp = &tv;

    r = select(nfds, &rfds, &wfds, NULL, tvp);

    mx_process_select(mx, r, nfds, &rfds, &wfds);

    return 0;
}

/*
 * Tell <mx> to wait until a message of message type <msg_type> arrives on file descriptor <fd>. The
 * version of the received message is returned through <version>, its payload through <payload> and
 * the payload size though <size>. Messages, timers and other received data that arrive while
 * waiting for this message will be eventsd up and delivered as soon as the flow-of-control returns
 * to <mx>'s main loop. This function will wait until the time given in <timeout> (a UTC timestamp
 * containing the number of seconds since 1970-01-01/00:00:00 UTC) for the message to arrive. It
 * returns 0 if the message did arrive on time, 1 if it didn't and -1 if any other (network) error
 * occurred.
 */
int mxAwait(MX *mx, int fd, int msg_type, int *version, char **payload, int *size, double timeout)
{
    MX_Event *last_evt = NULL;   /* Last event we looked at in the previous iteration. */
    MX_Event *evt;               /* Event we're looking at now. */

    MX_Timer *tm = mx_add_timer(mx, timeout, MX_ET_AWAIT, NULL, NULL);

    while (1) {
        if (last_evt == NULL)
            evt = listHead(&mx->events);
        else
            evt = listNext(last_evt);

        if (evt == NULL) {
            if (mx_collect_events(mx) <= 0) {
                mx_drop_timer(mx, tm);
                return -1;
            }
            else {
                continue;
            }
        }

        last_evt = evt;

        if (evt->event_type == MX_ET_AWAIT) {
            listRemove(&mx->events, evt);

            free(evt);

            return 1;
        }
        else if (evt->event_type == MX_ET_MESSAGE && evt->msg_type == msg_type && evt->fd == fd) {
            listRemove(&mx->events, evt);

            *version = evt->version;
            *payload = evt->payload;
            *size    = evt->size;

            free(evt);

            mx_drop_timer(mx, tm);

            return 0;
        }
    }
}

/*
 * Start <mx>'s main loop. This function won't return until all sockets and other file descriptors
 * have been closed and there are no more pending timers.
 */
int mxRun(MX *mx)
{
    MX_Event *evt;               /* Message we're looking at now. */

    while (1) {
        if (listIsEmpty(&mx->events)) {
            int r;

            if ((r = mx_collect_events(mx)) <= 0) return r;
        }

        evt = listRemoveHead(&mx->events);

        if (evt->event_type == MX_ET_DATA) {
            MX_File *f = mx->file[evt->fd];

            f->cb(mx, evt->fd, f->udata);
        }
        else if (evt->event_type == MX_ET_LISTEN) {
            int fd = tcpAccept(evt->fd);

            mx_add_fd(mx, fd, MX_ET_MESSAGE);
        }
        else if (evt->event_type == MX_ET_MESSAGE) {
            MX_Subscription *sub = hashGet(&mx->subs, HASH_VALUE(evt->msg_type));

            sub->cb(mx, evt->fd, evt->msg_type, evt->version, evt->payload, evt->size, sub->udata);
        }
        else if (evt->event_type == MX_ET_TIMER) {
            MX_Timer *tm = listRemoveHead(&mx->timers);

            tm->cb(mx, tm->t, tm->udata);
        }
        else {
            fprintf(stderr, "Panic!\n");
        }
    }

    return 0;
}

/*
 * Close down <mx>. This will close all sockets and other file descriptors and cancel all timers,
 * causing mxRun() to return.
 */
void mxClose(MX *mx)
{
    int fd;
    MX_Timer *tm;

    for (fd = 0; fd < mx->num_files; fd++) {
        mxDropData(mx, fd);
    }

    while ((tm = listRemoveHead(&mx->timers)) != NULL) {
        free(tm);
    }
}

/*
 * Destroy <mx>. Only call this function when mxRun() has returned.
 */
void mxDestroy(MX *mx)
{
    mxClose(mx);

    free(mx);
}
