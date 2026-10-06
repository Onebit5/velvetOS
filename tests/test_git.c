// SPDX-License-Identifier: GPL-2.0-only
/*
 * tests/test_git.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the object store, checked by handing the result to git.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "gitobj.h"

static int failures;

#define CHECK(cond, what) do {                                          \
        if (!(cond)) {                                                  \
            printf("  FAIL %s (%s:%d)\n", what, __FILE__, __LINE__);     \
            failures++;                                                 \
        }                                                               \
    } while (0)

static void test_names(void)
{
    char hex[GIT_HEX];

    /*
     * the empty blob, whose name every git repository in the world
     * contains and which is the one object nobody ever wrote on purpose
     */
    git_name(GIT_BLOB, "", 0, hex);
    CHECK(strcmp(hex, "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391") == 0,
          "the empty blob has the name it has everywhere");

    git_name(GIT_BLOB, "hello\n", 6, hex);
    CHECK(strcmp(hex, "ce013625030ba8dba906f756967f9e9ca394464a") == 0,
          "and `hello` is the one git prints for it");
}

static void test_tree_order(void)
{
    /* the case the format is peculiar about. */
    struct git_entry e[3];
    memset(e, 0, sizeof e);
    e[0].mode = GIT_MODE_TREE;  strcpy(e[0].name, "lib");
    e[1].mode = GIT_MODE_FILE;  strcpy(e[1].name, "lib.c");
    e[2].mode = GIT_MODE_FILE;  strcpy(e[2].name, "a.c");

    uint8_t out[512];
    long n = git_tree_encode(e, 3, out, sizeof out);
    CHECK(n > 0, "the tree encoded");

    struct git_entry back[3];
    int count = git_tree_decode(out, (size_t)n, back, 3);
    CHECK(count == 3, "and decoded to three entries");
    CHECK(strcmp(back[0].name, "a.c") == 0, "a.c first");
    CHECK(strcmp(back[1].name, "lib.c") == 0,
          "then lib.c, before the directory, not after it");
    CHECK(strcmp(back[2].name, "lib") == 0, "and lib last");

    /* the mode has to be octal without a leading zero */
    CHECK(memcmp(out, "100644 a.c", 10) == 0,
          "a file is 100644 and the name follows a space");
}

static void test_round_trip(void)
{
    const char *objects = "bin/tests/gitobjects";
    mkdir("bin/tests", 0755);
    mkdir(objects, 0755);

    char hex[GIT_HEX];
    const char *content = "the quick brown fox\n";
    CHECK(git_write(objects, GIT_BLOB, content, 20, hex) == 0,
          "a blob was written");

    /* writing it again must be quiet and must not change the name */
    char again[GIT_HEX];
    CHECK(git_write(objects, GIT_BLOB, content, 20, again) == 0
          && strcmp(hex, again) == 0,
          "and writing it twice is writing it once");

    enum git_type type;
    char back[128];
    long n = git_read(objects, hex, &type, back, sizeof back);
    CHECK(n == 20 && memcmp(back, content, 20) == 0,
          "and it reads back as itself");
    CHECK(type == GIT_BLOB, "still a blob");

    CHECK(git_read(objects, "0000000000000000000000000000000000000000",
                   &type, back, sizeof back) < 0,
          "and an object that is not there is not invented");
}



static uint8_t buffer[1 << 22];

static long slurp(const char *path, void *out, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return -1;
    }
    size_t n = fread(out, 1, cap, f);
    fclose(f);
    return (long)n;
}

int main(int argc, char **argv)
{
    if (argc >= 3) {
        const char *what = argv[1];
        const char *objects = argv[2];
        char hex[GIT_HEX];

        if (strcmp(what, "--hash-object") == 0 && argc >= 4) {
            long n = slurp(argv[3], buffer, sizeof buffer);
            if (n < 0 || git_write(objects, GIT_BLOB, buffer, (size_t)n,
                                   hex) != 0) {
                return 1;
            }
            printf("%s\n", hex);
            return 0;
        }
        if (strcmp(what, "--write-tree") == 0 && argc >= 4) {
            if (!git_write_tree(objects, argv[3], hex, buffer,
                                sizeof buffer)) {
                return 1;
            }
            printf("%s\n", hex);
            return 0;
        }
        if (strcmp(what, "--commit") == 0 && argc >= 6) {
            struct git_commit c;
            memset(&c, 0, sizeof c);
            snprintf(c.tree, sizeof c.tree, "%s", argv[3]);
            if (strcmp(argv[4], "-") != 0) {
                snprintf(c.parent, sizeof c.parent, "%s", argv[4]);
            }
            c.who = "velvet <velvet@example.invalid>";
            c.when = 1700000000;
            c.offset = "+0000";
            c.message = argv[5];
            long n = git_commit_encode(&c, buffer, sizeof buffer);
            if (n < 0 || git_write(objects, GIT_COMMIT, buffer, (size_t)n,
                                   hex) != 0) {
                return 1;
            }
            printf("%s\n", hex);
            return 0;
        }
        if (strcmp(what, "--cat-file") == 0 && argc >= 4) {
            enum git_type type;
            long n = git_read(objects, argv[3], &type, buffer, sizeof buffer);
            if (n < 0) {
                return 1;
            }
            fwrite(buffer, 1, (size_t)n, stdout);
            return 0;
        }
        if (strcmp(what, "--type") == 0 && argc >= 4) {
            enum git_type type;
            if (git_read(objects, argv[3], &type, buffer, sizeof buffer) < 0) {
                return 1;
            }
            printf("%s\n", type == GIT_BLOB ? "blob"
                         : type == GIT_TREE ? "tree" : "commit");
            return 0;
        }
        fprintf(stderr, "git: unknown %s\n", what);
        return 2;
    }

    test_names();
    test_tree_order();
    test_round_trip();

    if (failures > 0) {
        printf("  %d failed\n", failures);
        return 1;
    }
    return 0;
}
