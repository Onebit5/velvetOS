// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/libc/stdlib.h
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the allocator gets its memory from `mmap` and never gives any back.
 */

#ifndef LIBC_STDLIB_H
#define LIBC_STDLIB_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * the allocator gets its memory from `mmap` and never gives any back.
 * see stdlib.c: releasing a mapping means only doing so when every
 * block in it is free, which is a real allocator's job, this one is
 * enough to build a compiler with
 */
void *malloc(size_t n);
void  free(void *p);
void *calloc(size_t count, size_t size);
void *realloc(void *p, size_t n);

/*
 * `end` is how a caller tells "0" from "not a number", and is the whole
 * reason to use this rather than atoi
 */
long strtol(const char *s, char **end, int base);
int  atoi(const char *s);

void abort(void);

#endif
