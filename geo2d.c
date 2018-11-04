/*
 * geo2d.c: 2D geometry
 *
 * Copyright: (c) 2018 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2018-11-04
 * Version:   $Id: geo2d.c 293 2018-11-04 20:21:33Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "geo2d.h"

static inline double sqr(double x) { return x * x; }

/*
 * Make a vector with <x> and <y> as its coordinates.
 */
Vector2D g2dVectorNew(double x, double y)
{
    Vector2D v = { .x = x, .y = y };

    return v;
}

/*
 * Add <v1> and <v2> and return the result.
 */
Vector2D g2dVectorAdd(Vector2D v1, Vector2D v2)
{
    return g2dVectorNew(v1.x + v2.x, v1.y + v2.y);
}

/*
 * Subtract <v2> from <v1> and return the result.
 */
Vector2D g2dVectorSubtract(Vector2D v1, Vector2D v2)
{
    return g2dVectorNew(v2.x - v1.x, v2.y - v1.y);
}

/*
 * Multiply <v> by a factor of r and return the result.
 */
Vector2D g2dVectorMultiply(Vector2D v, double r)
{
    return g2dVectorNew(v.x * r, v.y * r);
}

/*
 * Divide <v> by <r> and return the result.
 */
Vector2D g2dVectorDivide(Vector2D v, double r)
{
    return g2dVectorNew(v.x / r, v.y / r);
}

/*
 * Return the length of vector <v>.
 */
double g2dVectorLength(Vector2D v)
{
    return hypot(v.x, v.y);
}

/*
 * Normalize vector <v> and return the result.
 */
Vector2D g2dVectorNormalize(Vector2D v)
{
    double len = g2dVectorLength(v);

    return g2dVectorDivide(v, len);
}

/*
 * Make a line with position vector <pv> and direction vector <dv>.
 */
Line2D g2dLineNew(Vector2D pv, Vector2D dv)
{
    Line2D l = {
        .pv = pv,
        .dv = dv
    };

    return l;
}

/*
 * Make a line that runs through <p1> and <p2>.
 */
Line2D g2dLineThrough(Vector2D p1, Vector2D p2)
{
    Vector2D dv = { .x = p2.x - p1.x, .y = p2.y - p1.y };

    return g2dLineNew(p1, dv);
}

/*
 * Intersect lines <l1> and <l2>.
 *
 * If there is a single intersection point r1 and r2 are set to how far it lies
 * along the direction vectors of <l1> and <l2>, and 1 is returned. If the lines
 * are parallel but not overlapping r1 and r2 are set to positive or negative
 * infinity and 0 is returned. If the lines overlap r1 and r2 are set to NaN and
 * 0 is returned. r1 and/or r2 may be set to NULL.
 */
int g2dLineLineIntersect(Line2D l1, Line2D l2, double *r1, double *r2)
{
    double p, q;
    double num = (l2.dv.y * (l1.pv.x - l2.pv.x) - l2.dv.x * (l1.pv.y - l2.pv.y));
    double den = (l2.dv.x * l1.dv.y - l1.dv.x * l2.dv.y);

    p = num / den;

    if (l2.dv.x != 0) {
        q = (l1.pv.x + p * l1.dv.x - l2.pv.x) / l2.dv.x;
    }
    else {
        q = (l1.pv.y + p * l1.dv.y - l2.pv.y) / l2.dv.y;
    }

    if (r1) *r1 = p;
    if (r2) *r2 = q;

    return (isfinite(p) && isfinite(q));
}

/*
 * Intersect lines <l1> and <l2> and return the intersection point.
 *
 * if the lines are parallel, the returned coordinates will be positive or
 * negative infinity, or NaN.
 */
Vector2D g2dLineLineIntersection(Line2D l1, Line2D l2)
{
    double r;

    g2dLineLineIntersect(l1, l2, &r, NULL);

    return g2dVectorAdd(l1.pv, g2dVectorMultiply(l1.dv, r));
}

/*
 * Create a circle with center <center> and radius <radius>.
 */
Circle2D g2dCircleNew(Vector2D center, double radius)
{
    Circle2D circle = {
        .c = center,
        .r = radius
    };

    return circle;
}

/*
 * Intersect circle <c> with line <l> and write the positions of the
 * intersection on the line to r1 and r2. The number of intersections (0, 1 or
 * 2) is returned. r1 and r2 may be given as NULL.
 */
int g2dCircleLineIntersect(Circle2D c, Line2D l, double *r1, double *r2)
{
    l.pv.x -= c.c.x;
    l.pv.y -= c.c.y;

    const double A = sqr(l.dv.x) + sqr(l.dv.y);
    const double B = 2 * (l.pv.x * l.dv.x + l.pv.y * l.dv.y);
    const double C = sqr(l.pv.x) + sqr(l.pv.y) - sqr(c.r);

    const double discr = sqr(B) - 4 * A * C;

    if (discr > 0) {
        if (r1) *r1 = (-B - sqrt(discr)) / (2 * A);
        if (r2) *r2 = (-B + sqrt(discr)) / (2 * A);

        return 2;
    }
    else if (discr == 0) {
        if (r1) *r1 = -B / (2 * A);
        if (r2) *r2 = -B / (2 * A);

        return 1;
    }
    else {
        return 0;
    }
}

/*
 * Intersect circle <c> with line <l> and write the intersections to p1 and p2.
 * The number of intersections (0, 1 or * 2) is returned. p1 and p2 may be given
 * as NULL.
 */
