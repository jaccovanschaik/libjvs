#ifndef NX_H
#define NX_H

/* nx.h: Description
 *
 * Copyright:	(c) 2012 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <sys/select.h>

#include "buffer.h"

typedef struct NX NX;

/*
 * Create a Network Exchange listening on <host> and <port>. <host> may
 * be NULL, in which case the Exchange will listen on all interfaces.
 * <port> may be -1, in which case the system will choose a port number.
 * Use nxListenPort to find out which port was chosen.
 */
NX *nxCreate(const char *host, int port);

/*
 * Close down this Network Exchange. Closes the listen port and all
 * other connections and cancels all timeouts. The nxRun() mainloop will
 * then exit.
 */
void nxClose(NX *nx);

/*
 * Destroy Network Exchange <nx>. It is a bad idea to call this inside a
 * callback function. Instead, call nxClose() inside the callback, then
 * call nxDestroy once nxRun() has returned.
 */
void nxDestroy(NX *nx);

/*
 * Return the port that <nx> listens on.
 */
int nxListenPort(NX *nx);

/*
 * Return the hostname that <nx> listens on. May be "0.0.0.0", in which
 * case <nx> listens on all interfaces.
 */
const char *nxListenHost(NX *nx);

/*
 * Return the local port for the connection on <fd>.
 */
int nxLocalPort(int fd);

/*
 * Return the local hostname for the connection on <fd>.
 */
const char *nxLocalHost(int fd);

/*
 * Return the remote port for the connection on <fd>.
 */
int nxRemotePort(int fd);

/*
 * Return the remote hostname for the connection on <fd>.
 */
const char *nxRemoteHost(int fd);

/*
 * Queue the first <len> bytes from <data> to be sent over <fd>. Returns
 * the number of bytes queued (which is always <len>).
 */
int nxQueue(NX *nx, int fd, const Buffer *data);

/*
 * Add data received on <fd> to <data>. Returns the number of bytes added.
 */
int nxGet(NX *nx, int fd, Buffer *data);

/*
 * Make and return a connection to port <port> on host <host>.
 */
int nxConnect(NX *nx, const char *host, int port);

/*
 * Disconnect connection the connection on <fd>.
 */
void nxDisconnect(NX *nx, int fd);

/*
 * Set <handler> as the function to be called when a new connection is
 * made. Only one function can be set; subsequent calls will overwrite
 * previous ones. <handler> will *not* be called for connections users
 * create themselves (using nxConnect()).
 */
void nxOnConnect(NX *nx, void (*handler)(NX *nx, int fd));

/*
 * Set <handler> as the function to be called when a connection is
 * dropped. Only one function can be set; subsequent calls will
 * overwrite previous ones. <handler> will *not* be called for
 * connections users drop themselves (using nxDisconnect()).
 */
void nxOnDisconnect(NX *nx, void (*handler)(NX *nx, int fd));

/*
 * Set <handler> as the function to be called when new data comes in.
 * Only one function can be set; subsequent calls will overwrite
 * previous ones.
 */
void nxOnData(NX *nx, void (*handler)(NX *nx, int fd));

/*
 * Set <handler> as the function to be called when an error occurs. The
 * connection on which it occurred and the errno code will be passed to
 * the handler.
 */
void nxOnError(NX *nx, void (*handler)(NX *nx, int fd, int err));

/*
 * Return the current *UTC* time (number of seconds since
 * 1970-01-01/00:00:00 UTC) as a double.
 */
double nxNow(void);

/*
 * Add a timeout at UTC time t, calling <handler> with <nx>, time <t>
 * and <udata>.
 */
void nxAddTimeout(NX *nx, double t, void *udata, void (*handler)(NX *nx, double t,
                    void *udata));

/*
 * Return an array of file descriptor that <nx> wants to listen on. The number of returned file
 * descriptors is returned through <count>.
 */
int *nxFdsForReading(NX *nx, int *count);

/*
 * Return an array of file descriptor that <nx> wants to write to. The number of returned file
 * descriptors is returned through <count>.
 */
int *nxFdsForWriting(NX *nx, int *count);

/*
 * Prepare arguments for a call to select() on behalf of <nx>. Returned are the nfds, rfds and wfds
 * parameters and a pointer to a struct timeval pointer.
 */
int nxPrepareSelect(NX *nx, int *nfds, fd_set *rfds, fd_set *wfds, struct timeval
                      **tvpp);

/*
 * Run the Network Exchange. New connection requests from external
 * parties will be accepted automatically, calling the on_connect
 * handler. On errors and end-of-file conditions connections will
 * automatically be closed, calling the on_error and on_disconnect
 * handlers. This function will return either when the select inside
 * fails (returning errno) or when there are no more connections and
 * timeouts left.
 */
int nxRun(NX *nx);

int nxHandleSelect(NX *nx, int r, fd_set *rfds, fd_set *wfds);

#endif
