// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/reboot.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * reboot, which used to be a kernel command: sever the bond and begin anew.
 */

#include "syscall.h"

int main(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;
    /* it does not come back when it works */
    reboot(REBOOT_RESTART);
    write("only the master may restart the machine\n");
    return 1;
}
