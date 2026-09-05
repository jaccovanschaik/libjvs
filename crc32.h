#ifndef CRC32_H
#define CRC32_H

/*
 * crc32.c: calculate crc32 checksum.
 *
 * Copyright: (c) 2026 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2026-09-04
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 *
 * vim: softtabstop=4 shiftwidth=4 expandtab textwidth=80
 */


#include <stdint.h>
#include <stdlib.h>

uint32_t crc32(uint32_t crc, const char *buf, size_t len);

#endif
