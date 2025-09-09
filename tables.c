/*
 * tables.c: XXX
 *
 * Copyright: (c) 2025 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2025-09-09
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 *
 * vim: softtabstop=4 shiftwidth=4 expandtab textwidth=80
 */

#include "tables.h"
#include "buffer.h"
#include "utils.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <sys/param.h>

/*
 * Data for a table.
 */
struct Table {
    int    rows, cols;
    int   *width;
    int    next_line;
    Buffer line;
    char **title;
    char **cell;
};

/*
 * "Special" characters for box graphics.
 */
enum {
    TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT,
    TOP_CROSS, BOTTOM_CROSS, LEFT_CROSS, RIGHT_CROSS, CENTER_CROSS,
    TOP_EDGE, BOTTOM_EDGE, LEFT_EDGE, RIGHT_EDGE, CENTER_HOR, CENTER_VER,
    EDGE_COUNT
};

/*
 * Use these characters (strings, really) when outputting ASCII-only graphics.
 */
static const char *ascii_edges[] = {
    [TOP_LEFT]     = "+",
    [TOP_RIGHT]    = "+",
    [BOTTOM_LEFT]  = "+",
    [BOTTOM_RIGHT] = "+",
    [TOP_CROSS]    = "+",
    [BOTTOM_CROSS] = "+",
    [LEFT_CROSS]   = "+",
    [RIGHT_CROSS]  = "+",
    [CENTER_CROSS] = "+",
    [TOP_EDGE]     = "-",
    [BOTTOM_EDGE]  = "-",
    [LEFT_EDGE]    = "|",
    [RIGHT_EDGE]   = "|",
    [CENTER_HOR]   = "-",
    [CENTER_VER]   = "|",
};

/*
 * And use these when outputting box graphics. These are all UTF-8 multi-byte
 * characters.
 */
static const char *box_edges[] = {
    [TOP_LEFT]     = "┌",
    [TOP_RIGHT]    = "┐",
    [BOTTOM_LEFT]  = "└",
    [BOTTOM_RIGHT] = "┘",
    [TOP_CROSS]    = "┬",
    [BOTTOM_CROSS] = "┴",
    [LEFT_CROSS]   = "├",
    [RIGHT_CROSS]  = "┤",
    [CENTER_CROSS] = "┼",
    [TOP_EDGE]     = "─",
    [BOTTOM_EDGE]  = "─",
    [LEFT_EDGE]    = "│",
    [RIGHT_EDGE]   = "│",
    [CENTER_HOR]   = "─",
    [CENTER_VER]   = "│",
};

/*
 * Allow for <rows> rows and <cols> columns in table <tbl>. Expands all the
 * necessary data structures to hold these numbers of rows and columns. Handles
 * expansion only(!), because we never need to shrink a table.
 */
static void tbl_allow(Table *tbl, int rows, int cols)
{
    if (cols > tbl->cols) {
        tbl->title = realloc(tbl->title, cols * sizeof(char *));

        for (int col = tbl->cols; col < cols; col++) {
            tbl->title[col] = NULL;
        }

        tbl->width = realloc(tbl->width, cols * sizeof(int));

        for (int col = tbl->cols; col < cols; col++) {
            tbl->width[col] = 0;
        }
    }

    if (rows > tbl->rows || cols > tbl->cols) {
        char **new_cell = calloc(rows * cols, sizeof(char *));

        for (int row = 0; row < tbl->rows; row++) {
            for (int col = 0; col < tbl->cols; col++) {
                new_cell[col + cols * row] = tbl->cell[col + tbl->cols * row];
            }
        }

        free(tbl->cell);

        tbl->cell = new_cell;
    }

    tbl->rows = MAX(rows, tbl->rows);
    tbl->cols = MAX(cols, tbl->cols);
}

/*
 * Determine whether table <tbl> needs a header. Only necessary when at least
 * one of the columns has a defined title.
 */
static bool tbl_has_header(Table *tbl)
{
    for (int col = 0; col < tbl->cols; col++) {
        if (tbl->title[col] != NULL) return true;
    }

    return false;
}

