// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/gitio_host.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the host seam of the git object store.
 */

/* the seam, on the machine this is developed on. */
#include "gitio.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

long gio_read(const char *path, void *out, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return -1;
    }
    size_t n = fread(out, 1, cap, f);
    fclose(f);
    return (long)n;
}

int gio_write(const char *path, const void *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return -1;
    }
    bool ok = fwrite(data, 1, len, f) == len;
    fclose(f);
    return ok ? 0 : -1;
}

int gio_mkdir(const char *path)
{
    mkdir(path, 0755);
    return 0;
}

bool gio_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

bool gio_entry(const char *path, long n, char *name, size_t cap,
               bool *is_dir, bool *is_exec)
{
    DIR *d = opendir(path);
    if (d == NULL) {
        return false;
    }
    struct dirent *e;
    long at = 0;
    bool found = false;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
            continue;
        }
        if (at++ != n) {
            continue;
        }
        snprintf(name, cap, "%s", e->d_name);
        char full[2048];
        snprintf(full, sizeof full, "%s/%s", path, e->d_name);
        struct stat st;
        if (stat(full, &st) == 0) {
            *is_dir = S_ISDIR(st.st_mode) != 0;
            *is_exec = (st.st_mode & 0100) != 0;
        }
        found = true;
        break;
    }
    closedir(d);
    return found;
}
