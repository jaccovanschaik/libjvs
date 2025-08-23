#include "latlon.c"

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
