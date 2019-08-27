#ifndef MATRIX3_H
#define MATRIX3_H

/*
 * matrix3.h: Handles 3x3 matrices.
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-08-27
 * Version:   $Id: matrix3.h 344 2019-08-27 19:30:24Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "vector3.h"
#include "matrix2.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    Vector3 c[3];
} Matrix3;

/*
 * Return a matrix with all coefficients set to 0.
 */
Matrix3 m3New(void);

/*
 * Return a matrix with its coefficients set as indicated. Note that the first
 * three parameters are the first *row* of the matrix, the next three parameters
 * are the second row, etc. This allows for a more "natural" way of filling the
 * matrix.
 */
Matrix3 m3Make(double xx, double yx, double zx,
               double xy, double yy, double zy,
               double xz, double yz, double zz);

/*
 * Return a matrix where the three columns are set as indicated.
 */
Matrix3 m3MakeV(Vector3 c0, Vector3 c1, Vector3 c2);

/*
 * Return an identity matrix.
 */
Matrix3 m3Identity(void);

/*
 * Return a transposed version of the given matrix.
 */
Matrix3 m3Transposed(Matrix3 m);

/*
 * Transpose the matrix indicated by <m> in place.
 */
void m3Transpose(Matrix3 *m);

/*
 * Return the 2x2 "minor matrix" from <m> that *avoids* row <row> and column
 * <col>.
 */
Matrix2 m3Minor(Matrix3 m, int row, int col);

/*
 * Return row <row> from matrix <m> as a 3d vector.
 */
Vector3 m3Row(Matrix3 m, int row);

/*
 * Return the determinant of matrix <m>.
 */
double m3Det(Matrix3 m);

/*
 * Return the inverse of matrix <m>. Note that there is no way to report if the
 * determinant is 0, so it is left up to the caller to check for this and just
 * not call this function in that case. And if the caller has to calculate the
 * determinant themselves anyway, they might as well just pass it in.
 */
Matrix3 m3Inverse(Matrix3 m, double det);

/*
 * Invert matrix <m> in place. If succesful, 0 is returned. Otherwise (if the
 * determinant of the matrix is 0), 1 is returned and <m> is not changed.
 */
int m3Invert(Matrix3 *m);

/*
 * Return the adjugate of matrix <m> (where alternate coefficients have their
 * sign inverted).
 */
Matrix3 m3Adjugate(Matrix3 m);

/*
 * Return the result of multiplying matrices <m1> and <m2>.
 */
Matrix3 m3Product(Matrix3 m1, Matrix3 m2);

/*
 * Multiply the matrix indicated by <m1> with <m2> and put the result back into
 * <m1>.
 */
void m3Multiply(Matrix3 *m1, Matrix3 m2);

/*
 * Return the result of applying (multiplying) matrix <m> to vector <v>.
 */
Vector3 m3Applied(Matrix3 m, Vector3 v);

/*
 * Apply matrix <m> to <v> and put the result back into <v>.
 */
void m3Apply(Matrix3 m, Vector3 *v);

#ifdef __cplusplus
}
#endif

#endif
