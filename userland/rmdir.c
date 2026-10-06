// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/rmdir.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * remove an empty directory.
 */

#include "syscall.h"
#include "args.h"

static const struct opt rmdir_opts[] = {
    { 'v', "verbose", false, "name each one as it is removed" },
};

static const struct program rmdir_prog = {
    .name = "rmdir",
    .usage = "rmdir [-v] <directory>...",
    .summary = "remove an empty directory",
    .opts = rmdir_opts,
    .opt_count = sizeof rmdir_opts / sizeof rmdir_opts[0],
};

void _start(int argc, char **argv)
{
    struct args a;
    const char *error;
    if (!args_parse(&rmdir_prog, argc, argv, &a, &error)) {
        write("rmdir: ");
        write(error);
        write("\n");
        exit(1);
    }
    if (a.wants_help || a.count == 0) {
        args_usage(&rmdir_prog);
        exit(a.wants_help ? 0 : 1);
    }

    long bad = 0;
    for (int i = 0; i < a.count; i++) {
        if (rmdir(a.rest[i]) < 0) {
            write("cannot remove ");
            write(a.rest[i]);
            write("\n");
            bad++;
        } else if (args_has(&a, &rmdir_prog, 'v')) {
            write(a.rest[i]);
            write("\n");
        }
    }
    exit(bad == 0 ? 0 : 1);
}
