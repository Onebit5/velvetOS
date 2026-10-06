// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/textbuf.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the thing an editor edits.
 */

#include "textbuf.h"
#include <stdlib.h>
#include <string.h>



static bool line_room(struct line *l, size_t want)
{
    if (l->cap >= want) {
        return true;
    }
    size_t cap = (l->cap == 0) ? 32 : l->cap;
    while (cap < want) {
        cap *= 2;
    }
    char *fresh = realloc(l->text, cap);
    if (fresh == NULL) {
        return false;
    }
    l->text = fresh;
    l->cap  = cap;
    return true;
}

static bool buf_room(struct textbuf *b, size_t want)
{
    if (b->cap >= want) {
        return true;
    }
    size_t cap = (b->cap == 0) ? 64 : b->cap;
    while (cap < want) {
        cap *= 2;
    }
    struct line *fresh = realloc(b->line, cap * sizeof *fresh);
    if (fresh == NULL) {
        return false;
    }
    /* the new tail has to start empty. */
    memset(fresh + b->cap, 0, (cap - b->cap) * sizeof *fresh);
    b->line = fresh;
    b->cap  = cap;
    return true;
}

bool textbuf_init(struct textbuf *b)
{
    memset(b, 0, sizeof *b);
    if (!buf_room(b, 1)) {
        return false;
    }
    /* a file with nothing in it still has one line: the empty one the cursor sits on. */
    b->count = 1;
    return true;
}

void textbuf_free(struct textbuf *b)
{
    for (size_t i = 0; i < b->cap; i++) {
        free(b->line[i].text);
    }
    free(b->line);
    for (size_t i = 0; i < b->undo_count; i++) {
        free(b->undo[i].text);
    }
    memset(b, 0, sizeof *b);
}



static void drop_from(struct textbuf *b, size_t at)
{
    for (size_t i = at; i < b->undo_count; i++) {
        free(b->undo[i].text);
        b->undo[i].text = NULL;
    }
    b->undo_count = at;
}

static bool push(struct textbuf *b, enum edit_kind kind, size_t line,
                 size_t col, const char *text, size_t len)
{
    /* an edit after an undo throws away what was undone. */
    drop_from(b, b->undo_at);

    if (b->undo_count == UNDO_MAX) {
        /*
         * the oldest goes. a bounded history is a promise that can be
         * kept; an unbounded one is a program that grows until it dies
         * while somebody is typing into it
         */
        free(b->undo[0].text);
        memmove(b->undo, b->undo + 1, (UNDO_MAX - 1) * sizeof *b->undo);
        b->undo_count--;
        if (b->undo_at > 0) {
            b->undo_at--;
        }
    }

    struct edit *e = &b->undo[b->undo_count];
    e->kind = kind;
    e->line = line;
    e->col  = col;
    e->len  = len;
    e->text = NULL;

    if (len > 0 && text != NULL) {
        e->text = malloc(len);
        if (e->text == NULL) {
            return false;
        }
        memcpy(e->text, text, len);
    }
    b->undo_count++;
    b->undo_at = b->undo_count;
    b->dirty   = true;
    return true;
}

void textbuf_break_run(struct textbuf *b)
{
    b->coalescing = false;
}

void textbuf_forget_history(struct textbuf *b)
{
    drop_from(b, 0);
    b->undo_at    = 0;
    b->dirty      = false;
    b->coalescing = false;
}



/* the half that changes the text and records nothing. */
static bool raw_insert(struct textbuf *b, size_t line, size_t col,
                       const char *s, size_t n)
{
    if (line >= b->count) {
        return false;
    }
    struct line *l = &b->line[line];
    if (col > l->len) {
        return false;
    }
    if (!line_room(l, l->len + n)) {
        return false;
    }
    memmove(l->text + col + n, l->text + col, l->len - col);
    memcpy(l->text + col, s, n);
    l->len += n;
    return true;
}

static bool raw_delete(struct textbuf *b, size_t line, size_t col,
                       size_t n)
{
    if (line >= b->count) {
        return false;
    }
    struct line *l = &b->line[line];
    if (col + n > l->len) {
        return false;
    }
    memmove(l->text + col, l->text + col + n, l->len - col - n);
    l->len -= n;
    return true;
}

