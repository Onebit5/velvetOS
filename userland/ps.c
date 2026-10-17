// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/ps.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * ps, which used to be a kernel command: the threads that walk this realm.
 */

#include "report.h"

int main(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;
    return report("/proc/threads");
}
