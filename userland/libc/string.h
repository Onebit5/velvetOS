// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/libc/string.h
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * strncpy pads to `n` and does *not* always terminate, which is what it
 * has always done and is why nobody should reach for it first.
 */

#ifndef LIBC_STRING_H
#define LIBC_STRING_H

#include <stddef.h>

void  *memcpy(void *dst, const void *src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
void  *memset(void *dst, int c, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
void  *memchr(const void *p, int c, size_t n);

size_t strlen(const char *s);
size_t strnlen(const char *s, size_t max);
char  *strcpy(char *dst, const char *src);

/*
 * strncpy pads to `n` and does *not* always terminate, which is what it
 * has always done and is why nobody should reach for it first. it is
 * here because ported code uses it
 */
char  *strncpy(char *dst, const char *src, size_t n);

char  *strcat(char *dst, const char *src);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
int    strcasecmp(const char *a, const char *b);
char  *strchr(const char *s, int c);
char  *strrchr(const char *s, int c);
char  *strstr(const char *hay, const char *needle);
char  *strdup(const char *s);

#endif
