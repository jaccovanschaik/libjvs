#include "list.c"

#include "utils.h"

#include <stdarg.h>

typedef struct {
    ListNode _node;
    int i;
} Data;

#define TEST_PTR(expr, exp) { \
    void *test = expr; \
    if (test != exp) { \
        fprintf(stderr, \
                "%s:%d: " #expr " returns %p, should be %p (%s).\n", \
                __FILE__, __LINE__, (void *) test, (void *) exp, #exp); \
        errors++; \
    } \
}

#define TEST_INT(expr, exp) { \
    int test = expr; \
    if (test != exp) { \
        fprintf(stderr, \
                "%s:%d: " #expr " returns %d, should be %d.\n", \
                __FILE__, __LINE__, test, exp); \
        errors++; \
    } \
}

static int cmp(const void *a, const void *b)
{
    const Data *A = a;
    const Data *B = b;

    return(A->i - B->i);
}

static int test_list(List *list, ...)
{
    int errors = 0;
    Data *req = NULL, *act = listHead(list);
    va_list ap;

    va_start(ap, list);

    act = listHead(list);
    req = va_arg(ap, Data *);

    while (req != NULL && act != NULL) {
        if (req != act) errors++;

        act = listNext(act);
        req = va_arg(ap, Data *);
    }

    while (act != NULL) {
        errors++;
        act = listNext(act);
    }

    while (req != NULL) {
        errors++;
        req = va_arg(ap, Data *);
    }

    va_end(ap);

    return errors;
}

