/*
 * TokenStream library.
 *
 * Copyright:	(c) 2008 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>

#include "buffer.h"
#include "debug.h"
#include "list.h"
#include "ts.h"

typedef struct {
    ListNode _node;
    char *file;
    FILE *fp;
    int line, column;
} StackFrame;

typedef struct {
    ListNode _node;
    TokenType type;
    union {
        char   *s;
        long    l;
        double  d;
    } u;
} Token;

struct TokenStream {
    Buffer *buffer;
    Token last, *pushback;
    Buffer *error;
    List stack;
};

static void token_clr(Token *token)
{
    if ((token->type & TT_STRING) && token->u.s != NULL)
        free(token->u.s);

    memset(&token->u, 0, sizeof(token->u));

    token->type = TT_NONE;
}

static int token_set(Token *token, TokenType type, const char *data)
{
    char *endp;

    token_clr(token);

    token->type = type;

    switch(type) {
    case TT_LONG:
        token->u.l = strtol(data, &endp, 0);
        return (endp == data + strlen(data)) ? 0 : 1;
    case TT_DOUBLE:
        token->u.d = strtod(data, &endp);
        return (endp == data + strlen(data)) ? 0 : 1;
    default:
        token->u.s = strdup(data);
        return 0;
    }
}

static Token *token_new(TokenType type, const char *data)
{
    Token *token = calloc(1, sizeof(Token));

    token_set(token, type, data);

    return token;
}

static void token_del(Token *token)
{
    token_clr(token);

    free(token);
}

static const char *token_str(Token *token)
{
    Buffer buf = { };

    if (!token) return NULL;

    switch(token->type) {
    case TT_NONE:
        return "";
    case TT_EOF:
        return "eof";
    case TT_ERROR:
        return "error";
    case TT_LONG:
        bufSetF(&buf, "%ld", token->u.l);
        return bufGet(&buf);
    case TT_DOUBLE:
        bufSetF(&buf, "%g", token->u.d);
        return bufGet(&buf);
    default:
        bufSet(&buf, token->u.s, strlen(token->u.s));
        return bufGet(&buf);
    }
}

static void add_stack_frame(TokenStream *ts, const char *file, FILE *fp)
{
    StackFrame *sf = calloc(1, sizeof(StackFrame));

    sf->file   = strdup(file);
    sf->fp     = fp;
    sf->line   = 1;
    sf->column = 0;

    listInsertHead(&ts->stack, sf);
}

/*
 * Return a textual representation of <type>, which must be a single TokenType
 * value (i.e. a bitmask with only one bit set).
 */
static const char *single_token_type(TokenType type)
{
    dbgAssert(stderr, (type & TT_ALL) != 0, "Invalid token type");

    switch(type) {
    case TT_NONE:
        return "none";
    case TT_USTRING:
        return "unquoted string";
    case TT_QSTRING:
        return "quoted string";
    case TT_LONG:
        return "integer";
    case TT_DOUBLE:
        return "double";
    case TT_OBRACE:
        return "open brace";
    case TT_CBRACE:
        return "close brace";
    case TT_OBRACKET:
        return "open bracket";
    case TT_CBRACKET:
        return "close bracket";
    case TT_OPAREN:
        return "open parenthesis";
    case TT_CPAREN:
        return "close parenthesis";
    case TT_EOF:
        return "end of file";
    case TT_ERROR:
        return "error";
    default:
        return "invalid";
    }
}

static void set_error_msg(TokenStream *ts,
        const char *file, int line, int column, const char *fmt, ...)
{
    va_list ap;

    if (ts->error == NULL) ts->error = bufCreate();

    bufAddF(ts->error, "%s:%d:%d: ", file, line, column);

    va_start(ap, fmt);
    bufAddV(ts->error, fmt, ap);
    va_end(ap);
}

/*
 * Return a textual representation of bitmask <mask>. Each set bit will be
 * named.
 */
