#include "udp.c"
#include "net.h"

static int errors = 0;

int main(void)
{
    int r;
    char buffer[16];

    uint16_t recv_port = 1234;

    int recv_fd = udpSocket();
    int send_fd = udpSocket();

    // Simple test: bind receiving socket, send from sending socket.

    udpBind(recv_fd, "localhost", recv_port);

    udpSend(send_fd, "localhost", recv_port, "Hallo!", 6);
    r = read(recv_fd, buffer, sizeof(buffer));

    make_sure_that(r == 6);
    make_sure_that(strncmp(buffer, "Hallo!", 6) == 0);

    close(recv_fd);
    close(send_fd);

    // Bind both sending and receiving socket, send from sending socket.

    uint16_t send_port = 1235;

    recv_fd = udpSocket();
    send_fd = udpSocket();

    udpBind(send_fd, "localhost", send_port);
    udpBind(recv_fd, "localhost", recv_port);

    udpSend(send_fd, "localhost", recv_port, "Hallo!", 6);
    r = read(recv_fd, buffer, sizeof(buffer));

    make_sure_that(r == 6);
    make_sure_that(strncmp(buffer, "Hallo!", 6) == 0);

    // Also try reverse: send from receive socket, receive on send socket.

    udpSend(recv_fd, "localhost", send_port, "Hallo!", 6);
    r = read(send_fd, buffer, sizeof(buffer));

    make_sure_that(r == 6);
    make_sure_that(strncmp(buffer, "Hallo!", 6) == 0);

    return errors;
}
