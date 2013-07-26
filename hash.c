/*
 * Provides hash tables.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "list.h"

#include "hash.h"

/* An entry in a hash table. */

struct HashEntry {
    ListNode _node;     /* Make it listable. */
    const void *data;   /* Pointer to some data. */
    void *key;          /* Points to the associated key. */
    int key_len;        /* Length of the key. */
};

/* Definition of a hash key. Depending on the number of significant bits, an
 * unsigned integer type is picked from stdint.h. The maximum theoretical number
 * of bits is 64. Theoretical, because that would result in a hash table with
 * 18.446.744.073.709.551.616 buckets. */

#if HASH_BITS <= 8
typedef uint8_t HashKey;
#elif HASH_BITS <= 16
typedef uint16_t HashKey;
#elif HASH_BITS <= 32
typedef uint32_t HashKey;
#elif HASH_BITS <= 64
typedef uint64_t HashKey;
#else
#error "Too many bits in hash key"
#endif

/*
 * Return a hash key for <key>, which is <key_len> characters long.
 */
static HashKey hash(const char *key, int key_len)
{
   int i;
   HashKey hash_key = 1;

   for (i = 0; i < key_len; i++) {
     hash_key = hash_key * 317 + (unsigned char) key[i];
   }

   return hash_key & (HASH_BUCKETS - 1);
}

/*
 * Find the entry for <key> with length <key_len> in <bucket>.
 */
static HashEntry *find_entry_in_bucket(const List *bucket, const char *key, int key_len)
{
   HashEntry *entry;

   for (entry = listHead(bucket); entry; entry = listNext(entry)) {
     if (memcmp(entry->key, key, key_len) == 0) return entry;
   }

   return NULL;
}

/*
 * Initialize hash table <table>.
 */
void hashInitTable(HashTable *table)
{
    memset(table, 0, sizeof(HashTable));
}

/*
 * Create a new hash table.
 */
HashTable *hashCreateTable(void)
{
   return calloc(1, sizeof(HashTable));
}

/*
 * Clear table <tbl> i.e. remove all its entries. The user data that the
 * entries point to is *not* removed.
 */
void hashClearTable(HashTable *tbl)
{
   int i;
   HashEntry *entry;

   for (i = 0; i < HASH_BUCKETS; i++) {
      while ((entry = listRemoveHead(&tbl->bucket[i])) != NULL) {
         free(entry->key);
         free(entry);
      }
   }
}

/*
 * Delete hash table <tbl> and its contents. The user data that its
 * entries point to is *not* removed.
 */
void hashDeleteTable(HashTable *tbl)
{
    hashClearTable(tbl);

    free(tbl);
}

/*
 * Add an entry that points to <data> to <tbl>. Associate it with <key>, whose
 * length is <key_len>. <tbl>, <data> and <key> must not be NULL, <key_len> must
 * be greater than 0. If an entry with the same key already exists this function
 * calls abort().
 */
void hashAdd(HashTable *tbl, const void *data, const void *key, int key_len)
{
   HashEntry *entry;
   HashKey hash_key;

   assert(data != NULL);
   assert(key  != NULL);
   assert(key_len > 0);

   hash_key = hash(key, key_len);

   if (find_entry_in_bucket(&tbl->bucket[hash_key], key, key_len) != NULL)
      abort();    /* Entry *must not* exist. */

   entry = calloc(1, sizeof(HashEntry));

   entry->data    = data;
   entry->key     = malloc(key_len);
   entry->key_len = key_len;

   memcpy(entry->key, key, key_len);

   listAppendTail(&tbl->bucket[hash_key], entry);
}

/*
 * Set the existing entry in <tbl> for <key>, whose length is <key_len>, to
 * <data>. <tbl>, <data> and <key> must not be NULL, <key_len> must
 * be greater than 0. If no such entry exists this function calls abort().
 */
