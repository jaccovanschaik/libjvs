#ifndef MATRIX2_H
#define MATRIX2_H

/*
 * matrix2.h: Handles 2x2 matrices.
 *
 * matrix2.h is part of libjvs.
 *
 * Copyright: (c) 2019-2024 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-08-27
 * Version:   $Id: matrix2.h 497 2024-06-03 12:37:20Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "vector2.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    Vector2 c[2];
} Matrix2;

/*
 * Return a matrix with all coefficients set to 0.
 */
Matrix2 m2New(void);

/*
 * Return a matrix with its coefficients set as indicated. Note that the first
 * two parameters are the first *row* of the matrix and the next two parameters
 * are the second row. This allows for a more "natural" way of filling the
 * matrix.
 */
Matrix2 m2Make(double xx, double yx,
               double xy, double yy);

/*
 * Return a matrix where the two columns are set as indicated.
 */
Matrix2 m2MakeV(Vector2 c0, Vector2 c1);

/*
 * Return an identity matrix.
 */
Matrix2 m2Identity(void);

/*
 * Return a transposed version of the given matrix.
 */
Matrix2 m2Transposed(Matrix2 m);

/*
 * Transpose the matrix indicated by <m> in place.
 */
void m2Transpose(Matrix2 *m);

/*
 * Return row <row> from matrix <m> as a 2d vector.
 */
Vector2 m2Row(Matrix2 m, int row);

/*
 * Return the determinant of <m>.
 */
double m2Det(Matrix2 m);

/*
 * Return a version of <m> where each coefficient is scaled by <factor>.
 */
Matrix2 m2Scaled(Matrix2 m, double factor);

/*
 * Scale the matrix indicated by <m> by <factor>.
 */
void m2Scale(Matrix2 *m, double factor);

/*
 * Return the inverse of matrix <m>. Note that there is no way to report if the
 * determinant is 0, so it is left up to the caller to check for this and avoid
 * calling this function in that case. And if the caller has to calculate the
 * determinant themselves anyway, they might as well just pass it in so I don't
 * have to do it again.
 */
Matrix2 m2Inverse(Matrix2 m, double det);

/*
 * Invert matrix <m> in place. If succesful, 0 is returned. Otherwise (if the
 * determinant of the matrix is 0), 1 is returned and <m> is not changed.
 */
int m2Invert(Matrix2 *m);

/*
 * Return the adjugate of matrix <m> (where alternate coefficients have their
 * sign inverted).
 */
Matrix2 m2Adjugate(Matrix2 m);

/*
 * Return the result of multiplying matrices <m1> and <m2>.
 */
Matrix2 m2Product(Matrix2 m1, Matrix2 m2);

/*
 * Multiply the matrix indicated by <m1> with <m2> and put the result back into
 * <m1>.
 */
void m2Multiply(Matrix2 *m1, Matrix2 m2);

/*
 * Return the result of applying (multiplying) matrix <m> to vector <v>.
 */
Vector2 m2Applied(Matrix2 m, Vector2 v);

/*
 * Apply matrix <m> to <v> and put the result back into <v>.
 */
void m2Apply(Matrix2 m, Vector2 *v);

#ifdef __cplusplus
}
#endif

#endif
