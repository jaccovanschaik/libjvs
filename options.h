#ifndef OPTIONS_H
#define OPTIONS_H

/*
 * options.h: Option parser.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: options.h 268 2016-03-07 13:41:28Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

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
 * Parse <argc> and <argv> and add the found options to <options>. Shuffles <argv> so that
 * non-option arguments are moved to the back, and returns the index of the first non-option
 * argument.
 */
int optParse(Options *options, int argc, char *argv[]);

/*
 * Return TRUE if the option with <long_name> was set on the command line.
 */
int optIsSet(Options *options, const char *long_name);

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

#endif
