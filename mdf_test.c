#include "mdf.c"
#include "utils.h"

static void dump(MDF_Object *obj, Buffer *buf)
{
    while (obj != NULL) {
        if (bufLen(buf) > 0) bufAddC(buf, ' ');

        bufAddF(buf, "%s ", obj->name);

        switch(obj->type) {
        case MDF_STRING:
            bufAddF(buf, "\"%s\"", obj->u.s);
            break;
        case MDF_INT:
            bufAddF(buf, "%ld", obj->u.i);
            break;
        case MDF_FLOAT:
            bufAddF(buf, "%g", obj->u.f);
            break;
        case MDF_CONTAINER:
            bufAddF(buf, "{");
            dump(obj->u.c, buf);
            bufAddF(buf, " }");
            break;
        }

        obj = obj->next;
    }
}

static int errors = 0;

typedef struct {
    int error;
    const char *input;
    const char *output;
} Test;

static void do_test(int index, Test *test)
{
    Buffer output = { 0 };

    MDF_Stream *stream = mdfOpenString(test->input);
    MDF_Object *object = mdfParse(stream);

    dump(object, &output);

    if (test->error) {
        if (object != NULL) {
            fprintf(stderr, "Test %d:\n", index);
            fprintf(stderr, "\texpected error \"%s\"\n", test->output);
            fprintf(stderr, "\tgot output \"%s\"\n", bufGet(&output));

            errors++;
        }
        else if (strcmp(mdfError(stream), test->output) != 0) {
            fprintf(stderr, "Test %d:\n", index);
            fprintf(stderr, "\texpected error \"%s\"\n", test->output);
            fprintf(stderr, "\tgot error \"%s\"\n", mdfError(stream));

            errors++;
        }
    }
    else {
        if (object == NULL) {
            fprintf(stderr, "Test %d:\n", index);
            fprintf(stderr, "\texpected output \"%s\"\n", test->output);
            fprintf(stderr, "\tgot error \"%s\"\n", mdfError(stream));

            errors++;
        }
        else if (strcmp(bufGet(&output), test->output) != 0) {
            fprintf(stderr, "Test %d:\n", index);
            fprintf(stderr, "\texpected output \"%s\"\n", test->output);
            fprintf(stderr, "\tgot output \"%s\"\n", bufGet(&output));

            errors++;
        }
    }

    mdfClose(stream);
    mdfFree(object);
    bufRewind(&output);
}

static Test test[] = {
    { 0, "Test 123",               "Test 123" },
    { 0, "Test -123",              "Test -123" },
    { 0, "Test 033",               "Test 27" },
    { 0, "Test 0x10",              "Test 16" },
    { 0, "Test 1.3",               "Test 1.3" },
    { 0, "Test -1.3",              "Test -1.3" },
    { 0, "Test 1e3",               "Test 1000" },
    { 0, "Test 1e-3",              "Test 0.001" },
    { 0, "Test -1e3",              "Test -1000" },
    { 0, "Test -1e-3",             "Test -0.001" },
    { 0, "Test \"ABC\"",           "Test \"ABC\"" },
    { 0, "Test \"\\t\\r\\n\\\\\"", "Test \"\t\r\n\\\"" },
    { 0, "Test 123 # Comment",     "Test 123" },
    { 0, "Test { Test1 123 Test2 1.3 Test3 \"ABC\" }",
         "Test { Test1 123 Test2 1.3 Test3 \"ABC\" }" },
    { 0, "Test 123 456",           "Test 123 Test 456" },
    { 0, "123",                    "(null) 123" },
    { 0, "Test { 123 } { \"ABC\" }",
         "Test { (null) 123 } Test { (null) \"ABC\" }" },
    { 0, "Test { Test1 123 } { Test2 \"ABC\" }",
         "Test { Test1 123 } Test { Test2 \"ABC\" }" },
    { 1, "123ABC",
         "<string>:1: unrecognized value \"123ABC\"" },
    { 1, "123XYZ",
         "<string>:1: unexpected character 'X' (ascii 88)" },
    { 1, "ABC$",
         "<string>:1: unexpected character '$' (ascii 36)" },
    { 1, "123$",
         "<string>:1: unexpected character '$' (ascii 36)" },
    { 1, "Test {\n\tTest1 123\n\tTest2 1.3\n\tTest3 \"ABC\\0\"\n}",
         "<string>:4: invalid escape sequence \"\\0\"" },
    { 1, "Test { Test2 { Test3 123 Test4 1.3 Test5 \"ABC\" }",
         "<string>:1: unexpected end of file" },
    { 1, "Test { Test1 123 Test2 1.3 Test3 \"ABC\" } }",
         "<string>:1: unbalanced '}'" },
};

static int num_tests = sizeof(test) / sizeof(test[0]);

int main(void)
{
    int i;

    make_sure_that(strcmp(mdfTypeName(MDF_STRING), "string") == 0);
    make_sure_that(strcmp(mdfTypeName(MDF_INT), "int") == 0);
    make_sure_that(strcmp(mdfTypeName(MDF_FLOAT), "float") == 0);
    make_sure_that(strcmp(mdfTypeName(MDF_CONTAINER), "container") == 0);

    for (i = 0; i < num_tests; i++) {
        do_test(i, test + i);
    }

    return errors;
}
