// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/parts.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * parts, which used to be a kernel command: what each drive says it holds.
 */

#include "report.h"

int main(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;
    return report("/proc/partitions");
}
