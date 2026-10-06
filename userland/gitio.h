// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/gitio.h
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * everything the object store wants from the world, which is five things.
 */

#ifndef USER_GITIO_H
#define USER_GITIO_H

#include <stddef.h>
#include <stdbool.h>

/* everything the object store wants from the world, which is five things. */

/* the whole file. returns its length, or -1 if it is not there */
long gio_read(const char *path, void *out, size_t cap);

/* the whole file, made if it does not exist. 0 or -1 */
int gio_write(const char *path, const void *data, size_t len);

/*
 * make one directory. already existing is success, because the store
 * makes `objects/ab` before every write and cares only that it is there
 */
int gio_mkdir(const char *path);

bool gio_exists(const char *path);

/* the nth name in a directory, false when there are no more. */
bool gio_entry(const char *path, long n, char *name, size_t cap,
               bool *is_dir, bool *is_exec);

#endif
