// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/resize.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * resize [work | path] [blocks], which used to be a kernel command: grow a
 * filesystem into the partition it sits in, or shrink it, and with no
 * number fill the partition.
 */

#include "syscall.h"
#include <stdio.h>
#include <string.h>

/* a count of blocks, or -1 for anything that is not one */
static long blocks_in(const char *str_word)
{
    long i_n = 0;

    if (*str_word == '\0') {
        return -1;
    }
    for (; *str_word != '\0'; str_word++) {
        if (*str_word < '0' || *str_word > '9') {
            return -1;
        }
        i_n = i_n * 10 + (*str_word - '0');
    }
    return i_n;
}

int main(int i_argc, char **pp_argv)
{
    struct user_resize r;
    const char *str_at = "/";
    long i_want = 0;

    for (int i_at = 1; i_at < i_argc; i_at++) {
        if (strcmp(pp_argv[i_at], "work") == 0) {
            str_at = "/work";
        } else if (pp_argv[i_at][0] == '/') {
            str_at = pp_argv[i_at];
        } else if ((i_want = blocks_in(pp_argv[i_at])) <= 0) {
            printf("resize [work] [blocks], with no number it fills the "
                   "partition\n");
            return 1;
        }
    }

    long i_done = resize(str_at, i_want, &r);
    if (r.u_room > 0) {
        printf("%s holds %u blocks of %u, and its partition has room for "
               "%lu\n", str_at, r.u_blocks_before, r.u_block_size,
               (unsigned long)r.u_room);
    }
    if (i_done != 0) {
        printf("%s\n", r.arr_why[0] != '\0' ? r.arr_why
                                            : "it would not resize");
        if (r.u_blocks_in_way > 0 || r.u_inodes_in_way > 0) {
            printf("%lu block(s) and %lu inode(s) are out past where the "
                   "new end would\nbe. nothing is moved here, so this "
                   "refuses rather than losing them.\nempty the tail and "
                   "ask again\n", (unsigned long)r.u_blocks_in_way,
                   (unsigned long)r.u_inodes_in_way);
        } else if (i_want > 0 && (unsigned long)i_want > r.u_ceiling
                   && r.u_ceiling > 0) {
            printf("the descriptor table describes every group and sits "
                   "at the front of\nevery group, so it can only grow "
                   "into the room reserved for it when\nthe filesystem "
                   "was made. that stops at %lu blocks\n",
                   (unsigned long)r.u_ceiling);
        }
        return 1;
    }

    if (r.u_blocks_after == r.u_blocks_before) {
        printf("which is already the size it is. nothing to do\n");
        return 0;
    }
    printf("%u blocks in %u group(s), was %u in %u\n", r.u_blocks_after,
           r.u_groups_after, r.u_blocks_before, r.u_groups_before);
    if (r.u_inodes_after != r.u_inodes_before) {
        printf("and %u inodes, was %u, a group brings its own, so a disk "
               "that grew\nwithout them would fill up having used a "
               "fraction of itself\n", r.u_inodes_after, r.u_inodes_before);
    }
    printf("`fsck` will say whether the kernel is telling the truth\n");
    return 0;
}
