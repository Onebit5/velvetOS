// SPDX-License-Identifier: GPL-2.0-only
/*
 * toolchain/ar.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * an archive.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_MEMBERS 256
#define MAX_SYMS    16384

struct member {
    char     name[128];     /* the basename, which is all an archive keeps */
    uint8_t *data;
    size_t   size;
    size_t   at;            /* where its header lands, once that is known */
    long     longname;      /* offset into the name table, or -1 */
};
static struct member member[MAX_MEMBERS];
static int members;

struct entry {
    char name[192];
    int  from;              /* which member defines it */
};
static struct entry entry[MAX_SYMS];
static int entries;

static void die(const char *what, const char *detail)
{
    fprintf(stderr, "ar: %s%s%s\n", what, detail ? ": " : "",
            detail ? detail : "");
    exit(1);
}

struct shdr {
    uint32_t name; uint32_t type; uint64_t flags; uint64_t addr;
    uint64_t offset; uint64_t size; uint32_t link; uint32_t info;
    uint64_t align; uint64_t entsize;
};

struct sym {
    uint32_t name; uint8_t info; uint8_t other; uint16_t shndx;
    uint64_t value; uint64_t size;
};

/* every name a member offers. */
static void index_member(int which)
{
    struct member *m = &member[which];
    if (m->size < 64 || memcmp(m->data, "\177ELF", 4) != 0) {
        /*
         * not an object. it can still live in the archive, an archive
         * is a container and does not care, it simply defines nothing
         */
        return;
    }
    uint64_t shoff = *(uint64_t *)(m->data + 40);
    uint16_t shnum = *(uint16_t *)(m->data + 60);
    struct shdr *sh = (struct shdr *)(m->data + shoff);

    for (uint16_t i = 0; i < shnum; i++) {
        if (sh[i].type != 2) {              /* SHT_SYMTAB */
            continue;
        }
        struct sym *syms = (struct sym *)(m->data + sh[i].offset);
        size_t n = sh[i].size / sizeof *syms;
        const char *str = (const char *)(m->data + sh[sh[i].link].offset);

        for (size_t s = 0; s < n; s++) {
            uint8_t bind = syms[s].info >> 4;
            if ((bind != 1 && bind != 2) || syms[s].shndx == 0) {
                continue;                   /* local, or a use */
            }
            const char *name = str + syms[s].name;
            if (name[0] == '\0') {
                continue;
            }
            if (entries >= MAX_SYMS) {
                die("more names than can be indexed", name);
            }
            snprintf(entry[entries].name, sizeof entry[0].name, "%s", name);
            entry[entries].from = which;
            entries++;
        }
    }
}

/*
 * a header field is text padded with spaces, and *not* terminated,
 * the sixty bytes are the whole of it, and a stray nul in the middle is
 * an archive every other tool refuses
 */
static void field(char *at, size_t width, const char *text)
{
    size_t n = strlen(text);
    if (n > width) {
        die("a header field will not fit", text);
    }
    memcpy(at, text, n);
    memset(at + n, ' ', width - n);
}

static void header(FILE *f, const char *name, const char *mode, size_t size)
{
    char h[60];
    char text[32];
    /*
     * the name table has no date, no owner and no mode, it is not a
     * file and never was, and ar leaves those four fields blank rather
     * than writing zeros in them. it is the sort of difference that
     * only a byte comparison ever finds
     */
    bool real = (mode[0] != '\0');
    field(h + 0,  16, name);
    field(h + 16, 12, real ? "0" : "");  /* the date, which is always zero:
                                          * an archive built twice from the
                                          * same objects should be the same
                                          * file, and a timestamp is the one
                                          * thing that would stop it */
    field(h + 28, 6,  real ? "0" : "");  /* uid */
    field(h + 34, 6,  real ? "0" : "");  /* gid */
    field(h + 40, 8,  mode);
    snprintf(text, sizeof text, "%zu", size);
    field(h + 48, 10, text);
    memcpy(h + 58, "`\n", 2);
    fwrite(h, 1, sizeof h, f);
}

static uint8_t *slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        die("cannot read", path);
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)n + 1);
    if (data == NULL || fread(data, 1, (size_t)n, f) != (size_t)n) {
        die("could not read all of", path);
    }
    fclose(f);
    *size = (size_t)n;
    return data;
}