void hashSet(HashTable *tbl, const void *data, const void *key, int key_len)
{
   HashEntry *entry;
   HashKey hash_key;

   assert(data != NULL);
   assert(key  != NULL);
   assert(key_len > 0);

   hash_key = hash(key, key_len);

   if (!(entry = find_entry_in_bucket(&tbl->bucket[hash_key], key, key_len)))
      abort();    /* Entry *must* exist. */

   entry->data = data;
}

/*
 * Get the data associated with <key>, whose length is <key_len> from <tbl>. If
 * no such entry exists NULL is returned. <tbl> and <key> must not be NULL,
 * <key_len> must be greater than 0.
 */
void *hashGet(const HashTable *tbl, const void *key, int key_len)
{
   HashEntry *entry;
   HashKey hash_key = hash(key, key_len);

   assert(tbl != NULL);
   assert(key != NULL);
   assert(key_len > 0);

   entry = find_entry_in_bucket(&tbl->bucket[hash_key], key, key_len);

   return entry ? (void *) entry->data : NULL;
}

/*
 * Delete the entry in <tbl> for <key> with length <key_len>. <tbl> and <key>
 * must not be NULL, <key_len> must be greater than 0. If no such entry exists
 * this function calls abort().
 */
void hashDel(HashTable *tbl, const void *key, int key_len)
{
   HashEntry *entry;
   HashKey hash_key = hash(key, key_len);

   assert(tbl != NULL);
   assert(key != NULL);
   assert(key_len > 0);

   entry = find_entry_in_bucket(&tbl->bucket[hash_key], key, key_len);

   assert(entry);

   listRemove(&tbl->bucket[hash_key], entry);

   free(entry->key);
   free(entry);
}

static List *hash_next_bucket(HashTable *tbl, List *bucket)
{
    int i;

    if (bucket == NULL)
        i = 0;
    else
        i = bucket - tbl->bucket + 1;

    do {
        bucket = tbl->bucket + i;

        if (!listIsEmpty(bucket)) {
            return bucket;
        }
    } while (++i < HASH_BUCKETS);

    return NULL;
}

/*
 * Return a pointer to the first entry in <tbl>, or NULL if <tbl> is empty.
 */
const void *hashFirst(HashTable *tbl)
{
    tbl->bucket_iter = hash_next_bucket(tbl, NULL);

    if (tbl->bucket_iter == NULL) {
        tbl->entry_iter = NULL;
        return NULL;
    }
    else {
        tbl->entry_iter = listHead(tbl->bucket_iter);
        return tbl->entry_iter->data;
    }
}

/*
 * Return the next entry in <tbl>, or NULL if there are no more entries. Note that this function
 * (and hashFirst() above) is not particularly quick. If you need to iterate over the entries in
 * the hash table, and do it quickly, it might be best to also put those entries in a linked list
 * and use that to iterate, rather than these functions.
 */
const void *hashNext(HashTable *tbl)
{
    if (tbl->entry_iter == NULL) {
        return NULL;
    }
    else if ((tbl->entry_iter = listNext(tbl->entry_iter)) != NULL) {
        return tbl->entry_iter->data;
    }
    else if ((tbl->bucket_iter = hash_next_bucket(tbl, tbl->bucket_iter)) != NULL) {
        tbl->entry_iter = listHead(tbl->bucket_iter);
        return tbl->entry_iter->data;
    }
    else {
        return NULL;
    }
}

#ifdef TEST
#include <stdio.h>

static int errors = 0;

void _make_sure_that(const char *file, int line, const char *str, int val)
{
   if (!val) {
      fprintf(stderr, "%s:%d: Expression \"%s\" failed\n", file, line, str);
      errors++;
   }
}

#define make_sure_that(expr) _make_sure_that(__FILE__, __LINE__, #expr, (expr))

