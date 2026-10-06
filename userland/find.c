// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/find.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * find, every name under here.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "syscall.h"

/*
 * how deep it will go. a bound rather than a recursion that trusts the
 * filesystem: a directory loop, which a symlink can make, would
 * otherwise walk until the stack ran out
 */
#define MAX_DEPTH 24

static const char *wanted;
static long found;

static void walk(char *path, size_t len, int depth)
{
    if (depth > MAX_DEPTH) {
        return;
    }

    for (long i = 0; ; i++) {
        char name[128];
        if (readdir_at(i, name, sizeof name, path) < 0) {
            break;
        }
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }

        /* the child's path, built in the buffer the parent handed down. */
        size_t n = strlen(name);
        bool root = (len == 1 && path[0] == '/');
        if (len + n + 2 > 512) {
            continue;       /* too deep to name; skip rather than overrun */
        }
        size_t at = len;
        if (!root) {
            path[at++] = '/';
        }
        memcpy(path + at, name, n);
        at += n;
        path[at] = '\0';

        if (wanted == NULL || strstr(name, wanted) != NULL) {
            printf("%s\n", path);
            found++;
        }

        struct stat st;
        if (stat(path, &st) == 0 && st.is_dir) {
            walk(path, at, depth + 1);
        }
        path[len] = '\0';       /* put it back for the next name */
    }
}

int main(int argc, char **argv)
{
    static char path[512];
    const char *start = (argc >= 2) ? argv[1] : "/";
    wanted = (argc >= 3) ? argv[2] : NULL;

    snprintf(path, sizeof path, "%s", start);
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        path[--len] = '\0';     /* one trailing slash is not two */
    }

    walk(path, len, 0);

    if (found == 0 && wanted != NULL) {
        fprintf(STDERR_FILENO, "find: nothing under %s named like '%s'\n",
                start, wanted);
        return 1;
    }
    return 0;
}
