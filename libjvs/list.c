/*
 * liblist: A package for handling linked lists.
 *
 * Copyright: (c) 2004-2005 Jacco van Schaik (jacco@frontier.nl)
 * Version:   $Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "list.h"

#define N(pointer) ((ListNode *) pointer)

char *_id_list = "$Id$\n";

/* Create a new, empty list. */

List *listCreate(void)
{
   List *list = calloc(1, sizeof(List));

   assert(list);

   return list;
}

/* Initialize the list specified by <list>. */

void listInitialize(List *list)
{
    assert(list != NULL);

    memset(list, 0, sizeof(List));
}

/* Insert <node> at the head of list <list>. */

void f_listInsertHead(List *list, ListNode *node)
{
    assert(list != NULL);
    assert(node != NULL);

    assert(N(node)->next == NULL);
    assert(N(node)->prev == NULL);
    assert(N(node)->list == NULL);

    N(node)->prev = NULL;
    N(node)->next = list->head;

    if (list->head != NULL)
	list->head->prev = N(node);
    else
	list->tail = N(node);

    list->head = N(node);

    N(node)->list = list;

    list->length++;
}

/* Append <node> to the tail of list <list>. */

void f_listAppendTail(List *list, ListNode *node)
{
    assert(list != NULL);
    assert(node != NULL);

    assert(N(node)->next == NULL);
    assert(N(node)->prev == NULL);
    assert(N(node)->list == NULL);

    N(node)->next = NULL;
    N(node)->prev = list->tail;

    if (list->tail != NULL)
	list->tail->next = N(node);
    else
	list->head = N(node);

    list->tail = N(node);

    N(node)->list = list;

    list->length++;
}

/* Insert <node> just before <before> in list <list>. */

void f_listInsert(List *list, ListNode *node, ListNode *before)
{
    assert(list != NULL);
    assert(node != NULL);

    assert(N(node)->next == NULL);
    assert(N(node)->prev == NULL);
    assert(N(node)->list == NULL);

    assert(before == NULL || N(before)->list == list);

    if (before == NULL) {
	f_listAppendTail(list, node);
	return;
    }
    if (N(before)->prev == NULL) {
	f_listInsertHead(list, node);
	return;
    }

    N(node)->next = N(before);
    N(node)->prev = N(before)->prev;

    N(before)->prev->next = N(node);
    N(before)->prev = N(node);

    N(node)->list = list;

    list->length++;
}

/* Append <node> to <after> in list <list>. */

void f_listAppend(List *list, ListNode *node, ListNode *after)
{
    assert(list != NULL);
    assert(node != NULL);

    assert(N(node)->next == NULL);
    assert(N(node)->prev == NULL);
    assert(N(node)->list == NULL);

    assert(after == NULL || N(after)->list == list);

    if (after == NULL) {
	f_listInsertHead(list, node);
	return;
    }
    if (N(after)->next == NULL) {
	f_listAppendTail(list, node);
	return;
    }

    N(node)->prev = N(after);
    N(node)->next = N(after)->next;

    N(after)->next->prev = N(node);
    N(after)->next = N(node);

    N(node)->list = list;

    list->length++;
}

/* Remove <node> from list <list>. */

void f_listRemove(List *list, ListNode *node)
{
    assert(list != NULL);
    assert(node != NULL);

    assert(N(node)->list == list);

    if (N(node)->prev != NULL)
	N(node)->prev->next = N(node)->next;
    else
	list->head = N(node)->next;

    if (N(node)->next != NULL)
	N(node)->next->prev = N(node)->prev;
    else
	list->tail = N(node)->prev;

    N(node)->next = NULL;
    N(node)->prev = NULL;

    N(node)->list = NULL;

    list->length--;
}

/* Return the node before <node>. */

void *f_listPrev(const ListNode *node)
{
    return(N(node)->prev);
}

/* Return the node following <node>. */

void *f_listNext(const ListNode *node)
{
    return(N(node)->next);
}

/* Return the node at the head of <list>. */

