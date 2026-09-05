/*
 * crc32_test.c: Test crc32 function.
 *
 * Copyright: (c) 2026 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2026-09-05
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 *
 * vim: softtabstop=4 shiftwidth=4 expandtab textwidth=80
 */

#include "crc32.h"
#include "utils.h"

#include <string.h>

static int errors = 0;

int main(void)
{
    // Example string from https://rosettacode.org/wiki/CRC-32
    const char *str = "The quick brown fox jumps over the lazy dog";

    uint32_t crc = crc32(0, str, strlen(str));

    make_sure_that(crc == 0x414FA339);

    crc = 0;

    for (const char *p = str; *p != '\0'; p++) {
        crc = crc32(crc, p, 1);
    }

    make_sure_that(crc == 0x414FA339);

    return errors;
}
