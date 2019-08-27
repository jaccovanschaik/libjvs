#ifndef LIBJVS_PA_H
#define LIBJVS_PA_H

/*
 * pa.h: Handle arrays of pointers.
 *
 * pa.h is part of libjvs.
 *
 * Copyright:   (c) 2013-2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: pa.h 343 2019-08-27 08:39:24Z jacco $
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
