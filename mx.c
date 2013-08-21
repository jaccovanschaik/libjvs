/*
 * mx.c: Description
 *
 * This module is useful to create network-oriented servers like web- or file servers.
 *
 * mxRun()
 *
 * or (if you want to call select() yourself)
 *
 * mxGetReadFDs
 * mxGetWriteFDs
 * mxGetTimeout
 * select()
 * mxProcessSelect
 *
 * Copyright:	(c) 2012 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#define _GNU_SOURCE

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
#include "udp.h"
#include "defs.h"
#include "debug.h"
#include "hash.h"
#include "utils.h"

#include "mx.h"

typedef struct {
    Buffer incoming, outgoing;
    void *on_file_data_udata;
    void (*on_file_data_handler)(MX *mx, int fd, void *udata);
} MX_Connection;

typedef struct {
    ListNode _node;
    double t;
    void *on_time_udata;
    void (*on_time)(MX *mx, double t, void *udata);
} MX_Timeout;

typedef struct {
    void *on_message_udata;
    void (*on_message_handler)(MX *mx, int fd, int type, int version,
            const char *payload, size_t size, void *udata);
} MX_MsgType;

struct MX {
    int num_connections;
    MX_Connection **connection;

    HashTable msg_hash;

    void *on_error_udata;
    void (*on_error_handler)(MX *mx, int fd, int error, void *udata);
    void *on_connect_udata;
    void (*on_connect_handler)(MX *mx, int fd, void *udata);
    void *on_disconnect_udata;
    void (*on_disconnect_handler)(MX *mx, int fd, void *udata);
    List timeouts;
};

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
 * Convert double <time> and write it to the timeval at <tv>.
 */
static void mx_double_to_timeval(double time, struct timeval *tv)
{
    tv->tv_sec = (int) time;
    tv->tv_usec = 1000000 * fmod(time, 1.0);
}

/*
 * Add file descriptor <fd> to <mx>, representing a connection of type <type>.
 */
static void mx_add_file(MX *mx, int fd)
{
    int new_num_connections = fd + 1;

    if (new_num_connections > mx->num_connections) {
        mx->connection = realloc(mx->connection, new_num_connections * sizeof(MX_Connection *));

        memset(mx->connection + mx->num_connections, 0,
                sizeof(MX_Connection *) * (new_num_connections - mx->num_connections));

        mx->num_connections = new_num_connections;
    }

    if (mx->connection[fd] == NULL) {
        mx->connection[fd] = calloc(1, sizeof(MX_Connection));
    }
}

/*
 * Handle incoming data on the socket at file descriptor <fd>. Callback for mxOnFile.
 */
static void mx_handle_message_data(MX *mx, int fd, void *udata)
{
    char data[9000];

    int r = read(fd, data, sizeof(data));

    if (r < 0) {
        if (mx->on_error_handler != NULL) {
            mx->on_error_handler(mx, fd, errno, mx->on_error_udata);
        }

        close(fd);
        mxDropFile(mx, fd);
    }
    else if (r == 0) {
        if (mx->on_disconnect_handler != NULL) {
            mx->on_disconnect_handler(mx, fd, mx->on_disconnect_udata);
        }

        close(fd);
        mxDropFile(mx, fd);
    }
    else {
        int size, type, version;

        MX_Connection *conn = mx->connection[fd];

        bufAdd(&conn->incoming, data, r);

        while (bufLen(&conn->incoming) >= 12) {
            const char *payload;
            MX_MsgType *msg_type;

            bufUnpack(&conn->incoming,
                    PACK_INT32, &size,
                    PACK_INT32, &type,
                    PACK_INT32, &version,
                    END);

            if (bufLen(&conn->incoming) < size) break;

            payload = bufGet(&conn->incoming) + 12;

            msg_type = hashGet(&mx->msg_hash, HASH_VALUE(type));

            msg_type->on_message_handler(mx, fd, type, version,
                    payload, size, msg_type->on_message_udata);

            bufTrim(&conn->incoming, 12 + size, 0);
        }
    }
}

/*
 * A select() call has told us that <fd> is writable. Handle this.
 */
static void mx_handle_writeable(MX *mx, int fd)
{
    int r;

    MX_Connection *conn = mx->connection[fd];

    assert(fd >= 0 && fd < mx->num_connections);
    assert(conn != NULL);

    r = write(fd, bufGet(&conn->outgoing), bufLen(&conn->outgoing));

    if (r < 0) {
        if (mx->on_error_handler != NULL) {
            mx->on_error_handler(mx, fd, errno, mx->on_error_udata);
        }

        close(fd);
        mxDropFile(mx, fd);
    }
    else {
        bufTrim(&conn->outgoing, r, 0);
    }
}

