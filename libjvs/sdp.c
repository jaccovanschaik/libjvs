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
    SDP_STATE_CR,
    SDP_STATE_EOF
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
    else if ((c = src->get_char(src)) == '\n' || c == '\r') {
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
    src->stack = calloc(1, sizeof(List *));
    src->stack[0] = objects;
    src->stack_size = 1;

    src->state = SDP_STATE_NONE;
    src->cur_line = 1;

    while (src->state != SDP_STATE_EOF) {
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
                    return -1;
                }
                sdp_pop_list(src);
            }
            else if (c == '\r') {
                src->state = SDP_STATE_CR;
            }
            else if (c == '#') {
                src->state = SDP_STATE_COMMENT;
            }
            else if (c == 0) {
                if (src->stack_size != 1) {
                    bufSetF(&error_msg, "%d: Unmatched open brace.", src->cur_line);
                    return -1;
                }
                src->state = SDP_STATE_EOF;
            }
            else {
                bufSetF(&error_msg, "%d: Unexpected character '%c' (ascii %d).",
                        src->cur_line, c, c);
                return -1;
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
            else if (c == 0) {
                sdp_add_object(SDP_STRING, bufGet(&src->value_buf), src->cur_line, cur_list);
                bufClear(&src->value_buf);
                src->state = SDP_STATE_EOF;
            }
            else {
                bufSetF(&error_msg, "%d: Unexpected character '%c' following \"%s\".",
                        src->cur_line, c, bufGet(&src->value_buf));
                return -1;
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
                return -1;
            }
            else {
                bufSetF(&error_msg, "%d: Unexpected character '%c' following \"%s\".",
                        src->cur_line, c, bufGet(&src->value_buf));
                return -1;
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
            else if (c == 0) {
                sdp_add_object(SDP_LONG, bufGet(&src->value_buf), src->cur_line, cur_list);
                src->state = SDP_STATE_EOF;
            }
            else {
                bufSetF(&error_msg, "%d: Unexpected character '%c' following \"%s\".",
                        src->cur_line, c, bufGet(&src->value_buf));
                return -1;
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
            else if (c == 0) {
                sdp_add_object(SDP_DOUBLE, bufGet(&src->value_buf), src->cur_line, cur_list);
                src->state = SDP_STATE_EOF;
            }
            else {
                bufSetF(&error_msg, "%d: Unexpected character '%c' following \"%s\".",
                        src->cur_line, c, bufGet(&src->value_buf));
                return -1;
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
                return -1;
            }
        case SDP_STATE_ESCAPE:
            if (c == 'n')
                bufAddC(&src->value_buf, '\n');
            else if (c == 'r')
                bufAddC(&src->value_buf, '\r');
            else if (c == 't')
                bufAddC(&src->value_buf, '\t');
            else if (c == '\\')
                bufAddC(&src->value_buf, '\\');
            else if (c == '"')
                bufAddC(&src->value_buf, '"');
            else if (c == 0) {
                bufSetF(&error_msg, "%d: Escape sequence not terminated.", src->cur_line);
                return -1;
            }
            else {
                bufSetF(&error_msg, "%d: Invalid escape sequence \"\\%c\"", src->cur_line, c);
                return -1;
            }
            src->state = src->saved_state;
            break;
        case SDP_STATE_CR:
            if (c == '\n') {
                src->cur_line--;
            }
            else if (c == 0) {
                src->state = SDP_STATE_EOF;
            }
            else {
                sdp_unget_char(src, c);
            }
            src->state = SDP_STATE_NONE;
        case SDP_STATE_COMMENT:
            if (c == '\r') {
                src->state = SDP_STATE_CR;
            }
            else if (c == 0) {
                src->state = SDP_STATE_EOF;
            }
            else if (c == '\n') {
                src->state = SDP_STATE_NONE;
            }
            break;
        case SDP_STATE_EOF:
            break;
        }
    }

    bufReset(&src->value_buf);

    free(src->stack);
    free(src);

    return 0;
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

void _make_sure_that(const char *file, int line, const char *str, int val)
{
   if (!val) {
      fprintf(stderr, "%s:%d: Expression \"%s\" failed\n", file, line, str);
      errors++;
   }
}

#define make_sure_that(expr) _make_sure_that(__FILE__, __LINE__, #expr, (expr))

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

struct {
    char *input;
    int return_code;
    char *output;
} test[] = {
    {   "Hoi",
        0,
        "S(Hoi)"
    },
    {   "Hee hallo",
        0,
        "S(Hee) S(hallo)"
    },
    {   "\"Hee hallo\"",
        0,
        "S(Hee hallo)"
    },
    {   "12",
        0,
        "L(12)"
    },
    {   "1.5",
        0,
        "D(1.500000)"
    },
    {   "{ \"Level 1\" }",
        0,
        "C(S(Level 1))"
    },
    {   "{ { \"Level 2\" } }",
        0,
        "C(C(S(Level 2)))"
    },
    {   "Hoi \"Hallo\" 1 2.5 { 1 { 2 } }",
        0,
        "S(Hoi) S(Hallo) L(1) D(2.500000) C(L(1) C(L(2)))"
    },
    {   "12.34.56",
        -1,
        "1: Unexpected character '.' following \"12.34\"."
    }
};

int num_tests = sizeof(test) / sizeof(test[0]);

int main(int argc, char *argv[])
{
    int i, r;

    for (i = 0; i < num_tests; i++) {
        char *output;

        List *objects = listCreate();

        r = sdpReadString(test[i].input, objects);

        make_sure_that(r == test[i].return_code);

        if (r == 0) {
            Buffer *buf = bufCreate();
            my_dump(objects, buf);
            output = bufFinish(buf);
        }
        else {
            output = sdpError();
        }

D       fprintf(stderr, "%s\n", output);

        make_sure_that(strcmp(output, test[i].output) == 0);

        free(output);
        sdpFree(objects);
    }

    return errors;
}
#endif
