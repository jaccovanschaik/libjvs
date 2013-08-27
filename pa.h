#ifndef PA_H
#define PA_H

/*
 * pa.h: Description
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: pa.h 205 2013-08-27 19:33:55Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

typedef struct {
    int num_ptrs;
    void **ptr;
} PA;

/*
 * Set the entry at <index> of pointer array <pa> to <ptr>.
 */
void paSet(PA *pa, int index, void *ptr);

/*
 * Get the entry at <index> from pointer array <pa>.
 */
void *paGet(PA *pa, int index);

/*
 * Drop (i.e. set to NULL) the entry at <index> in pointer array <pa>.
 */
void paDrop(PA *pa, int index);

#endif
