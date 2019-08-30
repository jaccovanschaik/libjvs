/*
 * matrix2.c: Handles 2x2 matrices.
 *
 * matrix2.c is part of libjvs.
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-08-27
 * Version:   $Id: matrix2.c 350 2019-08-30 12:07:22Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <string.h>
#include <assert.h>

#include "matrix2.h"

/*
 * Return a matrix with all coefficients set to 0.
 */
Matrix2 m2New(void)
{
    Matrix2 m;

    memset(&m, 0, sizeof(m));

    return m;
}

/*
 * Return a matrix with its coefficients set as indicated. Note that the first
 * two parameters are the first *row* of the matrix and the next two parameters
 * are the second row. This allows for a more "natural" way of filling the
 * matrix.
 */
Matrix2 m2Make(double xx, double yx,
               double xy, double yy)
{
    Matrix2 m = {
        .c[0] = v2Make(xx, xy),
        .c[1] = v2Make(yx, yy),
    };

    return m;
}

/*
 * Return a matrix where the two columns are set as indicated.
 */
Matrix2 m2MakeV(Vector2 c0, Vector2 c1)
{
    Matrix2 m = { .c[0] = c0, .c[1] = c1 };

    return m;
}

/*
 * Return an identity matrix.
 */
Matrix2 m2Identity(void)
{
    return m2Make(1, 0,
                  0, 1);
}

/*
 * Return a transposed version of the given matrix.
 */
Matrix2 m2Transposed(Matrix2 m)
{
    return m2Make(
            m.c[0].r[0], m.c[0].r[1],
            m.c[1].r[0], m.c[1].r[1]);
}

/*
 * Transpose the matrix indicated by <m> in place.
 */
void m2Transpose(Matrix2 *m)
{
    *m = m2Transposed(*m);
}

/*
 * Return row <row> from matrix <m> as a 2d vector.
 */
Vector2 m2Row(Matrix2 m, int row)
{
    assert(row >= 0 && row < 2);

    return v2Make(m.c[0].r[row], m.c[1].r[row]);
}

/*
 * Return the determinant of <m>.
 */
double m2Det(Matrix2 m)
{
    return m.c[0].r[0] * m.c[1].r[1] - m.c[0].r[1] * m.c[1].r[0];
}

/*
 * Return a version of <m> where each coefficient is scaled by <factor>.
 */
Matrix2 m2Scaled(Matrix2 m, double factor)
{
    Matrix2 s;

    int row, col;

    for (row = 0; row < 2; row++) {
        for (col = 0; col < 2; col++) {
            s.c[col].r[row] = m.c[col].r[row] * factor;
        }
    }

    return s;
}

/*
 * Scale the matrix indicated by <m> by <factor>.
 */
void m2Scale(Matrix2 *m, double factor)
{
    *m = m2Scaled(*m, factor);
}

/*
 * Return the inverse of matrix <m>. Note that there is no way to report if the
 * determinant is 0, so it is left up to the caller to check for this and avoid
 * calling this function in that case. And if the caller has to calculate the
 * determinant themselves anyway, they might as well just pass it in so I don't
 * have to do it again.
 */
Matrix2 m2Inverse(Matrix2 m, double det)
{
    return m2Scaled(m2Make(m.c[1].r[1], -m.c[1].r[0],
                          -m.c[0].r[1],  m.c[0].r[0]), 1 / det);
}

/*
 * Invert matrix <m> in place. If succesful, 0 is returned. Otherwise (if the
 * determinant of the matrix is 0), 1 is returned and <m> is not changed.
 */
int m2Invert(Matrix2 *m)
{
    double det = m2Det(*m);

    if (det == 0) {
        return 1;
    }

    *m = m2Inverse(*m, det);

    return 0;
}

/*
 * Return the adjugate of matrix <m> (where alternate coefficients have their
 * sign inverted).
 */
Matrix2 m2Adjugate(Matrix2 m)
{
    Matrix2 A;

    int row, col, sign = 1;

    for (row = 0; row < 2; row++) {
        for (col = 0; col < 2; col++) {
            A.c[col].r[row] = sign * m.c[col].r[row];
            sign = -sign;
        }
    }

    return A;
}

/*
 * Return the result of multiplying matrices <m1> and <m2>.
 */
Matrix2 m2Product(Matrix2 m1, Matrix2 m2)
{
    Matrix2 P;

    int row, col;

    for (row = 0; row < 2; row++) {
        Vector2 r = m2Row(m2, row);

        for (col = 0; col < 2; col++) {
            P.c[col].r[row] = v2Dot(m1.c[col], r);
        }
    }

    return P;
}

/*
 * Multiply the matrix indicated by <m1> with <m2> and put the result back into
 * <m1>.
 */
void m2Multiply(Matrix2 *m1, Matrix2 m2)
{
    *m1 = m2Product(*m1, m2);
}

/*
 * Return the result of applying (multiplying) matrix <m> to vector <v>.
 */
Vector2 m2Applied(Matrix2 m, Vector2 v)
{
    Vector2 res;

    int row;

    for (row = 0; row < 2; row++) {
        res.r[row] = v2Dot(v, m2Row(m, row));
    };

    return res;
}

/*
 * Apply matrix <m> to <v> and put the result back into <v>.
 */
void m2Apply(Matrix2 m, Vector2 *v)
{
    *v = m2Applied(m, *v);
}

#ifdef TEST
#include "utils.h"

static int errors = 0;

