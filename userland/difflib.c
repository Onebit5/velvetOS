// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/difflib.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * telling two files apart.
 */

#include "difflib.h"
#include <stdlib.h>
#include <string.h>

static bool line_equal(const struct diff_lines *a, size_t i,
                       const struct diff_lines *b, size_t j)
{
    if (a->len[i] != b->len[j]) {
        return false;
    }
    return memcmp(a->text[i], b->text[j], a->len[i]) == 0;
}

bool diff_same(const struct diff_lines *a, const struct diff_lines *b)
{
    if (a->count != b->count) {
        return false;
    }
    for (size_t i = 0; i < a->count; i++) {
        if (!line_equal(a, i, b, i)) {
            return false;
        }
    }
    return true;
}

bool diff_compare(const struct diff_lines *a, const struct diff_lines *b,
                  struct diff_edit *out, size_t max, size_t *n)
{
    *n = 0;

    if (a->count > DIFF_MAX_LINES || b->count > DIFF_MAX_LINES) {
        return false;
    }

    /* the commonest case, answered without a table. */
    if (diff_same(a, b)) {
        for (size_t i = 0; i < a->count; i++) {
            if (*n >= max) {
                return false;
            }
            out[(*n)++] = (struct diff_edit){ DIFF_SAME, i };
        }
        return true;
    }

    size_t rows = a->count + 1;
    size_t cols = b->count + 1;

    /*
     * the table is (rows * cols) of the length of the longest common
     * subsequence of each pair of prefixes. checked against wrapping
     * before it is allocated: two large counts multiplied is exactly
     * where a size becomes small and the allocation succeeds at the
     * wrong size
     */
    if (cols != 0 && rows > (size_t)-1 / cols / sizeof(unsigned)) {
        return false;
    }
    unsigned *table = calloc(rows * cols, sizeof *table);
    if (table == NULL) {
        return false;
    }

#define AT(i, j) table[(i) * cols + (j)]

    /*
     * filled from the end backwards, so the walk that recovers the path
     * can go forwards, which is the order the edits have to come out
     * in, and reversing a list afterwards is a second thing to get
     * wrong
     */
    for (size_t i = a->count; i-- > 0; ) {
        for (size_t j = b->count; j-- > 0; ) {
            if (line_equal(a, i, b, j)) {
                AT(i, j) = AT(i + 1, j + 1) + 1;
            } else {
                unsigned down = AT(i + 1, j);
                unsigned right = AT(i, j + 1);
                AT(i, j) = (down > right) ? down : right;
            }
        }
    }

    size_t i = 0, j = 0;
    bool ok = true;

    while (i < a->count && j < b->count) {
        if (*n >= max) {
            ok = false;
            break;
        }
        if (line_equal(a, i, b, j)) {
            out[(*n)++] = (struct diff_edit){ DIFF_SAME, i };
            i++;
            j++;
        } else if (AT(i + 1, j) >= AT(i, j + 1)) {
            /* the line in `a` is not in `b`: it was removed. */
            out[(*n)++] = (struct diff_edit){ DIFF_REMOVED, i };
            i++;
        } else {
            out[(*n)++] = (struct diff_edit){ DIFF_ADDED, j };
            j++;
        }
    }

    /* whatever is left on one side is all insertion or all deletion */
    while (ok && i < a->count) {
        if (*n >= max) { ok = false; break; }
        out[(*n)++] = (struct diff_edit){ DIFF_REMOVED, i++ };
    }
    while (ok && j < b->count) {
        if (*n >= max) { ok = false; break; }
        out[(*n)++] = (struct diff_edit){ DIFF_ADDED, j++ };
    }

#undef AT
    free(table);
    return ok;
}
