/*
 * ns.c: Network Server.
 *
 * ns.c is part of libjvs.
 *
 * Copyright:   (c) 2013-2022 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: ns.c 462 2022-08-19 06:10:50Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "net.h"
#include "tcp.h"
#include "dis.h"
#include "utils.h"
#include "debug.h"

#include "ns.h"
#include "ns-types.h"

static void ns_handle_data(Dispatcher *dis, int fd, __attribute__((unused)) void *udata)
{
    char data[9000];

    NS *ns = (NS *) dis;
    NS_Connection *conn = paGet(&ns->connections, fd);

    int n;

    P dbgPrint(stderr, "Reading from fd %d...\n", fd);

    n = read(fd, data, sizeof(data));

    P dbgPrint(stderr, "read() returned %d.\n", n);

    if (n > 0) {
        P dbgPrint(stderr, "Adding to incoming buffer.\n");

        bufAdd(&conn->incoming, data, n);

        if (ns->on_socket_cb != NULL) {
            P dbgPrint(stderr, "Calling on_socket_cb.\n");

            ns->on_socket_cb(ns, fd,
                    bufGet(&conn->incoming), bufLen(&conn->incoming),
                    ns->on_socket_udata);
        }
    }
    else if (n == 0) {
        P dbgPrint(stderr, "End of file, disconnecting.\n");

        nsDisconnect(ns, fd);

        if (ns->on_disconnect_cb != NULL) {
            P dbgPrint(stderr, "Calling on_disconnect_cb.\n");

            ns->on_disconnect_cb(ns, fd, ns->on_disconnect_udata);
        }
    }
    else {
        P dbgPrint(stderr, "Error, disconnecting.\n");

        nsDisconnect(ns, fd);

        if (ns->on_error_cb != NULL) {
            P dbgPrint(stderr, "Calling on_error_cb.\n");

            ns->on_error_cb(ns, fd, errno, ns->on_error_udata);
        }
    }
}

static void ns_add_connection(NS *ns, int fd)
{
    NS_Connection *conn = calloc(1, sizeof(NS_Connection));

    paSet(&ns->connections, fd, conn);

    P dbgPrint(stderr, "New connection on fd %d\n", fd);

    disOnData(&ns->dis, fd, ns_handle_data, NULL);
}

static void ns_accept_connection(Dispatcher *dis, int fd,
        __attribute__((unused)) void *udata)
{
    NS *ns = (NS *) dis;

    fd = tcpAccept(fd);

    ns_add_connection(ns, fd);

    if (ns->on_connect_cb) {
        ns->on_connect_cb(ns, fd, ns->on_connect_udata);
    }
}

/*
 * Initialize network server <ns>.
 */
void nsInit(NS *ns)
{
    memset(ns, 0, sizeof(NS));
}

/*
 * Create a new Network Server.
 */
NS *nsCreate(void)
{
    NS *ns = calloc(1, sizeof(NS));

    disInit(&ns->dis);

    return ns;
}

/*
 * Open a listen socket on port <port>, address <host> and return its file
 * descriptor. If <port> <= 0, a random port will be opened (find out which
 * using netLocalPort() on the returned file descriptor). If <host> is NULL,
 * the socket will listen on all interfaces. Connection requests will be
 * accepted automatically. Data coming in on the resulting socket will be
 * reported via the callback installed using nsOnSocket().
 */
int nsListen(NS *ns, const char *host, uint16_t port)
{
    int listen_fd;

    if ((listen_fd = tcpListen(host, port)) < 0) {
        return -1;
    }

    disOnData(&ns->dis, listen_fd, ns_accept_connection, NULL);

    return listen_fd;
}

/*
 * Arrange for <cb> to be called when a new connection is accepted.
 */
void nsOnConnect(NS *ns, void (*cb)(NS *ns, int fd, void *udata), void *udata)
{
    ns->on_connect_cb = cb;
    ns->on_connect_udata = udata;
}

