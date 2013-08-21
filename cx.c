/*
 * cx.c: Communications Exchange.
 *
 * This module is useful to create network-oriented servers like web- or file servers.
 *
 * cxRun()
 *
 * or (if you want to call select() yourself)
 *
 * cxGetReadFDs
 * cxGetWriteFDs
 * cxGetTimeout
 * select()
 * cxProcessSelect
 *
 * Copyright:	(c) 2012 Jacco van Schaik (jacco@jaccovanschaik.net)
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
#include "net.h"
#include "tcp.h"
#include "udp.h"
#include "defs.h"
#include "debug.h"
#include "utils.h"

#include "cx.h"

typedef struct {
    Buffer incoming, outgoing;
    void *on_file_udata;
    void (*on_file_handler)(CX *cx, int fd, void *udata);
} CX_Connection;

typedef struct {
    ListNode _node;
    double t;
    void *on_time_udata;
    void (*on_time_handler)(CX *cx, double t, void *udata);
} CX_Timeout;

struct CX {
    int num_connections;
    CX_Connection **connection;
    void *on_socket_udata;
    void (*on_socket_handler)(CX *cx, int fd, const char *data, size_t size, void *udata);
    void *on_error_udata;
    void (*on_error_handler)(CX *cx, int fd, int error, void *udata);
    void *on_connect_udata;
    void (*on_connect_handler)(CX *cx, int fd, void *udata);
    void *on_disconnect_udata;
    void (*on_disconnect_handler)(CX *cx, int fd, void *udata);
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
 * Add file descriptor <fd> to <cx>, representing a connection of type <type>.
 */
static void cx_add_file(CX *cx, int fd)
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
}

/*
 * Handle incoming data on the socket at file descriptor <fd>. Callback for cxOnFile.
 */
static void cx_handle_socket_data(CX *cx, int fd, void *udata)
{
    char data[9000];

    int r = read(fd, data, sizeof(data));

    if (r < 0) {
        if (cx->on_error_handler != NULL) {
            cx->on_error_handler(cx, fd, errno, cx->on_error_udata);
        }

        close(fd);
        cxDropFile(cx, fd);
    }
    else if (r == 0) {
        if (cx->on_disconnect_handler != NULL) {
            cx->on_disconnect_handler(cx, fd, cx->on_disconnect_udata);
        }

        close(fd);
        cxDropFile(cx, fd);
    }
    else {
        if (cx->on_socket_handler != NULL) {
            cx->on_socket_handler(cx, fd, data, r, cx->on_socket_udata);
        }
    }
}

/*
 * A select() call has told us that <fd> is writable. Handle this.
 */
static void cx_handle_writeable(CX *cx, int fd)
{
    int r;

    CX_Connection *conn = cx->connection[fd];

    assert(fd >= 0 && fd < cx->num_connections);
    assert(conn != NULL);

    r = write(fd, bufGet(&conn->outgoing), bufLen(&conn->outgoing));

    if (r < 0) {
        if (cx->on_error_handler != NULL) {
            cx->on_error_handler(cx, fd, errno, cx->on_error_udata);
        }

        close(fd);
        cxDropFile(cx, fd);
    }
    else {
        bufTrim(&conn->outgoing, r, 0);
    }
}

/*
 * Handle an incoming connection request on <fd>.
 */
static void cx_handle_connection_request(CX *cx, int fd, void *udata)
{
    fd = tcpAccept(fd);

    if (fd < 0) return;

    cxOnFile(cx, fd, cx_handle_socket_data, NULL);

    if (cx->on_connect_handler != NULL) {
        cx->on_connect_handler(cx, fd, cx->on_connect_udata);
    }
}

/*
 * Create and return a communications exchange.
 */
CX *cxCreate(void)
{
    return calloc(1, sizeof(CX));
}

