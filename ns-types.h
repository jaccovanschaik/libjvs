#ifndef NS_TYPES_H
#define NS_TYPES_H

/*
 * ns-types.h: types for ns.c
 *
 * ns-types.h is part of libjvs.
 *
 * Copyright:   (c) 2013-2021 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: ns-types.h 438 2021-08-19 10:10:03Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "buffer.h"
#include "pa.h"

#include "dis.h"
#include "dis-types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    Buffer incoming;
} NS_Connection;

struct NS {
    Dispatcher dis;     /* Must be the first element in this struct. */

    PointerArray connections;

    void (*on_connect_cb)(NS *ns, int fd, void *udata);
    void *on_connect_udata;

    void (*on_disconnect_cb)(NS *ns, int fd, void *udata);
    void *on_disconnect_udata;

    void (*on_error_cb)(NS *ns, int fd, int error, void *udata);
    void *on_error_udata;

    void (*on_socket_cb)(NS *ns, int fd, const char *buffer, int size,
            void *udata);
    void *on_socket_udata;
};

#ifdef __cplusplus
}
#endif

#endif
