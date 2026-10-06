// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/diff.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * diff, what changed between two files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "syscall.h"
#include "difflib.h"

#define MAX_LINES DIFF_MAX_LINES

struct file {
    char   *blob;           /* the whole thing, with newlines turned to 0 */
    const char *text[MAX_LINES];
    size_t  len[MAX_LINES];
    size_t  count;
};

/* read the whole file and cut it into lines in place. */
static bool slurp(const char *path, struct file *f)
{
    long fd = open(path);
    if (fd < 0) {
        return false;
    }

    size_t cap = 8192, len = 0;
    f->blob = malloc(cap);
    if (f->blob == NULL) {
        close(fd);
        return false;
    }
    for (;;) {
        if (len == cap) {
            char *bigger = realloc(f->blob, cap * 2);
            if (bigger == NULL) {
                close(fd);
                return false;
            }
            f->blob = bigger;
            cap *= 2;
        }
        long n = read_fd(fd, f->blob + len, (long)(cap - len));
        if (n <= 0) {
            break;
        }
        len += (size_t)n;
    }
    close(fd);

    f->count = 0;
    size_t start = 0;
    for (size_t i = 0; i <= len; i++) {
        if (i == len || f->blob[i] == '\n') {
            if (i == len && start == i) {
                break;      /* a trailing newline is not an empty line */
            }
            if (f->count >= MAX_LINES) {
                return false;
            }
            f->text[f->count] = f->blob + start;
            f->len[f->count]  = i - start;
            f->count++;
            start = i + 1;
        }
    }
    return true;
}

static void show(const char *prefix, const char *s, size_t n)
{
    printf("%s", prefix);
    write_fd(STDOUT, s, (long)n);
    printf("\n");
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(STDERR_FILENO, "diff <old> <new>\n");
        return 2;
    }

    static struct file fa, fb;
    if (!slurp(argv[1], &fa)) {
        fprintf(STDERR_FILENO, "diff: cannot read %s\n", argv[1]);
        return 2;
    }
    if (!slurp(argv[2], &fb)) {
        fprintf(STDERR_FILENO, "diff: cannot read %s\n", argv[2]);
        return 2;
    }

    struct diff_lines a = { fa.text, fa.len, fa.count };
    struct diff_lines b = { fb.text, fb.len, fb.count };

    if (diff_same(&a, &b)) {
        return 0;       /* and print nothing, which is what silence means */
    }

    static struct diff_edit edits[MAX_LINES * 2];
    size_t n = 0;
    if (!diff_compare(&a, &b, edits, sizeof edits / sizeof edits[0], &n)) {
        fprintf(STDERR_FILENO, "diff: those are too big to compare\n");
        return 2;
    }

    /* only the changes, with a line of context either side. */
    for (size_t i = 0; i < n; i++) {
        if (edits[i].op == DIFF_SAME) {
            bool near = (i > 0 && edits[i - 1].op != DIFF_SAME)
                     || (i + 1 < n && edits[i + 1].op != DIFF_SAME);
            if (near) {
                show("  ", a.text[edits[i].line], a.len[edits[i].line]);
            }
            continue;
        }
        if (edits[i].op == DIFF_REMOVED) {
            show("- ", a.text[edits[i].line], a.len[edits[i].line]);
        } else {
            show("+ ", b.text[edits[i].line], b.len[edits[i].line]);
        }
    }
    return 1;
}
