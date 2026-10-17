// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/sync.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * sync, which used to be a kernel command: what is in memory, put onto
 * the disk.
 */

#include "syscall.h"

int main(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;
    if (sync() != 0) {
        write("the drive refused something. what is on it is not what the "
              "kernel believes\n");
        return 1;
    }
    write("written. the disk holds what the kernel does\n");
    return 0;
}
