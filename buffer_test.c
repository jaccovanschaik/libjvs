#include "buffer.c"

static int errors = 0;

int main(void)
{
    Buffer buf1 = { 0 };
    Buffer buf2 = { 0 };
    Buffer *buf3;

    // ** bufRewind

    bufRewind(&buf1);

    make_sure_that(bufLen(&buf1) == 0);
    make_sure_that(bufIsEmpty(&buf1));

    // ** The bufAdd* family

    bufAdd(&buf1, "ABCDEF", 3);

    make_sure_that(bufLen(&buf1) == 3);
    make_sure_that(!bufIsEmpty(&buf1));
    make_sure_that(strcmp(bufGet(&buf1), "ABC") == 0);
    make_sure_that(bufGetC(&buf1, 0) == 'A');
    make_sure_that(bufGetC(&buf1, 1) == 'B');
    make_sure_that(bufGetC(&buf1, 2) == 'C');
    make_sure_that(bufGetC(&buf1, 3) == '\0');
    make_sure_that(bufGetC(&buf1, 4) == '\0');
    make_sure_that(bufGetC(&buf1, 5) == '\0');

    bufAddC(&buf1, 'D');

    make_sure_that(bufLen(&buf1) == 4);
    make_sure_that(strcmp(bufGet(&buf1), "ABCD") == 0);

    bufAddF(&buf1, "%d", 1234);

    make_sure_that(bufLen(&buf1) == 8);
    make_sure_that(strcmp(bufGet(&buf1), "ABCD1234") == 0);

    bufAddS(&buf1, "XYZ");

    make_sure_that(bufLen(&buf1) == 11);
    make_sure_that(strcmp(bufGet(&buf1), "ABCD1234XYZ") == 0);

    // ** Overflow the initial 16 allocated bytes.

    bufAddF(&buf1, "%s", "1234567890");

    make_sure_that(bufLen(&buf1) == 21);
    make_sure_that(strcmp(bufGet(&buf1), "ABCD1234XYZ1234567890") == 0);

    // ** The bufSet* family

    bufSet(&buf1, "ABCDEF", 3);

    make_sure_that(bufLen(&buf1) == 3);
    make_sure_that(strcmp(bufGet(&buf1), "ABC") == 0);

    bufSetC(&buf1, 'D');

    make_sure_that(bufLen(&buf1) == 1);
    make_sure_that(strcmp(bufGet(&buf1), "D") == 0);

    bufSetF(&buf1, "%d", 1234);

    make_sure_that(bufLen(&buf1) == 4);
    make_sure_that(strcmp(bufGet(&buf1), "1234") == 0);

    bufSetS(&buf1, "ABCDEF");

    make_sure_that(bufLen(&buf1) == 6);
    make_sure_that(strcmp(bufGet(&buf1), "ABCDEF") == 0);

    // ** bufRewind again

    bufRewind(&buf1);

    make_sure_that(bufLen(&buf1) == 0);
    make_sure_that(strcmp(bufGet(&buf1), "") == 0);

    // ** bufCat

    bufSet(&buf1, "ABC", 3);
    bufSet(&buf2, "DEF", 3);

    buf3 = bufCat(&buf1, &buf2);

    make_sure_that(&buf1 == buf3);

    make_sure_that(bufLen(&buf1) == 6);
    make_sure_that(strcmp(bufGet(&buf1), "ABCDEF") == 0);

    make_sure_that(bufLen(&buf2) == 3);
    make_sure_that(strcmp(bufGet(&buf2), "DEF") == 0);

    // ** bufFinish

    char *r;

    // Regular string
    buf3 = bufCreate("ABCDEF");
    make_sure_that(strcmp((r = bufFinish(buf3)), "ABCDEF") == 0);
    free(r);

    // Empty buffer (where buf->data == NULL)
    buf3 = bufCreate(NULL);
    make_sure_that(strcmp((r = bufFinish(buf3)), "") == 0);
    free(r);

    // Reset buffer (where buf->data != NULL)
    buf3 = bufCreate("ABCDEF");
    bufRewind(buf3);
    make_sure_that(strcmp((r = bufFinish(buf3)), "") == 0);
    free(r);

    // ** bufFinishN

    // Empty buffer (where buf->data == NULL)
    buf3 = bufCreate(NULL);
    make_sure_that(bufFinishN(buf3) == NULL);

    // Reset buffer (where buf->data != NULL)
    buf3 = bufCreate("ABCDEF");
    bufRewind(buf3);
    make_sure_that(bufFinishN(buf3) == NULL);

    // ** bufTrim

    bufSetF(&buf1, "ABCDEF");

    make_sure_that(strcmp(bufGet(bufTrim(&buf1, 0, 0)), "ABCDEF") == 0);
    make_sure_that(strcmp(bufGet(bufTrim(&buf1, 1, 0)), "BCDEF") == 0);
    make_sure_that(strcmp(bufGet(bufTrim(&buf1, 0, 1)), "BCDE") == 0);
    make_sure_that(strcmp(bufGet(bufTrim(&buf1, 1, 1)), "CD") == 0);
    make_sure_that(strcmp(bufGet(bufTrim(&buf1, 3, 3)), "") == 0);

    // ** bufPack

    bufPack(&buf1,
           PACK_INT8,   0x01,
           PACK_INT16,  0x0123,
           PACK_INT32,  0x01234567,
           PACK_INT64,  0x0123456789ABCDEF,
           PACK_FLOAT,  0.0,
           PACK_DOUBLE, 0.0,
           PACK_STRING, "Hoi1",
           PACK_DATA,   "Hoi2", 4,
           PACK_RAW,    "Hoi3", 4,
           END);

    make_sure_that(bufLen(&buf1) == 47);
    make_sure_that(memcmp(bufGet(&buf1),
        "\x01"
        "\x01\x23"
        "\x01\x23\x45\x67"
        "\x01\x23\x45\x67\x89\xAB\xCD\xEF"
        "\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x04Hoi1"
        "\x00\x00\x00\x04Hoi2"
        "Hoi3", 47) == 0);

    // ** bufUnpack

    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    float f32;
    double f64;
    char *string, *data, raw[4];
    int data_size;

    bufUnpack(&buf1,
            PACK_INT8,   &u8,
            PACK_INT16,  &u16,
            PACK_INT32,  &u32,
            PACK_INT64,  &u64,
            PACK_FLOAT,  &f32,
            PACK_DOUBLE, &f64,
            PACK_STRING, &string,
            PACK_DATA,   &data, &data_size,
            PACK_RAW,    &raw, sizeof(raw),
            END);

    make_sure_that(u8  == 0x01);
    make_sure_that(u16 == 0x0123);
    make_sure_that(u32 == 0x01234567);
    make_sure_that(u64 == 0x0123456789ABCDEF);
    make_sure_that(f32 == 0.0);
    make_sure_that(f64 == 0.0);
    make_sure_that(memcmp(string, "Hoi1", 5) == 0);
    make_sure_that(memcmp(data, "Hoi2", 4) == 0);
    make_sure_that(data_size == 4);
    make_sure_that(memcmp(raw, "Hoi3", 4) == 0);

    free(string);
    free(data);

    bufUnpack(&buf1,
            PACK_INT8,   NULL,
            PACK_INT16,  NULL,
            PACK_INT32,  NULL,
            PACK_INT64,  NULL,
            PACK_FLOAT,  NULL,
            PACK_DOUBLE, NULL,
            PACK_STRING, NULL,
            PACK_DATA,   NULL, NULL,
            PACK_RAW,    NULL, 4,
            END);

    // ** bufList

    const char *name[] = { "Mills", "Berry", "Buck", "Stipe" };

    bufRewind(&buf1);

    bufList(&buf1, ", ", " and ", TRUE, TRUE, "%s", name[0]);

    make_sure_that(strcmp(bufGet(&buf1), "Mills") == 0);

    bufRewind(&buf1);

    bufList(&buf1, ", ", " and ", TRUE, FALSE, "%s", name[0]);
    bufList(&buf1, ", ", " and ", FALSE, TRUE, "%s", name[1]);

    make_sure_that(strcmp(bufGet(&buf1), "Mills and Berry") == 0);

    bufRewind(&buf1);

    bufList(&buf1, ", ", " and ", TRUE,  FALSE, "%s", name[0]);
    bufList(&buf1, ", ", " and ", FALSE, FALSE, "%s", name[1]);
    bufList(&buf1, ", ", " and ", FALSE, TRUE,  "%s", name[2]);

    make_sure_that(strcmp(bufGet(&buf1), "Mills, Berry and Buck") == 0);

    bufRewind(&buf1);

    bufList(&buf1, ", ", " and ", TRUE,  FALSE, "%s", name[0]);
    bufList(&buf1, ", ", " and ", FALSE, FALSE, "%s", name[1]);
    bufList(&buf1, ", ", " and ", FALSE, FALSE, "%s", name[2]);
    bufList(&buf1, ", ", " and ", FALSE, TRUE,  "%s", name[3]);

    make_sure_that(strcmp(bufGet(&buf1), "Mills, Berry, Buck and Stipe") == 0);

    bufClear(&buf1);
    bufClear(&buf2);

    // ** bufStartsWith, bufEndsWith

    bufSetS(&buf1, "abcdef");

    make_sure_that(bufStartsWith(&buf1, "abc") == true);
    make_sure_that(bufStartsWith(&buf1, "def") == false);
    make_sure_that(bufEndsWith(&buf1, "def") == true);
    make_sure_that(bufEndsWith(&buf1, "abc") == false);

    make_sure_that(bufStartsWith(&buf1, "%s", "abc") == true);
    make_sure_that(bufStartsWith(&buf1, "%s", "def") == false);
    make_sure_that(bufEndsWith(&buf1, "%s", "def") == true);
    make_sure_that(bufEndsWith(&buf1, "%s", "abc") == false);

    bufClear(&buf1);

    bufSetS(&buf1, "123456789");

    make_sure_that(bufStartsWith(&buf1, "123") == true);
    make_sure_that(bufStartsWith(&buf1, "789") == false);
    make_sure_that(bufEndsWith(&buf1, "789") == true);
    make_sure_that(bufEndsWith(&buf1, "123") == false);

    make_sure_that(bufStartsWith(&buf1, "%d", 123) == true);
    make_sure_that(bufStartsWith(&buf1, "%d", 789) == false);
    make_sure_that(bufEndsWith(&buf1, "%d", 789) == true);
    make_sure_that(bufEndsWith(&buf1, "%d", 123) == false);

    bufClear(&buf1);

    return errors;
}
