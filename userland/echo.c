// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/echo.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * echo, which used to be a kernel command.
 */

#include "syscall.h"
#include "args.h"

static const struct opt echo_opts[] = {
    { 'n', "no-newline", false, "leave the trailing newline off" },
};

static const struct program echo = {
    .name = "echo",
    .usage = "echo [-n] <words...>",
    .summary = "say something back",
    .opts = echo_opts,
    .opt_count = sizeof echo_opts / sizeof echo_opts[0],
};

void _start(int argc, char **argv)
{
    struct args a;
    const char *error;
    if (!args_parse(&echo, argc, argv, &a, &error)) {
        write("echo: ");
        write(error);
        write("\n");
        exit(1);
    }
    if (a.wants_help) {
        args_usage(&echo);
        exit(0);
    }

    for (int i = 0; i < a.count; i++) {
        if (i > 0) {
            write(" ");
        }
        write(a.rest[i]);
    }
    if (!args_has(&a, &echo, 'n')) {
        write("\n");
    }
    exit(0);
}
