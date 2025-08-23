#include "options.c"

int errors = 0;

/*
 * Test a single short option without an argument.
 */
static void test1(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "-a" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_NONE);

    r = optParse(opts, argc, argv);

    make_sure_that(r == 2);
    make_sure_that(strcmp(argv[r], "bla") == 0);
    make_sure_that(optIsSet(opts, "option-a"));
    make_sure_that(optArg(opts, "option-a", NULL) == NULL);

    optDestroy(opts);
}

/*
 * Test a single long option without an argument.
 */
static void test2(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "--option-a" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_NONE);

    r = optParse(opts, argc, argv);

    make_sure_that(r == 2);
    make_sure_that(strcmp(argv[r], "bla") == 0);
    make_sure_that(optIsSet(opts, "option-a"));
    make_sure_that(optArg(opts, "option-a", NULL) == NULL);

    optDestroy(opts);
}

/*
 * Test short options with and without arguments.
 */
static void test3(void)
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
    make_sure_that(optArg(opts, "option-a", NULL) == NULL);
    make_sure_that(optIsSet(opts, "option-b"));
    make_sure_that(strcmp(optArg(opts, "option-b", NULL), "foo") == 0);

    optDestroy(opts);
}

/*
 * Test long options with and without arguments.
 */
static void test4(void)
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
    make_sure_that(optArg(opts, "option-a", NULL) == NULL);
    make_sure_that(optIsSet(opts, "option-b"));
    make_sure_that(strcmp(optArg(opts, "option-b", NULL), "foo") == 0);

    optDestroy(opts);
}

/*
 * Test combined short options with and without arguments.
 */
static void test5(void)
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
    make_sure_that(optArg(opts, "option-a", NULL) == NULL);
    make_sure_that(optIsSet(opts, "option-b"));
    make_sure_that(strcmp(optArg(opts, "option-b", NULL), "foo") == 0);

    optDestroy(opts);
}

/*
 * Test error on unexpected option.
 */
static void test6(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "-ab", "foo" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_NONE);

    r = optParse(opts, argc, argv);

    make_sure_that(r == -1);
    make_sure_that(strcmp(optErrors(opts),
                "Unknown option or argument in \"-ab\".\n") == 0);

    optDestroy(opts);
}

/*
 * Test a short option without an argument and a subsequent non-option argument.
 */
static void test7(void)
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

/*
 * Test a long option without an argument and a subsequent non-option argument.
 */
static void test8(void)
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

/*
 * Test error when an argument is given to an option that doesn't want one.
 */
static void test9(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "--option-a=foo" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_NONE);

    r = optParse(opts, argc, argv);

    make_sure_that(r == -1);
    make_sure_that(strcmp(optErrors(opts),
                "Unknown option or argument in \"--option-a=foo\".\n") == 0);

    optDestroy(opts);
}

/*
 * Test giving an argument to a long option with an optional argument.
 */
static void test10(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "--option-a=foo" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_OPTIONAL);

    r = optParse(opts, argc, argv);

    make_sure_that(r == 2);
    make_sure_that(optArg(opts, "option-a", NULL) != NULL);
    make_sure_that(strcmp(optArg(opts, "option-a", NULL), "foo") == 0);

    optDestroy(opts);
}

/*
 * Test *not* giving an argument to a long option with an optional argument.
 */
static void test11(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "--option-a" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_OPTIONAL);

    r = optParse(opts, argc, argv);

    make_sure_that(r == 2);
    make_sure_that(optArg(opts, "option-a", NULL) == NULL);

    optDestroy(opts);
}

/*
 * Test giving an argument to a short option with an optional argument.
 */
static void test12(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "-afoo" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_OPTIONAL);

    r = optParse(opts, argc, argv);

    make_sure_that(r == 2);
    make_sure_that(optArg(opts, "option-a", NULL) != NULL);
    make_sure_that(strcmp(optArg(opts, "option-a", NULL), "foo") == 0);

    optDestroy(opts);
}

/*
 * Test *not* giving an argument to a short option with an optional argument.
 */
static void test13(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "-a" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_OPTIONAL);

    r = optParse(opts, argc, argv);

    make_sure_that(r == 2);
    make_sure_that(optArg(opts, "option-a", NULL) == NULL);

    optDestroy(opts);
}

/*
 * Test multiple non-argument long options *without* associated short options.
 */
static void test14(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "--option-a", "--option-b" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 0, ARG_NONE);
    optAdd(opts, "option-b", 0, ARG_NONE);

    r = optParse(opts, argc, argv);

    make_sure_that(r == 3);
    make_sure_that(strcmp(argv[r], "bla") == 0);
    make_sure_that(optIsSet(opts, "option-a"));
    make_sure_that(optIsSet(opts, "option-b"));

    optDestroy(opts);
}

/*
 * Test error when not giving an argument to a short option that requires one.
 */
static void test15(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "-a", "-b", "-c" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_NONE);
    optAdd(opts, "option-b", 'b', ARG_NONE);
    optAdd(opts, "option-c", 'c', ARG_REQUIRED);

    r = optParse(opts, argc, argv);

    make_sure_that(r == -2);
    make_sure_that(strcmp(optErrors(opts),
                "Missing argument for \"-c\".\n") == 0);

    optDestroy(opts);
}

/*
 * Test error where the same option was specified more than once.
 */
static void test16(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "-a" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_NONE);
    optAdd(opts, "option-a", 'a', ARG_NONE);

    r = optParse(opts, argc, argv);

    make_sure_that(r == -3);

    make_sure_that(strcmp(optErrors(opts),
            "Option '--option-a' specified more than once.\n"
            "Option '-a' specified more than once.\n") == 0);

    optDestroy(opts);
}

/*
 * Test error where options was given more than once.
 */
static void test17(void)
{
    Options *opts = optCreate();

    char *argv[] = { "main", "bla", "-a", "-a" };
    int r, argc = sizeof(argv) / sizeof(argv[2]);

    optAdd(opts, "option-a", 'a', ARG_NONE);

    r = optParse(opts, argc, argv);

    make_sure_that(r == -4);
    make_sure_that(strcmp(optErrors(opts),
                "Option '--option-a' or '-a' given more than once.\n") == 0);

    optDestroy(opts);
}

int main(void)
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
    test10();
    test11();
    test12();
    test13();
    test14();
    test15();
    test16();
    test17();

    return errors;
}
