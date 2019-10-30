#ifndef GEOMETRY2_H
#define GEOMETRY2_H

/*
 * geometry2.h: 2-dimensional geometry.
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-10-14
 * Version:   $Id: geometry2.h 358 2019-10-25 12:29:14Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "vector2.h"

typedef struct {
    Vector2 pv;     // Position vector
    Vector2 dv;     // Direction vector
} Line2;

typedef struct {
    Vector2 c;      // Center
    double r;       // Radius
} Circle2;

typedef struct {
    int count;
    Vector2 p[];
} Polygon2;

/*                _   _       _
 * If line <l> is l = s + n * d, this returns the <n> where line <l>
 * intersects line <m>. The result might be +/- infinity (if the lines are
 * parallel) or NAN (if they coincide).
 */
double g2LineLineIntersect(Line2 l, Line2 m);

/*
 * Returns the point where line <l> and <m> intersect. The coefficients of the
 * result may be +/- infinity (if the lines are parallel) or NAN (if they
 * coincide).
 */
Vector2 g2LineLineIntersection(Line2 l, Line2 m);

/*                _   _       _
 * If line <l> is l = s + n * d, this returns the <n> where point <p> is
 * projected on <l>. The result might be +/- infinity (if the lines are
 * parallel) or NAN (if they coincide).
 */
double g2PointLineProject(Vector2 p, Line2 l);

/*
 * Returns the projection of <p> on <l>. The coefficients of the result may be
 * +/- infinity (if the lines are parallel) or NAN (if they coincide).
 */
Vector2 g2PointLineProjection(Vector2 p, Line2 l);

#endif
