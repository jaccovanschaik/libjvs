/*
 * tree.c: Store data in an addressable tree structure.
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-11-07
 * Version:   $Id: tree.c 387 2019-12-15 17:47:27Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "tree.h"
#include "debug.h"

#include <stdint.h>
#include <string.h>

/*
 * Find the branch with id <id> in <tree>. Search only in the range of branches between <low_index> 
 * and <high_index>, inclusive.
 */
static int find_branch_between(const Tree *tree, uint8_t id, int low_index, int high_index)
{
    if (low_index > high_index) return -1;

    uint8_t low_id  = tree->branch[low_index]->id;
    uint8_t high_id = tree->branch[high_index]->id;

    if (id < low_id)  return -1;
    if (id > high_id) return -1;

    if (id == low_id)  return low_index;
    if (id == high_id) return high_index;

    int middle_index = (low_index + high_index) / 2;

    uint8_t middle_id = tree->branch[middle_index]->id;

    if (id == middle_id)
        return middle_index;
    else if (id < middle_id)
        return find_branch_between(tree, id, low_index + 1, middle_index - 1);
    else
        return find_branch_between(tree, id, middle_index + 1, high_index - 1);
}
/*
 * Insert branch <branch> into <tree> at index <at_index>.
 */
static void insert_branch(Tree *tree, Tree *branch, int at_index)
{
    tree->branch = realloc(tree->branch,
            (tree->branch_count + 1) * sizeof(Tree *));

    memmove(tree->branch + at_index + 1,
            tree->branch + at_index,
            (tree->branch_count - at_index) * sizeof(Tree *));

    tree->branch[at_index] = branch;

    tree->branch_count++;
}

/*
 * Remove and return the branch at <at_index> in <tree>.
 */
static Tree *remove_branch(Tree *tree, int at_index)
{
    Tree *branch = tree->branch[at_index];

    for (int i = 0; i < branch->branch_count; i++) {
        remove_branch(branch, i);
    }

    tree->branch_count--;

    memmove(tree->branch + at_index,
            tree->branch + at_index + 1,
            (tree->branch_count - at_index) * sizeof(Tree *));

    tree->branch = realloc(tree->branch,
            tree->branch_count * sizeof(Tree *));

    return branch;
}

/*
 * Find and return the branch with id <id> in <tree>.
 */
static int find_branch(const Tree *tree, uint8_t id)
{
    return find_branch_between(tree, id, 0, tree->branch_count - 1);
}

/*
 * Insert and return an empty branch with id <id> into <tree>.
 */
static Tree *add_branch(Tree *tree, uint8_t id)
{
    Tree *branch = calloc(1, sizeof(*branch));

    branch->id = id;

    int i;

    for (i = 0; i < tree->branch_count; i++) {
        if (tree->branch[i]->id > id) break;
    }

    insert_branch(tree, branch, i);

    return branch;
}

/*
 * Find and return the leaf for key <key> (with size <key_size>) in <tree>. The
 * key contains the path through the tree from this location, so it may be the
 * end portion of a full key.
 */
static Tree *find_leaf(Tree *tree, const void *key, size_t key_size)
{
    if (key_size == 0) return tree;

    uint8_t id = ((uint8_t *) key)[0];

    int index = find_branch(tree, id);

    if (index == -1) {
        return NULL;
    }
    else {
        return find_leaf(tree->branch[index], ((uint8_t *) key) + 1, key_size - 1);
    }
}

/*
 * Find and return the leaf for key <key> (with length <key_size>) in <tree>,
 * and create it if it doesn't exist. As above, the key may be the end portion
 * of a longer key that describes the path to go from this location to the leaf.
 */
static Tree *find_or_add_leaf(Tree *tree, const void *key, size_t key_size)
{
    if (key_size == 0) return tree;

    uint8_t id = ((uint8_t *) key)[0];

    int index = find_branch(tree, id);

    Tree *branch;

    if (index == -1) {
        branch = add_branch(tree, id);
    }
    else {
        branch = tree->branch[index];
    }

    return find_or_add_leaf(branch, ((uint8_t *) key) + 1, key_size - 1);
}

/*
 * Delete the leaf for key <key> (with length <key_size>) in <tree>. Again, the
 * key may be the end portion of a longer key that describes the path to go from
 * this location to the leaf. Returns 0 if the leaf was found, 1 if it wasn't.
 */