/*
 * Open a listen socket bound to <host> and <port> and return its file descriptor. If <host> is NULL
 * the socket will listen on all interfaces. If <port> is equal to 0, the socket will be bound to a
 * random local port (use tcpLocalPort() on the returned fd to find out which). Any connection
 * requests will be accepted automatically. Use cxOnConnect to be notified of new connections.
 * Incoming data will be reported through the handler installed by cxOnSocket.
 */
int cxTcpListen(CX *cx, const char *host, int port)
{
    int fd = tcpListen(host, port);

    if (fd >= 0) {
        cxOnFile(cx, fd, cx_handle_connection_request, NULL);
    }

    return fd;
}

/*
 * Open a UDP socket bound to <host> and <port> and listen on it for data. Incoming data will be
 * reported through the handler installed by cxOnSocket. The file descriptor for the created
 * socket is returned.
 */
int cxUdpListen(CX *cx, const char *host, int port)
{
    int fd = udpSocket();

    if (fd >= 0 && netBind(fd, host, port) == 0) {
        cxOnFile(cx, fd, cx_handle_socket_data, NULL);
    }

    return fd;
}

/*
 * Make a TCP connection to <host> on <port>. Incoming data will be reported through the handler
 * installed by cxOnSocket. The file descriptor for the created socket is returned.
 */
int cxTcpConnect(CX *cx, const char *host, int port)
{
    int fd = tcpConnect(host, port);

    if (fd >= 0) {
        cxOnFile(cx, fd, cx_handle_socket_data, NULL);
    }

    return fd;
}

/*
 * Make a UDP "connection" to <host> on <port>. Connection in this case means that data is sent to
 * the indicated address by default, without the need to specify an address on every send. The
 * file descriptor for the created socket is returned.
 */
int cxUdpConnect(CX *cx, const char *host, int port)
{
    int fd = udpSocket();

    if (fd >= 0 && netConnect(fd, host, port) == 0) {
        cx_add_file(cx, fd);
    }

    return fd;
}

/*
 * Set a handler to be called at time <t> (in seconds since 1970-01-01/00:00:00 UTC).
 * <on_time_handler> will be called with the given <cx>, <t> and <udata>.
 */
void cxOnTime(CX *cx, double t, void (*on_time_handler)(CX *cx, double t, void *udata), void *udata)
{
    CX_Timeout *tm = calloc(1, sizeof(CX_Timeout));

    tm->t = t;
    tm->on_time_udata = udata;
    tm->on_time_handler = on_time_handler;

    listAppendTail(&cx->timeouts, tm);

    listSort(&cx->timeouts, cx_compare_timeouts);
}

/*
 * Drop timeout at time <t>. Both <t> and <on_time_handler> must match the earlier call to cxOnTime.
 */
void cxDropTime(CX *cx, double t, void (*on_time_handler)(CX *cx, double t, void *udata))
{
    CX_Timeout *tm, *next;

    for (tm = listHead(&cx->timeouts); tm; tm = next) {
        next = listNext(tm);

        if (tm->t == t && tm->on_time_handler == on_time_handler) {
            listRemove(&cx->timeouts, tm);
            free(tm);
            break;
        }
    }
}

/*
 * Call <handler> when new data on one of the connected sockets comes in.
 */
void cxOnSocket(CX *cx,
        void (*handler)(CX *cx, int fd, const char *data, size_t size, void *udata),
        void *udata)
{
    cx->on_socket_handler = handler;
    cx->on_socket_udata = udata;
}

/*
 * Subscribe to input. When data is available on <fd>, <on_file_handler> will be called with the given
 * <cx>, <fd> and <udata>. Only one handler per file descriptor can be set, subsequent calls will
 * override earlier ones.
 */