/*
 * Format one line of output text and put it in <line>. If <bold> is true, the
 * text fields will be bolded using an ANSI escape sequence. The <left_edge>
 * string will be used for the left edge of the line, <right_edge> will be used
 * for the right edge, and <center_edge> will be used as the separator between
 * columns. The number of columns is <cols>. For each column, the width that it
 * should be given is in <width>, and its text content is in <text>.
 */
static void tbl_format_text(Buffer *line, bool bold,
        const char *left_edge, const char *right_edge, const char *center_edge,
        int cols, int *width, char *const *text)
{
    bufRewind(line);

    bufAddS(line, left_edge);

    for (int col = 0; col < cols; col++) {
        if (text[col] == NULL || text[col][0] == '\0') {
            bufAddF(line, " %*s ", width[col], "");
        }
        else {
            bufAddF(line, " %s%-*s%s ",
                    bold ? "\033[1m" : "",
                    (int) utf8_field_width(text[col], width[col]), text[col],
                    bold ? "\033[0m" : "");
        }

        if (col < cols - 1) {
            bufAddS(line, center_edge);
        }
        else {
            bufAddS(line, right_edge);
        }
    }
}

/*
 * Format a separator line and put it in <line>. The string to use as the left
 * edge is in <left_edge>, the right edge is in <right_edge>, <center_cross> is
 * used to separate columns, and everywhere else (where the text fields are in
 * other lines) is filled using <center_edge>. The number of columns is <cols>,
 * and the width that each column should be given is in <width>.
 */
static void tbl_format_sep(Buffer *line,
        const char *left_edge, const char *right_edge, const char *center_edge,
        const char *center_cross, int cols, int *width)
{
    bufRewind(line);

    bufAddS(line, left_edge);

    for (int col = 0; col < cols; col++) {
        for (int c = 0; c < width[col] + 2; c++) {
            bufAddS(line, center_edge);
        }

        if (col < cols - 1) {
            bufAddS(line, center_cross);
        }
        else {
            bufAddS(line, right_edge);
        }
    }
}

/*
 * Create and return a new table.
 */
Table *tblCreate(void)
{
    Table *tbl = calloc(1, sizeof(Table));

    return tbl;
}

/*
 * Set the header above column <col> in table <tbl> to the string defined by
 * <fmt> and the parameters in <ap>.
 */
int tblVaSetHeader(Table *tbl, int col, const char *fmt, va_list ap)
{
    tbl_allow(tbl, tbl->rows, col + 1);

    free(tbl->title[col]);

    int r = vasprintf(&tbl->title[col], fmt, ap);

    r = utf8_strlen(tbl->title[col]);

    if (r > tbl->width[col]) tbl->width[col] = r;

    return r;
}

/*
 * Set the header above column <col> in table <tbl> to the string defined by
 * <fmt> and the subsequent parameters.
 */
__attribute__((format (printf, 3, 4)))
int tblSetHeader(Table *tbl, int col, const char *fmt, ...)
{
    va_list ap;
    int r;

    va_start(ap, fmt);
    r = tblVaSetHeader(tbl, col, fmt, ap);
    va_end(ap);

    return r;
}

/*
 * Set the cell at row <row> and column <col> in table <tbl> to the string
 * defined by <fmt> and the parameters in <ap>.
 */
int tblVaSetCell(Table *tbl, int row, int col,
                  const char *fmt, va_list ap)
{
    tbl_allow(tbl, row + 1, col + 1);

    free(tbl->cell[col + tbl->cols * row]);

    int r = vasprintf(&tbl->cell[col + tbl->cols * row], fmt, ap);

    r = utf8_strlen(tbl->cell[col + tbl->cols * row]);

    if (r > tbl->width[col]) tbl->width[col] = r;

    return r;
}

/*
 * Set the cell at row <row> and column <col> in table <tbl> to the string
 * defined by <fmt> and the subsequent parameters.
 */
