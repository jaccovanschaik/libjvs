/*
 * Provides hash tables. Entries are hashed using arbitrary-length keys.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: hash.h 242 2007-06-23 23:12:05Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifndef HASH_H
#define HASH_H

/* Use these macros to provide the <key> and <key_len> parameters in the
 * hashAdd(), hashSet(), hashGet() and hashDel() functions. HASH_STRING can be
 * used to supply a string and its corresponding length, HASH_VALUE can be used
 * to supply a basic type and its size. */

#define HASH_STRING(s) s, strlen(s)
#define HASH_VALUE(k)  &(k), sizeof(k)

typedef struct HashTable HashTable;

/* Create a new hash table. */
HashTable *hashCreateTable(void);

/* Delete hash table <tbl> and its contents. The data that the entries in it
 * point to is *not* removed. */
void hashDeleteTable(HashTable *tbl);

/* Add an entry that points to <data> to <tbl>. Associate it with <key>, whose
 * length is <key_len>. <tbl>, <data> and <key> must not be NULL, <key_len> must
 * be greater than 0. If an entry with the same key already exists this function
 * calls abort(). */
void hashAdd(HashTable *tbl, void *data, void *key, int key_len);

/* Set the existing entry in <tbl> for <key>, whose length is <key_len>, to
 * <data>. <tbl>, <data> and <key> must not be NULL, <key_len> must
 * be greater than 0. If no such entry exists this function calls abort(). */
void hashSet(HashTable *tbl, void *data, void *key, int key_len);

/* Get the data associated with <key>, whose length is <key_len> from <tbl>. If
 * no such entry exists NULL is returned. <tbl> and <key> must not be NULL,
 * <key_len> must be greater than 0. */
void *hashGet(HashTable *tbl, void *key, int key_len);

/* Delete the entry in <tbl> for <key> with length <key_len>. <tbl> and <key>
 * must not be NULL, <key_len> must be greater than 0. If no such entry exists
 * this function calls abort(). */
void hashDel(HashTable *tbl, void *key, int key_len);

#endif