#ifndef TABLES_H
#define TABLES_H

/*
 * tables.h: Format text tables.
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
#include <stdbool.h>

typedef struct Table Table;

/*
 * Styles to choose from.
 */
typedef enum {
    TBL_STYLE_ASCII,    // Use only ASCII '+', '-' and '|'.
    TBL_STYLE_BOX,      // UTF8 single-line outline with square corners.
    TBL_STYLE_ROUND,    // UTF8 single-line outline with rounded corners.
    TBL_STYLE_DOUBLE,   // UTF8 double-line outline with square corners.
    TBL_STYLE_HEAVY,    // UTF8 header with heavy lines, body with light.
} TableStyle;

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
 * If <bold_headers> is true, the column titles in the header will be bolded
 * using ANSI escape sequences. <style> specifies which table style to use.
 *
 * Returns a pointer to a formatted output string, which is overwritten on each
 * call.
 */
const char *tblGetLine(Table *tbl,
        int max_width, bool bold_headers, TableStyle style);

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
