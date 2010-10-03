/*
 * A simple tokenizer.
 *
 * Copyright:	(c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: st.h 242 2007-06-23 23:12:05Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifndef ST_H
#define ST_H

#include <stdio.h>

typedef enum {
   ST_QUOTED_STRING,
   ST_UNQUOTED_STRING,
   ST_LONG,
   ST_DOUBLE,
   ST_CONTAINER
} stType;

typedef struct stItem stItem;

struct stItem {
   stItem *next;
   stItem *parent;
   char   *file;
   int     line;
   stType  type;
   union {
      char   *s;
      long    l;
      double  d;
      stItem *c;
   } data;
};

/*
 * Read tokens from file with <filename> and opened as <fp>, and return the
 * first (or "root") item.
 */
stItem *stRead(FILE *fp, char *filename);

/*
 * Write the items from <root> onwards nicely formatted to <fp>.
 */
void stWrite(FILE *fp, stItem *root);

/*
 * Free the items from <root> onwards. If <strings_too> is TRUE, the string data
 * for quoted and unquoted strings will also be freed. Otherwise they are kept
 * (so that clients can store a pointer to the string and not have to strcpy()
 * them).
 */
void stFree(stItem *root, int strings_too);

/*
 * Check that <item> is of type <type>.
 */
int stIsType(stItem *item, stType type);

/*
 * Check that <item> is a quoted string containing <text> (sans quotes).
 */
int stIsQuotedString(stItem *item, char *text);

/*
 * Check that <item> is an unquoted string containing <text>.
 */
int stIsUnquotedString(stItem *item, char *text);

/*
 * Check that <item> is a string (quoted or unquoted) containing <text>.
 */
int stIsString(stItem *item, char *text);

/*
 * Check that <item> is a long with value <l>.
 */
int stIsLong(stItem *item, long l);

/*
 * Check that <item> is a double with value <d>.
 */
int stIsDouble(stItem *item, double d);

/*
 * Check that <item> has a value equivalent to <l>. Quoted strings and doubles
 * are converted to a long and compared.
 */
int stHasLongValue(stItem *item, long l);

/*
 * Check that <item> has a value equivalent to <d>. Quoted strings and longs
 * are converted to a double and compared.
 */
int stHasDoubleValue(stItem *item, double d);

/*
 * Check that <item> is a container.
 */
int stIsContainer(stItem *item);

#endif
