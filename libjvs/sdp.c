/*
 * sdp.c: Simple Data Parser.
 *
 * Author:	Jacco van Schaik (jacco.van.schaik@dnw.aero)
 * Copyright:	(c) 2013 DNW German-Dutch Windtunnels
 * Created:	2013-07-03
 * Version:	$Id$
 */

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "utils.h"
#include "buffer.h"

#include "sdp.h"

/*
 * The types of "SDP" there are.
 */
typedef enum {
    SDP_SOURCE_NONE,
    SDP_SOURCE_FILE,
    SDP_SOURCE_FD,
    SDP_SOURCE_STR
} SDP_SourceType;

/*
 * The states of the parser state machine.
 */
typedef enum {
    SDP_STATE_NONE,
    SDP_STATE_USTRING,
    SDP_STATE_QSTRING,
    SDP_STATE_LONG,
    SDP_STATE_DOUBLE,
    SDP_STATE_EXPONENT,
    SDP_STATE_SCIENTIFIC,
    SDP_STATE_ESCAPE,
    SDP_STATE_COMMENT,
    SDP_STATE_EOF,
    SDP_STATE_ERROR
} SDP_State;

/*
 * Advance declaration of an SDP type.
 */
typedef struct SDP SDP;

/*
 * An SDP contains all the data needed to parse an input file.
 */
struct SDP {
    SDP_SourceType type;
    int (*get_char)(SDP *sdp);
    Buffer unget_buf, value_buf;
    List **stack;
    int stack_size;
    SDP_State state, saved_state;
    int cur_line;
    union {
        FILE *fp;
        int fd;
        const char *str;
    } u;
};

/*
 * Global buffer for error messages.
 */
static Buffer error_msg = { 0 };

/*
 * Get a character from <sdp>, if <sdp> has type SDP_SOURCE_FILE.
 */
static int sdp_get_char_file(SDP *sdp)
{
    int c = fgetc(sdp->u.fp);

    if (c != EOF)
        return c;
    else if (errno == 0)
        return 0;
    else
        return -1;
}

/*
 * Get a character from <sdp>, if <sdp> has type SDP_SOURCE_FD.
 */
static int sdp_get_char_fd(SDP *sdp)
{
    char p;

    int n = read(sdp->u.fd, &p, 1);

    return n == 1 ? p : n;
}

/*
 * Get a character from <sdp>, if <sdp> has type SDP_SOURCE_STR.
 */
static int sdp_get_char_str(SDP *sdp)
{
    int c = *sdp->u.str;

    if (c != 0) {
        sdp->u.str++;
    }

    return c;
}

/*
 * Push back character <c> into <sdp>. We use a special "unget buffer" to hold these instead of
 * relying on a (possibly non-existing) unget mechanism from the underlying data source.
 */
static void sdp_unget_char(SDP *sdp, int c)
{
    bufAddC(&sdp->unget_buf, c);
}

/*
 * Get a character from <sdp>. Take into account possibly earlier "ungotten" characters, and also
 * replace any kind of end-of-line terminator with a single line feed.
 */
static int sdp_get_char(SDP *sdp)
{
    int c;

    if (bufLen(&sdp->unget_buf) > 0) {
        c = bufGet(&sdp->unget_buf)[bufLen(&sdp->unget_buf) - 1];
        bufTrim(&sdp->unget_buf, 0, 1);
    }
    else if ((c = sdp->get_char(sdp)) == '\n') {
        sdp->cur_line++;
    }
    else if (c == '\r') {
        int c2 = sdp->get_char(sdp);

        if (c2 != '\n') {
            sdp_unget_char(sdp, c2);
        }

        c = '\n';
        sdp->cur_line++;
    }

    return c;
}

/*
 * Push an SDP_object list onto the stack in <sdp>.
 */
static void sdp_push_list(SDP *sdp, List *list)
{
    sdp->stack_size++;

    sdp->stack = realloc(sdp->stack, sdp->stack_size * sizeof(List *));

    sdp->stack[sdp->stack_size - 1] = list;
}

/*
 * Pop an SDP_object list from the stack in <sdp>.
 */
