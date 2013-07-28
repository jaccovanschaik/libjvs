/*
 * bitmask.c: Handle bitmasks of unlimited size.
 *
 * Copyright:	(c) 2008 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "buffer.h"
#include "bitmask.h"
#include "utils.h"
#include "defs.h"

#define ONE_INDENT "    "

/*
 * Check that <mask> is big enough for <bit> and, if necessary, resize it.
 */
static void bm_check_size(Bitmask *mask, unsigned int bit)
{
    int n_required_bytes = 1 + (bit / 8);

    if (mask->n_bytes < n_required_bytes) {
        mask->bits = realloc(mask->bits, n_required_bytes);

        /* Be sure to clear the new bytes we just got. */
        memset(mask->bits + mask->n_bytes, 0, n_required_bytes - mask->n_bytes);

        mask->n_bytes = n_required_bytes;
    }
}

/*
 * Compare masks <left> and <right>, and return 1 if <left> is larger
 * than <right>, -1 if <left> is smaller than <right> and 0 if both
 * masks are equal.
 */
int bmCompare(const Bitmask *left, const Bitmask *right)
{
    int i, n_bytes = MAX(left->n_bytes, right->n_bytes);

    for (i = n_bytes - 1; i >= 0; i--) {
        uint8_t l = left->n_bytes > i ? left->bits[i] : 0;
        uint8_t r = right->n_bytes > i ? right->bits[i] : 0;

        if (l < r)
            return -1;
        else if (l > r)
            return 1;
    }

    return 0;
}

/*
 * Allocate a new Bitmask and return a pointer to it.
 */
Bitmask *bmCreate(void)
{
    return calloc(1, sizeof(Bitmask));
}

/*
 * Clear all bits in <mask>.
 */
void bmZero(Bitmask *mask)
{
    if (mask->bits) free(mask->bits);

    mask->bits    = NULL;
    mask->n_bytes = 0;
}

/*
 * Free the Bitmask at <mask>.
 */
void bmDestroy(Bitmask *mask)
{
    bmZero(mask);

    free(mask);
}

/*
 * Set bit <bit> in <mask>.
 */
void bmSetBit(Bitmask *mask, unsigned int bit)
{
    bm_check_size(mask, bit);

    mask->bits[bit / 8] |= (1 << (bit % 8));
}

/*
 * Get bit <bit> in <mask>. Returns 1 if the bit is set, 0 otherwise.
 */
int bmGetBit(const Bitmask *mask, unsigned int bit)
{
    if (bit >= 8 * mask->n_bytes)
        return 0;
    else
        return (mask->bits[bit / 8] & (1 << (bit % 8))) ? 1 : 0;
}

/*
 * Clear bit <bit> in <mask>.
 */
void bmClrBit(Bitmask *mask, unsigned int bit)
{
    if (bit >= 8 * mask->n_bytes)
        return;
    else
        mask->bits[bit / 8] &= ~(1 << (bit % 8));
}

/*
 * Set the bits given after <mask>. End the list with END.
 */
void bmSetBits(Bitmask *mask, ...)
{
    int b;
    va_list ap;

    va_start(ap, mask);

    while ((b = (va_arg(ap, int))) != END) {
        bmSetBit(mask, b);
    }

    va_end(ap);
}

/*
 * Clear the bits given after <mask>. End the list with END.
 */
void bmClrBits(Bitmask *mask, ...)
{
    int b;
    va_list ap;

    va_start(ap, mask);

    while ((b = (va_arg(ap, int))) != END) {
        bmClrBit(mask, b);
    }

    va_end(ap);
}

#ifdef TEST
static int errors = 0;