/*
 * Handle an incoming connection request on <fd>.
 */
static void mx_handle_connection_request(MX *mx, int fd, void *udata)
{
    fd = tcpAccept(fd);

    if (fd < 0) return;

    mxOnFile(mx, fd, mx_handle_message_data, NULL);

    if (mx->on_connect_handler != NULL) {
        mx->on_connect_handler(mx, fd, mx->on_connect_udata);
    }
}

/*
 * Create and return a communications exchange.
 */
MX *mxCreate(void)
{
    return calloc(1, sizeof(MX));
}

/*
 * Open a listen socket bound to <host> and <port> and return its file descriptor. If <host> is NULL
 * the socket will listen on all interfaces. If <port> is equal to 0, the socket will be bound to a
 * random local port (use tcpLocalPort() on the returned fd to find out which). Any connection
 * requests will be accepted automatically. Use mxOnConnect to be notified of new connections.
 * Incoming messages will be reported through the handler installed by mxOnMessage.
 */
int mxTcpListen(MX *mx, const char *host, int port)
{
    int fd = tcpListen(host, port);

    if (fd >= 0) {
        mxOnFile(mx, fd, mx_handle_connection_request, NULL);
    }

    return fd;
}

/*
 * Open a UDP socket bound to <host> and <port> and listen on it for data. Incoming messages will be
 * reported through the handler installed by mxOnMessage. The file descriptor for the created
 * socket is returned.
 */
int mxUdpListen(MX *mx, const char *host, int port)
{
    int fd = udpSocket(host, port);

    if (fd >= 0) {
        mxOnFile(mx, fd, mx_handle_message_data, NULL);
    }

    return fd;
}

/*
 * Make a TCP connection to <host> on <port>. Incoming messages will be reported through the handler
 * installed by mxOnMessage. The file descriptor for the created socket is returned.
 */
int mxTcpConnect(MX *mx, const char *host, int port)
{
    int fd = tcpConnect(host, port);

    if (fd >= 0) {
        mxOnFile(mx, fd, mx_handle_message_data, NULL);
    }

    return fd;
}

/*
 * Make a UDP "connection" to <host> on <port>. Connection in this case means that data is sent to
 * the indicated address by default, without the need to specify an address on every send. The
 * file descriptor for the created socket is returned.
 */
int mxUdpConnect(MX *mx, const char *host, int port)
{
    int fd = udpConnect(host, port);

    mx_add_file(mx, fd);

    return fd;
}

/*
 * Set a handler to be called at time <t> (in seconds since 1970-01-01/00:00:00 UTC). <on_time> will
 * be called with the given <mx>, <t> and <udata>.
 */
void mxOnTime(MX *mx, double t, void (*on_time)(MX *mx, double t, void *udata), void *udata)
{
    MX_Timeout *tm = calloc(1, sizeof(MX_Timeout));

    tm->t = t;
    tm->on_time_udata = udata;
    tm->on_time = on_time;

    listAppendTail(&mx->timeouts, tm);

    listSort(&mx->timeouts, mx_compare_timeouts);
}

/*
 * Drop timeout at time <t>. Both <t> and <on_time> must match the earlier call to mxOnTime.
 */
void mxDropTime(MX *mx, double t, void (*on_time)(MX *mx, double t, void *udata))
{
    MX_Timeout *tm, *next;

    for (tm = listHead(&mx->timeouts); tm; tm = next) {
        next = listNext(tm);

        if (tm->t == t && tm->on_time == on_time) {
            listRemove(&mx->timeouts, tm);
            free(tm);
        }
    }
}

/*
 * Return the current UTC time (number of seconds since 1970-01-01/00:00:00 UTC) as a double.
 */
