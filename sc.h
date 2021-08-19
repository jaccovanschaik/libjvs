#ifndef SC_H
#define SC_H

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
 * Copyright: (c) 2021 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2021-08-19 11:00:05.207629253 +0200
 * Version:   $Id: sc.h 437 2021-08-19 10:08:31Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdlib.h>
#include <stdarg.h>

typedef struct SC SC;

/*
 * Create a new string carousel and give it <count> entries.
 */
SC *scCreate(size_t count);

/*
 * Set the next entry in the string carousel to the string given by the
 * printf-compatible format string <fmt> and the arguments in <ap>.
 */
const char *scAddV(SC *sc, const char *fmt, va_list ap);

/*
 * Set the next entry in the string carousel to the string given by the
 * printf-compatible format string <fmt> and the subsequent arguments.
 */
__attribute__((format (printf, 2, 3)))
const char *scAdd(SC *sc, const char *fmt, ...);

/*
 * Destroy the string carousel <sc>.
 */
void scDestroy(SC *sc);

#endif
