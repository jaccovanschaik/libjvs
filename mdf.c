/*
 * mdf.c: Minimal Data Format parser.
 *
 * A data file consists of a sequence of name/value pairs. Names are unquoted
 * strings, starting with a letter or underscore and followed by any number of
 * letters, underscores or digits. Values are any of the following:
 *
 * - A double-quoted string;
 * - A long integer (Hexadecimal if starting with 0x, octal if starting with 0,
 *   otherwise decimal);
 * - A double-precision float;
 * - A container, started with { and ended with }, containing a new sequence of
 *   name/value pairs.
 *
 * Data to be read can be given as a filename, a file descriptor, a FILE pointer
 * or a character string. The first object in the data is returned to the caller
 * as an MDF_Object, and the caller can go to subsequent objects by following a
 * "next" pointer. Contents of the objects are stored in a union based on the
 * type of object described above.
 *
 * mdf.c is part of libjvs.
 *
 * Copyright:   (c) 2013-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: mdf.c 507 2025-08-23 14:43:51Z jacco $
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

#include "mdf.h"

/* State of the parser. */

typedef enum {
    MDF_STATE_NONE,
    MDF_STATE_COMMENT,
    MDF_STATE_NAME,
    MDF_STATE_STRING,
    MDF_STATE_ESCAPE,
    MDF_STATE_NUMBER,
    MDF_STATE_ERROR,
    MDF_STATE_END
} MDF_State;

/* Type of the stream. */

typedef enum {
    MDF_ST_NONE,
    MDF_ST_FILE,     /* The caller gave me a filename. */
    MDF_ST_FD,       /* The caller gave me a file descriptor. */
    MDF_ST_FP,       /* The caller gave me a FILE pointer. */
    MDF_ST_STRING    /* The caller gave me a string to parse. */
} MDF_StreamType;

/* Definition of a stream. */

