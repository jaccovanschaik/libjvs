/*
 * vector2.c: Handles 2d vectors.
 *
 * vector2.c is part of libjvs.
 *
 * Copyright: (c) 2019-2021 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-08-27
 * Version:   $Id: vector2.c 438 2021-08-19 10:10:03Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <math.h>

#include "vector2.h"

/*
 * Return a new vector with all coordinates set to 0.
 */
Vector2 v2New(void)
{
    return v2Make(0, 0);
}

/*
 * Return a new vector with coordinates set to (<x>, <y>, <z>).
 */
Vector2 v2Make(double x, double y)
{
    Vector2 v = { .r[0] = x, .r[1] = y };

    return v;
}

/*
 * Set the coordinates of <v> to (<x>, <y>, <z>).
 */
void v2Set(Vector2 *v, double x, double y)
{
    v->r[0] = x;
    v->r[1] = y;
}

/*
 * Return the sum of vectors <v1> and <v2>.
 */
Vector2 v2Sum(Vector2 v1, Vector2 v2)
{
    return v2Make(v1.r[0] + v2.r[0], v1.r[1] + v2.r[1]);
}

/*
 * Add vector <d> to <v>.
 */
void v2Add(Vector2 *v, Vector2 d)
{
    v->r[0] += d.r[0];
    v->r[1] += d.r[1];
}

/*
 * Return the difference of vectors <v1> and <v2>.
 */
Vector2 v2Diff(Vector2 v1, Vector2 v2)
{
    return v2Make(v1.r[0] - v2.r[0], v1.r[1] - v2.r[1]);
}

/*
 * Subtract vector <d> from <v>.
 */
void v2Sub(Vector2 *v, Vector2 d)
{
    v->r[0] -= d.r[0];
    v->r[1] -= d.r[1];
}

/*
 * Return the square of the length of <v>.
 */
double v2LenSquared(Vector2 v)
{
    return v.r[0] * v.r[0] + v.r[1] * v.r[1];
}

/*
 * Return the length of vector <v>.
 */
double v2Len(Vector2 v)
{
    return sqrt(v2LenSquared(v));
}

/*
 * Scale vector <v> with factor <scale>.
 */
void v2Scale(Vector2 *v, double scale)
{
    v->r[0] *= scale;
    v->r[1] *= scale;
}

/*
 * Return vector <v> scaled with factor <scale>.
 */
Vector2 v2Scaled(Vector2 v, double scale)
{
    return v2Make(v.r[0] * scale, v.r[1] * scale);
}

/*
 * Normalize vector <v>, i.e. set its length to 1.
 */
void v2Normalize(Vector2 *v)
{
    double len = v2Len(*v);

    v->r[0] /= len;
    v->r[1] /= len;
}

/*
 * Return vector <v> normalized, i.e. with its length set to 1.
 */
Vector2 v2Normalized(Vector2 v)
{
    return v2Scaled(v, 1 / v2Len(v));
}

/*
 * Return the dot product of vectors <v1> and <v2>.
 */
double v2Dot(Vector2 v1, Vector2 v2)
{
    return v1.r[0] * v2.r[0] + v1.r[1] * v2.r[1];
}

/*
 * Return the cosine of the angle between vectors <v1> and <v2>.
 */
double v2Cos(Vector2 v1, Vector2 v2)
{
    return v2Dot(v1, v2) / (v2Len(v1) * v2Len(v2));
}

/*
 * Return the angle between vectors <v1> and <v2> in radians.
 */
double v2Angle(Vector2 v1, Vector2 v2)
{
    return acos(v2Cos(v1, v2));
}

#ifdef TEST
#include "utils.h"

static int errors = 0;

int main(void)
{
    Vector2 v1 = v2New();

    make_sure_that(v1.r[0] == 0);
    make_sure_that(v1.r[1] == 0);

    v2Set(&v1, 0, 1);

    make_sure_that(v1.r[0] == 0);
    make_sure_that(v1.r[1] == 1);

    Vector2 v2 = v2Make(1, 2);

    make_sure_that(v2.r[0] == 1);
    make_sure_that(v2.r[1] == 2);

    v2Add(&v1, v2);

    make_sure_that(v1.r[0] == 1);
    make_sure_that(v1.r[1] == 3);

    Vector2 v3 = v2Sum(v1, v2);

    make_sure_that(v3.r[0] == 2);
    make_sure_that(v3.r[1] == 5);

    v2Sub(&v3, v1);

    make_sure_that(v3.r[0] == 1);
    make_sure_that(v3.r[1] == 2);

    v3 = v2Diff(v3, v1);

    make_sure_that(v3.r[0] == 0);
    make_sure_that(v3.r[1] == -1);

    v3 = v2Make(3, 4);

    make_sure_that(v3.r[0] == 3);
    make_sure_that(v3.r[1] == 4);
    make_sure_that(v2LenSquared(v3) == 25);
    make_sure_that(v2Len(v3) == 5);

    v2 = v2Scaled(v3, 2);

    make_sure_that(v2.r[0] == 6);
    make_sure_that(v2.r[1] == 8);

    v2Scale(&v3, 2);

    make_sure_that(v3.r[0] == 6);
    make_sure_that(v3.r[1] == 8);

    v1 = v2Normalized(v3);

    make_sure_that(close_to(v1.r[0], 0.6));
    make_sure_that(close_to(v1.r[1], 0.8));

    v2Normalize(&v3);

    make_sure_that(close_to(v3.r[0], 0.6));
    make_sure_that(close_to(v3.r[1], 0.8));

    v2Set(&v1, 1, 2);
    v2Set(&v2, 2, 1);

    make_sure_that(v2Dot(v1, v2) == 4);
    make_sure_that(close_to(v2Cos(v1, v2), 0.8));

    double angle = acos(0.8);

    make_sure_that(close_to(v2Angle(v1, v2), angle));

    return errors;
}
#endif
