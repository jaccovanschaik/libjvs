/*
 * options.c: Option parser.
 *
 * options.c is part of libjvs.
 *
 * Copyright:   (c) 2013-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: options.c 507 2025-08-23 14:43:51Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>

#include "pa.h"
#include "hashlist.h"
#include "hash.h"
#include "buffer.h"
#include "utils.h"
#include "debug.h"

#include "options.h"

typedef struct {
    ListNode _node;
    char *opt;
    char *arg;
} OptionResult;

typedef struct {
    const char *long_name;
    char short_name;
    OPT_Argument argument;
} Option;

struct Options {
    PointerArray options;
    HashList results;
    int err;
    Buffer errors;
};

#if 0
static void dump_args(FILE *fp, const char *msg, int argc, char *argv[])
{
    fprintf(fp, "%s\n", msg);

    for (int i = 0; i < argc; i++) {
        fprintf(fp, "arg %d: \"%s\"\n", i, argv[i]);
    }
}
#endif

/*
 * Find the option with short name <short_name> in options and return it, or
 * return NULL if it couldn't be found.
 */
static Option *opt_find_short(Options *options, char short_name)
{
    int i;

    for (i = 0; i < paCount(&options->options); i++) {
        Option *opt = paGet(&options->options, i);

        if (opt->short_name == short_name) return opt;
    }

    return NULL;
}

/*
 * Find the option with long name <long_name> in options and return it, or
 * return NULL if it couldn't be found.
 */
static Option *opt_find_long(Options *options, const char *long_name)
{
    int i;

    for (i = 0; i < paCount(&options->options); i++) {
        Option *opt = paGet(&options->options, i);

        if (strcmp(opt->long_name, long_name) == 0) return opt;
    }

    return NULL;
}

/*
 * Add a parse result for option <opt> (with associated argument <arg>) to
 * <options>. <argv0> is used for an error message if the option was given
 * more than once.
 */
static void opt_add_result(Options *options,
        const Option *opt, const char *arg)
{
    if (hlContains(&options->results, HASH_STRING(opt->long_name))) {
        bufAddF(&options->errors, "Option '--%s' ", opt->long_name);

        if (opt->short_name != 0) {
            bufAddF(&options->errors, "or '-%c' ", opt->short_name);
        }

        bufAddF(&options->errors, "given more than once.\n");

        options->err = -4;
        return;
    }

    OptionResult *result = calloc(1, sizeof(*result));

    result->arg = arg ? strdup(arg) : NULL;
    result->opt = strdup(opt->long_name);

    hlAdd(&options->results, result, result->opt, strlen(result->opt));
}

/*
 * Create a new option parser.
 */
Options *optCreate(void)
{
    return calloc(1, sizeof(Options));
}

/*
 * Add an option with long name <long_name> and short name <short_name> to
 * <options>. <argument> specifies whether the option can, may, or can not
 * have an argument.
 */
void optAdd(Options *options,
        const char *long_name, char short_name, OPT_Argument argument)
{
    if (opt_find_long(options, long_name) != NULL) {
        bufAddF(&options->errors, "Option '--%s' specified more than once.\n",
                long_name);
        options->err = -3;
    }

    if (short_name != 0 && opt_find_short(options, short_name) != NULL) {
        bufAddF(&options->errors, "Option '-%c' specified more than once.\n",
                short_name);
        options->err = -3;
    }

    if (options->err != 0) return;

    Option *option = calloc(1, sizeof(Option));

    option->long_name  = long_name;
    option->short_name = short_name;
    option->argument   = argument;

    paSet(&options->options, paCount(&options->options), option);
}

/*
 * Parse <argc> and <argv> and add the found options to <options>. If no errors
 * occurred, <argv> will be shuffled so that non-option arguments are moved to
 * the back, and the index of the first non-option is returned. Otherwise
 * returns -1 if an unknown option was found, or -2 if no argument was found for
 * an option that requires one.
 */
int optParse(Options *options, int argc, char *argv[])
{
    Buffer opt_string = { 0 };
    int i, c;

    if (options->err != 0) return options->err;

    struct option *long_options =
        calloc(paCount(&options->options) + 1, sizeof(struct option));

    bufAddC(&opt_string, ':');

    for (i = 0; i < paCount(&options->options); i++ ) {
        Option *opt = paGet(&options->options, i);

        long_options[i].name = opt->long_name;
        long_options[i].flag = NULL;
        long_options[i].val  = opt->short_name;

        bufAddC(&opt_string, opt->short_name);

        if (opt->argument == ARG_NONE) {
            long_options[i].has_arg = no_argument;
        }
        else if (opt->argument == ARG_REQUIRED) {
            long_options[i].has_arg = required_argument;
            bufAddS(&opt_string, ":");
        }
        else if (opt->argument == ARG_OPTIONAL) {
            long_options[i].has_arg = optional_argument;
            bufAddS(&opt_string, "::");
        }
    }

    optind = 1;

    while (1) {
        Option *opt;
        int option_index = 0;

        c = getopt_long(argc, argv, bufGet(&opt_string),
                long_options, &option_index);

        if (c == -1) {
            break;
        }
        else if (c == '?') {
            bufAddF(&options->errors,
                    "Unknown option or argument in \"%s\".\n",
                    argv[optind - 1]);
            break;
        }
        else if (c == ':') {
            bufAddF(&options->errors,
                    "Missing argument for \"%s\".\n", argv[optind - 1]);
            break;
        }
        else if (c == 0) {
            opt = opt_find_long(options, long_options[option_index].name);
            opt_add_result(options, opt, optarg);
        }
        else {
            opt = opt_find_short(options, c);
            opt_add_result(options, opt, optarg);
        }
    }

    bufClear(&opt_string);
    free(long_options);

    if (options->err != 0) {
        return options->err;
    }
    else if (c == -1)
        return optind;
    else if (c == '?')
        return -1;
    else if (c == ':')
        return -2;
    else
        dbgAbort(stderr, "Unexpected return value for getopt_long.\n");
}

/*
 * Return the error encountered by optParse after it returned -1 or -2.
 */
const char *optErrors(const Options *options)
{
    return bufGet(&options->errors);
}

/*
 * Return TRUE if the option with <long_name> was set on the command line.
 */
int optIsSet(const Options *options, const char *long_name)
{
    return hlContains(&options->results, HASH_STRING(long_name));
}

/*
 * Return the argument given for the option with <long_name>. Returns NULL if
 * the option was not set on the command line, or if it didn't have an
 * argument.
 */
const char *optArg(Options *options, const char *long_name,
        const char *fallback)
{
    OptionResult *result = hlGet(&options->results, HASH_STRING(long_name));

    return (result && result->arg) ? result->arg : fallback;
}

/*
 * Clear out <options> without deleting <options> itself.
 */
void optClear(Options *options)
{
    int i;
    OptionResult *result, *next;

    for (i = 0; i < paCount(&options->options); i++) {
        Option *opt = paGet(&options->options, i);

        free(opt);
    }

    for (result = hlHead(&options->results); result; result = next) {
        next = hlNext(result);

        hlDel(&options->results, HASH_STRING(result->opt));

        free(result->arg);
        free(result->opt);
        free(result);
    }

    paClear(&options->options);
}

/*
 * Delete <options>.
 */
void optDestroy(Options *options)
{
    optClear(options);

    free(options);
}
