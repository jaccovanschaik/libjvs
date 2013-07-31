/*
 * dp.c: Data parser
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "buffer.h"

#include "dp.h"

typedef enum {
    DP_STATE_NONE,
    DP_STATE_COMMENT,
    DP_STATE_NAME,
    DP_STATE_STRING,
    DP_STATE_ESCAPE,
    DP_STATE_NUMBER,
    DP_STATE_ERROR,
    DP_STATE_END
} DP_State;

typedef enum {
    DP_ST_NONE,
    DP_ST_FILE,
    DP_ST_FD,
    DP_ST_FP,
    DP_ST_STRING
} DP_StreamType;

struct DP_Stream {
    DP_StreamType type;
    Buffer error;
    char *file;
    int line;
    union {
        FILE *fp;
        const char *str;
    } u;
};

/*
 * Push back character <c> onto the input stream.
 */
static void dp_unget_char(DP_Stream *stream, int c)
{
    if (c == EOF) return;

    if (stream->type == DP_ST_STRING) {
        stream->u.str--;

        /* We can't overwrite the string that the user has given us sinze it's declared const, so
         * we'll just assert that the character we're pushing back is the same that was already
         * there. */

        assert(c == *stream->u.str);
    }
    else {
        ungetc(c, stream->u.fp);
    }

    if (c == '\n') stream->line--;
}

/*
 * Get a character from the input stream.
 */
static int dp_get_char(DP_Stream *stream)
{
    int c;

    if (stream->type == DP_ST_STRING) {
        c = *stream->u.str;

        if (c == '\0')
            c = EOF;
        else
            stream->u.str++;
    }
    else {
        c = fgetc(stream->u.fp);
    }

    /* Squish any end-of-line sequence into just a line feed. */

    if (c == '\r') {
        int c2 = dp_get_char(stream);

        if (c2 != '\n') {
            dp_unget_char(stream, c2);
        }

        c = '\n';
    }

    if (c == '\n') stream->line++;

    return c;
}

/*
 * Generate an error message about unexpected character <c> (which may be EOF) on <stream>.
 */
static void dp_unexpected(DP_Stream *stream, int c)
{
    bufSetF(&stream->error, "%s:%d: unexpected ", stream->file, stream->line);

    if (c == EOF) {
        bufAddF(&stream->error, "end of file");
    }
    else {
        bufAddF(&stream->error, "character '%c' (ascii %d)", c, c);
    }
}

/*
 * Interpret <value> as a number and update <obj> with what we think it is.
 */
static int dp_interpret_number(const char *value, DP_Object *obj)
{
    long i;
    double f;

    char *end;

    i = strtol(value, &end, 0);

    if (end == value + strlen(value)) {
        obj->type = DP_INT;
        obj->u.i = i;

        return 0;
    }

    f = strtod(value, &end);

    if (end == value + strlen(value)) {
        obj->type = DP_FLOAT;
        obj->u.f = f;

        return 0;
    }

    return -1;
}

/*
 * Create and return a DP_Object of type <type>.
 */
static DP_Object *dp_new_object(DP_Type type)
{
    DP_Object *obj = calloc(1, sizeof(DP_Object));

    obj->type = type;

    return obj;
}

/*
 * Create an object with type <type> and name <name>, which was found in file <file> on line <line>.
 * <root> and <last> are pointers to the addresses of the first and last elements in the list to
 * which the new object must be added.
 */
static DP_Object *dp_add_object(DP_Type type, const Buffer *name,
                                const char *file, int line,
                                DP_Object **root, DP_Object **last)
{
    DP_Object *obj = dp_new_object(type);

    obj->line = line;
    obj->file = file;

    /* If a name was given use it. Otherwise, if there is a preceding object copy its name.
     * Otherwise simply set the name to NULL. */

    if (name != NULL && bufLen(name) != 0)
        obj->name = strdup(bufGet(name));
    else if (*last != NULL && (*last)->name != NULL)
        obj->name = strdup((*last)->name);

    /* If there is no preceding object, this new object is the first in a new list which makes it
     * the root of the new list. */

    if (*last == NULL)
        *root = obj;
    else
        (*last)->next = obj;

    /* In any case, this object is the last in the current list. */

    *last = obj;

    return obj;
}

