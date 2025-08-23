/*
 * list.c: A package for handling linked lists of structs. To make a
 * struct "listable" it must have a "ListNode" struct named "_node" as
 * its first element.
 *
 * list.c is part of libjvs.
 *
 * Copyright:   (c) 2004-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: list.c 507 2025-08-23 14:43:51Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "list.h"

/*
 * Create a new, empty list.
 */
List *listCreate(void)
{
   List *list = calloc(1, sizeof(List));

   assert(list);

   return list;
}

/*
 * Initialize the list specified by <list>.
 */
void listInitialize(List *list)
{
    assert(list != NULL);

    memset(list, 0, sizeof(List));
}

/*
 * Insert <node> at the head of list <list>.
 */
void _listInsertHead(List *list, ListNode *node)
{
    assert(list != NULL);
    assert(node != NULL);

    assert(node->next == NULL);
    assert(node->prev == NULL);
    assert(node->list == NULL);

    node->prev = NULL;
    node->next = list->head;

    if (list->head != NULL)
        list->head->prev = node;
    else
        list->tail = node;

    list->head = node;

    node->list = list;

    list->length++;
}

/*
 * Append <node> to the tail of list <list>.
 */
void _listAppendTail(List *list, ListNode *node)
{
    assert(list != NULL);
    assert(node != NULL);

    assert(node->next == NULL);
    assert(node->prev == NULL);
    assert(node->list == NULL);

    node->next = NULL;
    node->prev = list->tail;

    if (list->tail != NULL)
        list->tail->next = node;
    else
        list->head = node;

    list->tail = node;

    node->list = list;

    list->length++;
}

/*
 * Insert <node> just before <before> in list <list>.
 */
void _listInsert(List *list, ListNode *node, ListNode *before)
{
    assert(list != NULL);
    assert(node != NULL);

    assert(node->next == NULL);
    assert(node->prev == NULL);
    assert(node->list == NULL);

    assert(before == NULL || before->list == list);

    if (before == NULL) {
        _listAppendTail(list, node);
        return;
    }
    if (before->prev == NULL) {
        _listInsertHead(list, node);
        return;
    }

    node->next = before;
    node->prev = before->prev;

    before->prev->next = node;
    before->prev = node;

    node->list = list;

    list->length++;
}

/*
 * Append <node> to <after> in list <list>.
 */
void _listAppend(List *list, ListNode *node, ListNode *after)
{
    assert(list != NULL);
    assert(node != NULL);

    assert(node->next == NULL);
    assert(node->prev == NULL);
    assert(node->list == NULL);

    assert(after == NULL || after->list == list);

    if (after == NULL) {
        _listInsertHead(list, node);
        return;
    }
    if (after->next == NULL) {
        _listAppendTail(list, node);
        return;
    }

    node->prev = after;
    node->next = after->next;

    after->next->prev = node;
    after->next = node;

    node->list = list;

    list->length++;
}

/*
 * Insert <node> into <list>, maintaining the order in the list according to
 * comparison function <cmp>. If there are already one or more entries in the
 * list with the same "rank" as <node>, <node> will be inserted *before* those
 * entries. Also, this function starts searching from the head of the list, so
 * if you suspect <node> will end up near the head this might be the function
 * to use.
 */
void _listInsertOrdered(List *list, ListNode *node,
        int(*cmp)(const void *, const void *))
{
    assert(list != NULL);
    assert(node != NULL);
    assert(cmp != NULL);

    assert(node->next == NULL);
    assert(node->prev == NULL);
    assert(node->list == NULL);

    ListNode *cursor;

    for (cursor = list->head; cursor; cursor = cursor->next) {
        if (cmp(cursor, node) >= 0) break;
    }

    _listInsert(list, node, cursor);
}

/*
 * Insert <node> into <list>, maintaining the order in the list according to
 * comparison function <cmp>. If there are already one or more entries in the
 * list with the same "rank" as <node>, <node> will be inserted *after* those
 * entries. Also, this function starts searching from the tail of the list, so
 * if you suspect <node> will end up near the tail this might be the function
 * to use.
 */
void _listAppendOrdered(List *list, ListNode *node,
        int(*cmp)(const void *, const void *))
{
    assert(list != NULL);
    assert(node != NULL);
    assert(cmp != NULL);

    assert(node->next == NULL);
    assert(node->prev == NULL);
    assert(node->list == NULL);

    ListNode *cursor;

    for (cursor = list->tail; cursor; cursor = cursor->prev) {
        if (cmp(node, cursor) >= 0) break;
    }

    _listAppend(list, node, cursor);
}

/*
 * Remove <node> from list <list>.
 */
void _listRemove(List *list, ListNode *node)
{
    assert(list != NULL);
    assert(node != NULL);

    assert(node->list == list);

    if (node->prev != NULL)
        node->prev->next = node->next;
    else
        list->head = node->next;

    if (node->next != NULL)
        node->next->prev = node->prev;
    else
        list->tail = node->prev;

    node->next = NULL;
    node->prev = NULL;

    node->list = NULL;

    list->length--;
}

/*
 * Return the node before <node>.
 */
void *_listPrev(const ListNode *node)
{
    return(node->prev);
}

/*
 * Return the node following <node>.
 */
void *_listNext(const ListNode *node)
{
    return(node->next);
}

/*
 * Return the node at the head of <list>.
 */
void *listHead(const List *list)
{
    return(list->head);
}

/*
 * Return the node at the end of <list>.
 */
void *listTail(const List *list)
{
    return(list->tail);
}

/*
 * Remove the first node in list <list> and return a pointer to it.
 */
void *listRemoveHead(List *list)
{
    ListNode *node;

    assert(list != NULL);

    if (list->head == NULL) return(NULL);

    node = list->head;

    _listRemove(list, node);

    return(node);
}

/*
 * Remove the last node in list <list> and return a pointer to it.
 */
void *listRemoveTail(List *list)
{
    ListNode *node;

    assert(list != NULL);

    if (list->tail == NULL) return(NULL);

    node = list->tail;

    _listRemove(list, node);

    return(node);
}

/*
 * Return the number of nodes in list <list>.
 */
int listLength(const List *list)
{
    assert(list != NULL);

    return list->length;
}

/*
 * Return TRUE if <list> is empty.
 */
int listIsEmpty(const List *list)
{
    assert(list != NULL);

    return(list->length == 0);
}

/*
 * Return the list that contains <node>, or NULL if <node> is not contained in
 * a list.
 */
List *_listContaining(ListNode *node)
{
    return node->list;
}

/*
 * Sort <list> using comparison function <cmp>.
 */
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
            _listRemove(&left, l);      /* Remove <l> from <left>... */
            _listInsert(list, l, r);    /* Put <l> into <list>, before <r>. */
            l = listHead(&left);         /* Get the next <l>. */
        }
        else {
            r = _listNext(r);           /* Move onto the next <r>. */
        }
    }
}
