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

typedef struct Node Node;
typedef struct List List;

struct Node {
  Node *next;
  Node *prev;
  List *list;
};

struct List {
  Node *head;
  Node *tail;
  int length;
};

#define listInsertHead(l, n) f_listInsertHead((l), &(n)->node)
#define listAppendTail(l, n) f_listAppendTail((l), &(n)->node)
#define listInsert(l, n, b)  f_listInsert((l), &(n)->node, &(b)->node)
#define listInsert(l, n, b)  f_listInsert((l), &(n)->node, &(b)->node)
#define listNext(n)          f_listNext(&(n)->node)
#define listPrev(n)          f_listPrev(&(n)->node)
#define listRemove(l, n)     f_listRemove((l), &(n)->node)

/* Create a new, empty list. */
List *listCreate(void);

/* Initialize the list specified by <list>. */
void listInitialize(List *list);

/* Insert <node> at the head of list <list>. */
void f_listInsertHead(List *list, Node *node);

/* Append <node> to the tail of list <list>. */
void f_listAppendTail(List *list, Node *node);

/* Insert <node> just before <before> in list <list>. */
void f_listInsert(List *list, Node *node, Node *before);

/* Append <node> to <after> in list <list>. */
void f_listAppend(List *list, Node *node, Node *after);

/* Remove <node> from list <list>. */
void f_listRemove(List *list, Node *node);

/* Return the node before <node>. */
void *f_listPrev(const Node *node);

/* Return the node following <node>. */
void *f_listNext(const Node *node);

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