static bool raw_insert_line(struct textbuf *b, size_t at, const char *s,
                            size_t n)
{
    if (at > b->count || !buf_room(b, b->count + 1)) {
        return false;
    }
    memmove(b->line + at + 1, b->line + at,
            (b->count - at) * sizeof *b->line);
    memset(&b->line[at], 0, sizeof b->line[at]);
    b->count++;

    if (n > 0) {
        if (!line_room(&b->line[at], n)) {
            return false;
        }
        memcpy(b->line[at].text, s, n);
        b->line[at].len = n;
    }
    return true;
}

/* trusts its caller about the last line. */
static bool raw_delete_line(struct textbuf *b, size_t at)
{
    if (at >= b->count) {
        return false;
    }
    free(b->line[at].text);
    memmove(b->line + at, b->line + at + 1,
            (b->count - at - 1) * sizeof *b->line);
    b->count--;
    memset(&b->line[b->count], 0, sizeof b->line[b->count]);
    return true;
}

bool textbuf_insert(struct textbuf *b, size_t line, size_t col,
                    const char *s, size_t n)
{
    if (n == 0) {
        return true;
    }
    if (!raw_insert(b, line, col, s, n)) {
        return false;
    }

    /* consecutive typing becomes one undo entry. */
    if (b->coalescing && b->undo_at == b->undo_count && b->undo_count > 0) {
        struct edit *last = &b->undo[b->undo_count - 1];
        if (last->kind == EDIT_INSERT_TEXT && last->line == line
         && last->col + last->len == col) {
            char *bigger = realloc(last->text, last->len + n);
            if (bigger != NULL) {
                memcpy(bigger + last->len, s, n);
                last->text = bigger;
                last->len += n;
                b->dirty = true;
                return true;
            }
        }
    }

    if (!push(b, EDIT_INSERT_TEXT, line, col, s, n)) {
        return false;
    }
    b->coalescing = true;
    return true;
}

bool textbuf_delete(struct textbuf *b, size_t line, size_t col, size_t n)
{
    if (n == 0) {
        return true;
    }
    if (line >= b->count || col + n > b->line[line].len) {
        return false;
    }
    /*
     * what is about to go, recorded *before* it goes, which is the
     * whole difference between an undo that works and one that puts
     * back whatever happened to be there afterwards
     */
    if (!push(b, EDIT_DELETE_TEXT, line, col, b->line[line].text + col, n)) {
        return false;
    }
    b->coalescing = false;
    return raw_delete(b, line, col, n);
}

bool textbuf_split(struct textbuf *b, size_t line, size_t col)
{
    if (line >= b->count || col > b->line[line].len) {
        return false;
    }
    struct line *l = &b->line[line];
    size_t tail = l->len - col;

    if (!raw_insert_line(b, line + 1, l->text + col, tail)) {
        return false;
    }
    b->line[line].len = col;

    if (!push(b, EDIT_SPLIT_LINE, line, col, NULL, 0)) {
        return false;
    }
    b->coalescing = false;
    return true;
}

bool textbuf_join(struct textbuf *b, size_t line)
{
    if (line + 1 >= b->count) {
        return false;
    }
    size_t at = b->line[line].len;
    struct line *next = &b->line[line + 1];

    if (!raw_insert(b, line, at, next->text, next->len)) {
        return false;
    }
    if (!raw_delete_line(b, line + 1)) {
        return false;
    }
    if (!push(b, EDIT_JOIN_LINE, line, at, NULL, 0)) {
        return false;
    }
    b->coalescing = false;
    return true;
}

bool textbuf_insert_line(struct textbuf *b, size_t at, const char *s,
                         size_t n)
{
    if (!raw_insert_line(b, at, s, n)) {
        return false;
    }
    if (!push(b, EDIT_INSERT_LINE, at, 0, NULL, 0)) {
        return false;
    }
    b->coalescing = false;
    return true;
}

