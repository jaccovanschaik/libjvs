/*
 * tree.c: Store data in an addressable tree structure.
 *
 * Copyright: (c) 2019 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2019-11-07
 * Version:   $Id: tree.c 366 2019-11-09 20:00:50Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "tree.h"
#include "debug.h"

#include <stdint.h>
#include <string.h>

struct Tree {
    uint8_t id;
    const void *data;
    Tree **branch;
    int branch_count;
};

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

static int find_branch(const Tree *tree, uint8_t id)
{
    return find_branch_between(tree, id, 0, tree->branch_count - 1);
}

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

static Tree *find_leaf(Tree *tree, const void *key, size_t key_len)
{
    if (key_len == 0) return tree;

    uint8_t id = ((uint8_t *) key)[0];

    int index = find_branch(tree, id);

    if (index == -1) {
        return NULL;
    }
    else {
        return find_leaf(tree->branch[index], ((uint8_t *) key) + 1, key_len - 1);
    }
}

static Tree *find_or_add_leaf(Tree *tree, const void *key, size_t key_len)
{
    if (key_len == 0) return tree;

    uint8_t id = ((uint8_t *) key)[0];

    int index = find_branch(tree, id);

    Tree *branch;

    if (index == -1) {
        branch = add_branch(tree, id);
    }
    else {
        branch = tree->branch[index];
    }

    return find_or_add_leaf(branch, ((uint8_t *) key) + 1, key_len - 1);
}

static int delete_leaf(Tree *tree, const void *key, size_t key_len)
{
    if (key_len == 0) {
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

    if (delete_leaf(branch, ((uint8_t *) key) + 1, key_len - 1) != 0) {
        return 1;
    }
    else if (branch->data == NULL && branch->branch_count == 0) {
        remove_branch(tree, index);
        free(branch);
    }

    return 0;
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
 * Add the data at <data> to the tree, addressable using <key> (which has length
 * <key_len>).
 */
void treeAdd(Tree *tree, const void *data, const void *key, size_t key_len)
{
    Tree *leaf = find_or_add_leaf(tree, key, key_len);

    dbgAssert(stderr, leaf->data == NULL, "Key already used.\n");

    leaf->data = data;
}

/*
 * Return the data pointer that was associated earlier with <key> (which has
 * length <key_len>).
 */
void *treeGet(Tree *tree, const void *key, size_t key_len)
{
    Tree *leaf = find_leaf(tree, key, key_len);

    return leaf ? (void *) leaf->data : NULL;
}

/*
 * Change the data addressed by <key> (with length <key_len>) to <data>.
 */
void treeSet(Tree *tree, const void *data, const void *key, size_t key_len)
{
    Tree *leaf = find_leaf(tree, key, key_len);

    dbgAssert(stderr, leaf != NULL && leaf->data != NULL, "Key doesn't exist.\n");

    leaf->data = data;
}

/*
 * Drop the association of key <key> (with length <key_len>) with its data from
 * <tree>. The data itself is not touched.
 */
void treeDrop(Tree *tree, const void *key, size_t key_len)
{
    int r = delete_leaf(tree, key, key_len);

    dbgAssert(stderr, r == 0, "Key doesn't exist.\n");
}

/*
 * Destroy the entire data tree <tree>. The data that was entered into it
 * earlier remains untouched.
 */
void treeDestroy(Tree *tree)
{
    for (int i = 0; i < tree->branch_count; i++) {
        treeDestroy(tree->branch[i]);
    }

    free(tree->branch);
    free(tree);
}

#ifdef TEST
#include "utils.h"

static int errors = 0;

int main(int argc, char *argv[])
{
    static const char *triple_a = "AAA";
    static const char *double_a = "AA";
    static const char *single_a = "A";
    static const char *empty = "";
    static const char *alternative = "alternative";

    Tree *tree = treeCreate();

    make_sure_that(tree != NULL);

    make_sure_that(tree->branch_count == 0);

    treeAdd(tree, double_a, STRING_KEY(double_a));

    make_sure_that(tree->branch_count == 1);

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == NULL);
    make_sure_that(tree->branch_count == 1);

    make_sure_that(tree->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->data == NULL);
    make_sure_that(tree->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->data == double_a);
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
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch_count == 0);

    treeAdd(tree, empty, empty, 0);

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
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch_count == 0);

    treeSet(tree, alternative, empty, 0);

    make_sure_that(treeGet(tree, empty, 0) == alternative);

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == alternative);
    make_sure_that(tree->branch_count == 1);

    make_sure_that(tree->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->data == single_a);
    make_sure_that(tree->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->data == double_a);
    make_sure_that(tree->branch[0]->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->data == triple_a);
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch_count == 0);

    treeDrop(tree, empty, 0);

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
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch_count == 0);

    treeDrop(tree, STRING_KEY(triple_a));

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == NULL);
    make_sure_that(tree->branch_count == 1);

    make_sure_that(tree->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->data == single_a);
    make_sure_that(tree->branch[0]->branch_count == 0);

    treeDestroy(tree);

    return errors;
}
#endif
