// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/sort.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * put lines in order.
 */

#include "syscall.h"
#include "args.h"
#include "lines.h"

static const struct opt sort_opts[] = {
    { 'r', "reverse", false, "largest first" },
    { 'u', "unique",  false, "drop lines that repeat" },
    { 'n', "numeric", false, "compare as numbers where both look like one" },
};

static const struct program sort = {
    .name = "sort",
    .usage = "sort [-r] [-u] [-n] [file...]",
    .summary = "put lines in order",
    .opts = sort_opts,
    .opt_count = sizeof sort_opts / sizeof sort_opts[0],
};

#define MAX_LINES 512
#define WIDTH     160

static char held[MAX_LINES][WIDTH];
static long count;
static bool overflowed;

static void keep(const char *line)
{
    if (count == MAX_LINES) {
        overflowed = true;
        return;
    }
    long i = 0;
    while (line[i] != '\0' && i < WIDTH - 1) {
        held[count][i] = line[i];
        i++;
    }
    held[count][i] = '\0';
    count++;
}

static void soak(long fd)
{
    struct lines l;
    lines_open(&l, fd);

    char line[LINE_CAP];
    long len;
    while (lines_next(&l, line, sizeof line, &len)) {
        keep(line);
    }
}

/* is the whole line a number? leading minus allowed, nothing else */
static bool numeric(const char *s, long *out)
{
    long sign = 1;
    if (*s == '-') {
        sign = -1;
        s++;
    }
    if (*s == '\0') {
        return false;
    }
    long v = 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') {
            return false;
        }
        v = v * 10 + (*s - '0');
    }
    *out = v * sign;
    return true;
}

static int compare(const char *a, const char *b, bool as_numbers)
{
    if (as_numbers) {
        long x, y;
        /* only when *both* look like numbers. */
        if (numeric(a, &x) && numeric(b, &y)) {
            return (x < y) ? -1 : (x > y) ? 1 : 0;
        }
    }
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static void swap(long i, long j)
{
    for (long k = 0; k < WIDTH; k++) {
        char t = held[i][k];
        held[i][k] = held[j][k];
        held[j][k] = t;
    }
}

void _start(int argc, char **argv)
{
    struct args a;
    const char *error;
    if (!args_parse(&sort, argc, argv, &a, &error)) {
        write("sort: ");
        write(error);
        write("\n");
        exit(1);
    }
    if (a.wants_help) {
        args_usage(&sort);
        exit(0);
    }

    bool reverse = args_has(&a, &sort, 'r');
    bool unique = args_has(&a, &sort, 'u');
    bool as_numbers = args_has(&a, &sort, 'n');

    if (a.count == 0) {
        soak(STDIN);
    } else {
        for (int i = 0; i < a.count; i++) {
            long fd = open(a.rest[i]);
            if (fd < 0) {
                write("sort: cannot open ");
                write(a.rest[i]);
                write("\n");
                continue;
            }
            soak(fd);
            close(fd);
        }
    }

    if (overflowed) {
        write("sort: more than ");
        write_num(MAX_LINES);
        write(" lines, the program can only hold that many at once,\n");
        write("      and sorting needs all of them before it can print any\n");
    }

    /* insertion sort: take each line and walk it back to where it belongs. */
    for (long i = 1; i < count; i++) {
        for (long j = i; j > 0; j--) {
            int c = compare(held[j - 1], held[j], as_numbers);
            if (reverse) {
                c = -c;
            }
            if (c <= 0) {
                break;
            }
            swap(j - 1, j);
        }
    }

    for (long i = 0; i < count; i++) {
        if (unique && i > 0 && compare(held[i - 1], held[i], false) == 0) {
            continue;
        }
        put_line(held[i]);
    }

    exit(overflowed ? 1 : 0);
}
