// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/whoami.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * whoami, from the other side of the boundary.
 */

#include "syscall.h"

void _start(int argc, char **argv)
{
    (void)argc; (void)argv;

    long uid = getuid();

    write("this program is pid ");
    write_num(getpid());
    write(", running as uid ");
    write_num(uid);
    write("\n");

    /* and the boundary, demonstrated rather than described */
    write("\ntrying to read velvet-room.txt:\n");
    long fd = open("velvet-room.txt");
    if (fd < 0) {
        write("  refused: the mode and the uid do not agree,\n");
        write("  and there is nothing more to be done about it.\n");
        exit(1);
    }

    char buf[128];
    for (;;) {
        long n = read_fd(fd, buf, sizeof buf);
        if (n <= 0) break;
        write_fd(STDOUT, buf, n);
    }
    close(fd);
    exit(0);
}
