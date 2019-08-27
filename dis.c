/*
 * dis.c: I/O Dispatcher.
 *
 * dis.c is part of libjvs.
 *
 * Copyright:   (c) 2013-2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: dis.c 343 2019-08-27 08:39:24Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <sys/select.h>

#include "list.h"
#include "debug.h"
#include "utils.h"

#include "dis.h"
#include "dis-types.h"

/*
 * Create a new dispatcher.
 */
Dispatcher *disCreate(void)
{
    return calloc(1, sizeof(Dispatcher));
}

/*
 * Initialize dispatcher <dis>.
 */
void disInit(Dispatcher *dis)
{
    memset(dis, 0, sizeof(Dispatcher));
}

/*
 * Arrange for <cb> to be called when there is data available on file descriptor <fd>. <cb> will be
 * called with the given <dis>, <fd> and <udata>, which is a pointer to "user data" that will be
 * returned <cb> as it was given here, and that will not be accessed by dis in any way.
 */
void disOnData(Dispatcher *dis, int fd,
        void (*cb)(Dispatcher *dis, int fd, void *udata), const void *udata)
{
    DIS_File *file;

    dbgAssert(stderr, fd >= 0, "bad file descriptor: %d\n", fd);

    P dbgPrint(stderr, "Adding file on fd %d\n", fd);

    if ((file = paGet(&dis->files, fd)) == NULL) {
        file = calloc(1, sizeof(DIS_File));

        paSet(&dis->files, fd, file);
    }

    file->cb = cb;
    file->udata = udata;
}

/*
 * Drop the subscription on file descriptor <fd>.
 */
void disDropData(Dispatcher *dis, int fd)
{
    DIS_File *file = paGet(&dis->files, fd);

    P dbgPrint(stderr, "Dropping fd %d\n", fd);

    dbgAssert(stderr, file != NULL, "unknown file descriptor: %d\n", fd);

    paDrop(&dis->files, fd);
}

/*
 * Write the first <size> bytes of <data> to <fd>, for which the disOnData function must have been
 * called previously. You may write to <fd> via any other means, but if you use this function the
 * data will be written out, possibly piece by piece but always without blocking, when the given
 * file descriptor becomes writable.
 */
void disWrite(Dispatcher *dis, int fd, const char *data, size_t size)
{
    DIS_File *file = paGet(&dis->files, fd);

    dbgAssert(stderr, file != NULL, "unknown file descriptor: %d\n", fd);

    bufAdd(&file->outgoing, data, size);
}

/*
 * Pack the arguments following <fd> into a string according to the strpack interface in utils.h and
 * send it via <dis> to <fd>.
 */
void disPack(Dispatcher *dis, int fd, ...)
{
    va_list ap;

    va_start(ap, fd);
    disVaPack(dis, fd, ap);
    va_end(ap);
}

/*
 * Pack the arguments contained in ap into a string according to the vstrpack interface in utils.h
 * and send it via <dis> to <fd>.
 */
void disVaPack(Dispatcher *dis, int fd, va_list ap)
{
    char *str;

    int n = vastrpack(&str, ap);

    disWrite(dis, fd, str, n);

    free(str);
}

/*
 * Arrange for <cb> to be called at time <t>, which is the (double precision floating point) number
 * of seconds since 00:00:00 UTC on 1970-01-01 (aka. the UNIX epoch). <cb> will be called with the
 * given <dis>, <t> and <udata>. You can get the current time using dnow() from utils.c.
 */
void disOnTime(Dispatcher *dis, double t, void (*cb)(Dispatcher *dis, double t, void *udata), const void *udata)
{
    DIS_Timer *next_timer, *new_timer = calloc(1, sizeof(DIS_Timer));

    new_timer->t = t;
    new_timer->cb = cb;
    new_timer->udata = udata;

    for (next_timer = listHead(&dis->timers); next_timer; next_timer = listNext(next_timer)) {
        if (next_timer->t > new_timer->t) break;
    }

    listInsert(&dis->timers, new_timer, next_timer);
}

/*
 * Cancel the timer that was set for time <t> with callback <cb>.
 */
void disDropTime(Dispatcher *dis, double t, void (*cb)(Dispatcher *dis, double t, void *udata))
{
    DIS_Timer *timer;

    for (timer = listHead(&dis->timers); timer; timer = listNext(timer)) {
        if (timer->t == t && timer->cb == cb) {
            listRemove(&dis->timers, timer);
            free(timer);
            return;
        }
    }

    dbgAbort(stderr, "no such timer\n");
}

/*
 * Return the number of file descriptors that <dis> is monitoring (i.e. max_fd - 1).
 */
int disFdCount(Dispatcher *dis)
{
    return paCount(&dis->files);
}

/*
 * Return TRUE if <fd> has been given to <dis> using disOnData(), FALSE otherwise.
 */
