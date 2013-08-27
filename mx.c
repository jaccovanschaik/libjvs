/*
 * mx.c: Message Exchange.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: mx.c 207 2013-08-27 20:24:28Z jacco $
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

/*
 * The various event types that we can handle.
 */
typedef enum {
    MX_ET_NONE,
    MX_ET_DATA,                         /* Data on an mxOnFile file descriptor. */
    MX_ET_LISTEN,                       /* mxListen connection request. */
    MX_ET_MESSAGE,                      /* Incoming message on a messaging socket. */
    MX_ET_TIMER,                        /* mxOnTime timer going off. */
    MX_ET_AWAIT,                        /* mxAwait timeout. */
    MX_ET_ERROR,                        /* Error while awaiting/reading messages. */
    MX_ET_CONN,                         /* Connection created. */
    MX_ET_DISC                          /* Connection broken. */
} MX_EventType;

/*
 * A subscription on a message type.
 */
typedef struct {
    void (*cb)(MX *mx, int fd,          /* Callback to call. */
            MX_Type type, MX_Version version, MX_Size size, const char *payload, void *udata);
    void *udata;                        /* User data to return. */
} MX_Subscription;

/*
 * An opened file descriptor.
 */
typedef struct {
    MX_EventType event_type;            /* Event type we expect here. */
    Buffer incoming, outgoing;          /* Incoming and outgoing data buffers. */
    void (*cb)(MX *mx, int fd, void *udata);    /* Callback on incoming data. */
    void *udata;                        /* User data to return in callback. */
} MX_File;

/*
 * A pending timer.
 */
typedef struct {
    ListNode _node;
    MX_EventType event_type;            /* Event type we're waiting for. */
    double t;                           /* Time at which to trigger. */
    void (*cb)(MX *mx, double t, void *udata);  /* Callback to call. */
    void *udata;                        /* User data to return. */
} MX_Timer;

/* The following contain specific data for the various events. */

/*
 * A data event (incoming data on a file descriptor given in an mxOnFile call).
 */
typedef struct {
    int fd;                             /* The file descriptor that has data. */
} MX_DataEvent;

/*
 * A message event (incoming message on a connected messaging socket).
 */
typedef struct {
    int fd;
    MX_Type type;
    MX_Version version;
    MX_Size size;
    char *payload;
} MX_MessageEvent;

/*
 * An error event (error occurred while waiting for or reading a file descriptor).
 */
typedef struct {
    int fd;                             /* File descriptor where the error occurred. */
    const char *whence;                 /* Function that reported the error. */
    int error;                          /* errno error code. */
} MX_ErrorEvent;

/*
 * A connection event (new connection on an mxListen socket).
 */
typedef struct {
    int fd;                             /* Listen socket file descriptor. */
} MX_ConnEvent;

/*
 * A disconnect event (connection broken/end-of-file).
 */
typedef struct {
    int fd;                             /* File descriptor of broken connection. */
    const char *whence;                 /* Function that reported the error. */
} MX_DiscEvent;

/*
 * This is the event "superclass".
 */
typedef struct {
    ListNode _node;
    MX_EventType event_type;            /* Type of the event. */
    union {
        MX_DataEvent data;
        MX_MessageEvent msg;
        MX_ErrorEvent err;
        MX_ConnEvent conn;
        MX_DiscEvent disc;
    } u;
} MX_Event;

/*
 * The MX struct.
 */
struct MX {
    int num_files;                      /* Number of entries in <file>. */
    MX_File **file;                     /* MX_File data per file descriptor. */
    HashTable subs;                     /* List of subscriptions. */
    List timers;                        /* List of timers, ordered by time. */

    List pending;                       /* Arrived events waiting to be processed. */
    List waiting;                       /* Events that arrived while in an mxAwait call. */

    fd_set readable;                    /* File descriptors for which a data event was reported. */

    /*
     * Callbacks and user data pointers for mxOnConnect, mxOnDisconnect and mxOnError subscriptions.
     */

    void (*on_connect_cb)(MX *mx, int fd, void *udata);
    void *on_connect_udata;

    void (*on_disconnect_cb)(MX *mx, int fd, const char *whence, void *udata);
    void *on_disconnect_udata;

    void (*on_error_cb)(MX *mx, int fd, const char *whence, int error, void *udata);
    void *on_error_udata;

    void *ext;
};

/*
 * Return TRUE if <fd> is known to <mx>, otherwise FALSE.
 */
