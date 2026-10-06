// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/gitio_velvet.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the seam, on this machine.
 */

#include "gitio.h"
#include "syscall.h"

long gio_read(const char *path, void *out, size_t cap)
{
    long fd = open(path);
    if (fd < 0) {
        return -1;
    }
    long have = 0;
    long n;
    while (have < (long)cap
           && (n = read_fd(fd, (char *)out + have, (long)cap - have)) > 0) {
        have += n;
    }
    close(fd);
    return have;
}

int gio_write(const char *path, const void *data, size_t len)
{
    long fd = create(path);
    if (fd < 0) {
        return -1;
    }
    long done = 0;
    while (done < (long)len) {
        long n = write_fd(fd, (const char *)data + done, (long)len - done);
        if (n <= 0) {
            close(fd);
            return -1;
        }
        done += n;
    }
    close(fd);
    return 0;
}

int gio_mkdir(const char *path)
{
    mkdir(path);        /* already there is not an error worth having */
    return 0;
}

bool gio_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

bool gio_entry(const char *path, long n, char *name, size_t cap,
               bool *is_dir, bool *is_exec)
{
    char line[256];
    if (readdir_at(n, line, (long)sizeof line, path) <= 0) {
        return false;
    }
    unsigned long i = 0;
    while (line[i] != '\0' && i + 1 < cap) {
        name[i] = line[i];
        i++;
    }
    name[i] = '\0';

    char full[512];
    unsigned long at = 0;
    for (const char *p = path; *p != '\0' && at + 1 < sizeof full; p++) {
        full[at++] = *p;
    }
    if (at > 0 && full[at - 1] != '/' && at + 1 < sizeof full) {
        full[at++] = '/';
    }
    for (unsigned long k = 0; k < i && at + 1 < sizeof full; k++) {
        full[at++] = name[k];
    }
    full[at] = '\0';

    struct stat st;
    *is_dir = false;
    *is_exec = false;
    if (stat(full, &st) == 0) {
        *is_dir = st.is_dir != 0;
        *is_exec = (st.mode & 0100) != 0;
    }
    return true;
}

/*
 * epoch.c is two things: the arithmetic that turns a date into a number,
 * which is pure and is what a commit's timestamp needs, and
 * `epoch_now()`, which asks the timer chip what time it is and is
 * therefore kernel-only. an archive pulls in whole members, so taking
 * the first drags in the second and its reference to `pit_uptime_ms`.
 *
 * so it is answered here, and answered honestly: there is no timer chip
 * in ring 3, and nothing in this program calls the function that would
 * want one, the date comes from a file's timestamp instead. splitting
 * epoch.c the way signal.c and tty.c are split is the real fix, and it
 * belongs to whichever version next has a reason to touch it.
 */
unsigned long pit_uptime_ms(void);
unsigned long pit_uptime_ms(void)
{
    return 0;
}
