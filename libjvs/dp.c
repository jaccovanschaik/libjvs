/*
 * dp.c: Data Parser.
 *
 * Copyright:   (c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>

#include "buffer.h"

#include "dp.h"

typedef enum {
    DP_PT_NONE,
    DP_PT_FILE,
    DP_PT_FD,
    DP_PT_FP,
    DP_PT_STR
} DP_ParserType;

typedef enum {
    DP_PS_NONE,
    DP_PS_COMMENT,
    DP_PS_NAME,
    DP_PS_STRING,
    DP_PS_ESCAPE,
    DP_PS_NUMBER,
    DP_PS_ERROR,
    DP_PS_EOF
} DP_ParserState;

struct DP_Parser {
    DP_ParserType type;
    DP_ParserState state;
    Buffer name, value, error;
    int stack_depth;
    List **stack;
    int line;
    union {
        FILE *fp;
        const char *str;
    } u;
};

/*
 * Push back character <c> onto the input stream.
 */
static void dp_unget_char(DP_Parser *parser, int c)
{
    if (c == EOF) return;

    if (parser->type == DP_PT_STR) {
        parser->u.str--;

        /* We don't want to overwrite the string that the user has given us, so we'll just assert
         * that the character we're pushing back is the same that was already there. */

        assert(c == *parser->u.str);
    }
    else {
        ungetc(c, parser->u.fp);
    }

    if (c == '\n') parser->line--;
}

/*
 * Get a character from the input stream.
 */
static int dp_get_char(DP_Parser *parser)
{
    int c;

    if (parser->type == DP_PT_STR) {
        c = *parser->u.str;

        if (c == '\0')
            c = EOF;
        else
            parser->u.str++;
    }
    else {
        c = fgetc(parser->u.fp);
    }

    /* Squish any end-of-line sequence into just a line feed. */

    if (c == '\r') {
        int c2 = dp_get_char(parser);

        if (c2 != '\n') {
            dp_unget_char(parser, c2);
        }

        c = '\n';
    }

    if (c == '\n') parser->line++;

    return c;
}

/*
 * Push DP_Object list <list> onto <parser>'s stack.
 */
static void dp_push(DP_Parser *parser, List *list)
{
    parser->stack_depth++;

    parser->stack = realloc(parser->stack, parser->stack_depth * sizeof(List *));

    parser->stack[parser->stack_depth - 1] = list;
}

/*
 * Pop the top-level DP_Object list from <parser>'s stack and return it.
 */
static List *dp_pop(DP_Parser *parser)
{
    List *top_of_stack;

    if (parser->stack_depth == 0) {
        return NULL;
    }

    top_of_stack = parser->stack[parser->stack_depth - 1];

    parser->stack_depth--;

    parser->stack = realloc(parser->stack, parser->stack_depth * sizeof(List *));

    return top_of_stack;
}

/*
 * Add a DP_Object to <objects>, using the current data in <parser>.
 */
static DP_Object *dp_add_object(DP_Parser *parser, List *objects)
{
    char *end = NULL;
    DP_Object *obj;
    long i;
    double f;

    const char *value = bufGet(&parser->value);

    obj = calloc(1, sizeof(DP_Object));
    obj->line = parser->line;

    if (parser->state == DP_PS_NONE) {
        obj->type = DP_CONTAINER;
    }
    else if (parser->state == DP_PS_STRING) {
        obj->type = DP_STRING;
        obj->u.s = strdup(value);
    }
    else if ((i = strtol(value, &end, 0)), end == value + strlen(value)) {
        obj->type = DP_INT;
        obj->u.i = i;
    }
    else if ((f = strtod(value, &end)), end == value + strlen(value)) {
        obj->type = DP_FLOAT;
        obj->u.f = f;
    }
    else {
        bufSetF(&parser->error, "%d: unrecognized value \"%s\"", parser->line, value);
        dpFreeObject(obj);
        return NULL;
    }

    if (bufLen(&parser->name) > 0)
        obj->name = strdup(bufGet(&parser->name));
    else
        obj->name = NULL;

    listAppendTail(objects, obj);

    return obj;
}

/*
 * Run <parser> and append the found objects to <objects>.
 */