/*
 * Arrange for <cb> to be called when a connection is lost. *Not* called on
 * nsDisconnect().
 */
void nsOnDisconnect(NS *ns, void (*cb)(NS *ns, int fd, void *udata), void *udata)
{
    ns->on_disconnect_cb = cb;
    ns->on_disconnect_udata = udata;
}

/*
 * Arrange for <cb> to be called when data comes in on any connected socket.
 * <cb> will be called with the <ns>, <fd> and <udata> given here, and with a
 * pointer to the collected data so far in <buffer> with its size in <size>.
 */
void nsOnSocket(NS *ns, void (*cb)(NS *ns, int fd, const char *buffer, int size, void *udata),
        void *udata)
{
    ns->on_socket_cb = cb;
    ns->on_socket_udata = udata;
}

/*
 * Arrange for <cb> to be called when a connection is lost. *Not* called on
 * nsDisconnect().
 */
void nsOnError(NS *ns, void (*cb)(NS *ns, int fd, int error, void *udata), void *udata)
{
    ns->on_error_cb = cb;
    ns->on_error_udata = udata;
}

/*
 * Make a connection to the given <host> and <port>. Incoming data on this
 * socket is reported using the callback installed with nsOnSocket(). The new
 * file descriptor is returned, or -1 if an error occurred.
 */
int nsConnect(NS *ns, const char *host, uint16_t port)
{
    int fd = tcpConnect(host, port);

    if (fd >= 0) {
        ns_add_connection(ns, fd);
    }

    return fd;
}

/*
 * Disconnect from a file descriptor that was returned earlier using
 * nsConnect().
 */
void nsDisconnect(NS *ns, int fd)
{
    NS_Connection *conn = paGet(&ns->connections, fd);

    P dbgPrint(stderr, "Closing fd %d\n", fd);

    close(fd);

    disDropData(&ns->dis, fd);

    paDrop(&ns->connections, fd);

    bufRewind(&conn->incoming);

    free(conn);
}

/*
 * Arrange for <cb> to be called when there is data available on file
 * descriptor <fd>. <cb> will be called with the given <ns>, <fd> and <udata>,
 * which is a pointer to "user data" that will be returned <cb> as it was
 * given here, and that will not be accessed by dis in any way.
 */
void nsOnData(NS *ns, int fd, void (*cb)(NS *ns, int fd, void *udata),
        const void *udata)
{
    disOnData(&ns->dis, fd,
            (void(*)(Dispatcher *dis, int fd, void *udata)) cb, udata);
}

/*
 * Drop the subscription on file descriptor <fd>.
 */
void nsDropData(NS *ns, int fd)
{
    P dbgPrint(stderr, "Calling disDropData for fd %d\n", fd);

    disDropData(&ns->dis, fd);
}

/*
 * Write the first <size> bytes of <data> to <fd>, which must be known to
 * <ns>. You may write to <fd> via any other means, but if you use this
 * function the data will be written out, possibly piece by piece but always
 * without blocking, when the given file descriptor becomes writable.
 */
void nsWrite(NS *ns, int fd, const char *data, size_t size)
{
    disWrite(&ns->dis, fd, data, size);
}

/*
 * Pack the arguments following <fd> according to the strpack interface from
 * utils.h and send the resulting string to <fd> via <ns>.
 */
void nsPack(NS *ns, int fd, ...)
{
    va_list ap;

    va_start(ap, fd);
    nsVaPack(ns, fd, ap);
    va_end(ap);
}

/*
 * Pack the arguments contained in <ap> according to the strpack interface
 * from utils.h and send the resulting string to <fd> via <ns>.
 */
void nsVaPack(NS *ns, int fd, va_list ap)
{
    char *str;

    int n = vastrpack(&str, ap);

    nsWrite(ns, fd, str, n);

    free(str);
}

/*
 * Return a pointer to the start of the incoming buffer.
 */
const char *nsIncoming(NS *ns, int fd)
{
    NS_Connection *conn = paGet(&ns->connections, fd);

    return bufGet(&conn->incoming);
}

