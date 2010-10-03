/*
 * A simple tokenizer.
 *
 * Copyright:	(c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: st.c 244 2007-08-13 10:58:11Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#include "buffer.h"

#include "st.h"

typedef enum {
   S_SPACE,
   S_COMMENT,
   S_LONG,
   S_DOUBLE,
   S_QUOTED_STRING,
   S_UNQUOTED_STRING
} State;

typedef struct {
   State   state;
   stItem *root;
   stItem *parent;
   stItem *last;
   char   *file;
   int     line;
} Position;

static stItem *alloc_item(Position *pos, stType type)
{
   stItem *new_item = calloc(1, sizeof(stItem));

   if (!pos->root) pos->root = new_item;
   if (pos->last)  pos->last->next = new_item;

   pos->last = new_item;

   assert(pos->parent == NULL || pos->parent->type == ST_CONTAINER);

   new_item->parent = pos->parent;
   new_item->file   = pos->file;
   new_item->line   = pos->line;
   new_item->type   = type;;

   if (new_item->parent && new_item->parent->data.c == NULL)
      new_item->parent->data.c = new_item;

   return new_item;
}

static int enter_container_item(Position *pos)
{
   stItem *new_item = alloc_item(pos, ST_CONTAINER);

   pos->parent = new_item;
   pos->last = NULL;

   return 0;
}

static int exit_container_item(Position *pos)
{
   pos->last   = pos->parent;
   pos->parent = pos->parent ? pos->parent->parent : NULL;

   return 0;
}

static int add_long_item(Buffer *buf, Position *pos)
{
   stItem *new_item = alloc_item(pos, ST_LONG);

   return (sscanf(bufGet(buf), "%ld", &new_item->data.l) != 1);
}

static int add_double_item(Buffer *buf, Position *pos)
{
   char *end_ptr;

   stItem *new_item = alloc_item(pos, ST_DOUBLE);

   new_item->data.d = strtod(bufGet(buf), &end_ptr);

   return (end_ptr - bufGet(buf) != strlen(bufGet(buf)));
}

static int add_string_item(Buffer *buf, stType type, Position *pos)
{
   stItem *new_item = alloc_item(pos, type);

   new_item->data.s = strdup(bufGet(buf));

   return 0;
}

static void write_items(FILE *fp, stItem *item, int indent);

static int ifprintf(FILE *fp, int indent, char *fmt, ...)
{
   int i, rv;

   va_list ap;

   va_start(ap, fmt);

   for (i = 0; i < indent; i++) {
      fprintf(fp, "    ");
   }

   rv = vfprintf(fp, fmt, ap);

   va_end(ap);

   return rv;
}

static void write_item(FILE *fp, stItem *item, int indent)
{
   switch(item->type) {
   case ST_QUOTED_STRING:
      ifprintf(fp, indent, "string \"%s\"\n", item->data.s);
      break;
   case ST_UNQUOTED_STRING:
      ifprintf(fp, indent, "string %s\n", item->data.s);
      break;
   case ST_LONG:
      ifprintf(fp, indent, "long   %ld\n", item->data.l);
      break;
   case ST_DOUBLE:
      ifprintf(fp, indent, "double %lg\n", item->data.d);
      break;
   case ST_CONTAINER:
      ifprintf(fp, indent, "{\n");
      write_items(fp, item->data.c, indent + 1);
      ifprintf(fp, indent, "}\n");
      break;
   }
}

static void write_items(FILE *fp, stItem *item, int indent)
{
   while (item) {
      write_item(fp, item, indent);
      item = item->next;
   }
}

/*
 * Read tokens from file with <filename> and opened as <fp>, and return the
 * first (or "root") item.
 */