static int mx_has_fd(const MX *mx, int fd)
{
    if (fd >= mx->num_files)
        return FALSE;
    else if (mx->file[fd] == NULL)
        return FALSE;
    else
        return TRUE;
}

/*
 * Add file descriptor <fd>, on which we expect <event_type> events, to <mx>.
 */
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

/*
 * Compare the two MX_Timers pointed to by <p1> and <p2> and return -1, 1 or 0 if <p1> is earlier
 * than, later than or at the same time as <p2>.
 */
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
 * Create a new event of type <type> and add it to <queue>.
 */
static MX_Event *mx_create_event(List *queue, MX_EventType type)
{
    MX_Event *evt = calloc(1, sizeof(MX_Event));

    evt->event_type = type;

    listAppendTail(queue, evt);

    return evt;
}

/*
 * Add an error event with the given parameters to <queue>.
 */
static MX_Event *mx_queue_error(List *queue, int fd, char *whence, int error)
{
    MX_Event *evt = mx_create_event(queue, MX_ET_ERROR);

    evt->u.err.fd = fd;
    evt->u.err.error = error;
    evt->u.err.whence = whence;

    return evt;
}

/*
 * Add a disconnect event with the given parameters to <queue>.
 */
static MX_Event *mx_queue_disconnect(List *queue, int fd, char *whence)
{
    MX_Event *evt = mx_create_event(queue, MX_ET_DISC);

    evt->u.disc.fd = fd;
    evt->u.disc.whence = whence;

    return evt;
}

/*
 * Add a connect event with the given <fd> to <queue>.
 */
static MX_Event *mx_queue_connect(List *queue, int fd)
{
    MX_Event *evt = mx_create_event(queue, MX_ET_CONN);

    evt->u.conn.fd = fd;

    return evt;
}

/*
 * Add a data event on <fd> to <queue>.
 */
static MX_Event *mx_queue_data(List *queue, int fd)
{
    MX_Event *evt = mx_create_event(queue, MX_ET_DATA);

    evt->u.data.fd = fd;

    return evt;
}

/*
 * Add a message event with the given parameters to <queue>.
 */
static MX_Event *mx_queue_message(List *queue,
        int fd, MX_Type type, MX_Version version, MX_Size size, char *payload)
{
    MX_Event *evt = mx_create_event(queue, MX_ET_MESSAGE);

    evt->u.msg.fd = fd;
    evt->u.msg.type = type;
    evt->u.msg.version = version;
    evt->u.msg.size = size;
    evt->u.msg.payload = payload;

    return evt;
}

/*
 * Get new events for <mx> and store them in <queueu>.
 */