const char *tsTypeString(TokenType mask)
{
    int bit = 0, count = 0;
    unsigned int higher_bits_mask = UINT_MAX;
    static Buffer *buffer = NULL;

    if (buffer == NULL)
        buffer = bufCreate();
    else
        bufClear(buffer);

    for (bit = 0; bit <= 8 * sizeof(unsigned int); bit++) {
        if ((mask & higher_bits_mask) == 0) break;

        higher_bits_mask <<= 1;

        if ((mask & (1 << bit)) == 0) continue;

        if (count == 0)
            bufAddF(buffer, "%s", single_token_type(1 << bit));
        else if ((mask & higher_bits_mask) == 0)
            bufAddF(buffer, " or %s", single_token_type(1 << bit));
        else
            bufAddF(buffer, ", %s", single_token_type(1 << bit));

        count++;
    }

    return bufFinish(buffer);
}

/*
 * Return a textual representation of the data in the last token.
 */
const char *tsDataString(TokenStream *ts)
{
    return token_str(&ts->last);
}

/*
 * Open a token stream, reading from <file>.
 */
TokenStream *tsOpen(const char *file)
{
    TokenStream *ts;

    FILE *fp = fopen(file, "r");

    if (fp == NULL) return NULL;

    ts = calloc(1, sizeof(TokenStream));

    ts->buffer = bufCreate();

    add_stack_frame(ts, file, fp);

    return ts;
}

/*
 * Close the token stream <ts>.
 */
void tsClose(TokenStream *ts)
{
    StackFrame *sf;

    while ((sf = listRemoveHead(&ts->stack)) != NULL) {
        fclose(sf->fp);

        if (sf->file) free(sf->file);
    }

    bufDestroy(ts->buffer);

    if (ts->error) free(ts->error);

    free(ts);
}

/*
 * Connect a token stream to <fp>, which must have been opened for reading. The
 * name for the top-level file is set to a generic name, deduced from the type
 * of the given file pointer. Change it with tsSetFileName().
 */
TokenStream *tsConnect(FILE *fp)
{
    struct stat statbuf;
    char *file;

    TokenStream *ts = calloc(1, sizeof(TokenStream));

    ts->buffer = bufCreate();

    fstat(fileno(fp), &statbuf);

    if (S_ISREG(statbuf.st_mode))
        file = strdup("File");
    else if (S_ISCHR(statbuf.st_mode) || S_ISBLK(statbuf.st_mode))
        file = strdup("Device");
    else if (S_ISFIFO(statbuf.st_mode))
        file = strdup("FIFO");
    else if (S_ISSOCK(statbuf.st_mode))
        file = strdup("Socket");
    else
        file = strdup("Unknown");

    add_stack_frame(ts, file, fp);

    return ts;
}

/*
 * Set the (top-level) filename for this TokenStream. Can be used together with
 * tsConnect, when the file was opened by the caller.
 */
void tsSetFileName(TokenStream *ts, const char *file)
{
    StackFrame *top = listTail(&ts->stack);

    if (top->file != NULL) free(top->file);

    top->file = strdup(file);
}

typedef enum {
    ST_NONE,
    ST_COMMENT,
    ST_INCLUDE,
    ST_UNQUOTED_STRING,
    ST_QUOTED_STRING,
    ST_LONG,
    ST_DOUBLE
} TState;

/*
 * Get a token from TokenStream <ts>. The type is returned, the value of the
 * token is returned through <data>. This is a statically buffer which is
 * overwritten on every call.
 *
 *  if type == TT_LONG:   *data points to a long;
 *  if type == TT_DOUBLE: *data points to a double;
 *  all others:              *data points to a character array.
 *
 * In case of error, TT_ERROR is returned and the error message can be
 * retrieved using tsError(). On end-of-file, TT_EOF is returned.
 */
