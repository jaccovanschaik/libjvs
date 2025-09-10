/*
 * tables.c: Format text tables.
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

typedef enum {
    INITIAL,
    TOP_OF_HEADER,
    TEXT_OF_HEADER,
    BOTTOM_OF_HEADER,
    TRANSITION,
    TOP_OF_BODY,
    TEXT_OF_BODY,
    BOTTOM_OF_BODY,
    FINAL
} OutputState;

/*
 * Data for a table.
 */
struct Table {
    int    rows, cols;
    int   *width;
    int    next_line;
    OutputState state;
    Buffer line;
    char **title;
    char **cell;
};

enum {
    HDR_TOP_LEFT, HDR_TOP_EDGE, HDR_TOP_SEP, HDR_TOP_RIGHT,
    HDR_TEXT_LEFT, HDR_TEXT_SEP, HDR_TEXT_RIGHT,
    HDR_BOTTOM_LEFT, HDR_BOTTOM_EDGE, HDR_BOTTOM_SEP, HDR_BOTTOM_RIGHT,
    TRANS_LEFT, TRANS_EDGE, TRANS_SEP, TRANS_RIGHT,
    BODY_TOP_LEFT, BODY_TOP_EDGE, BODY_TOP_SEP, BODY_TOP_RIGHT,
    BODY_TEXT_LEFT, BODY_TEXT_SEP, BODY_TEXT_RIGHT,
    BODY_BOTTOM_LEFT, BODY_BOTTOM_EDGE, BODY_BOTTOM_SEP, BODY_BOTTOM_RIGHT
};

#if 0
/*
 * "Special" characters for box graphics.
 */
enum {
    TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT,
    TOP_CROSS, BOTTOM_CROSS, LEFT_CROSS, RIGHT_CROSS, CENTER_CROSS,
    TOP_EDGE, BOTTOM_EDGE, LEFT_EDGE, RIGHT_EDGE, CENTER_HOR, CENTER_VER,
    EDGE_COUNT
};
#endif

/*
 * Use these characters (strings, really) when outputting ASCII-only graphics.
 */
static const char *fmt_ascii[] = {
    [HDR_TOP_LEFT]      = "+",  // -> +---+---+---+
    [HDR_TOP_EDGE]      = "-",  //     ^^^ ^^^ ^^^
    [HDR_TOP_SEP]       = "+",  //        ^   ^
    [HDR_TOP_RIGHT]     = "+",  //                ^
    [HDR_TEXT_LEFT]     = "|",  // -> | A | B | C |
    [HDR_TEXT_SEP]      = "|",  //        ^   ^
    [HDR_TEXT_RIGHT]    = "|",  //                ^
    [HDR_BOTTOM_LEFT]   = "+",  // -> +---+---+---+
    [HDR_BOTTOM_EDGE]   = "-",  //     ^^^ ^^^ ^^^
    [HDR_BOTTOM_SEP]    = "+",  //        ^   ^
    [HDR_BOTTOM_RIGHT]  = "+",  //                ^
    [TRANS_LEFT]        = "+",  // -> +---+---+---+
    [TRANS_EDGE]        = "-",  //     ^^^ ^^^ ^^^
    [TRANS_SEP]         = "+",  //        ^   ^
    [TRANS_RIGHT]       = "+",  //                ^
    [BODY_TOP_LEFT]     = "+",  // -> +---+---+---+
    [BODY_TOP_EDGE]     = "-",  //     ^^^ ^^^ ^^^
    [BODY_TOP_SEP]      = "+",  //        ^   ^
    [BODY_TOP_RIGHT]    = "+",  //                ^
    [BODY_TEXT_LEFT]    = "|",  // -> | X | Y | Z |
    [BODY_TEXT_SEP]     = "|",  //        ^   ^
    [BODY_TEXT_RIGHT]   = "|",  //                ^
    [BODY_BOTTOM_LEFT]  = "+",  // -> +---+---+---+
    [BODY_BOTTOM_EDGE]  = "-",  //     ^^^ ^^^ ^^^
    [BODY_BOTTOM_SEP]   = "+",  //        ^   ^
    [BODY_BOTTOM_RIGHT] = "+",  //                ^
};

/*
 * And use these when outputting box graphics. These are all UTF-8 multi-byte
 * characters.
 */
