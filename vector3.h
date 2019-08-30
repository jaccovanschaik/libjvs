#ifndef VECTOR3_H
#define VECTOR3_H

/*
 * vector3.h: Handles 3d vectors.
 *
 * vector3.h is part of libjvs.
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-08-27
 * Version:   $Id: vector3.h 346 2019-08-28 07:38:39Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double r[3];
} Vector3;

/*
 * Return a new vector with all coordinates set to 0.
 */
Vector3 v3New(void);

/*
 * Return a new vector with coordinates set to (<x>, <y>, <z>).
 */
Vector3 v3Make(double x, double y, double z);

/*
 * Set the coordinates of <v> to (<x>, <y>, <z>).
 */
void v3Set(Vector3 *v, double x, double y, double z);

/*
 * Return the sum of vectors <v1> and <v2>.
 */
Vector3 v3Sum(Vector3 v1, Vector3 v2);

/*
 * Add vector <d> to <v>.
 */
void v3Add(Vector3 *v, Vector3 d);

/*
 * Return the difference of vectors <v1> and <v2>.
 */
Vector3 v3Diff(Vector3 v1, Vector3 v2);

/*
 * Subtract vector <d> from <v>.
 */
void v3Sub(Vector3 *v, Vector3 d);

/*
 * Return the length of vector <v>.
 */
double v3Len(Vector3 v);

/*
 * Scale vector <v> with factor <scale>.
 */
void v3Scale(Vector3 *v, double scale);

/*
 * Return vector <v> scaled with factor <scale>.
 */
Vector3 v3Scaled(Vector3 v, double scale);

/*
 * Normalize vector <v>, i.e. set its length to 1.
 */
void v3Normalize(Vector3 *v);

/*
 * Return vector <v> normalized, i.e. with its length set to 1.
 */
Vector3 v3Normalized(Vector3 v);

/*
 * Return the dot product of vectors <v1> and <v2>.
 */
double v3Dot(Vector3 v1, Vector3 v2);

/*
 * Return the cosine of the angle between vectors <v1> and <v2>.
 */
double v3Cos(Vector3 v1, Vector3 v2);

/*
 * Return the angle between vectors <v1> and <v2> in radians.
 */
double v3Angle(Vector3 v1, Vector3 v2);

/*
 * Return the cross product of vectors <v1> and <v2>.
 */
Vector3 v3Cross(Vector3 v1, Vector3 v2);

#ifdef __cplusplus
}
#endif

#endif
