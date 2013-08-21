/*
 * mx.c: Message Exchange.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: mx.c 172 2013-08-19 14:48:41Z jacco $
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
#include "tcp.h"
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
    MX_ET_EOF
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
    double t;
} MX_TimerEvent;

typedef struct {
    int fd;
    const char *whence;
    int error;
} MX_ErrorEvent;

typedef struct {
    int fd;
    const char *whence;
} MX_EOFEvent;

typedef struct {
    ListNode _node;
    MX_EventType event_type;    /* MX_ET_DATA, MX_ET_LISTEN or MX_ET_MESSAGE */
    const void *udata;
    union {
        MX_DataEvent data;
        MX_MessageEvent message;
        MX_TimerEvent timer;
        MX_ErrorEvent error;
        MX_EOFEvent eof;
    } u;
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
 * Send a message with message type <type>, version <version> and payload <payload> with size
 * <size> via <mx> to <fd>. The message is added to an outgoing buffer in <mx>, and will be sent as
 * soon as the flow of control returns to <mx>'s main loop.
 */
int mxSend(MX *mx, int fd, MX_Type type, MX_Version version, const char *payload, MX_Size size)
{
    if (!mx_has_fd(mx, fd)) return -1;

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

static MX_Event *mx_create_event(List *queue, MX_EventType type, const void *udata)
{
    MX_Event *evt = calloc(1, sizeof(MX_Event));

    evt->event_type = type;
    evt->udata = udata;

    listAppendTail(queue, evt);

    return evt;
}

static MX_Event *mx_queue_timer(List *queue, MX_EventType event_type, double t, const void *udata)
{
    MX_Event *evt = mx_create_event(queue, event_type, udata);

    evt->u.timer.t = t;

    return evt;
}

static MX_Event *mx_queue_error(List *queue,
        int fd, const char *whence, int error, const void *udata)
{
    MX_Event *evt = mx_create_event(queue, MX_ET_ERROR, udata);

    evt->u.error.fd = fd;
    evt->u.error.error = error;
    evt->u.error.whence = whence;

    return evt;
}

static MX_Event *mx_queue_eof(List *queue, int fd, const char *whence, const void *udata)
{
    MX_Event *evt = mx_create_event(queue, MX_ET_EOF, udata);

    evt->u.eof.fd = fd;
    evt->u.eof.whence = whence;

    return evt;
}

static MX_Event *mx_queue_data(List *queue, int fd, const void *udata)
{
    MX_Event *evt = mx_create_event(queue, MX_ET_DATA, udata);

    evt->u.data.fd = fd;

    return evt;
}

static MX_Event *mx_queue_message(List *queue,
        int fd, MX_Type type, MX_Version version, MX_Size size, char *payload, const void *udata)
{
    MX_Event *evt = mx_create_event(queue, MX_ET_MESSAGE, udata);

    evt->u.message.fd = fd;
    evt->u.message.type = type;
    evt->u.message.version = version;
    evt->u.message.size = size;
    evt->u.message.payload = payload;

    return evt;
}

static int mx_get_events(MX *mx, List *queue)
{
    int r, nfds, count;
    fd_set rfds, wfds;
    struct timeval *tvp;

    r = select(nfds, &rfds, &wfds, NULL, tvp);

    if (r < 0) {
        mx_queue_error(queue, -1, "select", errno, mx->on_error_udata);

        return listLength(queue);
    }
    else if (r == 0) {
        MX_Timer *timer = listRemoveHead(&mx->timers);

        mx_queue_timer(queue, timer->event_type, timer->t, timer->udata);

        free(timer);
    }
    else {
        int fd;

        for (fd = 0; fd < nfds; fd++) {
            MX_File *file = mx->file[fd];

            if (FD_ISSET(fd, &wfds)) {
                r = write(fd, bufGet(&file->outgoing), bufLen(&file->outgoing));

                if (r < 0) {
                    mx_queue_error(queue, fd, "write", errno, mx->on_error_udata);
                    mxDropData(mx, fd);
                }
                else if (r == 0) {
                    mx_queue_eof(queue, fd, "write", mx->on_disconnect_udata);
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
                        mx_queue_error(queue, fd, "accept", errno, mx->on_error_udata);
                    }
                    else {
                        mx_add_fd(mx, new_fd, MX_ET_MESSAGE);
                    }
                }
                else if (file->event_type == MX_ET_DATA) {
                    mx_queue_data(queue, fd, file->udata);
                }
                else if (file->event_type == MX_ET_MESSAGE) {
                    char data[9000];

                    int r = read(fd, data, sizeof(data));

                    if (r < 0) {
                        mx_queue_error(mx, fd, "read", errno, mx->on_error_udata);
                        mxDropData(mx, fd);
                    }
                    else if (r == 0) {
                        mx_queue_eof(mx, fd, "read", mx->on_disconnect_udata);
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

                            mx_queue_message(queue,
                                    fd, type, version, size, payload, await_fd >= 0);
                        }
                    }
                }
            }
        }
    }

    return 0;
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
int mxAwait(MX *mx, int fd, int type, int *version, char **payload, int *size, double timeout)
{
    while (1) {
        int nfds, r;
        fd_set rfds, wfds;
        struct timeval *tvp;

        mx_prepare_select(mx, &nfds, &rfds, &wfds, &tvp);

        r = select(nfds, &rfds, &wfds, NULL, tvp);

        if ((r = mx_process_select(mx, r, nfds, &rfds, &wfds, fd, type)) != 0) {
            return -1;
        }
        else if (r == -2) {
            return 1;
        }
        else {
            return r;
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
