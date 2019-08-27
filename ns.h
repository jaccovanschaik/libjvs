#ifndef NS_H
#define NS_H

/*
 * ns.h: Network Server.
 *
 * ns.h is part of libjvs.
 *
 * Copyright:   (c) 2013-2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: ns.h 343 2019-08-27 08:39:24Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/select.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct NS NS;

/*
 * Initialize network server <ns>.
 */
void nsInit(NS *ns);

/*
 * Create a new Network Server.
 */
NS *nsCreate(void);

/*
 * Open a listen socket on port <port>, address <host> and return its file descriptor. If <port> <=
 * 0, a random port will be opened (find out which using netLocalPort() on the returned file
 * descriptor). If <host> is NULL, the socket will listen on all interfaces. Connection requests
 * will be accepted automatically. Data coming in on the resulting socket will be reported via the
 * callback installed using nsOnSocket().
 */
int nsListen(NS *ns, const char *host, uint16_t port);

/*
 * Arrange for <cb> to be called when a new connection is accepted.
 */
void nsOnConnect(NS *ns, void (*cb)(NS *ns, int fd, void *udata), void *udata);

/*
 * Arrange for <cb> to be called when a connection is lost. *Not* called on nsDisconnect().
 */
void nsOnDisconnect(NS *ns, void (*cb)(NS *ns, int fd, void *udata), void *udata);

/*
 * Arrange for <cb> to be called when data comes in on any connected socket. <cb> will be called
 * with the <ns>, <fd> and <udata> given here, and with a pointer to the collected data so far in
 * <buffer> with its size in <size>.
 */
void nsOnSocket(NS *ns, void (*cb)(NS *ns, int fd, const char *buffer, int size, void *udata),
        void *udata);

/*
 * Arrange for <cb> to be called when a connection is lost. *Not* called on nsDisconnect().
 */
void nsOnError(NS *ns, void (*cb)(NS *ns, int fd, int error, void *udata), void *udata);

/*
 * Make a connection to the given <host> and <port>. Incoming data on this 
 * socket is reported using the callback installed with nsOnSocket(). The new 
 * file descriptor is returned, or -1 if an error occurred.
 */
int nsConnect(NS *ns, const char *host, uint16_t port);

/*
 * Disconnect from a file descriptor that was returned earlier using nsConnect().
 */
void nsDisconnect(NS *ns, int fd);

/*
 * Arrange for <cb> to be called when there is data available on file descriptor <fd>. <cb> will be
 * called with the given <ns>, <fd> and <udata>, which is a pointer to "user data" that will be
 * returned <cb> as it was given here, and that will not be accessed by dis in any way.
 */
void nsOnData(NS *ns, int fd, void (*cb)(NS *ns, int fd, void *udata), const void *udata);

/*
 * Drop the subscription on file descriptor <fd>.
 */
void nsDropData(NS *ns, int fd);

/*
 * Write the first <size> bytes of <data> to <fd>, which must be known to <ns>. You may write to
 * <fd> via any other means, but if you use this function the data will be written out, possibly
 * piece by piece but always without blocking, when the given file descriptor becomes writable.
 */
void nsWrite(NS *ns, int fd, const char *data, size_t size);

/*
 * Pack the arguments following <fd> according to the strpack interface from utils.h and send the
 * resulting string to <fd> via <ns>.
 */
void nsPack(NS *ns, int fd, ...);

/*
 * Pack the arguments contained in <ap> according to the strpack interface from utils.h and send the
 * resulting string to <fd> via <ns>.
 */
void nsVaPack(NS *ns, int fd, va_list ap);

/*
 * Return a pointer to the start of the incoming buffer.
 */
const char *nsIncoming(NS *ns, int fd);

/*
 * Return the number of bytes available in the incoming buffer for <fd>.
 */
int nsAvailable(NS *ns, int fd);

/*
 * Discard the first <length> bytes of the incoming buffer for file descriptor <fd> (becuase you've
 * processed them).
 */
void nsDiscard(NS *ns, int fd, int length);

/*
 * Arrange for <cb> to be called at time <t>, which is the (double precision floating point) number
 * of seconds since 00:00:00 UTC on 1970-01-01 (aka. the UNIX epoch). <cb> will be called with the
 * given <ns>, <t> and <udata>. You can get the current time using nowd() from utils.c.
 */
void nsOnTime(NS *ns, double t, void (*cb)(NS *ns, double t, void *udata), const void *udata);

/*
 * Cancel the timer that was set for time <t> with callback <cb>.
 */
void nsDropTime(NS *ns, double t, void (*cb)(NS *ns, double t, void *udata));

/*
 * Return the number of file descriptors that <ns> is monitoring (i.e. max_fd - 1).
 */
int nsFdCount(NS *ns);

/*
 * Return TRUE if <fd> has been given to <ns> using nsOnData(), FALSE otherwise.
 */
int nsOwnsFd(NS *ns, int fd);

/*
 * Prepare a call to select() based on the files and timeouts set in <ns>. The
 * necessary parameters to select() are returned through <nfds>, <rfds>, <wfds>
 * and <tv> (exception-fds should be set to NULL). <*tv> is set to point to an
 * appropriate timeout value, or NULL if no timeout is to be set. This function
 * will clear <rfds> and <wfds>, so if callers want to add their own file
 * descriptors, they should do so after calling this function. This function
 * returns -1 if the first timeout should already have occurred, otherwise 0.
 */
int nsPrepareSelect(NS *ns, int *nfds, fd_set *rfds, fd_set *wfds, struct timeval **tv);

/*
 * Process (and subsequently discard) the first pending timeout.
 */
void nsHandleTimer(NS *ns);

/*
 * Handle readable and writable file descriptors in <rfds> and <wfds>, with <nfds> set to the
 * maximum number of file descriptors that may be set in <rfds> or <wfds>.
 */
void nsHandleFiles(NS *ns, int nfds, fd_set *rfds, fd_set *wfds);

/*
 * Handle readable file descriptor <fd>.
 */
void nsHandleReadable(NS *ns, int fd);

/*
 * Handle writable file descriptor <fd>.
 */
void nsHandleWritable(NS *ns, int fd);

/*
 * Process the results of a call to select(). <r> is select's return value, <rfds> and <wfds>
 * contain the file descriptors that select has marked as readable or writable and <nfds> is the
 * maximum number of file descriptors that may be set in <rfds> or <wfds>.
 */
void nsProcessSelect(NS *ns, int r, int nfds, fd_set *rfds, fd_set *wfds);

/*
 * Wait for file or timer events and handle them. This function returns 1 if there are no files or
 * timers to wait for, -1 if some error occurred, or 0 if any number of events was successfully
 * handled.
 */
int nsHandleEvents(NS *ns);

/*
 * Run the network server <ns>.
 */
int nsRun(NS *ns);

/*
 * Close the network server <ns>. This removes all file descriptors and timers, which will cause
 * nsRun() to return.
 */
void nsClose(NS *ns);

/*
 * Clear the contents of <ns> but don't free <ns> itself.
 */
void nsClear(NS *ns);

/*
 * Clear the contents of <ns> and then free <ns> itself. Do not call this from inside the nsRun()
 * loop. Instead, call nsClose(), wait for nsRun() to return and then call nsDestroy().
 */
void nsDestroy(NS *ns);

#ifdef __cplusplus
}
#endif

#endif