bool textbuf_delete_line(struct textbuf *b, size_t at)
{
    /*
     * the one place the rule lives: a buffer always keeps a line, so
     * there is somewhere for the cursor to be
     */
    if (at >= b->count || b->count == 1) {
        return false;
    }
    if (!push(b, EDIT_DELETE_LINE, at, 0, b->line[at].text,
              b->line[at].len)) {
        return false;
    }
    b->coalescing = false;
    return raw_delete_line(b, at);
}



/*
 * every edit is undone by doing its opposite, with the *raw* operations
 * so that undoing does not itself get recorded
 */
static bool reverse(struct textbuf *b, const struct edit *e, bool forward,
                    size_t *line, size_t *col)
{
    enum edit_kind kind = e->kind;

    if (!forward) {
        switch (kind) {
        case EDIT_INSERT_TEXT: kind = EDIT_DELETE_TEXT; break;
        case EDIT_DELETE_TEXT: kind = EDIT_INSERT_TEXT; break;
        case EDIT_SPLIT_LINE:  kind = EDIT_JOIN_LINE;   break;
        case EDIT_JOIN_LINE:   kind = EDIT_SPLIT_LINE;  break;
        case EDIT_INSERT_LINE: kind = EDIT_DELETE_LINE; break;
        case EDIT_DELETE_LINE: kind = EDIT_INSERT_LINE; break;
        }
    }

    *line = e->line;
    *col  = e->col;

    switch (kind) {
    case EDIT_INSERT_TEXT:
        if (!raw_insert(b, e->line, e->col, e->text, e->len)) {
            return false;
        }
        *col = e->col + e->len;
        return true;
    case EDIT_DELETE_TEXT:
        return raw_delete(b, e->line, e->col, e->len);
    case EDIT_SPLIT_LINE: {
        struct line *l = &b->line[e->line];
        size_t tail = l->len - e->col;
        if (!raw_insert_line(b, e->line + 1, l->text + e->col, tail)) {
            return false;
        }
        b->line[e->line].len = e->col;
        *line = e->line + 1;
        *col  = 0;
        return true;
    }
    case EDIT_JOIN_LINE: {
        if (e->line + 1 >= b->count) {
            return false;
        }
        struct line *next = &b->line[e->line + 1];
        if (!raw_insert(b, e->line, e->col, next->text, next->len)) {
            return false;
        }
        return raw_delete_line(b, e->line + 1);
    }
    case EDIT_INSERT_LINE:
        return raw_insert_line(b, e->line, e->text, e->len);
    case EDIT_DELETE_LINE:
        return raw_delete_line(b, e->line);
    }
    return false;
}

bool textbuf_undo(struct textbuf *b, size_t *line, size_t *col)
{
    if (b->undo_at == 0) {
        return false;
    }
    b->undo_at--;
    b->coalescing = false;

    /*
     * a delete-line entry carries the text it removed, so undoing it
     * needs that text put back where the insert-line half would look
     * for it. the fields already say so; this is only here because the
     * two kinds share a struct and it is worth being explicit
     */
    return reverse(b, &b->undo[b->undo_at], false, line, col);
}

bool textbuf_redo(struct textbuf *b, size_t *line, size_t *col)
{
    if (b->undo_at >= b->undo_count) {
        return false;
    }
    bool ok = reverse(b, &b->undo[b->undo_at], true, line, col);
    b->undo_at++;
    b->coalescing = false;
    return ok;
}



bool textbuf_find(const struct textbuf *b, const char *needle,
                  size_t from_line, size_t from_col,
                  size_t *at_line, size_t *at_col)
{
    size_t n = strlen(needle);
    if (n == 0) {
        return false;
    }

    for (size_t i = from_line; i < b->count; i++) {
        const struct line *l = &b->line[i];
        size_t start = (i == from_line) ? from_col : 0;

        if (l->len < n) {
            continue;
        }
        for (size_t c = start; c + n <= l->len; c++) {
            if (memcmp(l->text + c, needle, n) == 0) {
                *at_line = i;
                *at_col  = c;
                return true;
            }
        }
    }

    /* not found *between here and the end*, which is a different answer from "not in the file". */
    return false;
}
