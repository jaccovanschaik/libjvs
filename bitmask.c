/*
 * Type generator library, to be used together with the tgen program.
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
#include "tgen.h"
#include "defs.h"

#define ONE_INDENT "    "

/* Check that <mask> is big enough for <bit> and, if necessary, resize it. */
static void check_size(Bitmask *mask, int bit)
{
    int n_required_bytes = 1 + (bit / 8);

    if (mask->n_bytes < n_required_bytes) {
        mask->bits = realloc(mask->bits, n_required_bytes);

        /* Be sure to clear the new bytes we just got. */
        memset(mask->bits + mask->n_bytes, 0, n_required_bytes - mask->n_bytes);

        mask->n_bytes = n_required_bytes;
    }
}

/* Read a bitmask from <fp> and store it at <p>. */
int bmExtract(const char **p, int *remaining, Bitmask *mask)
{
    int i;
    uint8_t n_bytes;

    if (*remaining == 0) return 1;

    if (tgExtractU8(p, remaining, &n_bytes) != 0) return 1;

    if (n_bytes != mask->n_bytes) {
        mask->n_bytes = n_bytes;
        mask->bits = realloc(mask->bits, mask->n_bytes);
    }

    for (i = mask->n_bytes - 1; i >= 0; i--) {
        if (tgExtractU8(p, remaining, &mask->bits[i]) != 0) return 1;
    }

    return 0;
}

/* Write the bitmask at <mask> to <fp>. */
int bmEncode(Buffer *buf, const Bitmask *mask)
{
    int i;

    if (tgEncodeU8(buf, &mask->n_bytes) != 0) return 1;

    for (i = mask->n_bytes - 1; i >= 0; i--) {
        if (tgEncodeU8(buf, &mask->bits[i]) != 0) return 1;
    }

    return 0;
}

/*
 * Compare masks <left> and <right>, and return 1 if <left> is larger
 * than <right>, -1 if <left> is smaller than <right> and 0 if both
 * masks are equal.
 */
int bmCompare(const Bitmask *left, const Bitmask *right)
{
    int i, n_bits = sizeof(uint8_t) * MAX(left->n_bytes, right->n_bytes);

    for (i = n_bits - 1; i >= 0; i--) {
        if (bmGetBit(left, i) && !bmGetBit(right, i))
            return 1;
        else if (!bmGetBit(left, i) && bmGetBit(right, i))
            return -1;
    }

    return 0;
}

/* Allocate a new Bitmask and return a pointer to it. */
Bitmask *bmCreate(void)
{
    return calloc(1, sizeof(Bitmask));
}

/* Clear all bits in <mask>. */
void bmZero(Bitmask *mask)
{
    if (mask->bits) free(mask->bits);

    mask->bits    = NULL;
    mask->n_bytes = 0;
}

/* Free the Bitmask at <mask>. */
void bmDelete(Bitmask *mask)
{
    bmZero(mask);

    free(mask);
}

/* Set bit <bit> in <mask>. */
void bmSetBit(Bitmask *mask, int bit)
{
    check_size(mask, bit);

    mask->bits[bit / 8] |= (1 << (bit % 8));
}

/* Get bit <bit> in <mask>. Returns 1 if the bit is set, 0 otherwise. */
int bmGetBit(const Bitmask *mask, int bit)
{
    if (bit >= 8 * mask->n_bytes)
        return 0;
    else
        return (mask->bits[bit / 8] & (1 << (bit % 8))) ? 1 : 0;
}

/* Clear bit <bit> in <mask>. */
void bmClrBit(Bitmask *mask, int bit)
{
    if (bit >= 8 * mask->n_bytes)
        return;
    else
        mask->bits[bit / 8] &= ~(1 << (bit % 8));
}

/* Set the bits given after <mask>. End the list with END. */
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

/* Clear the bits given after <mask>. End the list with END. */
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