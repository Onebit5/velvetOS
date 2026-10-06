// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/lines.h
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * reading input a line at a time.
 */

#ifndef USER_LINES_H
#define USER_LINES_H

#include "syscall.h"

/* reading input a line at a time. */

#define LINES_BUF  1024
#define LINE_CAP   512

struct lines {
    long fd;
    char buf[LINES_BUF];
    long len;               /* how much of buf is real */
    long at;                /* how far through it the program is */
    bool done;              /* the fd said end of file */
};

static inline void lines_open(struct lines *l, long fd)
{
    l->fd = fd;
    l->len = 0;
    l->at = 0;
    l->done = false;
}

/* the next line, without its newline, NUL-terminated. */
static inline bool lines_next(struct lines *l, char *out, long cap,
                              long *out_len)
{
    long n = 0;

    for (;;) {
        if (l->at >= l->len) {
            if (l->done) {
                break;
            }
            l->len = read_fd(l->fd, l->buf, LINES_BUF);
            l->at = 0;
            if (l->len <= 0) {
                l->len = 0;
                l->done = true;
                break;
            }
        }

        char c = l->buf[l->at++];
        if (c == '\n') {
            out[n < cap - 1 ? n : cap - 1] = '\0';
            *out_len = n;
            return true;
        }
        if (n < cap - 1) {
            out[n++] = c;
        }
        /*
         * a line longer than the buffer is truncated rather than split
         * into two lines, because splitting it would silently turn one
         * line into two and change every count downstream
         */
    }

    out[n < cap - 1 ? n : cap - 1] = '\0';
    *out_len = n;
    return n > 0;
}

/* write a line back out, newline and all */
static inline void put_line(const char *s)
{
    write(s);
    write("\n");
}

#endif
