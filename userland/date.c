// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/date.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * date, which used to be a kernel command: what the battery-backed clock
 * believes, read through the kernel's count of seconds since 1970.
 */

#include "syscall.h"
#include <stdio.h>
#include <velvet/epoch.h>

static const char *const s_arr_months[] = {
    "", "january", "february", "march", "april", "may", "june", "july",
    "august", "september", "october", "november", "december",
};

static const char *suffix(unsigned u_day)
{
    if (u_day / 10 == 1) {
        return "th";
    }
    switch (u_day % 10) {
    case 1:
        return "st";
    case 2:
        return "nd";
    case 3:
        return "rd";
    default:
        return "th";
    }
}

int main(int i_argc, char **pp_argv)
{
    struct rtc_time t;

    (void)i_argc;
    (void)pp_argv;

    long i_now = now_seconds();
    if (i_now <= 0) {
        printf("the clock was never read, so the kernel does not know "
               "the time\n");
        return 1;
    }
    epoch_to_date((uint64_t)i_now, &t);
    printf("%02u:%02u:%02u on the %u%s of %s, %u\n", (unsigned)t.hour,
           (unsigned)t.minute, (unsigned)t.second, (unsigned)t.day,
           suffix(t.day), s_arr_months[t.month <= 12 ? t.month : 0],
           (unsigned)t.year);
    return 0;
}
