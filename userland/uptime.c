// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/uptime.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * uptime, which used to be a kernel command.
 */

#include "syscall.h"

void _start(int argc, char **argv)
{
    (void)argc; (void)argv;

    long ms = uptime();
    long s = ms / 1000;

    write("awake for ");
    write_num(s / 3600);
    write("h ");
    write_num((s / 60) % 60);
    write("m ");
    write_num(s % 60);
    write("s (");
    write_num(ms);
    write("ms)\n");
    exit(0);
}
