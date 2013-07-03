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

typedef enum {
    SDP_SOURCE_NONE,
    SDP_SOURCE_FILE,
    SDP_SOURCE_FD,
    SDP_SOURCE_STR
} SdpSourceType;

typedef struct SdpSource SdpSource;

typedef enum {
    SDP_STATE_NONE,
    SDP_STATE_USTRING,
    SDP_STATE_QSTRING,
    SDP_STATE_LONG,
    SDP_STATE_DOUBLE,
    SDP_STATE_SCIENTIFIC,
    SDP_STATE_ESCAPE,
    SDP_STATE_COMMENT,
    SDP_STATE_EOF,
    SDP_STATE_ERROR
} SdpState;

struct SdpSource {
    SdpSourceType type;
    int (*get_char)(SdpSource *src);
    Buffer unget_buf, value_buf;
    List **stack;
    int stack_size;
    SdpState state, saved_state;
    int cur_line;
    union {
        FILE *fp;
        int fd;
        const char *str;
    } u;
};

static Buffer error_msg = { 0 };

static int sdp_get_char_file(SdpSource *src)
{
    int c = fgetc(src->u.fp);

    if (c != EOF)
        return c;
    else if (errno == 0)
        return 0;
    else
        return -1;
}

static int sdp_get_char_fd(SdpSource *src)
{
    char p;

    int n = read(src->u.fd, &p, 1);

    return n == 1 ? p : n;
}

static int sdp_get_char_str(SdpSource *src)
{
    int c = *src->u.str;

    if (c != 0) {
        src->u.str++;
    }

    return c;
}

static void sdp_unget_char(SdpSource *src, int c)
{
    bufAddC(&src->unget_buf, c);
}

static int sdp_get_char(SdpSource *src)
{
    int c;

    if (bufLen(&src->unget_buf) > 0) {
        c = bufGet(&src->unget_buf)[bufLen(&src->unget_buf) - 1];
        bufTrim(&src->unget_buf, 0, 1);
    }
    else if ((c = src->get_char(src)) == '\n') {
        src->cur_line++;
    }
    else if (c == '\r') {
        int c2 = src->get_char(src);

        if (c2 != '\n') {
            sdp_unget_char(src, c2);
        }

        c = '\n';
        src->cur_line++;
    }

    return c;
}

static void sdp_push_list(SdpSource *src, List *list)
{
    src->stack_size++;

    src->stack = realloc(src->stack, src->stack_size * sizeof(List *));

    src->stack[src->stack_size - 1] = list;
}

static List *sdp_pop_list(SdpSource *src)
{
    List *top = src->stack[src->stack_size - 1];

    src->stack_size--;

    src->stack = realloc(src->stack, src->stack_size * sizeof(List *));

    return top;
}

static SdpObject *sdp_add_object(SdpType type, const char *data, int line, List *list)
{
    SdpObject *obj = calloc(1, sizeof(SdpObject));

    obj->type = type;
    obj->line = line;

    switch(type) {
    case SDP_STRING:
        obj->u.s = strdup(data);
        break;
    case SDP_LONG:
        obj->u.l = atol(data);
        break;
    case SDP_DOUBLE:
        obj->u.d = atof(data);
        break;
    case SDP_CONTAINER:
        break;
    }

    listAppendTail(list, obj);

    return obj;
}

