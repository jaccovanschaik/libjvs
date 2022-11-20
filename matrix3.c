/*
 * matrix3.c: Handles 3x3 matrices.
 *
 * matrix3.c is part of libjvs.
 *
 * Copyright: (c) 2019-2022 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-08-27
 * Version:   $Id: matrix3.c 467 2022-11-20 00:05:38Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <string.h>
#include <assert.h>

#include "matrix2.h"
#include "matrix3.h"

/*
 * Return a matrix with all coefficients set to 0.
 */
Matrix3 m3New(void)
{
    Matrix3 m;

    memset(&m, 0, sizeof(m));

    return m;
}

/*
 * Return a matrix with its coefficients set as indicated. Note that the first
 * three parameters are the first *row* of the matrix, the next three parameters
 * are the second row, etc. This allows for a more "natural" way of filling the
 * matrix.
 */
Matrix3 m3Make(double xx, double yx, double zx,
               double xy, double yy, double zy,
               double xz, double yz, double zz)
{
    Matrix3 m = {
        .c[0] = v3Make(xx, xy, xz),
        .c[1] = v3Make(yx, yy, yz),
        .c[2] = v3Make(zx, zy, zz),
    };

    return m;
}

/*
 * Return a matrix where the three columns are set as indicated.
 */
Matrix3 m3MakeV(Vector3 c0, Vector3 c1, Vector3 c2)
{
    Matrix3 m = { .c[0] = c0, .c[1] = c1, .c[2] = c2 };

    return m;
}

/*
 * Return an identity matrix.
 */
Matrix3 m3Identity(void)
{
    return m3Make(1, 0, 0,
                  0, 1, 0,
                  0, 0, 1);
}

/*
 * Return a transposed version of the given matrix.
 */
Matrix3 m3Transposed(Matrix3 m)
{
    return m3Make(
            m.c[0].r[0], m.c[0].r[1], m.c[0].r[2],
            m.c[1].r[0], m.c[1].r[1], m.c[1].r[2],
            m.c[2].r[0], m.c[2].r[1], m.c[2].r[2]);
}

/*
 * Transpose the matrix indicated by <m> in place.
 */
void m3Transpose(Matrix3 *m)
{
    *m = m3Transposed(*m);
}

/*
 * Return the 2x2 "minor matrix" from <m> that *avoids* row <row> and column
 * <col>.
 */
Matrix2 m3Minor(Matrix3 m, int row, int col)
{
    Matrix2 mm;

    int rows[2], cols[2];

    assert(row >= 0 && row <= 2);
    assert(col >= 0 && col <= 2);

    rows[0] = row == 0 ? 1 : 0;
    rows[1] = row == 2 ? 1 : 2;

    cols[0] = col == 0 ? 1 : 0;
    cols[1] = col == 2 ? 1 : 2;

    for (row = 0; row < 2; ++row) {
        for (col = 0; col < 2; ++col) {
            mm.c[col].r[row] = m.c[cols[col]].r[rows[row]];
        }
    }

    return mm;
}

/*
 * Return row <row> from matrix <m> as a 3d vector.
 */
Vector3 m3Row(Matrix3 m, int row)
{
    assert(row >= 0 && row < 3);

    return v3Make(m.c[0].r[row], m.c[1].r[row], m.c[2].r[row]);
}

/*
 * Return the determinant of matrix <m>.
 */
double m3Det(Matrix3 m)
{
    double d00 = m2Det(m3Minor(m, 0, 0));
    double d01 = m2Det(m3Minor(m, 0, 1));
    double d02 = m2Det(m3Minor(m, 0, 2));

    return m.c[0].r[0] * d00 - m.c[1].r[0] * d01 + m.c[1].r[0] * d02;
}

/*
 * Return a version of <m> where each coefficient is scaled by <factor>.
 */
Matrix3 m3Scaled(Matrix3 m, double factor)
{
    Matrix3 s;

    int row, col;

    for (row = 0; row < 3; row++) {
        for (col = 0; col < 3; col++) {
            s.c[col].r[row] = m.c[col].r[row] * factor;
        }
    }

    return s;
}

/*
 * Scale the matrix indicated by <m> by <factor>.
 */
void m3Scale(Matrix3 *m, double factor)
{
    *m = m3Scaled(*m, factor);
}

/*
 * Return the inverse of matrix <m>. Note that there is no way to report if the
 * determinant is 0, so it is left up to the caller to check for this and avoid
 * calling this function in that case. And if the caller has to calculate the
 * determinant themselves anyway, they might as well just pass it in so I don't
 * have to do it again.
 */
Matrix3 m3Inverse(Matrix3 m, double det)
{
    Matrix3 T = m3Transposed(m);
    Matrix3 I;

    int row, col, sign = 1;

    for (row = 0; row < 3; row++) {
        for (col = 0; col < 3; col++) {
            I.c[col].r[row] = sign * m2Det(m3Minor(T, row, col)) / det;
            sign = -sign;
        }
    }

    return I;
}

/*
 * Invert matrix <m> in place. If succesful, 0 is returned. Otherwise (if the
 * determinant of the matrix is 0), 1 is returned and <m> is not changed.
 */
int m3Invert(Matrix3 *m)
{
    double det = m3Det(*m);

    if (det == 0) {
        return 1;
    }

    *m = m3Inverse(*m, det);

    return 0;
}

/*
 * Return the adjugate of matrix <m> (where alternate coefficients have their
 * sign inverted).
 */
Matrix3 m3Adjugate(Matrix3 m)
{
    Matrix3 A;

    int row, col, sign = 1;

    for (row = 0; row < 3; row++) {
        for (col = 0; col < 3; col++) {
            A.c[col].r[row] = sign * m.c[col].r[row];
            sign = -sign;
        }
    }

    return A;
}

/*
 * Return the result of multiplying matrices <m1> and <m2>.
 */
Matrix3 m3Product(Matrix3 m1, Matrix3 m2)
{
    Matrix3 P;

    int row, col;

    for (row = 0; row < 3; row++) {
        Vector3 r = m3Row(m2, row);

        for (col = 0; col < 3; col++) {
            P.c[col].r[row] = v3Dot(m1.c[col], r);
        }
    }

    return P;
}

/*
 * Multiply the matrix indicated by <m1> with <m2> and put the result back into
 * <m1>.
 */
void m3Multiply(Matrix3 *m1, Matrix3 m2)
{
    *m1 = m3Product(*m1, m2);
}

/*
 * Return the result of applying (multiplying) matrix <m> to vector <v>.
 */
Vector3 m3Applied(Matrix3 m, Vector3 v)
{
    Vector3 res;

    int row;

    for (row = 0; row < 3; row++) {
        res.r[row] = v3Dot(v, m3Row(m, row));
    };

    return res;
}

/*
 * Apply matrix <m> to <v> and put the result back into <v>.
 */
void m3Apply(Matrix3 m, Vector3 *v)
{
    *v = m3Applied(m, *v);
}
