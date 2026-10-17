// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/dmesg.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * dmesg, which used to be a kernel command: everything boot said while
 * the screen was being kept quiet.
 */

#include "syscall.h"
#include <stdlib.h>

int main(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;

    long i_size = klogctl(SYSLOG_SIZE_BUFFER, NULL, 0);
    char *str_log = (i_size > 0) ? malloc((size_t)i_size) : NULL;
    if (str_log == NULL) {
        write("the kernel's log could not be read\n");
        return 1;
    }
    long i_len = klogctl(SYSLOG_READ_ALL, str_log, i_size);
    if (i_len < 0) {
        write("the kernel's log could not be read\n");
        return 1;
    }
    write_fd(STDOUT, str_log, i_len);
    return 0;
}
