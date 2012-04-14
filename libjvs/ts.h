#ifndef TS_H
#define TS_H

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

typedef enum {
   TT_NONE     = 0x0000,
   TT_USTRING  = 0x0001,
   TT_QSTRING  = 0x0002,
   TT_STRING   = 0x0003,
   TT_LONG     = 0x0004,
   TT_DOUBLE   = 0x0008,
   TT_NUMBER   = 0x000C,
   TT_OBRACE   = 0x0010,
   TT_CBRACE   = 0x0020,
   TT_OBRACKET = 0x0040,
   TT_CBRACKET = 0x0080,
   TT_OPAREN   = 0x0100,
   TT_CPAREN   = 0x0200,
   TT_EOF      = 0x0400,
   TT_ERROR    = 0x0800,
   TT_ALL      = 0x0FFF
} TokenType;

typedef struct TokenStream TokenStream;

/*
 * Return a textual representation of bitmask <mask>. Each set bit will be
 * named.
 */
const char *tsTypeString(TokenType mask);

/*
 * Return a textual representation of the data in the last token.
 */
const char *tsDataString(TokenStream *ts);

/*
 * Open a token stream, reading from <file>.
 */
TokenStream *tsOpen(const char *file);

/*
 * Close the token stream <ts>.
 */
void tsClose(TokenStream *ts);

/*
 * Connect a token stream to <fp>, which must have been opened for reading. The
 * name for the top-level file is set to a generic name, deduced from the type
 * of the given file pointer. Change it with tsSetFileName().
 */
TokenStream *tsConnect(FILE *fp);

/*
 * Set the (top-level) filename for this TokenStream. Can be used together with
 * tsConnect, when the file was opened by the caller.
 */
void tsSetFileName(TokenStream *ts, const char *file);

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
TokenType tsGetToken(TokenStream *ts);

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
TokenType tsExpectToken(TokenStream *ts, TokenType expected_type);

/*
 * Push back the last read token.
 */
void tsUngetToken(TokenStream *ts);

/*
 * Retrieve the last error that occurred.
 */
const char *tsError(TokenStream *ts);

/*
 * Retrieve the datafile currently being read.
 */
char *tsFile(TokenStream *ts);

/*
 * Retrieve the line number currently being read.
 */
int tsLine(TokenStream *ts);

/*
 * Retrieve the column number currently being read.
 */
int tsColumn(TokenStream *ts);

#endif