static List *sdp_pop_list(SDP *sdp)
{
    List *top = sdp->stack[sdp->stack_size - 1];

    sdp->stack_size--;

    sdp->stack = realloc(sdp->stack, sdp->stack_size * sizeof(List *));

    return top;
}

/*
 * Add an object with type <type>, found on line <line> and with value <value> to the end of list
 * <list>.
 */
static SDP_Object *sdp_add_object(SDP_Type type, const char *value, int line, List *list)
{
    SDP_Object *obj = calloc(1, sizeof(SDP_Object));

    obj->type = type;
    obj->line = line;

    switch(type) {
    case SDP_STRING:
        obj->u.s = strdup(value);
        break;
    case SDP_LONG:
        obj->u.l = atol(value);
        break;
    case SDP_DOUBLE:
        obj->u.d = atof(value);
        break;
    case SDP_CONTAINER:
        break;
    }

    listAppendTail(list, obj);

    return obj;
}

/*
 * Parse data from <sdp> and add the resulting objects to the end of <objects>.
 */
static int sdp_read(SDP *sdp, List *objects)
{
    int ret_code;

    sdp->stack = calloc(1, sizeof(List *));
    sdp->stack[0] = objects;
    sdp->stack_size = 1;

    sdp->state = SDP_STATE_NONE;
    sdp->cur_line = 1;

    while (sdp->state != SDP_STATE_EOF && sdp->state != SDP_STATE_ERROR) {
        int c = sdp_get_char(sdp);

        List *cur_list = sdp->stack[sdp->stack_size - 1];

        switch(sdp->state) {
        case SDP_STATE_NONE:
            if (isspace(c)) {
                continue;
            }
            else if (isalpha(c) || c == '_') {
                bufSetC(&sdp->value_buf, c);
                sdp->state = SDP_STATE_USTRING;
            }
            else if (isalpha(c) || c == '"') {
                bufClear(&sdp->value_buf);
                sdp->state = SDP_STATE_QSTRING;
            }
            else if (isdigit(c) || c == '-' || c == '+') {
                bufSetC(&sdp->value_buf, c);
                sdp->state = SDP_STATE_LONG;
            }
            else if (c == '{') {
                SDP_Object *obj = sdp_add_object(SDP_CONTAINER, NULL, sdp->cur_line, cur_list);
                sdp_push_list(sdp, &obj->u.c);
            }
            else if (c == '}') {
                if (sdp->stack_size == 1) {
                    bufSetF(&error_msg, "%d: Unmatched close brace.", sdp->cur_line);
                    sdp->state = SDP_STATE_ERROR;
                }
                sdp_pop_list(sdp);
            }
            else if (c == '#') {
                sdp->state = SDP_STATE_COMMENT;
            }
            else if (c == 0) {
                if (sdp->stack_size != 1) {
                    bufSetF(&error_msg, "%d: Unmatched open brace.", sdp->cur_line);
                    sdp->state = SDP_STATE_ERROR;
                }
                else {
                    sdp->state = SDP_STATE_EOF;
                }
            }
            else {
                bufSetF(&error_msg, "%d: Unexpected character '%c' (ascii %d).",
                        sdp->cur_line, c, c);
                sdp->state = SDP_STATE_ERROR;
            }
            break;
        case SDP_STATE_USTRING:
            if (isspace(c)) {
                sdp_add_object(SDP_STRING, bufGet(&sdp->value_buf), sdp->cur_line, cur_list);
                bufClear(&sdp->value_buf);
                sdp->state = SDP_STATE_NONE;
            }
            else if (isalnum(c) || c == '_') {
                bufAddC(&sdp->value_buf, c);
            }
            else if (c == '\\') {
                sdp->saved_state = sdp->state;
                sdp->state = SDP_STATE_ESCAPE;
            }
            else if (c == '{') {
                sdp_add_object(SDP_STRING, bufGet(&sdp->value_buf), sdp->cur_line, cur_list);
                sdp_unget_char(sdp, c);
                sdp->state = SDP_STATE_NONE;
            }
            else if (c == '}') {
                sdp_add_object(SDP_STRING, bufGet(&sdp->value_buf), sdp->cur_line, cur_list);
                sdp_unget_char(sdp, c);
                sdp->state = SDP_STATE_NONE;
            }
            else if (c == 0) {
                sdp_add_object(SDP_STRING, bufGet(&sdp->value_buf), sdp->cur_line, cur_list);
                bufClear(&sdp->value_buf);
                sdp->state = SDP_STATE_EOF;
            }
            else {
                bufSetF(&error_msg, "%d: Unexpected character '%c' following \"%s\".",
                        sdp->cur_line, c, bufGet(&sdp->value_buf));
                sdp->state = SDP_STATE_ERROR;
            }
            break;
        case SDP_STATE_QSTRING:
            if (c == '"') {
                sdp_add_object(SDP_STRING, bufGet(&sdp->value_buf), sdp->cur_line, cur_list);
                bufClear(&sdp->value_buf);
                sdp->state = SDP_STATE_NONE;
            }
            else if (c == '\\') {
                sdp->saved_state = sdp->state;
                sdp->state = SDP_STATE_ESCAPE;
            }
            else if (isprint(c)) {
                bufAddC(&sdp->value_buf, c);
            }
            else if (c == 0) {
                bufSetF(&error_msg, "%d: String not terminated.", sdp->cur_line);
                sdp->state = SDP_STATE_ERROR;
            }
            else {
                bufSetF(&error_msg, "%d: Unexpected character '%c' following \"%s\".",
                        sdp->cur_line, c, bufGet(&sdp->value_buf));
                sdp->state = SDP_STATE_ERROR;
            }
            break;
        case SDP_STATE_LONG:
            if (isdigit(c)) {
                bufAddC(&sdp->value_buf, c);
            }
            else if (c == '.') {
                bufAddC(&sdp->value_buf, c);
                sdp->state = SDP_STATE_DOUBLE;
            }
            else if (c == 'e' || c == 'E') {
                bufAddC(&sdp->value_buf, c);
                sdp->state = SDP_STATE_EXPONENT;
            }
            else if (isspace(c)) {
                sdp_add_object(SDP_LONG, bufGet(&sdp->value_buf), sdp->cur_line, cur_list);
                sdp->state = SDP_STATE_NONE;
            }
            else if (c == '{') {
                sdp_add_object(SDP_LONG, bufGet(&sdp->value_buf), sdp->cur_line, cur_list);
                sdp_unget_char(sdp, c);
                sdp->state = SDP_STATE_NONE;
            }
            else if (c == '}') {
                sdp_add_object(SDP_LONG, bufGet(&sdp->value_buf), sdp->cur_line, cur_list);
                sdp_unget_char(sdp, c);
                sdp->state = SDP_STATE_NONE;
            }
            else if (c == 0) {
                sdp_add_object(SDP_LONG, bufGet(&sdp->value_buf), sdp->cur_line, cur_list);
                sdp->state = SDP_STATE_EOF;
            }
            else {
                bufSetF(&error_msg, "%d: Unexpected character '%c' following \"%s\".",
                        sdp->cur_line, c, bufGet(&sdp->value_buf));
                sdp->state = SDP_STATE_ERROR;
            }
            break;
        case SDP_STATE_DOUBLE:
            if (isdigit(c)) {
                bufAddC(&sdp->value_buf, c);
            }
            else if (c == 'e' || c == 'E') {
                bufAddC(&sdp->value_buf, c);
                sdp->state = SDP_STATE_EXPONENT;
            }
            else if (isspace(c)) {
                sdp_add_object(SDP_DOUBLE, bufGet(&sdp->value_buf), sdp->cur_line, cur_list);
                sdp->state = SDP_STATE_NONE;
            }
            else if (c == '{') {
                sdp_add_object(SDP_DOUBLE, bufGet(&sdp->value_buf), sdp->cur_line, cur_list);
                sdp_unget_char(sdp, c);
                sdp->state = SDP_STATE_NONE;
            }
            else if (c == '}') {
                sdp_add_object(SDP_DOUBLE, bufGet(&sdp->value_buf), sdp->cur_line, cur_list);
                sdp_unget_char(sdp, c);
                sdp->state = SDP_STATE_NONE;
            }
            else if (c == 0) {
                sdp_add_object(SDP_DOUBLE, bufGet(&sdp->value_buf), sdp->cur_line, cur_list);
                sdp->state = SDP_STATE_EOF;
            }
            else {
                bufSetF(&error_msg, "%d: Unexpected character '%c' following \"%s\".",
                        sdp->cur_line, c, bufGet(&sdp->value_buf));
                sdp->state = SDP_STATE_ERROR;
            }
            break;
        case SDP_STATE_EXPONENT:
            if (isdigit(c) || c == '-' || c == '+') {
                bufAddC(&sdp->value_buf, c);
                sdp->state = SDP_STATE_SCIENTIFIC;
            }
            else {
                bufSetF(&error_msg, "%d: Missing exponent following '%c'.",
                        sdp->cur_line, bufGet(&sdp->value_buf)[bufLen(&sdp->value_buf) - 1]);
                sdp->state = SDP_STATE_ERROR;
            }
            break;
        case SDP_STATE_SCIENTIFIC:
            if (isdigit(c)) {
                bufAddC(&sdp->value_buf, c);
            }
            else if (isspace(c)) {
                sdp_add_object(SDP_DOUBLE, bufGet(&sdp->value_buf), sdp->cur_line, cur_list);
                sdp->state = SDP_STATE_NONE;
            }
            else if (c == '{') {
                sdp_add_object(SDP_DOUBLE, bufGet(&sdp->value_buf), sdp->cur_line, cur_list);
                sdp_unget_char(sdp, c);
                sdp->state = SDP_STATE_NONE;
            }
            else if (c == '}') {
                sdp_add_object(SDP_DOUBLE, bufGet(&sdp->value_buf), sdp->cur_line, cur_list);
                sdp_unget_char(sdp, c);
                sdp->state = SDP_STATE_NONE;
            }
            else if (c == 0) {
                sdp_add_object(SDP_DOUBLE, bufGet(&sdp->value_buf), sdp->cur_line, cur_list);
                sdp->state = SDP_STATE_EOF;
            }
            else {
                bufSetF(&error_msg, "%d: Unexpected character '%c' following \"%s\".",
                        sdp->cur_line, c, bufGet(&sdp->value_buf));
                sdp->state = SDP_STATE_ERROR;
            }
            break;
        case SDP_STATE_ESCAPE:
            if (c == 'n') {
                bufAddC(&sdp->value_buf, '\n');
                sdp->state = sdp->saved_state;
            }
            else if (c == 'r') {
                bufAddC(&sdp->value_buf, '\r');
                sdp->state = sdp->saved_state;
            }
            else if (c == 't') {
                bufAddC(&sdp->value_buf, '\t');
                sdp->state = sdp->saved_state;
            }
            else if (c == '\\') {
                bufAddC(&sdp->value_buf, '\\');
                sdp->state = sdp->saved_state;
            }
            else if (c == '"') {
                bufAddC(&sdp->value_buf, '"');
                sdp->state = sdp->saved_state;
            }
            else if (c == 0) {
                bufSetF(&error_msg, "%d: Escape sequence not terminated.", sdp->cur_line);
                sdp->state = SDP_STATE_ERROR;
            }
            else {
                bufSetF(&error_msg, "%d: Invalid escape sequence \"\\%c\".", sdp->cur_line, c);
                sdp->state = SDP_STATE_ERROR;
            }
            break;
        case SDP_STATE_COMMENT:
            if (c == 0) {
                sdp->state = SDP_STATE_EOF;
            }
            else if (c == '\n') {
                sdp->state = SDP_STATE_NONE;
            }
            break;
        default:
            break;
        }
    }

    ret_code = sdp->state == SDP_STATE_EOF ? 0 : 1;

    bufReset(&sdp->value_buf);

    free(sdp->stack);
    free(sdp);

    return ret_code;
}

