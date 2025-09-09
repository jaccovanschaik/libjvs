#include "utils.h"
#include "tables.c"

static int errors = 0;

int main(void)
{
    Table *tbl = tblCreate();

    check_string("+", tblGetLine(tbl, 0));
    check_string("+", tblGetLine(tbl, 0));
    make_sure_that(tblGetLine(tbl, 0) == NULL);

    tblRewind(tbl);

    tblSetCell(tbl, 0, 0, "Hoi!");

    check_string("+------+", tblGetLine(tbl, 0));
    check_string("| Hoi! |", tblGetLine(tbl, 0));
    check_string("+------+", tblGetLine(tbl, 0));
    make_sure_that(tblGetLine(tbl, 0) == NULL);

    tblRewind(tbl);

    check_string("┌──────┐", tblGetLine(tbl, TBL_BOX_CHARS));
    check_string("│ Hoi! │", tblGetLine(tbl, TBL_BOX_CHARS));
    check_string("└──────┘", tblGetLine(tbl, TBL_BOX_CHARS));
    make_sure_that(tblGetLine(tbl, 0) == NULL);

    tblRewind(tbl);

    tblDestroy(tbl);

    tbl = tblCreate();

    tblSetHeader(tbl, 0, "Title");

    tblSetCell(tbl, 0, 0, "Hoi");

    check_string("+-------+", tblGetLine(tbl, 0));
    check_string("| Title |", tblGetLine(tbl, 0));
    check_string("+-------+", tblGetLine(tbl, 0));
    check_string("| Hoi   |", tblGetLine(tbl, 0));
    check_string("+-------+", tblGetLine(tbl, 0));

    tblRewind(tbl);

    check_string("┌───────┐", tblGetLine(tbl, TBL_BOX_CHARS));
    check_string("│ Title │", tblGetLine(tbl, TBL_BOX_CHARS));
    check_string("├───────┤", tblGetLine(tbl, TBL_BOX_CHARS));
    check_string("│ Hoi   │", tblGetLine(tbl, TBL_BOX_CHARS));
    check_string("└───────┘", tblGetLine(tbl, TBL_BOX_CHARS));

    tblRewind(tbl);

    check_string("+-------+", tblGetLine(tbl, TBL_BOLD_TITLES));
    check_string("| \033[1mTitle\033[0m |", tblGetLine(tbl, TBL_BOLD_TITLES));
    check_string("+-------+", tblGetLine(tbl, TBL_BOLD_TITLES));
    check_string("| Hoi   |", tblGetLine(tbl, TBL_BOLD_TITLES));
    check_string("+-------+", tblGetLine(tbl, TBL_BOLD_TITLES));

    tblRewind(tbl);

    check_string("┌───────┐", tblGetLine(tbl, TBL_BOLD_TITLES|TBL_BOX_CHARS));
    check_string("│ \033[1mTitle\033[0m │", tblGetLine(tbl, TBL_BOLD_TITLES|TBL_BOX_CHARS));
    check_string("├───────┤", tblGetLine(tbl, TBL_BOLD_TITLES|TBL_BOX_CHARS));
    check_string("│ Hoi   │", tblGetLine(tbl, TBL_BOLD_TITLES|TBL_BOX_CHARS));
    check_string("└───────┘", tblGetLine(tbl, TBL_BOLD_TITLES|TBL_BOX_CHARS));

    tblRewind(tbl);

    return errors;
}
