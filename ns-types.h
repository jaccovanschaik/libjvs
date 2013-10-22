#ifndef NS_TYPES_H
#define NS_TYPES_H

/*
 * Network Server.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: ns-types.h 230 2013-09-30 18:29:53Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "buffer.h"
#include "pa.h"

#include "dis.h"
#include "dis-types.h"

typedef struct {
    Buffer incoming;
} NS_Connection;

struct NS {
    Dispatcher dis;     /* Must be the first element in this struct. */

    PointerArray connections;

    int listen_fd;      /* File descriptor for our listen socket. */
    int listen_port;    /* The number of our listen port. */
    char *listen_host;  /* The host name of our listen port. */

    void (*on_connect_cb)(NS *ns, int fd, void *udata);
    void *on_connect_udata;

    void (*on_disconnect_cb)(NS *ns, int fd, void *udata);
    void *on_disconnect_udata;

    void (*on_error_cb)(NS *ns, int fd, int error, void *udata);
    void *on_error_udata;

    void (*on_socket_cb)(NS *ns, int fd, const char *buffer, int size, void *udata);
    void *on_socket_udata;
};

#endif
