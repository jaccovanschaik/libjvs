#include "utils.h"
#include "tables.c"

static int errors = 0;

static void _check(const char *file, int line,
        Table *tbl, const char *exp, bool bold, TableFormat format)
{
    const char *act = tblGetLine(tbl, bold, format);

    if (strcmp(act, exp) != 0) {
        fprintf(stderr, "%s:%d: String does not match expectation.\n",
                file, line);
        fprintf(stderr, "Expected: \"%s\"\n", exp);
        fprintf(stderr, "Actual:   \"%s\"\n", act);

        errors++;
    }
}

#define check(tbl, exp, bold, fmt) \
    _check(__FILE__, __LINE__, tbl, exp, bold, fmt)

int main(void)
{
    Table *tbl = tblCreate();

    check(tbl, "++", false, TBL_FMT_ASCII);
    check(tbl, "++", false, TBL_FMT_ASCII);
    make_sure_that(tblGetLine(tbl, false, TBL_FMT_ASCII) == NULL);

    tblRewind(tbl);

    check(tbl, "┌┐", false, TBL_FMT_BOX);
    check(tbl, "└┘", false, TBL_FMT_BOX);
    make_sure_that(tblGetLine(tbl, false, TBL_FMT_BOX) == NULL);

    tblRewind(tbl);

    check(tbl, "╭╮", false, TBL_FMT_ROUND);
    check(tbl, "╰╯", false, TBL_FMT_ROUND);
    make_sure_that(tblGetLine(tbl, false, TBL_FMT_ROUND) == NULL);

    tblRewind(tbl);

    tblSetCell(tbl, 0, 0, "Hoi");
    tblSetCell(tbl, 0, 1, "Hällø");

    check(tbl, "+-----+-------+", false, TBL_FMT_ASCII);
    check(tbl, "| Hoi | Hällø |", false, TBL_FMT_ASCII);
    check(tbl, "+-----+-------+", false, TBL_FMT_ASCII);
    make_sure_that(tblGetLine(tbl, false, TBL_FMT_ASCII) == NULL);

    tblRewind(tbl);

    check(tbl, "┌─────┬───────┐", false, TBL_FMT_BOX);
    check(tbl, "│ Hoi │ Hällø │", false, TBL_FMT_BOX);
    check(tbl, "└─────┴───────┘", false, TBL_FMT_BOX);
    make_sure_that(tblGetLine(tbl, false, TBL_FMT_ASCII) == NULL);

    tblRewind(tbl);

    check(tbl, "╭─────┬───────╮", false, TBL_FMT_ROUND);
    check(tbl, "│ Hoi │ Hällø │", false, TBL_FMT_ROUND);
    check(tbl, "╰─────┴───────╯", false, TBL_FMT_ROUND);
    make_sure_that(tblGetLine(tbl, false, TBL_FMT_ASCII) == NULL);

    tblDestroy(tbl);

    tbl = tblCreate();

    tblSetHeader(tbl, 0, "First");
    tblSetHeader(tbl, 1, "2nd");

    tblSetCell(tbl, 0, 0, "Hoi");
    tblSetCell(tbl, 0, 1, "Hällø");

    tblSetCell(tbl, 1, 0, "Bye");
    tblSetCell(tbl, 1, 1, "Doei");

    check(tbl, "+-------+-------+", false, TBL_FMT_ASCII);
    check(tbl, "| First | 2nd   |", false, TBL_FMT_ASCII);
    check(tbl, "+-------+-------+", false, TBL_FMT_ASCII);
    check(tbl, "| Hoi   | Hällø |", false, TBL_FMT_ASCII);
    check(tbl, "| Bye   | Doei  |", false, TBL_FMT_ASCII);
    check(tbl, "+-------+-------+", false, TBL_FMT_ASCII);

    tblRewind(tbl);

    check(tbl, "+-------+-------+", true, TBL_FMT_ASCII);
    check(tbl, "| \033[1mFirst\033[0m | \033[1m2nd  \033[0m |", true,
            TBL_FMT_ASCII);
    check(tbl, "+-------+-------+", true, TBL_FMT_ASCII);
    check(tbl, "| Hoi   | Hällø |", true, TBL_FMT_ASCII);
    check(tbl, "| Bye   | Doei  |", true, TBL_FMT_ASCII);
    check(tbl, "+-------+-------+", true, TBL_FMT_ASCII);

    tblRewind(tbl);

    check(tbl, "┌───────┬───────┐", false, TBL_FMT_BOX);
    check(tbl, "│ First │ 2nd   │", false, TBL_FMT_BOX);
    check(tbl, "├───────┼───────┤", false, TBL_FMT_BOX);
    check(tbl, "│ Hoi   │ Hällø │", false, TBL_FMT_BOX);
    check(tbl, "│ Bye   │ Doei  │", false, TBL_FMT_BOX);
    check(tbl, "└───────┴───────┘", false, TBL_FMT_BOX);

    tblRewind(tbl);

    check(tbl, "╭───────┬───────╮", false, TBL_FMT_ROUND);
    check(tbl, "│ First │ 2nd   │", false, TBL_FMT_ROUND);
    check(tbl, "├───────┼───────┤", false, TBL_FMT_ROUND);
    check(tbl, "│ Hoi   │ Hällø │", false, TBL_FMT_ROUND);
    check(tbl, "│ Bye   │ Doei  │", false, TBL_FMT_ROUND);
    check(tbl, "╰───────┴───────╯", false, TBL_FMT_ROUND);

    tblRewind(tbl);

    check(tbl, "╔═══════╤═══════╗", false, TBL_FMT_DOUBLE);
    check(tbl, "║ First │ 2nd   ║", false, TBL_FMT_DOUBLE);
    check(tbl, "╠═══════╪═══════╣", false, TBL_FMT_DOUBLE);
    check(tbl, "║ Hoi   │ Hällø ║", false, TBL_FMT_DOUBLE);
    check(tbl, "║ Bye   │ Doei  ║", false, TBL_FMT_DOUBLE);
    check(tbl, "╚═══════╧═══════╝", false, TBL_FMT_DOUBLE);

    tblRewind(tbl);

    check(tbl, "┏━━━━━━━┯━━━━━━━┓", false, TBL_FMT_HEAVY);
    check(tbl, "┃ First │ 2nd   ┃", false, TBL_FMT_HEAVY);
    check(tbl, "┡━━━━━━━┿━━━━━━━┩", false, TBL_FMT_HEAVY);
    check(tbl, "│ Hoi   │ Hällø │", false, TBL_FMT_HEAVY);
    check(tbl, "│ Bye   │ Doei  │", false, TBL_FMT_HEAVY);
    check(tbl, "└───────┴───────┘", false, TBL_FMT_HEAVY);

    tblDestroy(tbl);

    return errors;
}
