#ifndef MX_H
#define MX_H

/*
 * mx.h: Description
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: mx.h 167 2013-08-13 18:59:00Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

typedef struct MX MX;

/*
 * Create and return a communications exchange.
 */
MX *mxCreate(void);

/*
 * Open a listen socket bound to <host> and <port> and return its file descriptor. If <host> is NULL
 * the socket will listen on all interfaces. If <port> is equal to 0, the socket will be bound to a
 * random local port (use tcpLocalPort() on the returned fd to find out which). Any connection
 * requests will be accepted automatically. Use mxOnConnect to be notified of new connections.
 * Incoming messages will be reported through the handler installed by mxOnMessage.
 */
int mxTcpListen(MX *mx, const char *host, int port);

/*
 * Open a UDP socket bound to <host> and <port> and listen on it for data. Incoming messages will be
 * reported through the handler installed by mxOnMessage. The file descriptor for the created
 * socket is returned.
 */
int mxUdpListen(MX *mx, const char *host, int port);

/*
 * Make a TCP connection to <host> on <port>. Incoming messages will be reported through the handler
 * installed by mxOnMessage. The file descriptor for the created socket is returned.
 */
int mxTcpConnect(MX *mx, const char *host, int port);

/*
 * Make a UDP "connection" to <host> on <port>. Connection in this case means that data is sent to
 * the indicated address by default, without the need to specify an address on every send. The
 * file descriptor for the created socket is returned.
 */
int mxUdpConnect(MX *mx, const char *host, int port);

/*
 * Set a handler to be called at time <t> (in seconds since 1970-01-01/00:00:00 UTC). <on_time> will
 * be called with the given <mx>, <t> and <udata>.
 */
void mxOnTime(MX *mx, double t, void (*on_time)(MX *mx, double t, void *udata), void *udata);

/*
 * Drop timeout at time <t>. Both <t> and <on_time> must match the earlier call to mxOnTime.
 */
void mxDropTime(MX *mx, double t, void (*on_time)(MX *mx, double t, void *udata));

/*
 * Return the current UTC time (number of seconds since 1970-01-01/00:00:00 UTC) as a double.
 */
double mxNow(void);

/*
 * Call <handler> when new data on one of the connected sockets comes in. You can install one
 * handler per message type. Any subsequent call to mxOnMessage with the same <type> will replace
 * the existing handler.
 */
void mxOnMessage(MX *mx, int type, void (*handler)(MX *mx, int fd, int type, int version, const
                   char *payload, size_t size, void *udata), void *udata);

/*
 * Drop an existing subscription to message type <type>.
 */
void mxDropMessage(MX *mx, int type);

/*
 * Subscribe to input. When data is available on <fd>, <handler> will be called with the given
 * <mx>, <fd> and <udata>. Only one handler per file descriptor can be set, subsequent calls will
 * override earlier ones.
 */
void mxOnFile(MX *mx, int fd, void (*handler)(MX *mx, int fd, void *udata), void *udata);

/*
 * Drop subscription to fd <fd>.
 */
void mxDropFile(MX *mx, int fd);

/*
 * Call <handler> with <udata> when a new connection is made, reporting the new file descriptor
 * through <fd>.
 */
void mxOnConnect(MX *mx, void (*handler)(MX *mx, int fd, void *udata), void *udata);

/*
 * Call <handler> with <udata> when the connection on <fd> is lost.
 */
void mxOnDisconnect(MX *mx, void (*handler)(MX *mx, int fd, void *udata), void *udata);

/*
 * Call <handler> when an error has occurred on <fd>. <error> is the associated errno.
 */
void mxOnError(MX *mx, void (*handler)(MX *mx, int fd, int error, void *udata), void *udata);

/*
 * Add a message with type <type>, version <version> and payload <payload> with size <size> to the
 * output buffer of <fd>. The message will be sent when the flow-of-control returns to the main
 * loop.
 */
void mxSend(MX *mx, int fd, int type, int version, const char *payload, size_t size);

/*
 * Clear <rfds>, then fill it with the file descriptors that have been given to <mx> in a
 * mxOnFile call. Return the number of file descriptors that may be set. <rfds> can then be
 * passed to select().
 */
int mxGetReadFDs(MX *mx, fd_set *rfds);

/*
 * Clear <wfds>, then fill it with the file descriptors that have data queued for write. Return the
 * number of file descriptors that may be set. <wfds> can then be passed to select().
 */
int mxGetWriteFDs(MX *mx, fd_set *wfds);

/*
 * Return TRUE if <fd> is handled by <mx>.
 */
int mxOwnsFD(MX *mx, int fd);

/*
 * Get the timeout to use for a call to select. If a timeout is required it is copied to <tv>, and 1
 * is returned. Otherwise 0 is returned, and the last parameter of select should be set to NULL.
 * <tv> is not changed in that case.
 */
int mxGetTimeout(MX *mx, struct timeval *tv);

/*
 * Process the results from a select() call. Returns 0 on success or -1 on error.
 */
int mxProcessSelect(MX *mx, int r, fd_set *rfds, fd_set *wfds);

/*
 * Run the communications exchange. This function will return when there are no more timeouts to
 * wait for and no file descriptors to listen on (which can be forced by calling mxClose()). The
 * return value in this case will be 0. If any error occurred it will be -1.
 */
int mxRun(MX *mx);

/*
 * Close down Communications Exchange <mx>. This forcibly stops <mx> from listening on any file
 * descriptor and removes all pending timeouts, which causes mxRun() to return.
 */
void mxClose(MX *mx);

/*
 * Free the memory occupied by <mx>. Call this outside of the mx loop, i.e. after mxRun() returns.
 * You can force mxRun() to return by calling mxClose().
 */
void mxFree(MX *mx);

#endif
