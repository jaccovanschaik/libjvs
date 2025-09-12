#include "utils.h"
#include "tables.c"

static int errors = 0;

static void _check(const char *file, int line,
        Table *tbl, const char *exp, int width, bool bold, TableStyle format)
{
    const char *act = tblGetLine(tbl, width, bold, format);

    if (strcmp(act, exp) != 0) {
        fprintf(stderr, "%s:%d: String does not match expectation.\n",
                file, line);
        fprintf(stderr, "Expected: \"%s\"\n", exp);
        fprintf(stderr, "Actual:   \"%s\"\n", act);

        errors++;
    }
}

#define check(tbl, exp, width, bold, fmt) \
    _check(__FILE__, __LINE__, tbl, exp, width, bold, fmt)

int main(void)
{
    Table *tbl = tblCreate();

    // Empty table in ASCII.

    check(tbl, "++", 0, false, TBL_STYLE_ASCII);
    check(tbl, "++", 0, false, TBL_STYLE_ASCII);
    make_sure_that(tblGetLine(tbl, 0, false, TBL_STYLE_ASCII) == NULL);

    tblRewind(tbl);

    // Empty table, single lines, square corners.

    check(tbl, "┌┐", 0, false, TBL_STYLE_BOX);
    check(tbl, "└┘", 0, false, TBL_STYLE_BOX);
    make_sure_that(tblGetLine(tbl, 0, false, TBL_STYLE_BOX) == NULL);

    tblRewind(tbl);

    // Empty table, single lines, round corners.

    check(tbl, "╭╮", 0, false, TBL_STYLE_ROUND);
    check(tbl, "╰╯", 0, false, TBL_STYLE_ROUND);
    make_sure_that(tblGetLine(tbl, 0, false, TBL_STYLE_ROUND) == NULL);

    tblRewind(tbl);

    // Table without header, ASCII.

    tblSetCell(tbl, 0, 0, "Hoi");
    tblSetCell(tbl, 0, 1, "Hällø");

    check(tbl, "+-----+-------+", 0, false, TBL_STYLE_ASCII);
    check(tbl, "| Hoi | Hällø |", 0, false, TBL_STYLE_ASCII);
    check(tbl, "+-----+-------+", 0, false, TBL_STYLE_ASCII);
    make_sure_that(tblGetLine(tbl, 0, false, TBL_STYLE_ASCII) == NULL);

    tblRewind(tbl);

    // Table without header, single line, square corners.

    check(tbl, "┌─────┬───────┐", 0, false, TBL_STYLE_BOX);
    check(tbl, "│ Hoi │ Hällø │", 0, false, TBL_STYLE_BOX);
    check(tbl, "└─────┴───────┘", 0, false, TBL_STYLE_BOX);
    make_sure_that(tblGetLine(tbl, 0, false, TBL_STYLE_ASCII) == NULL);

    tblRewind(tbl);

    // Table without header, single line, round corners.

    check(tbl, "╭─────┬───────╮", 0, false, TBL_STYLE_ROUND);
    check(tbl, "│ Hoi │ Hällø │", 0, false, TBL_STYLE_ROUND);
    check(tbl, "╰─────┴───────╯", 0, false, TBL_STYLE_ROUND);
    make_sure_that(tblGetLine(tbl, 0, false, TBL_STYLE_ASCII) == NULL);

    tblDestroy(tbl);

    tbl = tblCreate();

    // Full table with a header, two rows and two columns, ASCII.

    tblSetHeader(tbl, 0, "First");
    tblSetHeader(tbl, 1, "2nd");

    tblSetCell(tbl, 0, 0, "Hoi");
    tblSetCell(tbl, 0, 1, "Hällø");

    tblSetCell(tbl, 1, 0, "Bye");
    tblSetCell(tbl, 1, 1, "Doei");

    check(tbl, "+-------+-------+", 0, false, TBL_STYLE_ASCII);
    check(tbl, "| First | 2nd   |", 0, false, TBL_STYLE_ASCII);
    check(tbl, "+-------+-------+", 0, false, TBL_STYLE_ASCII);
    check(tbl, "| Hoi   | Hällø |", 0, false, TBL_STYLE_ASCII);
    check(tbl, "| Bye   | Doei  |", 0, false, TBL_STYLE_ASCII);
    check(tbl, "+-------+-------+", 0, false, TBL_STYLE_ASCII);

    tblRewind(tbl);

    // Same, but with bold column titles.

    check(tbl, "+-------+-------+", 0, true, TBL_STYLE_ASCII);
    check(tbl, "| \033[1mFirst\033[0m | \033[1m2nd  \033[0m |",
            0, true, TBL_STYLE_ASCII);
    check(tbl, "+-------+-------+", 0, true, TBL_STYLE_ASCII);
    check(tbl, "| Hoi   | Hällø |", 0, true, TBL_STYLE_ASCII);
    check(tbl, "| Bye   | Doei  |", 0, true, TBL_STYLE_ASCII);
    check(tbl, "+-------+-------+", 0, true, TBL_STYLE_ASCII);

    tblRewind(tbl);

    // Same with single lines and square corners.

    check(tbl, "┌───────┬───────┐", 0, false, TBL_STYLE_BOX);
    check(tbl, "│ First │ 2nd   │", 0, false, TBL_STYLE_BOX);
    check(tbl, "├───────┼───────┤", 0, false, TBL_STYLE_BOX);
    check(tbl, "│ Hoi   │ Hällø │", 0, false, TBL_STYLE_BOX);
    check(tbl, "│ Bye   │ Doei  │", 0, false, TBL_STYLE_BOX);
    check(tbl, "└───────┴───────┘", 0, false, TBL_STYLE_BOX);

    tblRewind(tbl);

    // Same with single lines and rounded corners.

    check(tbl, "╭───────┬───────╮", 0, false, TBL_STYLE_ROUND);
    check(tbl, "│ First │ 2nd   │", 0, false, TBL_STYLE_ROUND);
    check(tbl, "├───────┼───────┤", 0, false, TBL_STYLE_ROUND);
    check(tbl, "│ Hoi   │ Hällø │", 0, false, TBL_STYLE_ROUND);
    check(tbl, "│ Bye   │ Doei  │", 0, false, TBL_STYLE_ROUND);
    check(tbl, "╰───────┴───────╯", 0, false, TBL_STYLE_ROUND);

    tblRewind(tbl);

    // Same with double lines.

    check(tbl, "╔═══════╤═══════╗", 0, false, TBL_STYLE_DOUBLE);
    check(tbl, "║ First │ 2nd   ║", 0, false, TBL_STYLE_DOUBLE);
    check(tbl, "╠═══════╪═══════╣", 0, false, TBL_STYLE_DOUBLE);
    check(tbl, "║ Hoi   │ Hällø ║", 0, false, TBL_STYLE_DOUBLE);
    check(tbl, "║ Bye   │ Doei  ║", 0, false, TBL_STYLE_DOUBLE);
    check(tbl, "╚═══════╧═══════╝", 0, false, TBL_STYLE_DOUBLE);

    tblRewind(tbl);

    // Same with heavy header and light body.

    check(tbl, "┏━━━━━━━┯━━━━━━━┓", 0, false, TBL_STYLE_HEAVY);
    check(tbl, "┃ First │ 2nd   ┃", 0, false, TBL_STYLE_HEAVY);
    check(tbl, "┡━━━━━━━┿━━━━━━━┩", 0, false, TBL_STYLE_HEAVY);
    check(tbl, "│ Hoi   │ Hällø │", 0, false, TBL_STYLE_HEAVY);
    check(tbl, "│ Bye   │ Doei  │", 0, false, TBL_STYLE_HEAVY);
    check(tbl, "└───────┴───────┘", 0, false, TBL_STYLE_HEAVY);

    tblRewind(tbl);

    check(tbl, "┏━━━━━━━┯━━━━┓", 14, false, TBL_STYLE_HEAVY);
    check(tbl, "┃ First │ 2n ┃", 14, false, TBL_STYLE_HEAVY);
    check(tbl, "┡━━━━━━━┿━━━━┩", 14, false, TBL_STYLE_HEAVY);
    check(tbl, "│ Hoi   │ Hä │", 14, false, TBL_STYLE_HEAVY);
    check(tbl, "│ Bye   │ Do │", 14, false, TBL_STYLE_HEAVY);
    check(tbl, "└───────┴────┘", 14, false, TBL_STYLE_HEAVY);

    tblRewind(tbl);

    check(tbl, "┏━━━━━━━┯━━━┓", 13, false, TBL_STYLE_HEAVY);
    check(tbl, "┃ First │ 2 ┃", 13, false, TBL_STYLE_HEAVY);
    check(tbl, "┡━━━━━━━┿━━━┩", 13, false, TBL_STYLE_HEAVY);
    check(tbl, "│ Hoi   │ H │", 13, false, TBL_STYLE_HEAVY);
    check(tbl, "│ Bye   │ D │", 13, false, TBL_STYLE_HEAVY);
    check(tbl, "└───────┴───┘", 13, false, TBL_STYLE_HEAVY);

    tblRewind(tbl);

    check(tbl, "┏━━━━━━━┯━━┓", 12, false, TBL_STYLE_HEAVY);
    check(tbl, "┃ First │  ┃", 12, false, TBL_STYLE_HEAVY);
    check(tbl, "┡━━━━━━━┿━━┩", 12, false, TBL_STYLE_HEAVY);
    check(tbl, "│ Hoi   │  │", 12, false, TBL_STYLE_HEAVY);
    check(tbl, "│ Bye   │  │", 12, false, TBL_STYLE_HEAVY);
    check(tbl, "└───────┴──┘", 12, false, TBL_STYLE_HEAVY);

    tblRewind(tbl);

    check(tbl, "┏━━━━┯━━┓", 9, false, TBL_STYLE_HEAVY);
    check(tbl, "┃ Fi │  ┃", 9, false, TBL_STYLE_HEAVY);
    check(tbl, "┡━━━━┿━━┩", 9, false, TBL_STYLE_HEAVY);
    check(tbl, "│ Ho │  │", 9, false, TBL_STYLE_HEAVY);
    check(tbl, "│ By │  │", 9, false, TBL_STYLE_HEAVY);
    check(tbl, "└────┴──┘", 9, false, TBL_STYLE_HEAVY);

    tblRewind(tbl);

    check(tbl, "┏━━┯━━┓", 1, false, TBL_STYLE_HEAVY);
    check(tbl, "┃  │  ┃", 1, false, TBL_STYLE_HEAVY);
    check(tbl, "┡━━┿━━┩", 1, false, TBL_STYLE_HEAVY);
    check(tbl, "│  │  │", 1, false, TBL_STYLE_HEAVY);
    check(tbl, "│  │  │", 1, false, TBL_STYLE_HEAVY);
    check(tbl, "└──┴──┘", 1, false, TBL_STYLE_HEAVY);

    tblDestroy(tbl);

    return errors;
}
