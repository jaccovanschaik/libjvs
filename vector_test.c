#include "vector.c"
#include "vector.h"

static int errors = 0;

int main(void)
{
    Matrix2x2 m2x2 = m2x2Make(-10.0001, 0,
                              0, 1);

    mPrint(stderr, m2x2, "Matrix2x2");

    Matrix3x2 m3x2 = m3x2Make(-251.111111111, 0,
                              1, 1,
                              0, 1);
    Vector2 in  = v2Make(1, -1);
    Vector3 out = mTransform(m3x2, in);

    vPrint(stderr, out, "Vector3 out");

    Vector3 v1 = v3Make(-1, 0,  1);
    Vector3 v2 = v3Make( 1, 0, -1);

    vPrint(stderr, vSum(v1, v2), "sum");

    return errors;
}
