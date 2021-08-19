#ifndef LIBJVS_PA_H
#define LIBJVS_PA_H

/*
 * pa.h: Handle arrays of pointers.
 *
 * It's sometimes useful to have an array of pointers where the indexes are
 * smallish integers. For example, you might want to keep some extra information
 * for a bunch of file descriptors that you have opened. However, it's a
 * nuisance to keep track of the size of the array when elements are added and
 * removed, necessitating lots of realloc() calls.
 *
 * Use PointerArrays. You allocate a PointerArray and can then set, get and
 * clear elements in this array with abandon. All the reallocations will be
 * handled automatically for you. To the user it looks like an infinite array
 * where every index is available. The actual memory used is one pointer for
 * each index up to the highest one used. Unused indices look like they are set
 * to NULL.
 *
 * pa.h is part of libjvs.
 *
 * Copyright:   (c) 2013-2021 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: pa.h 438 2021-08-19 10:10:03Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int num_ptrs;
    void **ptr;
} PointerArray;

/*
 * Set the entry at <index> of pointer array <pa> to <ptr>.
 */
void paSet(PointerArray *pa, int index, void *ptr);

/*
 * Get the entry at <index> from pointer array <pa>.
 */
void *paGet(PointerArray *pa, int index);

/*
 * Drop (i.e. set to NULL) the entry at <index> in pointer array <pa>.
 */
void paDrop(PointerArray *pa, int index);

/*
 * Return the number of allocated entries in the pointer array.
 */
int paCount(PointerArray *pa);

/*
 * Clear the contents of pointer array <pa>.
 */
void paClear(PointerArray *pa);

/*
 * Destroy pointer array <pa>.
 */
void paDestroy(PointerArray *pa);

#ifdef __cplusplus
}
#endif

#endif