int disOwnsFd(Dispatcher *dis, int fd)
{
    return (paGet(&dis->files, fd) != NULL);
}

/*
 * Prepare a call to select() based on the files and timeouts set in <dis>. The necessary parameters
 * to select() are returned through <nfds>, <rfds>, <wfds> and <tv> (exception-fds should be set to
 * NULL). <*tv> is set to point to an appropriate timeout value, or NULL if no timeout is to be set.
 * This function will clear <rfds> and <wfds>, so if callers want to add their own file descriptors,
 * they should do so after calling this function. This function returns -1 if the first timeout
 * should already have occurred, otherwise 0.
 */
int disPrepareSelect(Dispatcher *dis, int *nfds, fd_set *rfds, fd_set *wfds, struct timeval **tv)
{
    int fd;
    double delta_t;
    DIS_Timer *timer;

    *nfds = paCount(&dis->files);

    FD_ZERO(rfds);
    FD_ZERO(wfds);

    P dbgPrint(stderr, "File descriptors:");

    for (fd = 0; fd < *nfds; fd++) {
        DIS_File *file = paGet(&dis->files, fd);

        if (file == NULL) continue;

        P fprintf(stderr, " %d", fd);

        FD_SET(fd, rfds);

        if (bufLen(&file->outgoing) > 0) FD_SET(fd, wfds);

        P {
            fprintf(stderr, " (%s%s)",
                    FD_ISSET(fd, rfds) ? "r" : "",
                    FD_ISSET(fd, wfds) ? "w" : "");
        }
    }

    P fprintf(stderr, "\n");

    P dbgPrint(stderr, "%d pending timers:\n", listLength(&dis->timers));

    P for (timer = listHead(&dis->timers); timer; timer = listNext(timer)) {
        fprintf(stderr, "\t%f seconds\n", timer->t - dnow());
    }

    if ((timer = listHead(&dis->timers)) == NULL) {
        *tv = NULL;
    }
    else if ((delta_t = timer->t - dnow()) < 0) {
#if 0
        P dbgPrint(stderr, "First timer %f seconds ago, return -1\n", -delta_t);

        return -1;
#endif
        dis->tv.tv_sec = 0;
        dis->tv.tv_usec = 0;

        *tv = &dis->tv;
    }
    else {
        P dbgPrint(stderr, "First timer in %f seconds.\n", delta_t);

        dis->tv.tv_sec = (int) delta_t;
        dis->tv.tv_usec = 1000000 * fmod(delta_t, 1.0);

        *tv = &dis->tv;
    }

    return 0;
}

/*
 * Process (and subsequently discard) the first pending timeout.
 */
void disHandleTimer(Dispatcher *dis)
{
    DIS_Timer *timer = listRemoveHead(&dis->timers);

    dbgAssert(stderr, timer != NULL, "no pending timer.\n");

    timer->cb(dis, timer->t, (void *) timer->udata);

    free(timer);
}

/*
 * Handle readable and writable file descriptors in <rfds> and <wfds>, with <nfds> set to the
 * maximum number of file descriptors that may be set in <rfds> or <wfds>.
 */
void disHandleFiles(Dispatcher *dis, int nfds, fd_set *rfds, fd_set *wfds)
{
    int fd;

    for (fd = 0; fd < nfds; fd++) {
        if (disOwnsFd(dis, fd) && FD_ISSET(fd, rfds)) disHandleReadable(dis, fd);
        if (disOwnsFd(dis, fd) && FD_ISSET(fd, wfds)) disHandleWritable(dis, fd);
    }
}

/*
 * Handle readable file descriptor <fd>.
 */
void disHandleReadable(Dispatcher *dis, int fd)
{
    DIS_File *file = paGet(&dis->files, fd);

    dbgAssert(stderr, file != NULL, "unknown file descriptor: %d\n", fd);

    file->cb(dis, fd, (void *) file->udata);
}

/*
 * Handle writable file descriptor <fd>.
 */
void disHandleWritable(Dispatcher *dis, int fd)
{
    int r;

    DIS_File *file = paGet(&dis->files, fd);

    dbgAssert(stderr, file != NULL, "unknown file descriptor: %d\n", fd);

    r = write(fd, bufGet(&file->outgoing), bufLen(&file->outgoing));

    if (r > 0) {
        bufTrim(&file->outgoing, r, 0);
    }
}

/*
 * Process the results of a call to select(). <r> is select's return value, <rfds> and <wfds>
 * contain the file descriptors that select has marked as readable or writable and <nfds> is the
 * maximum number of file descriptors that may be set in <rfds> or <wfds>.
 */
