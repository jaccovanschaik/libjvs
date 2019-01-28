#ifndef LIBJVS_HASH_H
#define LIBJVS_HASH_H

/*
 * Provides hash tables. Entries are hashed using arbitrary-length keys.
 *
 * Part of libjvs.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "list.h"
#include "pa.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The number of significant bits in a hash key and the resulting number of
 * buckets in the hash table. */

#define HASH_BITS 12
#define HASH_BUCKETS (1 << (HASH_BITS))

/* A hash table. Contains <HASH_BUCKETS> buckets, each of which consists of a
 * list of HashEntry structs. */

typedef struct HashEntry HashEntry;

typedef struct {
    List bucket[HASH_BUCKETS];
} HashTable;

/* Use these macros to provide the <key> and <key_len> parameters in the
 * hashAdd(), hashSet(), hashGet() and hashDel() functions. HASH_STRING can be
 * used to supply a string and its corresponding length, HASH_VALUE can be used
 * to supply a basic type and its size. */

#define HASH_STRING(s) s, strlen(s)
#define HASH_VALUE(k)  &(k), sizeof(k)

/*
 * Initialize hash table <table>.
 */
void hashInitTable(HashTable *table);

/*
 * Create a new hash table.
 */
HashTable *hashCreateTable(void);

/*
 * Clear table <tbl> i.e. remove all its entries. The user data that the
 * entries point to is *not* removed.
 */
void hashClearTable(HashTable *tbl);

/*
 * Delete hash table <tbl> and its contents. The user data that its
 * entries point to is *not* removed.
 */
void hashDeleteTable(HashTable *tbl);

/*
 * Add an entry that points to <data> to <tbl>. Associate it with <key>, whose
 * length is <key_len>. <tbl>, <data> and <key> must not be NULL, <key_len> must
 * be greater than 0. If an entry with the same key already exists this function
 * calls abort().
 */
void hashAdd(HashTable *tbl, const void *data, const void *key, int key_len);

/*
 * Set the existing entry in <tbl> for <key>, whose length is <key_len>, to
 * <data>. <tbl>, <data> and <key> must not be NULL, <key_len> must
 * be greater than 0. If no such entry exists this function calls abort().
 */
void hashSet(HashTable *tbl, const void *data, const void *key, int key_len);

/*
 * Return TRUE if <tbl> has an entry for <key> with length <key_len>, FALSE otherwise. <tbl> and
 * <key> must not be NULL, <key_len> must be greater than 0.
 */
int hashContains(const HashTable *tbl, const void *key, int key_len);

/*
 * Get the data associated with <key>, whose length is <key_len> from <tbl>. If
 * no such entry exists NULL is returned. <tbl> and <key> must not be NULL,
 * <key_len> must be greater than 0.
 */
void *hashGet(const HashTable *tbl, const void *key, int key_len);

/*
 * Delete the entry in <tbl> for <key> with length <key_len>. <tbl> and <key>
 * must not be NULL, <key_len> must be greater than 0. If no such entry exists
 * this function calls abort().
 */
void hashDel(HashTable *tbl, const void *key, int key_len);

/*
 * Get the first entry in <tbl> and return its data pointer through <ptr>. Returns 1 if an entry was
 * found, otherwise 0 (in which case <*ptr> is not modified). Note that <*ptr> may be NULL if the
 * found entry contains a NULL pointer.
 */
int hashFirst(HashTable *tbl, void **ptr);

/*
 * Get the next entry in <tbl> and return its data pointer through <ptr>. Returns 1 if an entry was
 * found, otherwise 0 (in which case <*ptr> is not modified). Note that hashNext() and hashFirst()
 * above are not particularly quick. If you need to iterate over the entries in the hash table, and
 * do it quickly, it might be best to also put those entries in a linked list and use that to
 * iterate, rather than these functions. Also, these functions almost certainly return entries in a
 * different order than what they were added with.
 */
int hashNext(HashTable *tbl, void **ptr);

/*
 * Return a pointer array containing the number of entries in each hash bucket.
 */
PointerArray *hashStats(HashTable *tbl);

/*
 * Free the result from the hashStats function.
 */
void hashFreeStats(PointerArray *stats);

#ifdef __cplusplus
}
#endif

#endif