static int delete_leaf(Tree *tree, const void *key, size_t key_size)
{
    if (key_size == 0) {
        if (tree->data == NULL) {
            return 1;
        }
        else {
            tree->data = NULL;
            return 0;
        }
    }

    uint8_t id = ((uint8_t *) key)[0];

    int index = find_branch(tree, id);

    if (index == -1) return 1;

    Tree *branch = tree->branch[index];

    if (delete_leaf(branch, ((uint8_t *) key) + 1, key_size - 1) != 0) {
        return 1;
    }
    else if (branch->data == NULL && branch->branch_count == 0) {
        remove_branch(tree, index);
        free(branch);
    }

    return 0;
}

/*
 * Helper function for treeTraverse. Traverse the tree that has its root at
 * <root>, starting at <branch>. The key that lead us to <branch> is <key> with
 * size <key_size>. Call function <func> for all branches (including this one)
 * if they have data associated with them.
 */
static void tree_traverse(Tree *root, Tree *branch, uint8_t *key, size_t key_size,
        int (*func)(Tree *tree, void *data, const void *key, size_t key_size))
{
    if (branch->data != NULL) {
        func(branch, (void *) branch->data, key, key_size);
    }

    uint8_t *sub_key = calloc(key_size + 1, sizeof(uint8_t));

    memcpy(sub_key, key, key_size);

    for (int i = 0; i < branch->branch_count; i++) {
        sub_key[key_size] = branch->branch[i]->id;

        tree_traverse(root, branch->branch[i], sub_key, key_size + 1, func);
    }

    free(sub_key);
}

/*
 * Create an empty tree.
 */
Tree *treeCreate(void)
{
    Tree *tree = calloc(1, sizeof(*tree));

    return tree;
}

/*
 * Add the data at <data> to the tree, addressable using <key> (which has size
 * <key_size>).
 */
void treeAdd(Tree *tree, const void *data, const void *key, size_t key_size)
{
    Tree *leaf = find_or_add_leaf(tree, key, key_size);

    dbgAssert(stderr, leaf->data == NULL, "Key already used.\n");

    leaf->data = data;
}

/*
 * Return the data pointer that was associated earlier with <key> (which has
 * size <key_size>).
 */
void *treeGet(Tree *tree, const void *key, size_t key_size)
{
    Tree *leaf = find_leaf(tree, key, key_size);

    return leaf ? (void *) leaf->data : NULL;
}

/*
 * Change the data addressed by <key> (with size <key_size>) to <data>.
 */
void treeSet(Tree *tree, const void *data, const void *key, size_t key_size)
{
    Tree *leaf = find_leaf(tree, key, key_size);

    dbgAssert(stderr, leaf != NULL && leaf->data != NULL, "Key doesn't exist.\n");

    leaf->data = data;
}

/*
 * Traverse the tree at <tree>. Function <func> will be called for all data
 * items in the tree, passing in a pointer to this tree and to the data for the
 * item, and also the key and key size for the item.
 */
void treeTraverse(Tree *tree,
        int (*func)(Tree *tree, void *data, const void *key, size_t key_size))
{
    tree_traverse(tree, tree, (uint8_t *) "", 0, func);
}

/*
 * Drop the association of key <key> (with size <key_size>) with its data from
 * <tree>. The data itself is not touched.
 */
void treeDrop(Tree *tree, const void *key, size_t key_size)
{
    int r = delete_leaf(tree, key, key_size);

    dbgAssert(stderr, r == 0, "Key doesn't exist.\n");
}

/*
 * Clear the contents of <tree> but without freeing <tree> itself. It is
 * returned to the state just after treeCreate(). The data referenced by the
 * tree is unaffected.
 */
void treeClear(Tree *tree)
{
    for (int i = 0; i < tree->branch_count; i++) {
        treeDestroy(tree->branch[i]);
    }

    free(tree->branch);

    tree->data = NULL;
    tree->branch = NULL;
    tree->branch_count = 0;
}

/*
 * Destroy the entire data tree <tree>. The data that was entered into it
 * earlier is unaffected.
 */
void treeDestroy(Tree *tree)
{
    treeClear(tree);

    free(tree);
}

#ifdef TEST
#include "utils.h"

static int errors = 0;

static int check_entry(Tree *tree, void *data, const void *key, size_t key_size)
{
    make_sure_that(key_size == 3);

    make_sure_that(memcmp(data, key, 3) == 0);

    return 0;
}