int main(int argc, char *argv[])
{
    Bitmask mask = { 0 };
    Bitmask *mask2;

    bmSetBit(&mask, 0);

    make_sure_that(mask.n_bytes == 1);
    make_sure_that(mask.bits[0] == 0x01);

    make_sure_that(bmGetBit(&mask, 0) == 1);
    make_sure_that(bmGetBit(&mask, 1) == 0);
    make_sure_that(bmGetBit(&mask, 2) == 0);
    make_sure_that(bmGetBit(&mask, 3) == 0);
    make_sure_that(bmGetBit(&mask, 4) == 0);
    make_sure_that(bmGetBit(&mask, 5) == 0);
    make_sure_that(bmGetBit(&mask, 6) == 0);
    make_sure_that(bmGetBit(&mask, 7) == 0);
    make_sure_that(bmGetBit(&mask, 1000) == 0);

    bmSetBit(&mask, 9);

    make_sure_that(mask.n_bytes == 2);
    make_sure_that(mask.bits[0] == 0x01);
    make_sure_that(mask.bits[1] == 0x02);

    make_sure_that(bmGetBit(&mask, 0) == 1);
    make_sure_that(bmGetBit(&mask, 1) == 0);
    make_sure_that(bmGetBit(&mask, 2) == 0);
    make_sure_that(bmGetBit(&mask, 3) == 0);
    make_sure_that(bmGetBit(&mask, 4) == 0);
    make_sure_that(bmGetBit(&mask, 5) == 0);
    make_sure_that(bmGetBit(&mask, 6) == 0);
    make_sure_that(bmGetBit(&mask, 7) == 0);
    make_sure_that(bmGetBit(&mask, 8) == 0);
    make_sure_that(bmGetBit(&mask, 9) == 1);
    make_sure_that(bmGetBit(&mask, 1000) == 0);

    bmClrBit(&mask, 0);

    make_sure_that(mask.n_bytes == 2);
    make_sure_that(mask.bits[0] == 0x00);
    make_sure_that(mask.bits[1] == 0x02);

    make_sure_that(bmGetBit(&mask, 0) == 0);
    make_sure_that(bmGetBit(&mask, 1) == 0);
    make_sure_that(bmGetBit(&mask, 2) == 0);
    make_sure_that(bmGetBit(&mask, 3) == 0);
    make_sure_that(bmGetBit(&mask, 4) == 0);
    make_sure_that(bmGetBit(&mask, 5) == 0);
    make_sure_that(bmGetBit(&mask, 6) == 0);
    make_sure_that(bmGetBit(&mask, 7) == 0);
    make_sure_that(bmGetBit(&mask, 8) == 0);
    make_sure_that(bmGetBit(&mask, 9) == 1);
    make_sure_that(bmGetBit(&mask, 1000) == 0);

    bmClrBit(&mask, 9);

    make_sure_that(mask.n_bytes == 2);
    make_sure_that(mask.bits[0] == 0x00);
    make_sure_that(mask.bits[1] == 0x00);

    make_sure_that(bmGetBit(&mask, 0) == 0);
    make_sure_that(bmGetBit(&mask, 1) == 0);
    make_sure_that(bmGetBit(&mask, 2) == 0);
    make_sure_that(bmGetBit(&mask, 3) == 0);
    make_sure_that(bmGetBit(&mask, 4) == 0);
    make_sure_that(bmGetBit(&mask, 5) == 0);
    make_sure_that(bmGetBit(&mask, 6) == 0);
    make_sure_that(bmGetBit(&mask, 7) == 0);
    make_sure_that(bmGetBit(&mask, 8) == 0);
    make_sure_that(bmGetBit(&mask, 9) == 0);
    make_sure_that(bmGetBit(&mask, 1000) == 0);

    bmSetBits(&mask, 0, 2, 4, 6, 8, 10, 12, 14, END);

    make_sure_that(mask.n_bytes == 2);
    make_sure_that(mask.bits[0] == 0x55);
    make_sure_that(mask.bits[1] == 0x55);

    bmClrBits(&mask, 0, 2, 4, 6, 8, 10, 12, 14, END);

    make_sure_that(mask.n_bytes == 2);
    make_sure_that(mask.bits[0] == 0x00);
    make_sure_that(mask.bits[1] == 0x00);

    mask2 = bmCreate();

    make_sure_that(mask2->n_bytes == 0);
    make_sure_that(mask2->bits == NULL);

    make_sure_that(bmCompare(&mask, mask2) == 0);

    bmSetBit(&mask, 0);

    make_sure_that(bmCompare(&mask, mask2) == 1);

    bmSetBit(mask2, 0);

    make_sure_that(bmCompare(&mask, mask2) == 0);

    bmSetBit(mask2, 1);

    make_sure_that(bmCompare(&mask, mask2) == -1);

    bmZero(&mask);
    bmDestroy(mask2);

    return errors;
}
#endif
