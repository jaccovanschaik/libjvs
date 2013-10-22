/*
 * options.c: Option parser.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: options.c 241 2013-10-14 19:11:16Z jacco $
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
#include "hash.h"
#include "buffer.h"
#include "utils.h"

#include "options.h"

typedef struct {
    const char *long_name;
    char short_name;
    OPT_Argument argument;
} Option;

struct Options {
    PointerArray options;
    HashTable results;
    Buffer errors;
};

/*
 * Find the option with short name <short_name> in options and return it, or return NULL if it
 * couldn't be found.
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
 * Add a parse result for option <opt> (with associated argument <arg>) to <options>. <argv0> is
 * used for an error message if the option was given more than once.
 */
static void opt_add_result(Options *options,
        const char *argv0, const Option *opt, const char *arg)
{
    if (hashIsSet(&options->results, HASH_STRING(opt->long_name))) {
        fprintf(stderr, "%s: option '--%s/-%c' given more than once\n",
                argv0, opt->long_name, opt->short_name);
    }

    hashAdd(&options->results, arg ? strdup(arg) : NULL, HASH_STRING(opt->long_name));
}

/*
 * Create a new option parser.
 */
Options *optCreate(void)
{
    return calloc(1, sizeof(Options));
}

/*
 * Add an option with long name <long_name> and short name <short_name> to <options>. <argument>
 * specifies whether the option can, may, or can not have an argument.
 */
void optAdd(Options *options, const char *long_name, char short_name, OPT_Argument argument)
{
    Option *option = calloc(1, sizeof(Option));

    option->long_name  = long_name;
    option->short_name = short_name;
    option->argument   = argument;

    paSet(&options->options, paCount(&options->options), option);
}

/*
 * Parse <argc> and <argv> and add the found options to <options>. Shuffles <argv> so that
 * non-option arguments are moved to the back, and returns the index of the first non-option
 * argument.
 */
int optParse(Options *options, int argc, char *argv[])
{
    Buffer opt_string = { 0 };
    int i, c;

    struct option *long_options = calloc(paCount(&options->options) + 1, sizeof(struct option));

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
            bufAdd(&opt_string, ":", 1);
        }
        else if (opt->argument == ARG_OPTIONAL) {
            long_options[i].has_arg = optional_argument;
            bufAdd(&opt_string, "::", 2);
        }
    }

    optind = 1;

    while (1) {
        Option *opt;

        c = getopt_long(argc, argv, bufGet(&opt_string), long_options, NULL);

        if (c == -1 || c == '?') break;

        opt = opt_find_short(options, c);

        opt_add_result(options, argv[0], opt, optarg);
    }

    bufReset(&opt_string);
    free(long_options);

    return c == -1 ? optind : -1;
}

/*
 * Return TRUE if the option with <long_name> was set on the command line.
 */
int optIsSet(Options *options, const char *long_name)
{
    return hashIsSet(&options->results, HASH_STRING(long_name));
}

/*
 * Return the argument given for the option with <long_name>. Returns NULL if the option was not set
 * on the command line, or if it didn't have an argument.
 */
char *optArg(Options *options, const char *long_name)
{
    return hashGet(&options->results, HASH_STRING(long_name));
}

/*
 * Clear out <options> without deleting <options> itself.
 */
void optClear(Options *options)
{
    int i;
    void *ptr;

    for (i = 0; i < paCount(&options->options); i++) {
        Option *opt = paGet(&options->options, i);

        free(opt);
    }

    for (i = hashFirst(&options->results, &ptr); i; i = hashNext(&options->results, &ptr)) {
        if (ptr != NULL) free(ptr);
    }

    paClear(&options->options);
    hashClearTable(&options->results);
    bufReset(&options->errors);
}

/*
 * Delete <options>.
 */
void optDestroy(Options *options)
{
    optClear(options);

    free(options);
}

#ifdef TEST
int errors = 0;

void test1(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "-a" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_NONE);

    r = optParse(opts, argc, argv);

    make_sure_that(r == 2);
    make_sure_that(strcmp(argv[r], "bla") == 0);
    make_sure_that(optIsSet(opts, "option-a"));
    make_sure_that(optArg(opts, "option-a") == NULL);

    optDestroy(opts);
}

void test2(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "--option-a" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_NONE);

    r = optParse(opts, argc, argv);

    make_sure_that(r == 2);
    make_sure_that(strcmp(argv[r], "bla") == 0);
    make_sure_that(optIsSet(opts, "option-a"));
    make_sure_that(optArg(opts, "option-a") == NULL);

    optDestroy(opts);
}

void test3(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "-a", "-b", "foo" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_NONE);
    optAdd(opts, "option-b", 'b', ARG_REQUIRED);

    r = optParse(opts, argc, argv);

    make_sure_that(r == 4);
    make_sure_that(strcmp(argv[r], "bla") == 0);
    make_sure_that(optIsSet(opts, "option-a"));
    make_sure_that(optArg(opts, "option-a") == NULL);
    make_sure_that(optIsSet(opts, "option-b"));
    make_sure_that(strcmp(optArg(opts, "option-b"), "foo") == 0);

    optDestroy(opts);
}

void test4(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "--option-a", "--option-b", "foo" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_NONE);
    optAdd(opts, "option-b", 'b', ARG_REQUIRED);

    r = optParse(opts, argc, argv);

    make_sure_that(r == 4);
    make_sure_that(strcmp(argv[r], "bla") == 0);
    make_sure_that(optIsSet(opts, "option-a"));
    make_sure_that(optArg(opts, "option-a") == NULL);
    make_sure_that(optIsSet(opts, "option-b"));
    make_sure_that(strcmp(optArg(opts, "option-b"), "foo") == 0);

    optDestroy(opts);
}

void test5(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "-ab", "foo" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_NONE);
    optAdd(opts, "option-b", 'b', ARG_REQUIRED);

    r = optParse(opts, argc, argv);

    make_sure_that(r == 3);
    make_sure_that(strcmp(argv[r], "bla") == 0);
    make_sure_that(optIsSet(opts, "option-a"));
    make_sure_that(optArg(opts, "option-a") == NULL);
    make_sure_that(optIsSet(opts, "option-b"));
    make_sure_that(strcmp(optArg(opts, "option-b"), "foo") == 0);

    optDestroy(opts);
}

void test6(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "-ab", "foo" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_NONE);

    r = optParse(opts, argc, argv);

    make_sure_that(r == -1);

    optDestroy(opts);
}

void test7(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "-a", "foo" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_NONE);

    r = optParse(opts, argc, argv);

    make_sure_that(r == 2);
    make_sure_that(strcmp(argv[r], "bla") == 0);
    make_sure_that(strcmp(argv[r + 1], "foo") == 0);

    optDestroy(opts);
}

void test8(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "--option-a", "foo" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_NONE);

    r = optParse(opts, argc, argv);

    make_sure_that(r == 2);
    make_sure_that(strcmp(argv[r], "bla") == 0);
    make_sure_that(strcmp(argv[r + 1], "foo") == 0);

    optDestroy(opts);
}

void test9(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "--option-a=foo" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_NONE);

    r = optParse(opts, argc, argv);

    make_sure_that(r == -1);

    optDestroy(opts);
}

int main(int argc, char *argv[])
{
    test1();
    test2();
    test3();
    test4();
    test5();
    test6();
    test7();
    test8();
    test9();

    return errors;
}
#endif