int main(int argc, char *argv[])
{
    static const char *triple_a = "AAA";
    static const char *double_a = "AA";
    static const char *single_a = "A";
    static const char *empty = "";
    static const char *alternative = "alternative";

    Tree *tree = treeCreate();

    make_sure_that(tree != NULL);

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == NULL);
    make_sure_that(tree->branch == NULL);
    make_sure_that(tree->branch_count == 0);

    treeAdd(tree, double_a, STRING_KEY(double_a));

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == NULL);
    make_sure_that(tree->branch_count == 1);

    make_sure_that(tree->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->data == NULL);
    make_sure_that(tree->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->data == double_a);
    make_sure_that(tree->branch[0]->branch[0]->branch == NULL);
    make_sure_that(tree->branch[0]->branch[0]->branch_count == 0);

    treeAdd(tree, triple_a, STRING_KEY(triple_a));

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == NULL);
    make_sure_that(tree->branch_count == 1);

    make_sure_that(tree->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->data == NULL);
    make_sure_that(tree->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->data == double_a);
    make_sure_that(tree->branch[0]->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->data == triple_a);
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch == NULL);
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch_count == 0);

    treeAdd(tree, single_a, STRING_KEY(single_a));

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == NULL);
    make_sure_that(tree->branch_count == 1);

    make_sure_that(tree->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->data == single_a);
    make_sure_that(tree->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->data == double_a);
    make_sure_that(tree->branch[0]->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->data == triple_a);
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch == NULL);
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch_count == 0);

    treeAdd(tree, empty, STRING_KEY(empty));

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == empty);
    make_sure_that(tree->branch_count == 1);

    make_sure_that(tree->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->data == single_a);
    make_sure_that(tree->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->data == double_a);
    make_sure_that(tree->branch[0]->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->data == triple_a);
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch == NULL);
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch_count == 0);

    treeSet(tree, alternative, STRING_KEY(double_a));

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == empty);
    make_sure_that(tree->branch_count == 1);

    make_sure_that(tree->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->data == single_a);
    make_sure_that(tree->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->data == alternative);
    make_sure_that(tree->branch[0]->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->data == triple_a);
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch == NULL);
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch_count == 0);

    make_sure_that(treeGet(tree, STRING_KEY(double_a)) == alternative);

    treeDrop(tree, STRING_KEY(empty));

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == NULL);
    make_sure_that(tree->branch_count == 1);

    make_sure_that(tree->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->data == single_a);
    make_sure_that(tree->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->data == alternative);
    make_sure_that(tree->branch[0]->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->data == triple_a);
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch == NULL);
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch_count == 0);

    treeDrop(tree, STRING_KEY(double_a));

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == NULL);
    make_sure_that(tree->branch_count == 1);

    make_sure_that(tree->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->data == single_a);
    make_sure_that(tree->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->data == NULL);
    make_sure_that(tree->branch[0]->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->data == triple_a);
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch == NULL);
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch_count == 0);

    treeDrop(tree, STRING_KEY(triple_a));

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == NULL);
    make_sure_that(tree->branch_count == 1);

    make_sure_that(tree->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->data == single_a);
    make_sure_that(tree->branch[0]->branch == NULL);
    make_sure_that(tree->branch[0]->branch_count == 0);

    treeDrop(tree, STRING_KEY(single_a));

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == NULL);
    make_sure_that(tree->branch == NULL);
    make_sure_that(tree->branch_count == 0);

    treeDestroy(tree);

    tree = treeCreate();

    treeAdd(tree, double_a, STRING_KEY(double_a));
    treeAdd(tree, triple_a, STRING_KEY(triple_a));
    treeAdd(tree, single_a, STRING_KEY(single_a));
    treeAdd(tree, empty, STRING_KEY(empty));

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == empty);
    make_sure_that(tree->branch_count == 1);

    make_sure_that(tree->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->data == single_a);
    make_sure_that(tree->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->data == double_a);
    make_sure_that(tree->branch[0]->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->data == triple_a);
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch == NULL);
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch_count == 0);

    treeClear(tree);

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == NULL);
    make_sure_that(tree->branch == NULL);
    make_sure_that(tree->branch_count == 0);

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++) {
                char *key = calloc(4, 1);

                key[0] = 'A' + i;
                key[1] = 'A' + j;
                key[2] = 'A' + k;

                treeAdd(tree, key, key, 3);
            }
        }
    }

    treeTraverse(tree, check_entry);

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++) {
                char key[] = { 'A' + i, 'A' + j, 'A' + k };

                char *data = treeGet(tree, key, 3);

                make_sure_that(memcmp(data, key, 3) == 0);

                treeDrop(tree, key, 3);

                free(data);
            }
        }
    }

    free(tree);

    return errors;
}
#endif