static int mx_get_events(MX *mx, List *queue)
{
    int r, fd, nfds = 0;
    fd_set rfds, wfds;
    struct timeval tv, *tvp = NULL;
    const MX_Timer *timer;

    /* First check for pending timers and see if we need a timeout. */

    if ((timer = listHead(&mx->timers)) != NULL) {
        double delta_t = timer->t - nowd();

        if (delta_t <= 0) {
            /* The timeout is for *now*. No use calling select(), just queue an event and return. */

            mx_create_event(queue, timer->event_type);

            return 1;
        }
        else {
            /* Timeout in the future. Calculate <tv> and let <tvp> point to it. */

            tv.tv_sec = delta_t;
            tv.tv_usec = 1000000 * (delta_t - tv.tv_sec);

            tvp = &tv;
        }
    }

    /* Now see which file descriptors to check. First reset them all. */

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

    for (fd = 0; fd < mx->num_files; fd++) {
        const MX_File *file = mx->file[fd];

        if (file == NULL) {
            continue;                   /* File not used. */
        }

        /* Always check if readable, only check if writable if we have something to write. */

        FD_SET(fd, &rfds);
        if (bufLen(&file->outgoing) > 0) FD_SET(fd, &wfds);

        nfds = fd + 1;                  /* Update number-of-file-descriptors. */
    }

    /* If no file descriptors to check and no timeouts to wait for, return 0. */

    if (nfds == 0 && tvp == NULL) return 0;

    /* Alrighty then. Let's see what's out there. */

    r = select(nfds, &rfds, &wfds, NULL, tvp);

    if (r < 0) {
        /* An error occurred. Queue it and return. */

        mx_queue_error(queue, -1, "select", errno);
        return -1;
    }

    if (r == 0) {
        /* Timeout. Queue it and return. */

        mx_create_event(queue, timer->event_type);

        return 1;
    }

    /* Otherwise check all file descriptor. */

    for (fd = 0; fd < nfds; fd++) {
        MX_File *file = mx->file[fd];

        if (file == NULL) continue;

        if (FD_ISSET(fd, &wfds)) {
            /* File descriptor is writable. Send data. */

            r = write(fd, bufGet(&file->outgoing), bufLen(&file->outgoing));

            if (r < 0) {
                mx_queue_error(queue, fd, "write", errno);
                mxDropData(mx, fd);
            }
            else if (r == 0) {
                mx_queue_disconnect(queue, fd, "write");
                mxDropData(mx, fd);
            }
            else {
                bufTrim(&file->outgoing, r, 0);
            }
        }

        if (FD_ISSET(fd, &rfds)) {
            /* File descriptor is readable. Get data. */

            if (file->event_type == MX_ET_LISTEN) {
                /* It's a listen socket. Accept new connection. */

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
                /* It's a data socket opened by the user. Queue a data event. */

                mx_queue_data(queue, fd);
            }
            else if (file->event_type == MX_ET_MESSAGE) {
                /* It's a message socket opened by me. Get data and queue message event(s). */

                char data[9000];        /* Maximum expected size (TCP jumbo packet). */

                r = read(fd, data, sizeof(data));

                if (r < 0) {
                    mx_queue_error(queue, fd, "read", errno);
                    mxDropData(mx, fd);
                }
                else if (r == 0) {
                    mx_queue_disconnect(queue, fd, "read");
                    mxDropData(mx, fd);
                }
                else {
                    /* We've got data! Decode and queue available messages. */

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

    return 1;
}

/*
 * Process a data event.
 */
static void mx_process_data_event(MX *mx, MX_Event *evt)
{
    const MX_File *file = mx->file[evt->u.data.fd];

    if (file != NULL) {
        file->cb(mx, evt->u.data.fd, file->udata);
    }

    free(evt);
}

/*
 * Procss a message event.
 */
static void mx_process_message_event(MX *mx, MX_Event *evt)
{
    const MX_MessageEvent *msg = &evt->u.msg;
    const MX_Subscription *sub = hashGet(&mx->subs, HASH_VALUE(msg->type));

    if (sub != NULL) {
        sub->cb(mx, msg->fd, msg->type, msg->version, msg->size, msg->payload, sub->udata);
    }

    free(msg->payload);
    free(evt);
}

/*
 * Process a timer event.
 */
static void mx_process_timer_event(MX *mx, MX_Event *evt)
{
    MX_Timer *timer = listRemoveHead(&mx->timers);

    timer->cb(mx, timer->t, timer->udata);

    free(timer);
}

/*
 * Process an error event.
 */
static void mx_process_error_event(MX *mx, MX_Event *evt)
{
    const MX_ErrorEvent *err = &evt->u.err;

    if (mx->on_error_cb != NULL) {
        mx->on_error_cb(mx, err->fd, err->whence, err->error, mx->on_error_udata);
    }

    free(evt);
}

/*
 * Process a connection event.
 */
static void mx_process_conn_event(MX *mx, MX_Event *evt)
{
    const MX_ConnEvent *conn = &evt->u.conn;

    if (mx->on_connect_cb != NULL) {
        mx->on_connect_cb(mx, conn->fd, mx->on_connect_udata);
    }

    free(evt);
}

/*
 * Process a disconnect event.
 */
static void mx_process_disc_event(MX *mx, MX_Event *evt)
{
    const MX_DiscEvent *disc = &evt->u.disc;

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
 * Add an extension pointed to by <ext> to <mx>.
 */
void mxExtend(MX *mx, void *ext)
{
    mx->ext = ext;
}

/*
 * Get a previously defined extension from <mx>.
 */
void *mxExtension(const MX *mx)
{
    return mx->ext;
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
        void (*cb)(MX *mx, int fd, MX_Type type, MX_Version version,
            MX_Size size, const char *payload, void *udata),
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
int mxSend(const MX *mx,
        int fd, MX_Type type, MX_Version version, MX_Size size, const char *payload)
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
int mxPack(const MX *mx, int fd, MX_Type type, MX_Version version, ...)
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
int mxVaPack(const MX *mx, int fd, MX_Type type, MX_Version version, va_list ap)
{
    char *str;
    size_t size = vastrpack(&str, ap);

    int r = mxSend(mx, fd, type, version, size, str);

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
 * waiting for this message will be queued up and delivered as soon as the flow-of-control returns
 * to <mx>'s main loop. This function will wait until the time given in <timeout> (a UTC timestamp
 * containing the number of seconds since 1970-01-01/00:00:00 UTC) for the message to arrive. It
 * returns 0 if the message did arrive on time, 1 if it didn't and -1 if any other (network) error
 * occurred. The payload that is returned through <payload> is dynamically allocated and it is up to
 * the caller to free it when no longer needed.
 */
int mxAwait(MX *mx, int fd, MX_Type type, MX_Version *version,
        MX_Size *size, char **payload, double timeout)
{
    MX_Timer *timer = mx_add_timer(mx, timeout, MX_ET_AWAIT, NULL, NULL);

    while (1) {
        MX_Event *evt;

        while (listIsEmpty(&mx->waiting)) {
            int r;

            r = mx_get_events(mx, &mx->waiting);

            if (r < 0) {
                mx_drop_timer(mx, timer);

                return r;
            }
        }

        evt = listRemoveHead(&mx->waiting);

        if (evt->event_type == MX_ET_MESSAGE && evt->u.msg.fd == fd && evt->u.msg.type == type) {
            mx_drop_timer(mx, timer);

            *version = evt->u.msg.version;
            *payload = evt->u.msg.payload;
            *size    = evt->u.msg.size;

            free(evt);

            return 0;
        }
        else if (evt->event_type == MX_ET_AWAIT) {
            mx_drop_timer(mx, timer);

            free(evt);

            return 1;
        }
        else {
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
    REQ_ECHO,
    REQ_NO_REPLY,
    REQ_TCP_CONNECT,
    REQ_TCP_SEND,
    REQ_UDP_SEND,
    REQ_MSG_CONNECT,
    REQ_MSG_DISCONNECT,
    REQ_COUNT
};

void s_handle_message(MX *mx, int fd,
        MX_Type type, MX_Version version, MX_Size size, const char *payload, void *udata)
{
    static int tcp_fd, udp_fd, msg_fd;

    char *host;
    uint16_t port;

    char *string;

    switch(type) {
    case REQ_ECHO:
        mxSend(mx, fd, type, version, size, payload);
        break;
    case REQ_TCP_CONNECT:
        strunpack(payload, size,
                PACK_STRING,    &host,
                PACK_INT16,     &port,
                END);

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
        udpSend(udp_fd, host, port, string, strlen(string));
        break;
    case REQ_NO_REPLY:
        /* Be vewwy vewwy quiet. I'm hunting wabbits... */
        break;
    case REQ_MSG_CONNECT:
        strunpack(payload, size,
                PACK_STRING,    &host,
                PACK_INT16,     &port,
                END);

        msg_fd = mxConnect(mx, host, port);

        break;
    case REQ_MSG_DISCONNECT:
        mxDisconnect(mx, msg_fd);
        break;
    default:
        dbgPrint(stderr, "Got unexpected request %d\n", type);
        exit(0);
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
int server_fd, data_listen_fd, msg_listen_fd, tcp_fd, udp_fd;

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
    mxOnTime(mx, nowd() + 0.1, c_handle_timer, NULL);
}

void c_connect_to_server(MX *mx)
{
    server_fd = mxConnect(mx, "localhost", 1234);

    if (server_fd >= 0) {
        c_log(mx, "Connected to test server");
    }
    else {
        c_log(mx, "Connection to test server failed");
    }
}

void c_setup_tcp_listen_port(MX *mx)
{
    data_listen_fd = tcpListen(NULL, 2345);

    if (data_listen_fd >= 0)
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
    mxOnData(mx, data_listen_fd, c_accept_tcp_connection, NULL);

    mxPack(mx, server_fd, REQ_TCP_CONNECT, 0,
            PACK_STRING,    "localhost",
            PACK_INT16,     2345,
            END);
}

void c_handle_tcp_traffic(MX *mx, int fd, void *udata)
{
    char buffer[16];

    int r = read(fd, buffer, sizeof(buffer));

    c_log(mx, "Received %d bytes of TCP data: %.*s", r, r, buffer);
}

void c_test_tcp_traffic(MX *mx)
{
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

    c_log(mx, "Received %d bytes of UDP data: %.*s", r, r, buffer);
}

void c_test_udp_traffic(MX *mx)
{
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

    mxPack(mx, server_fd, REQ_ECHO, 0,
            PACK_RAW,   "Ping!", 5,
            END);

    r = mxAwait(mx, server_fd, REQ_ECHO, &version, &size, &payload, nowd() + 1);

    if (r == -1) {
        c_log(mx, "mxAwait returned an error");
    }
    else if (r == 1) {
        c_log(mx, "mxAwait timed out");
    }
    else if (size == 5 && strncmp(payload, "Ping!", size) == 0) {
        c_log(mx, "mxAwait returned reply");
        free(payload);
    }
    else {
        c_log(mx, "Unexpected reply from mxAwait: version = %d, size = %d, payload = \"%*s\"",
                version, size, size, payload);
    }
}

void c_test_await_timeout(MX *mx)
{
    int r;

    char *payload;

    MX_Version version;
    MX_Size size;

    mxPack(mx, server_fd, REQ_NO_REPLY, 0,
            PACK_RAW,   "Ping!", 5,
            END);

    r = mxAwait(mx, server_fd, REQ_NO_REPLY, &version, &size, &payload, nowd() + 0.1);

    if (r == -1) {
        c_log(mx, "mxAwait returned an error");
    }
    else if (r == 1) {
        c_log(mx, "mxAwait timed out");
    }
    else if (size == 5 && strncmp(payload, "Ping!", size) == 0) {
        c_log(mx, "mxAwait returned reply");
        free(payload);
    }
    else {
        c_log(mx, "Unexpected reply from mxAwait: version = %d, size = %d, payload = \"%*s\"",
                version, size, size, payload);
    }

}

void c_test_await_2_replies(MX *mx)
{
    int r1, r2;

    char *payload1, *payload2;

    MX_Version version;
    MX_Size size;

    mxPack(mx, server_fd, REQ_ECHO, 0,
            PACK_RAW,   "Ping!", 5,
            END);

    mxPack(mx, server_fd, REQ_ECHO, 0,
            PACK_RAW,   "Ping!", 5,
            END);

    r1 = mxAwait(mx, server_fd, REQ_ECHO, &version, &size, &payload1, nowd() + 1);
    r2 = mxAwait(mx, server_fd, REQ_ECHO, &version, &size, &payload2, nowd() + 1);

    if (r1 == 0 && r2 == 0) {
        c_log(mx, "mxAwait returned reply, twice");
        free(payload1);
        free(payload2);
    }
    else {
        c_log(mx, "Unexpected reply from mxAwait: r1 = %d, r2 = %d", r1, r2);
    }
}

void c_setup_msg_listen_port(MX *mx)
{
    msg_listen_fd = mxListen(mx, NULL, 5678);

    if (msg_listen_fd >= 0)
        c_log(mx, "Opened messaging listen port");
    else
        c_log(mx, "Couldn't open messaging listen port");
}

void c_handle_msg_connection_request(MX* mx, int fd, void *udata)
{
    c_log(mx, "Accepted message connection");
}

void c_test_msg_connection(MX *mx)
{
    mxOnConnect(mx, c_handle_msg_connection_request, NULL);

    mxPack(mx, server_fd, REQ_MSG_CONNECT, 0,
            PACK_STRING,    "localhost",
            PACK_INT16,     5678,
            END);
}

void c_handle_msg_disconnect(MX* mx, int fd, const char *whence, void *udata)
{
    c_log(mx, "Message connection lost in %s", whence);
}

void c_test_msg_disconnect(MX *mx)
{
    mxOnDisconnect(mx, c_handle_msg_disconnect, NULL);

    mxPack(mx, server_fd, REQ_MSG_DISCONNECT, 0, END);
}

void client(void)
{
    MX *mx;

    Test test[] = {
        { c_test_timer,             "Timer was triggered" },
        { c_connect_to_server,      "Connected to test server" },
        { c_setup_tcp_listen_port,  "Opened TCP listen port" },
        { c_test_tcp_connection,    "Accepted TCP connection" },
        { c_test_tcp_traffic,       "Received 4 bytes of TCP data: ABCD" },
        { c_open_udp_port,          "Opened UDP port" },
        { c_test_udp_traffic,       "Received 4 bytes of UDP data: EFGH" },
        { c_test_await_reply,       "mxAwait returned reply" },
        { c_test_await_timeout,     "mxAwait timed out" },
        { c_test_await_2_replies,   "mxAwait returned reply, twice" },
        { c_setup_msg_listen_port,  "Opened messaging listen port" },
        { c_test_msg_connection,    "Accepted message connection" },
        { c_test_msg_disconnect,    "Message connection lost in read" },
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
