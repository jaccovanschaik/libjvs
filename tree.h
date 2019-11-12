#ifndef TREE_H
#define TREE_H

/*
 * tree.h: Store data in an addressable tree structure.
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-11-07
 * Version:   $Id: tree.h 374 2019-11-12 07:42:17Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Tree Tree;

#define STRING_KEY(s) s, strlen(s)
#define VALUE_KEY(k)  &(k), sizeof(k)

/*
 * Create an empty tree.
 */
Tree *treeCreate(void);

/*
 * Add the data at <data> to the tree, addressable using <key> (which has length
 * <key_size>).
 */
void treeAdd(Tree *tree, const void *data, const void *key, size_t key_size);

/*
 * Return the data pointer that was associated earlier with <key> (which has
 * length <key_size>).
 */
void *treeGet(Tree *tree, const void *key, size_t key_size);

/*
 * Change the data addressed by <key> (with length <key_size>) to <data>.
 */
void treeSet(Tree *tree, const void *data, const void *key, size_t key_size);

/*
 * Traverse the tree at <tree>. Function <func> will be called for all data
 * items in the tree, passing in a pointer to this tree and to the data for the
 * item, and also the key and key length for the item.
 */
void treeTraverse(Tree *tree,
        int (*func)(Tree *tree, void *data, const void *key, size_t key_size));

/*
 * Drop the association of key <key> (with length <key_size>) with its data from
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
