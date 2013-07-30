/*
 * Header file for vector.c
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifndef VECTOR_H
#define VECTOR_H

#include <stdio.h>

typedef struct {
   int n_rows;
   double *row;
} Vector;

typedef struct {
   int n_rows;
   int n_cols;
   Vector *col;
} Matrix;

/* Create a new vector with <n_rows> rows. The vector is initialized to all
 * zeroes. */
Vector *vNew(int n_rows);

/* Delete (free) vector <v>. */
void vDel(Vector *v);

/* Return the length of vector <v>. */
double vLen(Vector *v);

/* Set vector <v> using the provided values. Be sure to pass in the correct
 * number of coordinates! */
void vSet(Vector *v, ...);

/* Set row <row> of vector <v> to <value>. */
void vSetRow(Vector *v, int row, double value);

/* Return row <row> of vector <v>. */
double vGetRow(Vector *v, int row);

/* Add vectors <v1> and <v2> and return the result in <v_out>. */
void vAdd(Vector *v_out, Vector *v1, Vector *v2);

/* Subtract vector <v2> from <v1> and return the result in <v_out>. */
void vSub(Vector *v_out, Vector *v1, Vector *v2);

/* Copy vector <v_in> in to vector <v_out>. Both vectors must already have the
 * same number of rows. */
void vCopy(Vector *v_out, Vector *v_in);

/* Duplicate <v_in> and return the result. */
Vector *vDup(Vector *v_in);

/* Scale vector <v_in> by <factor> and put the result in <v_out>. */
void vScale(Vector *v_out, Vector *v_in, double factor);

/* Normalize vector <v_in> (scale it so its length becomes 1) and put the result
 * into <v_out>. */
void vNorm(Vector *v_out, Vector *v_in);

/* Return the dot-product of vectors <v1> and <v2>. */
double vDotP(Vector *v1, Vector *v2);

/* Calculate the cross-product of vectors <v1> and <v2> and put the result into
 * <v_out>. All three vectors must have 3 rows. */
void vCrossP(Vector *v_out, Vector *v1, Vector *v2);

#endif
