#ifndef CX_H
#define CX_H

/*
 * cx.h: Communications Exchange.
 *
 * Copyright:	(c) 2012 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stddef.h>
#include <sys/select.h>

typedef struct CX CX;

/*
 * Create a communications exchange.
 */
CX *cxCreate(void);

/*
 * Subscribe to input. When data is available on <fd>, <on_file_data> will be called with the given
 * <cx>, <fd> and <udata>. Only one handler per file descriptor can be set, subsequent calls will
 * override earlier ones.
 */
void cxOnFileData(CX *cx, int fd, void (*on_file_data)(CX *cx, int fd, void *udata), void *udata);

/*
 * Drop subscription to fd <fd>.
 */
void cxDropFile(CX *cx, int fd);

/*
 * Return the current UTC time (number of seconds since 1970-01-01/00:00:00 UTC) as a double.
 */
double cxNow(void);

/*
 * Set a handler to be called at time <t> (in seconds since 1970-01-01/00:00:00 UTC). <on_time> will
 * be called with the given <cx>, <t> and <udata>.
 */
void cxOnTime(CX *cx, double t, void (*on_time)(CX *cx, double t, void *udata), void *udata);

/*
 * Drop timeout at time <t>. Both <t> and <on_time> must match the earlier call to cxOnTime.
 */
void cxDropTime(CX *cx, double t, void (*on_time)(CX *cx, double t, void *udata));

/*
 * Call <handler> with <udata> when a new connection is made on <fd>.
 */
void cxOnConnect(CX *cx, void (*handler)(CX *cx, int fd, void *udata), void *udata);

/*
 * Call <handler> with <udata> when the connection on <fd> is lost.
 */
void cxOnDisconnect(CX *cx, void (*handler)(CX *cx, int fd, void *udata), void *udata);

/*
 * Call <handler> when an error has occurred on <fd>. <error> is the associated errno.
 */
void cxOnSocketError(CX *cx, void (*handler)(CX *cx, int fd, int error, void *udata), void *udata);

/*
 * Call <handler> when new data on one of the connected sockets comes in.
 */
void cxOnSocketData(CX *cx, void (*handler)(CX *cx, int fd, const char *data, size_t size, void
                      *udata), void *udata);

/*
 * Open a listen socket bound to <host> and <port>.
 */
int cxTcpListen(CX *cx, const char *host, int port);

/*
 * Open a UDP socket bound to <host> and <port> and listen on it for data.
 */
int cxUdpListen(CX *cx, const char *host, int port);

/*
 * Make a TCP connection to <host> on <port>.
 */
int cxTcpConnect(CX *cx, const char *host, int port);

/*
 * Make a UDP "connection" to <host> on <port>.
 */
int cxUdpConnect(CX *cx, const char *host, int port);

/*
 * Add <data> with <size> to the output buffer of <fd>. The data will be sent when the
 * flow-of-control returns to the main loop.
 */
void cxSend(CX *cx, int fd, const char *data, size_t size);

/*
 * Clear <rfds>, then fill it with the file descriptors that have been given to <cx> in a
 * cxOnFileData call. Return the number of file descriptors that may be set. <rfds> can then be
 * passed to select().
 */
int cxGetReadFDs(CX *cx, fd_set *rfds);

/*
 * Clear <wfds>, then fill it with the file descriptors that have data queued for write. Return the
 * number of file descriptors that may be set. <wfds> can then be passed to select().
 */
int cxGetWriteFDs(CX *cx, fd_set *wfds);

/*
 * Return TRUE if <fd> is handled by <cx>.
 */
int cxOwnsFD(CX *cx, int fd);

/*
 * Get the timeout to use for a call to select. If a timeout is required it is copied to <tv>, and 1
 * is returned. Otherwise 0 is returned, and the last parameter of select should be set to NULL.
 * <tv> is not changed in that case.
 */
int cxGetTimeout(CX *cx, struct timeval *tv);

/*
 * Process the results from a select() call.
 */
int cxProcessSelect(CX *cx, int r, fd_set *rfds, fd_set *wfds);

/*
 * Run the communications exchange. This function will return when there are no more timeouts to
 * wait for and no file descriptors to listen on (which can be forced by calling cxClose()).
 */
int cxRun(CX *cx);

/*
 * Close down Communications Exchange <cx>. This forcibly stops <cx> from listening on any file
 * descriptor and removes all pending timeouts, which causes cxRun() to return.
 */
void cxClose(CX *cx);

/*
 * Free the memory occupied by <cx>. Call this outside of the cx loop, i.e. after cxRun() returns.
 * You can force cxRun() to return by calling cxClose().
 */
void cxFree(CX *cx);

#endif