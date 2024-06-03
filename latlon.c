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

#include "latlon.h"
#include "latlon_fields.h"
#include "debug.h"

#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <ctype.h>

typedef struct {
    double min, max, err;
} Limit;

static Limit *limits[LLF_COUNT] = {
    [LAT_DEG] = &((Limit) { .min =  -90, .max =  90 } ),
    [LAT_MIN] = &((Limit) { .min =    0, .err =  60 } ),
    [LAT_SEC] = &((Limit) { .min =    0, .err =  60 } ),
    [LON_DEG] = &((Limit) { .min = -180, .max = 180 } ),
    [LON_MIN] = &((Limit) { .min =    0, .err =  60 } ),
    [LON_SEC] = &((Limit) { .min =    0, .err =  60 } ),
};

typedef struct {
    char   *re_str;
    int     match[LLF_COUNT];
    regex_t re;
} Regex;

Regex regexes[] = {
    {   // DDMMSS.ss, DDDMMSS.ss
        "([+-]?)([0-9]{2})([0-9]{2})([0-9]{2}\\.?[0-9]*)([NS]?)"
        "[, \t]+"
        "([+-]?)([0-9]{3})([0-9]{2})([0-9]{2}\\.?[0-9]*)([EW]?)",
        {
          [LAT_SIGN] = 1,
          [LAT_DEG] = 2, [LAT_MIN] = 3, [LAT_SEC] = 4, [LAT_HEMI] = 5,
          [LON_SIGN] = 6,
          [LON_DEG] = 7, [LON_MIN] = 8, [LON_SEC] = 9, [LON_HEMI] = 10
        },
        { 0 },
    },
    {   // DDMM.mm, DDDMM.mm
        "([+-]?)([0-9]{2})([0-9]{2}\\.?[0-9]*)([NS]?)"
        "[, \t]+"
        "([+-]?)([0-9]{3})([0-9]{2}\\.?[0-9]*)([EW]?)",
        {
          [LAT_SIGN] = 1, [LAT_DEG] = 2, [LAT_MIN] = 3, [LAT_HEMI] = 4,
          [LON_SIGN] = 5, [LON_DEG] = 6, [LON_MIN] = 7, [LON_HEMI] = 8
        },
        { 0 },
    },
    {   // DD.dd, DDD.dd
        "([+-]?)([0-9]+\\.?[0-9]*)([NS]?)"
        "[, \t]+"
        "([+-]?)([0-9]+\\.?[0-9]*)([EW]?)",
        {
          [LAT_SIGN] = 1, [LAT_DEG] = 2, [LAT_HEMI] = 3,
          [LON_SIGN] = 4, [LON_DEG] = 5, [LON_HEMI] = 6
        },
        { 0 },
    },
    {   // DD°MM'SS.ss", DDD°MM'SS.ss"
        "([+-]?)([0-9]{1,2})°([0-9]{2})'([0-9]{2}\\.?[0-9]*)\"([NS]?)"
        "[, \t]+"
        "([+-]?)([0-9]{1,3})°([0-9]{2})'([0-9]{2}\\.?[0-9]*)\"([EW]?)",
        {
          [LAT_SIGN] = 1,
          [LAT_DEG] = 2, [LAT_MIN] = 3, [LAT_SEC] = 4, [LAT_HEMI] = 5,
          [LON_SIGN] = 6,
          [LON_DEG] = 7, [LON_MIN] = 8, [LON_SEC] = 9, [LON_HEMI] = 10
        },
        { 0 },
    },
};

static const int regex_count = sizeof(regexes) / sizeof(regexes[0]);

static bool debug = true;
static bool initialized = false;

#define NMATCH (LLF_COUNT + 1)

static void va_report_error(FILE *fp, const char *fmt, va_list ap)
{
    if (!debug) return;

    vfprintf(fp, fmt, ap);
}

static void report_error(FILE *fp, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    va_report_error(fp, fmt, ap);
    va_end(ap);
}

