// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/grep.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * lines that contain something.
 */

#include "syscall.h"
#include "args.h"
#include "lines.h"

static const struct opt grep_opts[] = {
    { 'i', "ignore-case", false, "treat upper and lower case as the same" },
    { 'v', "invert",      false, "the lines that do *not* contain it" },
    { 'n', "number",      false, "number each line that matches" },
    { 'c', "count",       false, "how many matched, rather than which" },
};

static const struct program grep = {
    .name = "grep",
    .usage = "grep [-i] [-v] [-n] [-c] <text> [file...]",
    .summary = "lines containing some text (plain text, not a regex)",
    .opts = grep_opts,
    .opt_count = sizeof grep_opts / sizeof grep_opts[0],
};

static char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static bool contains(const char *haystack, const char *needle, bool fold)
{
    if (needle[0] == '\0') {
        return true;        /* everything contains nothing */
    }
    for (long i = 0; haystack[i] != '\0'; i++) {
        long j = 0;
        while (needle[j] != '\0') {
            char a = haystack[i + j];
            char b = needle[j];
            if (fold) {
                a = lower(a);
                b = lower(b);
            }
            if (a != b) {
                break;
            }
            j++;
        }
        if (needle[j] == '\0') {
            return true;
        }
    }
    return false;
}

struct how {
    bool fold, invert, number, count_only;
    const char *prefix;     /* the filename, when there is more than one */
};

static long sift(long fd, const char *needle, const struct how *h)
{
    struct lines l;
    lines_open(&l, fd);

    char line[LINE_CAP];
    long len;
    long n = 0;
    long matched = 0;

    while (lines_next(&l, line, sizeof line, &len)) {
        n++;
        if (contains(line, needle, h->fold) == h->invert) {
            continue;
        }
        matched++;
        if (h->count_only) {
            continue;
        }
        if (h->prefix != NULL) {
            write(h->prefix);
            write(":");
        }
        if (h->number) {
            write_num(n);
            write(":");
        }
        put_line(line);
    }
    return matched;
}

void _start(int argc, char **argv)
{
    struct args a;
    const char *error;
    if (!args_parse(&grep, argc, argv, &a, &error)) {
        write("grep: ");
        write(error);
        write("\n");
        exit(1);
    }
    if (a.wants_help || a.count == 0) {
        args_usage(&grep);
        if (a.wants_help) {
            write("\n  this is a substring search, not a regular expression.\n");
            write("  the name is what everybody reaches for, so it keeps it.\n");
        }
        exit(a.wants_help ? 0 : 1);
    }

    const char *needle = a.rest[0];

    struct how h;
    h.fold = args_has(&a, &grep, 'i');
    h.invert = args_has(&a, &grep, 'v');
    h.number = args_has(&a, &grep, 'n');
    h.count_only = args_has(&a, &grep, 'c');
    h.prefix = NULL;

    /*
     * just a pattern and no files: standard input, which is what makes
     * `ps | grep hello` mean anything
     */
    if (a.count == 1) {
        long n = sift(STDIN, needle, &h);
        if (h.count_only) {
            write_num(n);
            write("\n");
        }
        exit(n > 0 ? 0 : 1);
    }

    long total = 0;
    long bad = 0;

    for (int i = 1; i < a.count; i++) {
        long fd = open(a.rest[i]);
        if (fd < 0) {
            write("grep: cannot open ");
            write(a.rest[i]);
            write("\n");
            bad++;
            continue;
        }
        /*
         * with more than one file every line says which it came from,
         * because otherwise the output is a list of lines from nowhere
         */
        h.prefix = (a.count > 2) ? a.rest[i] : NULL;
        long n = sift(fd, needle, &h);
        close(fd);

        if (h.count_only) {
            if (a.count > 2) {
                write(a.rest[i]);
                write(":");
            }
            write_num(n);
            write("\n");
        }
        total += n;
    }

    if (bad > 0) {
        exit(2);
    }
    exit(total > 0 ? 0 : 1);
}