static const char *fmt_utf8_square[] = {
    [HDR_TOP_LEFT]      = "┌",  // -> +---+---+---+
    [HDR_TOP_EDGE]      = "─",  //     ^^^ ^^^ ^^^
    [HDR_TOP_SEP]       = "│",  //        ^   ^
    [HDR_TOP_RIGHT]     = "┐",  //                ^
    [HDR_TEXT_LEFT]     = "│",  // -> | A | B | C |
    [HDR_TEXT_SEP]      = "│",  //        ^   ^
    [HDR_TEXT_RIGHT]    = "│",  //                ^
    [HDR_BOTTOM_LEFT]   = "└",  // -> +---+---+---+
    [HDR_BOTTOM_EDGE]   = "─",  //     ^^^ ^^^ ^^^
    [HDR_BOTTOM_SEP]    = "┴",  //        ^   ^
    [HDR_BOTTOM_RIGHT]  = "┘",  //                ^
    [TRANS_LEFT]        = "├",  // -> +---+---+---+
    [TRANS_EDGE]        = "─",  //     ^^^ ^^^ ^^^
    [TRANS_SEP]         = "┼",  //        ^   ^
    [TRANS_RIGHT]       = "┤",  //                ^
    [BODY_TOP_LEFT]     = "┌",  // -> +---+---+---+
    [BODY_TOP_EDGE]     = "─",  //     ^^^ ^^^ ^^^
    [BODY_TOP_SEP]      = "┬",  //        ^   ^
    [BODY_TOP_RIGHT]    = "┐",  //                ^
    [BODY_TEXT_LEFT]    = "│",  // -> | X | Y | Z |
    [BODY_TEXT_SEP]     = "│",  //        ^   ^
    [BODY_TEXT_RIGHT]   = "│",  //                ^
    [BODY_BOTTOM_LEFT]  = "└",  // -> +---+---+---+
    [BODY_BOTTOM_EDGE]  = "─",  //     ^^^ ^^^ ^^^
    [BODY_BOTTOM_SEP]   = "┴",  //        ^   ^
    [BODY_BOTTOM_RIGHT] = "┘",  //                ^
};

/*
 * And use these when outputting box graphics. These are all UTF-8 multi-byte
 * characters.
 */
static const char *fmt_utf8_round[] = {
    [HDR_TOP_LEFT]      = "╭",  // -> +---+---+---+
    [HDR_TOP_EDGE]      = "─",  //     ^^^ ^^^ ^^^
    [HDR_TOP_SEP]       = "│",  //        ^   ^
    [HDR_TOP_RIGHT]     = "╮",  //                ^
    [HDR_TEXT_LEFT]     = "│",  // -> | A | B | C |
    [HDR_TEXT_SEP]      = "│",  //        ^   ^
    [HDR_TEXT_RIGHT]    = "│",  //                ^
    [HDR_BOTTOM_LEFT]   = "╰",  // -> +---+---+---+
    [HDR_BOTTOM_EDGE]   = "─",  //     ^^^ ^^^ ^^^
    [HDR_BOTTOM_SEP]    = "┴",  //        ^   ^
    [HDR_BOTTOM_RIGHT]  = "╯",  //                ^
    [TRANS_LEFT]        = "├",  // -> +---+---+---+
    [TRANS_EDGE]        = "─",  //     ^^^ ^^^ ^^^
    [TRANS_SEP]         = "┼",  //        ^   ^
    [TRANS_RIGHT]       = "┤",  //                ^
    [BODY_TOP_LEFT]     = "╭",  // -> +---+---+---+
    [BODY_TOP_EDGE]     = "─",  //     ^^^ ^^^ ^^^
    [BODY_TOP_SEP]      = "┬",  //        ^   ^
    [BODY_TOP_RIGHT]    = "╮",  //                ^
    [BODY_TEXT_LEFT]    = "│",  // -> | X | Y | Z |
    [BODY_TEXT_SEP]     = "│",  //        ^   ^
    [BODY_TEXT_RIGHT]   = "│",  //                ^
    [BODY_BOTTOM_LEFT]  = "╰",  // -> +---+---+---+
    [BODY_BOTTOM_EDGE]  = "─",  //     ^^^ ^^^ ^^^
    [BODY_BOTTOM_SEP]   = "┴",  //        ^   ^
    [BODY_BOTTOM_RIGHT] = "╯",  //                ^
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
        if (col > 0) bufAddS(line, center_edge);

        if (text[col] == NULL || text[col][0] == '\0') {
            bufAddF(line, " %*s ", width[col], "");
        }
        else {
            bufAddF(line, " %s%-*s%s ",
                    bold ? "\033[1m" : "",
                    (int) utf8_field_width(text[col], width[col]), text[col],
                    bold ? "\033[0m" : "");
        }
    }

    bufAddS(line, right_edge);
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
        if (col > 0) bufAddS(line, center_cross);

        for (int c = 0; c < width[col] + 2; c++) {
            bufAddS(line, center_edge);
        }
    }

    bufAddS(line, right_edge);
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
 * - TBL_SQUARE_BOX:     Use graphical (UTF-8) box characters for a nicer output
 *                      format. If not given, basic ASCII characters are used.
 * - TBL_BOLD_TITLES:   Use ANSI escape sequences to print (only!) column
 *                      headers in bold.
 *
 * Returns a pointer to a formatted output string, which is overwritten on each
 * call.
 */