static int parse_float(const char *file, int line,
        int re, char *const parts[], int field, double *val)
{
    double ret;

    if (parts[field] == NULL) return 0;

    if (sscanf(parts[field], "%lf", &ret) != 1) {
        report_error(stderr,
                "%s:%d: internal error: regex %d found \"%s\" as %s "
                "but I couldn't parse it.\n", file, line,
                re, parts[field], latlon_string(field));

        return 1;
    }
    else if (limits[field] == NULL) {
        *val = ret;

        return 0;
    }
    else if (ret < limits[field]->min) {
        report_error(stderr, "%s:%d: %s can't be below %lg; got %lg.\n",
                file, line, latlon_string(field), limits[field]->min, ret);

        return 1;
    }
    else if (limits[field]->max > 0 && ret > limits[field]->max) {
        report_error(stderr, "%s:%d: %s can't be above %lg; got %lg.\n",
                file, line, latlon_string(field), limits[field]->max, ret);

        return 1;
    }
    else if (limits[field]->err > 0 && ret >= limits[field]->err) {
        report_error(stderr, "%s:%d: %s must be below %lg; got %lg.\n",
                file, line, latlon_string(field), limits[field]->err, ret);

        return 1;
    }
    else {
        *val = ret;

        return 0;
    }
}

static int parse_input(const char *file, int line,
        const char *text, double *lat, double *lon)
{
    regmatch_t pmatch[NMATCH];
    int re;

    for (re = 0; re < regex_count; re++) {
        if (regexec(&regexes[re].re, text, NMATCH, pmatch, 0) == 0) {
            break;
        }
    }

    if (re == regex_count) {
        report_error(stderr,
                "%s:%d: string did not match any regular expressions.\n",
                file, line);
        return 1;
    }

    char *parts[LLF_COUNT] = { 0 };

    for (int i = 0; i < LLF_COUNT; i++) {
        int match = regexes[re].match[i];

        if (match > 0) {
            parts[i] = strndup(text + pmatch[match].rm_so,
                              pmatch[match].rm_eo - pmatch[match].rm_so);
        }
    }

    double lat_deg = 0, lat_min = 0, lat_sec = 0;
    double lon_deg = 0, lon_min = 0, lon_sec = 0;

    if (parse_float(file, line, re, parts, LAT_DEG, &lat_deg) != 0 ||
        parse_float(file, line, re, parts, LAT_MIN, &lat_min) != 0 ||
        parse_float(file, line, re, parts, LAT_SEC, &lat_sec) != 0 ||
        parse_float(file, line, re, parts, LON_DEG, &lon_deg) != 0 ||
        parse_float(file, line, re, parts, LON_MIN, &lon_min) != 0 ||
        parse_float(file, line, re, parts, LON_SEC, &lon_sec) != 0)
    {
        return 1;
    }

    double lat_val = lat_deg + lat_min / 60 + lat_sec / 3600;
    double lon_val = lon_deg + lon_min / 60 + lon_sec / 3600;

    if (parts[LAT_SIGN] != NULL && parts[LAT_SIGN][0] != '\0') {
        if (parts[LAT_SIGN][0] == '-') {
            lat_val = -lat_val;
        }
        else if (parts[LAT_SIGN][0] != '+') {
            report_error(stderr,
                    "Internal error: didn't recognize \"%s\" as a %s\n",
                    parts[LAT_SIGN], latlon_string(LAT_SIGN));
            return 1;
        }
    }

    if (parts[LON_SIGN] != NULL && parts[LON_SIGN][0] != '\0') {
        if (parts[LON_SIGN][0] == '-') {
            lon_val = -lon_val;
        }
        else if (parts[LON_SIGN][0] != '+') {
            report_error(stderr,
                    "Internal error: didn't recognize \"%s\" as a %s\n",
                    parts[LON_SIGN], latlon_string(LON_SIGN));
            return 1;
        }
    }

    if (parts[LAT_HEMI] != NULL && parts[LAT_HEMI][0] != '\0') {
        if (toupper(parts[LAT_HEMI][0]) == 'S') {
            lat_val = -lat_val;
        }
        else if (toupper(parts[LAT_HEMI][0]) != 'N') {
            report_error(stderr,
                    "Internal error: didn't recognize \"%s\" as a %s\n",
                    parts[LAT_HEMI], latlon_string(LAT_HEMI));
            return 1;
        }
    }

    if (parts[LON_HEMI] != NULL && parts[LON_HEMI][0] != '\0') {
        if (toupper(parts[LON_HEMI][0]) == 'W') {
            lon_val = -lon_val;
        }
        else if (toupper(parts[LON_HEMI][0]) != 'E') {
            report_error(stderr,
                    "Internal error: didn't recognize \"%s\" as a %s\n",
                    parts[LON_HEMI], latlon_string(LON_HEMI));
            return 1;
        }
    }

    *lat = lat_val;
    *lon = lon_val;

    return 0;
}

