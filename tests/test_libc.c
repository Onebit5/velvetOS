// SPDX-License-Identifier: GPL-2.0-only
/*
 * tests/test_libc.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * host-side test for the library programs link against.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/*
 * the library under test, included rather than linked, so that its
 * static free list can be reset between sections
 */
#define malloc  libc_malloc
#define free    libc_free
#define calloc  libc_calloc
#define realloc libc_realloc
#define strtol  libc_strtol
#define atoi    libc_atoi
#define abort   libc_abort
#define vsnprintf libc_vsnprintf
#define snprintf  libc_snprintf
#define sprintf   libc_sprintf

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
} while (0)

/* what the program would have got from the kernel. */
static unsigned char arena[4 * 1024 * 1024];
static size_t arena_at;
static int mmap_calls;

void *libc_get_pages(long len)
{
    size_t n = ((size_t)len + 4095) & ~(size_t)4095;
    if (arena_at + n > sizeof arena) {
        return NULL;
    }
    void *p = arena + arena_at;
    arena_at += n;
    mmap_calls++;
    return p;
}

/* the kernel's, which is now the only one. */
#include "../kernel/lib/string.c"
#include "../userland/libc/stdlib.c"

/*
 * the free list is static inside stdlib.c; this puts the whole
 * allocator back to how it started
 */
static void heap_reset(void)
{
    free_list = NULL;
    arena_at  = 0;
    mmap_calls = 0;
    memset(arena, 0xAA, sizeof arena);   /* poison, so a block handed
                                          * back without being written
                                          * does not read as zeroes */
}

static size_t free_blocks(void)
{
    size_t n = 0;
    for (struct block *b = free_list; b != NULL; b = b->next) {
        n++;
    }
    return n;
}


#include <stdarg.h>
static int fmt(char *out, size_t max, const char *f, ...);