static int dp_parse(DP_Parser *parser, List *objects)
{
    DP_Object *obj;

    parser->line = 1;

    while (parser->state != DP_PS_ERROR && parser->state != DP_PS_EOF) {
        int c = dp_get_char(parser);

        switch(parser->state) {
        case DP_PS_NONE:
            if (c == '_' || isalpha(c)) {
                bufSetC(&parser->name, c);
                parser->state = DP_PS_NAME;
            }
            else if (c == '"') {
                bufClear(&parser->value);
                parser->state = DP_PS_STRING;
            }
            else if (isdigit(c) || c == '-' || c == '+' || c == '.') {
                bufSetC(&parser->value, c);
                parser->state = DP_PS_NUMBER;
            }
            else if (c == '{') {
                if ((obj = dp_add_object(parser, objects)) == NULL) {
                    parser->state = DP_PS_ERROR;
                }
                else {
                    dp_push(parser, objects);
                    objects = &obj->u.c;
                    bufClear(&parser->name);
                }
            }
            else if (c == '}') {
                DP_Object *last;

                if ((objects = dp_pop(parser)) == NULL) {
                    bufSetF(&parser->error, "%d: unbalanced '}'", parser->line);
                    parser->state = DP_PS_ERROR;
                }
                else if ((last = listTail(objects)) == NULL) {
                    bufClear(&parser->name);
                }
                else {
                    bufSet(&parser->name, last->name, strlen(last->name));
                }
            }
            else if (c == '#') {
                parser->state = DP_PS_COMMENT;
            }
            else if (c == EOF) {
                parser->state = DP_PS_EOF;
            }
            else if (!isspace(c)) {
                bufSetF(&parser->error, "%d: unexpected character '%c' following \"%s\"",
                        parser->line, c, bufGet(&parser->value));
                parser->state = DP_PS_ERROR;
            }
            break;
        case DP_PS_COMMENT:
            if (c == '\n') {
                parser->state = DP_PS_NONE;
            }
            else if (c == EOF) {
                parser->state = DP_PS_EOF;
            }
            break;
        case DP_PS_NAME:
            if (c == '_' || isalnum(c)) {
                bufAddC(&parser->name, c);
            }
            else if (isspace(c) || c == '{' || c == '}') {
                dp_unget_char(parser, c);
                parser->state = DP_PS_NONE;
            }
            else if (c == EOF) {
                bufSetF(&parser->error, "%d: value for \"%s\" expected",
                        parser->line, bufGet(&parser->name));
                parser->state = DP_PS_ERROR;
            }
            else {
                bufSetF(&parser->error, "%d: unexpected character '%c' following \"%s\"",
                        parser->line, c, bufGet(&parser->name));
                parser->state = DP_PS_ERROR;
            }
            break;
        case DP_PS_STRING:
            if (c == '\\') {
                parser->state = DP_PS_ESCAPE;
            }
            else if (c == '"') {
                if (dp_add_object(parser, objects) == NULL) {
                    parser->state = DP_PS_ERROR;
                }
                else {
                    parser->state = DP_PS_NONE;
                }
            }
            else if (isprint(c)) {
                bufAddC(&parser->value, c);
            }
            else if (c == EOF) {
                bufSetF(&parser->error, "%d: unexpected end of file following \"%s\"",
                        parser->line, bufGet(&parser->value));
                parser->state = DP_PS_ERROR;
            }
            else {
                bufSetF(&parser->error, "%d: unexpected character '%c' following \"%s\"",
                        parser->line, c, bufGet(&parser->value));
                parser->state = DP_PS_ERROR;
            }
            break;
        case DP_PS_ESCAPE:
            if (c == 't') {
                bufAddC(&parser->value, '\t');
                parser->state = DP_PS_STRING;
            }
            else if (c == 'r') {
                bufAddC(&parser->value, '\r');
                parser->state = DP_PS_STRING;
            }
            else if (c == 'n') {
                bufAddC(&parser->value, '\n');
                parser->state = DP_PS_STRING;
            }
            else if (c == '\\') {
                bufAddC(&parser->value, '\\');
                parser->state = DP_PS_STRING;
            }
            else if (c == EOF) {
                bufSetF(&parser->error, "%d: unexpected end of file in escape sequence",
                        parser->line);
                parser->state = DP_PS_ERROR;
            }
            else {
                bufSetF(&parser->error, "%d: unrecognized escape sequence \"\\%c\"",
                        parser->line, c);
                parser->state = DP_PS_ERROR;
            }
            break;
        case DP_PS_NUMBER:
            if (isxdigit(c) || c == 'x' || c == '.' ||
                c == 'e' || c == 'E' || c == '+' || c == '-') {
                bufAddC(&parser->value, c);
            }
            else if (isspace(c) || c == '{' || c == '}' || c == EOF) {
                dp_unget_char(parser, c);
                if (dp_add_object(parser, objects) == NULL) {
                    parser->state = DP_PS_ERROR;
                }
                else {
                    parser->state = DP_PS_NONE;
                }
            }
            else {
                bufSetF(&parser->error, "%d: unexpected character '%c' following \"%s\"",
                        parser->line, c, bufGet(&parser->value));
                parser->state = DP_PS_ERROR;
            }
            break;
        default:
            abort();
        }
    }

    if (parser->state == DP_PS_ERROR)
        return 1;
    else if (parser->stack_depth > 0) {
        bufSetF(&parser->error, "%d: unbalanced '{'", parser->line);
        return 1;
    }
    else
        return 0;
}

