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

/*
 * What is the next line of output we're going to return?
 */
typedef enum {
    INITIAL,            // Initial state, nothing returned yet.
    HEADER_TOP,         // Top of the header.
    HEADER_TEXT,        // Text of the header.
    HEADER_BOTTOM,      // Bottom of the header, if we have no body.
    TRANSITION,         // Transition between header and body, if we have both.
    BODY_TOP,           // Top of the body, if we have no header.
    BODY_TEXT,          // Text of the body.
    BODY_BOTTOM,        // Bottom of the body.
    FINAL               // Final state, nothing more to return.
} OutputState;

/*
 * Data for a table.
 */
struct Table {
    int         rows, cols;     // Number of rows and columns in the table.
    int        *width;          // <cols> column widths.
    int         output_count;   // The number of lines output so far.
    OutputState output_state;   // The state of the output.
    Buffer      output_buf;     // The last returned line of output.
    char      **title;          // <cols> pointers to column headers.
    char      **cell;           // <cols> * <rows> pointers to table cells.
};

/*
 * The characters to draw the table with. "_LEFT" is the left edge of the table,
 * "_RIGHT" is the right edge. "_SEP" is the separator between columns and
 * "_FILL" (if present) is the filler between those column separators.
 */
enum {
    // Top edge of the header
    HDR_TOP_LEFT, HDR_TOP_FILL, HDR_TOP_SEP, HDR_TOP_RIGHT,
    // Header text, with the column titles
    HDR_TEXT_LEFT, HDR_TEXT_SEP, HDR_TEXT_RIGHT,
    // Bottom edge of the header, if we have a header but no body.
    HDR_BOTTOM_LEFT, HDR_BOTTOM_FILL, HDR_BOTTOM_SEP, HDR_BOTTOM_RIGHT,
    // Transition between header and body, if we have both.
    TRANS_LEFT, TRANS_FILL, TRANS_SEP, TRANS_RIGHT,
    // Top edge of the body, if we have a body but no header.
    BODY_TOP_LEFT, BODY_TOP_FILL, BODY_TOP_SEP, BODY_TOP_RIGHT,
    // Body text.
    BODY_TEXT_LEFT, BODY_TEXT_SEP, BODY_TEXT_RIGHT,
    // Bottom edge of the body.
    BODY_BOTTOM_LEFT, BODY_BOTTOM_FILL, BODY_BOTTOM_SEP, BODY_BOTTOM_RIGHT
};

/*
 * The following tables specify which characters to use to draw the table in
 * various styles. You can find examples of what they look like in the test code
 * in tables_test.c
 *
 * Unfortunately, there are limitations to the styles that can be created. For
 * example, you can have rounded corners for single lines, but not for double or
 * heavy lines. Also, you can have a heavy header and a light body, but not a
 * double line header and a single line body. The glyphs needed to display those
 * styles simply don't exist.
 *
 * That is why there is a relatively small set of fixed styles to choose from,
 * and not, for example, separate choices for line style, separator style and
 * corner style for the header and the body. It would be far too easy to pick a
 * combination of styles that there are simply no glyphs for.
 */

/*
 * Use these characters (strings, really) when outputting ASCII-only graphics.
 */
static const char *style_ascii[] = {
    [HDR_TOP_LEFT]      = "+",  // > +---+---+---+
    [HDR_TOP_FILL]      = "-",  //    ^^^ ^^^ ^^^
    [HDR_TOP_SEP]       = "+",  //       ^   ^
    [HDR_TOP_RIGHT]     = "+",  //               ^
    [HDR_TEXT_LEFT]     = "|",  // > | A | B | C |
    [HDR_TEXT_SEP]      = "|",  //       ^   ^
    [HDR_TEXT_RIGHT]    = "|",  //               ^
    [HDR_BOTTOM_LEFT]   = "+",  // > +---+---+---+
    [HDR_BOTTOM_FILL]   = "-",  //    ^^^ ^^^ ^^^
    [HDR_BOTTOM_SEP]    = "+",  //       ^   ^
    [HDR_BOTTOM_RIGHT]  = "+",  //               ^
    [TRANS_LEFT]        = "+",  // > +---+---+---+
    [TRANS_FILL]        = "-",  //    ^^^ ^^^ ^^^
    [TRANS_SEP]         = "+",  //       ^   ^
    [TRANS_RIGHT]       = "+",  //               ^
    [BODY_TOP_LEFT]     = "+",  // > +---+---+---+
    [BODY_TOP_FILL]     = "-",  //    ^^^ ^^^ ^^^
    [BODY_TOP_SEP]      = "+",  //       ^   ^
    [BODY_TOP_RIGHT]    = "+",  //               ^
    [BODY_TEXT_LEFT]    = "|",  // > | X | Y | Z |
    [BODY_TEXT_SEP]     = "|",  //       ^   ^
    [BODY_TEXT_RIGHT]   = "|",  //               ^
    [BODY_BOTTOM_LEFT]  = "+",  // > +---+---+---+
    [BODY_BOTTOM_FILL]  = "-",  //    ^^^ ^^^ ^^^
    [BODY_BOTTOM_SEP]   = "+",  //       ^   ^
    [BODY_BOTTOM_RIGHT] = "+",  //               ^
};

/*
 * Use these when outputting single-line UTF-8 box graphics with square corners.
 * These strings (and all the ones below) use UTF-8 multi-byte characters.
 */
static const char *style_box[] = {
    [HDR_TOP_LEFT]      = "┌",  // > +---+---+---+
    [HDR_TOP_FILL]      = "─",  //    ^^^ ^^^ ^^^
    [HDR_TOP_SEP]       = "┬",  //       ^   ^
    [HDR_TOP_RIGHT]     = "┐",  //               ^
    [HDR_TEXT_LEFT]     = "│",  // > | A | B | C |
    [HDR_TEXT_SEP]      = "│",  //       ^   ^
    [HDR_TEXT_RIGHT]    = "│",  //               ^
    [HDR_BOTTOM_LEFT]   = "└",  // > +---+---+---+
    [HDR_BOTTOM_FILL]   = "─",  //    ^^^ ^^^ ^^^
    [HDR_BOTTOM_SEP]    = "┴",  //       ^   ^
    [HDR_BOTTOM_RIGHT]  = "┘",  //               ^
    [TRANS_LEFT]        = "├",  // > +---+---+---+
    [TRANS_FILL]        = "─",  //    ^^^ ^^^ ^^^
    [TRANS_SEP]         = "┼",  //       ^   ^
    [TRANS_RIGHT]       = "┤",  //               ^
    [BODY_TOP_LEFT]     = "┌",  // > +---+---+---+
    [BODY_TOP_FILL]     = "─",  //    ^^^ ^^^ ^^^
    [BODY_TOP_SEP]      = "┬",  //       ^   ^
    [BODY_TOP_RIGHT]    = "┐",  //               ^
    [BODY_TEXT_LEFT]    = "│",  // > | X | Y | Z |
    [BODY_TEXT_SEP]     = "│",  //       ^   ^
    [BODY_TEXT_RIGHT]   = "│",  //               ^
    [BODY_BOTTOM_LEFT]  = "└",  // > +---+---+---+
    [BODY_BOTTOM_FILL]  = "─",  //    ^^^ ^^^ ^^^
    [BODY_BOTTOM_SEP]   = "┴",  //       ^   ^
    [BODY_BOTTOM_RIGHT] = "┘",  //               ^
};

/*
 * Use these when outputting single-line UTF-8 box graphics with round corners.
 */
static const char *style_round[] = {
    [HDR_TOP_LEFT]      = "╭",  // > +---+---+---+
    [HDR_TOP_FILL]      = "─",  //    ^^^ ^^^ ^^^
    [HDR_TOP_SEP]       = "┬",  //       ^   ^
    [HDR_TOP_RIGHT]     = "╮",  //               ^
    [HDR_TEXT_LEFT]     = "│",  // > | A | B | C |
    [HDR_TEXT_SEP]      = "│",  //       ^   ^
    [HDR_TEXT_RIGHT]    = "│",  //               ^
    [HDR_BOTTOM_LEFT]   = "╰",  // > +---+---+---+
    [HDR_BOTTOM_FILL]   = "─",  //    ^^^ ^^^ ^^^
    [HDR_BOTTOM_SEP]    = "┴",  //       ^   ^
    [HDR_BOTTOM_RIGHT]  = "╯",  //               ^
    [TRANS_LEFT]        = "├",  // > +---+---+---+
    [TRANS_FILL]        = "─",  //    ^^^ ^^^ ^^^
    [TRANS_SEP]         = "┼",  //       ^   ^
    [TRANS_RIGHT]       = "┤",  //               ^
    [BODY_TOP_LEFT]     = "╭",  // > +---+---+---+
    [BODY_TOP_FILL]     = "─",  //    ^^^ ^^^ ^^^
    [BODY_TOP_SEP]      = "┬",  //       ^   ^
    [BODY_TOP_RIGHT]    = "╮",  //               ^
    [BODY_TEXT_LEFT]    = "│",  // > | X | Y | Z |
    [BODY_TEXT_SEP]     = "│",  //       ^   ^
    [BODY_TEXT_RIGHT]   = "│",  //               ^
    [BODY_BOTTOM_LEFT]  = "╰",  // > +---+---+---+
    [BODY_BOTTOM_FILL]  = "─",  //    ^^^ ^^^ ^^^
    [BODY_BOTTOM_SEP]   = "┴",  //       ^   ^
    [BODY_BOTTOM_RIGHT] = "╯",  //               ^
};

/*
 * Use these when outputting double-line UTF-8 box graphics. The outside of the
 * table and the transition between header and body are double-lined, the
 * separators between columns are still single-line.
 */
static const char *style_double[] = {
    [HDR_TOP_LEFT]      = "╔",  // > +---+---+---+
    [HDR_TOP_FILL]      = "═",  //    ^^^ ^^^ ^^^
    [HDR_TOP_SEP]       = "╤",  //       ^   ^
    [HDR_TOP_RIGHT]     = "╗",  //               ^
    [HDR_TEXT_LEFT]     = "║",  // > | A | B | C |
    [HDR_TEXT_SEP]      = "│",  //       ^   ^
    [HDR_TEXT_RIGHT]    = "║",  //               ^
    [HDR_BOTTOM_LEFT]   = "╚",  // > +---+---+---+
    [HDR_BOTTOM_FILL]   = "═",  //    ^^^ ^^^ ^^^
    [HDR_BOTTOM_SEP]    = "╧",  //       ^   ^
    [HDR_BOTTOM_RIGHT]  = "╝",  //               ^
    [TRANS_LEFT]        = "╠",  // > +---+---+---+
    [TRANS_FILL]        = "═",  //    ^^^ ^^^ ^^^
    [TRANS_SEP]         = "╪",  //       ^   ^
    [TRANS_RIGHT]       = "╣",  //               ^
    [BODY_TOP_LEFT]     = "╔",  // > +---+---+---+
    [BODY_TOP_FILL]     = "═",  //    ^^^ ^^^ ^^^
    [BODY_TOP_SEP]      = "╤",  //       ^   ^
    [BODY_TOP_RIGHT]    = "╗",  //               ^
    [BODY_TEXT_LEFT]    = "║",  // > | X | Y | Z |
    [BODY_TEXT_SEP]     = "│",  //       ^   ^
    [BODY_TEXT_RIGHT]   = "║",  //               ^
    [BODY_BOTTOM_LEFT]  = "╚",  // > +---+---+---+
    [BODY_BOTTOM_FILL]  = "═",  //    ^^^ ^^^ ^^^
    [BODY_BOTTOM_SEP]   = "╧",  //       ^   ^
    [BODY_BOTTOM_RIGHT] = "╝",  //               ^
};

/*
 * Use these when outputting heavy-lined UTF-8 box graphics. Only the outside of
 * the header is heavy-lined, all other lines are light.
 */
static const char *style_heavy[] = {
    [HDR_TOP_LEFT]      = "┏",  // > +---+---+---+
    [HDR_TOP_FILL]      = "━",  //    ^^^ ^^^ ^^^
    [HDR_TOP_SEP]       = "┯",  //       ^   ^
    [HDR_TOP_RIGHT]     = "┓",  //               ^
    [HDR_TEXT_LEFT]     = "┃",  // > | A | B | C |
    [HDR_TEXT_SEP]      = "│",  //       ^   ^
    [HDR_TEXT_RIGHT]    = "┃",  //               ^
    [HDR_BOTTOM_LEFT]   = "┗",  // > +---+---+---+
    [HDR_BOTTOM_FILL]   = "━",  //    ^^^ ^^^ ^^^
    [HDR_BOTTOM_SEP]    = "┷",  //       ^   ^
    [HDR_BOTTOM_RIGHT]  = "┛",  //               ^
    [TRANS_LEFT]        = "┡",  // > +---+---+---+
    [TRANS_FILL]        = "━",  //    ^^^ ^^^ ^^^
    [TRANS_SEP]         = "┿",  //       ^   ^
    [TRANS_RIGHT]       = "┩",  //               ^
    [BODY_TOP_LEFT]     = "┌",  // > +---+---+---+
    [BODY_TOP_FILL]     = "─",  //    ^^^ ^^^ ^^^
    [BODY_TOP_SEP]      = "┬",  //       ^   ^
    [BODY_TOP_RIGHT]    = "┐",  //               ^
    [BODY_TEXT_LEFT]    = "│",  // > | X | Y | Z |
    [BODY_TEXT_SEP]     = "│",  //       ^   ^
    [BODY_TEXT_RIGHT]   = "│",  //               ^
    [BODY_BOTTOM_LEFT]  = "└",  // > +---+---+---+
    [BODY_BOTTOM_FILL]  = "─",  //    ^^^ ^^^ ^^^
    [BODY_BOTTOM_SEP]   = "┴",  //       ^   ^
    [BODY_BOTTOM_RIGHT] = "┘",  //               ^
};

/*
 * Allow for <rows> rows and <cols> columns in table <tbl>. Expands all the
 * necessary data structures to hold these numbers of rows and columns. Handles
 * expansion only(!), because we never need to shrink a table.
 */
static void tbl_allow(Table *tbl, int rows, int cols)
{
    int old_rows = tbl->rows;
    int old_cols = tbl->cols;

    int new_rows = MAX(rows, tbl->rows);
    int new_cols = MAX(cols, tbl->cols);

    if (new_cols > old_cols) {
        tbl->title = realloc(tbl->title, new_cols * sizeof(char *));

        for (int col = old_cols; col < new_cols; col++) {
            tbl->title[col] = NULL;
        }

        tbl->width = realloc(tbl->width, new_cols * sizeof(int));

        for (int col = old_cols; col < new_cols; col++) {
            tbl->width[col] = 0;
        }
    }

    if (new_rows > old_rows || new_cols > old_cols) {
        char **new_cell = calloc(new_rows * new_cols, sizeof(char *));

        for (int row = 0; row < old_rows; row++) {
            for (int col = 0; col < old_cols; col++) {
                int new_index = col + new_cols * row;
                int old_index = col + old_cols * row;

                new_cell[new_index] = tbl->cell[old_index];
            }
        }

        free(tbl->cell);

        tbl->cell = new_cell;
    }

    tbl->rows = new_rows;
    tbl->cols = new_cols;
}

/*
 * Determine whether table <tbl> needs a header. A header is only necessary when
 * at least one of the columns has a defined title.
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
 * for the right edge, and <column_sep> will be used as the separator between
 * columns. The number of columns is <cols>. For each column, the width that it
 * should be given is in <width>, and its text content is in <text>.
 */
static void tbl_format_text(Buffer *line, bool bold,
        const char *left_edge, const char *right_edge, const char *column_sep,
        int cols, int *width, char *const *text)
{
    bufRewind(line);

    bufAddS(line, left_edge);

    for (int col = 0; col < cols; col++) {
        if (col > 0) bufAddS(line, column_sep);

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
 * edge is in <left_edge>, the right edge is in <right_edge>, <column_sep> is
 * used to separate columns, and everywhere else (where the text fields are in
 * other lines) is filled using <center_fill>. The number of columns is <cols>,
 * and the width that each column should be given is in <width>.
 */
static void tbl_format_sep(Buffer *line,
        const char *left_edge, const char *right_edge, const char *center_fill,
        const char *column_sep, int cols, int *width)
{
    bufRewind(line);

    bufAddS(line, left_edge);

    for (int col = 0; col < cols; col++) {
        if (col > 0) bufAddS(line, column_sep);

        for (int c = 0; c < width[col] + 2; c++) {
            bufAddS(line, center_fill);
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
 * If <bold_headers> is true, the column titles in the header will be bolded
 * using ANSI escape sequences. <style> specifies which table style to use.
 *
 * Returns a pointer to a formatted output string, which is overwritten on each
 * call.
 */
const char *tblGetLine(Table *tbl, bool bold_headers, TableStyle style)
{
    const char **mark;

    switch (style) {
    case TBL_STYLE_ASCII:
        mark = style_ascii;
        break;
    case TBL_STYLE_BOX:
        mark = style_box;
        break;
    case TBL_STYLE_ROUND:
        mark = style_round;
        break;
    case TBL_STYLE_DOUBLE:
        mark = style_double;
        break;
    case TBL_STYLE_HEAVY:
        mark = style_heavy;
        break;
    }

    int row;
    bool has_header = tbl_has_header(tbl);
    bool has_body   = tbl->rows > 0;

    if (tbl->output_state == INITIAL) {
        if (has_header) {
            tbl->output_state = HEADER_TOP;
        }
        else {
            tbl->output_state = BODY_TOP;
        }
    }

    switch (tbl->output_state) {
    case HEADER_TOP:
        tbl_format_sep(&tbl->output_buf,
                mark[HDR_TOP_LEFT], mark[HDR_TOP_RIGHT],
                mark[HDR_TOP_FILL], mark[HDR_TOP_SEP],
                tbl->cols, tbl->width);
        tbl->output_state = HEADER_TEXT;
        break;
    case HEADER_TEXT:
        tbl_format_text(&tbl->output_buf, bold_headers,
                mark[HDR_TEXT_LEFT], mark[HDR_TEXT_RIGHT], mark[HDR_TEXT_SEP],
                tbl->cols, tbl->width, tbl->title);
        if (has_body) {
            tbl->output_state = TRANSITION;
        }
        else {
            tbl->output_state = HEADER_BOTTOM;
        }
        break;
    case HEADER_BOTTOM:
        tbl_format_sep(&tbl->output_buf,
                mark[HDR_BOTTOM_LEFT], mark[HDR_BOTTOM_RIGHT],
                mark[HDR_BOTTOM_FILL], mark[HDR_BOTTOM_SEP],
                tbl->cols, tbl->width);
        tbl->output_state = FINAL;
        break;
    case TRANSITION:
        tbl_format_sep(&tbl->output_buf,
                mark[TRANS_LEFT], mark[TRANS_RIGHT],
                mark[TRANS_FILL], mark[TRANS_SEP],
                tbl->cols, tbl->width);
        tbl->output_state = BODY_TEXT;
        break;
    case BODY_TOP:
        tbl_format_sep(&tbl->output_buf,
                mark[BODY_TOP_LEFT], mark[BODY_TOP_RIGHT],
                mark[BODY_TOP_FILL], mark[BODY_TOP_SEP],
                tbl->cols, tbl->width);

        if (has_body) {
            tbl->output_state = BODY_TEXT;
        }
        else {
            tbl->output_state = BODY_BOTTOM;
        }

        break;
    case BODY_TEXT:
        if (has_header) {
            row = tbl->output_count - 3;
        }
        else {
            row = tbl->output_count - 1;
        }

        tbl_format_text(&tbl->output_buf, false,
                mark[BODY_TEXT_LEFT], mark[BODY_TEXT_RIGHT],
                mark[BODY_TEXT_SEP],
                tbl->cols, tbl->width, tbl->cell + tbl->cols * row);

        if (row + 1 >= tbl->rows) {
            tbl->output_state = BODY_BOTTOM;
        }
        break;
    case BODY_BOTTOM:
        tbl_format_sep(&tbl->output_buf,
                mark[BODY_BOTTOM_LEFT], mark[BODY_BOTTOM_RIGHT],
                mark[BODY_BOTTOM_FILL], mark[BODY_BOTTOM_SEP],
                tbl->cols, tbl->width);
        tbl->output_state = FINAL;
        break;
    case FINAL:
        return NULL;
    default:
        break;
    }

    tbl->output_count++;

    return bufGet(&tbl->output_buf);
}

/*
 * Rewind the output of table <tbl> back to the start. On the next call to
 * tblGetLine(), the first output line will (again) be returned.
 */
void tblRewind(Table *tbl)
{
    tbl->output_count = 0;
    tbl->output_state = INITIAL;
}

/*
 * Detroy table <tbl>. All internal data, including the most recently returned
 * output line will be destroyed.
 */
void tblDestroy(Table *tbl)
{
    free(tbl->width);

    bufClear(&tbl->output_buf);

    for (int col = 0; col < tbl->cols; col++) {
        free(tbl->title[col]);

        for (int row = 0; row < tbl->rows; row++) {
            free(tbl->cell[col + tbl->cols * row]);
        }
    }

    free(tbl->title);
    free(tbl->cell);

    free(tbl);
}
