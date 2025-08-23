#include "dis.c"

static int errors = 0;

static int fd[2];

static void handle_fd0(Dispatcher *dis, int fd, void *udata)
{
    UNUSED(udata);

    char buffer[16];

    int count = read(fd, buffer, sizeof(buffer));

    make_sure_that(count == 4);
    make_sure_that(strncmp(buffer, "Hoi!", 4) == 0);

    disClose(dis);
}

static void handle_timeout(Dispatcher *dis, double t, void *udata)
{
    UNUSED(dis);
    UNUSED(t);
    UNUSED(udata);

    if (write(fd[1], "Hoi!", 4) == -1) {
        perror("write");
        exit(1);
    }
}

int main(void)
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
