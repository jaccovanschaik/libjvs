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

struct Table {
    int    rows, cols;
    int   *width;
    int    next_line;
    Buffer line;
    char **title;
    char **cell;
};

enum {
    TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT,
    TOP_CROSS, BOTTOM_CROSS, LEFT_CROSS, RIGHT_CROSS, CENTER_CROSS,
    TOP_EDGE, BOTTOM_EDGE, LEFT_EDGE, RIGHT_EDGE, CENTER_HOR, CENTER_VER,
    EDGE_COUNT
};

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

static bool tbl_has_header(Table *tbl)
{
    for (int col = 0; col < tbl->cols; col++) {
        if (tbl->title[col] != NULL) return true;
    }

    return false;
}

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

Table *tblCreate(void)
{
    Table *tbl = calloc(1, sizeof(Table));

    return tbl;
}

int tblVaSetColumn(Table *tbl, int col, const char *fmt, va_list ap)
{
    tbl_allow(tbl, tbl->rows, col + 1);

    free(tbl->title[col]);

    int r = vasprintf(&tbl->title[col], fmt, ap);

    r = utf8_strlen(tbl->title[col]);

    if (r > tbl->width[col]) tbl->width[col] = r;

    return r;
}

int tblSetColumn(Table *tbl, int col, const char *fmt, ...)
{
    va_list ap;
    int r;

    va_start(ap, fmt);
    r = tblVaSetColumn(tbl, col, fmt, ap);
    va_end(ap);

    return r;
}

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

int tblSetCell(Table *tbl, int row, int col, const char *fmt, ...)
{
    va_list ap;
    int r;

    va_start(ap, fmt);
    r = tblVaSetCell(tbl, row, col, fmt, ap);
    va_end(ap);

    return r;
}

void tblRewind(Table *tbl)
{
    tbl->next_line = 0;
}

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
