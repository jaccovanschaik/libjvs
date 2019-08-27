#ifndef LIBJVS_DIS_TYPES_H
#define LIBJVS_DIS_TYPES_H

/*
 * dis-types.h: Type definitions for DIS.
 *
 * dis-types.h is part of libjvs.
 *
 * Copyright:   (c) 2013-2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: dis-types.h 343 2019-08-27 08:39:24Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "buffer.h"
#include "list.h"
#include "pa.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif
