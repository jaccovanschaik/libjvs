/*
 * mx.c: Message Exchange.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: mx.c 176 2013-08-20 17:18:28Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "buffer.h"
#include "utils.h"
#include "debug.h"
#include "hash.h"
#include "net.h"
#include "tcp.h"
#include "udp.h"
#include "mx.h"

typedef struct {
    void (*cb)(MX *mx, int fd, MX_Type type, MX_Version version, char *payload, MX_Size size, void *udata);
    void *udata;
} MX_Subscription;

typedef enum {
    MX_ET_NONE,
    MX_ET_DATA,
    MX_ET_LISTEN,
    MX_ET_MESSAGE,
    MX_ET_TIMER,
    MX_ET_AWAIT,
    MX_ET_ERROR,
    MX_ET_CONN,
    MX_ET_DISC
} MX_EventType;

typedef struct {
    MX_EventType event_type;    /* MX_ET_DATA, MX_ET_LISTEN or MX_ET_MESSAGE */
    Buffer incoming, outgoing;
    void (*cb)(MX *mx, int fd, void *udata);
    void *udata;
} MX_File;

typedef struct {
    ListNode _node;
    MX_EventType event_type;    /* MX_ET_TIMER or MX_ET_AWAIT */
    double t;
    void *udata;
    void (*cb)(MX *mx, double t, void *udata);
} MX_Timer;

typedef struct {
    int fd;
} MX_DataEvent;

typedef struct {
    int fd;
    MX_Type type;
    MX_Version version;
    MX_Size size;
    char *payload;
} MX_MessageEvent;

typedef struct {
    int fd;
    const char *whence;
    int error;
} MX_ErrorEvent;

typedef struct {
    int fd;
} MX_ConnEvent;

typedef struct {
    int fd;
    const char *whence;
} MX_DiscEvent;

typedef struct {
    ListNode _node;
    MX_EventType event_type;    /* MX_ET_DATA, MX_ET_LISTEN or MX_ET_MESSAGE */
    const void *udata;
    union {
        MX_DataEvent data;
        MX_MessageEvent msg;
        MX_ErrorEvent err;
        MX_ConnEvent conn;
        MX_DiscEvent disc;
    } u;
} MX_Event;

struct MX {
    int num_files;
    MX_File **file;
    HashTable subs;
    List timers;

    List waiting, pending;
    fd_set readable;

    void (*on_connect_cb)(MX *mx, int fd, void *udata);
    void *on_connect_udata;

    void (*on_disconnect_cb)(MX *mx, int fd, const char *whence, void *udata);
    void *on_disconnect_udata;