/*
 * Parse input stream <stream> and return the first element in the list of objects that was found.
 * <nesting_level> is the nesting level (w.r.t. braces) we're currently at.
 */
static DP_Object *dp_parse(DP_Stream *stream, int nesting_level)
{
    int c;

    DP_State state = DP_STATE_NONE;     /* Current parser state. */

    DP_Object *root = NULL;             /* Root (i.e. first) element of current object list. */
    DP_Object *last = NULL;             /* Last added element of current object list. */

    Buffer name = { 0 };                /* Name of current object. */
    Buffer value = { 0 };               /* Value of current object. */

    while (1) {
        if (state == DP_STATE_ERROR || state == DP_STATE_END) break;

        c = dp_get_char(stream);

        switch(state) {
        case DP_STATE_NONE:
            /* We're in whitespace. Let's see what we find. */

            if (c =='#') {
                state = DP_STATE_COMMENT;
            }
            else if (isalpha(c) || c == '_') {
                bufSetC(&name, c);
                state = DP_STATE_NAME;
            }
            else if (c == '+' || c == '-' || c == '.' || isdigit(c)) {
                bufSetC(&value, c);
                state = DP_STATE_NUMBER;
            }
            else if (c == '"') {
                state = DP_STATE_STRING;
                bufClear(&value);
            }
            else if (c == '{') {
                /* Parse the remainder of this container, including the closing brace. */

                DP_Object *obj =
                    dp_add_object(DP_CONTAINER, &name, stream->file, stream->line, &root, &last);

                if ((obj->u.c = dp_parse(stream, nesting_level + 1)) == NULL) {
                    state = DP_STATE_ERROR;
                }
            }
            else if (c == '}') {
                if (nesting_level > 0) {    /* OK, end of this container. */
                    state = DP_STATE_END;
                }
                else {                      /* We're at top-level, what's this doing here?! */
                    bufSetF(&stream->error, "%s:%d: unbalanced '}'", stream->file, stream->line);
                    state = DP_STATE_ERROR;
                }
            }
            else if (c == EOF && nesting_level == 0) {
                state = DP_STATE_END;
            }
            else if (!isspace(c)) {
                dp_unexpected(stream, c);
                state = DP_STATE_ERROR;
            }
            else {
                /* OK, another whitespace character. Just keep going. */
            }
            break;
        case DP_STATE_COMMENT:
            /* We're in a comment. Just keep going until the end of the line, or the file. */

            if (c == '\n') {
                state = DP_STATE_NONE;
            }
            else if (c == EOF) {
                state = DP_STATE_END;
            }
            else {
                /* Anything else just adds to the comment. */
            }
            break;
        case DP_STATE_NAME:
            /* We're in a name. We'll accept underscores, letters and digits. */

            if (c == '_' || isalnum(c)) {
                bufAddC(&name, c);
            }
            else if (isspace(c) || c == '{' || c == '}') {
                dp_unget_char(stream, c);
                state = DP_STATE_NONE;
            }
            else {
                dp_unexpected(stream, c);
                state = DP_STATE_ERROR;
            }
            break;
        case DP_STATE_STRING:
            /* We're in a string. Keep going until the closing quote (but: escape sequences!) */

            if (c == '\\') {
                state = DP_STATE_ESCAPE;
            }
            else if (c == '"') {
                DP_Object *obj =
                    dp_add_object(DP_STRING, &name, stream->file, stream->line, &root, &last);

                obj->u.s = strdup(bufGet(&value));
                state = DP_STATE_NONE;
            }
            else if (isprint(c)) {
                bufAddC(&value, c);
            }
            else {
                dp_unexpected(stream, c);
                state = DP_STATE_ERROR;
            }
            break;
        case DP_STATE_ESCAPE:
            /* Escape sequence encountered. Interpret it. */

            if (c == 't') {
                bufAddC(&value, '\t');
                state = DP_STATE_STRING;
            }
            else if (c == 'r') {
                bufAddC(&value, '\r');
                state = DP_STATE_STRING;
            }
            else if (c == 'n') {
                bufAddC(&value, '\n');
                state = DP_STATE_STRING;
            }
            else if (c == '\\') {
                bufAddC(&value, '\\');
                state = DP_STATE_STRING;
            }
            else {
                bufSetF(&stream->error,
                        "%s:%d: invalid escape sequence \"\\%c\"", stream->file, stream->line, c);
                state = DP_STATE_ERROR;
            }
            break;
        case DP_STATE_NUMBER:
            /* A number (float or int). Accept anything that can occur in a number (floating point,
             * octal or hex) and let strtod or strtol sort it out later. */

            if (isxdigit(c) || c == 'x' || c == '.' ||
                c == 'e' || c == 'E' || c == '+' || c == '-') {
                bufAddC(&value, c);
            }
            else if (isspace(c) || c == '{' || c == '}' || c == EOF) {
                DP_Object *obj =
                    dp_add_object(DP_INT, &name, stream->file, stream->line, &root, &last);

                if (dp_interpret_number(bufGet(&value), obj) == 0) {
                    state = DP_STATE_NONE;
                }
                else {
                    bufSetF(&stream->error, "%s:%d: unrecognized value \"%s\"",
                            stream->file, stream->line, bufGet(&value));
                    state = DP_STATE_ERROR;
                }

                dp_unget_char(stream, c);
            }
            else {
                dp_unexpected(stream, c);
                state = DP_STATE_ERROR;
            }
            break;
        default:
            abort();
        }
    }

    bufReset(&name);
    bufReset(&value);

    if (state == DP_STATE_ERROR) {
        dpFree(root);
        return NULL;
    }
    else {
        return root;
    }
}