void *listHead(const List *list)
{
    return(list->head);
}

/* Return the node at the end of <list>. */

void *listTail(const List *list)
{
    return(list->tail);
}

/* Remove the first node in list <list> and return a pointer to it. */

void *listRemoveHead(List *list)
{
    ListNode *node;

    assert(list != NULL);

    if (list->head == NULL) return(NULL);

    node = list->head;

    f_listRemove(list, node);

    return(node);
}

/* Remove the last node in list <list> and return a pointer to it. */

void *listRemoveTail(List *list)
{
    ListNode *node;

    assert(list != NULL);

    if (list->tail == NULL) return(NULL);

    node = list->tail;

    f_listRemove(list, node);

    return(node);
}

/* Return the number of nodes in list <list>. */

int listLength(const List *list)
{
    assert(list != NULL);

    return list->length;
}

/* Return TRUE if <list> is empty. */

int listIsEmpty(const List *list)
{
    assert(list != NULL);

    return(list->length == 0);
}

/* Sort <list> using comparison function <cmp>. */

void listSort(List *list, int(*cmp)(const void *, const void *))
{
    ListNode *l, *r;        /* Current node in l(eft) and r(ight) list. */
    List left;              /* Left list (<list> is used as the right list). */
    int i, len = listLength(list);

    /* This is a standard Merge Sort... */

    if (len <= 1) return;   /* A list with 1 element is already sorted. */

    /* Split <list> in two at (or near) the halfway point. */

    left.length   = len / 2;
    list->length -= left.length;

    l = listHead(list);

    for (i = 0; i < left.length; i++) {
        l->list = &left;
        l = l->next;
    }

    left.head = list->head;
    left.tail = l->prev;

    list->head = l;

    l->prev->next = NULL;
    l->prev = NULL;

    /* At this point <left> is a valid list containing the left half of the
     * original <list>, and <list> contains the right half. Call myself
     * recursively to sort these two lists. */

    listSort(&left, cmp);
    listSort(list, cmp);

    /* OK, so <left> and <list> are now both sorted. Now pick elements from
     * <left> and merge them into <list>. */

    l = listHead(&left);
    r = listHead(list);

    /* As long as we still have an element from <left>... */
    while (l) {
        /* If we reached the end of <list> *or* <l> should be left of <r>... */
        if (r == NULL || cmp(l, r) <= 0) {
            f_listRemove(&left, l);      /* Remove <l> from <left>... */
            f_listInsert(list, l, r);    /* Put <l> into <list>, before <r>. */
            l = listHead(&left);         /* Get the next <l>. */
        }
        else {
            r = f_listNext(r);           /* Move onto the next <r>. */
        }
    }
}

#ifdef TEST

typedef struct {
    ListNode _node;
    int i;
} Data;

#define TEST_PTR(expr, exp) { \
    void *test = expr; \
    if (test != exp) { \
        fprintf(stderr, \
                "%s:%d: " #expr " returns %p, should be %p (%s).\n", \
                __FILE__, __LINE__, test, exp, #exp); \
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

#define TRUE  (0 == 0)
#define FALSE (1 == 0)

int cmp(const void *a, const void *b)
{
    const Data *A = a;
    const Data *B = b;

    return(A->i - B->i);
}

int main(int argc, char *argv[])
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

    listRemove(&list, data[0]);
    listRemove(&list, data[1]);
    listRemove(&list, data[2]);
    listRemove(&list, data[3]);

    TEST_INT(listLength(&list), 0);
    TEST_INT(listIsEmpty(&list),  TRUE);

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

    TEST_PTR(listRemoveHead(&list), data[3]);
    TEST_PTR(listRemoveHead(&list), data[4]);
    TEST_PTR(listRemoveHead(&list), data[0]);
    TEST_PTR(listRemoveHead(&list), data[5]);
    TEST_PTR(listRemoveHead(&list), data[1]);
    TEST_PTR(listRemoveHead(&list), data[2]);

    exit(errors);
}
#endif