/*
 * Read SDP_Objects from file <fp> and append them to <objects>.
 */
int sdpReadFile(FILE *fp, List *objects)
{
    SDP *sdp = calloc(1, sizeof(SDP));

    sdp->type = SDP_SOURCE_FILE;
    sdp->get_char = sdp_get_char_file;
    sdp->u.fp = fp;

    return sdp_read(sdp, objects);
}

/*
 * Read SDP_Objects from file descriptor <fd> and append them to <objects>.
 */
int sdpReadFD(int fd, List *objects)
{
    SDP *sdp = calloc(1, sizeof(SDP));

    sdp->type = SDP_SOURCE_FD;
    sdp->get_char = sdp_get_char_fd;
    sdp->u.fd = fd;

    return sdp_read(sdp, objects);
}

/*
 * Read SDP_Objects from string <str> and append them to <objects>.
 */
int sdpReadString(const char *str, List *objects)
{
    SDP *sdp = calloc(1, sizeof(SDP));

    sdp->type = SDP_SOURCE_STR;
    sdp->get_char = sdp_get_char_str;
    sdp->u.str = str;

    return sdp_read(sdp, objects);
}

/*
 * Dump the list of SDP_Objects in <objects> on <fp>, indented by <indent> levels.
 */
void sdpDump(FILE *fp, int indent, List *objects)
{
    SDP_Object *obj;

    static char *type_name[] = {
        "String",
        "Long",
        "Double",
        "Container"
    };

    for (obj = listHead(objects); obj; obj = listNext(obj)) {
        ifprintf(fp, indent, "%s object: ", type_name[obj->type]);

        switch(obj->type) {
        case SDP_STRING:
            fprintf(fp, "\"%s\"\n", obj->u.s);
            break;
        case SDP_LONG:
            fprintf(fp, "%ld\n", obj->u.l);
            break;
        case SDP_DOUBLE:
            fprintf(fp, "%f\n", obj->u.d);
            break;
        case SDP_CONTAINER:
            fprintf(fp, "\n");
            sdpDump(fp, indent + 1, &obj->u.c);
            break;
        }
    }
}

