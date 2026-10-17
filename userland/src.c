// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/src.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * src [verify | unpack [where [what]]], which used to be a kernel
 * command: the source this machine was built from, still on the medium.
 * the kernel says what is there and hashes it; reading it and writing
 * a copy are what any program does with files.
 */

#include "syscall.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define SRC_AT   "/boot/src"
#define NAME_MAX 256
#define CHUNK    4096

static char s_arr_chunk[CHUNK];
static long s_i_files;

static void join(char *str_out, const char *str_dir, const char *str_name)
{
    snprintf(str_out, NAME_MAX, "%s/%s", str_dir, str_name);
}

/*
 * every directory on the way to `str_path` that is not there yet. the
 * archive lists a file by its whole path, docs/building.rst, rather than
 * by the directories it is in, so they are made as the files need them
 */
static void make_parents(const char *str_path)
{
    char arr_dir[NAME_MAX];
    struct stat st;

    snprintf(arr_dir, NAME_MAX, "%s", str_path);
    for (char *str_at = arr_dir + 1; *str_at != '\0'; str_at++) {
        if (*str_at == '/') {
            *str_at = '\0';
            if (stat(arr_dir, &st) != 0) {
                mkdir(arr_dir);
            }
            *str_at = '/';
        }
    }
}

/* one file across, read in pieces and written in the same pieces */
static bool copy_file(const char *str_from, const char *str_to)
{
    long i_in = open(str_from);
    if (i_in < 0) {
        return false;
    }
    struct stat st;
    make_parents(str_to);
    if (stat(str_to, &st) == 0) {
        unlink(str_to);         /* written again from the start */
    }
    long i_out = create(str_to);
    bool b_ok = i_out >= 0;
    while (b_ok) {
        long i_got = read_fd(i_in, s_arr_chunk, CHUNK);
        if (i_got <= 0) {
            b_ok = i_got == 0;
            break;
        }
        b_ok = write_fd(i_out, s_arr_chunk, i_got) == i_got;
    }
    close(i_in);
    if (i_out >= 0) {
        close(i_out);
    }
    return b_ok;
}

/* a directory and everything under it, depth first, as the archive holds it */
static bool copy_tree(const char *str_from, const char *str_to)
{
    char arr_name[NAME_MAX];
    char arr_from[NAME_MAX];
    char arr_to[NAME_MAX];

    mkdir(str_to);
    for (long i_at = 0; readdir_at(i_at, arr_name, NAME_MAX, str_from) >= 0;
         i_at++) {
        size_t sz_len = strlen(arr_name);
        bool b_dir = sz_len > 0 && arr_name[sz_len - 1] == '/';
        if (b_dir) {
            arr_name[sz_len - 1] = '\0';
        }
        join(arr_from, str_from, arr_name);
        join(arr_to, str_to, arr_name);
        if (b_dir ? !copy_tree(arr_from, arr_to)
                  : !copy_file(arr_from, arr_to)) {
            printf("stopped at %s\n", arr_from);
            return false;
        }
        if (!b_dir) {
            printf("  %s\n", arr_to);
            s_i_files++;
        }
    }
    return true;
}

static int unpack(const char *str_to, const char *str_what)
{
    char arr_from[NAME_MAX];

    if (getuid() != 0) {
        printf("only the master may write the source out. `cat "
               SRC_AT "/...` reads it\n");
        return 1;
    }
    if (str_what != NULL) {
        join(arr_from, SRC_AT, str_what);
    } else {
        snprintf(arr_from, NAME_MAX, "%s", SRC_AT);
    }
    struct stat st;
    if (stat(arr_from, &st) != 0) {
        printf("there is nothing called %s in the archive\n", arr_from);
        return 1;
    }

    printf("writing %s to %s\n", arr_from, str_to);
    bool b_done = copy_tree(arr_from, str_to);
    if (s_i_files == 0) {
        printf("nothing was copied. %s holds no files\n", arr_from);
        return 1;
    }
    if (!b_done) {
        printf("stopped after %ld files\n", s_i_files);
        return 1;
    }
    if (sync() != 0) {
        printf("%ld files written, but the drive would not flush\n",
               s_i_files);
        return 1;
    }
    printf("%ld files under %s, and they are yours to edit\n", s_i_files,
           str_to);
    return 0;
}

int main(int i_argc, char **pp_argv)
{
    struct user_source s;
    bool b_verify = i_argc >= 2 && strcmp(pp_argv[1], "verify") == 0;
    bool b_unpack = i_argc >= 2 && strcmp(pp_argv[1], "unpack") == 0;

    if (i_argc >= 2 && !b_verify && !b_unpack) {
        printf("src [verify|unpack [where [what]]]\n");
        return 1;
    }
    if (source_tree(0, &s) != 0) {
        printf("this image carries no source tree. it was either built "
               "without one or\nthis machine was started by something "
               "that is not philemon\n");
        return 1;
    }

    if (b_unpack) {
        return unpack(i_argc >= 3 ? pp_argv[2] : "/src",
                      i_argc >= 4 ? pp_argv[3] : NULL);
    }

    if (b_verify) {
        printf("reading %lu KiB off the medium\n",
               (unsigned long)(s.u_bytes / 1024));
        if (source_tree(SOURCE_HASH, &s) != 0) {
            printf("the medium stopped answering part way through. what "
                   "is out there is not\nall there\n");
            return 1;
        }
        if (strcmp(s.arr_medium, s.arr_built) == 0) {
            printf("%s\nthis is the source this kernel was built from\n",
                   s.arr_medium);
            return 0;
        }
        printf("the medium says %s\nthis kernel says %s\nthese are not the "
               "same tree. the archive, the kernel, or both came from\n"
               "somewhere else\n", s.arr_medium, s.arr_built);
        return 1;
    }

    printf("%lu files, %lu KiB, at sector %lu of the boot medium\n",
           (unsigned long)s.u_files, (unsigned long)(s.u_bytes / 1024),
           (unsigned long)s.u_lba);
    printf("built as %s\n", s.arr_built);
    if (s.u_bytes != s.u_built_bytes) {
        printf("but this kernel was built with an archive of %lu KiB, "
               "which is not the\none out there. `src verify` says how "
               "far apart they are\n",
               (unsigned long)(s.u_built_bytes / 1024));
    }
    printf("\nnone of it is in memory: " SRC_AT " is read off the drive "
           "when somebody looks.\n`src verify` hashes the whole of it "
           "against what this kernel was built\nfrom. `src unpack` writes "
           "a copy somewhere you can change it\n");
    return 0;
}