static void list(const char *path)
{
    size_t size;
    uint8_t *data = slurp(path, &size);
    if (size < 8 || memcmp(data, "!<arch>\n", 8) != 0) {
        die("not an archive", path);
    }
    const char *longnames = NULL;
    size_t at = 8;

    while (at + 60 <= size) {
        const char *h = (const char *)data + at;
        size_t msize = (size_t)strtoul(h + 48, NULL, 10);
        if (h[0] == '/' && h[1] == '/') {
            longnames = (const char *)data + at + 60;
        } else if (h[0] == '/' && (h[1] == ' ' || h[1] == '\n')) {
            /* the index, which is not a member anybody named */
        } else if (h[0] == '/' && longnames != NULL) {
            const char *name = longnames + strtoul(h + 1, NULL, 10);
            const char *end = strchr(name, '/');
            printf("%.*s\n", end ? (int)(end - name) : 0, name);
        } else {
            int n = 0;
            while (n < 16 && h[n] != '/' && h[n] != ' ') { n++; }
            printf("%.*s\n", n, h);
        }
        at += 60 + msize + (msize & 1);
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: ar rcs <archive.a> <member.o> ...\n"
                        "       ar t <archive.a>\n");
        return 2;
    }
    const char *how = argv[1];
    const char *path = argv[2];

    if (strchr(how, 't') != NULL) {
        list(path);
        return 0;
    }
    if (strchr(how, 'r') == NULL) {
        die("only r, s and t are known", how);
    }

    for (int i = 3; i < argc; i++) {
        if (members >= MAX_MEMBERS) {
            die("too many members", argv[i]);
        }
        struct member *m = &member[members];
        const char *slash = strrchr(argv[i], '/');
        snprintf(m->name, sizeof m->name, "%s", slash ? slash + 1 : argv[i]);
        m->data = slurp(argv[i], &m->size);
        m->longname = -1;
        index_member(members);
        members++;
    }

    /*
     * the index has to say where every member starts, and a member does
     * not have a place until everything before it has a size, so the
     * whole file is measured first and written second. that is the only
     * awkward part of the format and it is why this is not four lines
     */
    size_t names = 0;
    for (int i = 0; i < members; i++) {
        if (strlen(member[i].name) > 15) {
            member[i].longname = (long)names;
            names += strlen(member[i].name) + 2;    /* the name, `/`, `\n` */
        }
    }
    size_t names_size = names + (names & 1);

    size_t index_size = 4 + 4 * (size_t)entries;
    for (int i = 0; i < entries; i++) {
        index_size += strlen(entry[i].name) + 1;
    }
    /*
     * every member starts on an even offset, and the index rounds *its
     * own size up* to get there rather than being followed by a pad
     * byte the way the members are. the two are indistinguishable until
     * an archive turns up whose index is an odd number of bytes long
     */
    index_size += index_size & 1;

    size_t at = 8 + 60 + index_size;
    if (names_size != 0) {
        at += 60 + names_size;
    }
    for (int i = 0; i < members; i++) {
        member[i].at = at;
        at += 60 + member[i].size + (member[i].size & 1);
    }

    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        die("cannot write", path);
    }
    fwrite("!<arch>\n", 1, 8, f);

    header(f, "/", "0", index_size);
    uint8_t count[4] = { (uint8_t)(entries >> 24), (uint8_t)(entries >> 16),
                         (uint8_t)(entries >> 8), (uint8_t)entries };
    fwrite(count, 1, 4, f);
    for (int i = 0; i < entries; i++) {
        /*
         * big-endian, which is what this format says whatever the
         * machine thinks, an archive is meant to be readable by
         * whoever picks it up
         */
        size_t off = member[entry[i].from].at;
        uint8_t be[4] = { (uint8_t)(off >> 24), (uint8_t)(off >> 16),
                          (uint8_t)(off >> 8), (uint8_t)off };
        fwrite(be, 1, 4, f);
    }
    size_t written = 4 + 4 * (size_t)entries;
    for (int i = 0; i < entries; i++) {
        fwrite(entry[i].name, 1, strlen(entry[i].name) + 1, f);
        written += strlen(entry[i].name) + 1;
    }
    while (written < index_size) {
        fputc('\0', f);
        written++;
    }

    if (names_size != 0) {
        header(f, "//", "", names_size);
        for (int i = 0; i < members; i++) {
            if (member[i].longname >= 0) {
                fwrite(member[i].name, 1, strlen(member[i].name), f);
                fwrite("/\n", 1, 2, f);
            }
        }
        if (names & 1) {
            fputc('\n', f);
        }
    }

    for (int i = 0; i < members; i++) {
        char name[32];
        if (member[i].longname >= 0) {
            snprintf(name, sizeof name, "/%ld", member[i].longname);
        } else {
            snprintf(name, sizeof name, "%.15s/", member[i].name);
        }
        header(f, name, "644", member[i].size);
        fwrite(member[i].data, 1, member[i].size, f);
        if (member[i].size & 1) {
            fputc('\n', f);
        }
    }

    fclose(f);
    return 0;
}
