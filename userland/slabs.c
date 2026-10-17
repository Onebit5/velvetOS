// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/slabs.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * slabs, which used to be a kernel command: the object caches, and what they
 * hold.
 */

#include "report.h"

int main(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;
    return report("/proc/slabinfo");
}
