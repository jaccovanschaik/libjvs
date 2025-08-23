#include "tree.c"
#include "utils.h"

static int errors = 0;

static void check_entry(Tree *tree,
        const void *data, const void *key, size_t key_size)
{
#if 0
    fprintf(stderr,
            "check_entry: key_size = %zd, key = \"%.*s\", data = \"%.*s\"\n",
            key_size,
            (int) key_size, (char *) key, (int) key_size, (char *) data);
#endif

    UNUSED(tree);

    make_sure_that(key_size == 3);

    make_sure_that(memcmp(data, key, 3) == 0);
}

int main(void)
{
    static const char *triple_a = "AAA";
    static const char *double_a = "AA";
    static const char *single_a = "A";
    static const char *empty = "";
    static const char *alternative = "Alternative";

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

    treeAdd(tree, alternative, STRING_KEY(alternative));

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == empty);
    make_sure_that(tree->branch_count == 1);

    make_sure_that(tree->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->data == single_a);
    make_sure_that(tree->branch[0]->branch_count == 2);

    make_sure_that(tree->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->data == double_a);
    make_sure_that(tree->branch[0]->branch[0]->branch_count == 1);

    make_sure_that(tree->branch[0]->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->data == triple_a);
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch == NULL);
    make_sure_that(tree->branch[0]->branch[0]->branch[0]->branch_count == 0);

    Tree *t = tree;

    for (const char *p = alternative; *p != '\0'; p++) {
        int i;

        for (i = 0; i < t->branch_count; i++) {
            if (t->branch[i]->id == *p) goto found_a_branch;
        }

        make_sure_that("Couldn't find a branch for '%c'\n" && 0);

found_a_branch:
        t = t->branch[i];
    }

    make_sure_that(treeGet(tree, STRING_KEY(empty)) == empty);
    make_sure_that(treeGet(tree, STRING_KEY(single_a)) == single_a);
    make_sure_that(treeGet(tree, STRING_KEY(double_a)) == double_a);
    make_sure_that(treeGet(tree, STRING_KEY(triple_a)) == triple_a);
    make_sure_that(treeGet(tree, STRING_KEY(alternative)) == alternative);

    treeDrop(tree, STRING_KEY(empty));

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == NULL);
    make_sure_that(tree->branch_count == 1);

    make_sure_that(tree->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->data == single_a);
    make_sure_that(tree->branch[0]->branch_count == 2);

    make_sure_that(tree->branch[0]->branch[0]->id == 'A');
    make_sure_that(tree->branch[0]->branch[0]->data == double_a);
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
    make_sure_that(tree->branch[0]->branch_count == 2);

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
    make_sure_that(tree->branch[0]->branch_count == 1);

    treeDrop(tree, STRING_KEY(single_a));

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == NULL);
    make_sure_that(tree->branch_count == 1);

    treeDrop(tree, STRING_KEY(alternative));

    make_sure_that(tree->id == '\0');
    make_sure_that(tree->data == NULL);
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

#if 0
                fprintf(stderr, "Adding \"%s\" to tree.\n", key);
#endif

                treeAdd(tree, key, key, 3);
            }
        }
    }

    for (TreeIter *iter = treeIterate(tree); iter; iter = treeIterNext(iter)) {
        size_t key_size;
        const char *key = treeIterKey(iter, &key_size);
        const void *data = treeGet(tree, key, key_size);

        check_entry(tree, data, key, key_size);
    }

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