static int sdp_read(SdpSource *src, List *objects)
{
    int ret_code;

    src->stack = calloc(1, sizeof(List *));
    src->stack[0] = objects;
    src->stack_size = 1;

    src->state = SDP_STATE_NONE;
    src->cur_line = 1;

    while (src->state != SDP_STATE_EOF && src->state != SDP_STATE_ERROR) {
        int c = sdp_get_char(src);

        List *cur_list = src->stack[src->stack_size - 1];

        switch(src->state) {
        case SDP_STATE_NONE:
            if (isspace(c)) {
                continue;
            }
            else if (isalpha(c) || c == '_') {
                bufSetC(&src->value_buf, c);
                src->state = SDP_STATE_USTRING;
            }
            else if (isalpha(c) || c == '"') {
                bufClear(&src->value_buf);
                src->state = SDP_STATE_QSTRING;
            }
            else if (isdigit(c)) {
                bufSetC(&src->value_buf, c);
                src->state = SDP_STATE_LONG;
            }
            else if (c == '{') {
                SdpObject *obj = sdp_add_object(SDP_CONTAINER, NULL, src->cur_line, cur_list);
                sdp_push_list(src, &obj->u.c);
            }
            else if (c == '}') {
                if (src->stack_size == 1) {
                    bufSetF(&error_msg, "%d: Unmatched close brace.", src->cur_line);
                    src->state = SDP_STATE_ERROR;
                }
                sdp_pop_list(src);
            }
            else if (c == '#') {
                src->state = SDP_STATE_COMMENT;
            }
            else if (c == 0) {
                if (src->stack_size != 1) {
                    bufSetF(&error_msg, "%d: Unmatched open brace.", src->cur_line);
                    src->state = SDP_STATE_ERROR;
                }
                else {
                    src->state = SDP_STATE_EOF;
                }
            }
            else {
                bufSetF(&error_msg, "%d: Unexpected character '%c' (ascii %d).",
                        src->cur_line, c, c);
                src->state = SDP_STATE_ERROR;
            }
            break;
        case SDP_STATE_USTRING:
            if (isspace(c)) {
                sdp_add_object(SDP_STRING, bufGet(&src->value_buf), src->cur_line, cur_list);
                bufClear(&src->value_buf);
                src->state = SDP_STATE_NONE;
            }
            else if (isalnum(c) || c == '_') {
                bufAddC(&src->value_buf, c);
            }
            else if (c == '\\') {
                src->saved_state = src->state;
                src->state = SDP_STATE_ESCAPE;
            }
            else if (c == '{') {
                sdp_add_object(SDP_STRING, bufGet(&src->value_buf), src->cur_line, cur_list);
                sdp_unget_char(src, c);
                src->state = SDP_STATE_NONE;
            }
            else if (c == '}') {
                sdp_add_object(SDP_STRING, bufGet(&src->value_buf), src->cur_line, cur_list);
                sdp_unget_char(src, c);
                src->state = SDP_STATE_NONE;
            }
            else if (c == 0) {
                sdp_add_object(SDP_STRING, bufGet(&src->value_buf), src->cur_line, cur_list);
                bufClear(&src->value_buf);
                src->state = SDP_STATE_EOF;
            }
            else {
                bufSetF(&error_msg, "%d: Unexpected character '%c' following \"%s\".",
                        src->cur_line, c, bufGet(&src->value_buf));
                src->state = SDP_STATE_ERROR;
            }
            break;
        case SDP_STATE_QSTRING:
            if (c == '"') {
                sdp_add_object(SDP_STRING, bufGet(&src->value_buf), src->cur_line, cur_list);
                bufClear(&src->value_buf);
                src->state = SDP_STATE_NONE;
            }
            else if (c == '\\') {
                src->saved_state = src->state;
                src->state = SDP_STATE_ESCAPE;
            }
            else if (isprint(c)) {
                bufAddC(&src->value_buf, c);
            }
            else if (c == 0) {
                bufSetF(&error_msg, "%d: String not terminated.", src->cur_line);
                src->state = SDP_STATE_ERROR;
            }
            else {
                bufSetF(&error_msg, "%d: Unexpected character '%c' following \"%s\".",
                        src->cur_line, c, bufGet(&src->value_buf));
                src->state = SDP_STATE_ERROR;
            }
            break;
        case SDP_STATE_LONG:
            if (isdigit(c)) {
                bufAddC(&src->value_buf, c);
            }
            else if (c == '.') {
                bufAddC(&src->value_buf, c);
                src->state = SDP_STATE_DOUBLE;
            }
            else if (c == 'e' || c == 'E') {
                bufAddC(&src->value_buf, c);
                src->state = SDP_STATE_SCIENTIFIC;
            }
            else if (isspace(c)) {
                sdp_add_object(SDP_LONG, bufGet(&src->value_buf), src->cur_line, cur_list);
                src->state = SDP_STATE_NONE;
            }
            else if (c == '{') {
                sdp_add_object(SDP_LONG, bufGet(&src->value_buf), src->cur_line, cur_list);
                sdp_unget_char(src, c);
                src->state = SDP_STATE_NONE;
            }
            else if (c == '}') {
                sdp_add_object(SDP_LONG, bufGet(&src->value_buf), src->cur_line, cur_list);
                sdp_unget_char(src, c);
                src->state = SDP_STATE_NONE;
            }
            else if (c == 0) {
                sdp_add_object(SDP_LONG, bufGet(&src->value_buf), src->cur_line, cur_list);
                src->state = SDP_STATE_EOF;
            }
            else {
                bufSetF(&error_msg, "%d: Unexpected character '%c' following \"%s\".",
                        src->cur_line, c, bufGet(&src->value_buf));
                src->state = SDP_STATE_ERROR;
            }
            break;
        case SDP_STATE_DOUBLE:
            if (isdigit(c)) {
                bufAddC(&src->value_buf, c);
            }
            else if (c == 'e' || c == 'E') {
                bufAddC(&src->value_buf, c);
                src->state = SDP_STATE_SCIENTIFIC;
            }
            else if (isspace(c)) {
                sdp_add_object(SDP_DOUBLE, bufGet(&src->value_buf), src->cur_line, cur_list);
                src->state = SDP_STATE_NONE;
            }
            else if (c == '{') {
                sdp_add_object(SDP_DOUBLE, bufGet(&src->value_buf), src->cur_line, cur_list);
                sdp_unget_char(src, c);
                src->state = SDP_STATE_NONE;
            }
            else if (c == '}') {
                sdp_add_object(SDP_DOUBLE, bufGet(&src->value_buf), src->cur_line, cur_list);
                sdp_unget_char(src, c);
                src->state = SDP_STATE_NONE;
            }
            else if (c == 0) {
                sdp_add_object(SDP_DOUBLE, bufGet(&src->value_buf), src->cur_line, cur_list);
                src->state = SDP_STATE_EOF;
            }
            else {
                bufSetF(&error_msg, "%d: Unexpected character '%c' following \"%s\".",
                        src->cur_line, c, bufGet(&src->value_buf));
                src->state = SDP_STATE_ERROR;
            }
            break;
        case SDP_STATE_SCIENTIFIC:
            if (isdigit(c)) {
                bufAddC(&src->value_buf, c);
            }
            else if (isspace(c)) {
                sdp_add_object(SDP_DOUBLE, bufGet(&src->value_buf), src->cur_line, cur_list);
                src->state = SDP_STATE_NONE;
            }
            else if (c == 0) {
                sdp_add_object(SDP_DOUBLE, bufGet(&src->value_buf), src->cur_line, cur_list);
                src->state = SDP_STATE_EOF;
            }
            else {
                bufSetF(&error_msg, "%d: Unexpected character '%c' following \"%s\".",
                        src->cur_line, c, bufGet(&src->value_buf));
                src->state = SDP_STATE_ERROR;
            }
            break;
        case SDP_STATE_ESCAPE:
            if (c == 'n') {
                bufAddC(&src->value_buf, '\n');
                src->state = src->saved_state;
            }
            else if (c == 'r') {
                bufAddC(&src->value_buf, '\r');
                src->state = src->saved_state;
            }
            else if (c == 't') {
                bufAddC(&src->value_buf, '\t');
                src->state = src->saved_state;
            }
            else if (c == '\\') {
                bufAddC(&src->value_buf, '\\');
                src->state = src->saved_state;
            }
            else if (c == '"') {
                bufAddC(&src->value_buf, '"');
                src->state = src->saved_state;
            }
            else if (c == 0) {
                bufSetF(&error_msg, "%d: Escape sequence not terminated.", src->cur_line);
                src->state = SDP_STATE_ERROR;
            }
            else {
                bufSetF(&error_msg, "%d: Invalid escape sequence \"\\%c\".", src->cur_line, c);
                src->state = SDP_STATE_ERROR;
            }
            break;
        case SDP_STATE_COMMENT:
            if (c == 0) {
                src->state = SDP_STATE_EOF;
            }
            else if (c == '\n') {
                src->state = SDP_STATE_NONE;
            }
            break;
        default:
            break;
        }
    }

    ret_code = src->state == SDP_STATE_EOF ? 0 : 1;

    bufReset(&src->value_buf);

    free(src->stack);
    free(src);

    return ret_code;
}