TokenType tsGetToken(TokenStream *ts)
{
    int c;

    TState state = ST_NONE;
    StackFrame *curr;

    if (ts->error) return TT_ERROR;

    bufClear(ts->buffer);

    if (ts->pushback) {
        token_set(&ts->last, ts->pushback->type, token_str(ts->pushback));

        token_del(ts->pushback);

        ts->pushback = NULL;

        return ts->last.type;
    }

    while ((curr = listHead(&ts->stack)) != NULL) {
        if ((c = fgetc(curr->fp)) == EOF) {
            if (listLength(&ts->stack) == 1) return TT_EOF;

            listRemove(&ts->stack, curr);

            fclose(curr->fp);
            free(curr->file);

            free(curr);

            continue;
        }

        curr->column++;

        if (c == '\n') {
            curr->line++;
            curr->column = 0;
        }

        switch(state) {
        case ST_NONE:
            if (c == '{') {
                token_set(&ts->last, TT_OBRACE, "{");
                return ts->last.type;
            }
            else if (c == '}') {
                token_set(&ts->last, TT_CBRACE, "}");
                return ts->last.type;
            }
            else if (c == '[') {
                token_set(&ts->last, TT_OBRACKET, "[");
                return ts->last.type;
            }
            else if (c == ']') {
                token_set(&ts->last, TT_CBRACKET, "]");
                return ts->last.type;
            }
            else if (c == '(') {
                token_set(&ts->last, TT_OPAREN, "(");
                return ts->last.type;
            }
            else if (c == ')') {
                token_set(&ts->last, TT_CPAREN, ")");
                return ts->last.type;
            }
            else if (c == '"') {
                bufClear(ts->buffer);
                state = ST_QUOTED_STRING;
            }
            else if (c == '#') {
                state = ST_COMMENT;
            }
            else if (isalpha(c) || c == '_') {
                bufClear(ts->buffer);
                bufAddC(ts->buffer, c);
                state = ST_UNQUOTED_STRING;
            }
            else if (isdigit(c)) {
                bufClear(ts->buffer);
                bufAddC(ts->buffer, c);
                state = ST_LONG;
            }
            else if (c == '.') {
                bufClear(ts->buffer);
                bufAddC(ts->buffer, c);
                state = ST_DOUBLE;
            }
            else if (c == '<') {
                bufClear(ts->buffer);
                state = ST_INCLUDE;
            }
            else if (!isspace(c)) {
                set_error_msg(ts, curr->file, curr->line, curr->column,
                        "Unexpected character '%c' (ascii %d)", c, c);
                return TT_ERROR;
            }
            break;
        case ST_COMMENT:
            if (c == '\n') {
                state = ST_NONE;
            }
            break;
        case ST_INCLUDE:
            if (c != '>') {
                bufAddC(ts->buffer, c);
            }
            else {
                const char *file = bufGet(ts->buffer);
                FILE *fp;

                if ((fp = fopen(file, "r")) == NULL) {
                    set_error_msg(ts, curr->file, curr->line, curr->column,
                            "Couldn't open file \"%s\".", file);
                    return TT_ERROR;
                }
                else {
                    add_stack_frame(ts, file, fp);
                    state = ST_NONE;
                }
            }
            break;
        case ST_UNQUOTED_STRING:
            if (isalnum(c) || c == '_') {
                bufAddC(ts->buffer, c);
            }
            else if (!isspace(c)) {
                set_error_msg(ts, curr->file, curr->line, curr->column,
                        "Unexpected character '%c' (ascii %d)", c, c);
                return TT_ERROR;
            }
            else {
                token_set(&ts->last, TT_USTRING, bufGet(ts->buffer));
                return ts->last.type;
            }
            break;
        case ST_QUOTED_STRING:
            if (c != '"') {
                bufAddC(ts->buffer, c);
            }
            else {
                token_set(&ts->last, TT_QSTRING, bufGet(ts->buffer));
                return ts->last.type;
            }
            break;
        case ST_LONG:
            if (isxdigit(c) || c == 'x') {
                bufAddC(ts->buffer, c);
            }
            else if (c == '.') {
                bufAddC(ts->buffer, c);
                state = ST_DOUBLE;
            }
            else if (!isspace(c)) {
                set_error_msg(ts, curr->file, curr->line, curr->column,
                        "Unexpected character '%c' (ascii %d)", c, c);
                return TT_ERROR;
            }
            else if (token_set(&ts->last, TT_LONG, bufGet(ts->buffer)) != 0) {
                set_error_msg(ts, curr->file, curr->line, curr->column,
                        "Could not convert \"%s\" to an integer", bufGet(ts->buffer));
                return TT_ERROR;
            }
            else {
                return ts->last.type;
            }
            break;
        case ST_DOUBLE:
            if (isdigit(c) || c == '.') {
                bufAddC(ts->buffer, c);
            }
            else if (!isspace(c)) {
                set_error_msg(ts, curr->file, curr->line, curr->column,
                        "Unexpected character '%c' (ascii %d)", c, c);
                return TT_ERROR;
            }
            else if (token_set(&ts->last, TT_DOUBLE, bufGet(ts->buffer)) != 0) {
                set_error_msg(ts, curr->file, curr->line, curr->column,
                        "Could not convert \"%s\" to a double", bufGet(ts->buffer));
                return TT_ERROR;
            }
            else {
                return ts->last.type;
            }
            break;
        }
    }

    return TT_EOF;
}

