#!/bin/sh

# integer.sh: Description
#
# Copyright:	(c) 2012 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id$
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for
# details.

Usage() {
    echo `basename $1` >&2
    exit $2
}

if [ $# -eq 0 ]; then
    Usage $0 0
elif [ $# -ne 1 ]; then
    Usage $0 1
elif [ $1 = "-h" ]; then
    version=h
elif [ $1 = "-c" ]; then
    version=c
else
    Usage $0 2
fi

if [ $version = h ]; then
cat << EOF
#ifndef TGEN_H
#define TGEN_H

EOF
fi

echo "/* GENERATED CODE - DO NOT EDIT */\n"

if [ $version = c ]; then
cat << EOF
#include <stdint.h>
#include <string.h>

#include "buffer.h"
#include "tgen.h"
EOF
else
cat << EOF
#include <stdint.h>

#include "buffer.h"
EOF
fi

for size in 8 16 32 64; do
    for sign in s u; do
        if [ $sign = 's' ]; then
            short="I${size}"
            ctype="int${size}_t"
        else
            short="U${size}"
            ctype="uint${size}_t"
        fi
cat << EOF

/*
 * Binary-encode $ctype <value> into buffer <buf>.
 */
EOF
        echo -n "int tgEncode$short(Buffer *buf, const $ctype *value)"
        if [ $version = h ]; then
            echo ";"
        elif [ $size -eq 8 ]; then
cat << EOF

{
    bufAddC(buf, *value);

    return 0;
}
EOF
        elif [ $size -eq 16 ]; then
cat << EOF

{
    bufAddC(buf, (*value) >> 8);
    bufAddC(buf, (*value) & 0xFF);

    return 0;
}
EOF
        elif [ $size -eq 32 ]; then
cat << EOF

{
    bufAddC(buf, ((*value) >> 24) & 0xFF);
    bufAddC(buf, ((*value) >> 16) & 0xFF);
    bufAddC(buf, ((*value) >>  8) & 0xFF);
    bufAddC(buf, ((*value) >>  0) & 0xFF);

    return 0;
}
EOF
        elif [ $size -eq 64 ]; then
cat << EOF

{
    bufAddC(buf, ((*value) >> 56) & 0xFF);
    bufAddC(buf, ((*value) >> 48) & 0xFF);
    bufAddC(buf, ((*value) >> 40) & 0xFF);
    bufAddC(buf, ((*value) >> 32) & 0xFF);
    bufAddC(buf, ((*value) >> 24) & 0xFF);
    bufAddC(buf, ((*value) >> 16) & 0xFF);
    bufAddC(buf, ((*value) >>  8) & 0xFF);
    bufAddC(buf, ((*value) >>  0) & 0xFF);

    return 0;
}
EOF
        fi

cat << EOF

/*
 * Get a binary-encoded $ctype from <buf> and store it in <value>. If
 * succesful, the first part where the $ctype was stored will be
 * stripped from <buf>.
 */
EOF
        echo -n "int tgDecode$short(Buffer *buf, $ctype *value)"
        if [ $version = h ]; then
            echo ";"
        else
cat << EOF

{
    const char *ptr = bufGet(buf);
    int remaining = bufLen(buf);

    if (tgExtract$short(&ptr, &remaining, value) == 0) {
        bufTrim(buf, bufLen(buf) - remaining, 0);
        return 0;
    }

    return 1;
}
EOF
        fi

cat << EOF

/*
 * Get a binary-encoded $ctype from <ptr> (with <remaining> bytes
 * remaining) and store it in <value>.
 */
EOF
        echo -n "int tgExtract$short(const char **ptr, int *remaining, $ctype *value)"
        if [ $version = h ]; then
            echo ";"
        elif [ $size -eq 8 ]; then
cat << EOF

{
    if (*remaining < sizeof($ctype)) return 1;

    *value = (unsigned char) **ptr;

    (*ptr)++;
    (*remaining)--;

    return 0;
}
EOF
        else
cat << EOF

{
    int i;

    if (*remaining < sizeof($ctype)) return 1;

    *value = 0;

    for (i = 0; i < sizeof($ctype); i++) {
        *value <<= 8;
        *value += (unsigned char) **ptr;

        (*ptr)++;
        (*remaining)--;
    }

    return 0;
}
EOF
        fi

cat << EOF

/*
 * Clear the contents of <item>.
 */
EOF
        echo -n "void tgClear$short($ctype *item)"
        if [ $version = h ]; then
            echo ";"
        else
cat << EOF

{
    *item = 0;
}
EOF
        fi

cat << EOF

/*
 * Return -1, 1 or 0 depending on <left> being smaller than, larger than
 * or equal to <right>.
 */
EOF
        echo -n "int tgCompare$short(const $ctype *left, const $ctype *right)"
        if [ $version = h ]; then
            echo ";"
        else
cat << EOF

{
    if (*left < *right)
        return -1;
    else if (*left > *right)
        return 1;
    else
        return 0;
}
EOF
        fi
    done
done

for size in 32 64; do
    short="F${size}"
    if [ $size -eq 32 ]; then
        ctype="float"
    elif [ $size -eq 64 ]; then
        ctype="double"
    fi
cat << EOF

/*
 * Binary-encode $ctype <value> into buffer <buf>.
 */
EOF
    echo -n "int tgEncode$short(Buffer *buf, const $ctype *value)"
    if [ $version = h ]; then
        echo ";"
    else
cat << EOF

{
    bufAdd(buf, value, sizeof($ctype));

    return 0;
}
EOF
    fi

cat << EOF

/*
 * Get a binary-encoded $ctype from <buf> and store it in <value>. If
 * succesful, the first part where the $ctype was stored will be
 * stripped from <buf>.
 */
EOF
    echo -n "int tgDecode$short(Buffer *buf, $ctype *value)"
    if [ $version = h ]; then
        echo ";"
    else
cat << EOF

{
    const char *ptr = bufGet(buf);
    int remaining = bufLen(buf);

    if (tgExtract$short(&ptr, &remaining, value) == 0) {
        bufTrim(buf, bufLen(buf) - remaining, 0);
        return 0;
    }

    return 1;
}
EOF
    fi

cat << EOF

/*
 * Get a binary-encoded $ctype from <ptr> (with <remaining> bytes
 * remaining) and store it in <value>.
 */
EOF
    echo -n "int tgExtract$short(const char **ptr, int *remaining, $ctype *value)"
    if [ $version = h ]; then
        echo ";"
    else
cat << EOF

{
    if (*remaining < sizeof($ctype))
        return 1;
    else {
        memcpy(value, *ptr, sizeof($ctype));

        (*ptr) += sizeof($ctype);
        (*remaining) -= sizeof($ctype);

        return 0;
    }
}
EOF
    fi
cat << EOF

/*
 * Clear the contents of <item>.
 */
EOF
    echo -n "void tgClear$short($ctype *item)"
    if [ $version = h ]; then
        echo ";"
    else
cat << EOF

{
    *item = 0.0;
}
EOF
    fi
cat << EOF

/*
 * Return -1, 1 or 0 depending on <left> being smaller than, larger than
 * or equal to <right>.
 */
EOF
    echo -n "int tgCompare$short(const $ctype *left, const $ctype *right)"
    if [ $version = h ]; then
        echo ";"
    else
cat << EOF

{
    if (*left < *right)
        return -1;
    else if (*left > *right)
        return 1;
    else
        return 0;
}
EOF
    fi
done

for size in 32 64; do
    short="U${size}"
    ctype="uint${size}_t"

cat << EOF

/*
 * Binary-encode $ctype <value> into buffer <buf>, preceded by a byte count.
 */
EOF
    echo -n "int tgEncode${short}C(Buffer *buf, const $ctype *value)"
    if [ $version = h ]; then
        echo ";"
    else
cat << EOF

{
    int shift = 8 * sizeof($ctype) - 8;
    $ctype mask = 0xFF << shift;
    uint8_t byte, bytes;

    for (bytes = sizeof($ctype); bytes > 0; bytes--) {
        byte = (*value & mask) >> shift;

        if (byte != 0) break;

        mask >>= 8;
        shift -= 8;
    }

    bufAddC(buf, bytes);

    while (shift >= 0) {
        bufAddC(buf, byte);

        mask >>= 8;
        shift -= 8;

        byte = (*value & mask) >> shift;
    }

    return 0;
}

/*
 * Extract a $ctype from <ptr>, preceded by a byte count.
 */
EOF
    fi

    echo -n "int tgExtract${short}C(const char **ptr, int *remaining, $ctype *value)"
    if [ $version = h ]; then
        echo ";"
    else
cat << EOF

{
    const char *my_ptr = *ptr;
    int i, my_remaining = *remaining;

    uint8_t byte, bytes;

    if (my_remaining < 1) return 1;

    bytes = *my_ptr;

    my_ptr++;
    my_remaining--;

    if (my_remaining < bytes) return 1;

    *value = 0;

    for (i = 0; i < bytes; i++) {
        byte = *my_ptr;

        *value <<= 8;
        *value += byte;

        my_ptr++;
        my_remaining--;
    }

    *ptr = my_ptr;
    *remaining = my_remaining;

    return 0;
}
EOF
    fi
done

if [ $version = h ]; then
cat << EOF

#endif
EOF
fi

