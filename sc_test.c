#include "sc.c"
#include "utils.h"

static int errors = 0;

int main(void)
{
    SC *sc = scCreate(4);

    make_sure_that(sc != NULL);
    make_sure_that(sc->count == 4);
    make_sure_that(sc->cell != NULL);

    make_sure_that(sc->cell[0].str == NULL);
    make_sure_that(sc->cell[0].size == 0);

    make_sure_that(sc->cell[1].str == NULL);
    make_sure_that(sc->cell[1].size == 0);

    make_sure_that(sc->cell[2].str == NULL);
    make_sure_that(sc->cell[2].size == 0);

    make_sure_that(sc->cell[3].str == NULL);
    make_sure_that(sc->cell[3].size == 0);

    const char *p[5];

    p[0] = scAdd(sc, "Nul");

    make_sure_that(strcmp(p[0], "Nul") == 0);

    p[1] = scAdd(sc, "%s", "Een");

    make_sure_that(strcmp(p[1], "Een") == 0);
    make_sure_that(p[1] != p[0]);

    p[2] = scAdd(sc, "%d", 2);

    make_sure_that(strcmp(p[2], "2") == 0);
    make_sure_that(p[2] != p[0]);
    make_sure_that(p[2] != p[1]);

    p[3] = scAdd(sc, "<%s>", "Drie");

    make_sure_that(strcmp(p[3], "<Drie>") == 0);
    make_sure_that(p[3] != p[0]);
    make_sure_that(p[3] != p[1]);
    make_sure_that(p[3] != p[2]);

    p[4] = scAdd(sc, "%02d", 4);

    make_sure_that(strcmp(p[4], "04") == 0);
    make_sure_that(p[4] == p[0]);
    make_sure_that(p[4] != p[1]);
    make_sure_that(p[4] != p[2]);
    make_sure_that(p[4] != p[3]);
}