void cxOnFile(CX *cx, int fd, void (*on_file_handler)(CX *cx, int fd, void *udata), void *udata)
{
    cx_add_file(cx, fd);

    cx->connection[fd]->on_file_udata = udata;
    cx->connection[fd]->on_file_handler = on_file_handler;
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
 * Call <handler> with <udata> when a new connection is made, reporting the new file descriptor
 * through <fd>.
 */
void cxOnConnect(CX *cx, void (*handler)(CX *cx, int fd, void *udata), void *udata)
{
    cx->on_connect_handler = handler;
    cx->on_connect_udata = udata;
}

/*
 * Call <handler> with <udata> when the connection on <fd> is lost.
 */
void cxOnDisconnect(CX *cx, void (*handler)(CX *cx, int fd, void *udata), void *udata)
{
    cx->on_disconnect_handler = handler;
    cx->on_disconnect_udata = udata;
}

/*
 * Call <handler> when an error has occurred on <fd>. <error> is the associated errno.
 */
void cxOnError(CX *cx, void (*handler)(CX *cx, int fd, int error, void *udata), void *udata)
{
    cx->on_error_handler = handler;
    cx->on_error_udata = udata;
}

/*
 * Add <data> with <size> to the output buffer of <fd>. The data will be sent when the
 * flow-of-control returns to the main loop.
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
 * cxOnFile call. Return the number of file descriptors that may be set. <rfds> can then be
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
 * Clear <wfds>, then fill it with the file descriptors that have data queued for write. Return the
 * number of file descriptors that may be set. <wfds> can then be passed to select().
 */
int cxGetWriteFDs(CX *cx, fd_set *wfds)
{
    int fd;

    FD_ZERO(wfds);

    for (fd = 0; fd < cx->num_connections; fd++) {
        if (cx->connection[fd] == NULL || bufLen(&cx->connection[fd]->outgoing) == 0) continue;

        FD_SET(fd, wfds);
    }

    return cx->num_connections;
}

/*
 * Return TRUE if <fd> is handled by <cx>.
 */
int cxOwnsFD(CX *cx, int fd)
{
    return (fd >= 0 && fd < cx->num_connections && cx->connection[fd] != NULL);
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
        double dt = tm->t - nowd();

        if (dt < 0) dt = 0;

        cx_double_to_timeval(dt, tv);

        return 1;
    }
}

/*
 * Process the results from a select() call. Returns 0 on success or -1 on error.
 */
int cxProcessSelect(CX *cx, int r, fd_set *rfds, fd_set *wfds)
{
    if (r == -1) {
        if (errno != EINTR) return -1;
    }
    else if (r == 0) {
        CX_Timeout *tm = listRemoveHead(&cx->timeouts);

        tm->on_time_handler(cx, tm->t, tm->on_time_udata);

        free(tm);
    }
    else {
        int fd;

        for (fd = 0; fd < cx->num_connections; fd++) {
            CX_Connection *conn = cx->connection[fd];

            if (conn == NULL) continue;

            if (FD_ISSET(fd, rfds)) {
                conn->on_file_handler(cx, fd, conn->on_file_udata);
            }

            if (FD_ISSET(fd, wfds)) {
                cx_handle_writeable(cx, fd);
            }
        }
    }

    return 0;
}

/*
 * Run the communications exchange. This function will return when there are no more timeouts to
 * wait for and no file descriptors to listen on (which can be forced by calling cxClose()). The
 * return value in this case will be 0. If any error occurred it will be -1.
 */
