// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/wordcount.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * wordcount, the roadmap's test, written down.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct counts {
    long lines, words, bytes;
};

static void count_line(const char *line, long len, struct counts *c)
{
    c->lines++;
    c->bytes += len + 1;        /* the newline that was taken off */

    bool in_word = false;
    for (long i = 0; i < len; i++) {
        bool space = (line[i] == ' ' || line[i] == '\t');
        if (!space && !in_word) {
            c->words++;
        }
        in_word = !space;
    }
}

static void report(const struct counts *c, const char *what)
{
    printf("%8ld %8ld %8ld", c->lines, c->words, c->bytes);
    if (what != NULL) {
        printf(" %s", what);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    /*
     * a buffer from the heap rather than the stack, because that is the
     * thing the allocator added and a program that never allocates does not
     * test it
     */
    size_t cap = 4096;
    char *line = malloc(cap);
    if (line == NULL) {
        fprintf(STDERR_FILENO, "wordcount: out of memory\n");
        return 1;
    }

    struct counts total = { 0, 0, 0 };
    int files = 0;

    for (int i = 1; i < argc; i++) {
        long fd = open_file(argv[i]);
        if (fd < 0) {
            fprintf(STDERR_FILENO, "wordcount: cannot open %s\n", argv[i]);
            continue;
        }
        struct counts c = { 0, 0, 0 };
        for (;;) {
            long n = getline_fd((int)fd, line, cap);
            if (n <= 0) {
                break;
            }
            count_line(line, n, &c);
        }
        close_file(fd);

        report(&c, argv[i]);
        total.lines += c.lines;
        total.words += c.words;
        total.bytes += c.bytes;
        files++;
    }

    if (files == 0) {
        struct counts c = { 0, 0, 0 };
        for (;;) {
            long n = getline_fd(STDIN_FILENO, line, cap);
            if (n <= 0) {
                break;
            }
            count_line(line, n, &c);
        }
        report(&c, NULL);
    } else if (files > 1) {
        report(&total, "total");
    }

    free(line);
    return 0;       /* and a main that returns, which is the whole test */
}
