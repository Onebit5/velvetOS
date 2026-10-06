// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/libc/format.h
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the formatter, separated from the rest of stdio so that it can be
 * compiled with nothing underneath it.
 */

#ifndef LIBC_FORMAT_H
#define LIBC_FORMAT_H

#include <stddef.h>
#include <stdarg.h>

/*
 * the formatter, separated from the rest of stdio so that it can be
 * compiled with nothing underneath it. everything else in stdio ends in
 * a write, and a write is a syscall; none of this is
 */

/*
 * the one worth using: it cannot overrun, and it returns what it
 * *would* have written, so a caller can tell it was cut short, and can
 * ask how big a buffer needs to be by passing a max of zero
 */
int vsnprintf(char *out, size_t max, const char *fmt, va_list ap);
int snprintf(char *out, size_t max, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/* unbounded, and here only because ported code uses it */
int sprintf(char *out, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#endif
