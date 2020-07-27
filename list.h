#ifndef LIBJVS_LIST_H
#define LIBJVS_LIST_H

/*
 * list.h: A package for handling linked lists of structs. To make a
 * struct "listable" it must have a "ListNode" struct named "_node" as
 * their first element.
 *
 * list.h is part of libjvs.
 *
 * Copyright:   (c) 2004-2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: list.h 393 2020-07-27 13:54:37Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ListNode ListNode;
typedef struct List List;

struct ListNode {
  ListNode *next;
  ListNode *prev;
  List *list;
};

struct List {
  ListNode *head;
  ListNode *tail;
  int length;
};

#define listInsertHead(l, n) _listInsertHead((l), &(n)->_node)
#define listAppendTail(l, n) _listAppendTail((l), &(n)->_node)
#define listInsert(l, n, b)  _listInsert((l), &(n)->_node, &(b)->_node)
#define listAppend(l, n, a)  _listAppend((l), &(n)->_node, &(a)->_node)
#define listNext(n)          _listNext(&(n)->_node)
#define listPrev(n)          _listPrev(&(n)->_node)
#define listRemove(l, n)     _listRemove((l), &(n)->_node)
#define listContaining(n)    _listContaining(&(n)->_node)

/*
 * Create a new, empty list.
 */
List *listCreate(void);

/*
 * Initialize the list specified by <list>.
 */
void listInitialize(List *list);

/*
 * Insert <node> at the head of list <list>.
 */
void _listInsertHead(List *list, ListNode *node);

/*
 * Append <node> to the tail of list <list>.
 */
void _listAppendTail(List *list, ListNode *node);

/*
 * Insert <node> just before <before> in list <list>.
 */
void _listInsert(List *list, ListNode *node, ListNode *before);

/*
 * Append <node> to <after> in list <list>.
 */
void _listAppend(List *list, ListNode *node, ListNode *after);

/*
 * Remove <node> from list <list>.
 */
void _listRemove(List *list, ListNode *node);

/*
 * Return the node before <node>.
 */
void *_listPrev(const ListNode *node);

/*
 * Return the node following <node>.
 */
void *_listNext(const ListNode *node);

/*
 * Return the node at the head of <list>.
 */
void *listHead(const List *list);

/*
 * Return the node at the end of <list>.
 */
void *listTail(const List *list);

/*
 * Remove the first node in list <list> and return a pointer to it.
 */
void *listRemoveHead(List *list);

/*
 * Remove the last node in list <list> and return a pointer to it.
 */
void *listRemoveTail(List *list);

/*
 * Return the number of nodes in list <list>.
 */
int listLength(const List *list);

/*
 * Return TRUE if <list> is empty.
 */
int listIsEmpty(const List *list);

/*
 * Return the list that contains <node>, or NULL if <node> is not contained in a list.
 */
List *_listContaining(ListNode *node);

/*
 * Sort <list> using comparison function <cmp>.
 */
void listSort(List *list, int(*cmp)(const void *, const void *));

#ifdef __cplusplus
}
#endif

#endif