/*
 * Get a token of type <type> from TokenStream <ts>. If successful,
 * <expected_type> is returned and <*data> is set to point to the contents of
 * the token. If an error occurred, or the found token type is different than
 * expected, TT_ERROR is returned and the error message can be retrieved
 * using tsError(). On end-of-file, TT_EOF is returned.
 *
 * <*data> is set as follows:
 *
 *  if type == TT_LONG:   *data points to a long;
 *  if type == TT_DOUBLE: *data points to a double;
 *  all others:           *data points to a character array.
 */
TokenType tsExpectToken(TokenStream *ts, TokenType expected_type)
{
    TokenType actual_type;

    if (ts->error) {
        return TT_ERROR;
    }
    else if ((actual_type = tsGetToken(ts)) == TT_ERROR) {
        return TT_ERROR;
    }
    else if ((actual_type & expected_type) != 0) {
        return actual_type;
    }
    else {
        StackFrame *sf = listHead(&ts->stack);

        set_error_msg(ts, sf->file, sf->line, sf->column,
                "Expected %s, got %s.",
                tsTypeString(expected_type),
                tsTypeString(actual_type));

        return TT_ERROR;
    }

    return actual_type;
}

/*
 * Push back the last read token.
 */
void tsUngetToken(TokenStream *ts)
{
    if (ts->pushback != NULL) {
        StackFrame *sf = listHead(&ts->stack);

        set_error_msg(ts, sf->file, sf->line, sf->column,
                "Only one level of pushback allowed.");
        return;
    }

    ts->pushback = token_new(ts->last.type, token_str(&ts->last));
}

/*
 * Retrieve the last error that occurred.
 */
const char *tsError(TokenStream *ts)
{
    return ts->error ? bufGet(ts->error) : NULL;
}

/*
 * Retrieve the datafile currently being read.
 */
char *tsFile(TokenStream *ts)
{
    StackFrame *sf = listHead(&ts->stack);

    return sf->file;
}

/*
 * Retrieve the line number currently being read.
 */
int tsLine(TokenStream *ts)
{
    StackFrame *sf = listHead(&ts->stack);

    return sf->line;
}

/*
 * Retrieve the column number currently being read.
 */
int tsColumn(TokenStream *ts)
{
    StackFrame *sf = listHead(&ts->stack);

    return sf->column;
}