const char *tblGetLine(Table *tbl, bool bold_headers, TableFormat format)
{
    const char **edge;

    switch (format) {
    case TBL_FMT_ASCII:
        edge = fmt_ascii;
        break;
    case TBL_FMT_UTF8_SQUARE:
        edge = fmt_utf8_square;
        break;
    case TBL_FMT_UTF8_ROUND:
        edge = fmt_utf8_round;
        break;
    }

    int row;
    bool has_header = tbl_has_header(tbl);
    bool has_body   = tbl->rows > 0;

    if (tbl->state == INITIAL) {
        if (has_header) {
            tbl->state = TOP_OF_HEADER;
        }
        else {
            tbl->state = TOP_OF_BODY;
        }
    }

    switch (tbl->state) {
    case TOP_OF_HEADER:
        tbl_format_sep(&tbl->line,
                edge[HDR_TOP_LEFT], edge[HDR_TOP_RIGHT],
                edge[HDR_TOP_EDGE], edge[HDR_TOP_SEP],
                tbl->cols, tbl->width);
        tbl->state = TEXT_OF_HEADER;
        break;
    case TEXT_OF_HEADER:
        tbl_format_text(&tbl->line, bold_headers,
                edge[HDR_TEXT_LEFT], edge[HDR_TEXT_RIGHT], edge[HDR_TEXT_SEP],
                tbl->cols, tbl->width, tbl->title);
        if (has_body) {
            tbl->state = TRANSITION;
        }
        else {
            tbl->state = BOTTOM_OF_HEADER;
        }
        break;
    case BOTTOM_OF_HEADER:
        tbl_format_sep(&tbl->line,
                edge[HDR_BOTTOM_LEFT], edge[HDR_BOTTOM_RIGHT],
                edge[HDR_BOTTOM_EDGE], edge[HDR_BOTTOM_SEP],
                tbl->cols, tbl->width);
        tbl->state = FINAL;
        break;
    case TRANSITION:
        tbl_format_sep(&tbl->line,
                edge[TRANS_LEFT], edge[TRANS_RIGHT],
                edge[TRANS_EDGE], edge[TRANS_SEP],
                tbl->cols, tbl->width);
        tbl->state = TEXT_OF_BODY;
        break;
    case TOP_OF_BODY:
        tbl_format_sep(&tbl->line,
                edge[BODY_TOP_LEFT], edge[BODY_TOP_RIGHT],
                edge[BODY_TOP_EDGE], edge[BODY_TOP_SEP],
                tbl->cols, tbl->width);

        if (has_body) {
            tbl->state = TEXT_OF_BODY;
        }
        else {
            tbl->state = BOTTOM_OF_BODY;
        }

        break;
    case TEXT_OF_BODY:
        if (has_header) {
            row = tbl->next_line - 3;
        }
        else {
            row = tbl->next_line - 1;
        }

        tbl_format_text(&tbl->line, false,
                edge[HDR_TEXT_LEFT], edge[HDR_TEXT_RIGHT], edge[HDR_TEXT_SEP],
                tbl->cols, tbl->width, tbl->cell + tbl->cols * row);

        if (row + 1 >= tbl->rows) {
            tbl->state = BOTTOM_OF_BODY;
        }
        break;
    case BOTTOM_OF_BODY:
        tbl_format_sep(&tbl->line,
                edge[BODY_BOTTOM_LEFT], edge[BODY_BOTTOM_RIGHT],
                edge[BODY_BOTTOM_EDGE], edge[BODY_BOTTOM_SEP],
                tbl->cols, tbl->width);
        tbl->state = FINAL;
        break;
    case FINAL:
        return NULL;
    default:
        break;
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
    tbl->state = INITIAL;
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
