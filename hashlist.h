#ifndef HASHLIST_H
#define HASHLIST_H

/*
 * hashlist.c: Combine the best of hashes and linked lists!
 *
 * Hash tables are great for random access. Linked lists are great for
 * sequential access. But what if you have data in a hash table that you
 * sometimes want to access sequentially? Try the hashlist!
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-01-22
 * Version:   $Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "list.h"
#include "hash.h"

typedef struct {
    List list;
    HashTable hash;
} HashList;

/* Create a new, empty hashlist. */
HashList *hlCreate(void);

/* Initialize the hashlist specified by <hashlist>. */
void hlInitialize(HashList *hashlist);

/* Append <node> to the tail of hashlist <hashlist>. */
void f_hlAdd(HashList *hashlist, ListNode *node, const void *key, int key_len);

/*
 * Set the existing entry in <tbl> for <key>, whose length is <key_len>, to
 * <data>. <tbl>, <data> and <key> must not be NULL, <key_len> must
 * be greater than 0. If no such entry exists this function calls abort().
 */
void f_hlSet(HashList *hashlist, ListNode *node, const void *key, int key_len);

/*
 * Return TRUE if <tbl> has an entry for <key> with length <key_len>, FALSE otherwise. <tbl> and
 * <key> must not be NULL, <key_len> must be greater than 0.
 */
int hlContains(const HashList *hashlist, const void *key, int key_len);

/*
 * Get the data associated with <key>, whose length is <key_len> from <tbl>. If
 * no such entry exists NULL is returned. <tbl> and <key> must not be NULL,
 * <key_len> must be greater than 0.
 */
void *hlGet(HashList *hashlist, const void *key, int key_len);

/*
 * Delete the entry in <tbl> for <key> with length <key_len>. <tbl> and <key>
 * must not be NULL, <key_len> must be greater than 0. If no such entry exists
 * this function calls abort().
 */
void hlDel(HashList *hashlist, const void *key, int key_len);

/* Return the node at the head of <hashlist>. */
void *hlHead(const HashList *hashlist);

/* Return the node at the end of <hashlist>. */
void *hlTail(const HashList *hashlist);

/* Return the node before <node>. */
void *f_hlPrev(const ListNode *node);

/* Return the node following <node>. */
void *f_hlNext(const ListNode *node);

/* Return TRUE if <hashlist> is empty. */
int hlIsEmpty(const HashList *hashlist);

/* Sort <hashlist> using comparison function <cmp>. */
void hlSort(HashList *hashlist, int(*cmp)(const void *, const void *));

#endif
