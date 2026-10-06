// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/libc/stdio.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the half of stdio that reaches a descriptor.
 */

#include "stdio.h"
#include "string.h"
#include "../syscall.h"

/*
 * the buffer printf formats into before writing.
 *
 * one write per printf rather than one per character: a syscall for
 * every byte is what made the first version of `theodore` slower than
 * the network it was serving
 */
#define PRINT_MAX 1024

int fprintf(int fd, const char *fmt, ...)
{
    char buf[PRINT_MAX];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    size_t len = (n < 0) ? 0 : (size_t)n;
    if (len > sizeof buf - 1) {
        len = sizeof buf - 1;    /* it was cut short; write what fit */
    }
    write_fd(fd, buf, (long)len);
    return n;
}

int printf(const char *fmt, ...)
{
    char buf[PRINT_MAX];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    size_t len = (n < 0) ? 0 : (size_t)n;
    if (len > sizeof buf - 1) {
        len = sizeof buf - 1;
    }
    write_fd(STDOUT_FILENO, buf, (long)len);
    return n;
}

long term_mode_set(unsigned long flags)
{
    return tty_mode(flags);
}
long term_mode_get(void)
{
    return tty_mode_get();
}
long term_size(unsigned *cols, unsigned *rows)
{
    return winsize(cols, rows);
}

long open_file(const char *path)
{
    return open(path);
}
long close_file(long fd)
{
    return close(fd);
}
long read_file(long fd, void *buf, long len)
{
    return read_fd(fd, buf, len);
}

int puts(const char *s)
{
    size_t n = strlen(s);
    write_fd(STDOUT_FILENO, s, (long)n);
    write_fd(STDOUT_FILENO, "\n", 1);
    return (int)n + 1;
}

int putchar(int c)
{
    char b = (char)c;
    write_fd(STDOUT_FILENO, &b, 1);
    return c;
}

long getline_fd(int fd, char *out, size_t max)
{
    size_t at = 0;
    for (;;) {
        char c;
        long n = read_fd(fd, &c, 1);

        if (n < 0) {
            return -1;      /* interrupted, which since 0.3.7 happens */
        }
        if (n == 0) {
            return (at == 0) ? 0 : (long)at;
        }
        if (c == '\n') {
            break;
        }
        if (at + 1 < max) {
            out[at++] = c;
        }
        /*
         * and anything past the buffer is dropped rather than being
         * left to arrive at the front of the next line
         */
    }
    out[at] = '\0';
    return (long)at;
}
