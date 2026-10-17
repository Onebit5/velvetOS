// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/fsck.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * fsck [-n] [work | path], which used to be a kernel command: check a
 * filesystem, and mend what has exactly one right answer. the kernel
 * does the checking; this puts what it found into words.
 */

#include "syscall.h"
#include <stdio.h>
#include <string.h>

/* one line per kind of problem, by the kernel's numbering of them */
static const char *const s_arr_kinds[FSCK_PROBLEMS] = {
    [FSCK_SUPERBLOCK]     = "the superblock contradicts itself",
    [FSCK_BLOCK_UNMARKED] = "a block in use that the bitmap calls free",
    [FSCK_BLOCK_CROSSED]  = "a block two files both claim",
    [FSCK_BLOCK_LEAKED]   = "a block marked used that nothing claims",
    [FSCK_INODE_UNMARKED] = "a name pointing at a free inode",
    [FSCK_INODE_ORPHANED] = "an inode in use that no name reaches",
    [FSCK_LINKS_WRONG]    = "a link count that is not the number of names",
    [FSCK_DIR_NO_DOTS]    = "a directory without . or ..",
    [FSCK_DIR_BAD_PARENT] = ".. that is not the parent",
    [FSCK_DIR_BAD_RECORD] = "a directory record that walks off its block",
    [FSCK_EXTENT_BAD]     = "an extent tree that does not parse",
    [FSCK_COUNTS_WRONG]   = "a free count that is not the true one",
};

int main(int i_argc, char **pp_argv)
{
    struct user_fsck r;
    const char *str_at = "/";
    long i_flags = FSCK_MEND;

    for (int i_at = 1; i_at < i_argc; i_at++) {
        if (strcmp(pp_argv[i_at], "-n") == 0) {
            i_flags = 0;
        } else if (strcmp(pp_argv[i_at], "work") == 0) {
            str_at = "/work";
        } else {
            str_at = pp_argv[i_at];
        }
    }

    if (fsck(str_at, i_flags, &r) != 0) {
        printf("%s\n", r.arr_why[0] != '\0' ? r.arr_why : "cannot check it");
        return 1;
    }

    printf("%s\n", str_at);
    printf("%u blocks of %u, %u inodes, %u group(s)\n", r.u_blocks,
           r.u_block_size, r.u_inodes, r.u_groups);
    if (r.u_total_found == 0) {
        printf("nothing wrong with it\n");
        return 0;
    }
    for (int i_kind = 0; i_kind < FSCK_PROBLEMS; i_kind++) {
        if (r.arr_found[i_kind] == 0) {
            continue;
        }
        printf("  %u x %s", r.arr_found[i_kind], s_arr_kinds[i_kind]);
        if (r.arr_mended[i_kind] > 0) {
            printf("  (%u mended)", r.arr_mended[i_kind]);
        } else if (i_flags & FSCK_MEND) {
            printf("  (left alone, no answer to it but yours)");
        }
        printf("\n");
    }
    printf("%u problem(s), %u mended\n", r.u_total_found, r.u_total_mended);
    return 0;
}