int main(void)
{
    List list;
    Data *data[6];

    int i, errors = 0;

    for (i = 0; i < 6; i++) {
        data[i] = calloc(1, sizeof(Data));
    }

    listInitialize(&list);

    TEST_INT(listLength(&list), 0);
    TEST_INT(listIsEmpty(&list),  TRUE);

    listAppendTail(&list, data[0]);
    listAppendTail(&list, data[1]);
    listAppendTail(&list, data[2]);
    listAppendTail(&list, data[3]);

    TEST_INT(listLength(&list), 4);
    TEST_INT(listIsEmpty(&list),  FALSE);

    TEST_PTR(listHead(&list), data[0]);
    TEST_PTR(listTail(&list), data[3]);

    TEST_PTR(listNext(data[0]), data[1]);
    TEST_PTR(listNext(data[1]), data[2]);
    TEST_PTR(listNext(data[2]), data[3]);
    TEST_PTR(listNext(data[3]), NULL);

    TEST_PTR(listPrev(data[3]), data[2]);
    TEST_PTR(listPrev(data[2]), data[1]);
    TEST_PTR(listPrev(data[1]), data[0]);
    TEST_PTR(listPrev(data[0]), NULL);

    TEST_PTR(listContaining(data[0]), &list);
    TEST_PTR(listContaining(data[1]), &list);
    TEST_PTR(listContaining(data[2]), &list);
    TEST_PTR(listContaining(data[3]), &list);

    listRemove(&list, data[0]);
    listRemove(&list, data[1]);
    listRemove(&list, data[2]);
    listRemove(&list, data[3]);

    TEST_INT(listLength(&list), 0);
    TEST_INT(listIsEmpty(&list),  TRUE);

    TEST_PTR(listContaining(data[0]), NULL);
    TEST_PTR(listContaining(data[1]), NULL);
    TEST_PTR(listContaining(data[2]), NULL);
    TEST_PTR(listContaining(data[3]), NULL);

    listInsertHead(&list, data[3]);
    listInsertHead(&list, data[2]);
    listInsertHead(&list, data[1]);
    listInsertHead(&list, data[0]);

    TEST_INT(listLength(&list), 4);
    TEST_INT(listIsEmpty(&list),  FALSE);

    TEST_PTR(listHead(&list), data[0]);
    TEST_PTR(listTail(&list), data[3]);

    TEST_PTR(listNext(data[0]), data[1]);
    TEST_PTR(listNext(data[1]), data[2]);
    TEST_PTR(listNext(data[2]), data[3]);
    TEST_PTR(listNext(data[3]), NULL);

    TEST_PTR(listPrev(data[3]), data[2]);
    TEST_PTR(listPrev(data[2]), data[1]);
    TEST_PTR(listPrev(data[1]), data[0]);
    TEST_PTR(listPrev(data[0]), NULL);

    TEST_PTR(listRemoveHead(&list), data[0]);
    TEST_PTR(listRemoveHead(&list), data[1]);
    TEST_PTR(listRemoveTail(&list), data[3]);
    TEST_PTR(listRemoveTail(&list), data[2]);

    TEST_INT(listLength(&list), 0);
    TEST_INT(listIsEmpty(&list),  TRUE);

    listAppendTail(&list, data[0]);
    listAppendTail(&list, data[3]);
    listAppend(&list, data[1], data[0]);
    listInsert(&list, data[2], data[3]);

    TEST_PTR(listRemoveHead(&list), data[0]);
    TEST_PTR(listRemoveHead(&list), data[1]);
    TEST_PTR(listRemoveTail(&list), data[3]);
    TEST_PTR(listRemoveTail(&list), data[2]);

    data[0]->i = 3;
    data[1]->i = 4;
    data[2]->i = 5;
    data[3]->i = 1;
    data[4]->i = 2;
    data[5]->i = 3;

    listAppendTail(&list, data[0]);
    listAppendTail(&list, data[1]);
    listAppendTail(&list, data[2]);
    listAppendTail(&list, data[3]);
    listAppendTail(&list, data[4]);
    listAppendTail(&list, data[5]);

    listSort(&list, cmp);

    // We want this sort to be stable, so data[0] should end up *before*
    // data[5].

    TEST_PTR(listRemoveHead(&list), data[3]);
    TEST_PTR(listRemoveHead(&list), data[4]);
    TEST_PTR(listRemoveHead(&list), data[0]);
    TEST_PTR(listRemoveHead(&list), data[5]);
    TEST_PTR(listRemoveHead(&list), data[1]);
    TEST_PTR(listRemoveHead(&list), data[2]);

    data[0]->i = 0;
    data[1]->i = 2;
    data[2]->i = 4;

    data[3]->i = 3;
    data[4]->i = 3;
    data[5]->i = 3;

    listInsertOrdered(&list, data[1], cmp);
    make_sure_that(test_list(&list, data[1], NULL) == 0);

    listInsertOrdered(&list, data[0], cmp);
    make_sure_that(test_list(&list, data[0], data[1], NULL) == 0);

    listInsertOrdered(&list, data[2], cmp);
    make_sure_that(test_list(&list, data[0], data[1], data[2], NULL) == 0);

    listInsertOrdered(&list, data[3], cmp);
    make_sure_that(test_list(&list,
            data[0], data[1], data[3], data[2], NULL) == 0);

    listInsertOrdered(&list, data[4], cmp);
    make_sure_that(test_list(&list,
            data[0], data[1], data[4], data[3], data[2], NULL) == 0);

    listInsertOrdered(&list, data[5], cmp);
    make_sure_that(test_list(&list,
            data[0], data[1], data[5], data[4], data[3], data[2], NULL) == 0);

    listRemove(&list, data[3]);
    listRemove(&list, data[4]);
    listRemove(&list, data[5]);

    listAppendOrdered(&list, data[3], cmp);
    make_sure_that(test_list(&list,
            data[0], data[1], data[3], data[2], NULL) == 0);

    listAppendOrdered(&list, data[4], cmp);
    make_sure_that(test_list(&list,
            data[0], data[1], data[3], data[4], data[2], NULL) == 0);

    listAppendOrdered(&list, data[5], cmp);
    make_sure_that(test_list(&list,
            data[0], data[1], data[3], data[4], data[5], data[2], NULL) == 0);

    exit(errors);
}