/*
 * Create and return a stream of type <type>.
 */
static DP_Stream *dp_create_stream(DP_StreamType type)
{
    DP_Stream *stream = calloc(1, sizeof(DP_Stream));

    stream->type = type;

    return stream;
}

/*
 * Describe the type of stream <stream>.
 */
static char *dp_describe_stream(DP_Stream *stream)
{
    struct stat statbuf;

    if (stream->type == DP_ST_STRING) {
        return "<string>";
    }

    fstat(fileno(stream->u.fp), &statbuf);

    if (S_ISREG(statbuf.st_mode))
        return "<file>";
    else if (S_ISCHR(statbuf.st_mode) || S_ISBLK(statbuf.st_mode))
        return "<device>";
    else if (S_ISFIFO(statbuf.st_mode))
        return "<fifo>";
    else if (S_ISSOCK(statbuf.st_mode))
        return "<socket>";
    else
        return "<unknown>";
}

/*
 * Create and return a DP_Stream, using data from file <filename>.
 */
DP_Stream *dpOpenFile(const char *filename)
{
    DP_Stream *stream = dp_create_stream(DP_ST_FILE);

    if ((stream->u.fp = fopen(filename, "r")) == NULL) {
        dpClose(stream);
        return NULL;
    }

    stream->file = strdup(filename);

    return stream;
}

/*
 * Create and return a DP_Stream, using data from FILE pointer <fp>.
 */
DP_Stream *dpOpenFP(FILE *fp)
{
    DP_Stream *stream = dp_create_stream(DP_ST_FP);

    stream->u.fp = fp;
    stream->file = strdup(dp_describe_stream(stream));

    return stream;
}

/*
 * Create and return a DP_Stream, using data from file descriptor <fd>.
 */
DP_Stream *dpOpenFD(int fd)
{
    DP_Stream *stream = dp_create_stream(DP_ST_FD);

    if ((stream->u.fp = fdopen(fd, "r")) == NULL) {
        dpClose(stream);
        return NULL;
    }

    stream->file = strdup(dp_describe_stream(stream));

    return stream;
}

/*
 * Create and return a DP_Stream, using data from string <string>.
 */
DP_Stream *dpOpenString(const char *string)
{
    DP_Stream *stream = dp_create_stream(DP_ST_STRING);

    stream->u.str = string;
    stream->file = strdup(dp_describe_stream(stream));

    return stream;
}

/*
 * Parse <stream>, returning the first of the found objects.
 */
DP_Object *dpParse(DP_Stream *stream)
{
    if (stream == NULL) return NULL;

    stream->line = 1;

    return dp_parse(stream, 0);
}

/*
 * Return the type of <obj> as a string.
 */
const char *dpType(const DP_Object *obj)
{
    switch(obj->type) {
    case DP_STRING:
        return "string";
    case DP_INT:
        return "int";
    case DP_FLOAT:
        return "float";
    case DP_CONTAINER:
        return "container";
    default:
        return NULL;
    }
}

