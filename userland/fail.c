// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/fail.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * a program that goes wrong on purpose.
 */

#include "syscall.h"

void _start(int argc, char **argv)
{
    (void)argc; (void)argv;

    write("[fail] attempting something beyond reach\n");
    sleep(300);
    write("[fail] as expected, it did not go well\n");
    exit(42);
}
