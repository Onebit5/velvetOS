// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/libc/stdio.h
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * there are no FILE streams here, and that is deliberate rather than
 * unfinished.
 */

#ifndef LIBC_STDIO_H
#define LIBC_STDIO_H

#include <stddef.h>
#include <stdarg.h>
#include "format.h"

/*
 * there are no FILE streams here, and that is deliberate rather than
 * unfinished: a stream is a buffer plus a descriptor plus a position
 * plus a set of rules about when to flush, and every one of those is a
 * decision a program can make better than a library can. what is here
 * is the formatter and the descriptors, which is what printf actually
 * is once the ceremony is removed
 */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

int printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int fprintf(int fd, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
/*
 * opening a file, which is the one thing here that is not formatting.
 * a program that had to include syscall.h for this would be a program
 * that knows what kernel it is on, which is the thing this is trying
 * to stop being true
 */
/*
 * the terminal, as a mode. see userland/syscall.h for what the flags mean
 *, a program that asks for raw must put back what it was given, or
 * the shell that follows it inherits a terminal nobody can type at
 */
long term_mode_set(unsigned long flags);
long term_mode_get(void);
long term_size(unsigned *cols, unsigned *rows);

long open_file(const char *path);
long close_file(long fd);
long read_file(long fd, void *buf, long len);

int puts(const char *s);
int putchar(int c);

/*
 * read a line, without the newline. returns the length, 0 at end of
 * input, or -1 if it was interrupted, which is a real
 * third answer rather than an impossible one
 */
long getline_fd(int fd, char *out, size_t max);

#endif
