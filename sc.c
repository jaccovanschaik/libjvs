/*
 * sc.c: String carousel
 *
 * I often find myself in a situation where I'm returning a statically
 * allocated string from a function, often formatted using printf or strftime,
 * and usually to be used for logging or debugging. The function simply
 * overwrites that static string buffer on every call, growing or shrinking it
 * as needed.
 *
 * This works well until you try to get more than one string from that
 * function, for example when you want to pass them on to another function
 * (usually printf). You will find that, because it uses the same buffer space
 * every time, the second call will overwrite the buffer from the first call
 * and you will get the second buffer multiple times. What you ideally want is
 * to have more than a single buffer to temporarily store a formatted string
 * in while you use it.
 *
 * A string carousel can help. Instead of a single statically allocated string
 * buffer you use a single statically allocated carousel, and instead of just
 * returning a pointer to your buffer you first pass the string to be returned
 * to that carousel. It will store it internally and return a pointer to *its*
 * buffer, which you then return. You can do this a number of times (that
 * number to be specified by you) before it starts reusing its old buffers, so
 * up to that moment it will seem like you're returning a new string buffer
 * every time.
 *
 * Copyright: (c) 2021-2022 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2021-08-19 10:28:07.939717978 +0200
 * Version:   $Id: sc.c 467 2022-11-20 00:05:38Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "sc.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct {
    size_t size;
    char *str;
} SC_Cell;

struct SC {
    size_t   curr;
    size_t   count;
    SC_Cell *cell;
};

/*
 * Create a new string carousel and give it <count> entries.
 */
SC *scCreate(size_t count)
{
    SC *sc = calloc(1, sizeof(SC));

    sc->count = count;
    sc->cell  = calloc(count, sizeof(SC_Cell));

    return sc;
}

/*
 * Set the next entry in the string carousel to the string given by the
 * printf-compatible format string <fmt> and the arguments in <ap>.
 */
const char *scAddV(SC *sc, const char *fmt, va_list ap)
{
    sc->curr = (sc->curr + 1) % sc->count;

    SC_Cell *cell = sc->cell + sc->curr;

    va_list ap_copy;

    va_copy(ap_copy, ap);

    size_t n = vsnprintf(cell->str, cell->size, fmt, ap_copy);

    va_end(ap_copy);

    if (n >= cell->size) {
        cell->size = n + 1;
        cell->str  = realloc(cell->str, cell->size);

        vsnprintf(cell->str, cell->size, fmt, ap);
    }

    return cell->str;
}

/*
 * Set the next entry in the string carousel to the string given by the
 * printf-compatible format string <fmt> and the subsequent arguments.
 */
__attribute__((format (printf, 2, 3)))
const char *scAdd(SC *sc, const char *fmt, ...)
{
    const char *ptr;
    va_list ap;

    va_start(ap, fmt);
    ptr = scAddV(sc, fmt, ap);
    va_end(ap);

    return ptr;
}

/*
 * Destroy the string carousel <sc>.
 */
void scDestroy(SC *sc)
{
    for (size_t i = 0; i < sc->count; i++) {
        free(sc->cell[i].str);
    }

    free(sc);
}

#ifdef TEST
#include "utils.h"

static int errors = 0;

int main(void)
{
    SC *sc = scCreate(4);

    make_sure_that(sc != NULL);
    make_sure_that(sc->count == 4);
    make_sure_that(sc->cell != NULL);

    make_sure_that(sc->cell[0].str == NULL);
    make_sure_that(sc->cell[0].size == 0);

    make_sure_that(sc->cell[1].str == NULL);
    make_sure_that(sc->cell[1].size == 0);

    make_sure_that(sc->cell[2].str == NULL);
    make_sure_that(sc->cell[2].size == 0);

    make_sure_that(sc->cell[3].str == NULL);
    make_sure_that(sc->cell[3].size == 0);

    const char *p[5];

    p[0] = scAdd(sc, "Nul");

    make_sure_that(strcmp(p[0], "Nul") == 0);

    p[1] = scAdd(sc, "%s", "Een");

    make_sure_that(strcmp(p[1], "Een") == 0);
    make_sure_that(p[1] != p[0]);

    p[2] = scAdd(sc, "%d", 2);

    make_sure_that(strcmp(p[2], "2") == 0);
    make_sure_that(p[2] != p[0]);
    make_sure_that(p[2] != p[1]);

    p[3] = scAdd(sc, "<%s>", "Drie");

    make_sure_that(strcmp(p[3], "<Drie>") == 0);
    make_sure_that(p[3] != p[0]);
    make_sure_that(p[3] != p[1]);
    make_sure_that(p[3] != p[2]);

    p[4] = scAdd(sc, "%02d", 4);

    make_sure_that(strcmp(p[4], "04") == 0);
    make_sure_that(p[4] == p[0]);
    make_sure_that(p[4] != p[1]);
    make_sure_that(p[4] != p[2]);
    make_sure_that(p[4] != p[3]);
}

#endif
