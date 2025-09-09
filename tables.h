#ifndef TABLES_H
#define TABLES_H

/*
 * tables.h: XXX
 *
 * Copyright: (c) 2025 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2025-09-09
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 *
 * vim: softtabstop=4 shiftwidth=4 expandtab textwidth=80
 */

#include <stdlib.h>
#include <stdarg.h>

typedef struct Table Table;

typedef enum {
    TBL_BOLD_TITLES = 0x1,
    TBL_BOX_CHARS   = 0x2
} TableFlags;

Table *tblCreate(void);

int tblVaSetColumn(Table *tbl, int col, const char *fmt, va_list ap);

int tblSetColumn(Table *tbl, int col, const char *fmt, ...);

int tblVaSetCell(Table *tbl, int row, int col,
                  const char *fmt, va_list ap);

int tblSetCell(Table *tbl, int row, int col, const char *fmt, ...);

void tblRewind(Table *tbl);

const char *tblGetLine(Table *tbl, TableFlags flags);

#endif
