/*
 * pa.c: Handle arrays of pointers.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: pa.c 203 2013-08-27 19:27:22Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdlib.h>

#include "defs.h"

#include "pa.h"

void paSet(PA *pa, int index, void *ptr)
{
    if (ptr == NULL) {
        paDrop(pa, index);
    }
    else if (index >= pa->num_ptrs) {
        int new_num_ptrs = index + 1;

        pa->ptr = realloc(pa->ptr, sizeof(void *) * new_num_ptrs);

        memset(pa->ptr + pa->num_ptrs, 0, sizeof(void *) * (new_num_ptrs - pa->num_ptrs));

        pa->num_ptrs = new_num_ptrs;
    }

    pa->ptr[index] = ptr;
}

void *paGet(PA *pa, int index)
{
    if (index >= pa->num_ptrs) {
        return NULL;
    }
    else {
        return pa->ptr[index];
    }
}

void paDrop(PA *pa, int index)
{
    if (index >= pa->num_ptrs) return;

    pa->ptr[index] = NULL;

    if (index == pa->num_ptrs - 1) {
        int new_num_ptrs;

        while (pa->ptr[index] == NULL && index >= 0) index--;

        new_num_ptrs = index + 1;

        pa->ptr = realloc(pa->ptr, sizeof(void *) * new_num_ptrs);

        pa->num_ptrs = new_num_ptrs;
    }
}

#ifdef TEST
#include "utils.h"

int errors = 0;

int main(int argc, char *argv[])
{
    PA pa = { 0 };

    paSet(&pa, 0, (void *) 0x1);

    make_sure_that(pa.num_ptrs == 1);
    make_sure_that(paGet(&pa, 0) == (void *) 0x1);
    make_sure_that(paGet(&pa, 1) == NULL);
    make_sure_that(paGet(&pa, 2) == NULL);

    paSet(&pa, 2, (void *) 0x3);

    make_sure_that(pa.num_ptrs == 3);
    make_sure_that(paGet(&pa, 0) == (void *) 0x1);
    make_sure_that(paGet(&pa, 1) == NULL);
    make_sure_that(paGet(&pa, 2) == (void *) 0x3);

    paSet(&pa, 1, (void *) 0x2);

    make_sure_that(pa.num_ptrs == 3);
    make_sure_that(paGet(&pa, 0) == (void *) 0x1);
    make_sure_that(paGet(&pa, 1) == (void *) 0x2);
    make_sure_that(paGet(&pa, 2) == (void *) 0x3);

    paDrop(&pa, 1);

    make_sure_that(pa.num_ptrs == 3);
    make_sure_that(paGet(&pa, 0) == (void *) 0x1);
    make_sure_that(paGet(&pa, 1) == NULL);
    make_sure_that(paGet(&pa, 2) == (void *) 0x3);

    paDrop(&pa, 0);

    make_sure_that(pa.num_ptrs == 3);
    make_sure_that(paGet(&pa, 0) == NULL);
    make_sure_that(paGet(&pa, 1) == NULL);
    make_sure_that(paGet(&pa, 2) == (void *) 0x3);

    paDrop(&pa, 2);

    make_sure_that(pa.num_ptrs == 0);
    make_sure_that(paGet(&pa, 0) == NULL);
    make_sure_that(paGet(&pa, 1) == NULL);
    make_sure_that(paGet(&pa, 2) == NULL);
}
#endif
