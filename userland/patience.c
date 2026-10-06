// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/patience.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * patience, a program that refuses to be hurried.
 */

#include "syscall.h"

static void say(const char *s)
{
    write_fd(STDOUT, s, (long)ustrlen(s));
}

static void say_num(long n)
{
    char b[24];
    int at = 0;
    if (n == 0) {
        b[at++] = '0';
    }
    char tmp[24];
    int t = 0;
    while (n > 0) {
        tmp[t++] = (char)('0' + (n % 10));
        n /= 10;
    }
    while (t > 0) {
        b[at++] = tmp[--t];
    }
    b[at] = '\0';
    say(b);
}

/* how many interrupts have been asked for. */
static volatile long asked;

static void on_interrupt(long sig)
{
    (void)sig;
    asked++;

    /*
     * a handler should do almost nothing, and here that rule has teeth
     * for a reason worth stating: this one runs on the *same stack* as
     * whatever it interrupted, at a moment nobody chose. anything it
     * touches has to be safe to touch half way through, which for
     * most of a program is not true.
     *
     * a write is about as much as is defensible, and even that is a
     * choice: it is one syscall, and it is what makes the program
     * legible from outside
     */
    say("\n[not yet, ask again to stop]\n");
}

void _start(int argc, char **argv)
{
    long limit = 60;
    int deaf = 0;

    if (argc >= 2) {
        long v = 0;
        for (const char *p = argv[1]; *p; p++) {
            if (*p < '0' || *p > '9') {
                say("patience: that is not a number\n");
                exit(1);
            }
            v = v * 10 + (*p - '0');
        }
        if (v > 0) {
            limit = v;
        }
    }
    if (argc >= 3 && argv[2][0] == 'd') {
        deaf = 1;
    }

    if (deaf) {
        /* ignored, not handled. */
        signal(SIGINT, (void (*)(long))SIG_IGNORE);
        say("patience: deaf to interrupts. `signal <pid> kill` is the "
            "only way out\n");
    } else if (signal(SIGINT, on_interrupt) < 0) {
        say("patience: could not install a handler\n");
        exit(1);
    }

    /* and this one is refused, always, by every kernel that has ever had signals. */
    if (signal(SIGKILL, on_interrupt) >= 0) {
        say("patience: SIGKILL was caught, which should not be "
            "not have\n");
    }

    for (long i = 1; i <= limit; i++) {
        say_num(i);
        say(" ");

        /* a sleep interrupted by a signal comes back early and says so. */
        if (sleep(1000) < 0) {
            say("\n[the sleep was cut short]\n");
        }

        if (asked >= 2) {
            say("\npatience: very well.\n");
            exit(0);
        }
    }

    say("\npatience: done, uninterrupted");
    if (asked > 0) {
        say(", and asked to stop ");
        say_num(asked);
        say(" time(s), which it declined");
    }
    say("\n");
    exit(0);
}
