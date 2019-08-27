#ifndef VECTOR2_H
#define VECTOR2_H

/*
 * vector2.h: Handles 2d vectors.
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-08-27
 * Version:   $Id: vector2.h 344 2019-08-27 19:30:24Z jacco $
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

#ifdef __cplusplus
}
#endif

#endif