void disProcessSelect(Dispatcher *dis, int r, int nfds, fd_set *rfds, fd_set *wfds)
{
    P dbgPrint(stderr, "r = %d, nfds = %d.\n", r, nfds);

    P dumpfds(stderr, "\trfds:", nfds, rfds);
    P dumpfds(stderr, "\twfds:", nfds, wfds);

    if (r == 0) {
        P dbgPrint(stderr, "Timeout, calling disHandleTimer.\n");
        disHandleTimer(dis);
    }
    else if (r > 0) {
        P dbgPrint(stderr, "Data available, calling disHandleFiles.\n");
        disHandleFiles(dis, nfds, rfds, wfds);
    }
}

/*
 * Wait for file or timer events and handle them. This function returns 1 if there are no files or
 * timers to wait for, -1 if some error occurred, or 0 if any number of events was successfully
 * handled.
 */
int disHandleEvents(Dispatcher *dis)
{
    int nfds, r;
    fd_set rfds, wfds;
    struct timeval *tv;

    P dbgPrint(stderr, "Calling disPrepareSelect.\n");

    r = disPrepareSelect(dis, &nfds, &rfds, &wfds, &tv);

    P dbgPrint(stderr, "disPrepareSelect returned:\n\tr:    %d\n\tnfds: %d\n", r, nfds);

    if (r < 0) {
        /* Fake a timeout. */

        disProcessSelect(dis, 0, 0, NULL, NULL);

        return 0;
    }
    else if (nfds > 0) {
        P dumpfds(stderr, "\trfds:", nfds, &rfds);
        P dumpfds(stderr, "\twfds:", nfds, &wfds);
    }

    P {
        if (tv == NULL) {
            fprintf(stderr, "\ttv:   NULL\n");
        }
        else {
            fprintf(stderr, "\ttv:   %ld.%06ld\n", tv->tv_sec, tv->tv_usec);
        }
    }

    if (nfds == 0 && tv == NULL) {
        P dbgPrint(stderr, "No more files, no more timeouts: return 1.\n");
        return 1;
    }

    P dbgPrint(stderr, "Calling select.\n");

    r = select(nfds, &rfds, &wfds, NULL, tv);

    P dbgPrint(stderr, "select returned %d\n", r);

    if (r < 0) {
        return r;
    }
    else if (r > 0) {
        P dumpfds(stderr, "\trfds:", nfds, &rfds);
        P dumpfds(stderr, "\twfds:", nfds, &wfds);
    }

    P dbgPrint(stderr, "Calling disProcessSelect.\n");

    disProcessSelect(dis, r, nfds, &rfds, &wfds);

    return 0;
}

/*
 * Run dispatcher <dis>. Returns 0 if there were no more timers or files to wait for, or -1 in case
 * of an error.
 */
int disRun(Dispatcher *dis)
{
    int r;

    do {
        r = disHandleEvents(dis);
        P fprintf(stderr, "disHandleEvents returned %d\n", r);
    } while (r == 0);

    return r == 1 ? 0 : r;
}

/*
 * Close dispatcher <dis>. This removes all file descriptors and timers, which will
 * cause disRun() to return.
 */
void disClose(Dispatcher *dis)
{
    int fd;
    DIS_Timer *timer;

    for (fd = 0; fd < paCount(&dis->files); fd++) {
        DIS_File *file = paGet(&dis->files, fd);

        if (file != NULL) {
            paDrop(&dis->files, fd);

            bufReset(&file->outgoing);

            free(file);
        }
    }

    while ((timer = listRemoveHead(&dis->timers)) != NULL) {
        free(timer);
    }
}

/*
 * Clear the contents of <dis> but don't free <dis> itself.
 */
void disClear(Dispatcher *dis)
{
    disClose(dis);

    memset(dis, 0, sizeof(Dispatcher));
}

/*
 * Clear the contents of <dis> and then free <dis> itself. Do not call this from inside the disRun()
 * loop. Instead, call disClose(), wait for disRun() to return and then call disDestroy().
 */
void disDestroy(Dispatcher *dis)
{
    disClose(dis);  /* Just to be sure. */

    free(dis);
}

#ifdef TEST

static int errors = 0;

static int fd[2];

static void handle_fd0(Dispatcher *dis, int fd, void *udata)
{
    char buffer[16];

    int count = read(fd, buffer, sizeof(buffer));

    make_sure_that(count == 4);
    make_sure_that(strncmp(buffer, "Hoi!", 4) == 0);

    disClose(dis);
}

static void handle_timeout(Dispatcher *dis, double t, void *udata)
{
    if (write(fd[1], "Hoi!", 4) == -1) {
        perror("write");
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    int i;

    Dispatcher *dis = disCreate();

    if (pipe(fd) == -1) {
        perror("pipe");
        exit(1);
    }

    disOnData(dis, fd[0], handle_fd0, NULL);
    disOnTime(dis, dnow() + 0.1, handle_timeout, NULL);

    for (i = 0; i < fd[0]; i++) {
        make_sure_that(!disOwnsFd(dis, i));
    }

    make_sure_that(disOwnsFd(dis, fd[0]));

    disRun(dis);

    return errors;
}
#endif
