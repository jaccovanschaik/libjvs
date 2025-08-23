#include "vectors.c"
#include "vectors.h"

static int errors = 0;

int main(void)
{
    Matrix2x2 m2x2 = m2x2Make(1, 0,
                              0, 1);

    mPrint(stderr, m2x2, NULL);

    return errors;
}