/*
 * Create a Data Parser.
 */
DP_Parser *dpCreate(void)
{
    DP_Parser *parser = calloc(1, sizeof(DP_Parser));

    return parser;
}

/*
 * Clear out <parser>. Call this before any of the dpParse functions if you want to re-use an
 * existing parser.
 */
void dpClear(DP_Parser *parser)
{
    if (parser->type == DP_PT_FILE || parser->type == DP_PT_FD) {
        if (parser->u.fp != NULL) fclose(parser->u.fp);
    }

    if (parser->stack != NULL) {
        free(parser->stack);
    }

    bufReset(&parser->name);
    bufReset(&parser->value);
    bufReset(&parser->error);

    memset(parser, 0, sizeof(DP_Parser));
}

/*
 * Free the memory occupied by <parser>.
 */
void dpFree(DP_Parser *parser)
{
    dpClear(parser);

    free(parser);
}

/*
 * Using <parser>, parse the contents of <filename> and append the found objects to <objects>.
 */
int dpParseFile(DP_Parser *parser, const char *filename, List *objects)
{
    parser->type = DP_PT_FILE;

    if ((parser->u.fp = fopen(filename, "r")) == NULL) {
        bufSetF(&parser->error, "%s: %s", filename, strerror(errno));
        return 1;
    }
    else {
        return dp_parse(parser, objects);
    }
}

/*
 * Using <parser>, parse the contents from <fp> and append the found objects to <objects>.
 */
int dpParseFP(DP_Parser *parser, FILE *fp, List *objects)
{
    parser->type = DP_PT_FP;
    parser->u.fp = fp;

    return dp_parse(parser, objects);
}

/*
 * Using <parser>, parse the contents from <fd> and append the found objects to <objects>.
 */
int dpParseFD(DP_Parser *parser, int fd, List *objects)
{
    parser->type = DP_PT_FD;

    if ((parser->u.fp = fdopen(fd, "r")) == NULL) {
        bufSetF(&parser->error, "Couldn't fdopen file descriptor %d: %s", fd, strerror(errno));
        return 1;
    }
    else {
        return dp_parse(parser, objects);
    }
}

/*
 * Using <parser>, parse <string> and append the found objects to <objects>.
 */
int dpParseString(DP_Parser *parser, const char *string, List *objects)
{
    parser->type = DP_PT_STR;
    parser->u.str = string;

    return dp_parse(parser, objects);
}

/*
 * Return the name of type <type> as a string.
 */
const char *dpTypeAsString(DP_Type type)
{
    switch(type) {
    case DP_STRING:
        return "string";
    case DP_INT:
        return "integer";
    case DP_FLOAT:
        return "float";
    case DP_CONTAINER:
        return "container";
    default:
        return NULL;
    }
}

/*
 * Retrieve an error text from <parser>, in case any function has returned an error.
 */
const char *dpError(DP_Parser *parser)
{
    return bufGet(&parser->error);
}

/*
 * Free DP_Object <obj>.
 */
void dpFreeObject(DP_Object *obj)
{
    if (obj->type == DP_CONTAINER) {
        dpClearObjects(&obj->u.c);
    }
    else if (obj->type == DP_STRING && obj->u.s != NULL) {
        free(obj->u.s);
    }

    if (obj->name != NULL) free(obj->name);

    free(obj);
}

/*
 * Clear the list of objects in <objects>. <objects> itself is not removed.
 */
void dpClearObjects(List *objects)
{
    DP_Object *obj;

    while ((obj = listRemoveHead(objects)) != NULL) {
        dpFreeObject(obj);
    }
}

/*
 * Free the list of objects in <objects>.
 */
void dpFreeObjects(List *objects)
{
    dpClearObjects(objects);

    free(objects);
}

#ifdef TEST

static int errors = 0;

typedef struct {
    const char *input;
    int status;
    const char *output;
} Test;

