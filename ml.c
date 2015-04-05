/* ml.c: A package for handling multiple linked lists. Differs from list in that items can be in
 * more than one list at the same time.
 *
 * Copyright:	(c) 2005 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: ml.c 265 2015-04-05 18:06:58Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "debug.h"
#include "defs.h"
#include "ml.h"

/*
   The data structures we're using may be seen as follows:

         MListNode1  MListNode2  MListNode3  MListNode4  MListNode5  MListNode6 ...
   MList1     +
   MList2     +----------+----------+----------+----------+----------+
   MList3     +----------|----------|----------+          |          |
   MList4                +----------|---------------------|----------+
   MList5     +---------------------+---------------------+

   Every '+' represents a Link: a connection of a node (and thereby a listed
   node) to a list. The {prev/next}_in_list pointers are the horizontal lines,
   the {prev/next}_in_node pointers are the vertical lines. Every Link also has a
   pointer back to the list and the node that it's connected to.
 */

struct Link {
    Link *next_in_list;
    Link *prev_in_list;
    Link *next_in_node;
    Link *prev_in_node;
    MList *list;
    MListNode  *node;
};

static Link *ml_create_link(MListNode *node, MList *list,
                        Link *prev_in_list, Link *next_in_list)
{
    Link *link = calloc(1, sizeof(Link));

    link->list = list;
    link->node = node;

    link->next_in_node = NULL;
    link->prev_in_node = link->node->last_in_node;

    if (link->node->last_in_node != NULL)
        link->node->last_in_node->next_in_node = link;
    else
        link->node->first_in_node = link;

    link->node->last_in_node = link;

    link->next_in_list = next_in_list;
    link->prev_in_list = prev_in_list;

    if (next_in_list != NULL)
        next_in_list->prev_in_list = link;
    else
        link->list->last_in_list = link;

    if (prev_in_list != NULL)
        prev_in_list->next_in_list = link;
    else
        link->list->first_in_list = link;

    return(link);
}

static void ml_disconnect_link(Link *link)
{
    if (link->next_in_list != NULL)
        link->next_in_list->prev_in_list = link->prev_in_list;
    else
        link->list->last_in_list = link->prev_in_list;

    if (link->prev_in_list != NULL)
        link->prev_in_list->next_in_list = link->next_in_list;
    else
        link->list->first_in_list = link->next_in_list;

    if (link->next_in_node != NULL)
        link->next_in_node->prev_in_node = link->prev_in_node;
    else
        link->node->last_in_node = link->prev_in_node;

    if (link->prev_in_node != NULL)
        link->prev_in_node->next_in_node = link->next_in_node;
    else
        link->node->first_in_node = link->next_in_node;

    link->next_in_list = NULL;
    link->prev_in_list = NULL;
    link->next_in_node = NULL;
    link->prev_in_node = NULL;
}

static void ml_delete_link(Link *link)
{
    ml_disconnect_link(link);

    free(link);
}

/*
 * Find the first link that connects <list> with <node>.
 */

static Link *ml_find_first_link(MList *list, MListNode *node)
{
    Link *link;

    for (link = node->first_in_node; link; link = link->next_in_node) {
        if (link->list == list) break;
    }

    return(link);
}

/*
 * Find the last link that connects <list> with <node>.
 */

static Link *ml_find_last_link(MList *list, MListNode *node)
{
    Link *link;

    for (link = node->last_in_node; link; link = link->prev_in_node) {
        if (link->list == list) break;
    }

    return(link);
}

/*
 * Create and initialize a list
 */

MList *mlCreate(void)
{
    MList *list = calloc(1, sizeof(MList));

    return(list);
}

/*
 * Initialize a list
 */

void mlInitialize(MList *list)
{
    list->first_in_list = NULL;
    list->last_in_list  = NULL;
}

/*
 * Clear out and delete a list
 */

void mlDelete(MList *list)
{
    mlClear(list);

    free(list);
}

