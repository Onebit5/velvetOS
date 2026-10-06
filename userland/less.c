// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/less.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * less, a screenful at a time.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "syscall.h"
#include "textbuf.h"

static struct textbuf buf;
static unsigned cols = 80, rows = 25;
static size_t top;
static char message[128];
static char needle[64];

static unsigned page(void)
{
    return (rows > 2) ? rows - 1 : 1;
}

static char out[16 * 1024];
static size_t out_len;

static void emit(const char *s, size_t n)
{
    if (out_len + n > sizeof out) {
        write_fd(STDOUT, out, (long)out_len);
        out_len = 0;
    }
    if (n > sizeof out) {
        write_fd(STDOUT, s, (long)n);
        return;
    }
    memcpy(out + out_len, s, n);
    out_len += n;
}
static void emit_str(const char *s)
{
    emit(s, strlen(s));
}
static void flush(void)
{
    if (out_len > 0) { write_fd(STDOUT, out, (long)out_len); out_len = 0; }
}
static void move_to(unsigned r, unsigned c)
{
    char b[32];
    snprintf(b, sizeof b, "\033[%u;%uH", r + 1, c + 1);
    emit_str(b);
}

static void draw(void)
{
    emit_str("\033[2J");
    for (unsigned i = 0; i < page(); i++) {
        size_t n = top + i;
        if (n >= buf.count) {
            break;
        }
        move_to(i, 0);
        size_t len = buf.line[n].len;
        if (len > cols) { len = cols; }
        emit(buf.line[n].text, len);
    }

    move_to(rows - 1, 0);
    emit_str("\033[K");
    char bar[192];
    size_t last = top + page();
    if (last > buf.count) { last = buf.count; }
    snprintf(bar, sizeof bar,
             "  lines %lu-%lu of %lu   %s  [space/b move, / find, q quit]",
             (unsigned long)(top + 1), (unsigned long)last,
             (unsigned long)buf.count, message);
    emit_str(bar);
    move_to(rows - 1, (unsigned)strlen(bar));
    flush();
}

static void load(long fd)
{
    char chunk[1024];
    size_t line = 0;
    long n;
    while ((n = read_fd(fd, chunk, sizeof chunk)) > 0) {
        for (long i = 0; i < n; i++) {
            if (chunk[i] == '\n') {
                textbuf_insert_line(&buf, ++line, NULL, 0);
            } else if (chunk[i] != '\r')
{
                textbuf_insert(&buf, line, buf.line[line].len, &chunk[i], 1);
            }
        }
    }
    textbuf_forget_history(&buf);
}

static bool ask(const char *what, char *into, size_t max)
{
    size_t at = strlen(into);
    for (;;) {
        char bar[160];
        snprintf(bar, sizeof bar, "  %s%s", what, into);
        move_to(rows - 1, 0);
        emit_str("\033[K");
        emit_str(bar);
        move_to(rows - 1, (unsigned)strlen(bar));
        flush();

        long k = getkey();
        if (k == '\n' || k == '\r') { return true; }
        if (k == 0x1b || k == 0x07) { return false; }
        if ((k == '\b' || k == 0x7f) && at > 0) { into[--at] = '\0'; continue; }
        if (k >= ' ' && k < 0x7f && at + 1 < max) {
            into[at++] = (char)k;
            into[at] = '\0';
        }
    }
}

static void find_from(size_t line)
{
    if (needle[0] == '\0') {
        snprintf(message, sizeof message, "nothing to look for");
        return;
    }
    size_t l, c;
    if (textbuf_find(&buf, needle, line, 0, &l, &c)) {
        top = l;
        message[0] = '\0';
    } else if (textbuf_find(&buf, needle, 0, 0, &l, &c))
{
        top = l;
        snprintf(message, sizeof message, "wrapped");
    } else {
        snprintf(message, sizeof message, "not found");
    }
}

int main(int argc, char **argv)
{
    unsigned c = 0, r = 0;
    if (winsize(&c, &r) == 0 && c > 20 && r > 3) { cols = c; rows = r; }

    if (!textbuf_init(&buf)) {
        fprintf(STDERR_FILENO, "less: out of memory\n");
        return 1;
    }

    if (argc >= 2) {
        long fd = open(argv[1]);
        if (fd < 0) {
            fprintf(STDERR_FILENO, "less: cannot read %s\n", argv[1]);
            return 1;
        }
        load(fd);
        close(fd);
    } else {
        load(STDIN_FILENO);
    }

    long was = tty_mode(TTY_RAW);
    draw();

    bool running = true;
    while (running) {
        long k = getkey();
        if (k < 0) { continue; }
        message[0] = '\0';

        switch (k) {
        case ' ': case 0x06: case KEY_PGDN:
            top += page();
            break;
        case 'b': case 0x02: case KEY_PGUP:
            top = (top > page()) ? top - page() : 0;
            break;
        case 'j': case KEY_DOWN:
            top++;
            break;
        case 'k': case KEY_UP:
            if (top > 0) { top--; }
            break;
        case 'g': case KEY_HOME:
            top = 0;
            break;
        case 'G': case KEY_END:
            top = (buf.count > page()) ? buf.count - page() : 0;
            break;
        case '/':
            needle[0] = '\0';
            if (ask("find: ", needle, sizeof needle)) {
                find_from(0);
            }
            break;
        case 'n':
            find_from(top + 1);
            break;
        case 'q': case 0x18:
            running = false;
            break;
        default:
            break;
        }

        /*
         * never past the end: a pager showing a screen of nothing has
         * lost the file for the person reading it
         */
        if (buf.count > page() && top > buf.count - page()) {
            top = buf.count - page();
        } else if (buf.count <= page()) {
            top = 0;
        }
        draw();
    }

    tty_mode((unsigned long)was);
    emit_str("\033[2J");
    move_to(0, 0);
    flush();
    textbuf_free(&buf);
    return 0;
}