/*
 * Get the error that occurred as indicated by one of the sdpRead... functions returning -1. The
 * error message that is returned is dynamically allocated and should be free'd by the caller.
 */
char *sdpError(void)
{
    return bufDetach(&error_msg);
}

/*
 * Clear the list of SDP_Objects at <objects>. Clears the contents of the list but leaves the list
 * itself alone.
 */
void sdpClear(List *objects)
{
    SDP_Object *obj;

    while ((obj = listRemoveHead(objects)) != NULL) {
        if (obj->type == SDP_CONTAINER) {
            sdpClear(&obj->u.c);
        }
        else if (obj->type == SDP_STRING) {
            free(obj->u.s);
        }

        free(obj);
    }
}

/*
 * Clear and free the list of SDP_Objects at <objects>.
 */
void sdpFree(List *objects)
{
    sdpClear(objects);
    free(objects);
}

#ifdef TEST
static int errors = 0;

/*
 * Dump a simple representation of the objects in <objects> to <buf>.
 */
static void my_dump(List *objects, Buffer *buf)
{
    SDP_Object *obj;

    for (obj = listHead(objects); obj; obj = listNext(obj)) {
        if (obj != listHead(objects)) {
            bufAddC(buf, ' ');
        }

        switch(obj->type) {
        case SDP_STRING:
            bufAddF(buf, "S(%s)", obj->u.s);
            break;
        case SDP_LONG:
            bufAddF(buf, "L(%ld)", obj->u.l);
            break;
        case SDP_DOUBLE:
            bufAddF(buf, "D(%g)", obj->u.d);
            break;
        case SDP_CONTAINER:
            bufAddF(buf, "C(");
            my_dump(&obj->u.c, buf);
            bufAddF(buf, ")");
            break;
        }
    }
}