/*
 * Clear out (i.e. delete every link in) a list
 */

void mlClear(MList *list)
{
    Link *link, *next_link;

    for (link = list->first_in_list; link; link = next_link) {
        next_link = link->next_in_list;
        ml_delete_link(link);
    }
}

/*
 * Get the successor of <node> in <list>
 */

void *_mlNext(MList *list, MListNode *node)
{
    Link *next;
    Link *link;

    if (node == NULL) return(mlHead(list));

    link = ml_find_first_link(list, node);

    dbgAssert(stderr, link != NULL, "mlNext: <list> does not contain <node>\n");

    next = link->next_in_list;

    return(next == NULL ? NULL : next->node);
}

/*
 * Get the predecessor of <node> in <list>
 */

void *_mlPrev(MList *list, MListNode *node)
{
    Link *prev;
    Link *link;

    if (node == NULL) return(mlTail(list));

    link = ml_find_last_link(list, node);

    dbgAssert(stderr, link != NULL, "mlPrev: <list> does not contain <node>\n");

    prev = link->prev_in_list;

    return(prev == NULL ? NULL : prev->node);
}

/*
 * Get the first node in <list>
 */

void *mlHead(MList *list)
{
    return(list->first_in_list == NULL ? NULL : list->first_in_list->node);
}

/*
 * Get the last node in <list>
 */

void *mlTail(MList *list)
{
    return(list->last_in_list == NULL ? NULL : list->last_in_list->node);
}

/*
 * Append <node> to the end of <list>
 */

void _mlAppendTail(MList *list, MListNode *node)
{
    dbgAssert(stderr, !_mlContains(list, node),
            "List at %p already contains node at %p\n", (void *) list, (void *) node);

    ml_create_link(node, list, list->last_in_list, NULL);
}

/*
 * Insert <node> at the head of <list>
 */

void _mlInsertHead(MList *list, MListNode *node)
{
    dbgAssert(stderr, !_mlContains(list, node),
            "List at %p already contains node at %p\n", (void *) list, (void *) node);

    ml_create_link(node, list, NULL, list->first_in_list);
}

/*
 * Append <node> after <after> in <list>
 */

void _mlAppendAfter(MList *list, MListNode *node, MListNode *after)
{
    Link *next_link;
    Link *prev_link = NULL;

    dbgAssert(stderr, !_mlContains(list, node),
            "List at %p already contains node at %p\n", (void *) list, (void *) node);

    if (after != NULL)
        dbgAssert(stderr, (prev_link = ml_find_first_link(list, after)) != NULL,
                "mlAppendAfter: <after> is not an element of <list>\n");

    if (prev_link != NULL)
        next_link = prev_link->next_in_list;
    else
        next_link = list->first_in_list;

    ml_create_link(node, list, prev_link, next_link);
}

/*
 * Insert <node> before <before> in <list>
 */

void _mlInsertBefore(MList *list, MListNode *node, MListNode *before)
{
    Link *prev_link;
    Link *next_link = NULL;

    dbgAssert(stderr, !_mlContains(list, node),
            "List at %p already contains node at %p\n", (void *) list, (void *) node);

    if (before != NULL)
        dbgAssert(stderr, (next_link = ml_find_last_link(list, before)) != NULL,
                "mlAppendAfter: <before> is not an element of <list>\n");

    if (next_link != NULL)
        prev_link = next_link->prev_in_list;
    else
        prev_link = list->last_in_list;

    ml_create_link(node, list, prev_link, next_link);
}

/*
 * Remove <node> from <list>
 */

void _mlRemove(MList *list, MListNode *node)
{
    Link *link = ml_find_first_link(list, node);

    ml_delete_link(link);
}

/*
 * Remove first node from <list> and return its address
 */

void *mlRemoveHead(MList *list)
{
    Link *link;
    MListNode *node;

    if ((link = list->first_in_list) == NULL) return(NULL);

    node = link->node;

    ml_delete_link(link);

    return(node);
}