Test test[] = {
    { "Test 123", 0, "Test: I(123)" },
    { "Test -123", 0, "Test: I(-123)" },
    { "Test 033", 0, "Test: I(27)" },
    { "Test 0x10", 0, "Test: I(16)" },
    { "Test 1.3", 0, "Test: F(1.3)" },
    { "Test -1.3", 0, "Test: F(-1.3)" },
    { "Test 1e3", 0, "Test: F(1000)" },
    { "Test 1e-3", 0, "Test: F(0.001)" },
    { "Test -1e3", 0, "Test: F(-1000)" },
    { "Test -1e-3", 0, "Test: F(-0.001)" },
    { "Test \"ABC\"", 0, "Test: S(ABC)" },
    { "Test \"\\t\\r\\n\\\\\"", 0, "Test: S(\t\r\n\\)" },
    { "Test 123 # Comment", 0, "Test: I(123)" },
    { "Test { Test1 123 Test2 1.3 Test3 \"ABC\" }", 0,
      "Test: { Test1: I(123) Test2: F(1.3) Test3: S(ABC) }" },
    { "Test 123 456", 0, "Test: I(123) Test: I(456)" },
    { "123", 0, "(null): I(123)" },
    { "Test { 123 } { \"ABC\" }", 0,
      "Test: { (null): I(123) } Test: { (null): S(ABC) }" },
    { "Test { Test1 123 } { Test2 \"ABC\" }", 0,
      "Test: { Test1: I(123) } Test: { Test2: S(ABC) }" },
    { "123ABC", 1, "1: unrecognized value \"123ABC\"" },
    { "123XYZ", 1, "1: unexpected character 'X' following \"123\"" },
    { "ABC$", 1, "1: unexpected character '$' following \"ABC\"" },
    { "123$", 1, "1: unexpected character '$' following \"123\"" },
    { "Test {\n\tTest1 123\n\tTest2 1.3\n\tTest3 \"ABC\\0\"\n}", 1,
      "4: unrecognized escape sequence \"\\0\"" },
    { "Test { Test2 { Test3 123 Test4 1.3 Test5 \"ABC\" }", 1,
      "1: unbalanced '{'" },
    { "Test { Test1 123 Test2 1.3 Test3 \"ABC\" } }", 1,
      "1: unbalanced '}'" },
};

static int num_tests = sizeof(test) / sizeof(test[0]);

static void dump_object(Buffer *output, DP_Object *obj)
{
    DP_Object *child;

    if (bufLen(output) > 0) bufAddC(output, ' ');

    switch(obj->type) {
    case DP_STRING:
        bufAddF(output, "%s: S(%s)", obj->name, obj->u.s);
        break;
    case DP_INT:
        bufAddF(output, "%s: I(%ld)", obj->name, obj->u.i);
        break;
    case DP_FLOAT:
        bufAddF(output, "%s: F(%g)", obj->name, obj->u.f);
        break;
    case DP_CONTAINER:
        bufAddF(output, "%s: {", obj->name);
        for (child = listHead(&obj->u.c); child; child = listNext(child)) {
            dump_object(output, child);
        }
        bufAddF(output, " }");
        break;
    }
}

static void run_test(Test *test, DP_Parser *parser)
{
    int r;
    DP_Object *obj;

    char *output;
    List objects = { 0 };

    dpClear(parser);

    r = dpParseString(parser, test->input, &objects);

    if (r == 0) {
        Buffer buf = { 0 };

        for (obj = listHead(&objects); obj; obj = listNext(obj)) {
            dump_object(&buf, obj);
        }

        output = strdup(bufGet(&buf));

        bufReset(&buf);
    }
    else {
        output = strdup(dpError(parser));
    }

    if (r != test->status) {
        errors++;

        fprintf(stderr, "Test failed:\n");
        fprintf(stderr, "\tInput:    \"%s\"\n", test->input);
        fprintf(stderr, "\tReturned: %d\n", r);
        fprintf(stderr, "\tExpected: %d\n", test->status);
    }

    if (r != test->status || strcmp(output, test->output) != 0) {
        errors++;

        fprintf(stderr, "Test failed:\n");
        fprintf(stderr, "\tInput:    \"%s\"\n", test->input);
        fprintf(stderr, "\tOutput:   \"%s\"\n", output);
        fprintf(stderr, "\tExpected: \"%s\"\n", test->output);
    }

    free(output);
}

int main(int argc, char *argv[])
{
    int i;

    DP_Parser *parser = dpCreate();

    for (i = 0; i < num_tests; i++) {
        run_test(test + i, parser);
    }

    dpFree(parser);

    return errors;
}

#endif
