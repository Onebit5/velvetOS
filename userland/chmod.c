// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/chmod.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * chmod: change what may be done to a file.
 */

#include "syscall.h"
#include "args.h"

static const struct opt chmod_opts[] = {
    { 'v', "verbose", false, "name each one as it changes" },
};

static const struct program chmod_prog = {
    .name = "chmod",
    .usage = "chmod <octal> <file>...",
    .summary = "change a file's permissions (needs a filesystem that has any)",
    .opts = chmod_opts,
    .opt_count = sizeof chmod_opts / sizeof chmod_opts[0],
};

void _start(int argc, char **argv)
{
    struct args a;
    const char *error;
    if (!args_parse(&chmod_prog, argc, argv, &a, &error)) {
        write("chmod: ");
        write(error);
        write("\n");
        exit(1);
    }
    if (a.wants_help || a.count < 2) {
        args_usage(&chmod_prog);
        exit(a.wants_help ? 0 : 1);
    }

    long mode = 0;
    const char *p = a.rest[0];
    for (; *p; p++) {
        if (*p < '0' || *p > '7') {
            write("chmod: permissions are octal, 644, 755, 600\n");
            exit(1);
        }
        mode = mode * 8 + (*p - '0');
    }
    if (mode > 0777) {
        write("chmod: that is more bits than a permission has\n");
        exit(1);
    }

    long bad = 0;
    for (int i = 1; i < a.count; i++) {
        if (chmod(a.rest[i], mode) < 0) {
            write("chmod: cannot change ");
            write(a.rest[i]);
            write("\n");
            bad++;
        } else if (args_has(&a, &chmod_prog, 'v')) {
            write(a.rest[i]);
            write("\n");
        }
    }
    exit(bad == 0 ? 0 : 1);
}
