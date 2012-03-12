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
    TokenData data;
} Token;

struct TokenStream {
    Buffer *buffer;
    char *error;
    List stack;
    List pushback;
};

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
    case TT_STRING:
        return "string" ;
    case TT_USTRING:
        return "unquoted string";
    case TT_QSTRING:
        return "quoted string";
    case TT_NUMBER:
        return "number";
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

/*
 * Return a textual representation of bitmask <mask>. Each set bit will be
 * named.
 */
const char *tsTokenType(TokenType mask)
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
 * Return a textual representation of TokenData <data>, where the type
 * of the token that contained it is <type>.
 */
const char *tsTokenData(TokenType type, TokenData *data)
{
    static char buffer[80];

    if (type == TT_LONG) {
        snprintf(buffer, 80, "%s %ld", tsTokenType(type), data->l);
        return buffer;
    }
    else if (type == TT_DOUBLE) {
        snprintf(buffer, 80, "%s %g", tsTokenType(type), data->d);
        return buffer;
    }
    else if (type == TT_QSTRING || type == TT_USTRING) {
        snprintf(buffer, 80, "%s \"%s\"", tsTokenType(type), data->s);
        return buffer;
    }
    else {
        snprintf(buffer, 80, "%s", tsTokenType(type));
        return buffer;
    }
}

static char *error_msg(const char *file, int line, int column,
        const char *fmt, ...)
{
    va_list ap;

    static Buffer *buffer = NULL;

    if (buffer == NULL)
        buffer = bufCreate();
    else
        bufClear(buffer);

    bufAddF(buffer, "%s:%d:%d: ", file, line, column);

    va_start(ap, fmt);
    bufAddV(buffer, fmt, ap);
    va_end(ap);

    return bufGet(buffer);
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
TokenType tsGetToken(TokenStream *ts, TokenData *data)
{
    int c;

    TState state = ST_NONE;
    StackFrame *curr;

    if (ts->error) return TT_ERROR;

    bufClear(ts->buffer);

    if (!listIsEmpty(&ts->pushback)) {
        Token *token = listRemoveHead(&ts->pushback);
        TokenType type = token->type;

        if (data != NULL) {
            if (type == TT_LONG) {
                data->l = token->data.l;
            }
            else if (type == TT_LONG) {
                data->d = token->data.d;
            }
            else if (token->data.s != NULL) {
                bufAdd(ts->buffer, token->data.s, strlen(token->data.s));
                free(token->data.s);
                data->s = bufGet(ts->buffer);
            }
        }

        free(token);

        return type;
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
                if (data) data->s = "{";
                return TT_OBRACE;
            }
            else if (c == '}') {
                if (data) data->s = "}";
                return TT_CBRACE;
            }
            else if (c == '[') {
                if (data) data->s = "[";
                return TT_OBRACKET;
            }
            else if (c == ']') {
                if (data) data->s = "]";
                return TT_CBRACKET;
            }
            else if (c == '(') {
                if (data) data->s = "(";
                return TT_OPAREN;
            }
            else if (c == ')') {
                if (data) data->s = ")";
                return TT_CPAREN;
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
                ts->error = error_msg(curr->file, curr->line, curr->column,
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
                char *file = bufGet(ts->buffer);
                FILE *fp;

                if ((fp = fopen(file, "r")) == NULL) {
                    ts->error = error_msg(curr->file, curr->line, curr->column,
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
                ts->error = error_msg(curr->file, curr->line, curr->column,
                        "Unexpected character '%c' (ascii %d)", c, c);
                return TT_ERROR;
            }
            else {
                if (data) data->s = bufGet(ts->buffer);
                return TT_USTRING;
            }
            break;
        case ST_QUOTED_STRING:
            if (c != '"') {
                bufAddC(ts->buffer, c);
            }
            else {
                if (data) data->s = bufGet(ts->buffer);
                return TT_QSTRING;
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
                ts->error = error_msg(curr->file, curr->line, curr->column,
                        "Unexpected character '%c' (ascii %d)", c, c);
                return TT_ERROR;
            }
            else {
                char *endp, *p = bufGet(ts->buffer);
                static long l;

                l = strtol(p, &endp, 0);

                if (endp == p + bufLen(ts->buffer)) {
                    if (data) data->l = l;
                    return TT_LONG;
                }
                else {
                    ts->error = error_msg(curr->file, curr->line, curr->column,
                            "Could not convert \"%s\" to an integer", p);
                    return TT_ERROR;
                }
            }
            break;
        case ST_DOUBLE:
            if (isdigit(c) || c == '.') {
                bufAddC(ts->buffer, c);
            }
            else if (!isspace(c)) {
                ts->error = error_msg(curr->file, curr->line, curr->column,
                        "Unexpected character '%c' (ascii %d)", c, c);
                return TT_ERROR;
            }
            else {
                char *endp, *p = bufGet(ts->buffer);
                static double d;

                d = strtod(p, &endp);

                if (endp == p + bufLen(ts->buffer)) {
                    if (data) data->d = d;
                    return TT_DOUBLE;
                }
                else {
                    ts->error = error_msg(curr->file, curr->line, curr->column,
                            "Could not convert \"%s\" to a double", p);
                    return TT_ERROR;
                }
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
TokenType tsExpectToken(TokenStream *ts, TokenType expected_type, TokenData *data)
{
    TokenType actual_type;

    if (ts->error) return TT_ERROR;

    actual_type = tsGetToken(ts, data);

    if (actual_type == TT_ERROR) return actual_type;

    if ((actual_type & expected_type) ||
        ((expected_type & TT_STRING) &&
            ((actual_type & TT_QSTRING) || (actual_type & TT_USTRING))) ||
        ((expected_type & TT_NUMBER) &&
            ((actual_type & TT_LONG) || (actual_type & TT_DOUBLE))))
        return actual_type;

    if (actual_type != expected_type) {
        StackFrame *sf = listHead(&ts->stack);

        ts->error = error_msg(sf->file, sf->line, sf->column,
                "Expected %s, got %s.",
                tsTokenType(expected_type),
                tsTokenType(actual_type));

        return TT_ERROR;
    }

    return actual_type;
}

/*
 * Push back a token with type <type> and TokenData <data> onto <ts>.
 */
void tsUngetToken(TokenStream *ts, TokenType type, TokenData *data)
{
    Token *token = calloc(1, sizeof(Token));

    token->type = type;

    if (data != NULL) {
        token->data = *data;

        if (type != TT_LONG && type != TT_DOUBLE) {
            token->data.s = strdup(token->data.s);
        }
    }

    listInsertHead(&ts->pushback, token);
}

/*
 * Retrieve the last error that occurred.
 */
char *tsError(TokenStream *ts)
{
    return ts->error;
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
