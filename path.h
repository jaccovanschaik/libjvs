#ifndef PATH_H
#define PATH_H

/*
 * path.h: Find files in paths.
 *
 * Allows you to look up a file in one of the directories of a search path,
 * like a shell would do. Unlike a shell, this finds all regular files and
 * symbolic links, not just the executable ones. Uses a search tree to quickly
 * find the file.
 *
 * Note that the search tree is built when you call pathCreate and pathAdd, so
 * a file created anywhere in the search path after that will not be found.
 *
 * Copyright: (c) 2020 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2020-09-08
 * Version:   $Id: path.h 429 2021-06-27 22:20:40Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "hashlist.h"

#include <stdio.h>

typedef struct {
    HashList dirs;
    HashList files;
} Path;

/*
 * Create a path, using <initial> as its initial value. <initial> will be
 * handled as described in pathAdd() below, but here it may also be NULL.
 */
Path *pathCreate(const char *initial);

/*
 * Add one or more directories to <path>. <addition> may be a single directory,
 * or multiple directories separated by colons. Any names that don't exist or
 * aren't a directory will be silently ignored. <addition> may not be NULL.
 */
void pathAdd(Path *path, const char *addition);

/*
 * Get the full name for the first file in <path> that has name <filename>.
 * Returns NULL if no such file exists.
 */
const char *pathGet(Path *path, const char *filename);

/*
 * Use fopen() to open the first file in <path> that has name <filename>, using
 * mode <mode>. <filename> and <mode> are passed on to fopen() as-is. Unlike
 * fopen(), however, the file must already exist somewhere in <path> regardless
 * of <mode>, otherwise NULL is returned and errno is set to ENOENT.
 */
FILE *pathFOpen(Path *path, const char *filename, const char *mode);

/*
 * Use open() to open the first file in <path> that has name <filename>, using
 * flags <flags>. <filename> and <flags> are passed on to open() as-is. Unlike
 * open(), however, the file must already exist somewhere in <path> regardless
 * of <flags>, otherwise -1 is returned and errno is set to ENOENT.
 */
int pathOpen(Path *path, const char *filename, int flags);

/*
 * Destroy path <path>.
 */
void pathDestroy(Path *path);

#endif
