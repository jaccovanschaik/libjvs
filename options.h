#ifndef OPTIONS_H
#define OPTIONS_H

/*
 * options.h: Option parser.
 *
 * options.h is part of libjvs.
 *
 * Copyright:   (c) 2013-2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: options.h 364 2019-11-08 12:30:12Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Options Options;

typedef enum {
    ARG_NONE,
    ARG_OPTIONAL,
    ARG_REQUIRED
} OPT_Argument;

/*
 * Create a new option parser.
 */
Options *optCreate(void);

/*
 * Add an option with long name <long_name> and short name <short_name> to <options>. <argument>
 * specifies whether the option can, may, or can not have an argument.
 */
void optAdd(Options *options, const char *long_name, char short_name, OPT_Argument argument);

/*
 * Parse <argc> and <argv> and add the found options to <options>. If no errors
 * occurred, <argv> will be shuffled so that non-option arguments are moved to
 * the back, and the index of the first non-option is returned. Otherwise
 * returns -1 if an unknown option was found, or -2 if no argument was found for
 * an option that requires one.
 */
int optParse(Options *options, int argc, char *argv[]);

/*
 * Return the error encountered by optParse after it returned -1 or -2.
 */
const char *optErrors(const Options *options);

/*
 * Return TRUE if the option with <long_name> was set on the command line.
 */
int optIsSet(const Options *options, const char *long_name);

/*
 * Return the argument given for the option with <long_name>. Returns NULL if the option was not set
 * on the command line, or if it didn't have an argument.
 */
const char *optArg(Options *options, const char *long_name, const char *fallback);

/*
 * Clear out <options> without deleting <options> itself.
 */
void optClear(Options *options);

/*
 * Delete <options>.
 */
void optDestroy(Options *options);

#ifdef __cplusplus
}
#endif

#endif
