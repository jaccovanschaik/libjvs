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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "list.h"
#include "hash.h"

#include "hashlist.h"

#define hlAdd(hl, n, key, len)  f_hlAdd((hl), &(n)->_node, key, len)
#define hlSet(hl, n, key, len)  f_hlSet((hl), &(n)->_node, key, len)
#define hlNext(n)               f_hlNext(&(n)->_node)
#define hlPrev(n)               f_hlPrev(&(n)->_node)

/*
 * Create a new, empty hashlist.
 */
HashList *hlCreate(void)
{
    return calloc(1, sizeof(HashList));
}

/*
 * Initialize the hashlist specified by <hashlist>.
 */
void hlInitialize(HashList *hashlist)
{
    memset(hashlist, 0, sizeof(*hashlist));
}

/*
 * Add an entry that points to <node> to <hashlist>. Associate it with <key>,
 * whose length is <key_len>. <hashlist>, <hashlist> and <key> must not be NULL,
 * <key_len> must be greater than 0. If an entry with the same key already
 * exists this function calls abort().
 */
void f_hlAdd(HashList *hashlist, ListNode *node, const void *key, int key_len)
{
    hashAdd(&hashlist->hash, node, key, key_len);

    f_listAppendTail(&hashlist->list, node);
}

/*
 * Set the existing entry in <hashlist> for <key>, whose length is <key_len>, to
 * <node>. <hashlist>, <node> and <key> must not be NULL, <key_len> must be
 * greater than 0. If no such entry exists this function calls abort().
 */
void f_hlSet(HashList *hashlist, ListNode *node, const void *key, int key_len)
{
    hashSet(&hashlist->hash, node, key, key_len);
}

/*
 * Return TRUE if <hashlist> has an entry for <key> with length <key_len>, FALSE
 * otherwise. <hashlist> and <key> must not be NULL, <key_len> must be greater
 * than 0.
 */
int hlContains(const HashList *hashlist, const void *key, int key_len)
{
    return hashContains(&hashlist->hash, key, key_len);
}

/*
 * Get the data associated with <key>, whose length is <key_len> from
 * <hashlist>. If no such entry exists NULL is returned. <hashlist> and <key>
 * must not be NULL, <key_len> must be greater than 0.
 */
void *hlGet(HashList *hashlist, const void *key, int key_len)
{
    return hashGet(&hashlist->hash, key, key_len);
}

/*
 * Delete the entry in <hashlist> for <key> with length <key_len>. <hashlist>
 * and <key> must not be NULL, <key_len> must be greater than 0. If no such
 * entry exists this function calls abort().
 */
void hlDel(HashList *hashlist, const void *key, int key_len)
{
    ListNode *node = hashGet(&hashlist->hash, key, key_len);

    assert(node);

    hashDel(&hashlist->hash, key, key_len);

    f_listRemove(&hashlist->list, node);
}

/*
 * Return the node at the head of <hashlist>.
 */
void *hlHead(const HashList *hashlist)
{
    return listHead(&hashlist->list);
}

/*
 * Return the node at the end of <hashlist>.
 */
void *hlTail(const HashList *hashlist)
{
    return listTail(&hashlist->list);
}

/*
 * Return the node before <node>.
 */
void *f_hlPrev(const ListNode *node)
{
    return f_listPrev(node);
}

/*
 * Return the node following <node>.
 */
void *f_hlNext(const ListNode *node)
{
    return f_listNext(node);
}

/*
 * Return TRUE if <hashlist> is empty.
 */
int hlIsEmpty(const HashList *hashlist)
{
    return listIsEmpty(&hashlist->list);
}

/*
 * Sort <hashlist> using comparison function <cmp>.
 */
void hlSort(HashList *hashlist, int(*cmp)(const void *, const void *))
{
    listSort(&hashlist->list, cmp);
}
