// SPDX-License-Identifier: GPL-2.0-only
/*
 * tests/test_textbuf.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * host-side test for the thing an editor edits.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../userland/textbuf.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
} while (0)

/*
 * the whole buffer as one string, with newlines between lines, so a
 * test can say what the text should be rather than picking at it
 */
static char *flatten(const struct textbuf *b)
{
    static char out[8192];
    size_t at = 0;
    for (size_t i = 0; i < b->count; i++) {
        if (i > 0 && at < sizeof out - 1) {
            out[at++] = '\n';
        }
        for (size_t c = 0; c < b->line[i].len && at < sizeof out - 1; c++) {
            out[at++] = b->line[i].text[c];
        }
    }
    out[at] = '\0';
    return out;
}

static void put(struct textbuf *b, size_t line, const char *s)
{
    textbuf_insert(b, line, b->line[line].len, s, strlen(s));
}

int main(void)
{
    struct textbuf b;
    size_t l, c;



    CHECK(textbuf_init(&b), "a buffer is made");
    CHECK(b.count == 1,
          "and has one line, not none, a buffer with no lines has "
          "nowhere for the cursor to be, and every caller would need a "
          "special case for it");
    CHECK(strcmp(flatten(&b), "") == 0, "which is empty");



    put(&b, 0, "hello");
    CHECK(strcmp(flatten(&b), "hello") == 0, "text goes in");

    textbuf_insert(&b, 0, 0, ">", 1);
    CHECK(strcmp(flatten(&b), ">hello") == 0, "including at the front");

    textbuf_delete(&b, 0, 0, 1);
    CHECK(strcmp(flatten(&b), "hello") == 0, "and comes out again");

    textbuf_split(&b, 0, 2);
    CHECK(strcmp(flatten(&b), "he\nllo") == 0, "a line splits in two");
    CHECK(b.count == 2, "making two lines");

    textbuf_join(&b, 0);
    CHECK(strcmp(flatten(&b), "hello") == 0, "and joins back");
    CHECK(b.count == 1, "making one again");



    textbuf_free(&b);
    textbuf_init(&b);
    for (int i = 0; i < 5000; i++) {
        char row[64];
        snprintf(row, sizeof row, "line %d", i);
        textbuf_insert_line(&b, b.count, row, strlen(row));
    }
    CHECK(b.count == 5001,
          "five thousand lines, where the old fixed array stopped at six "
          "hundred and truncated anything past it");

    /* and a line longer than the old 240-character limit */
    {
        char big[4096];
        memset(big, 'x', sizeof big);
        textbuf_insert(&b, 0, 0, big, sizeof big);
        CHECK(b.line[0].len == sizeof big,
              "and a line of four thousand characters, where the old one "
              "held 240 and quietly dropped the rest");
    }



    textbuf_free(&b);
    textbuf_init(&b);
    put(&b, 0, "one");
    textbuf_break_run(&b);
    textbuf_split(&b, 0, 3);
    put(&b, 1, "two");
    textbuf_break_run(&b);

    const char *want = "one\ntwo";
    CHECK(strcmp(flatten(&b), want) == 0, "some text is built up");

    CHECK(textbuf_undo(&b, &l, &c), "and can be undone");
    CHECK(strcmp(flatten(&b), "one\n") == 0, "one step at a time");
    CHECK(textbuf_undo(&b, &l, &c), "again");
    CHECK(strcmp(flatten(&b), "one") == 0, "taking back the split");
    CHECK(textbuf_undo(&b, &l, &c), "and again");
    CHECK(strcmp(flatten(&b), "") == 0, "back to nothing");
    CHECK(!textbuf_undo(&b, &l, &c), "and there is nothing left to undo");

    /* a delete must record what it removed *before* removing it. */
    textbuf_free(&b);
    textbuf_init(&b);
    put(&b, 0, "abcdefgh");
    textbuf_forget_history(&b);
    textbuf_delete(&b, 0, 2, 3);            /* take out "cde" */
    CHECK(strcmp(flatten(&b), "abfgh") == 0, "three characters come out");
    textbuf_undo(&b, &l, &c);
    CHECK(strcmp(flatten(&b), "abcdefgh") == 0,
          "and undo puts back exactly those three, recorded before "
          "they went, since afterwards that offset holds whatever "
          "followed them");

    /*
     * the last line is never deleted: a buffer with none has nowhere for
     * the cursor to be
     */
    textbuf_free(&b);
    textbuf_init(&b);
    put(&b, 0, "only");
    CHECK(!textbuf_delete_line(&b, 0),
          "the only line cannot be deleted");
    CHECK(b.count == 1 && strcmp(flatten(&b), "only") == 0,
          "and is still there afterwards, a buffer with no lines has "
          "nowhere for the cursor to be, and every caller would need a "
          "special case for it");

    /* a long mixed sequence, undone all the way. */

    textbuf_free(&b);
    textbuf_init(&b);
    put(&b, 0, "the quick brown fox");
    textbuf_break_run(&b);

    /* this is the file as it stands. */
    textbuf_forget_history(&b);
    CHECK(!b.dirty,
          "a file just opened has no unsaved changes, however many "
          "operations it took to build it");

    char original[8192];
    strcpy(original, flatten(&b));
    size_t marks = 0;

    for (int round = 0; round < 40; round++) {
        switch (round % 6) {
        case 0: textbuf_insert(&b, 0, 0, "A", 1); break;
        case 1: textbuf_split(&b, 0, 2); break;
        case 2: textbuf_insert_line(&b, 1, "inserted", 8); break;
        case 3: if (b.count > 1) { textbuf_delete_line(&b, 1); } break;
        case 4: if (b.line[0].len > 3) { textbuf_delete(&b, 0, 1, 2); } break;
        case 5: if (b.count > 1) { textbuf_join(&b, 0); } break;
        }
        textbuf_break_run(&b);
        marks++;
    }
    CHECK(strcmp(flatten(&b), original) != 0, "forty edits change the text");

    size_t undone = 0;
    while (textbuf_undo(&b, &l, &c)) {
        undone++;
    }
    CHECK(undone > 0, "and all of them come back off");
    CHECK(strcmp(flatten(&b), original) == 0,
          "arriving at exactly the text it started from, which is the "
          "one property undo has that can be checked rather than tried");

    /* redo walks forward again to the same place */
    char after_undo[8192];
    strcpy(after_undo, flatten(&b));
    size_t redone = 0;
    while (textbuf_redo(&b, &l, &c)) {
        redone++;
    }
    CHECK(redone == undone, "redo goes as far forward as undo went back");
    CHECK(strcmp(flatten(&b), original) != 0,
          "and the text is the edited one again");

    /* an edit after an undo throws away what was undone */
    while (textbuf_undo(&b, &l, &c)) { }
    CHECK(strcmp(flatten(&b), after_undo) == 0, "wound all the way back");
    textbuf_insert(&b, 0, 0, "Z", 1);
    CHECK(!textbuf_redo(&b, &l, &c),
          "and an edit after an undo discards the redo, the "
          "alternative is a tree of histories, which is a different "
          "program");



    textbuf_free(&b);
    textbuf_init(&b);
    for (const char *p = "hello"; *p; p++) {
        textbuf_insert(&b, 0, b.line[0].len, p, 1);
    }
    CHECK(strcmp(flatten(&b), "hello") == 0, "five characters typed");
    CHECK(textbuf_undo(&b, &l, &c), "and one undo");
    CHECK(strcmp(flatten(&b), "") == 0,
          "takes back all of them, an undo that removed one character "
          "at a time would mean fifty presses to lose a word, which is "
          "not an editor anybody would use");

    /* but typing somewhere else is a new thing to undo */
    textbuf_free(&b);
    textbuf_init(&b);
    textbuf_insert(&b, 0, 0, "ab", 2);
    textbuf_insert(&b, 0, 0, "X", 1);       /* not where the last ended */
    CHECK(strcmp(flatten(&b), "Xab") == 0, "typing at the front");
    textbuf_undo(&b, &l, &c);
    CHECK(strcmp(flatten(&b), "ab") == 0,
          "is its own undo, because a run continues only where the last "
          "one ended, which is what a person means by it");

    /* moving the cursor ends a run, and that is the caller's job to say */
    textbuf_free(&b);
    textbuf_init(&b);
    textbuf_insert(&b, 0, 0, "ab", 2);
    textbuf_break_run(&b);
    textbuf_insert(&b, 0, 2, "cd", 2);
    textbuf_undo(&b, &l, &c);
    CHECK(strcmp(flatten(&b), "ab") == 0,
          "a break in the run makes the next typing separate, even where "
          "it continues from");



    textbuf_free(&b);
    textbuf_init(&b);
    for (int i = 0; i < UNDO_MAX * 2; i++) {
        textbuf_insert(&b, 0, 0, "x", 1);
        textbuf_break_run(&b);
    }
    undone = 0;
    while (textbuf_undo(&b, &l, &c)) {
        undone++;
    }
    CHECK(undone == UNDO_MAX,
          "the history is bounded, an unbounded one is a program that "
          "grows until it dies while somebody is typing into it");
    CHECK(b.line[0].len == UNDO_MAX,
          "and what is left is what the forgotten half had already done");



    textbuf_free(&b);
    textbuf_init(&b);
    put(&b, 0, "the fox");
    textbuf_insert_line(&b, 1, "a fox here", 10);
    textbuf_insert_line(&b, 2, "and a fox", 9);

    CHECK(textbuf_find(&b, "fox", 0, 0, &l, &c), "a word is found");
    CHECK(l == 0 && c == 4, "at the first place it occurs");

    CHECK(textbuf_find(&b, "fox", l, c + 1, &l, &c),
          "and searching again from just past it");
    CHECK(l == 1 && c == 2,
          "finds the *next* one, margaret only ever searched from the "
          "top, so pressing again found the same match forever");

    CHECK(textbuf_find(&b, "fox", l, c + 1, &l, &c), "and the one after");
    CHECK(l == 2 && c == 6, "on the last line");

    CHECK(!textbuf_find(&b, "fox", l, c + 1, &l, &c),
          "and then there are no more between here and the end, which "
          "is a different answer from `not in the file`, and the caller "
          "decides whether to wrap");

    CHECK(!textbuf_find(&b, "zebra", 0, 0, &l, &c), "a word that is absent");
    CHECK(!textbuf_find(&b, "", 0, 0, &l, &c), "and an empty needle");

    /* a needle longer than the line it might be on */
    CHECK(!textbuf_find(&b, "a much longer needle than any line", 0, 0,
                        &l, &c),
          "a needle longer than every line is not found rather than read "
          "past the end of one");

    textbuf_free(&b);
    CHECK(b.count == 0, "and freeing puts it all back");

    if (failures == 0) printf("all good\n");
    return failures;
}
