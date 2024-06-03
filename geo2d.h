#ifndef GEO2D_H
#define GEO2D_H

/*
 * geo2d.c: 2D geometry
 *
 * geo2d.c is part of libjvs.
 *
 * Copyright: (c) 2018-2024 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2018-11-04
 * Version:   $Id: geo2d.h 497 2024-06-03 12:37:20Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

typedef struct {
    double x, y;
} Vector2D;

typedef struct {
    Vector2D pv;
    Vector2D dv;
} Line2D;

typedef struct {
    Vector2D c;
    double r;
} Circle2D;

typedef struct {
    Vector2D c[2];
} Matrix2x2;

/*
 * Make a vector with <x> and <y> as its coordinates.
 */
Vector2D g2dVectorNew(double x, double y);

/*
 * Add <v1> and <v2> and return the result.
 */
Vector2D g2dVectorAdd(Vector2D v1, Vector2D v2);

/*
 * Subtract <v2> from <v1> and return the result.
 */
Vector2D g2dVectorSubtract(Vector2D v1, Vector2D v2);

/*
 * Multiply <v> by a factor of r and return the result.
 */
Vector2D g2dVectorMultiply(Vector2D v, double r);

/*
 * Divide <v> by <r> and return the result.
 */
Vector2D g2dVectorDivide(Vector2D v, double r);

/*
 * Return the length of vector <v>.
 */
double g2dVectorLength(Vector2D v);

/*
 * Normalize vector <v> and return the result.
 */
Vector2D g2dVectorNormalize(Vector2D v);

/*
 * Make a line with position vector <pv> and direction vector <dv>.
 */
Line2D g2dLineNew(Vector2D pv, Vector2D dv);

/*
 * Make a line that runs through <p1> and <p2>.
 */
Line2D g2dLineThrough(Vector2D p1, Vector2D p2);

/*
 * Intersect lines <l1> and <l2>.
 *
 * If there is a single intersection point r1 and r2 are set to how far it lies
 * along the direction vectors of <l1> and <l2>, and 1 is returned. If the lines
 * are parallel but not overlapping r1 and r2 are set to positive or negative
 * infinity and 0 is returned. If the lines overlap r1 and r2 are set to NaN and
 * 0 is returned. r1 and/or r2 may be set to NULL.
 */
int g2dLineLineIntersect(Line2D l1, Line2D l2, double *r1, double *r2);

/*
 * Intersect lines <l1> and <l2> and return the intersection point.
 *
 * if the lines are parallel, the returned coordinates will be positive or
 * negative infinity, or NaN.
 */
Vector2D g2dLineLineIntersection(Line2D l1, Line2D l2);

/*
 * Create a circle with center <center> and radius <radius>.
 */
Circle2D g2dCircleNew(Vector2D center, double radius);

/*
 * Intersect circle <c> with line <l> and write the positions of the
 * intersection on the line to r1 and r2. The number of intersections (0, 1 or
 * 2) is returned. r1 and r2 may be given as NULL.
 */
int g2dCircleLineIntersect(Circle2D c, Line2D l, double *r1, double *r2);

/*
 * Intersect circle <c> with line <l> and write the intersections to p1 and p2.
 * The number of intersections (0, 1 or * 2) is returned. p1 and p2 may be given
 * as NULL.
 */
int g2dCircleLineIntersections(Circle2D c, Line2D l, Vector2D *p1, Vector2D *p2);

/*
 * Return the radial from <p1> that <p2> is on.
 */
double g2dRadial(Vector2D p1, Vector2D p2);

/*
 * Normalize angle <angle> so that it lies in the range [0, 2 pi>.
 */
double g2dNormalizeAngle(double angle);

/*
 * Normalize sweep <sweep> so that it lies in the range [0, 2pi> if <clockwise>
 * is TRUE, or <-2pi, 0] if clockwise is FALSE.
 */
double g2dNormalizeSweep(double sweep, int clockwise);

/*
 * Return a 2x2 identity matrix.
 */
Matrix2x2 g2dMatrixIdentity(void);

/*
 * Return a matrix that scales by a factor of <x_factor> in the X direction and
 * a factor of <y_factor> in the Y direction.
 */
Matrix2x2 g2dMatrixScale(double x_factor, double y_factor);

/*
 * Return a matrix that rotates around the origin along <angle> degrees. NOTE:
 * positive angles are *clockwise*.
 */
Matrix2x2 g2dMatrixRotation(double angle);

/*
 * Multiply matrices m1 and m2 and return the result.
 */
Matrix2x2 g2dMatrixMultiply(Matrix2x2 m1, Matrix2x2 m2);

#endif
