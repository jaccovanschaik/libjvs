#ifndef LIBJVS_DIS_TYPES_H
#define LIBJVS_DIS_TYPES_H

/*
 * dis-types.h: Type definitions for DIS.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: dis-types.h 230 2013-09-30 18:29:53Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "buffer.h"
#include "list.h"
#include "pa.h"

typedef struct {
    Buffer outgoing;
    void (*cb)(Dispatcher *dis, int fd, void *udata);
    const void *udata;
} DIS_File;

typedef struct {
    ListNode _node;
    double t;
    void (*cb)(Dispatcher *dis, double t, void *udata);
    const void *udata;
} DIS_Timer;

struct Dispatcher {
    PointerArray files;
    List timers;
    struct timeval tv;
};

#endif
