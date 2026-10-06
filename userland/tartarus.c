// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/tartarus.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * tartarus: how deep can you go before the floor gives out.
 */

#include "syscall.h"
#include "args.h"

static const struct opt tartarus_opts[] = {
    { 'd', "depth", true,  "how many pages of stack to try for (200)" },
    { 'm', "map",   true,  "how many kilobytes to ask for (4096)" },
};

static const struct program tartarus = {
    .name = "tartarus",
    .usage = "tartarus [-d pages] [-m kilobytes]",
    .summary = "climb the stack and ask for memory, to show both arrive "
               "as they are used",
    .opts = tartarus_opts,
    .opt_count = sizeof tartarus_opts / sizeof tartarus_opts[0],
};

static long to_number(const char *s, long fallback)
{
    if (s == NULL || *s == '\0') {
        return fallback;
    }
    long v = 0;
    for (const char *p = s; *p; p++) {
        if (*p < '0' || *p > '9') return fallback;
        v = v * 10 + (*p - '0');
    }
    return (v < 1) ? 1 : v;
}

/*
 * one page of stack per call, written to so the page really has to
 * exist rather than merely being reserved. `volatile` so the compiler
 * cannot decide the whole thing is pointless, which it otherwise
 * would, correctly
 */
static long deepest;

static void climb(long floor, long limit)
{
    volatile char step[4000];
    step[0] = (char)floor;
    step[3999] = (char)floor;

    deepest = floor;
    if (floor >= limit) {
        return;
    }
    climb(floor + 1, limit);

    /*
     * read it back on the way out, so the page has to still be there
     * and hold what was put in it
     */
    if (step[0] != (char)floor || step[3999] != (char)floor) {
        write("tartarus: a floor changed under the program on the way back down\n");
    }
}

void _start(int argc, char **argv)
{
    struct args a;
    const char *error;
    if (!args_parse(&tartarus, argc, argv, &a, &error)) {
        write("tartarus: ");
        write(error);
        write("\n");
        exit(1);
    }
    if (a.wants_help) {
        args_usage(&tartarus);
        exit(0);
    }

    long depth = to_number(args_value(&a, &tartarus, 'd'), 200);
    long kb = to_number(args_value(&a, &tartarus, 'm'), 4096);



    write("climbing, a page of stack per floor...\n");
    climb(1, depth);
    write("  reached floor ");
    write_num(deepest);
    write(", about ");
    write_num(deepest * 4);
    write("kb of stack, none of which existed when the program started\n");



    char *room = mmap(kb * 1024);
    if (room == NULL) {
        write("could not ask for that much\n");
        exit(1);
    }

    write("asked for ");
    write_num(kb);
    write("kb and got it at once, nothing was made\n");

    /* touch the two ends and one page in the middle. */
    room[0] = 'a';
    room[kb * 512] = 'b';
    room[kb * 1024 - 1] = 'c';

    if (room[0] != 'a' || room[kb * 512] != 'b' || room[kb * 1024 - 1] != 'c') {
        write("...but it does not hold what the program put in it\n");
        exit(1);
    }

    write("touched three pages of it, and those three are all that exist\n");
    write("(`mem` before and after says so more honestly than the program can)\n");

    if (munmap(room) < 0) {
        write("could not give it back\n");
        exit(1);
    }
    write("gave it back\n");

    exit(0);
}
