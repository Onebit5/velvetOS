// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/ln.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * ln -s: another name for something, kept as text.
 */

#include "syscall.h"
#include "args.h"

static const struct opt ln_opts[] = {
    { 's', "symbolic", false, "make a symbolic link (the only kind here)" },
    { 'v', "verbose",  false, "say what was made" },
};

static const struct program ln_prog = {
    .name = "ln",
    .usage = "ln -s <target> <name>",
    .summary = "make a symbolic link",
    .opts = ln_opts,
    .opt_count = sizeof ln_opts / sizeof ln_opts[0],
};

void _start(int argc, char **argv)
{
    struct args a;
    const char *error;
    if (!args_parse(&ln_prog, argc, argv, &a, &error)) {
        write("ln: ");
        write(error);
        write("\n");
        exit(1);
    }
    if (a.wants_help || a.count != 2) {
        args_usage(&ln_prog);
        exit(a.wants_help ? 0 : 1);
    }
    if (!args_has(&a, &ln_prog, 's')) {
        write("ln: only -s is implemented. a hard link is a different "
              "thing and\n");
        write("    would want its own flag rather than being the default "
              "you get\n");
        write("    by forgetting one\n");
        exit(1);
    }

    if (symlink(a.rest[1], a.rest[0]) < 0) {
        write("ln: cannot make ");
        write(a.rest[1]);
        write(", the filesystem may have no symlinks to make\n");
        exit(1);
    }

    if (args_has(&a, &ln_prog, 'v')) {
        write(a.rest[1]);
        write(" -> ");
        write(a.rest[0]);
        write("\n");
    }
    exit(0);
}