/*
 * Return the number of bytes available in the incoming buffer for <fd>.
 */
int nsAvailable(NS *ns, int fd)
{
    NS_Connection *conn = paGet(&ns->connections, fd);

    return bufLen(&conn->incoming);
}

/*
 * Discard the first <length> bytes of the incoming buffer for file descriptor
 * <fd> (becuase you've processed them).
 */
void nsDiscard(NS *ns, int fd, int length)
{
    NS_Connection *conn = paGet(&ns->connections, fd);

    bufTrim(&conn->incoming, length, 0);
}

/*
 * Arrange for <cb> to be called at time <t>, which is the (double precision
 * floating point) number of seconds since 00:00:00 UTC on 1970-01-01 (aka.
 * the UNIX epoch). <cb> will be called with the given <ns>, <t> and <udata>.
 * You can get the current time using nowd() from utils.c.
 */
void nsOnTime(NS *ns, double t, void (*cb)(NS *ns, double t, void *udata),
        const void *udata)
{
    disOnTime(&ns->dis, t,
            (void(*)(Dispatcher *dis, double t, void *udata)) cb, udata);
}

/*
 * Cancel the timer that was set for time <t> with callback <cb>.
 */
void nsDropTime(NS *ns, double t, void (*cb)(NS *ns, double t, void *udata))
{
    disDropTime(&ns->dis, t,
            (void(*)(Dispatcher *dis, double t, void *udata)) cb);
}

/*
 * Return the number of file descriptors that <ns> is monitoring
 * (i.e. max_fd - 1).
 */
int nsFdCount(NS *ns)
{
    return disFdCount(&ns->dis);
}

/*
 * Return TRUE if <fd> has been given to <ns> using nsOnData(), FALSE otherwise.
 */
int nsOwnsFd(NS *ns, int fd)
{
    return disOwnsFd(&ns->dis, fd);
}

/*
 * Prepare a call to select() based on the files and timeouts set in <ns>. The
 * necessary parameters to select() are returned through <nfds>, <rfds>,
 * <wfds> and <tv> (exception-fds should be set to NULL). <*tv> is set to
 * point to an appropriate timeout value, or NULL if no timeout is to be set.
 * This function will clear <rfds> and <wfds>, so if callers want to add their
 * own file descriptors, they should do so after calling this function. This
 * function returns -1 if the first timeout should already have occurred,
 * otherwise 0.
 */
int nsPrepareSelect(NS *ns, int *nfds, fd_set *rfds, fd_set *wfds,
        struct timeval **tv)
{
    return disPrepareSelect(&ns->dis, nfds, rfds, wfds, tv);
}

/*
 * Process (and subsequently discard) the first pending timeout.
 */
void nsHandleTimer(NS *ns)
{
    disHandleTimer(&ns->dis);
}

/*
 * Handle readable and writable file descriptors in <rfds> and <wfds>, with
 * <nfds> set to the maximum number of file descriptors that may be set in
 * <rfds> or <wfds>.
 */
void nsHandleFiles(NS *ns, int nfds, fd_set *rfds, fd_set *wfds)
{
    disHandleFiles(&ns->dis, nfds, rfds, wfds);
}

/*
 * Handle readable file descriptor <fd>.
 */
void nsHandleReadable(NS *ns, int fd)
{
    disHandleReadable(&ns->dis, fd);
}

/*
 * Handle writable file descriptor <fd>.
 */
void nsHandleWritable(NS *ns, int fd)
{
    disHandleWritable(&ns->dis, fd);
}

/*
 * Process the results of a call to select(). <r> is select's return value,
 * <rfds> and <wfds> contain the file descriptors that select has marked as
 * readable or writable and <nfds> is the maximum number of file descriptors
 * that may be set in <rfds> or <wfds>.
 */
void nsProcessSelect(NS *ns, int r, int nfds, fd_set *rfds, fd_set *wfds)
{
    disProcessSelect(&ns->dis, r, nfds, rfds, wfds);
}

