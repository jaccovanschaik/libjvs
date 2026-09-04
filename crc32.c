/*
 * crc32.c: XXX
 *
 * Copyright: (c) 2026 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2026-09-04
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 *
 * vim: softtabstop=4 shiftwidth=4 expandtab textwidth=80
 */

#include "crc32.h"
#include "crc32_table.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

uint32_t crc32(uint32_t crc, const char *buf, size_t len)
{
    crc = ~crc;

    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc & 0xff) ^ buf[i]];
    }

    return ~crc;
}
