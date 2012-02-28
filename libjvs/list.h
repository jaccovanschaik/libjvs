/*
 * liblist: A package for handling linked lists.
 *
 * Copyright: (c) 2004-2005 Jacco van Schaik (jacco@frontier.nl)
 * Version:   $Id: list.h 242 2007-06-23 23:12:05Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifndef LIST_H
#define LIST_H

#ifndef NULL
#define NULL 0
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

#define listInsertHead(l, n) f_listInsertHead((l), &(n)->_node)
#define listAppendTail(l, n) f_listAppendTail((l), &(n)->_node)
#define listInsert(l, n, b)  f_listInsert((l), &(n)->_node, &(b)->_node)
#define listInsert(l, n, b)  f_listInsert((l), &(n)->_node, &(b)->_node)
#define listNext(n)          f_listNext(&(n)->_node)
#define listPrev(n)          f_listPrev(&(n)->_node)
#define listRemove(l, n)     f_listRemove((l), &(n)->_node)

/* Create a new, empty list. */
List *listCreate(void);

/* Initialize the list specified by <list>. */
void listInitialize(List *list);

/* Insert <node> at the head of list <list>. */
void f_listInsertHead(List *list, ListNode *node);

/* Append <node> to the tail of list <list>. */
void f_listAppendTail(List *list, ListNode *node);

/* Insert <node> just before <before> in list <list>. */
void f_listInsert(List *list, ListNode *node, ListNode *before);

/* Append <node> to <after> in list <list>. */
void f_listAppend(List *list, ListNode *node, ListNode *after);

/* Remove <node> from list <list>. */
void f_listRemove(List *list, ListNode *node);

/* Return the node before <node>. */
void *f_listPrev(const ListNode *node);

/* Return the node following <node>. */
void *f_listNext(const ListNode *node);

/* Return the node at the head of <list>. */
void *listHead(const List *list);

/* Return the node at the end of <list>. */
void *listTail(const List *list);

/* Remove the first node in list <list> and return a pointer to it. */
void *listRemoveHead(List *list);

/* Remove the last node in list <list> and return a pointer to it. */
void *listRemoveTail(List *list);

/* Return the number of nodes in list <list>. */
int listLength(const List *list);

/* Return TRUE if <list> is empty. */
int listIsEmpty(const List *list);

/* Sort <list> using comparison function <cmp>. */
void listSort(List *list, int(*cmp)(const void *, const void *));

#endif
