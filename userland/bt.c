// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/bt.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * bt, which used to be a kernel command: who called whom to get here, in the
 * kernel.
 */

#include "report.h"

int main(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;
    return report("/proc/stack");
}
