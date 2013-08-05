#ifndef BITMASK_H
#define BITMASK_H

/*
 * bitmask.h: Handle bitmasks of unlimited size.
 *
 * Copyright:	(c) 2008 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdint.h>

typedef struct {
    uint8_t  n_bytes;
    uint8_t *bits;
} Bitmask;

/*
 * Allocate a new Bitmask and return a pointer to it.
 */
Bitmask *bmCreate(void);

/*
 * Set bit number <bit> in <mask>.
 */
void bmSetBit(Bitmask *mask, unsigned int bit);

/*
 * Get bit number <bit> in <mask>. Returns 1 if the bit is set, 0 otherwise.
 */
int bmGetBit(const Bitmask *mask, unsigned int bit);

/*
 * Clear bit number <bit> in <mask>.
 */
void bmClrBit(Bitmask *mask, unsigned int bit);

/*
 * Set the bit numbers given after <mask>. End the list with END.
 */
void bmSetBits(Bitmask *mask, ...);

/*
 * Clear the bit numbers given after <mask>. End the list with END.
 */
void bmClrBits(Bitmask *mask, ...);

/*
 * Compare masks <left> and <right>, and return 1 if <left> is larger
 * than <right>, -1 if <left> is smaller than <right> and 0 if both
 * masks are equal.
 */
int bmCompare(const Bitmask *left, const Bitmask *right);

/*
 * Clear all bits in <mask>. You may free() <mask> after this.
 */
void bmClear(Bitmask *mask);

/*
 * Destroy the Bitmask at <mask>.
 */
void bmDestroy(Bitmask *mask);

#endif