/*
 * Wait for file or timer events and handle them. This function returns 1 if
 * there are no files or timers to wait for, -1 if some error occurred, or 0
 * if any number of events was successfully handled.
 */
int nsHandleEvents(NS *ns)
{
    return disHandleEvents(&ns->dis);
}

/*
 * Run the network server <ns>.
 */
int nsRun(NS *ns)
{
    return disRun(&ns->dis);
}

/*
 * Close the network server <ns>. This removes all file descriptors and
 * timers, which will cause nsRun() to return.
 */
void nsClose(NS *ns)
{
    disClose((Dispatcher *) ns);
}

/*
 * Clear the contents of <ns> but don't free <ns> itself.
 */
void nsClear(NS *ns)
{
    disClear(&ns->dis);

    memset(ns, 0, sizeof(NS));
}

/*
 * Clear the contents of <ns> and then free <ns> itself. Do not call this from
 * inside the nsRun() loop. Instead, call nsClose(), wait for nsRun() to
 * return and then call nsDestroy().
 */
void nsDestroy(NS *ns)
{
    nsClose(ns);

    free(ns);
}

#ifdef TEST
#include "utils.h"

int errors = 0;

static void on_time(NS *ns, double t, void *udata)
{
    uint16_t port = *((uint16_t *) udata);
    static int fd, step = 0;

    P dbgPrint(stderr, "port = %d\n", port);
    P dbgPrint(stderr, "step = %d\n", step);

    switch(step) {
    case 0:
        fd = nsConnect(ns, "localhost", port);
        break;
    case 1:
        nsPack(ns, fd, PACK_STRING, "Hoi!", END);
        break;
    default:
        nsClose(ns);
        return;
    }

    step++;

    nsOnTime(ns, t + 0.1, on_time, udata);
}

static void tester(uint16_t port)
{
    int r;

    NS *ns = nsCreate();

    nsOnTime(ns, dnow() + 0.1, on_time, &port);

    r = nsRun(ns);

    P dbgPrint(stderr, "nsRun returned %d\n", r);

    nsDestroy(ns);
}

static void on_connect(NS *ns, int fd, void *udata)
{
    UNUSED(ns);
    UNUSED(udata);

    P dbgPrint(stderr, "fd = %d\n", fd);
}

static void on_disconnect(NS *ns, int fd, void *udata)
{
    UNUSED(udata);

    P dbgPrint(stderr, "fd = %d\n", fd);

    nsClose(ns);
}

static void on_socket(NS *ns, int fd, const char *data, int size, void *udata)
{
    UNUSED(udata);

    int n;
    char *str;

    make_sure_that(data == nsIncoming(ns, fd));
    make_sure_that(size == nsAvailable(ns, fd));

    n = strunpack(data, size,
            PACK_STRING, &str,
            END);

    nsDiscard(ns, fd, n);

    P dbgPrint(stderr, "fd = %d, str = \"%s\", n = %d\n", fd, str, n);
}

static void testee(NS *ns)
{
    int r;

    nsOnConnect(ns, on_connect, NULL);
    nsOnDisconnect(ns, on_disconnect, NULL);
    nsOnSocket(ns, on_socket, NULL);

    r = nsRun(ns);

    P dbgPrint(stderr, "nsRun returned %d\n", r);

    nsDestroy(ns);
}

int main(void)
{
    NS *ns = nsCreate();

    int listen_fd   = nsListen(ns, "localhost", 0);
    uint16_t listen_port = netLocalPort(listen_fd);

    P fprintf(stderr, "listen_fd = %d\n", listen_fd);
    P fprintf(stderr, "listen_port = %d\n", listen_port);

    if (fork() == 0) {
        /* Child. */
        nsClose(ns);
        nsDestroy(ns);
        tester(listen_port);
    }
    else {
        /* Parent. */
        testee(ns);
    }

    return errors;
}
#endif
