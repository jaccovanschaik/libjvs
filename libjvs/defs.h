/*
 * Some useful #defines.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: defs.h 242 2007-06-23 23:12:05Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifndef DEFS_H
#define DEFS_H

#include <string.h>
#include <strings.h>

#define ever (;;)

#if defined(PARANOID)
#define P if (1)
#define D if (1)
#elif defined(DEBUG)
#define P if (0)
#define D if (1)
#else
#define P if (0)
#define D if (0)
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE (!FALSE)
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef SQR
#define SQR(x) ((x) * (x))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef CLAMP
#define CLAMP(x, a, b) MIN(MAX((x), (a)), (b))
#endif

#ifndef ABS
#define ABS(x) ((x) < 0 ? -(x) : (x))
#endif

#ifndef SGN
#define SGN(x) ((x) < 0 ? -1 : 1)
#endif

#ifndef STREQU
#define STREQU(a, b) (!strcmp((a), (b)))
#endif

#ifndef STRNEQU
#define STRNEQU(a, b, n) (!strncmp((a), (b), (n)))
#endif

#ifndef STRCEQU
#define STRCEQU(a, b) (!strcasecmp((a), (b)))
#endif

#ifndef STRNCEQU
#define STRNCEQU(a, b, n) (!strncasecmp((a), (b), (n)))
#endif

#ifndef ROUND_TO
#define ROUND_TO(val, step) \
        (SGN(val) * (step) * ((int) ((SGN(val) * (val) / (step)) + 0.5)))
#endif

#ifndef ROUND
#define ROUND(val) ROUND_TO(val, 1.0)
#endif

#ifndef ROUND_UP
#define ROUND_UP(val, step)   ((step) *  ceil((double) (val) / (double) (step)))
#endif

#ifndef ROUND_DOWN
#define ROUND_DOWN(val, step) ((step) * floor((double) (val) / (double) (step)))
#endif

#endif