/*
 * Read SdpObjects from file <fp> and append them to <objects>.
 */
int sdpReadFile(FILE *fp, List *objects)
{
    SdpSource *src = calloc(1, sizeof(SdpSource));

    src->type = SDP_SOURCE_FILE;
    src->get_char = sdp_get_char_file;
    src->u.fp = fp;

    return sdp_read(src, objects);
}

/*
 * Read SdpObjects from file descriptor <fd> and append them to <objects>.
 */
int sdpReadFD(int fd, List *objects)
{
    SdpSource *src = calloc(1, sizeof(SdpSource));

    src->type = SDP_SOURCE_FD;
    src->get_char = sdp_get_char_fd;
    src->u.fd = fd;

    return sdp_read(src, objects);
}

/*
 * Read SdpObjects from string <str> and append them to <objects>.
 */
int sdpReadString(const char *str, List *objects)
{
    SdpSource *src = calloc(1, sizeof(SdpSource));

    src->type = SDP_SOURCE_STR;
    src->get_char = sdp_get_char_str;
    src->u.str = str;

    return sdp_read(src, objects);
}

/*
 * Dump the list of SdpObjects in <objects> on <fp>, indented by <indent> levels.
 */
void sdpDump(FILE *fp, int indent, List *objects)
{
    SdpObject *obj;

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
 * Clear the list of SdpObjects at <objects>. Clears the contents of the list but leaves the list
 * itself alone.
 */
void sdpClear(List *objects)
{
    SdpObject *obj;

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
 * Clear and free the list of SdpObjects at <objects>.
 */
void sdpFree(List *objects)
{
    sdpClear(objects);
    free(objects);
}

#ifdef TEST
static int errors = 0;

static void my_dump(List *objects, Buffer *buf)
{
    SdpObject *obj;

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
            bufAddF(buf, "D(%f)", obj->u.d);
            break;
        case SDP_CONTAINER:
            bufAddF(buf, "C(");
            my_dump(&obj->u.c, buf);
            bufAddF(buf, ")");
            break;
        }
    }
}

