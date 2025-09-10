#include "utils.h"
#include "tables.c"

static int errors = 0;

int main(void)
{
    Table *tbl = tblCreate();

    check_string("++", tblGetLine(tbl, false, TBL_FMT_ASCII));
    check_string("++", tblGetLine(tbl, false, TBL_FMT_ASCII));
    make_sure_that(tblGetLine(tbl, false, TBL_FMT_ASCII) == NULL);

    tblRewind(tbl);

    check_string("┌┐", tblGetLine(tbl, false, TBL_FMT_UTF8_SQUARE));
    check_string("└┘", tblGetLine(tbl, false, TBL_FMT_UTF8_SQUARE));
    make_sure_that(tblGetLine(tbl, false, TBL_FMT_UTF8_SQUARE) == NULL);

    tblRewind(tbl);

    check_string("╭╮", tblGetLine(tbl, false, TBL_FMT_UTF8_ROUND));
    check_string("╰╯", tblGetLine(tbl, false, TBL_FMT_UTF8_ROUND));
    make_sure_that(tblGetLine(tbl, false, TBL_FMT_UTF8_ROUND) == NULL);

    tblRewind(tbl);

    tblSetCell(tbl, 0, 0, "Hoi!");

    check_string("+------+", tblGetLine(tbl, false, TBL_FMT_ASCII));
    check_string("| Hoi! |", tblGetLine(tbl, false, TBL_FMT_ASCII));
    check_string("+------+", tblGetLine(tbl, false, TBL_FMT_ASCII));
    make_sure_that(tblGetLine(tbl, false, TBL_FMT_ASCII) == NULL);

    tblRewind(tbl);

    check_string("┌──────┐", tblGetLine(tbl, false, TBL_FMT_UTF8_SQUARE));
    check_string("│ Hoi! │", tblGetLine(tbl, false, TBL_FMT_UTF8_SQUARE));
    check_string("└──────┘", tblGetLine(tbl, false, TBL_FMT_UTF8_SQUARE));
    make_sure_that(tblGetLine(tbl, false, TBL_FMT_ASCII) == NULL);

    tblRewind(tbl);

    check_string("╭──────╮", tblGetLine(tbl, false, TBL_FMT_UTF8_ROUND));
    check_string("│ Hoi! │", tblGetLine(tbl, false, TBL_FMT_UTF8_ROUND));
    check_string("╰──────╯", tblGetLine(tbl, false, TBL_FMT_UTF8_ROUND));
    make_sure_that(tblGetLine(tbl, false, TBL_FMT_ASCII) == NULL);

    tblRewind(tbl);

    tblDestroy(tbl);

    tbl = tblCreate();

    tblSetHeader(tbl, 0, "Title");

    tblSetCell(tbl, 0, 0, "Hoi");

    check_string("+-------+", tblGetLine(tbl, false, TBL_FMT_ASCII));
    check_string("| Title |", tblGetLine(tbl, false, TBL_FMT_ASCII));
    check_string("+-------+", tblGetLine(tbl, false, TBL_FMT_ASCII));
    check_string("| Hoi   |", tblGetLine(tbl, false, TBL_FMT_ASCII));
    check_string("+-------+", tblGetLine(tbl, false, TBL_FMT_ASCII));

    tblRewind(tbl);

    check_string("┌───────┐", tblGetLine(tbl, false, TBL_FMT_UTF8_SQUARE));
    check_string("│ Title │", tblGetLine(tbl, false, TBL_FMT_UTF8_SQUARE));
    check_string("├───────┤", tblGetLine(tbl, false, TBL_FMT_UTF8_SQUARE));
    check_string("│ Hoi   │", tblGetLine(tbl, false, TBL_FMT_UTF8_SQUARE));
    check_string("└───────┘", tblGetLine(tbl, false, TBL_FMT_UTF8_SQUARE));

    tblRewind(tbl);

    check_string("╭───────╮", tblGetLine(tbl, false, TBL_FMT_UTF8_ROUND));
    check_string("│ Title │", tblGetLine(tbl, false, TBL_FMT_UTF8_ROUND));
    check_string("├───────┤", tblGetLine(tbl, false, TBL_FMT_UTF8_ROUND));
    check_string("│ Hoi   │", tblGetLine(tbl, false, TBL_FMT_UTF8_ROUND));
    check_string("╰───────╯", tblGetLine(tbl, false, TBL_FMT_UTF8_ROUND));

    tblRewind(tbl);

    check_string("+-------+", tblGetLine(tbl, true, TBL_FMT_ASCII));
    check_string("| \033[1mTitle\033[0m |", tblGetLine(tbl, true, TBL_FMT_ASCII));
    check_string("+-------+", tblGetLine(tbl, true, TBL_FMT_ASCII));
    check_string("| Hoi   |", tblGetLine(tbl, true, TBL_FMT_ASCII));
    check_string("+-------+", tblGetLine(tbl, true, TBL_FMT_ASCII));

    tblRewind(tbl);

    check_string("┌───────┐", tblGetLine(tbl, true, TBL_FMT_UTF8_SQUARE));
    check_string("│ \033[1mTitle\033[0m │", tblGetLine(tbl, true, TBL_FMT_UTF8_SQUARE));
    check_string("├───────┤", tblGetLine(tbl, true, TBL_FMT_UTF8_SQUARE));
    check_string("│ Hoi   │", tblGetLine(tbl, true, TBL_FMT_UTF8_SQUARE));
    check_string("└───────┘", tblGetLine(tbl, true, TBL_FMT_UTF8_SQUARE));

    tblRewind(tbl);

    return errors;
}
