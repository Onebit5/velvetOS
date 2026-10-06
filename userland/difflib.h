// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/difflib.h
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * telling two files apart.
 */

#ifndef USER_DIFFLIB_H
#define USER_DIFFLIB_H

#include <stddef.h>
#include <stdbool.h>

/* telling two files apart. */

#define DIFF_MAX_LINES 4000

enum diff_op {
    DIFF_SAME,
    DIFF_ADDED,     /* in b, not in a */
    DIFF_REMOVED    /* in a, not in b */
};

struct diff_edit {
    enum diff_op op;
    size_t       line;      /* which line of a (REMOVED/SAME) or b (ADDED) */
};

/* the two files, as arrays of lines. */
struct diff_lines {
    const char **text;
    const size_t *len;
    size_t count;
};

/* work out what changed. */
bool diff_compare(const struct diff_lines *a, const struct diff_lines *b,
                  struct diff_edit *out, size_t max, size_t *n);

/*
 * true when the two are identical, which is worth a fast answer of its
 * own: it is the commonest case and needs no table at all
 */
bool diff_same(const struct diff_lines *a, const struct diff_lines *b);

#endif
