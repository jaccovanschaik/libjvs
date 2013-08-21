#ifndef MX_H
#define MX_H

/*
 * mx.h: Message Exchange.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: mx.h 170 2013-08-15 18:58:58Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

typedef struct MX MX;

/*
 * Create a Message Exchange.
 */
MX *mxCreate(void);

/*
 * Tell <mx> to listen on address <host> and <port> for new connection requests, and return the file
 * descriptor for the new listening socket. If <host> is NULL, <mx> will listen on all interfaces.
 * If <port> is 0, <mx> will select a random port to listen on. You can find out which port was
 * picked by passing the returned file descriptor to netLocalPort().
 */
int mxListen(MX *mx, const char *host, int port);

/*
 * Tell <mx> to call <cb> when a message of type <type> arrives. The file descriptor on which the
 * message was received is passed to <cb>, along with the type, version and payload contents and
 * size of the received message. Also passed to <cb> is the user data pointer <udata>. Only one
 * callback can be installed for each message type; subsequent calls will replace the installed
 * callback with <cb>.
 */
void mxOnMessage(MX *mx, int type, void (*cb)(MX *mx, int fd, int type, int version, char *payload,
                   int size, void *udata), void *udata);

/*
 * Tell <mx> to stop listening for messages with type <type>.
 */
void mxDropMessage(MX *mx, int type);

/*
 * Tell <mx> to call <cb> if data comes in on file descriptor <fd>. The file descriptor where the
 * data was received is passed to <cb>, in addition to user data pointer <udata>. Only one
 * callback can be installed for each message type; subsequent calls will replace the installed
 * callback with <cb>.
 */
void mxOnData(MX *mx, int fd, void (*cb)(MX *mx, int fd, void *udata), void *udata);

/*
 * Tell <mx> to stop listening for data on <fd>.
 */
void mxDropData(MX *mx, int fd);

/*
 * Tell <mx> to call <cb> at time <t> (a timestamp in seconds since 00:00:00 UTC on 1970-01-01). The
 * <t> and <udata> that were given are passed back to <cb>.
 */
void mxOnTime(MX *mx, double t, void (*cb)(MX *mx, double t, void *udata), void *udata);

/*
 * Tell <mx> to cancel the timeout at <t>, for which callback <cb> was installed.
 */
void mxDropTime(MX *mx, double t, void (*cb)(MX *mx, double t, void *udata));

/*
 * Tell <mx> to make a new connection to <host> on port <port>. The file descriptor of the new
 * connection is returned.
 */
int mxConnect(MX *mx, const char *host, int port);

/*
 * Tell <mx> to drop the connection on <fd>.
 */
int mxDisconnect(MX *mx, int fd);

/*
 * Send a message with type <type>, version <version> and payload <payload> with size <size> via
 * <mx> to <fd>. The message is added to an outgoing buffer in <mx>, and will be sent as soon as the
 * flow of control returns to <mx>'s main loop.
 */
int mxSend(MX *mx, int fd, int type, int version, const char *payload, int size);

/*
 * Send a message with type <type> and version <version> via <mx> to <fd>. The message payload is
 * packed using the PACK_* syntax described in utils.h. The message is added to an outgoing buffer
 * in <mx>, and will be sent as soon as the flow of control returns to <mx>'s main loop.
 */
int mxPack(MX *mx, int fd, int type, int version, ...);

/*
 * Send a message with type <type> and version <version> via <mx> to <fd>. The message payload is
 * packed using the PACK_* syntax described in utils.h. The message is added to an outgoing buffer
 * in <mx>, and will be sent as soon as the flow of control returns to <mx>'s main loop.
 */
int mxVaPack(MX *mx, int fd, int type, int version, va_list ap);

/*
 * Call <cb> when a new connection is accepted by <mx>. The file descriptor of the new connection is
 * passed to <cb>, along with the user data pointer <udata>. This function is *not* called for
 * connections made with mxConnect().
 */
void mxOnConnect(MX *mx, void (*cb)(MX *mx, int fd, void *udata), void *udata);

/*
 * Call <cb> when a connection is lost by <mx>. The file descriptor of the lost connection is
 * passed to <cb>, along with the user data pointer <udata>. This function is *not* called for
 * connections dropped using mxDisconnect().
 */
void mxOnDisconnect(MX *mx, void (*cb)(MX *mx, int fd, void *udata), void *udata);

/*
 * Tell <mx> to call <cb> when an error occurs on the connection using file descriptor <fd>. The
 * error code is passed to <cb>, along with the user data pointer <udata>.
 */
void mxOnError(MX *mx, void (*cb)(MX *mx, int fd, int error, void *udata), void *udata);

/*
 * Tell <mx> to wait until a message of type <type> arrives on file descriptor <fd>. The version of
 * the received message is passed to <cb>, along with its payload and payload size. Messages,
 * timeouts and other received data that arrives while waiting for this message will be queued up
 * and delivered as soon as the flow-of-control returns to <mx>'s main loop.
 */
int mxAwait(MX *mx, int fd, int type, int *version, char **payload, int *size, double timeout);

/*
 * Start <mx>'s main loop. This function won't return until all sockets and other file descriptors
 * have been closed and there are no more pending timeouts.
 */
int mxRun(MX *mx);

/*
 * Close down <mx>. This will close all sockets and other file descriptors and cancel all timeouts,
 * causing mxRun() to return.
 */
int mxClose(MX *mx);

/*
 * Destroy <mx>. Only call this function when mxRun() has returned.
 */
int mxDestroy(MX *mx);

#endif
