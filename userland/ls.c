// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/ls.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * ls.
 */

#include "syscall.h"
#include "args.h"

static const struct opt ls_opts[] = {
    /*
     * TODO: `-a` is declared and never read. the summary counts files and
     * directories either way, so the flag changes nothing while the help
     * text promises a choice that is not offered. wire it to the count or
     * drop it, the same decision string.h's two uncalled functions are
     * waiting on.
     */
    { 'a', "all",  false, "count the directories too, not just the files" },
    { '1', "one",  false, "one name per line and nothing else" },
    { 'l', "long", false, "how big each one is and when it was written" },
};

static const struct program ls = {
    .name = "ls",
    .usage = "ls [-a] [-1] [-l] [directory]",
    .summary = "what is in a directory",
    .opts = ls_opts,
    .opt_count = sizeof ls_opts / sizeof ls_opts[0],
};

static long ends_with_slash(const char *s)
{
    long n = 0;
    while (s[n]) n++;
    return n > 0 && s[n - 1] == '/';
}

/*
 * a number in a column of a fixed width, right-aligned, because a
 * column of sizes that do not line up is a column you have to read
 * rather than one you can glance at
 */
static void write_num_wide(long v, long width)
{
    char digits[24];
    long n = 0;
    if (v == 0) {
        digits[n++] = '0';
    }
    while (v > 0) {
        digits[n++] = (char)('0' + v % 10);
        v /= 10;
    }
    for (long i = n; i < width; i++) {
        write(" ");
    }
    while (n-- > 0) {
        char one[2] = { digits[n], '\0' };
        write(one);
    }
}

/*
 * two digits, always, so 2026-08-07 does not come out as 2026-8-7 and
 * shift everything after it along by a column
 */
static void write_two(long v)
{
    char out[3] = { (char)('0' + (v / 10) % 10), (char)('0' + v % 10), '\0' };
    write(out);
}

static bool join(char *out, long cap, const char *dir, const char *name)
{
    long n = 0;
    for (const char *p = dir; *p; p++) {
        if (n >= cap - 1) return false;
        out[n++] = *p;
    }
    if (n > 0 && out[n - 1] != '/') {
        if (n >= cap - 1) return false;
        out[n++] = '/';
    }
    for (const char *p = name; *p; p++) {
        if (n >= cap - 1) return false;
        out[n++] = *p;
    }
    out[n] = '\0';
    return true;
}

/* the long form of one name: size, date, name. */
/* rwxr-xr-x, which is nine bits read straight off the page. */
static void write_mode(const struct stat *st)
{
    static const char bits[] = "rwx";
    char out[11];

    out[0] = st->is_symlink ? 'l' : (st->is_dir ? 'd' : '-');
    for (int i = 0; i < 9; i++) {
        out[1 + i] = (st->mode & (0400 >> i)) ? bits[i % 3] : '-';
    }
    out[10] = '\0';
    write(out);
}

static void write_long(const char *dir, const char *name)
{
    char full[256];
    struct stat st;

    if (!join(full, sizeof full, dir, name) || stat(full, &st) < 0) {
        write("  ?????????  ?  ------- --:--  ");
        return;
    }

    write("  ");
    write_mode(&st);
    write(" ");
    write_num_wide((long)st.uid, 4);

    if (st.is_dir) {
        /*
         * a directory's size is the size of its own list of entries,
         * which is a fact about the filesystem and not about anything
         * anybody keeps in there. a dash says so
         */
        write("       -");
    } else {
        write_num_wide((long)st.size, 8);
    }

    if (st.year < 1980) {
        write("  ----------  --:--  ");
        return;
    }

    write("  ");
    write_num((long)st.year);
    write("-");
    write_two(st.month);
    write("-");
    write_two(st.day);
    write("  ");
    write_two(st.hour);
    write(":");
    write_two(st.minute);
    write("  ");
}

void _start(int argc, char **argv)
{
    struct args a;
    const char *error;
    if (!args_parse(&ls, argc, argv, &a, &error)) {
        write("ls: ");
        write(error);
        write("\n");
        exit(1);
    }
    if (a.wants_help) {
        args_usage(&ls);
        exit(0);
    }

    /*
     * here, not the root. `ls` with no argument means the directory you
     * are in, it hardcoded "/" and so ignored every `cd` ever typed,
     * which looked like the shell losing track of where it was rather
     * than like this line
     */
    const char *path = (a.count > 0) ? a.rest[0] : ".";
    bool plain = args_has(&a, &ls, '1');
    bool full = args_has(&a, &ls, 'l');

    char name[128];
    long files = 0;
    long dirs = 0;

    for (long i = 0; ; i++) {
        if (readdir_at(i, name, sizeof name, path) < 0) {
            break;
        }
        if (name[0] == '\0') {
            continue;
        }

        if (ends_with_slash(name)) {
            dirs++;
        } else {
            files++;
        }

        if (full) {
            write_long(path, name);
        } else if (!plain) {
            write("  ");
        }
        write(name);

        /*
         * a symlink is worth seeing as one, and worth seeing where it
         * points, that is most of why anybody runs ls -l on one
         */
        if (full) {
            char joined[256];
            struct stat st;
            char target[256];
            if (join(joined, sizeof joined, path, name)
                && stat(joined, &st) == 0 && st.is_symlink
                && readlink(joined, target, sizeof target) > 0) {
                write(" -> ");
                write(target);
            }
        }
        write("\n");
    }

    if (files == 0 && dirs == 0) {
        write("nothing there, or no such directory\n");
        exit(1);
    }

    if (plain) {
        exit(0);
    }

    write_num(files);
    write(" files");
    if (dirs > 0) {
        write(", ");
        write_num(dirs);
        write(" directories");
    }
    write("\n");
    exit(0);
}
