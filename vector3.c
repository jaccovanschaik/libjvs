/*
 * vector3.c: Handles 3d vectors.
 *
 * vector3.c is part of libjvs.
 *
 * Copyright: (c) 2019-2024 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-08-27
 * Version:   $Id: vector3.c 497 2024-06-03 12:37:20Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <math.h>

#include "vector3.h"

/*
 * Return a new vector with all coordinates set to 0.
 */
Vector3 v3New(void)
{
    return v3Make(0, 0, 0);
}

/*
 * Return a new vector with coordinates set to (<x>, <y>, <z>).
 */
Vector3 v3Make(double x, double y, double z)
{
    Vector3 v = { .r[0] = x, .r[1] = y, .r[2] = z };

    return v;
}

/*
 * Set the coordinates of <v> to (<x>, <y>, <z>).
 */
void v3Set(Vector3 *v, double x, double y, double z)
{
    v->r[0] = x;
    v->r[1] = y;
    v->r[2] = z;
}

/*
 * Return the sum of vectors <v1> and <v2>.
 */
Vector3 v3Sum(Vector3 v1, Vector3 v2)
{
    return v3Make(v1.r[0] + v2.r[0], v1.r[1] + v2.r[1], v1.r[2] + v2.r[2]);
}

/*
 * Add vector <d> to <v>.
 */
void v3Add(Vector3 *v, Vector3 d)
{
    v->r[0] += d.r[0];
    v->r[1] += d.r[1];
    v->r[2] += d.r[2];
}

/*
 * Return the difference of vectors <v1> and <v2>.
 */
Vector3 v3Diff(Vector3 v1, Vector3 v2)
{
    return v3Make(v1.r[0] - v2.r[0], v1.r[1] - v2.r[1], v1.r[2] - v2.r[2]);
}

/*
 * Subtract vector <d> from <v>.
 */
void v3Sub(Vector3 *v, Vector3 d)
{
    v->r[0] -= d.r[0];
    v->r[1] -= d.r[1];
    v->r[2] -= d.r[2];
}

/*
 * Return the square of the length of vector <v>.
 */
double v3LenSquared(Vector3 v)
{
    return v.r[0] * v.r[0] + v.r[1] * v.r[1] + v.r[2] * v.r[2];
}

/*
 * Return the length of vector <v>.
 */
double v3Len(Vector3 v)
{
    return sqrt(v3LenSquared(v));
}

/*
 * Scale vector <v> with factor <scale>.
 */
void v3Scale(Vector3 *v, double scale)
{
    v->r[0] *= scale;
    v->r[1] *= scale;
    v->r[2] *= scale;
}

/*
 * Return vector <v> scaled with factor <scale>.
 */
Vector3 v3Scaled(Vector3 v, double scale)
{
    return v3Make(v.r[0] * scale, v.r[1] * scale, v.r[2] * scale);
}

/*
 * Normalize vector <v>, i.e. set its length to 1.
 */
void v3Normalize(Vector3 *v)
{
    double len = v3Len(*v);

    v->r[0] /= len;
    v->r[1] /= len;
    v->r[2] /= len;
}

/*
 * Return vector <v> normalized, i.e. with its length set to 1.
 */
Vector3 v3Normalized(Vector3 v)
{
    return v3Scaled(v, 1 / v3Len(v));
}

/*
 * Return the dot product of vectors <v1> and <v2>.
 */
double v3Dot(Vector3 v1, Vector3 v2)
{
    return v1.r[0] * v2.r[0] + v1.r[1] * v2.r[1] + v1.r[2] * v2.r[2];
}

/*
 * Return the cosine of the angle between vectors <v1> and <v2>.
 */
double v3Cos(Vector3 v1, Vector3 v2)
{
    return v3Dot(v1, v2) / (v3Len(v1) * v3Len(v2));
}

/*
 * Return the angle between vectors <v1> and <v2> in radians.
 */
double v3Angle(Vector3 v1, Vector3 v2)
{
    return acos(v3Cos(v1, v2));
}

/*
 * Return the cross product of vectors <v1> and <v2>.
 */
Vector3 v3Cross(Vector3 v1, Vector3 v2)
{
    return v3Make(
            v1.r[1] * v2.r[2] - v1.r[2] * v2.r[1],
            v1.r[2] * v2.r[0] - v1.r[0] * v2.r[2],
            v1.r[0] * v2.r[1] - v1.r[1] * v2.r[0]);
}

#ifdef TEST
#include "utils.h"

static int errors = 0;

int main(void)
{
    Vector3 v1 = v3New();

    make_sure_that(v1.r[0] == 0);
    make_sure_that(v1.r[1] == 0);
    make_sure_that(v1.r[2] == 0);

    Vector3 v2 = v3Make(1, 2, 3);

    make_sure_that(v2.r[0] == 1);
    make_sure_that(v2.r[1] == 2);
    make_sure_that(v2.r[2] == 3);

    v3Set(&v1, 4, 5, 6);

    make_sure_that(v1.r[0] == 4);
    make_sure_that(v1.r[1] == 5);
    make_sure_that(v1.r[2] == 6);

    Vector3 v3 = v3Sum(v1, v2);

    make_sure_that(v3.r[0] == 5);
    make_sure_that(v3.r[1] == 7);
    make_sure_that(v3.r[2] == 9);

    v3Add(&v3, v1);

    make_sure_that(v3.r[0] == 9);
    make_sure_that(v3.r[1] == 12);
    make_sure_that(v3.r[2] == 15);

    v1 = v3Diff(v3, v2);

    make_sure_that(v1.r[0] == 8);
    make_sure_that(v1.r[1] == 10);
    make_sure_that(v1.r[2] == 12);

    v3Sub(&v1, v2);

    make_sure_that(v1.r[0] == 7);
    make_sure_that(v1.r[1] == 8);
    make_sure_that(v1.r[2] == 9);

    v3Set(&v1, 1, 4, 8);

    make_sure_that(v3LenSquared(v1) == 81);
    make_sure_that(v3Len(v1) == 9);

    v2 = v3Scaled(v1, 2);

    make_sure_that(v2.r[0] == 2);
    make_sure_that(v2.r[1] == 8);
    make_sure_that(v2.r[2] == 16);

    v3Scale(&v1, 3);

    make_sure_that(v1.r[0] == 3);
    make_sure_that(v1.r[1] == 12);
    make_sure_that(v1.r[2] == 24);

// void v3Normalize(Vector3 *v);
// Vector3 v3Normalized(Vector3 v);
// double v3Dot(Vector3 v1, Vector3 v2);
// double v3Cos(Vector3 v1, Vector3 v2);
// double v3Angle(Vector3 v1, Vector3 v2);
// Vector3 v3Cross(Vector3 v1, Vector3 v2);

    return errors;
}
#endif
