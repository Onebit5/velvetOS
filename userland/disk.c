// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/disk.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * disk, which used to be a kernel command: the drive, and the filesystems on
 * it.
 */

#include "report.h"

int main(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;
    return report("/proc/disk");
}
