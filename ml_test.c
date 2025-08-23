#include "ml.c"
#include <stdio.h>

typedef struct {
    MListNode _node;
    int i;
    float f;
    char s[16];
} Data;

#define TEST_PTR(expr, exp) { \
    void *test = expr; \
    if (test != exp) { \
        fprintf(stderr, \
                "%d:ERROR: " #expr " returns %p, should be %p (%s).\n", \
                __LINE__, (void *) test, (void *) exp, #exp); \
        exit(1); \
    } \
}

#define TEST_INT(expr, exp) { \
    int test = expr; \
    if (test != exp) { \
        fprintf(stderr, \
                "%d:ERROR: " #expr " returns %d, should be %d.\n", \
                __LINE__, test, exp); \
        exit(1); \
    } \
}

int Compare(const void *a, const void *b)
{
    const Data *A = a;
    const Data *B = b;

    return(A->i - B->i);
}

int main(void)
{
    MList *list1;
    MList *list2;
    Data *data[6];
    Data *null_ptr = NULL;
    int i;

    list1 = mlCreate();

    for (i = 0; i < 6; i++) {
        data[i] = calloc(1, sizeof(Data));
    }

    /* Make sure mlLength returns the correct length */

    TEST_INT(mlLength(list1), 0);
    TEST_INT(mlIsEmpty(list1),  TRUE);

    TEST_INT(mlContains(list1, data[0]), FALSE);
    TEST_INT(mlContains(list1, data[1]), FALSE);
    TEST_INT(mlContains(list1, data[2]), FALSE);
    TEST_INT(mlContains(list1, data[3]), FALSE);

    /* Fill list1 using mlAppend */

    mlAppendTail(list1, data[0]);
    mlAppendTail(list1, data[1]);
    mlAppendTail(list1, data[2]);
    mlAppendTail(list1, data[3]);

    /* Make sure mlLength returns the correct length */

    TEST_INT(mlLength(list1), 4);
    TEST_INT(mlIsEmpty(list1),  FALSE);

    TEST_INT(mlContains(list1, data[0]), TRUE);
    TEST_INT(mlContains(list1, data[1]), TRUE);
    TEST_INT(mlContains(list1, data[2]), TRUE);
    TEST_INT(mlContains(list1, data[3]), TRUE);

    /* Check if it was filled correctly by walking it in two directions */

    TEST_PTR(mlHead(list1), data[0]);
    TEST_PTR(mlNext(list1, data[0]), data[1]);
    TEST_PTR(mlNext(list1, data[1]), data[2]);
    TEST_PTR(mlNext(list1, data[2]), data[3]);
    TEST_PTR(mlNext(list1, data[3]), NULL);

    TEST_PTR(mlTail(list1), data[3]);
    TEST_PTR(mlPrev(list1, data[3]), data[2]);
    TEST_PTR(mlPrev(list1, data[2]), data[1]);
    TEST_PTR(mlPrev(list1, data[1]), data[0]);
    TEST_PTR(mlPrev(list1, data[0]), NULL);

    /* Also check that mlCountContaining returns the correct number of lists */

    TEST_INT(mlCountContaining(data[0]), 1);
    TEST_INT(mlCountContaining(data[1]), 1);
    TEST_INT(mlCountContaining(data[2]), 1);
    TEST_INT(mlCountContaining(data[3]), 1);

    /* Special behaviour of mlNext and mlPrev: next of NULL is mlHead,
     * prev of NULL is mlTail. */

    TEST_PTR(mlNext(list1, null_ptr), data[0]);
    TEST_PTR(mlPrev(list1, null_ptr), data[3]);

    /* Empty the list using mlRemoveHead */

    TEST_PTR(mlRemoveHead(list1), data[0]);
    TEST_PTR(mlRemoveHead(list1), data[1]);
    TEST_PTR(mlRemoveHead(list1), data[2]);
    TEST_PTR(mlRemoveHead(list1), data[3]);
    TEST_PTR(mlRemoveHead(list1), NULL);

    /* Make sure mlLength returns the correct length */

    TEST_INT(mlLength(list1), 0);

    /* Now mlCountContaining should return number of lists = 0 */

    TEST_INT(mlCountContaining(data[0]), 0);
    TEST_INT(mlCountContaining(data[1]), 0);
    TEST_INT(mlCountContaining(data[2]), 0);
    TEST_INT(mlCountContaining(data[3]), 0);

    /* Fill 'er up using mlInsert */

    mlInsertHead(list1, data[0]);
    mlInsertHead(list1, data[1]);
    mlInsertHead(list1, data[2]);
    mlInsertHead(list1, data[3]);

    /* Check it again */

    TEST_PTR(mlHead(list1), data[3]);
    TEST_PTR(mlNext(list1, data[3]), data[2]);
    TEST_PTR(mlNext(list1, data[2]), data[1]);
    TEST_PTR(mlNext(list1, data[1]), data[0]);
    TEST_PTR(mlNext(list1, data[0]), NULL);

    TEST_PTR(mlTail(list1), data[0]);
    TEST_PTR(mlPrev(list1, data[0]), data[1]);
    TEST_PTR(mlPrev(list1, data[1]), data[2]);
    TEST_PTR(mlPrev(list1, data[2]), data[3]);
    TEST_PTR(mlPrev(list1, data[3]), NULL);

    /* Check mlCountContaining again */

    TEST_INT(mlCountContaining(data[0]), 1);
    TEST_INT(mlCountContaining(data[1]), 1);
    TEST_INT(mlCountContaining(data[2]), 1);
    TEST_INT(mlCountContaining(data[3]), 1);

    /* Now empty it using mlRemoveTail */

    TEST_PTR(mlRemoveTail(list1), data[0]);
    TEST_PTR(mlRemoveTail(list1), data[1]);
    TEST_PTR(mlRemoveTail(list1), data[2]);
    TEST_PTR(mlRemoveTail(list1), data[3]);
    TEST_PTR(mlRemoveTail(list1), NULL);

    /* Make sure mlLength returns the correct length */

    TEST_INT(mlLength(list1), 0);

    /* Again, mlCountContaining should return number of lists = 0 */

    TEST_INT(mlCountContaining(data[0]), 0);
    TEST_INT(mlCountContaining(data[1]), 0);
    TEST_INT(mlCountContaining(data[2]), 0);
    TEST_INT(mlCountContaining(data[3]), 0);

    /* Check if mlHead and mlTail both return NULL */

    TEST_PTR(mlHead(list1), NULL);
    TEST_PTR(mlTail(list1), NULL);

    /* Fill 'er up again */

    mlAppendTail(list1, data[0]);
    mlAppendTail(list1, data[1]);
    mlAppendTail(list1, data[2]);
    mlAppendTail(list1, data[3]);

    /* See if mlRemove on the head item works */

    mlRemove(list1, data[0]);

    TEST_PTR(mlHead(list1), data[1]);
    TEST_PTR(mlNext(list1, data[1]), data[2]);
    TEST_PTR(mlNext(list1, data[2]), data[3]);
    TEST_PTR(mlNext(list1, data[3]), NULL);
    TEST_PTR(mlTail(list1), data[3]);

    /* Make sure mlLength returns the correct length */

    TEST_INT(mlLength(list1), 3);

    /* Now see if mlRemove on the tail item works */

    mlRemove(list1, data[3]);

    TEST_PTR(mlHead(list1), data[1]);
    TEST_PTR(mlNext(list1, data[1]), data[2]);
    TEST_PTR(mlNext(list1, data[2]), NULL);
    TEST_PTR(mlTail(list1), data[2]);

    /* Make sure mlLength returns the correct length */

    TEST_INT(mlLength(list1), 2);

    /* Remove what's left and fill er up again */

    mlRemoveHead(list1);
    mlRemoveHead(list1);

    /* Make sure mlLength returns the correct length */

    TEST_INT(mlLength(list1), 0);

    mlAppendTail(list1, data[0]);
    mlAppendTail(list1, data[1]);
    mlAppendTail(list1, data[2]);
    mlAppendTail(list1, data[3]);

    /* See if we can insert using mlAppendAfter */

    mlAppendAfter(list1, data[4], data[0]);

    TEST_PTR(mlHead(list1), data[0]);
    TEST_PTR(mlNext(list1, data[0]), data[4]);
    TEST_PTR(mlNext(list1, data[4]), data[1]);
    TEST_PTR(mlNext(list1, data[1]), data[2]);
    TEST_PTR(mlNext(list1, data[2]), data[3]);
    TEST_PTR(mlNext(list1, data[3]), NULL);

    TEST_PTR(mlTail(list1), data[3]);
    TEST_PTR(mlPrev(list1, data[3]), data[2]);
    TEST_PTR(mlPrev(list1, data[2]), data[1]);
    TEST_PTR(mlPrev(list1, data[1]), data[4]);
    TEST_PTR(mlPrev(list1, data[4]), data[0]);
    TEST_PTR(mlPrev(list1, data[0]), NULL);

    /* ... and also with mlInsertBefore */

    mlInsertBefore(list1, data[5], data[3]);

    TEST_PTR(mlHead(list1), data[0]);
    TEST_PTR(mlNext(list1, data[0]), data[4]);
    TEST_PTR(mlNext(list1, data[4]), data[1]);
    TEST_PTR(mlNext(list1, data[1]), data[2]);
    TEST_PTR(mlNext(list1, data[2]), data[5]);
    TEST_PTR(mlNext(list1, data[5]), data[3]);
    TEST_PTR(mlNext(list1, data[3]), NULL);

    TEST_PTR(mlTail(list1), data[3]);
    TEST_PTR(mlPrev(list1, data[3]), data[5]);
    TEST_PTR(mlPrev(list1, data[5]), data[2]);
    TEST_PTR(mlPrev(list1, data[2]), data[1]);
    TEST_PTR(mlPrev(list1, data[1]), data[4]);
    TEST_PTR(mlPrev(list1, data[4]), data[0]);
    TEST_PTR(mlPrev(list1, data[0]), NULL);

    /* See if mlAppendAfter and mlInsertBefore with after and before
     * set to NULL works
     */

    mlRemoveHead(list1);
    mlRemoveTail(list1);

    mlAppendAfter(list1, data[0], null_ptr);
    mlInsertBefore(list1, data[3], null_ptr);

    TEST_PTR(mlHead(list1), data[0]);
    TEST_PTR(mlNext(list1, data[0]), data[4]);
    TEST_PTR(mlNext(list1, data[4]), data[1]);
    TEST_PTR(mlNext(list1, data[1]), data[2]);
    TEST_PTR(mlNext(list1, data[2]), data[5]);
    TEST_PTR(mlNext(list1, data[5]), data[3]);
    TEST_PTR(mlNext(list1, data[3]), NULL);

    TEST_PTR(mlTail(list1), data[3]);
    TEST_PTR(mlPrev(list1, data[3]), data[5]);
    TEST_PTR(mlPrev(list1, data[5]), data[2]);
    TEST_PTR(mlPrev(list1, data[2]), data[1]);
    TEST_PTR(mlPrev(list1, data[1]), data[4]);
    TEST_PTR(mlPrev(list1, data[4]), data[0]);
    TEST_PTR(mlPrev(list1, data[0]), NULL);

    /* Clear list and see if it's really empty */

    mlClear(list1);

    TEST_PTR(mlHead(list1), NULL);
    TEST_PTR(mlTail(list1), NULL);

    /* Delete and re-create the list */

    mlDelete(list1);

    list1 = mlCreate();

    /* list2, why don't you come and join us! */

    list2 = mlCreate();

    /* Fill list1 and list2 with the same items */

    mlAppendTail(list1, data[0]);
    mlAppendTail(list1, data[1]);
    mlAppendTail(list2, data[0]);
    mlAppendTail(list2, data[1]);

    /* Check both lists */

    TEST_INT(mlLength(list1), 2);
    TEST_INT(mlLength(list2), 2);

    TEST_PTR(mlHead(list1), data[0]);
    TEST_PTR(mlNext(list1, data[0]), data[1]);
    TEST_PTR(mlNext(list1, data[1]), NULL);

    TEST_PTR(mlTail(list1), data[1]);
    TEST_PTR(mlPrev(list1, data[1]), data[0]);
    TEST_PTR(mlPrev(list1, data[0]), NULL);

    TEST_PTR(mlHead(list2), data[0]);
    TEST_PTR(mlNext(list2, data[0]), data[1]);
    TEST_PTR(mlNext(list2, data[1]), NULL);

    TEST_PTR(mlTail(list2), data[1]);
    TEST_PTR(mlPrev(list2, data[1]), data[0]);
    TEST_PTR(mlPrev(list2, data[0]), NULL);

    TEST_INT(mlCountContaining(data[0]), 2);
    TEST_INT(mlCountContaining(data[1]), 2);

    /* Now empty list1 without affecting list2 */

    TEST_PTR(mlRemoveHead(list1), data[0]);
    TEST_PTR(mlRemoveHead(list1), data[1]);
    TEST_PTR(mlRemoveHead(list1), NULL);

    TEST_INT(mlLength(list1), 0);
    TEST_INT(mlLength(list2), 2);

    TEST_PTR(mlHead(list2), data[0]);
    TEST_PTR(mlNext(list2, data[0]), data[1]);
    TEST_PTR(mlNext(list2, data[1]), NULL);

    TEST_PTR(mlTail(list2), data[1]);
    TEST_PTR(mlPrev(list2, data[1]), data[0]);
    TEST_PTR(mlPrev(list2, data[0]), NULL);

    /* Test SortMList: first delete and re-create list1. */

    mlDelete(list1);

    list1 = mlCreate();

    /* Fill list items with some data */

    data[0]->i = 0;
    data[1]->i = 1;
    data[2]->i = 2;
    data[3]->i = 3;
    data[4]->i = 4;
    data[5]->i = 5;

    /* Fill list in random order. */

    mlAppendTail(list1, data[0]);
    mlAppendTail(list1, data[2]);
    mlAppendTail(list1, data[4]);
    mlAppendTail(list1, data[3]);
    mlAppendTail(list1, data[5]);
    mlAppendTail(list1, data[1]);

    /* Sort list. Call should return 0. */

    mlSort(list1, Compare);

    /* Now the items should be sorted low-to-high. */

    TEST_PTR(mlHead(list1), data[0]);
    TEST_PTR(mlNext(list1, data[0]), data[1]);
    TEST_PTR(mlNext(list1, data[1]), data[2]);
    TEST_PTR(mlNext(list1, data[2]), data[3]);
    TEST_PTR(mlNext(list1, data[3]), data[4]);
    TEST_PTR(mlNext(list1, data[4]), data[5]);
    TEST_PTR(mlNext(list1, data[5]), NULL);

    exit(0);
}