int main(int argc, char *argv[])
{
    HashTable *table = hashCreateTable();

    struct Data {
        int i;
        char *s;
    };

    const struct Data *item;

    struct Data data[] = {
        { 0, "zero" },
        { 1, "one" },
        { 2, "two" },
        { 3, "three" },
        { 4, "four" },
    };

    int i, count = sizeof(data) / sizeof(data[0]);

    for (i = 0; i < count; i++) {
        hashAdd(table, data + i, HASH_VALUE(data[i].i));
        hashAdd(table, data + i, HASH_STRING(data[i].s));
    }

    make_sure_that(hashGet(table, HASH_VALUE(data[0].i)) == &data[0]);
    make_sure_that(hashGet(table, HASH_VALUE(data[1].i)) == &data[1]);
    make_sure_that(hashGet(table, HASH_VALUE(data[2].i)) == &data[2]);
    make_sure_that(hashGet(table, HASH_VALUE(data[3].i)) == &data[3]);
    make_sure_that(hashGet(table, HASH_VALUE(data[4].i)) == &data[4]);

    make_sure_that(hashGet(table, HASH_STRING("zero"))  == &data[0]);
    make_sure_that(hashGet(table, HASH_STRING("one"))   == &data[1]);
    make_sure_that(hashGet(table, HASH_STRING("two"))   == &data[2]);
    make_sure_that(hashGet(table, HASH_STRING("three")) == &data[3]);
    make_sure_that(hashGet(table, HASH_STRING("four"))  == &data[4]);

    for (i = 0; i < count; i++) {
        hashDel(table, HASH_VALUE(i));
    }

    make_sure_that(hashGet(table, HASH_VALUE(data[0].i)) == NULL);
    make_sure_that(hashGet(table, HASH_VALUE(data[1].i)) == NULL);
    make_sure_that(hashGet(table, HASH_VALUE(data[2].i)) == NULL);
    make_sure_that(hashGet(table, HASH_VALUE(data[3].i)) == NULL);
    make_sure_that(hashGet(table, HASH_VALUE(data[4].i)) == NULL);

    make_sure_that(hashGet(table, HASH_STRING("zero"))  == &data[0]);
    make_sure_that(hashGet(table, HASH_STRING("one"))   == &data[1]);
    make_sure_that(hashGet(table, HASH_STRING("two"))   == &data[2]);
    make_sure_that(hashGet(table, HASH_STRING("three")) == &data[3]);
    make_sure_that(hashGet(table, HASH_STRING("four"))  == &data[4]);

    for (i = 0; i < count; i++) {
        hashDel(table, HASH_STRING(data[i].s));
    }

    make_sure_that(hashGet(table, HASH_VALUE(data[0].i)) == NULL);
    make_sure_that(hashGet(table, HASH_VALUE(data[1].i)) == NULL);
    make_sure_that(hashGet(table, HASH_VALUE(data[2].i)) == NULL);
    make_sure_that(hashGet(table, HASH_VALUE(data[3].i)) == NULL);
    make_sure_that(hashGet(table, HASH_VALUE(data[4].i)) == NULL);

    make_sure_that(hashGet(table, HASH_STRING("zero"))  == NULL);
    make_sure_that(hashGet(table, HASH_STRING("one"))   == NULL);
    make_sure_that(hashGet(table, HASH_STRING("two"))   == NULL);
    make_sure_that(hashGet(table, HASH_STRING("three")) == NULL);
    make_sure_that(hashGet(table, HASH_STRING("four"))  == NULL);

    hashDeleteTable(table);

    table = hashCreateTable();

    for (i = 0; i < count; i++) {
        hashAdd(table, data + i, HASH_VALUE(data[i].i));
        hashAdd(table, data + i, HASH_STRING(data[i].s));
    }

    for (item = hashFirst(table); item != NULL; item = hashNext(table)) {
        hashDel(table, HASH_STRING(item->s));
        hashDel(table, HASH_VALUE(item->i));
    }

    hashDeleteTable(table);

    return errors;
}
#endif
