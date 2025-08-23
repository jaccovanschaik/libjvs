#include "hash.c"

#include <stdio.h>

static int errors = 0;

int main(void)
{
    HashTable *table = hashCreateTable();

    struct Data {
        int i;
        char *s;
    };

    struct Data data[] = {
        { 0, "zero" },
        { 1, "one" },
        { 2, "two" },
        { 3, "three" },
        { 4, "four" },
    };

    int i, count = sizeof(data) / sizeof(data[0]);

    for (i = 0; i < count; i++) {
        hashAdd(table, data + i, HASH_VALUE(data[i].i));
        hashAdd(table, data + i, HASH_STRING(data[i].s));
    }

    make_sure_that(hashGet(table, HASH_VALUE(data[0].i)) == &data[0]);
    make_sure_that(hashGet(table, HASH_VALUE(data[1].i)) == &data[1]);
    make_sure_that(hashGet(table, HASH_VALUE(data[2].i)) == &data[2]);
    make_sure_that(hashGet(table, HASH_VALUE(data[3].i)) == &data[3]);
    make_sure_that(hashGet(table, HASH_VALUE(data[4].i)) == &data[4]);

    make_sure_that(hashGet(table, HASH_STRING("zero"))  == &data[0]);
    make_sure_that(hashGet(table, HASH_STRING("one"))   == &data[1]);
    make_sure_that(hashGet(table, HASH_STRING("two"))   == &data[2]);
    make_sure_that(hashGet(table, HASH_STRING("three")) == &data[3]);
    make_sure_that(hashGet(table, HASH_STRING("four"))  == &data[4]);

    make_sure_that(hashContains(table, HASH_VALUE(data[0].i)));
    make_sure_that(hashContains(table, HASH_VALUE(data[1].i)));
    make_sure_that(hashContains(table, HASH_VALUE(data[2].i)));
    make_sure_that(hashContains(table, HASH_VALUE(data[3].i)));
    make_sure_that(hashContains(table, HASH_VALUE(data[4].i)));

    make_sure_that(hashContains(table, HASH_STRING("zero")));
    make_sure_that(hashContains(table, HASH_STRING("one")));
    make_sure_that(hashContains(table, HASH_STRING("two")));
    make_sure_that(hashContains(table, HASH_STRING("three")));
    make_sure_that(hashContains(table, HASH_STRING("four")));

    for (i = 0; i < count; i++) {
        hashDrop(table, HASH_VALUE(i));
    }

    make_sure_that(hashGet(table, HASH_VALUE(data[0].i)) == NULL);
    make_sure_that(hashGet(table, HASH_VALUE(data[1].i)) == NULL);
    make_sure_that(hashGet(table, HASH_VALUE(data[2].i)) == NULL);
    make_sure_that(hashGet(table, HASH_VALUE(data[3].i)) == NULL);
    make_sure_that(hashGet(table, HASH_VALUE(data[4].i)) == NULL);

    make_sure_that(hashGet(table, HASH_STRING("zero"))  == &data[0]);
    make_sure_that(hashGet(table, HASH_STRING("one"))   == &data[1]);
    make_sure_that(hashGet(table, HASH_STRING("two"))   == &data[2]);
    make_sure_that(hashGet(table, HASH_STRING("three")) == &data[3]);
    make_sure_that(hashGet(table, HASH_STRING("four"))  == &data[4]);

    make_sure_that(hashContains(table, HASH_VALUE(data[0].i)) == FALSE);
    make_sure_that(hashContains(table, HASH_VALUE(data[1].i)) == FALSE);
    make_sure_that(hashContains(table, HASH_VALUE(data[2].i)) == FALSE);
    make_sure_that(hashContains(table, HASH_VALUE(data[3].i)) == FALSE);
    make_sure_that(hashContains(table, HASH_VALUE(data[4].i)) == FALSE);

    make_sure_that(hashContains(table, HASH_STRING("zero")));
    make_sure_that(hashContains(table, HASH_STRING("one")));
    make_sure_that(hashContains(table, HASH_STRING("two")));
    make_sure_that(hashContains(table, HASH_STRING("three")));
    make_sure_that(hashContains(table, HASH_STRING("four")));

    for (i = 0; i < count; i++) {
        hashDrop(table, HASH_STRING(data[i].s));
    }

    make_sure_that(hashGet(table, HASH_VALUE(data[0].i)) == NULL);
    make_sure_that(hashGet(table, HASH_VALUE(data[1].i)) == NULL);
    make_sure_that(hashGet(table, HASH_VALUE(data[2].i)) == NULL);
    make_sure_that(hashGet(table, HASH_VALUE(data[3].i)) == NULL);
    make_sure_that(hashGet(table, HASH_VALUE(data[4].i)) == NULL);

    make_sure_that(hashGet(table, HASH_STRING("zero"))  == NULL);
    make_sure_that(hashGet(table, HASH_STRING("one"))   == NULL);
    make_sure_that(hashGet(table, HASH_STRING("two"))   == NULL);
    make_sure_that(hashGet(table, HASH_STRING("three")) == NULL);
    make_sure_that(hashGet(table, HASH_STRING("four"))  == NULL);

    make_sure_that(hashContains(table, HASH_VALUE(data[0].i)) == FALSE);
    make_sure_that(hashContains(table, HASH_VALUE(data[1].i)) == FALSE);
    make_sure_that(hashContains(table, HASH_VALUE(data[2].i)) == FALSE);
    make_sure_that(hashContains(table, HASH_VALUE(data[3].i)) == FALSE);
    make_sure_that(hashContains(table, HASH_VALUE(data[4].i)) == FALSE);

    make_sure_that(hashContains(table, HASH_STRING("zero"))  == FALSE);
    make_sure_that(hashContains(table, HASH_STRING("one"))   == FALSE);
    make_sure_that(hashContains(table, HASH_STRING("two"))   == FALSE);
    make_sure_that(hashContains(table, HASH_STRING("three")) == FALSE);
    make_sure_that(hashContains(table, HASH_STRING("four"))  == FALSE);

    hashDestroy(table);

    return errors;
}