    void (*on_error_cb)(MX *mx, int fd, const char *whence, int error, void *udata);
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

static MX_Event *mx_create_event(List *queue, MX_EventType type)
{
    MX_Event *evt = calloc(1, sizeof(MX_Event));

    evt->event_type = type;

    listAppendTail(queue, evt);

    return evt;
}

static MX_Event *mx_queue_timer(List *queue, MX_EventType event_type)
{
    MX_Event *evt = mx_create_event(queue, event_type);

    return evt;
}

static MX_Event *mx_queue_error(List *queue,
        int fd, char *whence, int error)
{
    MX_Event *evt = mx_create_event(queue, MX_ET_ERROR);

    evt->u.err.fd = fd;
    evt->u.err.error = error;
    evt->u.err.whence = whence;

    return evt;
}

static MX_Event *mx_queue_disc(List *queue, int fd, char *whence)
{
    MX_Event *evt = mx_create_event(queue, MX_ET_DISC);

    evt->u.disc.fd = fd;
    evt->u.disc.whence = whence;

    return evt;
}

static MX_Event *mx_queue_connect(List *queue, int fd)
{
    MX_Event *evt = mx_create_event(queue, MX_ET_CONN);

    evt->u.conn.fd = fd;

    return evt;
}

static MX_Event *mx_queue_data(List *queue, int fd)
{
    MX_Event *evt = mx_create_event(queue, MX_ET_DATA);

    evt->u.data.fd = fd;

    return evt;
}

static MX_Event *mx_queue_message(List *queue,
        int fd, MX_Type type, MX_Version version, MX_Size size, char *payload)
{
    MX_Event *evt = mx_create_event(queue, MX_ET_MESSAGE);

P   dbgPrint(stderr, "fd = %d\n", fd);

    evt->u.msg.fd = fd;
    evt->u.msg.type = type;
    evt->u.msg.version = version;
    evt->u.msg.size = size;
    evt->u.msg.payload = payload;

    return evt;
}

static int mx_get_events(MX *mx, List *queue)
{
    int r, fd, nfds = 0;
    fd_set rfds, wfds;
    struct timeval tv, *tvp = NULL;
    MX_Timer *timer;

    if ((timer = listHead(&mx->timers)) != NULL) {
        double delta_t = timer->t - nowd();

P       dbgPrint(stderr, "Have %d timers, first with event_type %d in %g seconds\n",
                listLength(&mx->timers), timer->event_type, delta_t);

        if (delta_t <= 0) {
P           dbgPrint(stderr, "delta_t < 0: queueing a timer event and returning 1\n");

            mx_queue_timer(queue, timer->event_type);

            return 1;
        }
        else {
            tv.tv_sec = delta_t;
            tv.tv_usec = 1000000 * (delta_t - tv.tv_sec);

P           dbgPrint(stderr, "Setting a timeval: sec = %ld, usec = %ld\n", tv.tv_sec, tv.tv_usec);

            tvp = &tv;
        }
    }

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

    for (fd = 0; fd < mx->num_files; fd++) {
        MX_File *file = mx->file[fd];

        if (file == NULL) {
            continue;
        }

P       dbgPrint(stderr, "fd %d:\n", fd);

        FD_SET(fd, &rfds);
        if (bufLen(&file->outgoing) > 0) FD_SET(fd, &wfds);

P       dbgPrint(stderr, "r = %d, w = %d\n", FD_ISSET(fd, &rfds), FD_ISSET(fd, &wfds));

        nfds = fd + 1;
    }

    if (nfds == 0 && tvp == NULL) return 0;

P   dbgPrint(stderr, "Calling select with nfds = %d\n", nfds);

    r = select(nfds, &rfds, &wfds, NULL, tvp);

P   dbgPrint(stderr, "Select returned %d\n", r);

    if (r < 0) {
        mx_queue_error(queue, -1, "select", errno);
    }
    else if (r == 0) {
        mx_queue_timer(queue, timer->event_type);
    }
    else {
        int fd;

        for (fd = 0; fd < nfds; fd++) {
            MX_File *file = mx->file[fd];

            if (FD_ISSET(fd, &wfds)) {
P               dbgPrint(stderr, "Writing %d bytes:\n", bufLen(&file->outgoing));

                r = write(fd, bufGet(&file->outgoing), bufLen(&file->outgoing));

P               dbgPrint(stderr, "Return code = %d\n", r);

                if (r < 0) {
                    mx_queue_error(queue, fd, "write", errno);
                    mxDropData(mx, fd);
                }
                else if (r == 0) {
                    mx_queue_disc(queue, fd, "write");
                    mxDropData(mx, fd);
                }
                else {
                    bufTrim(&file->outgoing, r, 0);
                }
            }

            if (FD_ISSET(fd, &rfds)) {
                if (file->event_type == MX_ET_LISTEN) {
                    int new_fd = tcpAccept(fd);

                    if (new_fd < 0) {
                        mx_queue_error(queue, fd, "accept", errno);
                    }
                    else {
                        mx_add_fd(mx, new_fd, MX_ET_MESSAGE);
                        mx_queue_connect(queue, new_fd);
                    }
                }
                else if (file->event_type == MX_ET_DATA) {
                    mx_queue_data(queue, fd);
                }
                else if (file->event_type == MX_ET_MESSAGE) {
                    char data[9000];

                    r = read(fd, data, sizeof(data));

                    if (r < 0) {
                        mx_queue_error(queue, fd, "read", errno);
                        mxDropData(mx, fd);
                    }
                    else if (r == 0) {
                        mx_queue_disc(queue, fd, "read");
                        mxDropData(mx, fd);
                    }
                    else {
                        bufAdd(&file->incoming, data, r);

                        while (bufLen(&file->incoming) >= 12) {
                            MX_Type type;
                            MX_Size size;
                            MX_Version version;
                            char *payload;

                            bufUnpack(&file->incoming,
                                    PACK_INT32, &type,
                                    PACK_INT32, &version,
                                    PACK_INT32, &size,
                                    END);

                            if (bufLen(&file->incoming) < 12 + size) break;

                            payload = memdup(bufGet(&file->incoming) + 12, size);

                            bufTrim(&file->incoming, 12 + size, 0);

                            mx_queue_message(queue, fd, type, version, size, payload);
                        }
                    }
                }
            }
        }
    }

    return 1;
}

static void mx_process_data_event(MX *mx, MX_Event *evt)
{
    MX_File *file = mx->file[evt->u.data.fd];

    if (file != NULL) {
        file->cb(mx, evt->u.data.fd, file->udata);
    }

    free(evt);
}

static void mx_process_message_event(MX *mx, MX_Event *evt)
{
    MX_MessageEvent *msg = &evt->u.msg;
    MX_Subscription *sub = hashGet(&mx->subs, HASH_VALUE(msg->type));

    if (sub != NULL) {
        sub->cb(mx, msg->fd, msg->type, msg->version, msg->payload, msg->size, sub->udata);
    }

    free(msg->payload);
    free(evt);
}

static void mx_process_timer_event(MX *mx, MX_Event *evt)
{
    MX_Timer *timer = listRemoveHead(&mx->timers);

    timer->cb(mx, timer->t, timer->udata);

    free(timer);
}

static void mx_process_error_event(MX *mx, MX_Event *evt)
{
    MX_ErrorEvent *err = &evt->u.err;

    if (mx->on_error_cb != NULL) {
        mx->on_error_cb(mx, err->fd, err->whence, err->error, mx->on_error_udata);
    }

    free(evt);
}

static void mx_process_conn_event(MX *mx, MX_Event *evt)
{
    MX_ConnEvent *conn = &evt->u.conn;

    if (mx->on_connect_cb != NULL) {
        mx->on_connect_cb(mx, conn->fd, mx->on_connect_udata);
    }

    free(evt);
}

static void mx_process_disc_event(MX *mx, MX_Event *evt)
{
    MX_DiscEvent *disc = &evt->u.disc;

    if (mx->on_disconnect_cb != NULL) {
        mx->on_disconnect_cb(mx, disc->fd, disc->whence, mx->on_disconnect_udata);
    }

    free(evt);
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
 * Tell <mx> to call <cb> when a message of message type <type> arrives. The file descriptor on
 * which the message was received is passed to <cb>, along with the message type, version and
 * payload contents and size of the received message. Also passed to <cb> is the user data pointer
 * <udata>. Only one callback can be installed for each message type; subsequent calls will replace
 * the installed callback with <cb>.
 */
void mxOnMessage(MX *mx, MX_Type type,
        void (*cb)(MX *mx, int fd, MX_Type type, MX_Version version, char *payload, MX_Size size, void *udata),
        void *udata)
{
    MX_Subscription *sub = hashGet(&mx->subs, HASH_VALUE(type));

    if (sub == NULL) {
        sub = calloc(1, sizeof(MX_Subscription));

        hashAdd(&mx->subs, sub, HASH_VALUE(type));
    }

    sub->cb = cb;
    sub->udata = udata;
}

/*
 * Tell <mx> to stop listening for messages with message type <type>.
 */
void mxDropMessage(MX *mx, MX_Type type)
{
    MX_Subscription *sub = hashGet(&mx->subs, HASH_VALUE(type));

    if (sub != NULL) {
        hashDel(&mx->subs, HASH_VALUE(type));
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
    MX_Timer *timer = calloc(1, sizeof(MX_Timer));

    timer->event_type = event_type;
    timer->t = t;
    timer->udata = udata;
    timer->cb = cb;

    listAppendTail(&mx->timers, timer);

    listSort(&mx->timers, mx_compare_timers);

    return timer;
}

static void mx_drop_timer(MX *mx, MX_Timer *timer)
{
    listRemove(&mx->timers, timer);

    free(timer);
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
 * Tell <mx> to cancel the timer at <t>, for which callback <cb> was installed.
 */
void mxDropTime(MX *mx, double t, void (*cb)(MX *mx, double t, void *udata))
{
    MX_Timer *timer, *next;

    for (timer = listHead(&mx->timers); timer; timer = next) {
        next = listNext(timer);

        if (timer->t == t && timer->cb == cb) {
            mx_drop_timer(mx, timer);
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

    if (fd >= 0) {
        mx_add_fd(mx, fd, MX_ET_MESSAGE);
    }

    return fd;
}

/*
 * Tell <mx> to drop the connection on <fd>.
 */
void mxDisconnect(MX *mx, int fd)
{
    if (!mx_has_fd(mx, fd)) return;

    close(fd);

    free(mx->file[fd]);
    mx->file[fd] = NULL;
}

/*
 * Send a message with message type <type>, version <version> and payload <payload> with size
 * <size> via <mx> to <fd>. The message is added to an outgoing buffer in <mx>, and will be sent as
 * soon as the flow of control returns to <mx>'s main loop.
 */
int mxSend(MX *mx, int fd, MX_Type type, MX_Version version, const char *payload, MX_Size size)
{
P   dbgPrint(stderr, "fd = %d, size = %d\n", fd, size);

    if (!mx_has_fd(mx, fd)) return -1;

P   dbgPrint(stderr, "Packing message\n");

    bufPack(&mx->file[fd]->outgoing,
            PACK_INT32, type,
            PACK_INT32, version,
            PACK_INT32, size,
            PACK_RAW,   payload, size,
            END);

    return 0;
}

/*
 * Send a message with message type <type> and version <version> via <mx> to <fd>. The message
 * payload is packed using the PACK_* syntax described in utils.h. The message is added to an
 * outgoing buffer in <mx>, and will be sent as soon as the flow of control returns to <mx>'s main
 * loop.
 */
int mxPack(MX *mx, int fd, MX_Type type, MX_Version version, ...)
{
    int r;
    va_list ap;

    va_start(ap, version);
    r = mxVaPack(mx, fd, type, version, ap);
    va_end(ap);

    return r;
}

/*
 * Send a message with message type <type> and version <version> via <mx> to <fd>. The message
 * payload is packed using the PACK_* syntax described in utils.h. The message is added to an
 * outgoing buffer in <mx>, and will be sent as soon as the flow of control returns to <mx>'s main
 * loop.
 */
int mxVaPack(MX *mx, int fd, MX_Type type, MX_Version version, va_list ap)
{
    char *str;
    size_t size = vastrpack(&str, ap);

    int r = mxSend(mx, fd, type, version, str, size);

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
void mxOnDisconnect(MX *mx, void (*cb)(MX *mx, int fd, const char *whence, void *udata), void *udata)
{
    mx->on_disconnect_cb = cb;
    mx->on_disconnect_udata = udata;
}

/*
 * Tell <mx> to call <cb> when an error occurs on the connection using file descriptor <fd>. The
 * error code is passed to <cb>, along with the user data pointer <udata>.
 */
void mxOnError(MX *mx, void (*cb)(MX *mx, int fd, const char *whence, int error, void *udata), void *udata)
{
    mx->on_error_cb = cb;
    mx->on_error_udata = udata;
}

/*
 * Tell <mx> to wait until a message of message type <type> arrives on file descriptor <fd>. The
 * version of the received message is returned through <version>, its payload through <payload> and
 * the payload size though <size>. Messages, timers and other received data that arrive while
 * waiting for this message will be eventsd up and delivered as soon as the flow-of-control returns
 * to <mx>'s main loop. This function will wait until the time given in <timeout> (a UTC timestamp
 * containing the number of seconds since 1970-01-01/00:00:00 UTC) for the message to arrive. It
 * returns 0 if the message did arrive on time, 1 if it didn't and -1 if any other (network) error
 * occurred.
 */
int mxAwait(MX *mx, int fd, MX_Type type, MX_Version *version, char **payload, MX_Size *size, double timeout)
{
    mx_add_timer(mx, timeout, MX_ET_AWAIT, NULL, NULL);

P   dbgPrint(stderr, "fd = %d, type = %d, timeout = %f\n", fd, type, timeout);

    while (1) {
        MX_Event *evt;

        while (listIsEmpty(&mx->waiting)) {
            int r;

P           dbgPrint(stderr, "Calling mx_get_events\n");

            r = mx_get_events(mx, &mx->waiting);

P           dbgPrint(stderr, "mx_get_events returned %d\n", r);

            if (r < 0) return r;
        }

        evt = listRemoveHead(&mx->waiting);

P       dbgPrint(stderr, "First event has event_type = %d\n", evt->event_type);

        if (evt->event_type == MX_ET_MESSAGE && evt->u.msg.fd == fd && evt->u.msg.type == type) {
            MX_Timer *timer = listRemoveHead(&mx->timers);

            free(timer);

            *version = evt->u.msg.version;
            *payload = evt->u.msg.payload;
            *size    = evt->u.msg.size;

            free(evt);

P           dbgPrint(stderr, "Returning waited-for message\n");

            return 0;
        }
        else if (evt->event_type == MX_ET_AWAIT) {
            MX_Timer *timer = listRemoveHead(&mx->timers);

            free(timer);
            free(evt);

P           dbgPrint(stderr, "Timed out. Returning 1\n");

            return 1;
        }
        else {
P           dbgPrint(stderr, "Appending event to \"pending\" list\n");

            listAppendTail(&mx->pending, evt);
        }
    }
}

/*
 * Start <mx>'s main loop. This function won't return until all sockets and other file descriptors
 * have been closed and there are no more pending timers.
 */
int mxRun(MX *mx)
{
    while (1) {
        MX_Event *evt;

        while ((evt = listRemoveHead(&mx->waiting)) != NULL) {
            listAppendTail(&mx->pending, evt);
        }

        while (listIsEmpty(&mx->pending)) {
            int r = mx_get_events(mx, &mx->pending);

P           dbgPrint(stderr, "mx_get_events returned %d\n", r);

            if (r <= 0) return r;
        }

        evt = listRemoveHead(&mx->pending);

        switch(evt->event_type) {
        case MX_ET_DATA:
            mx_process_data_event(mx, evt);
            break;
        case MX_ET_MESSAGE:
            mx_process_message_event(mx, evt);
            break;
        case MX_ET_TIMER:
            mx_process_timer_event(mx, evt);
            break;
        case MX_ET_ERROR:
            mx_process_error_event(mx, evt);
            break;
        case MX_ET_CONN:
            mx_process_conn_event(mx, evt);
            break;
        case MX_ET_DISC:
            mx_process_disc_event(mx, evt);
            break;
        default:
            break;
        }
    }
}

/*
 * Close down <mx>. This will close all sockets and other file descriptors and cancel all timers,
 * causing mxRun() to return.
 */
void mxClose(MX *mx)
{
    int fd;
    MX_Timer *timer;

    for (fd = 0; fd < mx->num_files; fd++) {
        mxDropData(mx, fd);
    }

    while ((timer = listRemoveHead(&mx->timers)) != NULL) {
        free(timer);
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

#ifdef TEST
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>

enum {
    REQ_NONE,
    REQ_QUIT,
    REQ_ECHO,
    REQ_NO_REPLY,
    REQ_TCP_CONNECT,
    REQ_TCP_SEND,
    REQ_UDP_SEND,
    REQ_COUNT
};

void s_handle_message(MX *mx, int fd,
        MX_Type type, MX_Version version, char *payload, MX_Size size, void *udata)
{
    static int tcp_fd, udp_fd;

    char *host;
    uint16_t port;

    char *string;

P   dbgPrint(stderr, "type = %d, version = %d, size = %d\n", type, version, size);
P   hexdump(stderr, payload, size);

    switch(type) {
    case REQ_QUIT:
        mxClose(mx);
        break;
    case REQ_ECHO:
        mxSend(mx, fd, type, version, payload, size);
        break;
    case REQ_TCP_CONNECT:
        strunpack(payload, size,
                PACK_STRING,    &host,
                PACK_INT16,     &port,
                END);

P       dbgPrint(stderr, "Connecting to %s:%d\n", host, port);

        tcp_fd = tcpConnect(host, port);

        break;
    case REQ_TCP_SEND:
        strunpack(payload, size, PACK_STRING, &string, END);

        write(tcp_fd, string, strlen(string));

        break;
    case REQ_UDP_SEND:
        strunpack(payload, size,
                PACK_STRING, &host,
                PACK_INT16,  &port,
                PACK_STRING, &string,
                END);

        udp_fd = udpSocket();
        netConnect(udp_fd, host, port);

        write(udp_fd, string, strlen(string));

        break;
    case REQ_NO_REPLY:
        /* Be vewwy vewwy quiet.. I'm hunting wabbits... */
        break;
    default:
        dbgPrint(stderr, "Got unexpected request %d\n", type);
        break;
    }
}

void server(void)
{
    int i;

    MX *mx = mxCreate();

    mxListen(mx, NULL, 1234);

    for (i = 0; i < REQ_COUNT; i++) {
        mxOnMessage(mx, i, s_handle_message, NULL);
    }

    mxRun(mx);
}

int step = 0, errors = 0;
int server_fd, listen_fd, tcp_fd, udp_fd;

typedef struct {
    void (*function)(MX *mx);
    const char *expectation;
} Test;

Test *current_test;

void run_test(MX *mx, Test *test)
{
    if (test->function == NULL) {
        mxClose(mx);
    }
    else {
        test->function(mx);
    }
}

void c_log(MX *mx, char *fmt, ...)
{
    char *msg;

    va_list ap;

    va_start(ap, fmt);
    vasprintf(&msg, fmt, ap);
    va_end(ap);

P   dbgPrint(stderr, "%s\n", msg);

    if (strcmp(current_test->expectation, msg) != 0) {
        dbgPrint(stderr, "Expected \"%s\"\n", current_test->expectation);
        dbgPrint(stderr, "Received \"%s\"\n", msg);

        errors++;
    }

    free(msg);

    run_test(mx, ++current_test);
}

void c_handle_timer(MX *mx, double t, void *udata)
{
    c_log(mx, "Timer was triggered");
}

void c_test_timer(MX *mx)
{
P   dbgPrint(stderr, "Testing timer\n");

    mxOnTime(mx, nowd() + 0.1, c_handle_timer, NULL);
}

void c_connect_to_server(MX *mx)
{
    server_fd = mxConnect(mx, "localhost", 1234);

P   dbgPrint(stderr, "Connected to the test server: server_fd = %d\n", server_fd);

    if (server_fd >= 0) {
        c_log(mx, "Connected to test server");
    }
    else {
        c_log(mx, "Connection to test server failed");
    }
}

void c_setup_tcp_port(MX *mx)
{
    listen_fd = tcpListen(NULL, 2345);

    if (listen_fd >= 0)
        c_log(mx, "Opened TCP listen port");
    else
        c_log(mx, "Couldn't open TCP listen port");
}

void c_accept_tcp_connection(MX *mx, int fd, void *udata)
{
    tcp_fd = tcpAccept(fd);

    if (tcp_fd >= 0)
        c_log(mx, "Accepted TCP connection");
    else
        c_log(mx, "Accept failed");
}

void c_test_tcp_connection(MX *mx)
{
    Buffer payload = { 0 };

P   dbgPrint(stderr, "Testing incoming TCP connection using a Data socket\n");

    mxOnData(mx, listen_fd, c_accept_tcp_connection, NULL);

    bufPack(&payload,
            PACK_STRING,    "localhost",
            PACK_INT16,     2345,
            END);

P   dbgPrint(stderr, "Sending REQ_TCP_CONNECT to fd %d\n", server_fd);

    mxSend(mx, server_fd, REQ_TCP_CONNECT, 0, bufGet(&payload), bufLen(&payload));

    bufReset(&payload);
}

void c_handle_tcp_traffic(MX *mx, int fd, void *udata)
{
    char buffer[16];

    int r = read(fd, buffer, sizeof(buffer));

    c_log(mx, "Received %d bytes of TCP data: %*s", r, r, buffer);
}

void c_test_tcp_traffic(MX *mx)
{
P   dbgPrint(stderr, "Testing incoming TCP traffic using a Data socket\n");

    mxOnData(mx, tcp_fd, c_handle_tcp_traffic, NULL);

    mxPack(mx, server_fd, REQ_TCP_SEND, 0, PACK_STRING, "ABCD", END);
}

void c_open_udp_port(MX *mx)
{
    if ((udp_fd = udpSocket()) >= 0 && netBind(udp_fd, NULL, 3456) == 0)
        c_log(mx, "Opened UDP port");
    else
        c_log(mx, "Couldn't opem UDP port");
}

void c_handle_udp_traffic(MX *mx, int fd, void *udata)
{
    char buffer[16];

    int r = read(fd, buffer, sizeof(buffer));

    c_log(mx, "Received %d bytes of UDP data: %*s", r, r, buffer);
}

void c_test_udp_traffic(MX *mx)
{
P   dbgPrint(stderr, "Testing incoming UDP traffic on Data socket\n");

    mxOnData(mx, udp_fd, c_handle_udp_traffic, NULL);

    mxPack(mx, server_fd, REQ_UDP_SEND, 0,
            PACK_STRING,    "localhost",
            PACK_INT16,     3456,
            PACK_STRING,    "EFGH",
            END);
}

void c_test_await_reply(MX *mx)
{
    int r;

    char *payload;

    MX_Version version;
    MX_Size size;

P   dbgPrint(stderr, "Testing mxAwait with a correct reply\n");

    mxPack(mx, server_fd, REQ_ECHO, 0,
            PACK_RAW,   "Ping!", 5,
            END);

    r = mxAwait(mx, server_fd, REQ_ECHO, &version, &payload, &size, nowd() + 1);

    if (r == -1)
        c_log(mx, "mxAwait returned an error");
    else if (r == 1)
        c_log(mx, "mxAwait timed out");
    else if (size == 5 && strncmp(payload, "Ping!", size) == 0)
        c_log(mx, "mxAwait returned reply");
    else
        c_log(mx, "Unexpected reply from mxAwait: version = %d, size = %d, payload = \"%*s\"",
                version, size, size, payload);
}

void c_test_await_timeout(MX *mx)
{
    int r;

    char *payload;

    MX_Version version;
    MX_Size size;

P   dbgPrint(stderr, "Testing mxAwait with a timeout\n");

    mxPack(mx, server_fd, REQ_NO_REPLY, 0,
            PACK_RAW,   "Ping!", 5,
            END);

    r = mxAwait(mx, server_fd, REQ_NO_REPLY, &version, &payload, &size, nowd() + 0.1);

    if (r == -1)
        c_log(mx, "mxAwait returned an error");
    else if (r == 1)
        c_log(mx, "mxAwait timed out");
    else if (size == 5 && strncmp(payload, "Ping!", size) == 0)
        c_log(mx, "mxAwait returned reply");
    else
        c_log(mx, "Unexpected reply from mxAwait: version = %d, size = %d, payload = \"%*s\"",
                version, size, size, payload);
}

void c_test_await_2_replies(MX *mx)
{
    int r1, r2;

    char *payload;

    MX_Version version;
    MX_Size size;

P   dbgPrint(stderr, "Testing multiple mxAwait calls\n");

    mxPack(mx, server_fd, REQ_ECHO, 0,
            PACK_RAW,   "Ping!", 5,
            END);

    mxPack(mx, server_fd, REQ_ECHO, 0,
            PACK_RAW,   "Ping!", 5,
            END);

    r1 = mxAwait(mx, server_fd, REQ_ECHO, &version, &payload, &size, nowd() + 1);
    r2 = mxAwait(mx, server_fd, REQ_ECHO, &version, &payload, &size, nowd() + 1);

    if (r1 == 0 && r2 == 0)
        c_log(mx, "mxAwait returned reply, twice");
    else
        c_log(mx, "Unexpected reply from mxAwait: r1 = %d, r2 = %d", r1, r2);
}

void client(void)
{
    MX *mx;

    Test test[] = {
        { c_test_timer,             "Timer was triggered" },
        { c_connect_to_server,      "Connected to test server" },
        { c_setup_tcp_port,         "Opened TCP listen port" },
        { c_test_tcp_connection,    "Accepted TCP connection" },
        { c_test_tcp_traffic,       "Received 4 bytes of TCP data: ABCD" },
        { c_open_udp_port,          "Opened UDP port" },
        { c_test_udp_traffic,       "Received 4 bytes of UDP data: EFGH" },
        { c_test_await_reply,       "mxAwait returned reply" },
        { c_test_await_timeout,     "mxAwait timed out" },
        { c_test_await_2_replies,   "mxAwait returned reply, twice" },
        { NULL,  NULL }
    };

    mx = mxCreate();

    run_test(mx, current_test = &test[0]);

    mxRun(mx);
}

int main(int argc, char *argv[])
{
    pid_t pid;

    if ((pid = fork()) == 0) {
        server();
    }
    else {
        client();
        kill(pid, SIGTERM);
    }

    return errors;
}
#endif
