#ifndef GEOMETRY2_H
#define GEOMETRY2_H

/*
 * geometry2.h: XXX
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-10-14
 * Version:   $Id: geometry2.h 351 2019-10-14 12:02:28Z jacco $
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

double g2LineLineIntersect(Line2 l, Line2 m);

Vector2 g2LineLineIntersection(Line2 l, Line2 m);

double g2PointLineProject(Vector2 p, Line2 l);

Vector2 g2PointLineProjection(Vector2 p, Line2 l);

#endif
