// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/report.h
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the kernel's reports, which it writes into /proc as they are read.
 */

#ifndef USER_REPORT_H
#define USER_REPORT_H

#include "syscall.h"

/* as much as the kernel will write into one file of /proc */
#define REPORT_MAX (40 * 1024)

/*
 * one file of /proc, copied to the screen, in the words the kernel's own
 * shell used to print it in. the kernel writes the whole file afresh for
 * every read and hands back the part asked for, so it is read in one go:
 * two smaller reads would be two writings of the file, and a table that
 * changed in between would come out stitched from both
 */
static inline int report(const char *str_path)
{
    static char s_arr_buf[REPORT_MAX];
    long i_fd = open(str_path);

    if (i_fd < 0) {
        write(str_path);
        write(": not for this user to read, or not there to be read\n");
        return 1;
    }
    for (;;) {
        long i_got = read_fd(i_fd, s_arr_buf, REPORT_MAX);
        if (i_got <= 0) {
            break;
        }
        write_fd(STDOUT, s_arr_buf, i_got);
    }
    close(i_fd);
    return 0;
}

/*
 * the same file into `str_buf`, ended by a NUL, for a program that reads
 * the report rather than shows it: its length, or -1. in one read, for
 * the reason report() gives
 */
static inline long report_text(const char *str_path, char *str_buf,
                               long i_size)
{
    long i_fd = open(str_path);
    long i_len = 0;

    if (i_fd < 0 || i_size <= 0) {
        return -1;
    }
    for (;;) {
        long i_got = read_fd(i_fd, str_buf + i_len, i_size - 1 - i_len);
        if (i_got <= 0) {
            break;
        }
        i_len += i_got;
    }
    close(i_fd);
    str_buf[i_len] = '\0';
    return i_len;
}

#endif
