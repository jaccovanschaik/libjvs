#ifndef LIBJVS_DEFS_H
#define LIBJVS_DEFS_H

/*
 * defs.h: some useful #defines.
 *
 * defs.h is part of libjvs.
 *
 * Copyright:   (c) 2007-2023 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: defs.h 475 2023-02-21 08:08:11Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

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

#ifndef END
#define END (-1)
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE (!FALSE)
#endif

#ifndef ON
#define ON      TRUE
#endif

#ifndef OFF
#define OFF     FALSE
#endif

#ifndef YES
#define YES     TRUE
#endif

#ifndef NO
#define NO      FALSE
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
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

#ifdef __cplusplus
}
#endif

#endif
