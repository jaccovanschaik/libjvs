/*
 * path.c: Find files in paths.
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
 * Version:   $Id: path.c 419 2021-06-06 11:13:46Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "path.h"
#include "buffer.h"
#include "defs.h"
#include "debug.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct {
    char *name;
} PathDir;

typedef struct {
    const PathDir *dir;
    char *name;
} PathFile;

static PathDir *path_create_dir(const char *dirname)
{
    PathDir *dir = calloc(1, sizeof(*dir));

    dir->name = strdup(dirname);

    return dir;
}

static PathFile *path_create_file(const char *filename, const PathDir *dir)
{
    PathFile *file = calloc(1, sizeof(*file));

    file->name = strdup(filename);
    file->dir  = dir;

    return file;
}

static void path_add_file(Path *path, const char *dirname, const char *filename)
{
    PathDir *dir;
    PathFile *file;

    if ((dir = treeGetS(&path->dirs, dirname)) == NULL) {
        treeAddS(&path->dirs, (dir = path_create_dir(dirname)), dirname);
    }

    if ((file = treeGetS(&path->files, filename)) == NULL) {
        treeAddS(&path->files, path_create_file(filename, dir), filename);
    }
}

/*
 * Create a path, using <initial> as its initial value. <initial> will be
 * handled as described in pathAdd() below, but here it may also be NULL.
 */
Path *pathCreate(const char *initial)
{
    Path *path = calloc(1, sizeof(*path));

    if (initial) pathAdd(path, initial);

    return path;
}

/*
 * Add one or more directories to <path>. <addition> may be a single directory,
 * or multiple directories separated by colons. Any names that don't exist or
 * aren't a directory will be silently ignored. <addition> may not be NULL.
 */
void pathAdd(Path *path, const char *addition)
{
    dbgAssert(stderr, addition != NULL, "<addition> may not be NULL");

    Buffer dirname = { 0 };

    const char *end = addition - 1;

    do {
        DIR *dir;
        struct dirent *entry;
        const char *begin = end + 1;

        for (end = begin; *end != ':' && *end != '\0'; end++) ;

        int length = end - begin;

        if (length == 0) continue;

        bufSet(&dirname, begin, length);

        if ((dir = opendir(bufGet(&dirname))) == NULL) continue;

        int dir_fd = dirfd(dir);

        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_UNKNOWN) {
                struct stat statbuf;

                fstatat(dir_fd, entry->d_name, &statbuf, 0);

                int type = statbuf.st_mode & S_IFMT;

                if (type == S_IFREG || type == S_IFLNK) {
                    path_add_file(path, bufGet(&dirname), entry->d_name);
                }
            }
            else if (entry->d_type == DT_REG || entry->d_type == DT_LNK) {
                path_add_file(path, bufGet(&dirname), entry->d_name);
            }
        }

        closedir(dir);
    } while (*end == ':');

    bufClear(&dirname);
}

/*
 * Get the full name for the first file in <path> that has name <filename>.
 * Returns NULL if no such file exists.
 */
const char *pathGet(Path *path, const char *filename)
{
    static Buffer namebuf = { 0 };

    PathFile *file;

    if ((file = treeGetS(&path->files, filename)) == NULL) return NULL;

    bufSetS(&namebuf, file->dir->name);
    bufAddS(&namebuf, "/");
    bufAddS(&namebuf, file->name);

    return bufGet(&namebuf);
}

/*
 * Use fopen() to open the first file in <path> that has name <filename>, using
 * mode <mode>. <filename> and <mode> are passed on to fopen() as-is. Unlike
 * fopen(), however, the file must already exist somewhere in <path> regardless
 * of <mode>, otherwise NULL is returned and errno is set to ENOENT.
 */
FILE *pathFOpen(Path *path, const char *filename, const char *mode)
{
    const char *fullpath;
    FILE *fp;

    if ((fullpath = pathGet(path, filename)) == NULL) {
        errno = ENOENT;
        return NULL;
    }
    else if ((fp = fopen(fullpath, mode)) == NULL) {
        return NULL;
    }
    else {
        return fp;
    }
}

/*
 * Use open() to open the first file in <path> that has name <filename>, using
 * flags <flags>. <filename> and <flags> are passed on to open() as-is. Unlike
 * open(), however, the file must already exist somewhere in <path> regardless
 * of <flags>, otherwise -1 is returned and errno is set to ENOENT.
 */
int pathOpen(Path *path, const char *filename, int flags)
{
    const char *fullpath;
    int fd;

    if ((fullpath = pathGet(path, filename)) == NULL) {
        errno = ENOENT;
        return -1;
    }
    else if ((fd = open(fullpath, flags)) == -1) {
        return -1;
    }
    else {
        return fd;
    }
}

static void path_free_dir(Tree *tree, void *data, const void *key, size_t key_size)
{
    UNUSED(tree);
    UNUSED(key);
    UNUSED(key_size);

    PathDir *dir = data;

    treeDrop(tree, key, key_size);

    free(dir->name);
    free(dir);
}

static void path_free_file(Tree *tree, void *data, const void *key, size_t key_size)
{
    UNUSED(tree);
    UNUSED(key);
    UNUSED(key_size);

    PathFile *file = data;

    treeDrop(tree, key, key_size);

    free(file->name);
    free(file);
}

/*
 * Destroy path <path>.
 */
void pathDestroy(Path *path)
{
    treeTraverse(&path->dirs, path_free_dir);
    treeTraverse(&path->files, path_free_file);

    treeDestroy(&path->dirs);
    treeDestroy(&path->files);
}

#ifdef TEST
#include "utils.h"

static int errors = 0;

int main(void)
{
    Path *path = pathCreate(getenv("PATH"));

    make_sure_that(pathGet(path, "ls") != NULL);
    make_sure_that(pathGet(path, "path.c") == NULL);

    pathAdd(path, ".");

    make_sure_that(pathGet(path, "path.c") != NULL);

    FILE *fp;

    make_sure_that((fp = pathFOpen(path, "path.c", "r")) != NULL);
    make_sure_that(fclose(fp) == 0);

    int fd;

    make_sure_that((fd = pathOpen(path, "path.c", O_RDONLY)) != -1);
    make_sure_that(close(fd) == 0);

    return errors;
}

#endif
