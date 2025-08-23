#include "bitmask.c"

static int errors = 0;

int main(void)
{
    Bitmask mask = { 0 };
    Bitmask *mask2;

    bmSetBit(&mask, 0);

    make_sure_that(mask.n_bytes == 1);
    make_sure_that(mask.bits[0] == 0x01);

    make_sure_that(bmGetBit(&mask, 0) == 1);
    make_sure_that(bmGetBit(&mask, 1) == 0);
    make_sure_that(bmGetBit(&mask, 2) == 0);
    make_sure_that(bmGetBit(&mask, 3) == 0);
    make_sure_that(bmGetBit(&mask, 4) == 0);
    make_sure_that(bmGetBit(&mask, 5) == 0);
    make_sure_that(bmGetBit(&mask, 6) == 0);
    make_sure_that(bmGetBit(&mask, 7) == 0);
    make_sure_that(bmGetBit(&mask, 1000) == 0);

    bmSetBit(&mask, 9);

    make_sure_that(mask.n_bytes == 2);
    make_sure_that(mask.bits[0] == 0x01);
    make_sure_that(mask.bits[1] == 0x02);

    make_sure_that(bmGetBit(&mask, 0) == 1);
    make_sure_that(bmGetBit(&mask, 1) == 0);
    make_sure_that(bmGetBit(&mask, 2) == 0);
    make_sure_that(bmGetBit(&mask, 3) == 0);
    make_sure_that(bmGetBit(&mask, 4) == 0);
    make_sure_that(bmGetBit(&mask, 5) == 0);
    make_sure_that(bmGetBit(&mask, 6) == 0);
    make_sure_that(bmGetBit(&mask, 7) == 0);
    make_sure_that(bmGetBit(&mask, 8) == 0);
    make_sure_that(bmGetBit(&mask, 9) == 1);
    make_sure_that(bmGetBit(&mask, 1000) == 0);

    bmClrBit(&mask, 0);

    make_sure_that(mask.n_bytes == 2);
    make_sure_that(mask.bits[0] == 0x00);
    make_sure_that(mask.bits[1] == 0x02);

    make_sure_that(bmGetBit(&mask, 0) == 0);
    make_sure_that(bmGetBit(&mask, 1) == 0);
    make_sure_that(bmGetBit(&mask, 2) == 0);
    make_sure_that(bmGetBit(&mask, 3) == 0);
    make_sure_that(bmGetBit(&mask, 4) == 0);
    make_sure_that(bmGetBit(&mask, 5) == 0);
    make_sure_that(bmGetBit(&mask, 6) == 0);
    make_sure_that(bmGetBit(&mask, 7) == 0);
    make_sure_that(bmGetBit(&mask, 8) == 0);
    make_sure_that(bmGetBit(&mask, 9) == 1);
    make_sure_that(bmGetBit(&mask, 1000) == 0);

    bmClrBit(&mask, 9);

    make_sure_that(mask.n_bytes == 2);
    make_sure_that(mask.bits[0] == 0x00);
    make_sure_that(mask.bits[1] == 0x00);

    make_sure_that(bmGetBit(&mask, 0) == 0);
    make_sure_that(bmGetBit(&mask, 1) == 0);
    make_sure_that(bmGetBit(&mask, 2) == 0);
    make_sure_that(bmGetBit(&mask, 3) == 0);
    make_sure_that(bmGetBit(&mask, 4) == 0);
    make_sure_that(bmGetBit(&mask, 5) == 0);
    make_sure_that(bmGetBit(&mask, 6) == 0);
    make_sure_that(bmGetBit(&mask, 7) == 0);
    make_sure_that(bmGetBit(&mask, 8) == 0);
    make_sure_that(bmGetBit(&mask, 9) == 0);
    make_sure_that(bmGetBit(&mask, 1000) == 0);

    bmSetBits(&mask, 0, 2, 4, 6, 8, 10, 12, 14, END);

    make_sure_that(mask.n_bytes == 2);
    make_sure_that(mask.bits[0] == 0x55);
    make_sure_that(mask.bits[1] == 0x55);

    bmClrBits(&mask, 0, 2, 4, 6, 8, 10, 12, 14, END);

    make_sure_that(mask.n_bytes == 2);
    make_sure_that(mask.bits[0] == 0x00);
    make_sure_that(mask.bits[1] == 0x00);

    mask2 = bmCreate();

    make_sure_that(mask2->n_bytes == 0);
    make_sure_that(mask2->bits == NULL);

    make_sure_that(bmCompare(&mask, mask2) == 0);

    bmSetBit(&mask, 0);

    make_sure_that(bmCompare(&mask, mask2) == 1);

    bmSetBit(mask2, 0);

    make_sure_that(bmCompare(&mask, mask2) == 0);

    bmSetBit(mask2, 1);

    make_sure_that(bmCompare(&mask, mask2) == -1);

    bmClear(&mask);
    bmDestroy(mask2);

    return errors;
}
