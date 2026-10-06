// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/ask.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * a program that reads the keyboard.
 */

#include "syscall.h"

static void prompt_for(const char *what, char *buf, long max)
{
    write(what);
    long n = read_fd(STDIN, buf, max - 1);
    if (n < 0) {
        buf[0] = '\0';
        return;
    }
    /* the newline came with it */
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
        n--;
    }
    buf[n] = '\0';
}

void _start(int argc, char **argv)
{
    (void)argc; (void)argv;

    /*
     * ctrl+c is a request here, not an order: ignoring it keeps the
     * process alive to decide, and the call it interrupts still comes
     * back -1. pressing it twice is the kernel's business, not the program's
     */
    signal(SIGINT, (void (*)(long))SIG_IGNORE);

    char name[64];

    write("[ask] pid ");
    write_num(getpid());
    write(", and the keyboard belongs to this program while it runs\n\n");

    prompt_for("  what is thy name? ", name, sizeof name);

    if (name[0] == '\0') {
        write("\n[ask] interrupted. i shall ask no more\n");
        exit(130);          /* what a shell would call SIGINT */
    }

    write("\n  well met, ");
    write(name);
    write(".\n\n");

    write("[ask] now say nothing, and press ctrl+c to interrupt the program\n");
    for (int i = 0; i < 20; i++) {
        if (sleep(500) < 0) {
            write("[ask] interrupted mid-sleep. leaving politely\n");
            exit(130);
        }
        write("  still waiting...\n");
    }

    write("[ask] nobody interrupted me. how patient thou art\n");
    exit(0);
}
