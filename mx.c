/*
 * mx.c: Message Exchange.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: mx.c 170 2013-08-15 18:58:58Z jacco $
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
#include "hash.h"
#include "tcp.h"
#include "mx.h"

typedef struct {
    void (*cb)(MX *mx, int fd, int type, int version, char *payload, int size, void *udata);
    void *udata;
} MX_MsgType;

typedef enum {
    MX_FT_NONE,
    MX_FT_LISTEN,
    MX_FT_MESSAGE,
    MX_FT_DATA
} MX_FileType;

typedef struct {
    MX_FileType type;
    Buffer incoming, outgoing;
    void (*cb)(MX *mx, int fd, void *udata);
    void *udata;
} MX_File;

typedef struct {
    ListNode _node;
    double t;
    void *udata;
    void (*cb)(MX *mx, double t, void *udata);
} MX_Timeout;

struct MX {
    int num_files;
    MX_File **file;
    HashTable msg_types;
    List timeouts;

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

static void mx_add_fd(MX *mx, int fd, MX_FileType type)
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

    mx_file->type = type;
}

static int mx_compare_timeouts(const void *p1, const void *p2)
{
    const MX_Timeout *t1 = p1;
    const MX_Timeout *t2 = p2;

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

    mx_add_fd(mx, fd, MX_FT_LISTEN);

    return fd;
}

/*
 * Tell <mx> to call <cb> when a message of type <type> arrives. The file descriptor on which the
 * message was received is passed to <cb>, along with the type, version and payload contents and
 * size of the received message. Also passed to <cb> is the user data pointer <udata>. Only one
 * callback can be installed for each message type; subsequent calls will replace the installed
 * callback with <cb>.
 */
void mxOnMessage(MX *mx, int type,
        void (*cb)(MX *mx, int fd, int type, int version, char *payload, int size, void *udata),
        void *udata)
{
    MX_MsgType *msg_type = hashGet(&mx->msg_types, HASH_VALUE(type));

    if (msg_type == NULL) {
        msg_type = calloc(1, sizeof(MX_MsgType));

        hashAdd(&mx->msg_types, msg_type, HASH_VALUE(type));
    }

    msg_type->cb = cb;
    msg_type->udata = udata;
}

/*
 * Tell <mx> to stop listening for messages with type <type>.
 */
void mxDropMessage(MX *mx, int type)
{
    MX_MsgType *msg_type = hashGet(&mx->msg_types, HASH_VALUE(type));

    if (msg_type != NULL) {
        hashDel(&mx->msg_types, HASH_VALUE(type));
        free(msg_type);
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
        mx_add_fd(mx, fd, MX_FT_DATA);
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
        free(mx->file[fd]);
        mx->file[fd] = NULL;
    }
}

/*
 * Tell <mx> to call <cb> at time <t> (a timestamp in seconds since 00:00:00 UTC on 1970-01-01). The
 * <t> and <udata> that were given are passed back to <cb>.
 */
void mxOnTime(MX *mx, double t, void (*cb)(MX *mx, double t, void *udata), void *udata)
{
    MX_Timeout *tm = calloc(1, sizeof(MX_Timeout));

    tm->t = t;
    tm->udata = udata;
    tm->cb = cb;

    listAppendTail(&mx->timeouts, tm);

    listSort(&mx->timeouts, mx_compare_timeouts);
}

/*
 * Tell <mx> to cancel the timeout at <t>, for which callback <cb> was installed.
 */
void mxDropTime(MX *mx, double t, void (*cb)(MX *mx, double t, void *udata))
{
    MX_Timeout *tm, *next;

    for (tm = listHead(&mx->timeouts); tm; tm = next) {
        next = listNext(tm);

        if (tm->t == t && tm->cb == cb) {
            listRemove(&mx->timeouts, tm);
            free(tm);
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

    mx_add_fd(mx, fd, MX_FT_MESSAGE);

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
 * Send a message with type <type>, version <version> and payload <payload> with size <size> via
 * <mx> to <fd>. The message is added to an outgoing buffer in <mx>, and will be sent as soon as the
 * flow of control returns to <mx>'s main loop.
 */
int mxSend(MX *mx, int fd, int type, int version, const char *payload, int size)
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
 * Send a message with type <type> and version <version> via <mx> to <fd>. The message payload is
 * packed using the PACK_* syntax described in utils.h. The message is added to an outgoing buffer
 * in <mx>, and will be sent as soon as the flow of control returns to <mx>'s main loop.
 */
int mxPack(MX *mx, int fd, int type, int version, ...)
{
    int r;
    va_list ap;

    va_start(ap, version);
    r = mxVaPack(mx, fd, type, version, ap);
    va_end(ap);

    return r;
}

/*
 * Send a message with type <type> and version <version> via <mx> to <fd>. The message payload is
 * packed using the PACK_* syntax described in utils.h. The message is added to an outgoing buffer
 * in <mx>, and will be sent as soon as the flow of control returns to <mx>'s main loop.
 */
int mxVaPack(MX *mx, int fd, int type, int version, va_list ap)
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

/*
 * Tell <mx> to wait until a message of type <type> arrives on file descriptor <fd>. The version of
 * the received message is passed to <cb>, along with its payload and payload size. Messages,
 * timeouts and other received data that arrives while waiting for this message will be queued up
 * and delivered as soon as the flow-of-control returns to <mx>'s main loop.
 */
int mxAwait(MX *mx, int fd, int type, int *version, char **payload, int *size, double timeout)
{
    return 0;
}

/*
 * Start <mx>'s main loop. This function won't return until all sockets and other file descriptors
 * have been closed and there are no more pending timeouts.
 */
int mxRun(MX *mx)
{
    return 0;
}

/*
 * Close down <mx>. This will close all sockets and other file descriptors and cancel all timeouts,
 * causing mxRun() to return.
 */
int mxClose(MX *mx)
{
    return 0;
}

/*
 * Destroy <mx>. Only call this function when mxRun() has returned.
 */
int mxDestroy(MX *mx)
{
    return 0;
}
