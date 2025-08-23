/*
 * pa.c: Handle arrays of pointers.
 *
 * It's sometimes useful to have an array of pointers where the indexes are
 * smallish integers. For example, you might want to keep some extra information
 * for a bunch of file descriptors that you have open. However, it's a nuisance
 * to keep track of the size of the array when elements are added and removed,
 * necessitating lots of realloc() calls.
 *
 * Use PointerArrays. You allocate a PointerArray and can then set, get and
 * clear elements in this array with abandon. All the reallocations will be
 * handled automatically for you. To the user it looks like an infinite array
 * where every index is available. The actual memory used is one pointer for
 * each index up to the highest one used. Unused indices look like they are set
 * to NULL.
 *
 * pa.c is part of libjvs.
 *
 * Copyright:   (c) 2013-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: pa.c 507 2025-08-23 14:43:51Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdlib.h>

#include "defs.h"
#include "debug.h"

#include "pa.h"

/*
 * Set the entry at <index> of pointer array <pa> to <ptr>.
 */
void paSet(PointerArray *pa, int index, void *ptr)
{
    if (ptr == NULL) {
        paDrop(pa, index);
    }
    else if (index < 0) {
        dbgAbort(stderr, "index must be >= 0, got %d\n", index);
    }
    else if (index >= pa->num_ptrs) {
        int new_num_ptrs = index + 1;

        pa->ptr = realloc(pa->ptr, sizeof(void *) * new_num_ptrs);

        memset(pa->ptr + pa->num_ptrs, 0,
                sizeof(void *) * (new_num_ptrs - pa->num_ptrs));

        pa->num_ptrs = new_num_ptrs;
    }

    pa->ptr[index] = ptr;
}

/*
 * Get the entry at <index> from pointer array <pa>.
 */
void *paGet(const PointerArray *pa, int index)
{
    if (index >= pa->num_ptrs) {
        return NULL;
    }
    else {
        return pa->ptr[index];
    }
}

/*
 * Drop (i.e. set to NULL) the entry at <index> in pointer array <pa>.
 */
void paDrop(PointerArray *pa, int index)
{
    if (index >= pa->num_ptrs) return;

    pa->ptr[index] = NULL;

    if (index == pa->num_ptrs - 1) {
        int new_num_ptrs;

        while (index >= 0 && pa->ptr[index] == NULL) index--;

        new_num_ptrs = index + 1;

        pa->ptr = realloc(pa->ptr, sizeof(void *) * new_num_ptrs);

        pa->num_ptrs = new_num_ptrs;
    }
}

/*
 * Return the number of allocated entries in the pointer array.
 */
int paCount(const PointerArray *pa)
{
    return pa->num_ptrs;
}

/*
 * Clear the contents of pointer array <pa>.
 */
void paClear(PointerArray *pa)
{
    if (pa->ptr != NULL) free(pa->ptr);

    pa->ptr = NULL;
    pa->num_ptrs = 0;
}

/*
 * Destroy pointer array <pa>.
 */
void paDestroy(PointerArray *pa)
{
    paClear(pa);

    free(pa);
}
