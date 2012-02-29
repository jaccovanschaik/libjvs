#ifndef DX_H
#define DX_H

/*
 * dx.h: Description
 *
 * Copyright:	(c) 2012 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */


typedef struct CX CX;

/* Create a communications exchange. */
CX *cxCreate(void);

/* Add file descriptor <fd>. */
void cxAddFd(CX *cx, int fd);

/* Drop file descriptor <fd>. */
void cxDropFd(CX *cx, int fd);

/* Subscribe to input from file descriptor <fd>. */
void cxSubscribe(CX *cx, int fd, void *udata, int (*handler)(CX *cx, int fd, void *udata));

/* Queue <len> bytes from <data> to be sent out over <fd>. Returns the
 * number of bytes that were queued, which is usually <len>, except on
 * error when it is 0. Call cxError() to get the associated errno. */
int cxQueue(CX *cx, int fd, const char *data, int len);

/* Get <len> bytes from the incoming buffer for <fd> and put them in
 * <data> (which should, obviously, be long enough to hold them all.
 * Might return less than <len> (even 0) if fewer bytes are in the
 * buffer. Call cxError() to find out * what happened. */
int cxGet(CX *cx, int fd, char *data, int len);

/* Returns the errno for the latest error that occurred. Will be 0 for a
 * simple end-of-file. */
int cxError(CX *cx, int fd);

/* Trim the first <len> bytes from the input buffer for <fd>, to be
 * called when you've handled them. */
void cxClear(CX *cx, int fd, int len);

/* Add a timeout at time <t>. */
void cxAddTimeout(CX *cx, double t, void *udata, int (*handler)(CX *cx, double t,
                    void *udata));

/* Run the communications exchange. */
int cxRun(CX *cx);

#endif
