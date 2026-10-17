// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/poweroff.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * poweroff, which used to be a kernel command: let the velvet room fade.
 */

#include "syscall.h"

int main(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;
    /* it does not come back when it works */
    reboot(REBOOT_POWER_OFF);
    write("only the master may power the machine off\n");
    return 1;
}
