#ifndef DIS_H
#define DIS_H

/*
 * dis.h: I/O Dispatcher.
 *
 * dis.h is part of libjvs.
 *
 * Copyright:   (c) 2013-2021 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: dis.h 438 2021-08-19 10:10:03Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stddef.h>
#include <sys/time.h>

typedef struct Dispatcher Dispatcher;

/*
 * Create a new dispatcher.
 */
Dispatcher *disCreate(void);

/*
 * Initialize dispatcher <dis>.
 */
void disInit(Dispatcher *dis);

/*
 * Arrange for <cb> to be called when there is data available on file
 * descriptor <fd>. <cb> will be called with the given <dis>, <fd> and
 * <udata>, which is a pointer to "user data" that will be returned <cb> as it
 * was given here, and that will not be accessed by dis in any way.
 */
void disOnData(Dispatcher *dis, int fd,
        void (*cb)(Dispatcher *dis, int fd, void *udata), const void *udata);

/*
 * Drop the subscription on file descriptor <fd>.
 */
void disDropData(Dispatcher *dis, int fd);

/*
 * Write the first <size> bytes of <data> to <fd>, for which the disOnData
 * function must have been called previously. You may write to <fd> via any
 * other means, but if you use this function the data will be written out,
 * possibly piece by piece but always without blocking, when the given file
 * descriptor becomes writable.
 */
void disWrite(Dispatcher *dis, int fd, const char *data, size_t size);

/*
 * Pack the arguments following <fd> into a string according to the strpack
 * interface in utils.h and send it via <dis> to <fd>.
 */
void disPack(Dispatcher *dis, int fd, ...);

/*
 * Pack the arguments contained in ap into a string according to the vstrpack
 * interface in utils.h and send it via <dis> to <fd>.
 */
void disVaPack(Dispatcher *dis, int fd, va_list ap);

/*
 * Arrange for <cb> to be called at time <t>, which is the (double precision
 * floating point) number of seconds since 00:00:00 UTC on 1970-01-01 (aka.
 * the UNIX epoch). <cb> will be called with the given <dis>, <t> and <udata>.
 * You can get the current time using dnow() from utils.c.
 */
void disOnTime(Dispatcher *dis, double t, void (*cb)(Dispatcher *dis, double t, void *udata), const void *udata);

/*
 * Cancel the timer that was set for time <t> with callback <cb>.
 */
void disDropTime(Dispatcher *dis, double t,
        void (*cb)(Dispatcher *dis, double t, void *udata));

/*
 * Return the number of file descriptors that <dis> is monitoring
 * (i.e. max_fd - 1).
 */
int disFdCount(Dispatcher *dis);

/*
 * Return TRUE if <fd> has been given to <dis> using disOnData(), FALSE
 * otherwise.
 */
int disOwnsFd(Dispatcher *dis, int fd);

/*
 * Prepare a call to select() based on the files and timeouts set in <dis>.
 * The necessary parameters to select() are returned through <nfds>, <rfds>,
 * <wfds> and <tv> (exception-fds should be set to NULL). <*tv> is set to
 * point to an appropriate timeout value, or NULL if no timeout is to be set.
 * This function will clear <rfds> and <wfds>, so if callers want to add their
 * own file descriptors, they should do so after calling this function. This
 * function returns -1 if the first timeout should already have occurred,
 * otherwise 0.
 */
int disPrepareSelect(Dispatcher *dis, int *nfds, fd_set *rfds, fd_set *wfds,
        struct timeval **tv);

/*
 * Process (and subsequently discard) the first pending timeout.
 */
void disHandleTimer(Dispatcher *dis);

/*
 * Handle readable and writable file descriptors in <rfds> and <wfds>, with
 * <nfds> set to the maximum number of file descriptors that may be set in
 * <rfds> or <wfds>.
 */
void disHandleFiles(Dispatcher *dis, int nfds, fd_set *rfds, fd_set *wfds);

/*
 * Handle readable file descriptor <fd>.
 */
void disHandleReadable(Dispatcher *dis, int fd);

/*
 * Handle writable file descriptor <fd>.
 */
void disHandleWritable(Dispatcher *dis, int fd);

/*
 * Process the results of a call to select(). <r> is select's return value,
 * <rfds> and <wfds> contain the file descriptors that select has marked as
 * readable or writable and <nfds> is the maximum number of file descriptors
 * that may be set in <rfds> or <wfds>.
 */
void disProcessSelect(Dispatcher *dis, int r, int nfds,
        fd_set *rfds, fd_set *wfds);

/*
 * Wait for file or timer events and handle them. This function returns 1 if
 * there are no files or timers to wait for, -1 if some error occurred, or 0
 * if any number of events was successfully handled.
 */
int disHandleEvents(Dispatcher *dis);

/*
 * Run dispatcher <dis>. Returns 0 if there were no more timers or files to
 * wait for, or -1 in case of an error.
 */
int disRun(Dispatcher *dis);

/*
 * Close dispatcher <dis>. This removes all file descriptors and timers, which
 * will cause disRun() to return.
 */
void disClose(Dispatcher *dis);

/*
 * Clear the contents of <dis> but don't free <dis> itself.
 */
void disClear(Dispatcher *dis);

/*
 * Clear the contents of <dis> and then free <dis> itself. Do not call this
 * from inside the disRun() loop. Instead, call disClose(), wait for disRun()
 * to return and then call disDestroy().
 */
void disDestroy(Dispatcher *dis);

#ifdef __cplusplus
}
#endif

#endif
