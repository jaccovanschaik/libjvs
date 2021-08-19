#ifndef VECTOR2_H
#define VECTOR2_H

/*
 * vector2.h: Handles 2d vectors.
 *
 * vector2.h is part of libjvs.
 *
 * Copyright: (c) 2019-2021 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-08-27
 * Version:   $Id: vector2.h 438 2021-08-19 10:10:03Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double r[2];
} Vector2;

/*
 * Return a new vector with all coordinates set to 0.
 */
Vector2 v2New(void);

/*
 * Return a new vector with coordinates set to (<x>, <y>, <z>).
 */
Vector2 v2Make(double x, double y);

/*
 * Set the coordinates of <v> to (<x>, <y>, <z>).
 */
void v2Set(Vector2 *v, double x, double y);

/*
 * Return the sum of vectors <v1> and <v2>.
 */
Vector2 v2Sum(Vector2 v1, Vector2 v2);

/*
 * Add vector <d> to <v>.
 */
void v2Add(Vector2 *v, Vector2 d);

/*
 * Return the difference of vectors <v1> and <v2>.
 */
Vector2 v2Diff(Vector2 v1, Vector2 v2);

/*
 * Subtract vector <d> from <v>.
 */
void v2Sub(Vector2 *v, Vector2 d);

/*
 * Return the square of the length of <v>.
 */
double v2LenSquared(Vector2 v);

/*
 * Return the length of vector <v>.
 */
double v2Len(Vector2 v);

/*
 * Scale vector <v> with factor <scale>.
 */
void v2Scale(Vector2 *v, double scale);

/*
 * Return vector <v> scaled with factor <scale>.
 */
Vector2 v2Scaled(Vector2 v, double scale);

/*
 * Normalize vector <v>, i.e. set its length to 1.
 */
void v2Normalize(Vector2 *v);

/*
 * Return vector <v> normalized, i.e. with its length set to 1.
 */
Vector2 v2Normalized(Vector2 v);

/*
 * Return the dot product of vectors <v1> and <v2>.
 */
double v2Dot(Vector2 v1, Vector2 v2);

/*
 * Return the cosine of the angle between vectors <v1> and <v2>.
 */
double v2Cos(Vector2 v1, Vector2 v2);

/*
 * Return the angle between vectors <v1> and <v2> in radians.
 */
double v2Angle(Vector2 v1, Vector2 v2);

#ifdef __cplusplus
}
#endif

#endif