/*
 * Compare the expected output <exp> and the actual output <actual> for test number <index>. If they
 * don't match, write an error message and increment errors.
 */
static void compare_output(int index, const char *exp, const char *actual)
{
    if (strcmp(exp, actual) != 0) {
        fprintf(stderr, "Test %d: unexpected output:\n", index);
        fprintf(stderr, "\tExp: \"%s\"\n", exp);
        fprintf(stderr, "\tGot: \"%s\"\n", actual);
        errors++;
    }
}

/*
 * The tests.
 */
struct {
    char *input;
    int return_code;
    char *expected_output;
} test[] = {
    {   "Hoi \"Hee hallo\" 1 2.5 { -2 { -4.5 } }", 0,
        "S(Hoi) S(Hee hallo) L(1) D(2.5) C(L(-2) C(D(-4.5)))"
    },
    {   "{{ABC}}",                  0,  "C(C(S(ABC)))"                                          },
    {   "{{123}}",                  0,  "C(C(L(123)))"                                          },
    {   "{{1.5}}",                  0,  "C(C(D(1.5)))"                                          },
    {   "{{ABC}",                   1,  "1: Unmatched open brace."                              },
    {   "{ABC}}",                   1,  "1: Unmatched close brace."                             },
    {   "\"ABC",                    1,  "1: String not terminated."                             },
    {   "1E3",                      0,  "D(1000)"                                               },
    {   "1.5E3",                    0,  "D(1500)"                                               },
    {   "-1E3",                     0,  "D(-1000)"                                              },
    {   "-1.5E3",                   0,  "D(-1500)"                                              },
    {   "1E-3",                     0,  "D(0.001)"                                              },
    {   "1.5E-3",                   0,  "D(0.0015)"                                             },
    {   "-1E-3",                    0,  "D(-0.001)"                                             },
    {   "-1.5E-3",                  0,  "D(-0.0015)"                                            },
    {   "--1E-3",                   1,  "1: Unexpected character '-' following \"-\"."          },
    {   "-1E--3",                   1,  "1: Unexpected character '-' following \"-1E-\"."       },
    {   "1E",                       1,  "1: Missing exponent following 'E'."                    },
    {   "12.34.56",                 1,  "1: Unexpected character '.' following \"12.34\"."      },
    {   "12e34.56",                 1,  "1: Unexpected character '.' following \"12e34\"."      },
    {   "12.34e56E23",              1,  "1: Unexpected character 'E' following \"12.34e56\"."   },
    {   " $ ",                      1,  "1: Unexpected character '$' (ascii 36)."               },
    {   "\"ABC\t\"",                1,  "1: Unexpected character '\t' following \"ABC\"."       },
    {   "\"ABC\\t\"",               0,  "S(ABC\t)"                                              },
    {   "\"ABC\\xDEF\"",            1,  "1: Invalid escape sequence \"\\x\"."                   },
    {   "\"ABC\\",                  1,  "1: Escape sequence not terminated."                    },
    {   "ABC$DEF",                  1,  "1: Unexpected character '$' following \"ABC\"."        },
    {   "\"ABC$DEF\"",              0,  "S(ABC$DEF)"                                            },
    {   "_123",                     0,  "S(_123)"                                               },
    {   "123_",                     1,  "1: Unexpected character '_' following \"123\"."        },
    {   "A\nB\nC\n123_\n",          1,  "4: Unexpected character '_' following \"123\"."        },
    {   "A\rB\rC\r123_\r",          1,  "4: Unexpected character '_' following \"123\"."        },
    {   "A\r\nB\r\nC\r\n123_\r\n",  1,  "4: Unexpected character '_' following \"123\"."        }
};

int num_tests = sizeof(test) / sizeof(test[0]);

int main(int argc, char *argv[])
{
    int i, r;

    Buffer *buf = bufCreate();

    for (i = 0; i < num_tests; i++) {
        const char *output;

        List *objects = listCreate();

        r = sdpReadString(test[i].input, objects);

        if (r != test[i].return_code) {
            fprintf(stderr, "Test %d: expected return code %d, got %d.\n",
                    i, test[i].return_code, r);
            errors++;
        }

        if (r == 0) {
            bufClear(buf);
            my_dump(objects, buf);
            output = bufGet(buf);
        }
        else {
            output = sdpError();
        }

D       fprintf(stderr, "Code:     %d\n", r);
D       fprintf(stderr, "Input:    %s\n", test[i].input);
D       fprintf(stderr, "Output:   %s\n", output);
D       fprintf(stderr, "Expected: %s\n", test[i].expected_output);
D       fprintf(stderr, "Error:    %s\n", sdpError());

        compare_output(i, test[i].expected_output, output);

        sdpFree(objects);
    }

    bufDestroy(buf);

    return errors;
}
#endif
