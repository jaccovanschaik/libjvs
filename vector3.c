/*
 * vector3.c: Handles 3d vectors.
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-08-27
 * Version:   $Id: vector3.c 344 2019-08-27 19:30:24Z jacco $
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
 * Return the length of vector <v>.
 */
double v3Len(Vector3 v)
{
    return sqrt(v.r[0] * v.r[0] + v.r[1] * v.r[1] + v.r[2] * v.r[2]);
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
