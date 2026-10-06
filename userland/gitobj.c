// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/gitobj.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the git object store: formatting objects, and reading them back.
 */

#include "gitobj.h"

#include <string.h>
#include <stdlib.h>

#include "gitio.h"
#include "lib/hash.h"
#include "lib/deflate.h"


/* this file used snprintf until it had to run in two places. */

static size_t put_text(char *out, size_t at, size_t cap, const char *s)
{
    while (*s != '\0' && at + 1 < cap) {
        out[at++] = *s++;
    }
    return at;
}

static size_t put_number(char *out, size_t at, size_t cap, unsigned long v,
                         int base)
{
    char digits[24];
    int n = 0;
    do {
        int d = (int)(v % (unsigned long)base);
        digits[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        v /= (unsigned long)base;
    } while (v > 0);
    while (n > 0 && at + 1 < cap) {
        out[at++] = digits[--n];
    }
    return at;
}

/*
 * `<objects>/ab/cdef...`, in the two pieces git splits a name into: the
 * first two characters are the directory, so a repository with a hundred
 * thousand objects has 256 directories rather than one
 */
static void object_path(char *out, size_t cap, const char *objects,
                        const char *hex, bool dir_only)
{
    size_t at = put_text(out, 0, cap, objects);
    at = put_text(out, at, cap, "/");
    if (at + 2 < cap) {
        out[at++] = hex[0];
        out[at++] = hex[1];
    }
    if (!dir_only) {
        at = put_text(out, at, cap, "/");
        at = put_text(out, at, cap, hex + 2);
    }
    out[at] = '\0';
}

static const char *type_word(enum git_type type)
{
    switch (type) {
    case GIT_TREE:   return "tree";
    case GIT_COMMIT: return "commit";
    default:         return "blob";
    }
}

/* `<type> <length>\0`, which is hashed with the content and stored with it. */
static int header_of(char *out, size_t cap, enum git_type type, size_t len)
{
    size_t at = put_text(out, 0, cap, type_word(type));
    at = put_text(out, at, cap, " ");
    at = put_number(out, at, cap, (unsigned long)len, 10);
    out[at] = '\0';
    return (int)at + 1;         /* the zero byte is part of it */
}

void git_name(enum git_type type, const void *data, size_t len,
              char hex[GIT_HEX])
{
    char header[64];
    int hn = header_of(header, sizeof header, type, len);

    struct sha1 s;
    sha1_init(&s);
    sha1_update(&s, header, (size_t)hn);
    sha1_update(&s, data, len);

    uint8_t digest[20];
    sha1_final(&s, digest);
    sha1_hex(digest, hex);
}

int git_write(const char *objects, enum git_type type, const void *data,
              size_t len, char hex[GIT_HEX])
{
    git_name(type, data, len, hex);

    char dir[1024], path[1152];
    object_path(dir, sizeof dir, objects, hex, true);
    object_path(path, sizeof path, objects, hex, false);

    /* already there is the ordinary case rather than an error. */
    if (gio_exists(path)) {
        return 0;
    }
    gio_mkdir(objects);
    gio_mkdir(dir);

    char header[64];
    int hn = header_of(header, sizeof header, type, len);

    size_t whole_len = (size_t)hn + len;
    uint8_t *whole = malloc(whole_len);
    if (whole == NULL) {
        return -1;
    }
    memcpy(whole, header, (size_t)hn);
    memcpy(whole + hn, data, len);

    /*
     * compressed, because a loose object is a zlib stream and nothing
     * about that is optional, git will not read it otherwise
     */
    size_t cap = whole_len + whole_len / 2 + 1024;
    uint8_t *packed = malloc(cap);
    if (packed == NULL) {
        free(whole);
        return -1;
    }
    long n = zlib_deflate(whole, whole_len, packed, cap);
    free(whole);
    if (n < 0) {
        free(packed);
        return -1;
    }

    int wrote = gio_write(path, packed, (size_t)n);
    free(packed);
    return wrote;
}

long git_read(const char *objects, const char *hex, enum git_type *type,
              void *out, size_t cap)
{
    char path[1152];
    object_path(path, sizeof path, objects, hex, false);

    /* an object is small: read it whole rather than growing a buffer. */
    size_t packed_cap = cap + 4096;
    uint8_t *packed = malloc(packed_cap);
    if (packed == NULL) {
        return -1;
    }
    long packed_len = gio_read(path, packed, packed_cap);
    if (packed_len <= 0) {
        free(packed);
        return -1;
    }

    /*
     * the whole thing, header and all, has to come out before the
     * header can say how long the content is, which is the price of
     * hashing them together, and the reason a git object cannot be
     * read as a stream without knowing its length twice
     */
    size_t room = cap + 64;
    uint8_t *whole = malloc(room);
    if (whole == NULL) {
        free(packed);
        return -1;
    }
    long n = zlib_inflate(packed, (size_t)packed_len, whole, room);
    free(packed);
    if (n < 0) {
        free(whole);
        return n;
    }

    uint8_t *nul = memchr(whole, '\0', (size_t)n);
    if (nul == NULL) {
        free(whole);
        return -1;
    }
    if (memcmp(whole, "blob ", 5) == 0) {
        *type = GIT_BLOB;
    } else if (memcmp(whole, "tree ", 5) == 0) {
        *type = GIT_TREE;
    } else if (memcmp(whole, "commit ", 7) == 0) {
        *type = GIT_COMMIT;
    } else {
        free(whole);
        return -1;
    }

    size_t body = (size_t)n - (size_t)(nul + 1 - whole);
    long claimed = 0;
    /*
     * XXX: this dereferences what memchr returned, plus one. the search
     * cannot fail today, and only because every type word the checks
     * above accept carries a space, at index 4 or index 6. reorder those
     * checks or add a type whose word has none and this reads address 1.
     * test the result for null and return -1, the way the search for the
     * length's own zero byte above already does.
     */
    for (const char *d = (char *)memchr(whole, ' ', (size_t)n) + 1;
         *d >= '0' && *d <= '9'; d++) {
        claimed = claimed * 10 + (*d - '0');
    }
    if (claimed != (long)body) {
        free(whole);
        return -1;      /* the header disagrees with what came out of it */
    }
    if (body > cap) {
        free(whole);
        return -1;
    }
    memcpy(out, nul + 1, body);
    free(whole);
    return (long)body;
}



/* the comparison the format demands, and it is not strcmp. */
static int tree_order(const struct git_entry *a, const struct git_entry *b)
{
    size_t an = strlen(a->name), bn = strlen(b->name);
    size_t n = an < bn ? an : bn;

    int d = memcmp(a->name, b->name, n);
    if (d != 0) {
        return d;
    }
    unsigned char at = an > n ? (unsigned char)a->name[n]
                    : (a->mode == GIT_MODE_TREE ? '/' : 0);
    unsigned char bt = bn > n ? (unsigned char)b->name[n]
                    : (b->mode == GIT_MODE_TREE ? '/' : 0);
    return (int)at - (int)bt;
}

long git_tree_encode(struct git_entry *entry, int count, void *out,
                     size_t cap)
{
    for (int i = 1; i < count; i++) {
        struct git_entry keep = entry[i];
        int j = i - 1;
        while (j >= 0 && tree_order(&entry[j], &keep) > 0) {
            entry[j + 1] = entry[j];
            j--;
        }
        entry[j + 1] = keep;
    }

    uint8_t *p = out;
    size_t at = 0;
    for (int i = 0; i < count; i++) {
        /*
         * octal, and with no leading zero: a directory is `40000` and
         * not `040000`, which is the sort of detail that only a real
         * git reading the result will ever tell you about
         */
        at = put_number((char *)p, at, cap, entry[i].mode, 8);
        at = put_text((char *)p, at, cap, " ");
        at = put_text((char *)p, at, cap, entry[i].name);
        if (at + 21 > cap) {
            return -1;
        }
        p[at++] = '\0';
        /* the twenty *raw* bytes, not the forty characters. */
        memcpy(p + at, entry[i].sha, 20);
        at += 20;
    }
    return (long)at;
}

int git_tree_decode(const void *data, size_t len, struct git_entry *out,
                    int max)
{
    const uint8_t *p = data;
    size_t at = 0;
    int found = 0;

    while (at < len && found < max) {
        size_t start = at;
        while (at < len && p[at] != ' ') {
            at++;
        }
        if (at >= len) {
            return -1;
        }
        char mode[16];
        size_t mode_len = at - start;
        if (mode_len >= sizeof mode) {
            return -1;
        }
        memcpy(mode, p + start, mode_len);
        mode[mode_len] = '\0';
        /*
         * octal, by hand. this machine's libc has strtol and the host's
         * has strtoul, and the store is not the place to find out which
         *, six digits of base eight is four lines
         */
        unsigned value = 0;
        for (const char *d = mode; *d >= '0' && *d <= '7'; d++) {
            value = value * 8 + (unsigned)(*d - '0');
        }
        out[found].mode = value;
        at++;

        start = at;
        while (at < len && p[at] != '\0') {
            at++;
        }
        if (at >= len || at - start >= sizeof out[found].name) {
            return -1;
        }
        memcpy(out[found].name, p + start, at - start);
        out[found].name[at - start] = '\0';
        at++;

        if (at + 20 > len) {
            return -1;
        }
        memcpy(out[found].sha, p + at, 20);
        at += 20;
        found++;
    }
    return found;
}



long git_commit_encode(const struct git_commit *c, void *out, size_t cap)
{
    /* plain text, and the order of the lines is part of the format. */
    char *o = out;
    size_t at = put_text(o, 0, cap, "tree ");
    at = put_text(o, at, cap, c->tree);
    at = put_text(o, at, cap, "\n");
    if (c->parent[0] != '\0') {
        at = put_text(o, at, cap, "parent ");
        at = put_text(o, at, cap, c->parent);
        at = put_text(o, at, cap, "\n");
    }
    for (int i = 0; i < 2; i++) {
        at = put_text(o, at, cap, i == 0 ? "author " : "committer ");
        at = put_text(o, at, cap, c->who);
        at = put_text(o, at, cap, " ");
        at = put_number(o, at, cap, (unsigned long)c->when, 10);
        at = put_text(o, at, cap, " ");
        at = put_text(o, at, cap, c->offset);
        at = put_text(o, at, cap, "\n");
    }
    at = put_text(o, at, cap, "\n");
    at = put_text(o, at, cap, c->message);
    return (long)at;
}

bool git_commit_decode(const void *data, size_t len, char tree[GIT_HEX],
                       char parent[GIT_HEX], const char **message)
{
    const char *p = data;
    size_t at = 0;
    tree[0] = '\0';
    parent[0] = '\0';
    *message = NULL;

    while (at < len) {
        size_t start = at;
        while (at < len && p[at] != '\n') {
            at++;
        }
        size_t line = at - start;
        if (line == 0) {
            /*
             * the blank line: everything after it is the message, and
             * nothing in it is a header however much it looks like one
             */
            *message = p + at + 1;
            return tree[0] != '\0';
        }
        if (line > 5 && memcmp(p + start, "tree ", 5) == 0 && line >= 45) {
            memcpy(tree, p + start + 5, 40);
            tree[40] = '\0';
        } else if (line > 7 && memcmp(p + start, "parent ", 7) == 0
                   && parent[0] == '\0' && line >= 47) {
            memcpy(parent, p + start + 7, 40);
            parent[40] = '\0';
        }
        at++;
    }
    return false;
}

static void hex_to_raw(const char *hex, uint8_t *out)
{
    for (int i = 0; i < 20; i++) {
        int hi = hex[i * 2], lo = hex[i * 2 + 1];
        hi = hi <= '9' ? hi - '0' : (hi | 32) - 'a' + 10;
        lo = lo <= '9' ? lo - '0' : (lo | 32) - 'a' + 10;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
}

bool git_write_tree(const char *objects, const char *dir, char hex[GIT_HEX],
                    void *scratch, size_t scratch_cap)
{
    struct git_entry *entry = malloc(sizeof *entry * 256);
    if (entry == NULL) {
        return false;
    }
    int count = 0;

    /*
     * FIXME: this stops at 256 entries. a directory holding more than
     * that gets a tree naming the first 256 of them and a commit saying
     * it is the whole of it, and nothing says otherwise: the hash is of
     * what was written, so the object is internally consistent and simply
     * not what was in the directory. real git reads it happily, which is
     * what makes it quiet. the count wants to grow with the entries, or
     * the walk wants to refuse.
     */
    for (long i = 0; count < 256; i++) {
        char name[256];
        bool is_dir = false, is_exec = false;
        if (!gio_entry(dir, i, name, sizeof name, &is_dir, &is_exec)) {
            break;
        }
        /* the repository is not part of what it records. */
        if (name[0] == '.' && name[1] == 'g' && name[2] == 'i'
            && name[3] == 't' && name[4] == '\0') {
            continue;
        }

        char path[1024];
        size_t at = put_text(path, 0, sizeof path, dir);
        at = put_text(path, at, sizeof path, "/");
        at = put_text(path, at, sizeof path, name);
        path[at] = '\0';

        char child[GIT_HEX];
        if (is_dir) {
            if (!git_write_tree(objects, path, child, scratch, scratch_cap)) {
                free(entry);
                return false;
            }
            entry[count].mode = GIT_MODE_TREE;
        } else {
            long n = gio_read(path, scratch, scratch_cap);
            if (n < 0 || git_write(objects, GIT_BLOB, scratch, (size_t)n,
                                   child) != 0) {
                free(entry);
                return false;
            }
            entry[count].mode = is_exec ? GIT_MODE_EXEC : GIT_MODE_FILE;
        }
        at = put_text(entry[count].name, 0, sizeof entry[count].name, name);
        entry[count].name[at] = '\0';
        hex_to_raw(child, entry[count].sha);
        count++;
    }

    long n = git_tree_encode(entry, count, scratch, scratch_cap);
    free(entry);
    if (n < 0) {
        return false;
    }
    return git_write(objects, GIT_TREE, scratch, (size_t)n, hex) == 0;
}
