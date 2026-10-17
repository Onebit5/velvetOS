// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/kill.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * kill [-signal] pid...: a signal, SIGTERM unless another is named, to
 * each process, or to a whole job as minus its group.
 */

#include "syscall.h"
#include <stdio.h>
#include <stdlib.h>

int main(int i_argc, char **pp_argv)
{
    long i_sig = SIGTERM;
    int i_at = 1;
    int i_failed = 0;

    if (i_argc > 1 && pp_argv[1][0] == '-' && pp_argv[1][1] >= '0'
        && pp_argv[1][1] <= '9') {
        i_sig = atoi(pp_argv[1] + 1);
        i_at = 2;
    }
    if (i_at >= i_argc) {
        printf("kill [-signal] pid..., and minus a group for a whole job\n");
        return 1;
    }
    for (; i_at < i_argc; i_at++) {
        long i_pid = atoi(pp_argv[i_at]);
        if (sigsend(i_pid, i_sig) != 0) {
            printf("%s: no such process, or not one of yours\n",
                   pp_argv[i_at]);
            i_failed = 1;
        }
    }
    return i_failed;
}
