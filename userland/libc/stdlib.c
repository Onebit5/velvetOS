// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/libc/stdlib.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the allocator, and the odds and ends that come with it.
 */

#include "stdlib.h"
#include "string.h"

/*
 * where pages come from.
 *
 * a seam, and a deliberate one. the allocator wants exactly two things
 * from the world, more address space, and a way to give up, and
 * pulling in the whole of syscall.h to get them makes this file
 * untestable: every declaration in there collides with a host libc's,
 * so the test could never compile the real allocator at all.
 *
 * one named hook instead. the kernel's mmap on a real machine, and a
 * slab of host memory in tests/test_libc.c, which means what is
 * tested is the allocator itself, with only where the memory came from
 * replaced
 */
#ifdef VELVETOS_HOSTED
void *libc_get_pages(long len);
#else
#include "../syscall.h"
static void *libc_get_pages(long len)
{
    return mmap(len);
}
#endif

/*
 * what a block knows about itself.
 *
 * `size` is the payload, not counting this header. keeping it that way
 * means malloc(n) and the size stored are the same number, which is one
 * fewer place to be off by sixteen
 */
struct block {
    size_t        size;
    struct block *next;     /* only meaningful while free */
};

#define HEADER    (sizeof(struct block))

/*
 * every payload starts on a sixteen-byte boundary, which is what the
 * abi requires of anything that might hold a long double or be handed
 * to a function expecting an aligned stack. it costs a few bytes per
 * block and removes an entire class of fault that only appears on some
 * types
 */
#define ALIGN     16
#define ALIGN_UP(n) (((n) + (ALIGN - 1)) & ~(size_t)(ALIGN - 1))

/*
 * how much to ask the kernel for at once. asking per allocation would
 * be a syscall per malloc and a page for a four-byte string
 */
#define CHUNK     (64 * 1024)

static struct block *free_list;


static void insert_free(struct block *b)
{
    struct block **at = &free_list;
    while (*at != NULL && *at < b) {
        at = &(*at)->next;
    }
    b->next = *at;
    *at = b;

    /* forward: does this block end exactly where the next one begins */
    uint8_t *end = (uint8_t *)b + HEADER + b->size;
    if (b->next != NULL && (uint8_t *)b->next == end) {
        b->size += HEADER + b->next->size;
        b->next  = b->next->next;
    }

    /*
     * and backward, which needs the block before it in the list, the
     * whole reason the list is sorted. without this half, freeing in
     * ascending order coalesces perfectly and freeing in descending
     * order does not coalesce at all, which is a bug that hides behind
     * whichever order the first test happened to use
     */
    if (at != &free_list) {
        struct block *prev = (struct block *)((uint8_t *)at
                             - __builtin_offsetof(struct block, next));
        if ((uint8_t *)prev + HEADER + prev->size == (uint8_t *)b) {
            prev->size += HEADER + b->size;
            prev->next  = b->next;
        }
    }
}

static bool grow(size_t need)
{
    size_t want = need + HEADER;
    if (want < CHUNK) {
        want = CHUNK;
    }
    want = ALIGN_UP(want);

    void *mem = libc_get_pages((long)want);
    if (mem == NULL) {
        return false;
    }
    struct block *b = (struct block *)mem;
    b->size = want - HEADER;
    b->next = NULL;
    insert_free(b);
    return true;
}

void *malloc(size_t n)
{
    if (n == 0) {
        /*
         * a distinct, valid, freeable pointer. returning NULL would be
         * legal and is worse: every caller then has to tell "no memory"
         * apart from "you asked for none"
         */
        n = 1;
    }
    n = ALIGN_UP(n);

    for (int attempt = 0; attempt < 2; attempt++) {
        struct block **at = &free_list;
        while (*at != NULL) {
            struct block *b = *at;
            if (b->size >= n) {
                /*
                 * split, but only if what is left could hold a header
                 * and something worth having. splitting off sixteen
                 * bytes of unusable remainder is how a heap fills with
                 * blocks nobody can use
                 */
                if (b->size >= n + HEADER + ALIGN) {
                    struct block *rest =
                        (struct block *)((uint8_t *)b + HEADER + n);
                    rest->size = b->size - n - HEADER;
                    rest->next = b->next;
                    b->size    = n;
                    *at = rest;
                } else {
                    *at = b->next;
                }
                b->next = NULL;
                return (uint8_t *)b + HEADER;
            }
            at = &b->next;
        }
        if (attempt == 0 && !grow(n)) {
            return NULL;
        }
    }
    return NULL;
}

void free(void *p)
{
    if (p == NULL) {
        return;         /* free(NULL) is defined to do nothing */
    }
    struct block *b = (struct block *)((uint8_t *)p - HEADER);
    insert_free(b);
}

void *calloc(size_t count, size_t size)
{
    /*
     * the multiplication is the whole of why calloc exists rather than
     * being malloc(a*b): a*b can wrap, and a wrapped size allocates a
     * small block that the caller then writes a large one into
     */
    if (count != 0 && size > (size_t)-1 / count) {
        return NULL;
    }
    size_t total = count * size;
    void *p = malloc(total);
    if (p != NULL) {
        memset(p, 0, total);
    }
    return p;
}

void *realloc(void *p, size_t n)
{
    if (p == NULL) {
        return malloc(n);
    }
    if (n == 0) {
        free(p);
        return NULL;
    }
    struct block *b = (struct block *)((uint8_t *)p - HEADER);
    if (b->size >= n) {
        return p;       /* it already fits */
    }
    void *fresh = malloc(n);
    if (fresh == NULL) {
        return NULL;    /* and the original is still valid, which is the
                         * contract everybody forgets */
    }
    memcpy(fresh, p, b->size);
    free(p);
    return fresh;
}


long strtol(const char *s, char **end, int base)
{
    const char *p = s;

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }

    int sign = 1;
    if (*p == '-') { sign = -1; p++; }
    else if (*p == '+') { p++; }

    if ((base == 0 || base == 16)
     && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        base = 16;
    } else if (base == 0) {
        base = (p[0] == '0') ? 8 : 10;
    }

    long value = 0;
    const char *digits = p;

    for (;;) {
        int d;
        if (*p >= '0' && *p <= '9')      { d = *p - '0'; }
        else if (*p >= 'a' && *p <= 'z') { d = *p - 'a' + 10; }
        else if (*p >= 'A' && *p <= 'Z') { d = *p - 'A' + 10; }
        else { break; }

        if (d >= base) {
            break;
        }
        value = value * base + d;
        p++;
    }

    /*
     * `end` points at the first character that was not part of a
     * number, and if there were no digits at all, at the original
     * string. that is how a caller tells "0" from "not a number", and
     * it is the entire reason to use this instead of atoi
     */
    if (end != NULL) {
        *end = (char *)((p == digits) ? s : p);
    }
    return sign * value;
}

int atoi(const char *s)
{
    return (int)strtol(s, NULL, 10);
}

void abort(void)
{
#ifndef VELVETOS_HOSTED
    exit(134);          /* 128 + SIGABRT, the usual encoding */
#endif
}
