#ifndef TREE_H
#define TREE_H

/*
 * tree.h: Store data in an addressable tree structure.
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-11-07
 * Version:   $Id: tree.h 366 2019-11-09 20:00:50Z jacco $
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
 * <key_len>).
 */
void treeAdd(Tree *tree, const void *data, const void *key, size_t key_len);

/*
 * Return the data pointer that was associated earlier with <key> (which has
 * length <key_len>).
 */
void *treeGet(Tree *tree, const void *key, size_t key_len);

/*
 * Change the data addressed by <key> (with length <key_len>) to <data>.
 */
void treeSet(Tree *tree, const void *data, const void *key, size_t key_len);

/*
 * Drop the association of key <key> (with length <key_len>) with its data from
 * <tree>. The data itself is not touched.
 */
void treeDrop(Tree *tree, const void *key, size_t key_len);

/*
 * Destroy the entire data tree <tree>. The data that was entered into it
 * earlier remains untouched.
 */
void treeDestroy(Tree *tree);

#ifdef __cplusplus
}
#endif

#endif
