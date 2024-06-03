#ifndef HASHLIST_H
#define HASHLIST_H

/*
 * hashlist.h: Combine the best of hashes and linked lists!
 *
 * Hash tables are great for random access. Linked lists are great for
 * sequential access. But what if you have data in a hash table that you
 * sometimes want to access sequentially? Try the hashlist!
 *
 * hashlist.h is part of libjvs.
 *
 * Copyright: (c) 2019-2024 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-01-22
 * Version:   $Id: hashlist.h 497 2024-06-03 12:37:20Z jacco $
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

#define hlAdd(hl, n, key, len)  _hlAdd((hl), &(n)->_node, key, len)
#define hlSet(hl, n, key, len)  _hlSet((hl), &(n)->_node, key, len)
#define hlNext(n)               _hlNext(&(n)->_node)
#define hlPrev(n)               _hlPrev(&(n)->_node)

/*
 * Create a new, empty hashlist.
 */
HashList *hlCreate(void);

/*
 * Initialize the hashlist specified by <hashlist>.
 */
void hlInitialize(HashList *hashlist);

/*
 * Add an entry that points to <node> to <hashlist>. Associate it with <key>,
 * whose length is <key_len>. <hashlist>, <hashlist> and <key> must not be NULL,
 * <key_len> must be greater than 0. If an entry with the same key already
 * exists this function calls abort(). The node is added to the end of the
 * embedded list.
 */
void _hlAdd(HashList *hashlist, ListNode *node, const void *key, int key_len);

/*
 * Set the existing entry in <hashlist> for <key>, whose length is <key_len>, to
 * <node>. <hashlist>, <node> and <key> must not be NULL, <key_len> must be
 * greater than 0. If no such entry exists this function calls abort().
 */
void _hlSet(HashList *hashlist, ListNode *node, const void *key, int key_len);

/*
 * Return TRUE if <hashlist> has an entry for <key> with length <key_len>, FALSE
 * otherwise. <hashlist> and <key> must not be NULL, <key_len> must be greater
 * than 0.
 */
int hlContains(const HashList *hashlist, const void *key, int key_len);

/*
 * Get the data associated with <key>, whose length is <key_len> from
 * <hashlist>. If no such entry exists NULL is returned. <hashlist> and <key>
 * must not be NULL, <key_len> must be greater than 0.
 */
void *hlGet(HashList *hashlist, const void *key, int key_len);

/*
 * Delete the entry in <hashlist> for <key> with length <key_len>. <hashlist>
 * and <key> must not be NULL, <key_len> must be greater than 0. If no such
 * entry exists this function calls abort().
 */
void hlDel(HashList *hashlist, const void *key, int key_len);

/*
 * Return the node at the head of <hashlist>.
 */
void *hlHead(const HashList *hashlist);

/*
 * Return the node at the end of <hashlist>.
 */
void *hlTail(const HashList *hashlist);

/*
 * Return the node before <node>.
 */
void *_hlPrev(const ListNode *node);

/*
 * Return the node following <node>.
 */
void *_hlNext(const ListNode *node);

/*
 * Return TRUE if <hashlist> is empty.
 */
int hlIsEmpty(const HashList *hashlist);

/*
 * Sort <hashlist> using comparison function <cmp>.
 */
void hlSort(HashList *hashlist, int(*cmp)(const void *, const void *));

/*
 * Clear hashlist <hashlist> i.e. remove all its entries. The user data that the
 * entries point to is *not* removed.
 */
void hlClear(HashList *hashlist);

/*
 * Delete hashlist <hashlist> and its contents. The user data that its entries
 * point to is *not* removed.
 */
void hlDestroy(HashList *hashlist);

#endif
