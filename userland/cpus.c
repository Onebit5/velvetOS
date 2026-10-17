// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/cpus.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * cpus, which used to be a kernel command: the processors, and which are awake.
 */

#include "report.h"

int main(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;
    return report("/proc/cpuinfo");
}