double mxNow(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

/*
 * Call <handler> when new data on one of the connected sockets comes in. You can install one
 * handler per message type. Any subsequent call to mxOnMessage with the same <type> will replace
 * the existing handler.
 */
void mxOnMessage(MX *mx, int type,
        void (*handler)(MX *mx, int fd, int type, int version,
            const char *payload, size_t size, void *udata),
        void *udata)
{
    MX_MsgType *msg_type = hashGet(&mx->msg_hash, HASH_VALUE(type));

    if (msg_type == NULL) {
        msg_type = calloc(1, sizeof(MX_MsgType));

        hashAdd(&mx->msg_hash, msg_type, HASH_VALUE(type));
    }

    msg_type->on_message_handler = handler;
    msg_type->on_message_udata = udata;
}

/*
 * Drop an existing subscription to message type <type>.
 */
void mxDropMessage(MX *mx, int type)
{
    MX_MsgType *msg_type = hashGet(&mx->msg_hash, HASH_VALUE(type));

    if (msg_type != NULL) {
        hashDel(&mx->msg_hash, HASH_VALUE(type));

        free(msg_type);
    }
}

/*
 * Subscribe to input. When data is available on <fd>, <handler> will be called with the given
 * <mx>, <fd> and <udata>. Only one handler per file descriptor can be set, subsequent calls will
 * override earlier ones.
 */
void mxOnFile(MX *mx, int fd, void (*handler)(MX *mx, int fd, void *udata), void *udata)
{
    mx_add_file(mx, fd);

    mx->connection[fd]->on_file_data_udata = udata;
    mx->connection[fd]->on_file_data_handler = handler;
}

/*
 * Drop subscription to fd <fd>.
 */
void mxDropFile(MX *mx, int fd)
{
    int new_num_connections;

    if (fd < mx->num_connections) mx->connection[fd] = NULL;

    for (fd = mx->num_connections - 1; fd >= 0; fd--) {
        if (mx->connection[fd] != NULL) break;
    }

    new_num_connections = fd + 1;

    if (new_num_connections < mx->num_connections) {
        mx->connection = realloc(mx->connection, new_num_connections * sizeof(MX_Connection *));

        mx->num_connections = new_num_connections;
    }
}

/*
 * Call <handler> with <udata> when a new connection is made, reporting the new file descriptor
 * through <fd>.
 */
void mxOnConnect(MX *mx, void (*handler)(MX *mx, int fd, void *udata), void *udata)
{
    mx->on_connect_handler = handler;
    mx->on_connect_udata = udata;
}

/*
 * Call <handler> with <udata> when the connection on <fd> is lost.
 */
void mxOnDisconnect(MX *mx, void (*handler)(MX *mx, int fd, void *udata), void *udata)
{
    mx->on_disconnect_handler = handler;
    mx->on_disconnect_udata = udata;
}

/*
 * Call <handler> when an error has occurred on <fd>. <error> is the associated errno.
 */
void mxOnError(MX *mx, void (*handler)(MX *mx, int fd, int error, void *udata), void *udata)
{
    mx->on_error_handler = handler;
    mx->on_error_udata = udata;
}

/*
 * Add a message with type <type>, version <version> and payload <payload> with size <size> to the
 * output buffer of <fd>. The message will be sent when the flow-of-control returns to the main
 * loop.
 */
void mxSend(MX *mx, int fd, int type, int version, const char *payload, size_t size)
{
    MX_Connection *conn = mx->connection[fd];

    assert(fd >= 0 && fd < mx->num_connections);
    assert(conn != NULL);

    bufPack(&conn->outgoing,
            PACK_INT32, size,
            PACK_INT32, type,
            PACK_INT32, version,
            PACK_RAW,   payload, size,
            END);
}

/*
 * Clear <rfds>, then fill it with the file descriptors that have been given to <mx> in a
 * mxOnFile call. Return the number of file descriptors that may be set. <rfds> can then be
 * passed to select().
 */
int mxGetReadFDs(MX *mx, fd_set *rfds)
{
    int fd;

    FD_ZERO(rfds);

    for (fd = 0; fd < mx->num_connections; fd++) {
        if (mx->connection[fd] == NULL) continue;

        FD_SET(fd, rfds);
    }

    return mx->num_connections;
}

/*
 * Clear <wfds>, then fill it with the file descriptors that have data queued for write. Return the
 * number of file descriptors that may be set. <wfds> can then be passed to select().
 */
int mxGetWriteFDs(MX *mx, fd_set *wfds)
{
    int fd;

    FD_ZERO(wfds);

    for (fd = 0; fd < mx->num_connections; fd++) {
        if (mx->connection[fd] == NULL || bufLen(&mx->connection[fd]->outgoing) == 0) continue;

        FD_SET(fd, wfds);
    }

    return mx->num_connections;
}

/*
 * Return TRUE if <fd> is handled by <mx>.
 */
int mxOwnsFD(MX *mx, int fd)
{
    return (fd >= 0 && fd < mx->num_connections && mx->connection[fd] != NULL);
}

/*
 * Get the timeout to use for a call to select. If a timeout is required it is copied to <tv>, and 1
 * is returned. Otherwise 0 is returned, and the last parameter of select should be set to NULL.
 * <tv> is not changed in that case.
 */
int mxGetTimeout(MX *mx, struct timeval *tv)
{
    MX_Timeout *tm;

    if ((tm = listHead(&mx->timeouts)) == NULL) {
        return 0;
    }
    else {
        double dt = tm->t - mxNow();

        if (dt < 0) dt = 0;

        mx_double_to_timeval(dt, tv);

        return 1;
    }
}

/*
 * Process the results from a select() call. Returns 0 on success or -1 on error.
 */
int mxProcessSelect(MX *mx, int r, fd_set *rfds, fd_set *wfds)
{
    if (r == -1) {
        if (errno != EINTR) return -1;
    }
    else if (r == 0) {
        MX_Timeout *tm = listRemoveHead(&mx->timeouts);

        tm->on_time(mx, tm->t, tm->on_time_udata);

        free(tm);
    }
    else {
        int fd;

        for (fd = 0; fd < mx->num_connections; fd++) {
            MX_Connection *conn = mx->connection[fd];

            if (conn == NULL) continue;

            if (FD_ISSET(fd, rfds)) {
                conn->on_file_data_handler(mx, fd, conn->on_file_data_udata);
            }

            if (FD_ISSET(fd, wfds)) {
                mx_handle_writeable(mx, fd);
            }
        }
    }

    return 0;
}

/*
 * Run the communications exchange. This function will return when there are no more timeouts to
 * wait for and no file descriptors to listen on (which can be forced by calling mxClose()). The
 * return value in this case will be 0. If any error occurred it will be -1.
 */
int mxRun(MX *mx)
{
    int r = 0;

    while (r >= 0) {
        struct timeval tv;
        fd_set rfds, wfds;

        int nfds = mx->num_connections;

        mxGetReadFDs(mx, &rfds);
        mxGetWriteFDs(mx, &wfds);

        r = mxGetTimeout(mx, &tv);

        if (nfds == 0 && r == 0) break;

        r = select(nfds, &rfds, &wfds, NULL, r ? &tv : NULL);

        if (r < 0) {
            dbgPrint(stderr, "select returned %d (%s)\n", r, strerror(errno));
            break;
        }

        r = mxProcessSelect(mx, r, &rfds, &wfds);

        if (r < 0) {
            dbgPrint(stderr, "mxProcessSelect returned %d (%s)\n", r, strerror(errno));
            break;
        }
    }

    return r;
}

/*
 * Close down Communications Exchange <mx>. This forcibly stops <mx> from listening on any file
 * descriptor and removes all pending timeouts, which causes mxRun() to return.
 */
void mxClose(MX *mx)
{
    int fd;
    MX_Timeout *tm;

    while ((tm = listRemoveHead(&mx->timeouts)) != NULL) {
        free(tm);
    }

    for (fd = 0; fd < mx->num_connections; fd++) {
        if (mx->connection[fd] != NULL) {
            bufReset(&mx->connection[fd]->incoming);
            bufReset(&mx->connection[fd]->outgoing);

            free(mx->connection[fd]);
            mx->connection[fd] = NULL;
        }
    }

    mx->num_connections = 0;
}

/*
 * Free the memory occupied by <mx>. Call this outside of the mx loop, i.e. after mxRun() returns.
 * You can force mxRun() to return by calling mxClose().
 */
void mxFree(MX *mx)
{
    mxClose(mx);    /* Should not be necessary, just to be safe. */

    free(mx);
}

#ifdef TEST
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "net.h"
#include "udp.h"

int errors = 0;

static int global_report_fd;

/*
 * Report an update to the client through <fd>.
 */
void report(int fd, const char *fmt, ...)
{
    static Buffer buf = { 0 };

    va_list ap;

    va_start(ap, fmt);
    bufSetV(&buf, fmt, ap);
    va_end(ap);

    write(fd, bufGet(&buf), bufLen(&buf));
}

/*
 * Handle incoming data (callback for mxOnFile on a connected socket).
 */
void handle_data(MX *mx, int fd, void *udata)
{
    char buffer[1500];

    int r = read(fd, buffer, sizeof(buffer));

    report(global_report_fd, "received '%.*s' on %s", r, buffer, (char *) udata);

    if (memcmp(buffer, "Quit", r) == 0) {
        mxClose(mx);
    }
}

/*
 * Accept connection request (callback for mxOnFile on a listen socket).
 */
void accept_connection(MX *mx, int fd, void *udata)
{
    report(global_report_fd, "accept connection on %s", (char *) udata);

    fd = tcpAccept(fd);

    mxOnFile(mx, fd, handle_data, udata);
}

/*
 * Report a new connection (callback for mxOnConnect).
 */
void report_connect(MX *mx, int fd, void *udata)
{
    report(global_report_fd, "accept connection on %s", (char *) udata);
}

void send_message(MX *mx, int fd, int type, int version, const char *fmt, ...)
{
    va_list ap;
    int n;
    char *payload;

    va_start(ap, fmt);
    n = vasprintf(&payload, fmt, ap);
    va_end(ap);

    mxSend(mx, fd, type, version, payload, n);

    free(payload);
}

/*
 * Handle incoming socket data (callback for mxOnMessage).
 */
void handle_message(MX *mx, int fd, int type, int version,
        const char *payload, size_t size, void *udata)
{
    report(global_report_fd, "received msg type %d, version %d, size %d, payload '%s'",
            type, version, size, payload);

    if (memcmp(payload, "Quit", 4) == 0) {
        mxClose(mx);
    }
}

/* ### Server ### */

void server(int report_fd, int tcp_port, int udp_port)
{
    MX *mx = mxCreate();

    global_report_fd = report_fd;

    mxTcpListen(mx, "localhost", tcp_port);
    mxUdpListen(mx, "localhost", udp_port);

    mxOnConnect(mx, report_connect, "server tcp");

    mxOnMessage(mx, 1, handle_message, "server tcp");
    mxOnMessage(mx, 3, handle_message, "server udp");

    mxRun(mx);

    mxFree(mx);

    exit(0);
}

/* ### Client callbacks ### */

/*
 * Handle timeout (callback for mxOnTime in the client). Sets up connections to the servers, which
 * should be up and running by now.
 */
void handle_timeout(MX *mx, double t, void *udata)
{
    int *fds = udata;

    fds[0] = mxTcpConnect(mx, "localhost", 10001);
    fds[1] = mxUdpConnect(mx, "localhost", 10002);
}

/*
 * Handle an incoming report from <fd>. Callback for mxOnFile and also, slightly hackishly for
 * mxOnDisconnect.
 */
void handle_report(MX *mx, int fd, void *udata)
{
    /* We will test the communications step by step. At every step we will check if we get the
     * expected response from the servers and then trigger the next step. */

    static int step = 0;
    char *expected_response[] = {           /* Expected responses at every step. */
        "accept connection on server tcp",
        "received msg type 1, version 2, size 10, payload 'Hello TCP!'",
        "received msg type 3, version 4, size 10, payload 'Hello UDP!'",
        "received msg type 1, version 6, size 4, payload 'Quit'",
        ""                                  /* Empty response on closed connection */
    };
    int n_responses = sizeof(expected_response) / sizeof(expected_response[0]);

    char buffer[100];

    int *fds = udata;   /* Client communicates the fds to use via udata... */

    int r = read(fd, buffer, sizeof(buffer));

P   fprintf(stderr, "handle_report, step %d, %d bytes: %.*s\n", step, r, r, buffer);

    if (step >= n_responses) {
        fprintf(stderr, "Missing expected_response for step %d.\n", step);
    }
    else if (strncmp(buffer, expected_response[step], strlen(expected_response[step])) != 0) {
        fprintf(stderr, "Unexpected response in step %d:\n\tExp: \"%s\"\n\tGot: \"%.*s\"\n",
                step, expected_response[step], r, buffer);
    }

    switch(step) {
    case 0:                         /* server is connected. */
        send_message(mx, fds[0], 1, 2, "Hello TCP!");
        break;
    case 1:
        send_message(mx, fds[1], 3, 4, "Hello UDP!");
        break;
    case 2:                         /* Got response; tests complete. */
        send_message(mx, fds[0], 1, 6, "Quit");
        break;
    case 3:                         /* Response to Quit request; nothing to do. */
        break;
    case 4:                         /* Server has closed the connection. */
        mxClose(mx);                /* Shut down myself. */
        break;
    default:
        break;
    }

    step++;
}

int main(int argc, char *argv[])
{
    MX *mx;

    int server_pipe[2], fds[2];

    pipe(server_pipe);

    if (fork() == 0) {
        server(server_pipe[1], 10001, 10002);
    }

    sleep(1);

    mx = mxCreate();

    mxOnFile(mx, server_pipe[0], handle_report, fds);

    mxOnTime(mx, mxNow() + 1, handle_timeout, fds);

    mxOnDisconnect(mx, handle_report, fds);

    mxRun(mx);

    mxFree(mx);

    return errors;
}
#endif
