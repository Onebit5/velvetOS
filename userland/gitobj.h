// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/gitobj.h
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * a git object store.
 */

#ifndef USER_GITOBJ_H
#define USER_GITOBJ_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * a git object store. an object's *name is its hash*: not a name with a
 * hash attached, the sha-1 of the content is the only name it has and where
 * the file goes is worked out from that name. so two identical files are
 * one object, a file cannot be edited without becoming a different object,
 * and checking is rehashing. history falls out of it: not a list of diffs
 * but a chain of complete snapshots that share what they have in common,
 * automatically, because equal content has equal names.
 *
 * every object is `<type> <length>\0` and then content, and the header is
 * hashed with it, which is why a git object's name is not the sha-1 of the
 * file. a **blob** is the bytes of a file with no name and no mode, which
 * is what lets the same content appear under two names for free. a **tree**
 * is a directory, a list of `<mode> <name>\0<20 raw bytes>` where the
 * twenty bytes name another object, and only four modes matter: 100644 a
 * file, 100755 an executable, 120000 a symlink, 40000 a directory, written
 * without a leading zero. a **commit** is a tree, some parents, who and
 * when, and a message, in plain text.
 *
 * a store only this machine can read would be a store that agrees with
 * itself, so the check is that real git reads what this writes, `git fsck`,
 * `git log`, `git cat-file`, and that git's tree hash and this one's are
 * the same forty characters for the same directory
 */

#define GIT_HEX 41              /* forty characters and a terminator */

enum git_type { GIT_BLOB, GIT_TREE, GIT_COMMIT };

#define GIT_MODE_FILE   0100644
#define GIT_MODE_EXEC   0100755
#define GIT_MODE_LINK   0120000
#define GIT_MODE_TREE   0040000

/* what this object would be called, without writing anything */
void git_name(enum git_type type, const void *data, size_t len,
              char hex[GIT_HEX]);

/*
 * write it into `objects`, the `.git/objects` directory, where git
 * would put it: the first two characters of the name are the directory
 * and the other thirty-eight are the file. compressed, because that is
 * what a loose object is.
 *
 * writing an object that is already there is not an error and does not
 * rewrite it. it cannot differ: the name came from the content
 */
int git_write(const char *objects, enum git_type type, const void *data,
              size_t len, char hex[GIT_HEX]);

/* and back again. returns the length, or negative. */
long git_read(const char *objects, const char *hex, enum git_type *type,
              void *out, size_t cap);

/* the second thing that catches people out is the order. */

struct git_entry {
    unsigned mode;
    char     name[256];
    uint8_t  sha[20];
};

/* encode the entries as a tree object. */
long git_tree_encode(struct git_entry *entry, int count, void *out,
                     size_t cap);

/* read one back. returns how many entries there were, or negative */
int git_tree_decode(const void *data, size_t len, struct git_entry *out,
                    int max);



struct git_commit {
    char        tree[GIT_HEX];
    char        parent[GIT_HEX];    /* empty for the first commit */
    const char *who;                /* "Name <email>" */
    long        when;               /* seconds since the epoch */
    const char *offset;             /* "+0000" */
    const char *message;
};

long git_commit_encode(const struct git_commit *c, void *out, size_t cap);

/*
 * what a commit says, without a parser worth the name: the tree, the
 * first parent, and where the message begins. that is everything `log`
 * needs and everything this stores
 */
bool git_commit_decode(const void *data, size_t len, char tree[GIT_HEX],
                       char parent[GIT_HEX], const char **message);

/* every file becomes a blob, every subdirectory a tree, and this tree names them. */
bool git_write_tree(const char *objects, const char *dir, char hex[GIT_HEX],
                    void *scratch, size_t scratch_cap);

#endif
