/*
 * dp.c: Description
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
    DP_PS_NAME,
    DP_PS_STRING,
    DP_PS_ESCAPE,
    DP_PS_INT,
    DP_PS_FLOAT,
    DP_PS_ERROR,
    DP_PS_EOF
} DP_ParserState;

struct DP_Parser {
    DP_ParserType type;
    DP_ParserState state;
    Buffer name, value, error;
    int stack_depth;
    List *stack;
    int line;
    union {
        FILE *fp;
        const char *str;
    } u;
};

static void dp_unget_char(DP_Parser *parser, int c)
{
    if (c == EOF) return;

    if (parser->type == DP_PT_STR) {
        parser->u.str--;

        assert(c == *parser->u.str);
    }
    else {
        ungetc(c, parser->u.fp);
    }
}

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

static void dp_push(DP_Parser *parser, List *list)
{
    parser->stack_depth++;

    parser->stack = realloc(parser->stack, parser->stack_depth * sizeof(List));

    memcpy(parser->stack + parser->stack_depth - 1, list, sizeof(List));
}

static List *dp_pop(DP_Parser *parser)
{
    List *top_of_stack;

    if (parser->stack_depth == 0) {
        return NULL;
    }

    top_of_stack = parser->stack + parser->stack_depth - 1;

    assert(parser->stack_depth > 0);

    parser->stack_depth--;

    parser->stack = realloc(parser->stack, parser->stack_depth * sizeof(List));

    return top_of_stack;
}

static DP_Object *dp_add_object(DP_Parser *parser, DP_Type type, List *objects)
{
    char *end = NULL;
    DP_Object *obj;

    const char *name  = bufGet(&parser->name);
    const char *value = bufGet(&parser->value);

    obj = calloc(1, sizeof(DP_Object));

    obj->line = parser->line;
    obj->type = type;

    switch(type) {
    case DP_STRING:
        obj->u.s = strdup(value);
        break;
    case DP_INT:
        obj->u.i = strtol(value, &end, 0);
        if (end < value + strlen(value)) {
            bufSetF(&parser->error, "%d: couldn't parse \"%s\"", parser->line, value);
            dpFreeObject(obj);
            return NULL;
        }
        break;
    case DP_FLOAT:
        obj->u.f = strtod(value, &end);
        if (end < value + strlen(value)) {
            bufSetF(&parser->error, "%d: couldn't parse \"%s\"", parser->line, value);
            dpFreeObject(obj);
            return NULL;
        }
        break;
    default:
        break;
    }

    if (name == NULL || strlen(name) == 0) {
        bufSetF(&parser->error, "%d: name expected", parser->line);
        return NULL;
    }

    obj->name = strdup(name);

    listAppendTail(objects, obj);

    return obj;
}

static int dp_parse(DP_Parser *parser, List *objects)
{
    DP_Object *obj;

    parser->line = 1;

    while (1) {
        int c = dp_get_char(parser);

        switch(parser->state) {
        case DP_PS_ERROR:
            return 1;
        case DP_PS_EOF:
            return 0;
        case DP_PS_NONE:
            if (c == '_' || isalpha(c)) {
                bufSetC(&parser->name, c);
                parser->state = DP_PS_NAME;
            }
            else if (c == '"') {
                bufClear(&parser->value);
                parser->state = DP_PS_STRING;
            }
            else if (isdigit(c) || c == '-' || c == '+') {
                bufSetC(&parser->value, c);
                parser->state = DP_PS_INT;
            }
            else if (c == '.') {
                bufSetC(&parser->value, c);
                parser->state = DP_PS_FLOAT;
            }
            else if (c == '{') {
                obj = dp_add_object(parser, DP_CONTAINER, objects);
                dp_push(parser, objects);
                objects = &obj->u.c;
            }
            else if (c == '}') {
                if ((objects = dp_pop(parser)) == NULL) {
                    bufSetF(&parser->error, "%d: unbalanced '}'", parser->line);
                    parser->state = DP_PS_ERROR;
                }
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
                        parser->line, c, bufGet(&parser->value));
                parser->state = DP_PS_ERROR;
            }
            break;
        case DP_PS_STRING:
            if (c == '\\') {
                parser->state = DP_PS_ESCAPE;
            }
            else if (c == '"') {
                dp_add_object(parser, DP_STRING, objects);
                parser->state = DP_PS_NONE;
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
            }
            else if (c == 'r') {
                bufAddC(&parser->value, '\r');
            }
            else if (c == 'n') {
                bufAddC(&parser->value, '\n');
            }
            else if (c == '\\') {
                bufAddC(&parser->value, '\\');
            }
            else if (c == EOF) {
                bufSetF(&parser->error, "%d: unexpected end of file in escape sequence",
                        parser->line);
                parser->state = DP_PS_ERROR;
            }
            parser->state = DP_PS_STRING;
            break;
        case DP_PS_INT:
            if (isxdigit(c) || c == 'x') {
                bufAddC(&parser->value, c);
            }
            else if (c == '.' || c == 'e' || c == 'E') {
                bufAddC(&parser->value, c);
                parser->state = DP_PS_FLOAT;
            }
            else if (isspace(c) || c == '{' || c == '}' || c == EOF) {
                dp_unget_char(parser, c);
                if (dp_add_object(parser, DP_INT, objects) == NULL) {
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
        case DP_PS_FLOAT:
            if (isdigit(c) || c == '+' || c == '-' || c == '.' || c == 'e' || c == 'E') {
                bufAddC(&parser->value, c);
            }
            else if (isspace(c) || c == '{' || c == '}' || c == EOF) {
                dp_unget_char(parser, c);
                if (dp_add_object(parser, DP_FLOAT, objects) == NULL) {
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
        }
    }

    if (parser->stack_depth > 1) {
        bufSetF(&parser->error, "%d: unbalanced '{'", parser->line);
        return 1;
    }

    return 0;
}

DP_Parser *dpCreate(void)
{
    DP_Parser *parser = calloc(1, sizeof(DP_Parser));

    return parser;
}

void dpClear(DP_Parser *parser)
{
    if (parser->type == DP_PT_FILE || parser->type == DP_PT_FD) {
        fclose(parser->u.fp);
    }

    if (parser->stack != NULL) {
        free(parser->stack);
    }

    bufReset(&parser->name);
    bufReset(&parser->value);
    bufReset(&parser->error);

    memset(parser, 0, sizeof(DP_Parser));
}

void dpFree(DP_Parser *parser)
{
    dpClear(parser);

    free(parser);
}

int dpParseFile(DP_Parser *parser, const char *filename, List *objects)
{
    parser->type = DP_PT_FILE;

    if ((parser->u.fp = fopen(filename, "r")) == NULL) {
        bufSetF(&parser->error, "Couldn't open file \"%s\": %s", filename, strerror(errno));
        return 1;
    }
    else {
        return dp_parse(parser, objects);
    }
}

int dpParseFP(DP_Parser *parser, FILE *fp, List *objects)
{
    parser->type = DP_PT_FP;
    parser->u.fp = fp;

    return dp_parse(parser, objects);
}

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

int dpParseString(DP_Parser *parser, const char *string, List *objects)
{
    parser->type = DP_PT_STR;
    parser->u.str = string;

    return dp_parse(parser, objects);
}

const char *dpError(DP_Parser *parser)
{
    return bufGet(&parser->error);
}

void dpFreeObject(DP_Object *obj)
{
    if (obj->type == DP_STRING && obj->u.s != NULL) free(obj->u.s);
    if (obj->name != NULL) free(obj->name);

    free(obj);
}

#ifdef TEST

static int errors = 0;

typedef struct {
    const char *input;
    int status;
    const char *output;
} Test;

Test test[] = {
    { "Test1 123", 0, "Test1: I(123)" },
    { "Test2 1.3", 0, "Test2: F(1.3)" },
    { "Test3 \"ABC\"", 0, "Test3: S(ABC)" },
    { "Test4 { Test4a 123 Test4b 1.3 Test4c \"ABC\" }",
      0,
      "Test4: { Test4a: I(123) Test4b: F(1.3) Test4c: S(ABC) }" },
    { "123ABC", 1, "1: couldn't parse \"123ABC\"" },
    { "123 123", 1, "1: name expected" },
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
        bufAddF(output, "%s: I(%d)", obj->name, obj->u.i);
        break;
    case DP_FLOAT:
        bufAddF(output, "%s: F(%g)", obj->name, obj->u.f);
        break;
    case DP_CONTAINER:
        bufAddF(output, "%s: {", obj->name);
        for (child = listHead(&obj->u.c); child; child = listNext(child)) {
            dump_object(output, child);
        }
        bufAddF(output, " }", obj->name);
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