static void compare_output(int index, const char *exp, const char *actual)
{
    if (strcmp(exp, actual) != 0) {
        fprintf(stderr, "Test %d: unexpected output:\n", index);
        fprintf(stderr, "\tExp: \"%s\"\n", exp);
        fprintf(stderr, "\tGot: \"%s\"\n", actual);
        errors++;
    }
}

struct {
    char *input;
    int return_code;
    char *output;
} test[] = {
    {   "Hoi \"Hee hallo\" 1 2.5 { 1 { 2 } }",
        0,
        "S(Hoi) S(Hee hallo) L(1) D(2.500000) C(L(1) C(L(2)))"
    },
    {   "\"ABC",
        1,
        "1: String not terminated."
    },
    {   "\"ABC\t\"",
        1,
        "1: Unexpected character '\t' following \"ABC\"."
    },
    {   "\"ABC\\xDEF\"",
        1,
        "1: Invalid escape sequence \"\\x\"."
    },
    {   "\"ABC\\",
        1,
        "1: Escape sequence not terminated."
    },
    {   "{{ABC}}",
        0,
        "C(C(S(ABC)))"
    },
    {   "{{123}}",
        0,
        "C(C(L(123)))"
    },
    {   "{{1.5}}",
        0,
        "C(C(D(1.500000)))"
    },
    {   "{{ABC}",
        1,
        "1: Unmatched open brace."
    },
    {   "{ABC}}",
        1,
        "1: Unmatched close brace."
    },
    {   "1E3",
        0,
        "D(1000.000000)"
    },
    {   "1.5E3",
        0,
        "D(1500.000000)"
    },
    {   "12.34.56",
        1,
        "1: Unexpected character '.' following \"12.34\"."
    },
    {   "12e34.56",
        1,
        "1: Unexpected character '.' following \"12e34\"."
    },
    {   "12.34e56E23",
        1,
        "1: Unexpected character 'E' following \"12.34e56\"."
    },
    {   "%",
        1,
        "1: Unexpected character '%' (ascii 37)."
    },
    {   "ABC$DEF",
        1,
        "1: Unexpected character '$' following \"ABC\"."
    },
    {   "\"ABC$DEF\"",
        0,
        "S(ABC$DEF)"
    },
    {   "_123",
        0,
        "S(_123)"
    },
    {   "123_",
        1,
        "1: Unexpected character '_' following \"123\"."
    },
    {   "A\nB\nC\n123_\n",
        1,
        "4: Unexpected character '_' following \"123\"."
    },
    {   "A\rB\rC\r123_\r",
        1,
        "4: Unexpected character '_' following \"123\"."
    },
    {   "A\r\nB\r\nC\r\n123_\r\n",
        1,
        "4: Unexpected character '_' following \"123\"."
    }
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
D       fprintf(stderr, "Expected: %s\n", test[i].output);
D       fprintf(stderr, "Error:    %s\n", sdpError());

        compare_output(i, test[i].output, output);

        sdpFree(objects);
    }

    bufDestroy(buf);

    return errors;
}
#endif
