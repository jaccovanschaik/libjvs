#ifndef LATLON_H
#define LATLON_H

/*
 * latlon.h: parse latitude/longitude strings, being as liberal as possible with
 *           the used format.
 *
 * Copyright: (c) 2024 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2024-02-17
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 *
 * vim: softtabstop=4 shiftwidth=4 expandtab textwidth=80
 */

#define latlon_parse(str, lat, lon) \
    _latlon_parse(__FILE__, __LINE__, str, lat, lon)

/*
 * Parse <str>, which contains a latitude and a longitude, and return those
 * through <lat> and <lon>. Returns 0 on success or 1 on failure.
 */
int _latlon_parse(const char *file, int line,
        const char *str, double *lat, double *lon);

#endif
