// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/arcana.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * arcana, which used to be a kernel command: the rank of this bond, and
 * its making, as /proc/version tells it.
 */

#include "report.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define NAME_MAX 256
#define RAMDISK  "/boot"

static char s_arr_version[512];
static char s_arr_mounts[REPORT_MAX];

/*
 * whether `str_dir` is a mount of its own, which is a filesystem other
 * than the ramdisk even when it hangs under it: /boot/src is the source
 * on the boot medium, and walking it is a walk of the whole tree
 */
static bool another_mount(const char *str_dir)
{
    size_t sz_len = strlen(str_dir);

    for (const char *str_line = s_arr_mounts; *str_line != '\0';) {
        if (strncmp(str_line, str_dir, sz_len) == 0
            && (str_line[sz_len] == ' ' || str_line[sz_len] == '\t')) {
            return true;
        }
        const char *str_next = strchr(str_line, '\n');
        if (str_next == NULL) {
            break;
        }
        str_line = str_next + 1;
    }
    return false;
}

/* how many files the ramdisk holds under `str_dir`, all the way down */
static long count_files(const char *str_dir)
{
    char arr_name[NAME_MAX];
    char arr_path[NAME_MAX];
    long i_count = 0;

    for (long i_at = 0; readdir_at(i_at, arr_name, NAME_MAX, str_dir) >= 0;
         i_at++) {
        size_t sz_len = strlen(arr_name);
        if (sz_len > 0 && arr_name[sz_len - 1] == '/') {
            arr_name[sz_len - 1] = '\0';
            snprintf(arr_path, NAME_MAX, "%s/%s", str_dir, arr_name);
            if (!another_mount(arr_path)) {
                i_count += count_files(arr_path);
            }
        } else {
            i_count++;
        }
    }
    return i_count;
}

/* the `)` that closes the `(` at `str_open`, brackets inside it and all */
static char *closing(char *str_open)
{
    int i_depth = 0;

    for (char *str_at = str_open; *str_at != '\0'; str_at++) {
        i_depth += (*str_at == '(') - (*str_at == ')');
        if (i_depth == 0) {
            return str_at;
        }
    }
    return NULL;
}

int main(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;

    /* "velvet version 0.4.1 (gcc 15.2.1) Oct 16 2026 15:20:09" */
    report_text("/proc/version", s_arr_version, sizeof s_arr_version);
    char *str_version = strstr(s_arr_version, "version ");
    char *str_gcc = strstr(s_arr_version, "(gcc ");
    char *str_end = (str_gcc != NULL) ? closing(str_gcc) : NULL;
    char *str_nl = strchr(s_arr_version, '\n');
    char *str_space = (str_version != NULL)
                      ? strchr(str_version + strlen("version "), ' ') : NULL;
    if (str_space == NULL || str_gcc == NULL || str_end == NULL) {
        printf("the kernel did not say what it is\n");
        return 1;
    }
    str_version += strlen("version ");
    *str_space = '\0';
    *str_end = '\0';
    if (str_nl != NULL) {
        *str_nl = '\0';
    }

    printf("\nThou art I... And I am thou...\n\n");
    printf("  THE COMPUTER ARCANA\n");
    printf("  rank %s, the bond deepens with every commit\n\n", str_version);
    printf("  version    velvet %s\n", str_version);
    printf("  forged     %s\n", str_end + 2);
    printf("  by         gcc %s\n", str_gcc + strlen("(gcc "));
    report_text("/proc/mounts", s_arr_mounts, REPORT_MAX);
    printf("  carrying   %ld files in the ramdisk\n\n",
           count_files(RAMDISK));
    return 0;
}
