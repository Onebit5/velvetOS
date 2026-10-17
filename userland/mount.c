// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/mount.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * mount, which used to be a kernel command: which filesystem is where.
 */

#include "report.h"

int main(int i_argc, char **pp_argv)
{
    (void)pp_argv;
    if (i_argc > 1) {
        /* the kernel's shell could move a mount; a program cannot ask to */
        write("mount only lists what is mounted. moving a mount is not "
              "something a program\nmay ask the kernel for\n");
        return 1;
    }
    return report("/proc/mounts");
}
