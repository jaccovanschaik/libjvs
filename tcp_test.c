#include "tcp.c"
#include "net.h"

static int errors = 0;

static int test_msg(int send_fd, int recv_fd, char *msg)
{
    int r;
    char buf[32];

    if ((r = tcpWrite(send_fd, msg, strlen(msg))) != (int) strlen(msg)) {
        fprintf(stderr, "tcpWrite to send_fd failed.\n");
        return 1;
    }
    else if ((r = tcpRead(recv_fd, buf, strlen(msg))) != (int) strlen(msg)) {
        fprintf(stderr, "tcpRead from recv_fd failed.\n");
        return 1;
    }
    else if (strncmp(buf, msg, strlen(msg)) != 0) {
        fprintf(stderr, "buf = \"%s\", msg = \"%s\"\n", buf, msg);
        return 1;
    }
    else {
        return 0;
    }
}

static int listen_fd, client_fd, server_fd;

static char *client_msg = "Hello client!";
static char *server_msg = "Hello server!";

static int make_connection(const char *host, uint16_t port, int family)
{
    int (*listen_func)(const char *host, uint16_t port);
    int (*connect_func)(const char *host, uint16_t port);

    if (family == AF_INET) {
        listen_func = tcp4Listen;
        connect_func = tcp4Connect;
    }
    else if (family == AF_INET6) {
        listen_func = tcp6Listen;
        connect_func = tcp6Connect;
    }
    else {
        listen_func = tcpListen;
        connect_func = tcpConnect;
    }

    if ((listen_fd = listen_func(host, port)) == -1) {
        dbgError(stderr, "tcpListen() failed");
        return 1;
    }

    if (port == 0) {
        port = netLocalPort(listen_fd);
    }

    if ((client_fd = connect_func(host, port)) == -1) {
        dbgError(stderr, "tcpConnect(IPv4) failed");
        return 1;
    }
    else if ((server_fd = tcpAccept(listen_fd)) == -1) {
        dbgError(stderr, "tcpAccept() failed");
        return 1;
    }

    return 0;
}

static int test_connection(const char *host, uint16_t port, int family)
{
    if (make_connection(host, port, family) != 0) {
        fprintf(stderr,
                "Couldn't create connection (host %s, port %hu, family %d)\n",
                host, port, family);
        return 1;
    }
    else if (test_msg(client_fd, server_fd, server_msg) != 0) {
        return 1;
    }
    else if (test_msg(server_fd, client_fd, client_msg) != 0) {
        return 1;
    }
    else if (close(listen_fd) != 0) {
        perror("close");
        return 1;
    }
    else if (close(client_fd) != 0) {
        perror("close");
        return 1;
    }
    else if (close(server_fd) != 0) {
        perror("close");
        return 1;
    }
    else {
        return 0;
    }
}

int main(void)
{
    errors += test_connection(NULL, 54321, AF_UNSPEC);
    errors += test_connection(NULL, 54321, AF_INET);
    errors += test_connection(NULL, 54321, AF_INET6);

    errors += test_connection("localhost", 54321, AF_UNSPEC);
    errors += test_connection("ip6-localhost", 54321, AF_UNSPEC);

    errors += test_connection("localhost", 54321, AF_INET);
    errors += test_connection("ip6-localhost", 54321, AF_INET6);

    errors += test_connection("localhost", 54321, AF_INET6);
    errors += test_connection("ip6-localhost", 54321, AF_INET);

    errors += test_connection(NULL, 0, AF_UNSPEC);
    errors += test_connection(NULL, 0, AF_INET);
    errors += test_connection(NULL, 0, AF_INET6);

    return errors;
}
