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

typedef struct NX NX;
typedef struct NX_Conn NX_Conn;

/* Create a Network Exchange. */
NX *nxCreate(const char *host, int port);

/* Close down this Network Exchange. This will cause the mainloop to
 * exit when all connections are closed and no timeouts are left. */
void nxClose(NX *nx);

/* Return the port that <nx> listens on. */
int nxListenPort(NX *nx);

/* Return the hostname that <nx> listens on. May be "0.0.0.0", in which
 * case <nx> listens on all interfaces. */
const char *nxListenHost(NX *nx);

/* Return the local port for <conn>. */
int nxLocalPort(NX_Conn *conn);

/* Return the local hostname for <conn>. */
const char *nxLocalHost(NX_Conn *conn);

/* Return the remote port for <conn>. */
int nxRemotePort(NX_Conn *conn);

/* Return the remote hostname for <conn>. */
const char *nxRemoteHost(NX_Conn *conn);

/* Queue the first <len> bytes from <data> to be sent over connection
 * <conn>. Returns the number of bytes queued (which is always <len>).
 */
int nxQueue(NX_Conn *conn, const char *data, int len);

/* Put <len> bytes received from <conn> into <data>. Returns the actual
 * number of bytes put in <data>, which may be less than <len> (even 0).
 */
int nxGet(NX_Conn *conn, char *data, int len);

/* Make and return a connection to port <port> on host <host>. */
NX_Conn *nxConnect(NX *nx, const char *host, int port);

/* Disconnect connection <conn>. */
void nxDisconnect(NX_Conn *conn);

/* Set <handler> as the function to be called when a new connection is
 * made. Only one funcation can be set; subsequent calls will overwrite
 * previous ones. <handler> will *not* be called for connections users
 * create themselves (using nxConnect()). */
void nxOnConnect(NX *nx, int (*handler)(NX_Conn *conn));

/* Set <handler> as the function to be called when a connection is
 * dropped. Only one funcation can be set; subsequent calls will
 * overwrite previous ones. <handler> will *not* be called for
 * connections users drop themselves (using nxDisconnect()). */
void nxOnDisconnect(NX *nx, int (*handler)(NX_Conn *conn));

/* Set <handler> as the function to be called when new data comes in.
 * Only one function can be set; subsequent calls will overwrite
 * previous ones. */
void nxOnData(NX *nx, int (*handler)(NX_Conn *conn));

/* Set <handler> as the function to be called when an error occurs. The
 * connection on which it occurred and the errno code will be passed to
 * the handler. */
void nxOnError(NX *nx, int (*handler)(NX_Conn *conn, int err));

/* Return the current *UTC* time (number of seconds since
 * 1970-01-01/00:00:00 UTC) as a double. */
double nxNow(void);

/* Add a timeout at UTC time t, calling <handler> with <nx>, time <t>
 * and <udata>. */
void nxAddTimeout(NX *nx, double t, void *udata, int (*handler)(NX *nx, double t,
                    void *udata));

/* Return the NX for <conn>. */
NX *nxFor(NX_Conn *conn);

/* Run the Network Exchange. New connection requests from external
 * parties will be accepted automatically, calling the on_connect
 * handler. On errors and end-of-file conditions connections will
 * automatically be closed, calling the on_error and on_disconnect
 * handlers. This function will return either when the select inside
 * fails (returning errno) or when there are no more connections and
 * timeouts left. */
int nxRun(NX *nx);

#endif
