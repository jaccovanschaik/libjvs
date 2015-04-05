#ifndef LIBJVS_ML_H
#define LIBJVS_ML_H

/* ml.c: A package for handling multiple linked lists. Differs from list in that items can be in
 * more than one list at the same time.
 *
 * Copyright: (c) 2005 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:   $Id: ml.h 265 2015-04-05 18:06:58Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>

typedef struct Link Link;

/*
 * A linked list.
 */

typedef struct {
    Link *first_in_list;
    Link *last_in_list;
} MList;

/*
 * A node. Put this as the first element in your structs.
 */

typedef struct {
    Link *first_in_node;
    Link *last_in_node;
} MListNode;

#define mlNext(l, i)              _mlNext((l), &(i)->_node)
#define mlPrev(l, i)              _mlPrev((l), &(i)->_node)
#define mlAppendTail(l, i)        _mlAppendTail((l), &(i)->_node)
#define mlInsertHead(l, i)        _mlInsertHead((l), &(i)->_node)
#define mlAppendAfter(l, i, a)    _mlAppendAfter((l), &(i)->_node, &(a)->_node)
#define mlInsertBefore(l, i, b)   _mlInsertBefore((l), &(i)->_node, &(b)->_node)
#define mlRemove(l, i)            _mlRemove((l), &(i)->_node)
#define mlCountContaining(i)      _mlCountContaining(&(i)->_node)
#define mlContains(l, i)          _mlContains((l), &(i)->_node)

/*
 * Create and initialize a list
 */
MList *mlCreate(void);

/*
 * Initialize a list
 */
void mlInitialize(MList *list);

/*
 * Clear out and delete a list
 */
void mlDelete(MList *list);

/*
 * Clear out (i.e. delete every link in) a list
 */
void mlClear(MList *list);

/*
 * Get the successor of <node> in <list>
 */
void *_mlNext(MList *list, MListNode *node);

/*
 * Get the predecessor of <node> in <list>
 */
void *_mlPrev(MList *list, MListNode *node);

/*
 * Get the first node in <list>
 */
void *mlHead(MList *list);

/*
 * Get the last node in <list>
 */
void *mlTail(MList *list);

/*
 * Append <node> to the end of <list>
 */
void _mlAppendTail(MList *list, MListNode *node);

/*
 * Insert <node> at the head of <list>
 */
void _mlInsertHead(MList *list, MListNode *node);

/*
 * Append <node> after <after> in <list>
 */
void _mlAppendAfter(MList *list, MListNode *node, MListNode *after);

/*
 * Insert <node> before <before> in <list>
 */
void _mlInsertBefore(MList *list, MListNode *node, MListNode *before);

/*
 * Remove <node> from <list>
 */
void _mlRemove(MList *list, MListNode *node);

/*
 * Remove first node from <list> and return its address
 */
void *mlRemoveHead(MList *list);

/*
 * Remove last node from <list> and return its address
 */
void *mlRemoveTail(MList *list);

/*
 * Return the number of lists that contain <node>
 */
int _mlCountContaining(MListNode *node);

/*
 * Return the length of <list>
 */
int mlLength(MList *list);

/*
 * Dump the contents of <list>, preceded by <str>, on <fp>
 */
void mlDump(FILE *fp, char *str, MList *list);

/*
 * Dump the contents of <node>, preceded by <str>, on <fp>
 */
void mlDumpItem(FILE *fp, char *str, MListNode *node);

/*
 * Sort <list>. Comparison function <cmp> should return a number less than, greater than or equal to
 * zero depending on whether <a> should be sorted before, after or at the same level as <b>.
 */
void mlSort(MList *list, int(*cmp)(const void *a, const void *b));

/*
 * Return TRUE if <list> is empty.
 */
int mlIsEmpty(MList *list);

/*
 * Return TRUE if <list> contains <node>.
 */
int _mlContains(MList *list, MListNode *node);

#endif
