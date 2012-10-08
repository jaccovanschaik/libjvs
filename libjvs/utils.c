/*
 * utils.c: Description
 *
 * Copyright:   (c) 2012 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>

#include "defs.h"
#include "utils.h"

/*
 * Output <level> levels of indent to <fp>.
 */
void findent(FILE *fp, int level)
{
    int i;

    for (i = 0; i < level; i++) {
        fputs("    ", fp);
    }
}

/*
 * Print output given by <fmt> and the following parameters, preceded by
 * <indent> levels of indent, to <fp>. Returns the number of characters
 * printed (just like fprintf).
 */
int ifprintf(FILE *fp, int indent, const char *fmt, ...)
{
    int r;
    va_list ap;

    findent(fp, indent);

    va_start(ap, fmt);
    r = vfprintf(fp, fmt, ap);
    va_end(ap);

    return r;
}

#define HEXDUMP_BYTES_PER_LINE 16

/*
 * Dump <size> bytes from <data> as a hexdump to <fp>.
 */
void hexdump(FILE *fp, const char *data, int size)
{
    int i, offset = 0;
    char buffer[80];

    while (offset < size) {
        sprintf(buffer, "%06X  ", offset);

        for (i = offset; i < MIN(offset + HEXDUMP_BYTES_PER_LINE, size); i++) {
            sprintf(buffer + 8 + (i - offset) * 3, "%02hhX ", data[i]);
        }

        memset(buffer + 8 + (i - offset) * 3, ' ',
               sizeof(buffer) - (8 + (i - offset) * 3));

        for (i = offset; i < MIN(offset + HEXDUMP_BYTES_PER_LINE, size); i++) {
            sprintf(buffer + 9 + HEXDUMP_BYTES_PER_LINE * 3 + (i - offset),
                    "%c", isprint(data[i]) ? data[i] : '.');
        }

        fputs(buffer, fp);
        fputc('\n', fp);

        offset += HEXDUMP_BYTES_PER_LINE;
    }
}

int vstrpack(char *str, int size, va_list ap)
{
    int8_t i8;
    uint8_t u8;
    int16_t i16;
    uint16_t u16;
    int32_t i32;
    uint32_t u32;
    int64_t i64;
    uint64_t u64;
    float f32;
    double f64;

    int type;

    char *s, *p = str;

#define PACK(data, reorder) \
    if (p + sizeof(data) <= str + size) *((typeof(data) *) p) = reorder(data); \
    p += sizeof(data);

    while ((type = va_arg(ap, int)) != END) {
        switch(type) {
        case PACK_INT8:
            i8 = va_arg(ap, int);
            PACK(i8, );
            break;
        case PACK_UINT8:
            u8 = va_arg(ap, unsigned int);
            PACK(u8, );
            break;
        case PACK_INT16:
            i16 = va_arg(ap, int);
            PACK(i16, htobe16);
            break;
        case PACK_UINT16:
            u16 = va_arg(ap, unsigned int);
            PACK(u16, htobe16);
            break;
        case PACK_INT32:
            i32 = va_arg(ap, int);
            PACK(i32, htobe32);
            break;
        case PACK_UINT32:
            u32 = va_arg(ap, unsigned int);
            PACK(u32, htobe32);
            break;
        case PACK_INT64:
            i64 = va_arg(ap, int64_t);
            PACK(i64, htobe64);
            break;
        case PACK_UINT64:
            u64 = va_arg(ap, uint64_t);
            PACK(u64, htobe64);
            break;
        case PACK_FLOAT:
            f32 = va_arg(ap, double);
            PACK(f32, htobe32);
            break;
        case PACK_DOUBLE:
            f64 = va_arg(ap, double);
            PACK(f64, htobe64);
            break;
        case PACK_STRING:
            s = va_arg(ap, char *);
            u32 = strlen(s);
            PACK(u32, htobe32);
            if (p + u32 <= str + size) memcpy(p, s, u32);
            p += u32;
            break;
        case PACK_DATA:
            s = va_arg(ap, char *);
            u32 = va_arg(ap, unsigned int);
            PACK(u32, htobe32);
            if (p + u32 <= str + size) memcpy(p, s, u32);
            p += u32;
            break;
        }
    }

    return p - str;
}

int strpack(char *str, int size, ...)
{
    int r;
    va_list ap;

    va_start(ap, size);
    r = vstrpack(str, size, ap);
    va_end(ap);

    return r;
}

int vstrunpack(char *str, int size, va_list ap)
{

    return 0;
}

int strunpack(char *str, int size, ...)
{
    int r;
    va_list ap;

    va_start(ap, size);
    r = vstrunpack(str, size, ap);
    va_end(ap);

    return r;
}

#ifdef TEST
static int errors = 0;

void _make_sure_that(const char *file, int line, const char *str, int val)
{
   if (!val) {
      fprintf(stderr, "%s:%d: Expression \"%s\" failed\n", file, line, str);
      errors++;
   }
}

#define make_sure_that(expr) _make_sure_that(__FILE__, __LINE__, #expr, (expr))

int main(int argc, char *argv[])
{
    char buffer[64] = { };

    char expected[] = {
        1,
        0, 2,
        0, 0, 0, 3,
        0, 0, 0, 0, 0, 0, 0, 4,
        0x00, 0x00, 0x80, 0x4B,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x43,
        0, 0, 0, 3, 'H', 'o', 'i',
        0, 0, 0, 5, 'H', 'e', 'l', 'l', 'o'
    };

    int r = strpack(buffer, sizeof(buffer),
            PACK_INT8,      1,
            PACK_INT16,     2,
            PACK_INT32,     3,
            PACK_INT64,     4L,
            PACK_FLOAT,     1.0,
            PACK_DOUBLE,    2.0,
            PACK_STRING,    "Hoi",
            PACK_DATA,      "Hello", 5,
            END);

    make_sure_that(r == 43);

    make_sure_that(memcmp(buffer, expected, 43) == 0);

    /* hexdump(stderr, buffer, r); */

    return errors;
}
#endif
