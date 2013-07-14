/*
 * Simple data reader.
 *
 * Copyright:	(c) 2006 Jacco van Schaik (jacco@frontier.nl)
 * Version:	$Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "defs.h"

#include "sdr.h"

#define CPP "/usr/bin/cpp -traditional"

static int  line;
static char file[FILENAME_MAX];

#define ONE_INDENT "  "

typedef struct {
   char *str;
   int written, allocated;
} Buffer;

static Buffer buffer = { };

static char *buf_get(Buffer *buffer)
{
   return buffer->str;
}

static void buf_append(Buffer *buffer, int c)
{
   if (buffer->allocated == 0) {
      buffer->allocated = 32;
      buffer->str       = calloc(1, buffer->allocated);
   }
   else if (buffer->written == buffer->allocated - 1) {
      buffer->allocated *= 2;
      buffer->str = realloc(buffer->str, buffer->allocated);
   }

   buffer->str[buffer->written++] = c;
   buffer->str[buffer->written] = '\0';
}

static void buf_clear(Buffer *buffer)
{
   buffer->written = 0;
   if (buffer->str) buffer->str[0] = '\0';
}

static void buf_delete(Buffer *buffer)
{
   free(buffer->str);

   buffer->allocated = 0;
   buffer->written   = 0;
   buffer->str       = NULL;
}

static void sdr_error(char *fmt, ...)
{
   va_list ap;

   fprintf(stderr, "%s:%d: ", file, line);

   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);

   fprintf(stderr, "\n");
}

static char *get_name(int c, FILE *fp)
{
   buf_clear(&buffer);
   buf_append(&buffer, c);

   while (TRUE) {
      if ((c = fgetc(fp)) == '\n') line++;

      if (isalnum(c) || c == '_')
         buf_append(&buffer, c);
      else if (isspace(c) || c == EOF)
         break;
      else {
         sdr_error("Unexpected character '%c' (ASCII %d)", c, c);
         return NULL;
      }
   }

   return buf_get(&buffer);
}

static void get_file_position(FILE *fp)
{
   int c;

   buf_clear(&buffer);

   while (TRUE) {
      if ((c = fgetc(fp)) == '\n') {
         line++;

         break;
      }
      else
         buf_append(&buffer, c);
   }

   if (sscanf(buf_get(&buffer), " %d \"%[^\"]\"", &line, file) != 2)
      sdr_error("Couldn't decode cpp line \"%s\"", buf_get(&buffer));
}

static sdrObject *create_string_object(FILE *fp, char *name, sdrObject *parent)
{
   int c, escape_is_active = FALSE;

   sdrObject *obj = calloc(1, sizeof(sdrObject));

   obj->type = SDR_STRING;
   obj->name   = name ? strdup(name) : NULL;
   obj->file = strdup(file);
   obj->line = line;
   obj->parent = parent;

   buf_clear(&buffer);

   while (TRUE) {
      if ((c = fgetc(fp)) == '\n') line++;

      if (c == EOF) {
         sdr_error("Unexpected end of file");
         sdrFree(obj, TRUE);
         return NULL;
      }
      else if (escape_is_active) {
         switch (c) {
         case  'a': buf_append(&buffer, '\a'); break;
         case  'b': buf_append(&buffer, '\b'); break;
         case  't': buf_append(&buffer, '\t'); break;
         case  'n': buf_append(&buffer, '\n'); break;
         case  'v': buf_append(&buffer, '\v'); break;
         case  'f': buf_append(&buffer, '\f'); break;
         case  'r': buf_append(&buffer, '\r'); break;
         case '\\': buf_append(&buffer, '\\'); break;
         default:
            sdr_error("Unknown escape sequence");
            sdrFree(obj, TRUE);
            return NULL;
         }

         escape_is_active = FALSE;
      }
      else if (c == '\\')
         escape_is_active = TRUE;
      else if (c == '"')
         break;
      else {
         buf_append(&buffer, c);
      }
   }

   obj->data.s = strdup(buf_get(&buffer));

   return obj;
}

static sdrObject *create_number_object(int c, FILE *fp, char *name, sdrObject
      *parent)
{
   long l;
   double d;

   char *str, *endptr;
   int   len;

   sdrObject *obj = calloc(1, sizeof(sdrObject));

   obj->name = name ? strdup(name) : "";
   obj->file = strdup(file);
   obj->line = line;
   obj->parent = parent;

   buf_clear(&buffer);
   buf_append(&buffer, c);

   while (TRUE) {
      if ((c = fgetc(fp)) == '\n') line++;

      if (isdigit(c) || c == '.' || c == '-' || c == '+' \
            || c == 'e' || c == 'E')
         buf_append(&buffer, c);
      else if (isspace(c) || c == EOF)
         break;
      else {
         sdr_error("Unexpected character '%c' (ASCII %d)", c, c);
         sdrFree(obj, TRUE);
         return NULL;
      }
   }

   str = buf_get(&buffer);
   len = strlen(str);

   l = strtol(str, &endptr, 10);

   if (endptr - str == len) {
      obj->type = SDR_LONG;
      obj->data.l = l;

      return obj;
   }

   d = strtod(str, &endptr);

   if (endptr - str == len) {
      obj->type = SDR_DOUBLE;
      obj->data.d = d;

      return obj;
   }

   sdr_error("Badly formatted number (%s)", str);

   return NULL;
}

static sdrObject *create_container_object(FILE *fp, char *name, sdrObject *grandparent)
{
   int c;

   sdrObject *new_obj, *last = NULL, *parent = calloc(1, sizeof(sdrObject));

   parent->type = SDR_CONTAINER;
   parent->name = name ? strdup(name) : "";
   parent->file = strdup(file);
   parent->line = line;
   parent->parent = grandparent;

   name = NULL;

   while (TRUE) {
      if ((c = fgetc(fp)) == '\n') line++;

      if (c == EOF && grandparent == NULL)
         return parent;
      else if (c == '}' && grandparent != NULL)
         return parent;
      else if (c == EOF) {
         sdr_error("Unexpected end of file");
         sdrFree(parent, TRUE);
         return NULL;
      }
      else if (isalpha(c)) {
         if ((name = get_name(c, fp)) == NULL) {
            sdrFree(parent, TRUE);
            return NULL;
         }
         continue;
      }
      else if (c == '#') {
         get_file_position(fp);
         continue;
      }
      else if (c == '"')
         new_obj = create_string_object(fp, name, parent);
      else if (c == '{')
         new_obj = create_container_object(fp, name, parent);
      else if (isdigit(c) || c == '-' || c == '+' || c == '.')
         new_obj = create_number_object(c, fp, name, parent);
      else if (!isspace(c)) {
         sdr_error("Unexpected character '%c' (ASCII %d)", c, c);
         sdrFree(parent, TRUE);
         return NULL;
      }
      else
         continue;

      if (new_obj == NULL) return NULL;

      if (last == NULL)
         parent->data.o = new_obj;
      else
         last->next = new_obj;

      last = new_obj;
   }
}

sdrObject *sdrRead(char *file)
{
   char *cmd;
   sdrObject *first, *root;
   FILE *fp;

   if (access(file, R_OK) != 0) {
      sdr_error("Can't read file \"%s\" (%s)", file, strerror(errno));
      return NULL;
   }

   cmd = malloc(strlen(file) + strlen(CPP) + 4);

   sprintf(cmd, CPP " \"%s\"", file);

   if ((fp = popen(cmd, "r")) == NULL) {
      sdr_error("Couldn't run cpp (%s)", strerror(errno));
      free(cmd);
      return NULL;
   }

   free(cmd);

   if ((root = create_container_object(fp, "root", NULL)) == NULL)
      return NULL;

   fclose(fp);

   for (first = root->data.o; first; first = first->next) {
      first->parent = NULL;
   }

   first = root->data.o;

   free(root->file);
   free(root->name);
   free(root);

   return first;
}

void sdrFree(sdrObject *obj, int clear_string_data)
{
   sdrObject *next_obj;

   while (obj != NULL) {
      next_obj = obj->next;

      if (obj->type == SDR_CONTAINER) {
         sdrFree(obj->data.o, clear_string_data);
      }
      else if (obj->type == SDR_STRING && clear_string_data) {
         if (obj->data.s) free(obj->data.s);
      }

      if (obj->file) free(obj->file);
      if (obj->name) free(obj->name);

      free(obj);

      obj = next_obj;
   }

   buf_delete(&buffer);
}

static void dump_origin(FILE *fp, int spaces, sdrObject *obj)
{
   int i;

   for (i = 0; i < spaces; i++) fputc(' ', fp);

   if (obj->parent)
      fprintf(fp, " # Child of %s", obj->parent->name);
   else
      fprintf(fp, " # Toplevel");

   fprintf(fp, " (%s:%d)\n", obj->file, obj->line);
}

void sdrDump(FILE *fp, sdrObject *obj, int indent)
{
   int i, n;

   while (obj) {
      n = indent * strlen(ONE_INDENT);

      for (i = 0; i < indent; i++) fputs(ONE_INDENT, fp);

      if (obj->name) n += fprintf(fp, "%s ", obj->name);

      switch(obj->type) {
      case SDR_STRING:
         n += fprintf(fp, "\"%s\"", obj->data.s);

         dump_origin(fp, 39 - n, obj);

         break;
      case SDR_LONG:
         n += fprintf(fp, "%ld", obj->data.l);

         dump_origin(fp, 39 - n, obj);

         break;
      case SDR_DOUBLE:
         n += fprintf(fp, "%g", obj->data.d);

         dump_origin(fp, 39 - n, obj);

         break;
      case SDR_CONTAINER:
         n += fprintf(fp, "{");

         dump_origin(fp, 39 - n, obj);

         sdrDump(fp, obj->data.o, indent + 1);

         for (i = 0; i < indent; i++) fputs(ONE_INDENT, fp);

         fprintf(fp, "}\n");
         break;
      }

      obj = obj->next;
   }
}
