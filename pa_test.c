#include "pa.c"
#include "utils.h"

int errors = 0;

int main(void)
{
    PointerArray pa = { 0 };

    paSet(&pa, 0, (void *) 0x1);

    make_sure_that(paCount(&pa) == 1);
    make_sure_that(paGet(&pa, 0) == (void *) 0x1);
    make_sure_that(paGet(&pa, 1) == NULL);
    make_sure_that(paGet(&pa, 2) == NULL);

    paSet(&pa, 2, (void *) 0x3);

    make_sure_that(paCount(&pa) == 3);
    make_sure_that(paGet(&pa, 0) == (void *) 0x1);
    make_sure_that(paGet(&pa, 1) == NULL);
    make_sure_that(paGet(&pa, 2) == (void *) 0x3);

    paSet(&pa, 1, (void *) 0x2);

    make_sure_that(paCount(&pa) == 3);
    make_sure_that(paGet(&pa, 0) == (void *) 0x1);
    make_sure_that(paGet(&pa, 1) == (void *) 0x2);
    make_sure_that(paGet(&pa, 2) == (void *) 0x3);

    paDrop(&pa, 0);

    make_sure_that(paCount(&pa) == 3);
    make_sure_that(paGet(&pa, 0) == NULL);
    make_sure_that(paGet(&pa, 1) == (void *) 0x2);
    make_sure_that(paGet(&pa, 2) == (void *) 0x3);

    paDrop(&pa, 2);

    make_sure_that(paCount(&pa) == 2);
    make_sure_that(paGet(&pa, 0) == NULL);
    make_sure_that(paGet(&pa, 1) == (void *) 0x2);
    make_sure_that(paGet(&pa, 2) == NULL);

    paDrop(&pa, 1);

    make_sure_that(paCount(&pa) == 0);
    make_sure_that(paGet(&pa, 0) == NULL);
    make_sure_that(paGet(&pa, 1) == NULL);
    make_sure_that(paGet(&pa, 2) == NULL);

    paClear(&pa);
}
