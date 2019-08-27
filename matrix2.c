/*
 * matrix2.c: Handles 2x2 matrices.
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-08-27
 * Version:   $Id: matrix2.c 344 2019-08-27 19:30:24Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "matrix2.h"

/*
 * Return the determinant of <m>.
 */
double m2Det(Matrix2 m)
{
    return m.c[0].r[0] * m.c[1].r[1] - m.c[0].r[1] * m.c[1].r[0];
}
