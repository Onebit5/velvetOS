// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/cp.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * copy a file.
 */

#include "syscall.h"
#include "args.h"

static const struct opt cp_opts[] = {
    { 'v', "verbose", false, "say what was copied and how much of it" },
    { 'n', "no-clobber", false, "stop rather than write over something" },
};

static const struct program cp = {
    .name = "cp",
    .usage = "cp [-v] [-n] <from> <to>",
    .summary = "copy a file",
    .opts = cp_opts,
    .opt_count = sizeof cp_opts / sizeof cp_opts[0],
};

static const char *basename(const char *path)
{
    const char *last = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/') {
            last = p + 1;
        }
    }
    return last;
}

static bool join(char *out, long cap, const char *dir, const char *name)
{
    long n = 0;
    for (const char *p = dir; *p; p++) {
        if (n >= cap - 1) return false;
        out[n++] = *p;
    }
    if (n > 0 && out[n - 1] != '/') {
        if (n >= cap - 1) return false;
        out[n++] = '/';
    }
    for (const char *p = name; *p; p++) {
        if (n >= cap - 1) return false;
        out[n++] = *p;
    }
    out[n] = '\0';
    return true;
}

static char buf[4096];

void _start(int argc, char **argv)
{
    struct args a;
    const char *error;
    if (!args_parse(&cp, argc, argv, &a, &error)) {
        write("cp: ");
        write(error);
        write("\n");
        exit(1);
    }
    if (a.wants_help || a.count != 2) {
        args_usage(&cp);
        exit(a.wants_help ? 0 : 1);
    }

    const char *from = a.rest[0];
    const char *to = a.rest[1];

    struct stat st;
    if (stat(from, &st) < 0) {
        write("cp: no such file: ");
        write(from);
        write("\n");
        exit(1);
    }
    if (st.is_dir) {
        write("cp: ");
        write(from);
        write(" is a directory\n");
        exit(1);
    }

    char joined[256];
    struct stat dest;
    bool exists = (stat(to, &dest) == 0);
    if (exists && dest.is_dir) {
        if (!join(joined, sizeof joined, to, basename(from))) {
            write("cp: that path is longer than the program can hold\n");
            exit(1);
        }
        to = joined;
        exists = (stat(to, &dest) == 0);
    }
    if (exists && args_has(&a, &cp, 'n')) {
        write("cp: ");
        write(to);
        write(" is already there\n");
        exit(1);
    }

    /*
     * create opens what is already there rather than emptying it, so
     * copying a short file over a long one would leave the tail of the
     * long one hanging off the end. remove it first: that is what
     * everybody means by copying over something
     */
    if (exists && !dest.is_dir && unlink(to) < 0) {
        write("cp: cannot replace ");
        write(to);
        write("\n");
        exit(1);
    }

    long in = open(from);
    if (in < 0) {
        write("cp: cannot read ");
        write(from);
        write("\n");
        exit(1);
    }

    long out = create(to);
    if (out < 0) {
        write("cp: cannot write ");
        write(to);
        write("\n");
        close(in);
        exit(1);
    }

    long total = 0;
    for (;;) {
        long n = read_fd(in, buf, sizeof buf);
        if (n < 0) {
            write("cp: the read failed partway\n");
            close(in);
            close(out);
            exit(1);
        }
        if (n == 0) {
            break;
        }

        /* a short write is not an error, it is a write that has to be finished. */
        long done = 0;
        while (done < n) {
            long w = write_fd(out, buf + done, n - done);
            if (w <= 0) {
                write("cp: the disk stopped taking bytes\n");
                close(in);
                close(out);
                exit(1);
            }
            done += w;
        }
        total += n;
    }

    close(in);
    close(out);

    if (args_has(&a, &cp, 'v')) {
        write(from);
        write(" -> ");
        write(to);
        write(" (");
        write_num(total);
        write(" bytes)\n");
    }
    exit(0);
}
