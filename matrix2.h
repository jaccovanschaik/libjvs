#ifndef MATRIX2_H
#define MATRIX2_H

/*
 * matrix2.h: Handles 2x2 matrices.
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-08-27
 * Version:   $Id: matrix2.h 344 2019-08-27 19:30:24Z jacco $
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
 * Return the determinant of <m>.
 */
double m2Det(Matrix2 m);

#ifdef __cplusplus
}
#endif

#endif
