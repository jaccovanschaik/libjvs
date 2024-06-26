/*
 * geometry2.h: 2-dimensional geometry.
 *
 * Copyright: (c) 2019-2024 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-10-14
 * Version:   $Id: geometry2.c 497 2024-06-03 12:37:20Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "vector2.h"
#include "matrix2.h"

#include "geometry2.h"

/*                _   _       _
 * If line <l> is l = s + n * d, this returns the <n> where line <l>
 * intersects line <m>. The result might be +/- infinity (if the lines are
 * parallel) or NAN (if they coincide).
 */
double g2LineLineIntersect(Line2 l, Line2 m)
{
    double num = (m.dv.r[1] * (l.pv.r[0] - m.pv.r[0]) - m.dv.r[0]
               * (l.pv.r[1] - m.pv.r[1]));
    double den = (m.dv.r[0] * l.dv.r[1] - l.dv.r[0] * m.dv.r[1]);

    return num / den;
}

/*
 * Returns the point where line <l> and <m> intersect. The coefficients of the
 * result may be +/- infinity (if the lines are parallel) or NAN (if they
 * coincide).
 */
Vector2 g2LineLineIntersection(Line2 l, Line2 m)
{
    double mult = g2LineLineIntersect(l, m);

    return v2Sum(l.pv, v2Scaled(l.dv, mult));
}

/*                _   _       _
 * If line <l> is l = s + n * d, this returns the <n> where point <p> is
 * projected on <l>.
 */
double g2PointLineProject(Vector2 p, Line2 l)
{
    double num = l.dv.r[0] * (p.r[0] - l.pv.r[0]) + l.dv.r[1]
               * (p.r[1] - l.pv.r[1]);
    double den = l.dv.r[0] * l.dv.r[0] + l.dv.r[1] * l.dv.r[1];

    return num / den;
}

/*
 * Returns the projection of <p> on <l>.
 */
Vector2 g2PointLineProjection(Vector2 p, Line2 l)
{
    double mult = g2PointLineProject(p, l);

    return v2Sum(l.pv, v2Scaled(l.dv, mult));
}
