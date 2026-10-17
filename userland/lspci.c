// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/lspci.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * lspci, which used to be a kernel command: what is plugged into this machine.
 */

#include "report.h"

int main(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;
    return report("/proc/pci");
}
