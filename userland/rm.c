// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/rm.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * remove files.
 */

#include "syscall.h"
#include "args.h"

static const struct opt rm_opts[] = {
    { 'f', "force",   false, "say nothing about names that were not there" },
    { 'i', "ask",     false, "ask before each one" },
    { 'v', "verbose", false, "name each one as it goes" },
};

static const struct program rm = {
    .name = "rm",
    .usage = "rm [-f] [-i] [-v] <file>...",
    .summary = "remove files",
    .opts = rm_opts,
    .opt_count = sizeof rm_opts / sizeof rm_opts[0],
};

/* a yes has to be typed. */
static bool agreed(void)
{
    char c;
    if (read_fd(STDIN, &c, 1) != 1) {
        return false;
    }
    bool yes = (c == 'y' || c == 'Y');

    /* the rest of the line is theirs, not the next question's */
    while (c != '\n' && read_fd(STDIN, &c, 1) == 1) {
    }
    return yes;
}

void _start(int argc, char **argv)
{
    struct args a;
    const char *error;
    if (!args_parse(&rm, argc, argv, &a, &error)) {
        write("rm: ");
        write(error);
        write("\n");
        exit(1);
    }
    if (a.wants_help || a.count == 0) {
        args_usage(&rm);
        exit(a.wants_help ? 0 : 1);
    }

    bool force = args_has(&a, &rm, 'f');
    bool ask = args_has(&a, &rm, 'i');
    bool loud = args_has(&a, &rm, 'v');

    long bad = 0;
    for (int i = 0; i < a.count; i++) {
        const char *name = a.rest[i];

        /* stat first, so that "it is a directory" and "it is not there" are different answers. */
        struct stat st;
        if (stat(name, &st) < 0) {
            if (!force) {
                write("rm: no such file: ");
                write(name);
                write("\n");
                bad++;
            }
            continue;
        }
        if (st.is_dir) {
            write("rm: ");
            write(name);
            write(" is a directory, rmdir removes those\n");
            bad++;
            continue;
        }

        if (ask) {
            write("remove ");
            write(name);
            write("? ");
            if (!agreed()) {
                continue;
            }
        }

        if (unlink(name) < 0) {
            if (!force) {
                write("rm: cannot remove ");
                write(name);
                write("\n");
                bad++;
            }
            continue;
        }
        if (loud) {
            write("removed ");
            write(name);
            write("\n");
        }
    }

    exit(bad == 0 ? 0 : 1);
}