int main(void)
{


    heap_reset();
    char *a = libc_malloc(100);
    CHECK(a != NULL, "a hundred bytes are handed over");
    CHECK(((uintptr_t)a % 16) == 0,
          "aligned to sixteen, which is what the abi requires of "
          "anything that might hold a long double");

    memset(a, 'x', 100);            /* it must be writable, all of it */
    libc_free(a);

    char *b = libc_malloc(100);
    CHECK(b == a, "and the same block comes back once it is free");
    libc_free(b);

    CHECK(libc_malloc(0) != NULL,
          "malloc(0) gives a real pointer rather than NULL, otherwise "
          "every caller has to tell 'no memory' from 'you asked for "
          "none'");

    /* two blocks are not the same block */
    heap_reset();
    char *p1 = libc_malloc(64);
    char *p2 = libc_malloc(64);
    CHECK(p1 != p2, "two allocations are two places");
    CHECK(p1 + 64 <= p2 || p2 + 64 <= p1, "that do not overlap");



    heap_reset();
    char *c1 = libc_malloc(64);
    char *c2 = libc_malloc(64);
    char *c3 = libc_malloc(64);
    (void)c2;

    /*
     * there is already one free block: what is left of the chunk the
     * allocator asked the kernel for. c3 sits directly in front of it,
     * so freeing c3 merges with it, which is itself coalescing, and
     * is why this counts blocks at the end rather than after each step
     */
    libc_free(c1);
    libc_free(c3);
    CHECK(free_blocks() == 2,
          "two frees with a live block between them stay two, c1 on "
          "its own, and c3 merged into the tail of the chunk");

    libc_free(c2);
    CHECK(free_blocks() == 1,
          "and freeing the one between them joins all three back into "
          "one, without this, a program that allocates and frees in a "
          "loop ends up with a list of blocks too small to use and a "
          "heap that only grows");

    /* the same, freed in the other order. */
    heap_reset();
    char *d[8];
    for (int i = 0; i < 8; i++) {
        d[i] = libc_malloc(64);
    }
    for (int i = 7; i >= 0; i--) {
        libc_free(d[i]);
    }
    CHECK(free_blocks() == 1,
          "eight blocks freed newest-first coalesce into one, which is "
          "the direction a forwards-only merge gets wrong");

    /* and in a shuffled order, which is what a real program does */
    heap_reset();
    for (int i = 0; i < 8; i++) {
        d[i] = libc_malloc(64);
    }
    static const int order[8] = { 3, 0, 6, 1, 7, 2, 5, 4 };
    for (int i = 0; i < 8; i++) {
        libc_free(d[order[i]]);
    }
    CHECK(free_blocks() == 1,
          "and so do eight freed in no particular order");

    /* the real test of it: a loop that would exhaust a heap that does not merge. */
    heap_reset();
    bool ran_out = false;
    for (int i = 0; i < 200; i++) {
        void *big = libc_malloc(1024 * 1024);
        if (big == NULL) {
            ran_out = true;
            break;
        }
        libc_free(big);
    }
    CHECK(!ran_out,
          "a megabyte allocated and freed two hundred times never runs "
          "out, this is the whole point, and a heap without "
          "coalescing fails it on the second pass");
    CHECK(mmap_calls <= 2,
          "and asks the kernel for memory once or twice, not two "
          "hundred times");



    heap_reset();
    char *whole = libc_malloc(4096);
    libc_free(whole);
    char *small = libc_malloc(16);
    CHECK(small == whole,
          "a small request takes the front of a big free block");
    char *rest = libc_malloc(64);
    CHECK(rest != NULL && rest > small,
          "and what is left of it is still usable");

    /*
     * a request that nearly fills a block must not leave a remainder too
     * small to hold a header, that remainder is unreachable memory
     */
    heap_reset();
    whole = libc_malloc(200);
    libc_free(whole);
    small = libc_malloc(190);
    CHECK(small == whole, "a request that nearly fills a block takes it");
    CHECK(free_blocks() == 1,
          "without splitting off a remainder too small to be used, which "
          "would be memory nothing could ever reach again");



    heap_reset();
    unsigned char *z = libc_calloc(100, 4);
    CHECK(z != NULL, "calloc gives memory");
    bool all_zero = true;
    for (int i = 0; i < 400; i++) {
        if (z[i] != 0) { all_zero = false; }
    }
    CHECK(all_zero, "and every byte of it is zero, over poisoned memory");

    CHECK(libc_calloc((size_t)-1 / 2, 4) == NULL,
          "a count and size that would wrap is refused, a wrapped "
          "size allocates a small block that the caller then writes a "
          "large one into, which is the whole reason calloc exists "
          "rather than being malloc(a*b)");

    heap_reset();
    char *r = libc_malloc(32);
    memcpy(r, "hello", 6);
    char *bigger = libc_realloc(r, 4096);
    CHECK(bigger != NULL, "realloc grows");
    CHECK(memcmp(bigger, "hello", 6) == 0, "and keeps what was there");

    CHECK(libc_realloc(NULL, 32) != NULL, "realloc(NULL) is malloc");

    heap_reset();
    r = libc_malloc(64);
    CHECK(libc_realloc(r, 32) == r,
          "and shrinking in place is allowed rather than required to "
          "move");



    libc_free(NULL);            /* defined to do nothing, not to crash */
    CHECK(true, "free(NULL) does nothing at all");



    char *end = NULL;
    CHECK(libc_strtol("123", &end, 10) == 123, "a decimal number");
    CHECK(*end == '\0', "consuming all of it");

    CHECK(libc_strtol("  -42abc", &end, 10) == -42, "with spaces and a sign");
    CHECK(strcmp(end, "abc") == 0, "and says where it stopped");

    CHECK(libc_strtol("ff", NULL, 16) == 255, "hexadecimal when asked");
    CHECK(libc_strtol("0x1f", NULL, 16) == 31, "with an 0x that is allowed");
    CHECK(libc_strtol("0x1f", NULL, 0) == 31, "and base 0 works it out");
    CHECK(libc_strtol("017", NULL, 0) == 15, "including octal");
    CHECK(libc_strtol("17", NULL, 0) == 17, "and plain decimal");

    end = NULL;
    CHECK(libc_strtol("hello", &end, 10) == 0, "a word is zero");
    CHECK(end != NULL && strcmp(end, "hello") == 0,
          "with `end` back at the start, which is the only way a caller "
          "can tell \"0\" from \"not a number\", and is the entire "
          "reason to use this instead of atoi");

    CHECK(libc_atoi("42") == 42, "atoi is the short way round");



    char buf[64];

    CHECK(strlen("hello") == 5, "strlen counts");
    CHECK(strnlen("hello", 3) == 3, "strnlen stops");
    CHECK(strnlen("ab", 10) == 2, "and does not run past the end");

    strcpy(buf, "hello");
    CHECK(strcmp(buf, "hello") == 0, "strcpy copies");
    strcat(buf, ", world");
    CHECK(strcmp(buf, "hello, world") == 0, "strcat appends");

    CHECK(strcmp("a", "b") < 0 && strcmp("b", "a") > 0, "strcmp orders");
    CHECK(strncmp("abcd", "abce", 3) == 0, "strncmp stops where told");
    CHECK(strcasecmp("HeLLo", "hello") == 0, "strcasecmp ignores case");

    CHECK(strchr("hello", 'l') == strstr("hello", "ll"), "strchr finds");
    CHECK(strchr("hello", 'z') == NULL, "and says when it does not");
    CHECK(strrchr("hello", 'l')[1] == 'o', "strrchr finds the last one");
    CHECK(strchr("hello", '\0') != NULL,
          "and the terminator is findable, which ported code relies on");

    CHECK(strstr("hello world", "world") != NULL, "strstr finds");
    CHECK(strstr("hello", "") != NULL, "everybody contains nothing");
    CHECK(strstr("hello", "xyz") == NULL, "and not what is not there");

    /* strncpy's two famous behaviours, both deliberate */
    memset(buf, 'Z', sizeof buf);
    strncpy(buf, "ab", 8);
    CHECK(buf[2] == '\0' && buf[7] == '\0',
          "strncpy pads the whole width with zeroes");
    memset(buf, 'Z', sizeof buf);
    strncpy(buf, "abcdefgh", 8);
    CHECK(buf[7] == 'h' && buf[8] == 'Z',
          "and does not terminate when the source filled it exactly, "
          "which is what it has always done and why nobody should reach "
          "for it first");

    /* memmove's whole reason to exist */
    strcpy(buf, "0123456789");
    memmove(buf + 2, buf, 8);
    CHECK(memcmp(buf, "0101234567", 10) == 0,
          "memmove copies correctly when the regions overlap forwards");
    strcpy(buf, "0123456789");
    memmove(buf, buf + 2, 8);
    CHECK(memcmp(buf, "23456789", 8) == 0, "and backwards");

    CHECK(memchr("abcdef", 'd', 6) != NULL, "memchr finds");
    CHECK(memchr("abcdef", 'z', 6) == NULL, "and does not invent");



    CHECK(fmt(buf, sizeof buf, "hello") == 5, "plain text is copied");
    CHECK(strcmp(buf, "hello") == 0, "exactly");

    fmt(buf, sizeof buf, "%d and %d", 42, -7);
    CHECK(strcmp(buf, "42 and -7") == 0, "numbers, with signs");

    fmt(buf, sizeof buf, "%s/%c", "path", 'x');
    CHECK(strcmp(buf, "path/x") == 0, "strings and characters");

    fmt(buf, sizeof buf, "%x %X", 255UL, 255UL);
    CHECK(strcmp(buf, "ff FF") == 0, "hexadecimal in both cases");

    fmt(buf, sizeof buf, "%5d|%-5d|", 42, 42);
    CHECK(strcmp(buf, "   42|42   |") == 0, "width, and left alignment");

    fmt(buf, sizeof buf, "%05d", 42);
    CHECK(strcmp(buf, "00042") == 0, "zero padding");

    fmt(buf, sizeof buf, "%05d", -42);
    CHECK(strcmp(buf, "-0042") == 0,
          "with the sign before the padding, \"000-42\" is a number "
          "nobody can read");

    fmt(buf, sizeof buf, "%ld %zu", 5L, (size_t)6);
    CHECK(strcmp(buf, "5 6") == 0,
          "length modifiers are accepted, because ported code writes "
          "them and refusing would mean editing every line that does");

    fmt(buf, sizeof buf, "100%%");
    CHECK(strcmp(buf, "100%") == 0, "a doubled percent is one percent");

    fmt(buf, sizeof buf, "%s", (char *)NULL);
    CHECK(strcmp(buf, "(null)") == 0,
          "a null string prints rather than faulting, it is somebody's "
          "bug either way, and one that prints can be found");



    char tiny[8];
    memset(tiny, 'Z', sizeof tiny);
    int n = fmt(tiny, sizeof tiny, "%s", "0123456789");
    CHECK(n == 10,
          "it returns what it *would* have written, which is how a "
          "caller knows it was cut short");
    CHECK(tiny[7] == '\0', "and terminates within the buffer");
    CHECK(strlen(tiny) == 7, "having written exactly what fits");

    memset(tiny, 'Z', sizeof tiny);
    fmt(tiny, 1, "hello");
    CHECK(tiny[0] == '\0', "a buffer of one holds only the terminator");
    CHECK(tiny[1] == 'Z', "and nothing is written past it");

    memset(tiny, 'Z', sizeof tiny);
    n = fmt(tiny, 0, "hello");
    CHECK(n == 5, "a buffer of nothing still counts");
    CHECK(tiny[0] == 'Z',
          "and writes nothing at all, which is how a caller asks how "
          "big a buffer needs to be before allocating one");

    /* a format ending in a bare percent must not read past it */
    fmt(buf, sizeof buf, "abc%");
    CHECK(strcmp(buf, "abc%") == 0,
          "a format ending in a bare %% stops there rather than reading "
          "off the end of the string");

    if (failures == 0) printf("all good\n");
    return failures;
}

/* included last, so the test above can use everything it defines */
#include "../userland/libc/format.c"

static int fmt(char *out, size_t max, const char *f, ...)
{
    va_list ap;
    va_start(ap, f);
    int n = libc_vsnprintf(out, max, f, ap);
    va_end(ap);
    return n;
}