stItem *stRead(FILE *fp, char *filename)
{
   char c;

   Buffer  *buffer = bufCreate();
   Position pos;

   pos.state  = S_SPACE;
   pos.root   = NULL;
   pos.parent = NULL;
   pos.last   = NULL;
   pos.file   = filename ? strdup(filename) : NULL;
   pos.line   = 1;

   while ((c = getc(fp)) != EOF) {
      if (c == '\n') pos.line++;

      switch(pos.state) {
      case S_SPACE:
         if (isdigit(c)) {
            bufAdd(buffer, &c, 1);
            pos.state = S_LONG;
         }
         else if (c == '"') {
            pos.state = S_QUOTED_STRING;
         }
         else if (c == '{') {
            enter_container_item(&pos);
         }
         else if (c == '}') {
            exit_container_item(&pos);
         }
         else if (c == '.') {
            bufAdd(buffer, &c, 1);
            pos.state = S_DOUBLE;
         }
         else if (c == '#') {
            pos.state = S_COMMENT;
         }
         else if (!isspace(c)) {
            bufAdd(buffer, &c, 1);
            pos.state = S_UNQUOTED_STRING;
         }
         break;
      case S_COMMENT:
         if (c == '\n') {
            pos.state = S_SPACE;
         }
         break;
      case S_LONG:
         if (c == '.' || c == 'e' || c == 'E') {
            bufAdd(buffer, &c, 1);
            pos.state = S_DOUBLE;
         }
         else if (isspace(c)) {
            if (add_long_item(buffer, &pos) != 0) goto error_exit;

            bufClear(buffer);
            pos.state = S_SPACE;
         }
         else if (!isdigit(c)) {
            bufAdd(buffer, &c, 1);
            pos.state = S_UNQUOTED_STRING;
         }
         else {
            bufAdd(buffer, &c, 1);
         }
         break;
      case S_DOUBLE:
         if (isspace(c)) {
            if (add_double_item(buffer, &pos) != 0) goto error_exit;

            bufClear(buffer);
            pos.state = S_SPACE;
         }
         else if (c == '.' || c == 'e' || c == 'E' || isdigit(c)) {
            bufAdd(buffer, &c, 1);
         }
         else {
            goto error_exit;
         }
         break;
      case S_QUOTED_STRING:
         if (c == '"') {
            if (add_string_item(buffer, ST_QUOTED_STRING, &pos) != 0)
               goto error_exit;

            bufClear(buffer);
            pos.state = S_SPACE;
         }
         else if (c == '\\') {
	    int ch = getc(fp);

	    switch(ch) {
            case 'n': bufAddC(buffer, '\n'); break;
            case 'r': bufAddC(buffer, '\r'); break;
            case 't': bufAddC(buffer, '\t'); break;
            case '0': bufAddC(buffer, '\0'); break;
            default:  bufAddC(buffer, ch); break;
	    }
         }
         else {
            bufAdd(buffer, &c, 1);
         }
         break;
      case S_UNQUOTED_STRING:
         if (isalnum(c) || c == '_') {
            bufAdd(buffer, &c, 1);
         }
	 else {
            if (add_string_item(buffer, ST_UNQUOTED_STRING, &pos) != 0)
               goto error_exit;

            bufClear(buffer);
            pos.state = S_SPACE;
         }
         break;
      }
   }

   bufDestroy(buffer);

   return pos.root;

error_exit:
   bufDestroy(buffer);
   if (pos.file) free(pos.file);
   stFree(pos.root, 1);
   return NULL;
}

/*
 * Write the items from <root> onwards nicely formatted to <fp>.
 */
void stWrite(FILE *fp, stItem *root)
{
   write_items(fp, root, 0);
}

/*
 * Free the items from <root> onwards. If <strings_too> is TRUE, the string data
 * for quoted and unquoted strings will also be freed. Otherwise they are kept
 * (so that clients can store a pointer to the string and not have to strcpy()
 * them).
 */
void stFree(stItem *root, int strings_too)
{
}

/*
 * Check that <item> is of type <type>.
 */
int stIsType(stItem *item, stType type)
{
   return (item != NULL && item->type == type);
}

/*
 * Check that <item> is a quoted string containing <text> (sans quotes).
 */
int stIsQuotedString(stItem *item, char *text)
{
   return (stIsType(item, ST_QUOTED_STRING) && strcmp(item->data.s, text) == 0);
}

/*
 * Check that <item> is an unquoted string containing <text>.
 */
int stIsUnquotedString(stItem *item, char *text)
{
   return (stIsType(item, ST_UNQUOTED_STRING) && strcmp(item->data.s, text) == 0);
}

/*
 * Check that <item> is a string (quoted or unquoted) containing <text>.
 */
int stIsString(stItem *item, char *text)
{
   return (stIsQuotedString(item, text) || stIsUnquotedString(item, text));
}

/*
 * Check that <item> is a long with value <l>.
 */
int stIsLong(stItem *item, long l)
{
   return (stIsType(item, ST_LONG) && item->data.l == l);
}

/*
 * Check that <item> is a double with value <d>.
 */
int stIsDouble(stItem *item, double d)
{
   return (stIsType(item, ST_DOUBLE) && item->data.d == d);
}

/*
 * Check that <item> has a value equivalent to <l>. Quoted strings and doubles
 * are converted to a long and compared.
 */
int stHasLongValue(stItem *item, long l)
{
   if (item == NULL) return 0;

   if (stIsLong(item, l)) return 1;
   if (item->type == ST_DOUBLE && ((long) item->data.d) == l) return 1;
   if (item->type == ST_QUOTED_STRING) {
      char *endp;
      long value = strtol(item->data.s, &endp, 10);
      if (*endp == '\0' && value == l) return 1;
   }

   return 0;
}

/*
 * Check that <item> has a value equivalent to <d>. Quoted strings and longs
 * are converted to a double and compared.
 */
int stHasDoubleValue(stItem *item, double d)
{
   if (item == NULL) return 0;

   if (stIsDouble(item, d)) return 1;
   if (item->type == ST_LONG && ((double) item->data.l) == d) return 1;
   if (item->type == ST_QUOTED_STRING) {
      char *endp;
      double value = strtod(item->data.s, &endp);
      if (*endp == '\0' && value == d) return 1;
   }

   return 0;
}

/*
 * Check that <item> is a container.
 */
int stIsContainer(stItem *item)
{
   return (stIsType(item, ST_CONTAINER));
}
