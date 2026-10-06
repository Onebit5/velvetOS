// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/head.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the first few lines of something.
 */

#include "syscall.h"
#include "args.h"
#include "lines.h"

static const struct opt head_opts[] = {
    { 'n', "lines",   true,  "how many lines (10 by default)" },
    { 'q', "quiet",   false, "no header, even with several files" },
};

static const struct program head = {
    .name = "head",
    .usage = "head [-n count] [file...]",
    .summary = "the first few lines of something, or of standard input",
    .opts = head_opts,
    .opt_count = sizeof head_opts / sizeof head_opts[0],
};

static long to_number(const char *s, long fallback)
{
    if (s == NULL || *s == '\0') {
        return fallback;
    }
    long v = 0;
    for (const char *p = s; *p; p++) {
        if (*p < '0' || *p > '9') {
            return fallback;
        }
        v = v * 10 + (*p - '0');
        if (v > 1000000) {
            return 1000000;
        }
    }
    return v;
}

static void take(long fd, long count)
{
    struct lines l;
    lines_open(&l, fd);

    char line[LINE_CAP];
    long len;
    for (long i = 0; i < count && lines_next(&l, line, sizeof line, &len); i++) {
        put_line(line);
    }
}

void _start(int argc, char **argv)
{
    struct args a;
    const char *error;
    if (!args_parse(&head, argc, argv, &a, &error)) {
        write("head: ");
        write(error);
        write("\n");
        exit(1);
    }
    if (a.wants_help) {
        args_usage(&head);
        exit(0);
    }

    long count = to_number(args_value(&a, &head, 'n'), 10);

    /*
     * no filenames means standard input, which is either the keyboard
     * or whoever is on the other side of a bar. this program cannot
     * tell, and that is exactly the point
     */
    if (a.count == 0) {
        take(STDIN, count);
        exit(0);
    }

    bool quiet = args_has(&a, &head, 'q') || a.count == 1;
    long bad = 0;

    for (int i = 0; i < a.count; i++) {
        long fd = open(a.rest[i]);
        if (fd < 0) {
            write("head: cannot open ");
            write(a.rest[i]);
            write("\n");
            bad++;
            continue;
        }
        if (!quiet) {
            if (i > 0) {
                write("\n");
            }
            write("==> ");
            write(a.rest[i]);
            write(" <==\n");
        }
        take(fd, count);
        close(fd);
    }

    exit(bad == 0 ? 0 : 1);
}
