// SPDX-License-Identifier: GPL-2.0-only
/*
 * tests/test_difflib.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * host-side test for telling two files apart.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../userland/difflib.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
} while (0)

/* a file, from an array of strings */
static const char *a_text[64];
static size_t a_len[64];
static const char *b_text[64];
static size_t b_len[64];

static struct diff_lines make(const char **into, size_t *lens,
                             const char *const *from, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        into[i] = from[i];
        lens[i] = strlen(from[i]);
    }
    struct diff_lines d = { into, lens, n };
    return d;
}

/* replay the edits onto `a` and see whether the result is `b`. */
static bool rebuilds(const struct diff_lines *a, const struct diff_lines *b,
                     const struct diff_edit *e, size_t n)
{
    size_t at = 0;
    for (size_t i = 0; i < n; i++) {
        const char *want;
        size_t want_len;

        if (e[i].op == DIFF_REMOVED) {
            continue;                       /* not in the result */
        }
        if (e[i].op == DIFF_SAME) {
            want = a->text[e[i].line];
            want_len = a->len[e[i].line];
        } else {
            want = b->text[e[i].line];
            want_len = b->len[e[i].line];
        }
        if (at >= b->count) {
            return false;                   /* too many lines */
        }
        if (b->len[at] != want_len
         || memcmp(b->text[at], want, want_len) != 0) {
            return false;
        }
        at++;
    }
    return at == b->count;
}

static struct diff_edit edits[256];
static size_t n;

int main(void)
{
    struct diff_lines a, b;



    static const char *same1[] = { "one", "two", "three" };
    a = make(a_text, a_len, same1, 3);
    b = make(b_text, b_len, same1, 3);

    CHECK(diff_same(&a, &b), "two identical files are identical");
    CHECK(diff_compare(&a, &b, edits, 256, &n), "and compare");
    CHECK(n == 3, "with one edit per line");
    for (size_t i = 0; i < n; i++) {
        CHECK(edits[i].op == DIFF_SAME, "all of them unchanged");
    }
    CHECK(rebuilds(&a, &b, edits, n), "and rebuilding gives the same file");

    /* the case that makes a diff worth writing. */

    static const char *before[] = { "one", "two", "three" };
    static const char *after[]  = { "zero", "one", "two", "three" };
    a = make(a_text, a_len, before, 3);
    b = make(b_text, b_len, after, 4);

    CHECK(!diff_same(&a, &b), "a line was added");
    CHECK(diff_compare(&a, &b, edits, 256, &n), "and it compares");
    CHECK(rebuilds(&a, &b, edits, n), "and the edits rebuild the new file");

    size_t added = 0, removed = 0, unchanged = 0;
    for (size_t i = 0; i < n; i++) {
        if (edits[i].op == DIFF_ADDED)   { added++; }
        if (edits[i].op == DIFF_REMOVED) { removed++; }
        if (edits[i].op == DIFF_SAME)    { unchanged++; }
    }
    CHECK(added == 1 && removed == 0 && unchanged == 3,
          "reported as *one* line added and three unchanged, a "
          "comparison that walked in step would call all four different, "
          "which is true and useless");



    static const char *full[] = { "a", "b", "c", "d" };
    static const char *gone[] = { "a", "c", "d" };
    a = make(a_text, a_len, full, 4);
    b = make(b_text, b_len, gone, 3);

    CHECK(diff_compare(&a, &b, edits, 256, &n), "a removal compares");
    CHECK(rebuilds(&a, &b, edits, n), "and rebuilds");
    removed = added = 0;
    for (size_t i = 0; i < n; i++) {
        if (edits[i].op == DIFF_REMOVED) { removed++; }
        if (edits[i].op == DIFF_ADDED)   { added++; }
    }
    CHECK(removed == 1 && added == 0, "as one removal and nothing else");



    static const char *was[]  = { "a", "b", "c" };
    static const char *now[]  = { "a", "B", "c" };
    a = make(a_text, a_len, was, 3);
    b = make(b_text, b_len, now, 3);

    CHECK(diff_compare(&a, &b, edits, 256, &n), "a change compares");
    CHECK(rebuilds(&a, &b, edits, n), "and rebuilds");

    /*
     * and the removal comes first, which is what every diff prints and
     * what anybody reading one expects
     */
    size_t first_change = 0;
    while (first_change < n && edits[first_change].op == DIFF_SAME) {
        first_change++;
    }
    CHECK(first_change < n && edits[first_change].op == DIFF_REMOVED,
          "with the removal before the insertion, the tie goes that "
          "way so a change reads the way diffs have always printed it");



    static const char *nothing[] = { "" };
    a = make(a_text, a_len, was, 3);
    b = make(b_text, b_len, nothing, 0);
    b.count = 0;

    CHECK(diff_compare(&a, &b, edits, 256, &n), "everything removed");
    CHECK(n == 3, "three edits");
    for (size_t i = 0; i < n; i++) {
        CHECK(edits[i].op == DIFF_REMOVED, "all of them removals");
    }
    CHECK(rebuilds(&a, &b, edits, n), "rebuilding gives an empty file");

    a.count = 0;
    b = make(b_text, b_len, was, 3);
    CHECK(diff_compare(&a, &b, edits, 256, &n), "and everything added");
    for (size_t i = 0; i < n; i++) {
        CHECK(edits[i].op == DIFF_ADDED, "all of them insertions");
    }
    CHECK(rebuilds(&a, &b, edits, n), "rebuilding gives the whole file");

    /* both empty */
    a.count = 0;
    b.count = 0;
    CHECK(diff_compare(&a, &b, edits, 256, &n) && n == 0,
          "two empty files differ by nothing");



    static const char *rep1[] = { "x", "x", "x", "y" };
    static const char *rep2[] = { "x", "y" };
    a = make(a_text, a_len, rep1, 4);
    b = make(b_text, b_len, rep2, 2);
    CHECK(diff_compare(&a, &b, edits, 256, &n), "repeated lines compare");
    CHECK(rebuilds(&a, &b, edits, n),
          "and rebuild, a diff that matched the wrong `x` would still "
          "look plausible and would not reconstruct the file");



    static const char *shortl[] = { "ab" };
    static const char *longl[]  = { "abc" };
    a = make(a_text, a_len, shortl, 1);
    b = make(b_text, b_len, longl, 1);
    CHECK(!diff_same(&a, &b),
          "a line that is a prefix of the other is not the same line, "
          "comparing only the shorter length is the classic way to miss "
          "a truncation");
    CHECK(diff_compare(&a, &b, edits, 256, &n) && rebuilds(&a, &b, edits, n),
          "and it rebuilds correctly");



    a = make(a_text, a_len, was, 3);
    b = make(b_text, b_len, now, 3);
    CHECK(!diff_compare(&a, &b, edits, 2, &n),
          "running out of room is refused rather than truncated: half a "
          "diff is a set of changes that does not add up");

    {
        struct diff_lines big = { NULL, NULL, DIFF_MAX_LINES + 1 };
        CHECK(!diff_compare(&big, &b, edits, 256, &n),
              "and a file too big for the table is refused rather than "
              "taking the machine down trying");
    }

    if (failures == 0) printf("all good\n");
    return failures;
}
