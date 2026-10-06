// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/wc.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * count what went past.
 */

#include "syscall.h"
#include "args.h"
#include "lines.h"

static const struct opt wc_opts[] = {
    { 'l', "lines", false, "count lines only" },
    { 'w', "words", false, "count words only" },
    { 'c', "chars", false, "count characters only" },
};

static const struct program wc = {
    .name = "wc",
    .usage = "wc [-l] [-w] [-c] [file...]",
    .summary = "count lines, words and characters",
    .opts = wc_opts,
    .opt_count = sizeof wc_opts / sizeof wc_opts[0],
};

struct count {
    long lines, words, chars;
};

static void tally(long fd, struct count *c)
{
    struct lines l;
    lines_open(&l, fd);

    char line[LINE_CAP];
    long len;
    while (lines_next(&l, line, sizeof line, &len)) {
        c->lines++;
        c->chars += len + 1;    /* the newline counts, as it does everywhere */

        bool in_word = false;
        for (long i = 0; i < len; i++) {
            bool space = (line[i] == ' ' || line[i] == '\t');
            if (!space && !in_word) {
                c->words++;
            }
            in_word = !space;
        }
    }
}

static void report(const struct count *c, const char *name, bool l, bool w,
                   bool ch)
{
    if (l) { write("  "); write_num(c->lines); }
    if (w) { write("  "); write_num(c->words); }
    if (ch) { write("  "); write_num(c->chars); }
    if (name != NULL) {
        write("  ");
        write(name);
    }
    write("\n");
}

void _start(int argc, char **argv)
{
    struct args a;
    const char *error;
    if (!args_parse(&wc, argc, argv, &a, &error)) {
        write("wc: ");
        write(error);
        write("\n");
        exit(1);
    }
    if (a.wants_help) {
        args_usage(&wc);
        exit(0);
    }

    bool l = args_has(&a, &wc, 'l');
    bool w = args_has(&a, &wc, 'w');
    bool c = args_has(&a, &wc, 'c');
    if (!l && !w && !c) {
        l = w = c = true;       /* no choice made is all three */
    }

    if (a.count == 0) {
        struct count n = { 0, 0, 0 };
        tally(STDIN, &n);
        report(&n, NULL, l, w, c);
        exit(0);
    }

    struct count total = { 0, 0, 0 };
    long bad = 0;

    for (int i = 0; i < a.count; i++) {
        long fd = open(a.rest[i]);
        if (fd < 0) {
            write("wc: cannot open ");
            write(a.rest[i]);
            write("\n");
            bad++;
            continue;
        }
        struct count n = { 0, 0, 0 };
        tally(fd, &n);
        close(fd);
        report(&n, a.rest[i], l, w, c);

        total.lines += n.lines;
        total.words += n.words;
        total.chars += n.chars;
    }

    if (a.count > 1) {
        report(&total, "total", l, w, c);
    }
    exit(bad == 0 ? 0 : 1);
}