/*
 * Parse <str>, which contains a latitude and a longitude, and return those
 * through <lat> and <lon>. Returns 0 on success or 1 on failure.
 */
int _latlon_parse(const char *file, int line,
        const char *str, double *lat, double *lon)
{
    if (!initialized) {
        for (int i = 0; i < regex_count; i++) {
            if (regcomp(&regexes[i].re, regexes[i].re_str, REG_EXTENDED) != 0) {
                report_error(stderr, "%s:%d: regcomp failed for RE:\n\"%s\"\n",
                        file, line, regexes[i].re_str);
                return 1;
            }
        }

        initialized = true;
    }

    return parse_input(file, line, str, lat, lon);
}

#ifdef TEST
#include "utils.h"

static bool equal(int digits, double a, double b)
{
    char *as, *bs;

    if (asprintf(&as, "%.*f", digits, a) == -1 ||
        asprintf(&bs, "%.*f", digits, b) == -1) abort();

    bool result = (strcmp(as, bs) == 0);

    free(as);
    free(bs);

    return result;
}

static int errors = 0;

int main(void)
{
    double lat, lon;

    struct {
        const char *str;
        int ret;
        double lat, lon;
    } cases[] = {
        {  "520123.45,   0040123.45",  0,  52.023181,  4.023181 },
        {  "520123.45N,  0040123.45E", 0,  52.023181,  4.023181 },
        { "-520123.45,  -0040123.45",  0, -52.023181, -4.023181 },
        {  "520123.45S,  0040123.45W", 0, -52.023181, -4.023181 },
        { "-520123.45S, -0040123.45W", 0,  52.023181,  4.023181 },

        {  "5201.2345,   00401.2345",  0,  52.020575,  4.020575 },
        {  "5201.2345N,  00401.2345E", 0,  52.020575,  4.020575 },
        { "-5201.2345,  -00401.2345",  0, -52.020575, -4.020575 },
        {  "5201.2345S,  00401.2345W", 0, -52.020575, -4.020575 },
        { "-5201.2345S, -00401.2345W", 0,  52.020575,  4.020575 },

        {  "52.012345,   004.012345",  0,  52.012345,  4.012345 },
        {  "52.012345N,  004.012345E", 0,  52.012345,  4.012345 },
        { "-52.012345,  -004.012345",  0, -52.012345, -4.012345 },
        {  "52.012345S,  004.012345W", 0, -52.012345, -4.012345 },
        { "-52.012345S, -004.012345W", 0,  52.012345,  4.012345 },

        { "52°01'23.45\", 004°01'23.45\"",   0,  52.023181,  4.023181 },
        { "52°01'23.45\"N, 004°01'23.45\"E", 0,  52.023181,  4.023181 },
        { "52°01'23.45\"S, 004°01'23.45\"W", 0, -52.023181, -4.023181 },
    };

    int case_count = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < case_count; i++) {
        int r;

        // hexdump(stderr, cases[i].str, strlen(cases[i].str));

        if ((r = latlon_parse(cases[i].str, &lat, &lon)) != cases[i].ret) {
            dbgPrint(stderr,
                    "Case %d: latlon_parse returned %d instead of %d.\n",
                    i, r, cases[i].ret);
            errors++;
        }
        else if (!equal(6, lat, cases[i].lat)) {
            dbgPrint(stderr,
                    "Case %d: latitude %g differs from expected value %g\n",
                    i, lat, cases[i].lat);
            errors++;
        }
        else if (!equal(6, lon, cases[i].lon)) {
            dbgPrint(stderr,
                    "Case %d: longitude %g differs from expected value %g\n",
                    i, lon, cases[i].lon);
            errors++;
        }
    }

    return errors;
}

#endif
