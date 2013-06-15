#ifndef DX_H
#define DX_H

/*
 * cx.h: Communications Exchange.
 *
 * Copyright:	(c) 2012 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

typedef struct CX CX;

/*
 * Create a communications exchange.
 */
CX *cxCreate(void);

/*
 * Subscribe to input. When data is available on <fd>, <handler> will be called with the given <cx>,
 * <fd> and <udata>. Only one handler per file descriptor can be set, subsequent calls will override
 * earlier ones.
 */
void cxAddFile(CX *cx, int fd, int (*handler)(CX *cx, int fd, void *udata), void
                 *udata);

/*
 * Drop subscription to fd <fd>.
 */
void cxDropFile(CX *cx, int fd);

/*
 * Return the current UTC time (number of seconds since 1970-01-01/00:00:00 UTC) as a double.
 */
double cxNow(void);

/*
 * Set a handler to be called at time <t> (in seconds since 1970-01-01/00:00:00 UTC). <handler> will
 * be called with the given <cx>, <t> and <udata>.
 */
void cxAddTime(CX *cx, double t, int (*handler)(CX *cx, double t, void *udata),
                 void *udata);

/*
 * Drop timeout at time <t>. Both <t> and <handler> must match the earlier call to cxAddTime.
 */
void cxDropTime(CX *cx, double t, int (*handler)(CX *cx, double t, void *udata));

/*
 * Fill <rfds> with file descriptors that may have input data, and return the number of file
 * descriptors that may be set.
 */
int cxFillFDs(CX *cx, fd_set *rfds);

/*
 * Return TRUE if <fd> is handled by <cx>.
 */
int cxOwnsFD(CX *cx, int fd);

/*
 * Prepare a call to select() for <cx>. <rfds> is filled with file descriptors that may have input
 * data and <tv> is set to a pointer to a struct timeval to be used as a timeout (which may be NULL
 * if no timeouts are pending). The number of file descriptors that may be set is returned.
 */
int cxPrepareSelect(CX *cx, fd_set *rfds, struct timeval **tv);

/*
 * Process the results from a select() call.
 */
int cxProcessSelect(CX *cx, int r, fd_set *rfds);

/*
 * Run the communications exchange. This function will return when there are no more timeouts to
 * wait for and no file descriptors to listen on (which can be forced by calling cxClose()).
 */
int cxRun(CX *cx);

/*
 * Close down Communications Exchange <cx>. This forcibly stops <cx> from listening on any file
 * descriptor and removes all pending timeouts, which causes cxRun() to return.
 */
int cxClose(CX *cx);

/*
 * Free the memory occupied by <cx>. Call this outside of the cx loop, i.e. after cxRun() returns.
 * You can force cxRun() to return by calling cxClose().
 */
int cxFree(CX *cx);

#endif
