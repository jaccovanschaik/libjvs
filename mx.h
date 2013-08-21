#ifndef MX_H
#define MX_H

/*
 * mx.h: Message Exchange.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: mx.h 174 2013-08-20 14:05:45Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdint.h>

typedef struct MX MX;

typedef uint32_t MX_Size;
typedef uint32_t MX_Type;
typedef uint32_t MX_Version;

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
 * Tell <mx> to call <cb> when a message of message type <type> arrives. The file descriptor on
 * which the message was received is passed to <cb>, along with the message type, version and
 * payload contents and size of the received message. Also passed to <cb> is the user data pointer
 * <udata>. Only one callback can be installed for each message type; subsequent calls will replace
 * the installed callback with <cb>.
 */
void mxOnMessage(MX *mx, MX_Type type, void (*cb)(MX *mx, int fd, MX_Type type, MX_Version version,
                   char *payload, MX_Size size, void *udata), void *udata);

/*
 * Tell <mx> to stop listening for messages with message type <type>.
 */
void mxDropMessage(MX *mx, MX_Type type);

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
void mxDisconnect(MX *mx, int fd);

/*
 * Send a message with message type <type>, version <version> and payload <payload> with size
 * <size> via <mx> to <fd>. The message is added to an outgoing buffer in <mx>, and will be sent as
 * soon as the flow of control returns to <mx>'s main loop.
 */
int mxSend(MX *mx, int fd, MX_Type type, MX_Version version, const char *payload, MX_Size size);

/*
 * Send a message with message type <type> and version <version> via <mx> to <fd>. The message
 * payload is packed using the PACK_* syntax described in utils.h. The message is added to an
 * outgoing buffer in <mx>, and will be sent as soon as the flow of control returns to <mx>'s main
 * loop.
 */
int mxPack(MX *mx, int fd, MX_Type type, MX_Version version, ...);

/*
 * Send a message with message type <type> and version <version> via <mx> to <fd>. The message
 * payload is packed using the PACK_* syntax described in utils.h. The message is added to an
 * outgoing buffer in <mx>, and will be sent as soon as the flow of control returns to <mx>'s main
 * loop.
 */
int mxVaPack(MX *mx, int fd, MX_Type type, MX_Version version, va_list ap);

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
void mxOnDisconnect(MX *mx, void (*cb)(MX *mx, int fd, const char *whence, void *udata), void
                      *udata);

/*
 * Tell <mx> to call <cb> when an error occurs on the connection using file descriptor <fd>. The
 * error code is passed to <cb>, along with the user data pointer <udata>.
 */
void mxOnError(MX *mx, void (*cb)(MX *mx, int fd, const char *whence, int error, void *udata), void
                 *udata);

/*
 * Tell <mx> to wait until a message of message type <type> arrives on file descriptor <fd>. The
 * version of the received message is returned through <version>, its payload through <payload> and
 * the payload size though <size>. Messages, timers and other received data that arrive while
 * waiting for this message will be eventsd up and delivered as soon as the flow-of-control returns
 * to <mx>'s main loop. This function will wait until the time given in <timeout> (a UTC timestamp
 * containing the number of seconds since 1970-01-01/00:00:00 UTC) for the message to arrive. It
 * returns 0 if the message did arrive on time, 1 if it didn't and -1 if any other (network) error
 * occurred.
 */
int mxAwait(MX *mx, int fd, int type, int *version, char **payload, int *size, double timeout);

/*
 * Start <mx>'s main loop. This function won't return until all sockets and other file descriptors
 * have been closed and there are no more pending timers.
 */
int mxRun(MX *mx);

/*
 * Close down <mx>. This will close all sockets and other file descriptors and cancel all timers,
 * causing mxRun() to return.
 */
void mxClose(MX *mx);

/*
 * Destroy <mx>. Only call this function when mxRun() has returned.
 */
void mxDestroy(MX *mx);

#endif
