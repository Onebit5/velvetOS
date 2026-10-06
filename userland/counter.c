// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/counter.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * a second program, so there can be two.
 */

#include "syscall.h"

void _start(int argc, char **argv)
{
    (void)argc; (void)argv;

    /* a page of its own, at an address the other copy also thinks it owns. */
    static long private_count;

    write("[counter] awake, and this memory is the program's alone\n");

    for (int i = 0; i < 12; i++) {
        private_count++;
        write("[counter] the program says ");
        write_num(private_count);
        write(", uptime ");
        write_num(uptime());
        write("ms\n");
        sleep(600);
    }

    write("[counter] done, and it never once saw the other\n");
    exit(0);
}
