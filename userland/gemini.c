// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/gemini.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * gemini: fork, made visible.
 */

#include "syscall.h"
#include "args.h"

static const struct opt gemini_opts[] = {
    { 'n', "number", true,  "how many times to fork (1 by default)" },
    { 'q', "quiet",  false, "just the answers, no commentary" },
};

static const struct program gemini = {
    .name = "gemini",
    .usage = "gemini [-n count] [-q]",
    .summary = "fork, and show that the two halves stopped sharing memory",
    .opts = gemini_opts,
    .opt_count = sizeof gemini_opts / sizeof gemini_opts[0],
};

/*
 * a whole page of it, in .bss, so what is being written to is certainly
 * a page the two of them were sharing a moment ago rather than
 * something the compiler kept in a register
 */
static char shared[4096];

/*
 * and something on the stack, which is the other place a fork has to
 * come out right, the child's stack is a copy of the parent's at the
 * instant of the call, right down to the frame this runs in
 */
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
    return (v < 1) ? 1 : (v > 4 ? 4 : v);
}

static bool quiet;

static void say(const char *who, const char *what)
{
    write("[");
    write(who);
    write("] ");
    write(what);
    write("\n");
}

int main_body(long rounds)
{
    long on_the_stack = 1000;

    for (long i = 0; i < rounds; i++) {
        /*
         * both halves are about to be told the same thing, and both are
         * about to disagree with it
         */
        for (int j = 0; j < 4096; j++) {
            shared[j] = 'p';
        }

        long child = fork();
        if (child < 0) {
            write("gemini: could not fork\n");
            return 1;
        }

        if (child == 0) {
            /*
             * the child. the same program, the same instruction, the
             * same everything except this one number
             */
            for (int j = 0; j < 4096; j++) {
                shared[j] = 'c';
            }
            on_the_stack += 1;

            if (!quiet) {
                say("child", "the program wrote 'c' over the page the project were sharing");
            }
            write("[child] the page says: ");
            char one[2] = { shared[0], '\0' };
            write(one);
            write(", and its stack says ");
            write_num(on_the_stack);
            write("\n");

            /* the child leaves here. */
            exit(0);
        }

        /*
         * the parent. give the child a moment to have written, so that
         * the two lines come out in an order a person can read, this
         * proves nothing and is only politeness
         */
        int code = 0;
        wait(child, &code);

        on_the_stack += 100;

        if (!quiet) {
            say("parent", "the child has been and gone");
        }
        write("[parent] the page says: ");
        char one[2] = { shared[0], '\0' };
        write(one);
        write(", and its stack says ");
        write_num(on_the_stack);
        write("\n");

        if (shared[0] != 'p') {
            write("[parent] ...which is wrong. the child wrote through its "
                  "memory, so\n");
            write("         the pages never actually separated\n");
            return 1;
        }
    }

    if (!quiet) {
        write("\nboth halves wrote to the same address and neither saw the "
              "other's\n");
        write("write. nothing was copied until that moment, before it, one "
              "page\n");
        write("had two owners and both of them thought it was read-only.\n");
    }
    return 0;
}

void _start(int argc, char **argv)
{
    struct args a;
    const char *error;
    if (!args_parse(&gemini, argc, argv, &a, &error)) {
        write("gemini: ");
        write(error);
        write("\n");
        exit(1);
    }
    if (a.wants_help) {
        args_usage(&gemini);
        exit(0);
    }

    quiet = args_has(&a, &gemini, 'q');
    long rounds = to_number(args_value(&a, &gemini, 'n'), 1);

    exit(main_body(rounds));
}
