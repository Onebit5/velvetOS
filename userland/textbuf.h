// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/textbuf.h
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the thing an editor edits.
 */

#ifndef USER_TEXTBUF_H
#define USER_TEXTBUF_H

#include <stddef.h>
#include <stdbool.h>

/* the thing an editor edits. */

struct line {
    char  *text;        /* not terminated: `len` is the truth */
    size_t len, cap;
};

/* what was done, so it can be undone. */
enum edit_kind {
    EDIT_INSERT_TEXT,   /* characters went in at (line, col) */
    EDIT_DELETE_TEXT,   /* and this is what came out */
    EDIT_SPLIT_LINE,    /* enter, at (line, col) */
    EDIT_JOIN_LINE,     /* backspace at the start of a line */
    EDIT_INSERT_LINE,
    EDIT_DELETE_LINE
};

struct edit {
    enum edit_kind kind;
    size_t line, col;
    char  *text;        /* what was inserted or removed */
    size_t len;
};

#define UNDO_MAX 256

struct textbuf {
    struct line *line;
    size_t count, cap;

    /* the undo stack, and how much of it has been walked back. */
    struct edit undo[UNDO_MAX];
    size_t undo_count;
    size_t undo_at;

    bool dirty;
    bool coalescing;    /* the last edit may absorb the next one */
};

bool textbuf_init(struct textbuf *b);
void textbuf_free(struct textbuf *b);

/* the whole of the editing, and every one of them records an undo */
bool textbuf_insert(struct textbuf *b, size_t line, size_t col,
                    const char *s, size_t n);
bool textbuf_delete(struct textbuf *b, size_t line, size_t col, size_t n);
bool textbuf_split(struct textbuf *b, size_t line, size_t col);
bool textbuf_join(struct textbuf *b, size_t line);
bool textbuf_insert_line(struct textbuf *b, size_t at, const char *s,
                         size_t n);
bool textbuf_delete_line(struct textbuf *b, size_t at);

/* true if something was undone. */
bool textbuf_undo(struct textbuf *b, size_t *line, size_t *col);
bool textbuf_redo(struct textbuf *b, size_t *line, size_t *col);

/* end the run of typing that undo would coalesce. */
void textbuf_break_run(struct textbuf *b);

/* this is the file as it stands: forget how it got here. */
void textbuf_forget_history(struct textbuf *b);

/* find `needle` at or after (line, col). */
bool textbuf_find(const struct textbuf *b, const char *needle,
                  size_t from_line, size_t from_col,
                  size_t *at_line, size_t *at_col);

#endif
