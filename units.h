#ifndef LIBJVS_UNITS_H
#define LIBJVS_UNITS_H

/*
 * Useful unit conversions.
 *
 * Part of libjvs.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>		/* for M_PI */

/*
 * This is how this should be used. If you have something that isn't SI,
 * you should say what unit it is. So for example if you want to set a
 * variable to 2000 feet, you should say "alt = 2000 FEET". The alt
 * variable will then be set to the equivalent altitude in meters.
 *
 * Conversely, if you want to convert something back to a unit other than
 * SI, you should say "in what unit" you want to have that value. So if
 * you want to print the alt variable in feet you should say "printf(alt
 * IN_FEET)" and the equivalent altitude in feet will be printed.
*/
							/* to SI   */

#define MINUTES			* 60.0		        /* [sec]   */
#define HOURS			* 3600.0		/* [sec]   */
#define FEET			* 0.3048		/* [m]     */
#define FLIGHTLEVELS		* 30.48			/* [m]     */
#define MILES			* 1852.0		/* [m]     */
#define DEGREES_CELSIUS		+ 273.0			/* [K]     */
#define KNOTS			* 0.51444		/* [m/s]   */
#define TONS			* 1000.0		/* [kg]    */
#define PERCENT			* 0.01			/* [1]     */
#define DEGREES			* ( M_PI / 180 )	/* [rad]   */
#define FT_PER_MIN		FEET / 60.0		/* [m/s]   */
#define DEG_PER_MIN		DEGREES / 60.0		/* [rad/s] */
#define MBAR			* 100.0			/* [N/m2]  */

#define IN_MINUTES		/ 60.0
#define IN_HOURS		/ 3600.0
#define IN_FEET			/ 0.3048
#define IN_FLIGHTLEVELS		/ 30.48
#define IN_MILES		/ 1852.0
#define IN_DEGREES_CELSIUS	- 273.0
#define IN_KNOTS		/ 0.51444
#define IN_TONS			* 0.001
#define IN_PERCENT		* 100.0
#define IN_DEGREES		* 180 / M_PI
#define IN_FT_PER_MIN		* 60 / 0.3048
#define IN_DEG_PER_MIN		* 60 * 180 / M_PI
#define IN_MBAR			/ 100.0

#ifdef __cplusplus
}
#endif

#endif
