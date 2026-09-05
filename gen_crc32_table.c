/*
 * gen_crc32_table.c: Generate a CRC32 polynomial table.
 *
 * Shamelessly stolen (and adapted) from... well from everywhere, because this
 * code is all over the Internet. At least the calculation part.
 *
 * Copyright: (c) 2026 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2026-09-04
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 *
 * vim: softtabstop=4 shiftwidth=4 expandtab textwidth=80
 */

#include <stdio.h>
#include <stdint.h>

int main(void)
{
    uint32_t rem;
    int i, j;

    fprintf(stdout, "static uint32_t crc32_table[] = {\n    ");

    for (i = 0; i < 256; i++) {
        rem = i;

        for (j = 0; j < 8; j++) {
            if (rem & 1) {
                rem >>= 1;
                rem ^= 0xedb88320;
            }
            else {
                rem >>= 1;
            }
        }

        fprintf(stdout, "0x%08x", rem);

        if (i == 255) {
            fprintf(stdout, "\n");
        }
        else if ((i % 4) == 3) {
            fprintf(stdout, ",\n    ");
        }
        else {
            fprintf(stdout, ", ");
        }
    }

    fprintf(stdout, "};\n");
}
