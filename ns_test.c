#include "ns.c"
#include "utils.h"

int errors = 0;

static void on_time(NS *ns, double t, void *udata)
{
    uint16_t port = *((uint16_t *) udata);
    static int fd, step = 0;

    P dbgPrint(stderr, "port = %d\n", port);
    P dbgPrint(stderr, "step = %d\n", step);

    switch(step) {
    case 0:
        fd = nsConnect(ns, "localhost", port);
        break;
    case 1:
        nsPack(ns, fd, PACK_STRING, "Hoi!", END);
        break;
    default:
        nsClose(ns);
        return;
    }

    step++;

    nsOnTime(ns, t + 0.1, on_time, udata);
}

static void tester(uint16_t port)
{
    int r;

    NS *ns = nsCreate();

    nsOnTime(ns, dnow() + 0.1, on_time, &port);

    r = nsRun(ns);

    P dbgPrint(stderr, "nsRun returned %d\n", r);

    nsDestroy(ns);
}

static void on_connect(NS *ns, int fd, void *udata)
{
    UNUSED(ns);
    UNUSED(udata);

    P dbgPrint(stderr, "fd = %d\n", fd);
}

static void on_disconnect(NS *ns, int fd, void *udata)
{
    UNUSED(udata);

    P dbgPrint(stderr, "fd = %d\n", fd);

    nsClose(ns);
}

static void on_socket(NS *ns, int fd, const char *data, int size, void *udata)
{
    UNUSED(udata);

    int n;
    char *str;

    make_sure_that(data == nsIncoming(ns, fd));
    make_sure_that(size == nsAvailable(ns, fd));

    n = strunpack(data, size,
            PACK_STRING, &str,
            END);

    nsDiscard(ns, fd, n);

    P dbgPrint(stderr, "fd = %d, str = \"%s\", n = %d\n", fd, str, n);
}

static void testee(NS *ns)
{
    int r;

    nsOnConnect(ns, on_connect, NULL);
    nsOnDisconnect(ns, on_disconnect, NULL);
    nsOnSocket(ns, on_socket, NULL);

    r = nsRun(ns);

    P dbgPrint(stderr, "nsRun returned %d\n", r);

    nsDestroy(ns);
}

int main(void)
{
    NS *ns = nsCreate();

    int listen_fd   = nsListen(ns, "localhost", 0);
    uint16_t listen_port = netLocalPort(listen_fd);

    P fprintf(stderr, "listen_fd = %d\n", listen_fd);
    P fprintf(stderr, "listen_port = %d\n", listen_port);

    if (fork() == 0) {
        /* Child. */
        nsClose(ns);
        nsDestroy(ns);
        tester(listen_port);
    }
    else {
        /* Parent. */
        testee(ns);
    }

    return errors;
}
