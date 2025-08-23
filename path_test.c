#include "path.c"
#include "utils.h"

static int errors = 0;

int main(void)
{
    Path *path = pathCreate(getenv("PATH"));

    make_sure_that(pathGet(path, "ls") != NULL);
    make_sure_that(pathGet(path, "path.c") == NULL);

    pathAdd(path, ".");

    make_sure_that(pathGet(path, "path.c") != NULL);

    FILE *fp;

    make_sure_that((fp = pathFOpen(path, "path.c", "r")) != NULL);
    make_sure_that(fclose(fp) == 0);

    int fd;

    make_sure_that((fd = pathOpen(path, "path.c", O_RDONLY)) != -1);
    make_sure_that(close(fd) == 0);

    return errors;
}