int g2dCircleLineIntersections(Circle2D c, Line2D l, Vector2D *p1, Vector2D *p2)
{
    double r1, r2;

    const int r = g2dCircleLineIntersect(c, l, &r1, &r2);

    if (r >= 0) {
        if (p1) *p1 = g2dVectorAdd(l.pv, g2dVectorMultiply(l.dv, r));
        if (p2) *p2 = g2dVectorAdd(l.pv, g2dVectorMultiply(l.dv, r));
    }

    return r;
}

/*
 * Return the radial from <p1> that <p2> is on.
 */
double g2dRadial(Vector2D p1, Vector2D p2)
{
    return g2dNormalizeAngle(atan2(p2.x - p1.x, p2.y - p1.y));
}

/*
 * Normalize angle <angle> so that it lies in the range [0, 2 pi>.
 */
double g2dNormalizeAngle(double angle)
{
    while (angle <  0)        angle += 2 * M_PI;
    while (angle >= 2 * M_PI) angle -= 2 * M_PI;

    return angle;
}

/*
 * Normalize sweep <sweep> so that it lies in the range [0, 2pi> if <clockwise>
 * is TRUE, or <-2pi, 0] if clockwise is FALSE.
 */
double g2dNormalizeSweep(double sweep, int clockwise)
{
    if (clockwise) {
        while (sweep <  0)        sweep += 2 * M_PI;
        while (sweep >= 2 * M_PI) sweep -= 2 * M_PI;
    }
    else {
        while (sweep >  0)        sweep -= 2 * M_PI;
        while (sweep <= 2 * M_PI) sweep += 2 * M_PI;
    }

    return sweep;
}

#ifdef TEST

#include "utils.h"

int main(int argc, char *argv[])
{
    int r, errors = 0;
    double r1, r2;

    Vector2D p;
    Vector2D p1 = { 2, 0 };
    Vector2D p2 = { 2, 1 };

    Line2D l1 = g2dLineThrough(p1, p2);

    make_sure_that(l1.pv.x == 2);
    make_sure_that(l1.pv.y == 0);
    make_sure_that(l1.dv.x == 0);
    make_sure_that(l1.dv.y == 1);

    p1.x = 0;
    p1.y = 1;
    p2.x = 1;
    p2.y = 1;

    Line2D l2 = g2dLineThrough(p1, p2);

    make_sure_that(l2.pv.x == 0);
    make_sure_that(l2.pv.y == 1);
    make_sure_that(l2.dv.x == 1);
    make_sure_that(l2.dv.y == 0);

    r = g2dLineLineIntersect(l1, l2, &r1, &r2);

    // fprintf(stderr, "r = %d, r1 = %g, r2 = %g\n", r, r1, r2);

    make_sure_that(r == 1);
    make_sure_that(r1 == 1);
    make_sure_that(r2 == 2);

    p = g2dLineLineIntersection(l1, l2);

    make_sure_that(p.x == 2);
    make_sure_that(p.y == 1);

    // fprintf(stderr, "p.x = %g, p.y = %g\n", p.x, p.y);

    // Parallel but non-overlapping

    l1.dv.x = 1;
    l1.dv.y = 0;

    r = g2dLineLineIntersect(l1, l2, &r1, &r2);

    // fprintf(stderr, "r = %d, r1 = %g, r2 = %g\n", r, r1, r2);

    make_sure_that(r == 0);
    make_sure_that(isinf(r1) == 1);
    make_sure_that(isinf(r2) == 1);

    p = g2dLineLineIntersection(l1, l2);

    // fprintf(stderr, "p.x = %g, p.y = %g\n", p.x, p.y);

    // Anti-parallel but non-overlapping

    l1.dv.x = -1;
    l1.dv.y = 0;

    r = g2dLineLineIntersect(l1, l2, &r1, &r2);

    // fprintf(stderr, "r = %d, r1 = %g, r2 = %g\n", r, r1, r2);

    make_sure_that(r == 0);
    make_sure_that(isinf(r1));
    make_sure_that(isinf(r2));

    p = g2dLineLineIntersection(l1, l2);

    // fprintf(stderr, "p.x = %g, p.y = %g\n", p.x, p.y);

    // Parallel and overlapping

    l1.pv.x = 2;
    l1.pv.y = 1;

    l1.dv.x = 1;
    l1.dv.y = 0;

    r = g2dLineLineIntersect(l1, l2, &r1, &r2);

    // fprintf(stderr, "r = %d, r1 = %g, r2 = %g\n", r, r1, r2);

    make_sure_that(r == 0);
    make_sure_that(isnan(r1) == 1);
    make_sure_that(isnan(r2) == 1);

    p = g2dLineLineIntersection(l1, l2);

    // fprintf(stderr, "p.x = %g, p.y = %g\n", p.x, p.y);

    // Anti-parallel and overlapping

    l1.dv.x = -1;
    l1.dv.y = 0;

    r = g2dLineLineIntersect(l1, l2, &r1, &r2);

    // fprintf(stderr, "r = %d, r1 = %g, r2 = %g\n", r, r1, r2);

    make_sure_that(r == 0);
    make_sure_that(isnan(r1) == 1);
    make_sure_that(isnan(r2) == 1);

    p = g2dLineLineIntersection(l1, l2);

    // fprintf(stderr, "p.x = %g, p.y = %g\n", p.x, p.y);

    return errors;
}
#endif
