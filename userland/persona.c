// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/persona.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * persona, which used to be a kernel command: fastfetch, if fastfetch had
 * read a tarot deck. everything it says is read out of /proc.
 */

#include "report.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINES 8

static const char *const s_arr_mask[LINES] = {
    "     .-\"\"\"\"\"-.     ",
    "   .'  _     _  '.   ",
    "  /   (o)   (o)   \\  ",
    " |       ---       | ",
    " |    \\  ___  /    | ",
    "  \\    '.___.'    /  ",
    "   '.           .'   ",
    "     '-._____.-'     ",
};

static char s_arr_text[REPORT_MAX];

/* the number after `str_label` in the text, or 0 when it is not there */
static long number_after(const char *str_text, const char *str_label)
{
    const char *str_at = strstr(str_text, str_label);
    return (str_at != NULL) ? strtol(str_at + strlen(str_label), NULL, 10)
                            : 0;
}

/* one line of `str_text` after `str_label`, copied out */
static void line_after(const char *str_text, const char *str_label,
                       char *str_out, size_t sz_size)
{
    const char *str_at = strstr(str_text, str_label);
    size_t sz_len = 0;

    if (str_at != NULL) {
        str_at += strlen(str_label);
        while (str_at[sz_len] != '\0' && str_at[sz_len] != '\n'
               && sz_len + 1 < sz_size) {
            str_out[sz_len] = str_at[sz_len];
            sz_len++;
        }
    }
    str_out[sz_len] = '\0';
}

/* the thread table has one line per thread, each starting with its id */
static long count_threads(const char *str_text)
{
    long i_count = 0;

    for (const char *str_line = str_text; *str_line != '\0';) {
        const char *str_digit = str_line;
        while (*str_digit == ' ') {
            str_digit++;
        }
        if (str_digit - str_line >= 2 && *str_digit >= '0'
            && *str_digit <= '9') {
            i_count++;
        }
        const char *str_next = strchr(str_line, '\n');
        if (str_next == NULL) {
            break;
        }
        str_line = str_next + 1;
    }
    return i_count;
}

int main(int i_argc, char **pp_argv)
{
    char arr_brand[64];
    char arr_version[64];
    unsigned u_cols = 0;
    unsigned u_rows = 0;

    (void)i_argc;
    (void)pp_argv;

    report_text("/proc/cpuinfo", s_arr_text, REPORT_MAX);
    line_after(s_arr_text, "model name  ", arr_brand, sizeof arr_brand);
    report_text("/proc/version", s_arr_text, REPORT_MAX);
    line_after(s_arr_text, "velvet version ", arr_version,
               sizeof arr_version);
    char *str_space = strchr(arr_version, ' ');
    if (str_space != NULL) {
        *str_space = '\0';         /* the number, and not what follows it */
    }
    report_text("/proc/meminfo", s_arr_text, REPORT_MAX);
    long i_total = number_after(s_arr_text, "  total  ");
    long i_used = number_after(s_arr_text, "  used   ") / 1024;
    report_text("/proc/threads", s_arr_text, REPORT_MAX);
    long i_threads = count_threads(s_arr_text);
    long i_ms = uptime();
    winsize(&u_cols, &u_rows);

    for (int i_line = 0; i_line < LINES; i_line++) {
        printf("%s", s_arr_mask[i_line]);
        switch (i_line) {
        case 0:
            printf("velvet@velvetOS");
            break;
        case 1:
            printf("-------------");
            break;
        case 2:
            printf("arcana    the Computer, rank %s", arr_version);
            break;
        case 3:
            printf("persona   %s", arr_brand);
            break;
        case 4:
            printf("awakened  %ldh %ldm %lds", i_ms / 3600000,
                   (i_ms / 60000) % 60, (i_ms / 1000) % 60);
            break;
        case 5:
            printf("souls     %ld threads bound to the wheel", i_threads);
            break;
        case 6:
            printf("memory    %ld / %ld MiB", i_used, i_total);
            break;
        case 7:
            printf("vision    %ux%u of glyphs", u_cols, u_rows);
            break;
        }
        printf("\n");
    }
    printf("\n");
    return 0;
}
