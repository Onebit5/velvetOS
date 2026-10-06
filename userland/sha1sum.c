// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/sha1sum.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * what is this file, exactly.
 */

#include "syscall.h"
#include "args.h"
#include "lib/hash.h"

static const struct opt sha1sum_opts[] = {
    { 'c', "crc32", false, "crc32 instead, as gpt and zip use" },
    { 'a', "adler32", false, "adler32, as a zlib stream ends with" },
    { 'g', "git", false, "the name git would give this file" },
};

static const struct program sha1sum = {
    .name = "sha1sum",
    .usage = "sha1sum [-c] [-a] [-g] [file...]",
    .summary = "print the checksum of each file",
    .opts = sha1sum_opts,
    .opt_count = sizeof sha1sum_opts / sizeof sha1sum_opts[0],
};

/* the answers being accumulated, all three at once. */
struct sums {
    struct sha1 sha;
    uint32_t    crc;
    uint32_t    adler;
    long        bytes;
};

static void sums_init(struct sums *s)
{
    sha1_init(&s->sha);
    s->crc = crc32_start();
    s->adler = adler32_start();
    s->bytes = 0;
}

static void sums_take(struct sums *s, const char *data, long len)
{
    sha1_update(&s->sha, data, (unsigned long)len);
    s->crc = crc32_more(s->crc, data, (unsigned long)len);
    s->adler = adler32_more(s->adler, data, (unsigned long)len);
    s->bytes += len;
}

/* a whole file, in whatever pieces `read` hands over. */
static bool eat(long fd, struct sums *s)
{
    char buf[512];
    long n;
    while ((n = read_fd(fd, buf, sizeof buf)) > 0) {
        sums_take(s, buf, n);
    }
    return n == 0;
}

static void write_hex32(uint32_t v)
{
    static const char digit[] = "0123456789abcdef";
    char out[9];
    for (int i = 0; i < 8; i++) {
        out[7 - i] = digit[(v >> (i * 4)) & 0x0f];
    }
    out[8] = '\0';
    write(out);
}

static void report(struct sums *s, const char *name, bool crc, bool adler,
                   bool git)
{
    uint8_t digest[20];
    char hex[41];

    if (crc) {
        write_hex32(s->crc);
    } else if (adler) {
        write_hex32(s->adler);
    } else if (git) {
        /* git names a blob by hashing "blob <length>\0" and then the content. */
        struct sha1 g;
        char header[32];
        long n = 0;
        const char *word = "blob ";
        while (*word != '\0') {
            header[n++] = *word++;
        }
        long value = s->bytes;
        char digits[20];
        long d = 0;
        do {
            digits[d++] = (char)('0' + value % 10);
            value /= 10;
        } while (value > 0);
        while (d > 0) {
            header[n++] = digits[--d];
        }
        header[n++] = '\0';

        sha1_init(&g);
        sha1_update(&g, header, (unsigned long)n);
        /*
         * and the content again, which is the price of not knowing the
         * length until the end
         */
        long fd = name != NULL ? open(name) : -1;
        if (fd >= 0) {
            char buf[512];
            long got;
            while ((got = read_fd(fd, buf, sizeof buf)) > 0) {
                sha1_update(&g, buf, (unsigned long)got);
            }
            close(fd);
        }
        sha1_final(&g, digest);
        sha1_hex(digest, hex);
        write(hex);
    } else {
        sha1_final(&s->sha, digest);
        sha1_hex(digest, hex);
        write(hex);
    }

    write("  ");
    write(name != NULL ? name : "-");
    write("\n");
}

void _start(int argc, char **argv)
{
    struct args a;
    const char *error;
    if (!args_parse(&sha1sum, argc, argv, &a, &error)) {
        write("sha1sum: ");
        write(error);
        write("\n");
        exit(1);
    }
    if (a.wants_help) {
        args_usage(&sha1sum);
        exit(0);
    }

    bool crc = args_has(&a, &sha1sum, 'c');
    bool adler = args_has(&a, &sha1sum, 'a');
    bool git = args_has(&a, &sha1sum, 'g');

    if (a.count == 0) {
        struct sums s;
        sums_init(&s);
        eat(STDIN, &s);
        /*
         * --git needs the file a second time, and standard input does
         * not come round twice
         */
        report(&s, NULL, crc, adler, false);
        exit(0);
    }

    long bad = 0;
    for (int i = 0; i < a.count; i++) {
        long fd = open(a.rest[i]);
        if (fd < 0) {
            write("sha1sum: cannot open ");
            write(a.rest[i]);
            write("\n");
            bad++;
            continue;
        }
        struct sums s;
        sums_init(&s);
        eat(fd, &s);
        close(fd);
        report(&s, a.rest[i], crc, adler, git);
    }
    exit(bad > 0 ? 1 : 0);
}
