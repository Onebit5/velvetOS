// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/touch.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * touch.
 */

#include "syscall.h"
#include "args.h"

static const struct opt touch_opts[] = {
    { 'c', "no-create", false, "only stamp files that are already there" },
    { 'v', "verbose",   false, "say which were made and which were stamped" },
};

static const struct program touch = {
    .name = "touch",
    .usage = "touch [-c] [-v] <file>...",
    .summary = "make an empty file, or bring its timestamp up to date",
    .opts = touch_opts,
    .opt_count = sizeof touch_opts / sizeof touch_opts[0],
};

void _start(int argc, char **argv)
{
    struct args a;
    const char *error;
    if (!args_parse(&touch, argc, argv, &a, &error)) {
        write("touch: ");
        write(error);
        write("\n");
        exit(1);
    }
    if (a.wants_help || a.count == 0) {
        args_usage(&touch);
        exit(a.wants_help ? 0 : 1);
    }

    bool loud = args_has(&a, &touch, 'v');
    bool only_existing = args_has(&a, &touch, 'c');

    long bad = 0;
    for (int i = 0; i < a.count; i++) {
        const char *name = a.rest[i];

        struct stat st;
        bool there = (stat(name, &st) == 0);

        if (!there && only_existing) {
            continue;       /* -c means exactly this: no, and no complaint */
        }
        if (there && st.is_dir) {
            write("touch: ");
            write(name);
            write(" is a directory\n");
            bad++;
            continue;
        }

        /* create makes it if it is not there. */
        long fd = create(name);
        if (fd < 0) {
            write("touch: cannot touch ");
            write(name);
            write("\n");
            bad++;
            continue;
        }
        write_fd(fd, "", 0);
        close(fd);

        if (loud) {
            write(there ? "stamped " : "made ");
            write(name);
            write("\n");
        }
    }

    exit(bad == 0 ? 0 : 1);
}
