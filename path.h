#ifndef PATH_H
#define PATH_H

/*
 * path.h: XXX
 *
 * Copyright: (c) 2020 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2020-09-08
 * Version:   $Id: path.h 400 2020-09-08 15:02:08Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "tree.h"

#include <stdio.h>

typedef struct {
    Tree dirs;
    Tree files;
} Path;

/*
 * Create a path, using <initial> as its initial value. <initial> will be
 * handled as described in pathAdd() below. In addition, it may also be NULL.
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
