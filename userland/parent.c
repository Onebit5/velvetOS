// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/parent.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * a program that starts another program.
 */

#include "syscall.h"

void _start(int argc, char **argv)
{
    (void)argc; (void)argv;

    write("[parent] i am pid ");
    write_num(getpid());
    write("\n");

    write("[parent] starting bin/fail, which i expect to go badly\n");
    long child = spawn("bin/fail");
    if (child < 0) {
        write("[parent] could not start it\n");
        exit(1);
    }

    write("[parent] it is pid ");
    write_num(child);
    write(", and i shall wait\n");

    int code = 0;
    if (wait(child, &code) < 0) {
        write("[parent] waiting failed\n");
        exit(1);
    }

    write("[parent] it exited ");
    write_num(code);
    write(", exactly as foretold\n");

    /* that number crossed two address spaces and outlived the thread that produced it. */
    exit(code);
}
