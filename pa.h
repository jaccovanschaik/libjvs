#ifndef PA_H
#define PA_H

/*
 * pa.h: Description
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: pa.h 203 2013-08-27 19:27:22Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

typedef struct {
    int num_ptrs;
    void **ptr;
} PA;

void paSet(PA *pa, int index, void *ptr);

void *paGet(PA *pa, int index);

void paDrop(PA *pa, int index);


#endif