int main(int argc, char *argv[])
{
    Matrix2 m1 = m2New();

    make_sure_that(m1.c[0].r[0] == 0);
    make_sure_that(m1.c[0].r[1] == 0);
    make_sure_that(m1.c[1].r[0] == 0);
    make_sure_that(m1.c[1].r[1] == 0);

    m1 = m2Make(1, 2,
               3, 4);

    make_sure_that(m1.c[0].r[0] == 1);
    make_sure_that(m1.c[0].r[1] == 3);
    make_sure_that(m1.c[1].r[0] == 2);
    make_sure_that(m1.c[1].r[1] == 4);

    Vector2 v1 = v2Make(1, 2);
    Vector2 v2 = v2Make(3, 4);

    m1 = m2MakeV(v1, v2);

    make_sure_that(m1.c[0].r[0] == 1);
    make_sure_that(m1.c[0].r[1] == 2);
    make_sure_that(m1.c[1].r[0] == 3);
    make_sure_that(m1.c[1].r[1] == 4);

    m1 = m2Transposed(m1);

    make_sure_that(m1.c[0].r[0] == 1);
    make_sure_that(m1.c[0].r[1] == 3);
    make_sure_that(m1.c[1].r[0] == 2);
    make_sure_that(m1.c[1].r[1] == 4);

    m2Transpose(&m1);

    make_sure_that(m1.c[0].r[0] == 1);
    make_sure_that(m1.c[0].r[1] == 2);
    make_sure_that(m1.c[1].r[0] == 3);
    make_sure_that(m1.c[1].r[1] == 4);

    m1 = m2Scaled(m1, 2);

    make_sure_that(m1.c[0].r[0] == 2);
    make_sure_that(m1.c[0].r[1] == 4);
    make_sure_that(m1.c[1].r[0] == 6);
    make_sure_that(m1.c[1].r[1] == 8);

    m2Scale(&m1, 0.5);

    make_sure_that(m1.c[0].r[0] == 1);
    make_sure_that(m1.c[0].r[1] == 2);
    make_sure_that(m1.c[1].r[0] == 3);
    make_sure_that(m1.c[1].r[1] == 4);

    v1 = m2Row(m1, 0);
    v2 = m2Row(m1, 1);

    make_sure_that(v1.r[0] == 1);
    make_sure_that(v1.r[1] == 3);
    make_sure_that(v2.r[0] == 2);
    make_sure_that(v2.r[1] == 4);

    m1 = m2Identity();

    make_sure_that(m1.c[0].r[0] == 1);
    make_sure_that(m1.c[0].r[1] == 0);
    make_sure_that(m1.c[1].r[0] == 0);
    make_sure_that(m1.c[1].r[1] == 1);

    m1 = m2Make(0, -1,              // 90 degrees rotation CCW
                1,  0);

    make_sure_that(m1.c[0].r[0] ==  0);
    make_sure_that(m1.c[0].r[1] ==  1);
    make_sure_that(m1.c[1].r[0] == -1);
    make_sure_that(m1.c[1].r[1] ==  0);

    v1 = v2Make(1, 0);              // 2 primary unit vectors
    v2 = v2Make(0, 1);

    v1 = m2Applied(m1, v1);         // Rotate...
    v2 = m2Applied(m1, v2);

    make_sure_that(v1.r[0] ==  0);  // ... and check.
    make_sure_that(v1.r[1] ==  1);
    make_sure_that(v2.r[0] == -1);
    make_sure_that(v2.r[1] ==  0);

    int r = m2Invert(&m1);

    make_sure_that(r == 0);

    make_sure_that(m1.c[0].r[0] ==  0);
    make_sure_that(m1.c[0].r[1] == -1);
    make_sure_that(m1.c[1].r[0] ==  1);
    make_sure_that(m1.c[1].r[1] ==  0);

    m2Apply(m1, &v1);               // Rotate previous results back...
    m2Apply(m1, &v2);

    make_sure_that(v1.r[0] == 1);   // Should result in restored unit vectors.
    make_sure_that(v1.r[1] == 0);
    make_sure_that(v2.r[0] == 0);
    make_sure_that(v2.r[1] == 1);

    double det = m2Det(m1);

    make_sure_that(det == 1);

    Matrix2 m2 = m2Inverse(m1, det);

    make_sure_that(m2.c[0].r[0] ==  0);
    make_sure_that(m2.c[0].r[1] ==  1);
    make_sure_that(m2.c[1].r[0] == -1);
    make_sure_that(m2.c[1].r[1] ==  0);

    Matrix2 m = m2Product(m1, m2);

    make_sure_that(m.c[0].r[0] == 1);
    make_sure_that(m.c[0].r[1] == 0);
    make_sure_that(m.c[1].r[0] == 0);
    make_sure_that(m.c[1].r[1] == 1);

    m2Multiply(&m1, m2);

    make_sure_that(m1.c[0].r[0] == 1);
    make_sure_that(m1.c[0].r[1] == 0);
    make_sure_that(m1.c[1].r[0] == 0);
    make_sure_that(m1.c[1].r[1] == 1);

    m2 = m2Adjugate(m1);

    make_sure_that(m2.c[0].r[0] ==  1);
    make_sure_that(m2.c[0].r[1] ==  0);
    make_sure_that(m2.c[1].r[0] ==  0);
    make_sure_that(m2.c[1].r[1] == -1);

    m = m2Make(1, 2,            // Non-invertible matrix
               1, 2);

    det = m2Det(m);

    make_sure_that(det == 0);

    r = m2Invert(&m);

    make_sure_that(r != 0);

    return errors;
}

#endif
