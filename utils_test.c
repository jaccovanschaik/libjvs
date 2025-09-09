#include "utils.c"
int errors = 0;

int main(void)
{
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    double f64;
    float f32;
    char *sp, *dp, *buf_p;
    char raw_buf[6] = { 0 };
    int len;

    char buffer[64] = { 0 };

    unsigned char expected[] = {
        1,
        0, 2,
        0, 0, 0, 3,
        0, 0, 0, 0, 0, 0, 0, 4,
        0x3F, 0x80, 0x00, 0x00,
        0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0, 0, 0, 3, 'H', 'o', 'i',
        0, 0, 0, 5, 'H', 'e', 'l', 'l', 'o',
        'W', 'o', 'r', 'l', 'd'
    };

    int r = strpack(buffer, sizeof(buffer),
            PACK_INT8,      1,
            PACK_INT16,     2,
            PACK_INT32,     3,
            PACK_INT64,     4LL,
            PACK_FLOAT,     1.0,
            PACK_DOUBLE,    2.0,
            PACK_STRING,    "Hoi",
            PACK_DATA,      "Hello", 5,
            PACK_RAW,       "World xxx", 5,
            END);

    make_sure_that(r == 48);

    make_sure_that(memcmp(buffer, expected, 48) == 0);

    r = astrpack(&buf_p,
            PACK_INT8,      1,
            PACK_INT16,     2,
            PACK_INT32,     3,
            PACK_INT64,     4LL,
            PACK_FLOAT,     1.0,
            PACK_DOUBLE,    2.0,
            PACK_STRING,    "Hoi",
            PACK_DATA,      "Hello", 5,
            PACK_RAW,       "World xxx", 5,
            END);

    make_sure_that(r == 48);

    make_sure_that(memcmp(buf_p, expected, 48) == 0);

    r = strunpack(buffer, sizeof(buffer),
            PACK_INT8,      &u8,
            PACK_INT16,     &u16,
            PACK_INT32,     &u32,
            PACK_INT64,     &u64,
            PACK_FLOAT,     &f32,
            PACK_DOUBLE,    &f64,
            PACK_STRING,    &sp,
            PACK_DATA,      &dp, &len,
            PACK_RAW,       raw_buf, 5,
            END);

    make_sure_that(u8  == 1);
    make_sure_that(u16 == 2);
    make_sure_that(u32 == 3);
    make_sure_that(u64 == 4);
    make_sure_that(f32 == 1.0);
    make_sure_that(f64 == 2.0);
    make_sure_that(strcmp(sp, "Hoi") == 0);
    make_sure_that(len == 5);
    make_sure_that(memcmp(dp, "Hello", 5) == 0);
    make_sure_that(memcmp(raw_buf, "World", 5) == 0);
    make_sure_that(r == 48);

    r = strunpack(buffer, sizeof(buffer),
            PACK_INT8,      NULL,
            PACK_INT16,     NULL,
            PACK_INT32,     NULL,
            PACK_INT64,     NULL,
            PACK_FLOAT,     NULL,
            PACK_DOUBLE,    NULL,
            PACK_STRING,    NULL,
            PACK_DATA,      NULL, NULL,
            PACK_RAW,       NULL, 5,
            END);

    make_sure_that(r == 48);

    r = ihexstr(&sp, 1, "0123456789ABCD\n\r", 16);

    make_sure_that(r == 75);

    make_sure_that(strncmp(sp,
                "  000000"
                "  30 31 32 33 34 35 36 37 38 39 41 42 43 44 0A 0D"
                " 0123456789ABCD..\n", r) == 0);

    setenv("TEST_String_1234", "test result", 1);

    char *result = env_expand("Testing env_expand: <$TEST_String_1234>");

    make_sure_that(strcmp(result, "Testing env_expand: <test result>") == 0);

    free(result);

    int32_t sec  = 43200;       // 12:00:00.987654321 UTC, 1970-01-01
    int32_t nsec = 987654321;

    check_string(
            t_format_c(sec, nsec, "UTC", "%Y-%m-%d/%H:%M:%S"),
            "1970-01-01/12:00:00");

    check_string(
            t_format_c(sec, nsec, "UTC", "%Y-%m-%d/%H:%M:%0S"),
            "1970-01-01/12:00:01");

    check_string(
            t_format_c(sec, nsec, "UTC", "%Y-%m-%d/%H:%M:%1S"),
            "1970-01-01/12:00:01.0");

    check_string(
            t_format_c(sec, nsec, "UTC", "%Y-%m-%d/%H:%M:%2S"),
            "1970-01-01/12:00:00.99");

    check_string(
            t_format_c(sec, nsec, "Europe/Amsterdam", "%Y-%m-%d/%H:%M:%4S"),
            "1970-01-01/13:00:00.9877");

    check_string(
            t_format_c(sec, nsec, "America/New_York", "%Y-%m-%d/%H:%M:%5S"),
            "1970-01-01/07:00:00.98765");

    check_string(
            t_format_c(sec, nsec, "Asia/Shanghai", "%Y-%m-%d/%H:%M:%9S"),
            "1970-01-01/20:00:00.987654321");

    char *door_win = "T\xFCr";
    char *door_utf = "T\xC3\xBCr";
    iconv_t cd = iconv_open("UTF-8", "WINDOWS-1252");

    size_t out_len;
    const char *out = convert_charset(cd, door_win, strlen(door_win), &out_len);

    make_sure_that(out_len == 4);
    make_sure_that(memcmp(out, door_utf, strlen(door_utf)) == 0);

    make_sure_that(utf8_strlen("Hällø!") == 6);

    make_sure_that(utf8_field_width("Hällø!", 8) == 10);

    return errors;
}
