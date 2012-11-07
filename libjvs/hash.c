/*
 * Provides hash tables.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: hash.c 242 2007-06-23 23:12:05Z jacco $
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

typedef struct {
    ListNode _node;     /* Make it listable. */
    const void *data;   /* Pointer to some data. */
    void *key;          /* Points to the associated key. */
    int key_len;        /* Length of the key. */
} HashEntry;

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

    int *data1 = malloc(sizeof(int));
    char *data2 = strdup("Hoi");

    *data1 = 123;

    hashAdd(table, data1, HASH_VALUE(*data1));
    hashAdd(table, data2, HASH_STRING(data2));

    make_sure_that(hashGet(table, HASH_VALUE(*data1)) == data1);
    make_sure_that(hashGet(table, HASH_STRING(data2)) == data2);

    hashSet(table, data2, HASH_VALUE(*data1));
    hashSet(table, data1, HASH_STRING(data2));

    make_sure_that(hashGet(table, HASH_VALUE(*data1)) == data2);
    make_sure_that(hashGet(table, HASH_STRING(data2)) == data1);

    hashDel(table, HASH_VALUE(*data1));
    hashDel(table, HASH_STRING(data2));

    make_sure_that(hashGet(table, HASH_VALUE(*data1)) == NULL);
    make_sure_that(hashGet(table, HASH_STRING(data2)) == NULL);

    hashDeleteTable(table);

    return errors;
}
#endif
