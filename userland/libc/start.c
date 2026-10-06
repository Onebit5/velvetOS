// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/libc/start.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * crt0: the thing that makes `main` possible.
 */

#include "../syscall.h"

int main(int argc, char **argv);

void _start(int argc, char **argv)
{
    /*
     * and a main that simply falls off its end returns 0, because C
     * says so about main specifically, which is a rule that exists
     * precisely so that programs need not remember it
     */
    exit(main(argc, argv));
}
