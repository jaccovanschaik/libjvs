/*
 * vector.c: calculations with vectors and matrices.
 *
 * vector.c is part of libjvs.
 *
 * Copyright:   (c) 2007-2021 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: vector.c 438 2021-08-19 10:10:03Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <math.h>

#include "defs.h"

#include "vector.h"

/*
 * Create a new vector with <n_rows> rows. The vector is initialized to all
 * zeroes.
 */
Vector *vNew(int n_rows)
{
   Vector *v = calloc(1, sizeof(Vector));

   v->n_rows = n_rows;

   v->row = calloc(n_rows, sizeof(double));

   return v;
}

/*
 * Delete (free) vector <v>.
 */
void vDel(Vector *v)
{
   free(v->row);

   free(v);
}

/*
 * Return the length of vector <v>.
 */
double vLen(Vector *v)
{
   int i;
   double sum = 0;

   for (i = 0; i < v->n_rows; i++) {
      sum += SQR(v->row[i]);
   }

   return sqrt(sum);
}

/*
 * Set vector <v> using the provided values. Be sure to pass in the correct
 * number of coordinates!
 */
void vSet(Vector *v, ...)
{
   int i;
   va_list ap;

   va_start(ap, v);

   for (i = 0; i < v->n_rows; i++) {
      v->row[i] = va_arg(ap, double);
   }

   va_end(ap);
}

/*
 * Set row <row> of vector <v> to <value>.
 */
void vSetRow(Vector *v, int row, double value)
{
   assert(row >= 0 && row < v->n_rows);

   v->row[row] = value;
}

/*
 * Return row <row> of vector <v>.
 */
double vGetRow(Vector *v, int row)
{
   assert(row >= 0 && row < v->n_rows);

   return v->row[row];
}

/*
 * Add vectors <v1> and <v2> and return the result in <v_out>.
 */
void vAdd(Vector *v_out, Vector *v1, Vector *v2)
{
   int i;

   assert(v_out->n_rows == v1->n_rows && v1->n_rows == v2->n_rows);

   for (i = 0; i < v1->n_rows; i++) {
      v_out->row[i] = v1->row[i] + v2->row[i];
   }
}

/*
 * Subtract vector <v2> from <v1> and return the result in <v_out>.
 */
void vSub(Vector *v_out, Vector *v1, Vector *v2)
{
   int i;

   assert(v_out->n_rows == v1->n_rows && v1->n_rows == v2->n_rows);

   for (i = 0; i < v1->n_rows; i++) {
      v_out->row[i] = v1->row[i] - v2->row[i];
   }
}

/*
 * Copy vector <v_in> to vector <v_out>. Both vectors must already have the
 * same number of rows.
 */
void vCopy(Vector *v_out, Vector *v_in)
{
   int i;

   assert(v_out->n_rows == v_in->n_rows);

   for (i = 0; i < v_in->n_rows; i++) {
      v_out->row[i] = v_in->row[i];
   }
}

/*
 * Duplicate <v_in> and return the result.
 */
Vector *vDup(Vector *v_in)
{
   Vector *v_out = vNew(v_in->n_rows);

   vCopy(v_out, v_in);

   return v_out;
}

/*
 * Scale vector <v_in> by <factor> and put the result in <v_out>.
 */
void vScale(Vector *v_out, Vector *v_in, double factor)
{
   int i;

   assert(v_out->n_rows == v_in->n_rows);

   for (i = 0; i < v_in->n_rows; i++) {
      v_out->row[i] = v_in->row[i] * factor;
   }
}

/*
 * Normalize vector <v_in> (scale it so its length becomes 1) and put the result
 * into <v_out>.
 */
void vNorm(Vector *v_out, Vector *v_in)
{
   double len = vLen(v_in);

   vScale(v_out, v_in, 1 / len);
}

/*
 * Return the dot-product of vectors <v1> and <v2>.
 */
double vDotP(Vector *v1, Vector *v2)
{
   int i;

   double sum = 0;

   assert(v1->n_rows == v2->n_rows);

   for (i = 0; i < v1->n_rows; i++) {
      sum += v1->row[i] * v2->row[i];
   }

   return sum;
}

/*
 * Calculate the cross-product of vectors <v1> and <v2> and put the result into
 * <v_out>. All three vectors must have 3 rows.
 */
void vCrossP(Vector *v_out, Vector *v1, Vector *v2)
{
   Vector *v_tmp;

   assert(v_out->n_rows == v1->n_rows &&
          v1->n_rows == v2->n_rows &&
          v2->n_rows == 3);

   v_tmp = vNew(v_out->n_rows);

   v_tmp->row[0] = v1->row[1] * v2->row[2] + v1->row[2] * v2->row[1];
   v_tmp->row[1] = v1->row[2] * v2->row[0] + v1->row[0] * v2->row[2];
   v_tmp->row[2] = v1->row[0] * v2->row[1] + v1->row[1] * v2->row[0];

   vCopy(v_out, v_tmp);

   vDel(v_tmp);
}
