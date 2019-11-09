/*
 * hashlist.c: Combine the best of hashes and linked lists!
 *
 * Hash tables are great for random access. Linked lists are great for
 * sequential access. But what if you have data in a hash table that you
 * sometimes want to access sequentially? Try the hashlist!
 *
 * hashlist.c is part of libjvs.
 *
 * Copyright: (c) 2019-2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-01-22
 * Version:   $Id: hashlist.c 367 2019-11-09 20:04:24Z jacco $
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
 * exists this function calls abort(). The node is added to the end of the
 * embedded list.
 */
void _hlAdd(HashList *hashlist, ListNode *node, const void *key, int key_len)
{
    hashAdd(&hashlist->hash, node, key, key_len);

    _listAppendTail(&hashlist->list, node);
}

/*
 * Set the existing entry in <hashlist> for <key>, whose length is <key_len>, to
 * <node>. <hashlist>, <node> and <key> must not be NULL, <key_len> must be
 * greater than 0. If no such entry exists this function calls abort().
 */
void _hlSet(HashList *hashlist, ListNode *node, const void *key, int key_len)
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

    hashDrop(&hashlist->hash, key, key_len);

    _listRemove(&hashlist->list, node);
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
void *_hlPrev(const ListNode *node)
{
    return _listPrev(node);
}

/*
 * Return the node following <node>.
 */
void *_hlNext(const ListNode *node)
{
    return _listNext(node);
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

/*
 * Clear hashlist <hashlist> i.e. remove all its entries. The user data that the
 * entries point to is *not* removed.
 */
void hlClear(HashList *hashlist)
{
    ListNode *ptr;

    while ((ptr = listRemoveHead(&hashlist->list)) != NULL) ;

    hashClear(&hashlist->hash);
}

/*
 * Delete hashlist <hashlist> and its contents. The user data that its entries
 * point to is *not* removed.
 */
void hlDestroy(HashList *hashlist)
{
    hlClear(hashlist);

    free(hashlist);
}
