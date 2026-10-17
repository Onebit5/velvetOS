// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/vmm.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * vmm, which used to be a kernel command: where the kernel keeps things, which
 * is root's to see.
 */

#include "report.h"

int main(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;
    return report("/proc/vmm");
}
