// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/hello.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the first program velvetOS ever ran that it did not also contain.
 */

#include "syscall.h"

void _start(int argc, char **argv)
{
    (void)argc; (void)argv;

    write("\n");
    write("I am thou... thou art I...\n");
    write("A voice speaks from ring 3, where it can touch nothing\n");
    write("and must ask for everything.\n\n");

    write("  privilege   3 (the outer ring)\n");
    write("  uptime      ");
    write_num(uptime());
    write(" ms since the bond was formed\n\n");

    write("Counting, so thou may watch the wheel keep turning:\n");
    for (int i = 1; i <= 5; i++) {
        write("  ");
        write_num(i);
        write(" ... the kernel yet lives\n");
        sleep(400);
    }

    write("\nMy purpose is fulfilled. Returning to the sea of souls.\n\n");
    exit(0);
}