/*
 * Retrieve an error text from <stream>, in case any function has returned an error.
 */
const char *dpError(const DP_Stream *stream)
{
    if (stream == NULL)
        return NULL;
    else
        return bufGet(&stream->error);
}

/*
 * Free the object list starting at <root>.
 */
void dpFree(DP_Object *root)
{
    while (root != NULL) {
        DP_Object *next = root->next;

        if (root->name != NULL)
            free(root->name);

        if (root->type == DP_STRING && root->u.s != NULL)
            free(root->u.s);
        else if (root->type == DP_CONTAINER && root->u.c != NULL)
            dpFree(root->u.c);

        free(root);

        root = next;
    }
}

/*
 * Close <stream> and free all its memory.
 */
void dpClose(DP_Stream *stream)
{
    if (stream->type == DP_ST_FILE || stream->type == DP_ST_FD) {
        if (stream->u.fp != NULL) fclose(stream->u.fp);
    }

    bufReset(&stream->error);
    free(stream->file);

    free(stream);
}

#ifdef TEST
static void dump(DP_Object *obj, Buffer *buf)
{
    while (obj != NULL) {
        if (bufLen(buf) > 0) bufAddC(buf, ' ');

        bufAddF(buf, "%s ", obj->name);

        switch(obj->type) {
        case DP_STRING:
            bufAddF(buf, "\"%s\"", obj->u.s);
            break;
        case DP_INT:
            bufAddF(buf, "%ld", obj->u.i);
            break;
        case DP_FLOAT:
            bufAddF(buf, "%g", obj->u.f);
            break;
        case DP_CONTAINER:
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

    DP_Stream *stream = dpOpenString(test->input);
    DP_Object *object = dpParse(stream);

    dump(object, &output);

    if (test->error) {
        if (object != NULL) {
            fprintf(stderr, "Test %d:\n", index);
            fprintf(stderr, "\texpected error \"%s\"\n", test->output);
            fprintf(stderr, "\tgot output \"%s\"\n", bufGet(&output));

            errors++;
        }
        else if (strcmp(dpError(stream), test->output) != 0) {
            fprintf(stderr, "Test %d:\n", index);
            fprintf(stderr, "\texpected error \"%s\"\n", test->output);
            fprintf(stderr, "\tgot error \"%s\"\n", dpError(stream));

            errors++;
        }
    }
    else {
        if (object == NULL) {
            fprintf(stderr, "Test %d:\n", index);
            fprintf(stderr, "\texpected output \"%s\"\n", test->output);
            fprintf(stderr, "\tgot error \"%s\"\n", dpError(stream));

            errors++;
        }
        else if (strcmp(bufGet(&output), test->output) != 0) {
            fprintf(stderr, "Test %d:\n", index);
            fprintf(stderr, "\texpected output \"%s\"\n", test->output);
            fprintf(stderr, "\tgot output \"%s\"\n", bufGet(&output));

            errors++;
        }
    }

    dpClose(stream);
    dpFree(object);
    bufReset(&output);
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
    { 1, "123ABC",                 "<string>:1: unrecognized value \"123ABC\"" },
    { 1, "123XYZ",                 "<string>:1: unexpected character 'X' (ascii 88)" },
    { 1, "ABC$",                   "<string>:1: unexpected character '$' (ascii 36)" },
    { 1, "123$",                   "<string>:1: unexpected character '$' (ascii 36)" },
    { 1, "Test {\n\tTest1 123\n\tTest2 1.3\n\tTest3 \"ABC\\0\"\n}",
         "<string>:4: invalid escape sequence \"\\0\"" },
    { 1, "Test { Test2 { Test3 123 Test4 1.3 Test5 \"ABC\" }",
         "<string>:1: unexpected end of file" },
    { 1, "Test { Test1 123 Test2 1.3 Test3 \"ABC\" } }",
         "<string>:1: unbalanced '}'" },
};

static int num_tests = sizeof(test) / sizeof(test[0]);

int main(int argc, char *argv[])
{
    int i;

    for (i = 0; i < num_tests; i++) {
        do_test(i, test + i);
    }

    return errors;
}
#endif
