// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/mem.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * mem, which used to be a kernel command: frames and heap, honestly counted.
 */

#include "report.h"

int main(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;
    return report("/proc/meminfo");
}