int cxRun(CX *cx)
{
    int r = 0;

    while (r >= 0) {
        struct timeval tv;
        fd_set rfds, wfds;

        int nfds = cx->num_connections;

        cxGetReadFDs(cx, &rfds);
        cxGetWriteFDs(cx, &wfds);

        r = cxGetTimeout(cx, &tv);

        if (nfds == 0 && r == 0) break;

        r = select(nfds, &rfds, &wfds, NULL, r ? &tv : NULL);

        if (r < 0) {
            dbgPrint(stderr, "select returned %d (%s)\n", r, strerror(errno));
            break;
        }

        r = cxProcessSelect(cx, r, &rfds, &wfds);

        if (r < 0) {
            dbgPrint(stderr, "cxProcessSelect returned %d (%s)\n", r, strerror(errno));
            break;
        }
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
            bufReset(&cx->connection[fd]->incoming);
            bufReset(&cx->connection[fd]->outgoing);

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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "net.h"
#include "udp.h"

/*
 * The test code forks off two servers. The first handles connections by opening its own sockets and
 * monitoring them through the cxOnFile function. The second uses the cxTcpListen, cxUdpListen,
 * cxOnConnect and cxOnSocket functions. The remaining parent process is connected to both
 * servers using pipes, through which it monitors the output of the servers as it connects to them
 * and sends them data. The client itself tests the cxOnTime and cxOnDisconnect functions.
 */

int errors = 0;

static int global_report_fd;

/* ### Callbacks shared by both servers ### */

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
 * Handle incoming data (callback for cxOnFile on a connected socket).
 */
void handle_data(CX *cx, int fd, void *udata)
{
    char buffer[1500];

    int r = read(fd, buffer, sizeof(buffer));

    report(global_report_fd, "received '%.*s' on %s", r, buffer, (char *) udata);

    if (memcmp(buffer, "Quit", r) == 0) {
        cxClose(cx);
    }
}

/*
 * Accept connection request (callback for cxOnFile on a listen socket).
 */
void accept_connection(CX *cx, int fd, void *udata)
{
    report(global_report_fd, "accept connection on %s", (char *) udata);

    fd = tcpAccept(fd);

    cxOnFile(cx, fd, handle_data, udata);
}

/*
 * Report a new connection (callback for cxOnConnect).
 */
void report_connect(CX *cx, int fd, void *udata)
{
    report(global_report_fd, "accept connection on %s", (char *) udata);
}

/*
 * Handle incoming socket data (callback for cxOnSocket).
 */
void handle_socket(CX *cx, int fd, const char *data, size_t size, void *udata)
{
    report(global_report_fd, "received '%.*s' on %s", size, data, (char *) udata);

    if (memcmp(data, "Quit", size) == 0) {
        cxClose(cx);
    }
}

/* ### Server 1 ### */

void server1(int report_fd, int tcp_port, int udp_port)
{
    CX *cx = cxCreate();

    int tcp_fd = tcpListen("localhost", tcp_port);
    int udp_fd = udpSocket();

    netBind(udp_fd, "localhost", udp_port);

    global_report_fd = report_fd;

    cxOnFile(cx, tcp_fd, accept_connection, "server1 tcp");
    cxOnFile(cx, udp_fd, handle_data, "server1 udp");

    cxRun(cx);

    cxFree(cx);

    exit(0);
}

/* ### Server 2 ### */

void server2(int report_fd, int tcp_port, int udp_port)
{
    CX *cx = cxCreate();

    global_report_fd = report_fd;

    cxTcpListen(cx, "localhost", tcp_port);
    cxUdpListen(cx, "localhost", udp_port);

    cxOnConnect(cx, report_connect, "server2 tcp");
    cxOnSocket(cx, handle_socket, "server2");

    cxRun(cx);

    cxFree(cx);

    exit(0);
}

/* ### Client callbacks ### */

/*
 * Handle timeout (callback for cxOnTime in the client). Sets up connections to server 1.
 */
void handle_timeout_1(CX *cx, double t, void *udata)
{
    int *fds = udata;

    fds[0] = cxTcpConnect(cx, "localhost", 10001);
    fds[1] = cxUdpConnect(cx, "localhost", 10002);
}

/*
 * Handle timeout (callback for cxOnTime in the client). Sets up connections to server 2.
 */
void handle_timeout_2(CX *cx, double t, void *udata)
{
    int *fds = udata;

    fds[2] = cxTcpConnect(cx, "localhost", 10003);
    fds[3] = cxUdpConnect(cx, "localhost", 10004);
}

/*
 * Handle an incoming report from <fd>. Callback for cxOnFile and also, slightly hackishly for
 * cxOnDisconnect.
 */
void handle_report(CX *cx, int fd, void *udata)
{
    /* We will test the communications step by step. At every step we will check if we get the
     * expected response from the servers and then trigger the next step. */

    static int step = 0;
    char *expected_response[] = {           /* Expected responses at every step. */
        "accept connection on server1 tcp", /* First the servers accept new connections... */
        "accept connection on server2 tcp",
        "received '1' on server1 tcp",      /* Then server1 gets data on both its sockets... */
        "received '2' on server1 udp",
        "received '3' on server2",          /* server2 uses the same callback for UDP and TCP, */
        "received '4' on server2",          /* so we can't differentiate between the two. */
        "received 'Quit' on server1 tcp",   /* server1 about to shut down... */
        "",                                 /* Empty response on closed connection */
        "received 'Quit' on server2",       /* server2 about to shut down... */
        ""                                  /* Empty response on closed connection */
    };
    int n_responses = sizeof(expected_response) / sizeof(expected_response[0]);

    char buffer[100];

    int *fds = udata;   /* Client communicates the fds to use via udata... */

    int r = read(fd, buffer, sizeof(buffer));

D   fprintf(stderr, "handle_report, step %d, %d bytes: %.*s\n", step, r, r, buffer);

    if (step >= n_responses) {
        fprintf(stderr, "Missing expected_response for step %d.\n", step);
    }
    else if (strncmp(buffer, expected_response[step], strlen(expected_response[step])) != 0) {
        fprintf(stderr, "Unexpected response in step %d:\n\tExp: \"%s\"\n\tGot: \"%.*s\"\n",
                step, expected_response[step], r, buffer);
    }

    switch(step) {
    case 0:                         /* server1 is connected. */
        break;                      /* No action required. */
    case 1:                         /* server2 now also connected. */
        write(fds[0], "1", 1);      /* Test server1's TCP socket. */
        break;
    case 2:                         /* Got response. */
        write(fds[1], "2", 1);      /* Test server1's UDP socket. */
        break;
    case 3:                         /* Got response. */
        write(fds[2], "3", 1);      /* Test server2's TCP socket. */
        break;
    case 4:                         /* Got response. */
        write(fds[3], "4", 1);      /* Test server2's UDP socket. */
        break;
    case 5:                         /* Got response; tests complete. */
        write(fds[0], "Quit", 4);   /* Tell server1 to quit. */
        break;
    case 6:                         /* server1 has received a Quit command. */
        break;                      /* No action required. */
    case 7:                         /* server1 has shut down. */
        write(fds[2], "Quit", 4);   /* Tell server2 to quit. */
        break;
    case 8:                         /* server2 has received a Quit command. */
        break;                      /* No action required. */
    case 9:                         /* server2 also gone. */
        cxClose(cx);                /* Shut down myself. */
        break;
    default:
        break;
    }

    step++;
}

int main(int argc, char *argv[])
{
    CX *cx;

    int server1_pipe[2], server2_pipe[2], fds[4];

    pipe(server1_pipe);
    pipe(server2_pipe);

    if (fork() == 0) {
        server1(server1_pipe[1], 10001, 10002);
    }
    else if (fork() == 0) {
        server2(server2_pipe[1], 10003, 10004);
    }

    cx = cxCreate();

    cxOnFile(cx, server1_pipe[0], handle_report, fds);
    cxOnFile(cx, server2_pipe[0], handle_report, fds);

    cxOnTime(cx, nowd() + 0.1, handle_timeout_1, fds);
    cxOnTime(cx, nowd() + 0.2, handle_timeout_2, fds);

    cxOnDisconnect(cx, handle_report, fds);

    cxRun(cx);

    cxFree(cx);

    return errors;
}
#endif
