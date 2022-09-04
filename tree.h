#ifndef TREE_H
#define TREE_H

/*
 * tree.h: Store data in an addressable tree structure.
 *
 * Copyright: (c) 2019-2022 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-11-07
 * Version:   $Id: tree.h 465 2022-09-04 22:08:30Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Tree Tree;
typedef struct TreeIter TreeIter;

struct Tree {
    uint8_t id;
    const void *data;
    Tree **branch;
    int branch_count;
};

#define STRING_KEY(s) s, strlen(s)
#define VALUE_KEY(k)  &(k), sizeof(k)

#define treeAddS(t, d, s)   treeAdd(t, d, s, strlen(s))
#define treeSetS(t, d, s)   treeSet(t, d, s, strlen(s))
#define treeGetS(t, s)      treeGet(t, s, strlen(s))
#define treeDropS(t, s)     treeDrop(t, s, strlen(s))

/*
 * Create an empty tree.
 */
Tree *treeCreate(void);

/*
 * Add the data at <data> to the tree, addressable using <key> (which has size
 * <key_size>).
 */
void treeAdd(Tree *tree, const void *data, const void *key, size_t key_size);

/*
 * Return the data pointer that was associated earlier with <key> (which has
 * size <key_size>).
 */
void *treeGet(Tree *tree, const void *key, size_t key_size);

/*
 * Change the data addressed by <key> (with size <key_size>) to <data>.
 */
void treeSet(Tree *tree, const void *data, const void *key, size_t key_size);

/*
 * Create an iterator that we can use to traverse <tree>. This function
 * returns an iterator that "points to" the first leaf in the tree, or NULL if
 * the tree has no leaves at all. Use treeIterKey to get the key and key_size
 * for this leaf. Use treeIterNext to proceed to the next leaf.
 */
TreeIter *treeIterate(const Tree *tree);

/*
 * Update <iter> to point to the next leaf in the tree, and return it. If
 * there are no more leaves, the iterator is discarded, NULL is returned and
 * the iterator can no longer be used. If you want to break off *before* this
 * happens you can use treeIterStop to discard the iterator yourself.
 */
TreeIter *treeIterNext(TreeIter *iter);

/*
 * Return a pointer to the key for the data item that <iter> currently points
 * to. The length of the key is returned through <key_size>.
 */
const void *treeIterKey(TreeIter *iter, size_t *key_size);

/*
 * Stop iterating using <iter>. <iter> is discarded and can no longer be used
 * after calling this function.
 */
void treeIterStop(TreeIter *iter);

/*
 * Drop the association of key <key> (with size <key_size>) with its data from
 * <tree>. The data itself is not touched.
 */
void treeDrop(Tree *tree, const void *key, size_t key_size);

/*
 * Clear the contents of <tree> but without freeing <tree> itself. It is
 * returned to the state just after treeCreate(). The data referenced by the
 * tree is unaffected.
 */
void treeClear(Tree *tree);

/*
 * Destroy the entire data tree <tree>. The data that was entered into it
 * earlier is unaffected.
 */
void treeDestroy(Tree *tree);

#ifdef __cplusplus
}
#endif

#endif
