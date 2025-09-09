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

/*
 * Create and return a new table.
 */
Table *tblCreate(void);

/*
 * Set the header above column <col> in table <tbl> to the string defined by
 * <fmt> and the parameters in <ap>.
 */
int tblVaSetHeader(Table *tbl, int col, const char *fmt, va_list ap);

/*
 * Set the header above column <col> in table <tbl> to the string defined by
 * <fmt> and the subsequent parameters.
 */
__attribute__((format (printf, 3, 4)))
int tblSetHeader(Table *tbl, int col, const char *fmt, ...);

/*
 * Set the cell at row <row> and column <col> in table <tbl> to the string
 * defined by <fmt> and the parameters in <ap>.
 */
int tblVaSetCell(Table *tbl, int row, int col,
                  const char *fmt, va_list ap);

/*
 * Set the cell at row <row> and column <col> in table <tbl> to the string
 * defined by <fmt> and the subsequent parameters.
 */
__attribute__((format (printf, 4, 5)))
int tblSetCell(Table *tbl, int row, int col, const char *fmt, ...);

/*
 * Get subsequent lines to print <tbl>. This function will return, at every
 * call, sequential character strings to print the given table. If there are no
 * more lines to print it will return NULL.
 *
 * <flags> is the bitwise-or of:
 * - TBL_BOX_CHARS:     Use graphical (UTF-8) box characters for a nicer output
 *                      format. If not given, basic ASCII characters are used.
 * - TBL_BOLD_TITLES:   Use ANSI escape sequences to print (only!) column
 *                      headers in bold.
 *
 * Returns a pointer to a formatted output string, which is overwritten on each
 * call.
 */
const char *tblGetLine(Table *tbl, TableFlags flags);

/*
 * Rewind the output of table <tbl> back to the start. On the next call to
 * tblGetLine(), the first output line will (again) be returned.
 */
void tblRewind(Table *tbl);

/*
 * Detroy table <tbl>. All internal data, including the most recently returned
 * output line will be destroyed.
 */
void tblDestroy(Table *tbl);

#endif