/*
 * Remove last node from <list> and return its address
 */

void *mlRemoveTail(MList *list)
{
    Link *link;
    MListNode *node;

    if ((link = list->last_in_list) == NULL) return(NULL);

    node = link->node;

    ml_delete_link(link);

    return(node);
}

/*
 * Return the number of lists that contain <node>
 */

int _mlCountContaining(MListNode *node)
{
    Link *link;
    int count = 0;

    for (link = node->first_in_node; link; link = link->next_in_node) count++;

    return(count);
}

/*
 * Return the length of <list>
 */

int mlLength(MList *list)
{
    Link *link;
    int count = 0;

    for (link = list->first_in_list; link; link = link->next_in_list) count++;

    return(count);
}

/*
 * Dump the contents of <list>, preceded by <str>, on <fp>
 */

void mlDump(FILE *fp, char *str, MList *list)
{
    Link *link;

    fprintf(fp, "%sMList at %p:\n", str ? str : "", (void *) list);

    for (link = list->first_in_list; link; link = link->next_in_list) {
        fprintf(fp, "\tLink at %p: list = %p, node = %p\n",
                (void *) link, (void *) link->list, (void *) link->node);
    }
}

/*
 * Dump the contents of <node>, preceded by <str>, on <fp>
 */

void mlDumpItem(FILE *fp, char *str, MListNode *node)
{
    Link *link;

    fprintf(fp, "%sMListNode at %p:\n", str ? str : "", (void *) node);

    for (link = node->first_in_node; link; link = link->next_in_node) {
        fprintf(fp, "\tLink at %p: list = %p, node = %p\n",
                (void *) link, (void *) link->list, (void *) link->node);
    }
}

/*
 * Sort <list>. Comparison function <cmp> should return a number less than, greater than or equal to
 * zero depending on whether <a> should be sorted before, after or at the same level as <b>.
 */
void mlSort(MList *list, int(*cmp)(const void *a, const void *b))
{
    MListNode *l, *r;        /* Current node in l(eft) and r(ight) list. */
    MList left = { 0 };      /* Left list (<list> is used as the right list). */
    int i, len = mlLength(list);

    /* This is a standard Merge Sort... */

    if (len <= 1) return;   /* A list with 1 element is already sorted. */

    /* Split <list> in two at (or near) the halfway point. */

    len /= 2;

    for (i = 0 ; i < len; i++)
        _mlAppendTail(&left, (MListNode *) mlRemoveHead(list));

    /* At this point <left> is a valid list containing the left half of the
     * original <list>, and <list> contains the right half. Call myself
     * recursively to sort these two lists. */

    mlSort(&left, cmp);
    mlSort(list, cmp);

    /* OK, so <left> and <list> are now both sorted. Now pick elements from
     * <left> and merge them into <list>. */

    l = mlHead(&left);
    r = mlHead(list);

    /* As long as we still have an element from <left>... */
    while (l) {
        /* If we reached the end of <list> *or* <l> should be left of <r>... */
        if (r == NULL || cmp(l, r) <= 0) {
            _mlRemove(&left, l);     /* Remove <l> from <left>... */
            _mlInsertBefore(list, l, r);   /* Put <l> into <list>, before <r>. */
            l = mlHead(&left);        /* Get the next <l>. */
        }
        else {
            r = _mlNext(list, r);          /* Move onto the next <r>. */
        }
    }
}

/*
 * Return TRUE if <list> is empty.
 */

int mlIsEmpty(MList *list)
{
    return(list->first_in_list == NULL);
}

/*
 * Return TRUE if <list> contains <node>.
 */

int _mlContains(MList *list, MListNode *node)
{
    return(ml_find_first_link(list, node) != NULL);
}

#ifdef TEST
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

int main(int argc, char *argv[])
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
#endif