__attribute__((format (printf, 4, 5)))
int tblSetCell(Table *tbl, int row, int col, const char *fmt, ...)
{
    va_list ap;
    int r;

    va_start(ap, fmt);
    r = tblVaSetCell(tbl, row, col, fmt, ap);
    va_end(ap);

    return r;
}

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
const char *tblGetLine(Table *tbl, TableFlags flags)
{
    const char **edge;

    if ((flags & TBL_BOX_CHARS) == 0) {
        edge = ascii_edges;
    }
    else {
        edge = box_edges;
    }

    bool has_header = tbl_has_header(tbl);

    if (has_header) {
        if (tbl->next_line == 0) {
            // top edge
            tbl_format_sep(&tbl->line,
                    edge[TOP_LEFT], edge[TOP_RIGHT], edge[TOP_EDGE],
                    edge[TOP_CROSS], tbl->cols, tbl->width);
        }
        else if (tbl->next_line == 1) {
            // header text
            tbl_format_text(&tbl->line, flags & TBL_BOLD_TITLES,
                    edge[LEFT_EDGE], edge[RIGHT_EDGE], edge[CENTER_VER],
                    tbl->cols, tbl->width, tbl->title);
        }
        else if (tbl->next_line == 2) {
            // separator line between header and body
            tbl_format_sep(&tbl->line,
                    edge[LEFT_CROSS], edge[RIGHT_CROSS], edge[CENTER_HOR],
                    edge[CENTER_CROSS], tbl->cols, tbl->width);
        }
        else if (tbl->next_line < tbl->rows + 3) {
            // body
            int row = tbl->next_line - 3;

            tbl_format_text(&tbl->line, false,
                    edge[LEFT_EDGE], edge[RIGHT_EDGE], edge[CENTER_VER],
                    tbl->cols, tbl->width, tbl->cell + tbl->cols * row);
        }
        else if (tbl->next_line == tbl->rows + 3) {
            // bottom edge
            tbl_format_sep(&tbl->line,
                    edge[BOTTOM_LEFT], edge[BOTTOM_RIGHT], edge[BOTTOM_EDGE],
                    edge[BOTTOM_CROSS], tbl->cols, tbl->width);
        }
        else {
            // past bottom edge
            return NULL;
        }
    }
    else {
        if (tbl->next_line == 0) {
            // top edge
            tbl_format_sep(&tbl->line,
                    edge[TOP_LEFT], edge[TOP_RIGHT], edge[TOP_EDGE],
                    edge[TOP_CROSS], tbl->cols, tbl->width);
        }
        else if (tbl->next_line < tbl->rows + 1) {
            // body
            int row = tbl->next_line - 1;

            tbl_format_text(&tbl->line, false,
                    edge[LEFT_EDGE], edge[RIGHT_EDGE], edge[CENTER_VER],
                    tbl->cols, tbl->width, tbl->cell + tbl->cols * row);
        }
        else if (tbl->next_line == tbl->rows + 1) {
            // bottom edge
            tbl_format_sep(&tbl->line,
                    edge[BOTTOM_LEFT], edge[BOTTOM_RIGHT], edge[BOTTOM_EDGE],
                    edge[BOTTOM_CROSS], tbl->cols, tbl->width);
        }
        else {
            // past bottom edge
            return NULL;
        }
    }

    tbl->next_line++;

    return bufGet(&tbl->line);
}

/*
 * Rewind the output of table <tbl> back to the start. On the next call to
 * tblGetLine(), the first output line will (again) be returned.
 */
void tblRewind(Table *tbl)
{
    tbl->next_line = 0;
}

/*
 * Detroy table <tbl>. All internal data, including the most recently returned
 * output line will be destroyed.
 */
void tblDestroy(Table *tbl)
{
    free(tbl->width);

    bufClear(&tbl->line);

    for (int col = 0; col < tbl->cols; col++) {
        free(tbl->title[col]);

        for (int row = 0; row < tbl->rows; row++) {
            free(tbl->cell[col + tbl->cols * row]);
        }
    }

    free(tbl->title);
    free(tbl->cell);
}