struct MDF_Stream {
    MDF_StreamType type;
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
static void mdf_unget_char(MDF_Stream *stream, int c)
{
    if (c == EOF) return;

    if (stream->type == MDF_ST_STRING) {
        stream->u.str--;

        /* We can't overwrite the string that the user has given us since it's
         * declared const, so we'll just assert that the character we're
         * pushing back is the same that was already there. */

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
static int mdf_get_char(MDF_Stream *stream)
{
    int c;

    if (stream->type == MDF_ST_STRING) {
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
        int c2 = mdf_get_char(stream);

        if (c2 != '\n') {
            mdf_unget_char(stream, c2);
        }

        c = '\n';
    }

    if (c == '\n') stream->line++;

    return c;
}

/*
 * Generate an error message about unexpected character <c> (which may be EOF)
 * on <stream>.
 */
static void mdf_unexpected(MDF_Stream *stream, int c)
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
static int mdf_interpret_number(const char *value, MDF_Object *obj)
{
    long i;
    double f;

    char *end;

    i = strtol(value, &end, 0);

    if (end == value + strlen(value)) {
        obj->type = MDF_INT;
        obj->u.i = i;

        return 0;
    }

    f = strtod(value, &end);

    if (end == value + strlen(value)) {
        obj->type = MDF_FLOAT;
        obj->u.f = f;

        return 0;
    }

    return -1;
}

/*
 * Create and return an MDF_Object of type <type>.
 */
static MDF_Object *mdf_new_object(MDF_Type type)
{
    MDF_Object *obj = calloc(1, sizeof(MDF_Object));

    obj->type = type;

    return obj;
}

/*
 * Create an object with type <type> and name <name>, which was found in file
 * <file> on line <line>. <root> and <last> are pointers to the addresses of
 * the first and last elements in the list to which the new object must be
 * added, and they will be updated as necessary.
 */
static MDF_Object *mdf_add_object(MDF_Type type, const Buffer *name,
                                const char *file, int line,
                                MDF_Object **root, MDF_Object **last)
{
    MDF_Object *obj = mdf_new_object(type);

    obj->line = line;
    obj->file = file;

    /* If a name was given use it. Otherwise, if there is a preceding object
     * copy its name. Otherwise simply set the name to NULL. */

    if (name != NULL && bufLen(name) != 0)
        obj->name = strdup(bufGet(name));
    else if (*last != NULL && (*last)->name != NULL)
        obj->name = strdup((*last)->name);

    /* If there is no preceding object, this new object is the first in a new
     * list which makes it the root of the new list. */

    if (*last == NULL)
        *root = obj;
    else
        (*last)->next = obj;

    /* In any case, this object is the last in the current list. */

    *last = obj;

    return obj;
}

/*
 * Parse input stream <stream> and return the first element in the list of
 * objects that was found. <nesting_level> is the nesting level (w.r.t.
 * braces) we're currently at.
 */
static MDF_Object *mdf_parse(MDF_Stream *stream, int nesting_level)
{
    int c;

    MDF_State state = MDF_STATE_NONE;   /* Current parser state. */

    MDF_Object *root = NULL;            /* First element of current obj list. */
    MDF_Object *last = NULL;            /* Last element of current obj list. */

    Buffer name = { 0 };                /* Name of current object. */
    Buffer value = { 0 };               /* Value of current object. */

    while (1) {
        if (state == MDF_STATE_ERROR || state == MDF_STATE_END) break;

        c = mdf_get_char(stream);

        switch(state) {
        case MDF_STATE_NONE:
            /* We're in whitespace. Let's see what we find. */

            if (c =='#') {
                state = MDF_STATE_COMMENT;
            }
            else if (isalpha(c) || c == '_') {
                bufSetC(&name, c);
                state = MDF_STATE_NAME;
            }
            else if (c == '+' || c == '-' || c == '.' || isdigit(c)) {
                bufSetC(&value, c);
                state = MDF_STATE_NUMBER;
            }
            else if (c == '"') {
                state = MDF_STATE_STRING;
                bufClear(&value);
            }
            else if (c == '{') {
                /* Parse the remainder of this container, including the
                 * closing brace. */

                MDF_Object *obj =
                    mdf_add_object(MDF_CONTAINER, &name,
                            stream->file, stream->line, &root, &last);

                if ((obj->u.c = mdf_parse(stream, nesting_level + 1)) == NULL) {
                    if (bufIsEmpty(&stream->error)) {
                        state = MDF_STATE_NONE;
                    }
                    else {
                        state = MDF_STATE_ERROR;
                    }
                }
            }
            else if (c == '}') {
                if (nesting_level > 0) {    /* OK, end of this container. */
                    state = MDF_STATE_END;
                }
                else {  /* We're at top-level, what's this doing here?! */
                    bufSetF(&stream->error, "%s:%d: unbalanced '}'",
                            stream->file, stream->line);
                    state = MDF_STATE_ERROR;
                }
            }
            else if (c == EOF && nesting_level == 0) {
                state = MDF_STATE_END;
            }
            else if (!isspace(c)) {
                mdf_unexpected(stream, c);
                state = MDF_STATE_ERROR;
            }
            else {
                /* OK, another whitespace character. Just keep going. */
            }
            break;
        case MDF_STATE_COMMENT:
            /* We're in a comment. Just keep going until the end of the line,
             * or the file. */

            if (c == '\n') {
                state = MDF_STATE_NONE;
            }
            else if (c == EOF) {
                state = MDF_STATE_END;
            }
            else {
                /* Anything else just adds to the comment. */
            }
            break;
        case MDF_STATE_NAME:
            /* We're in a name. We'll accept underscores, letters and digits. */

            if (c == '_' || isalnum(c)) {
                bufAddC(&name, c);
            }
            else if (isspace(c) || c == '{' || c == '}') {
                mdf_unget_char(stream, c);
                state = MDF_STATE_NONE;
            }
            else {
                mdf_unexpected(stream, c);
                state = MDF_STATE_ERROR;
            }
            break;
        case MDF_STATE_STRING:
            /* We're in a string. Keep going until the closing quote (but:
             * escape sequences!) */

            if (c == '\\') {
                state = MDF_STATE_ESCAPE;
            }
            else if (c == '"') {
                MDF_Object *obj =
                    mdf_add_object(MDF_STRING, &name,
                            stream->file, stream->line, &root, &last);

                obj->u.s = strdup(bufGet(&value));
                state = MDF_STATE_NONE;
            }
            else if (isprint(c)) {
                bufAddC(&value, c);
            }
            else {
                mdf_unexpected(stream, c);
                state = MDF_STATE_ERROR;
            }
            break;
        case MDF_STATE_ESCAPE:
            /* Escape sequence encountered. Interpret it. */

            if (c == 't') {
                bufAddC(&value, '\t');
                state = MDF_STATE_STRING;
            }
            else if (c == 'r') {
                bufAddC(&value, '\r');
                state = MDF_STATE_STRING;
            }
            else if (c == 'n') {
                bufAddC(&value, '\n');
                state = MDF_STATE_STRING;
            }
            else if (c == '"') {
                bufAddC(&value, '"');
                state = MDF_STATE_STRING;
            }
            else if (c == '\\') {
                bufAddC(&value, '\\');
                state = MDF_STATE_STRING;
            }
            else {
                bufSetF(&stream->error,
                        "%s:%d: invalid escape sequence \"\\%c\"",
                        stream->file, stream->line, c);
                state = MDF_STATE_ERROR;
            }
            break;
        case MDF_STATE_NUMBER:
            /* A number (float or int). Accept anything that can occur in a
             * number (floating point, octal or hex) and let strtod or strtol
             * sort it out later. */

            if (isxdigit(c) || c == 'x' || c == '.' ||
                c == 'e' || c == 'E' || c == '+' || c == '-') {
                bufAddC(&value, c);
            }
            else if (isspace(c) || c == '{' || c == '}' || c == EOF) {
                MDF_Object *obj =
                    mdf_add_object(MDF_INT, &name, stream->file,
                            c == '\n' ? stream->line - 1 : stream->line,
                            &root, &last);

                if (mdf_interpret_number(bufGet(&value), obj) == 0) {
                    state = MDF_STATE_NONE;
                }
                else {
                    bufSetF(&stream->error, "%s:%d: unrecognized value \"%s\"",
                            stream->file, stream->line, bufGet(&value));
                    state = MDF_STATE_ERROR;
                }

                mdf_unget_char(stream, c);
            }
            else {
                mdf_unexpected(stream, c);
                state = MDF_STATE_ERROR;
            }
            break;
        default:
            abort();
        }
    }

    bufClear(&name);
    bufClear(&value);

    if (state == MDF_STATE_ERROR) {
        mdfFree(root);
        return NULL;
    }
    else {
        return root;
    }
}

/*
 * Create and return a stream of type <type>.
 */
static MDF_Stream *mdf_create_stream(MDF_StreamType type)
{
    MDF_Stream *stream = calloc(1, sizeof(MDF_Stream));

    stream->type = type;

    return stream;
}

/*
 * Describe the type of stream <stream>.
 */
static char *mdf_describe_stream(MDF_Stream *stream)
{
    struct stat statbuf;

    if (stream->type == MDF_ST_STRING) {
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
 * Create and return an MDF_Stream, using data from file <filename>.
 */
MDF_Stream *mdfOpenFile(const char *filename)
{
    MDF_Stream *stream = mdf_create_stream(MDF_ST_FILE);

    if ((stream->u.fp = fopen(filename, "r")) == NULL) {
        mdfClose(stream);
        return NULL;
    }

    stream->file = strdup(filename);

    return stream;
}

/*
 * Create and return an MDF_Stream, using data from FILE pointer <fp>.
 */
MDF_Stream *mdfOpenFP(FILE *fp)
{
    MDF_Stream *stream = mdf_create_stream(MDF_ST_FP);

    stream->u.fp = fp;
    stream->file = strdup(mdf_describe_stream(stream));

    return stream;
}

/*
 * Create and return an MDF_Stream, using data from file descriptor <fd>.
 */
MDF_Stream *mdfOpenFD(int fd)
{
    MDF_Stream *stream = mdf_create_stream(MDF_ST_FD);

    // Create a duplicate of fd so the original remains open when we call
    // fclose on stream->u.fp.

    fd = dup(fd);

    if ((stream->u.fp = fdopen(fd, "r")) == NULL) {
        mdfClose(stream);
        return NULL;
    }

    stream->file = strdup(mdf_describe_stream(stream));

    return stream;
}

/*
 * Create and return an MDF_Stream, using data from string <string>.
 */
MDF_Stream *mdfOpenString(const char *string)
{
    MDF_Stream *stream = mdf_create_stream(MDF_ST_STRING);

    stream->u.str = string;
    stream->file = strdup(mdf_describe_stream(stream));

    return stream;
}

/*
 * Parse <stream>, returning the first of the found objects.
 */
MDF_Object *mdfParse(MDF_Stream *stream)
{
    if (stream == NULL) return NULL;

    stream->line = 1;

    return mdf_parse(stream, 0);
}

/*
 * Return type <type> as a string.
 */
const char *mdfTypeName(const MDF_Type type)
{
    switch(type) {
    case MDF_STRING:
        return "string";
    case MDF_INT:
        return "int";
    case MDF_FLOAT:
        return "float";
    case MDF_CONTAINER:
        return "container";
    default:
        return NULL;
    }
}

/*
 * Retrieve an error text from <stream>, in case any function has returned an
 * error.
 */
const char *mdfError(const MDF_Stream *stream)
{
    if (stream == NULL)
        return NULL;
    else
        return bufGet(&stream->error);
}

/*
 * Free the object list starting at <root>.
 */
void mdfFree(MDF_Object *root)
{
    while (root != NULL) {
        MDF_Object *next = root->next;

        if (root->name != NULL)
            free(root->name);

        if (root->type == MDF_STRING)
            free(root->u.s);
        else if (root->type == MDF_CONTAINER)
            mdfFree(root->u.c);

        free(root);

        root = next;
    }
}

/*
 * Close <stream> and free all its memory.
 */
void mdfClose(MDF_Stream *stream)
{
    if (stream->type == MDF_ST_FILE || stream->type == MDF_ST_FD) {
        // These I have opened myself, so I should close them too.
        if (stream->u.fp != NULL) fclose(stream->u.fp);
    }

    bufRewind(&stream->error);
    free(stream->file);

    free(stream);
}
