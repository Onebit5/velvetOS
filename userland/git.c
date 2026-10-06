// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/git.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * git, inside the machine.
 */

#include "syscall.h"
#include "args.h"
#include "gitobj.h"
#include "gitio.h"
#include "lib/epoch.h"
#include "difflib.h"

static const struct program git_prog = {
    .name = "git",
    .usage = "git init | hash-object <file> | cat-file <sha> | write-tree | "
             "commit <message> | log | diff | branch [name] | checkout <what>",
    .summary = "a git repository, in the machine that made it",
};

#define GIT_DIR      ".git"
#define OBJECTS_DIR  ".git/objects"
#define HEAD_PATH    ".git/HEAD"
#define HEADS_DIR    ".git/refs/heads"

static char scratch[1 << 20];

/* joining a path, again. */
static unsigned long put_path(char *out, unsigned long at, unsigned long cap,
                              const char *s)
{
    while (*s != '\0' && at + 1 < cap) {
        out[at++] = *s++;
    }
    return at;
}

static void say(const char *s)
{
    write(s);
}

static void say_line(const char *s)
{
    write(s);
    write("\n");
}

/* the branch HEAD names, and what it points at. */
/* which branch HEAD names. */
static void current_branch(char out[128])
{
    char buf[256];
    long n = gio_read(HEAD_PATH, buf, sizeof buf - 1);
    int at = 0;
    if (n > 16) {
        buf[n] = '\0';
        const char *p = buf;
        while (*p != '\0' && *p != '/') { p++; }       /* past refs */
        if (*p == '/') { p++; }
        while (*p != '\0' && *p != '/') { p++; }       /* past heads */
        if (*p == '/') { p++; }
        while (*p != '\0' && *p != '\n' && at < 127) {
            out[at++] = *p++;
        }
    }
    if (at == 0) {
        /*
         * FIXME: a detached head lands here. HEAD holding a bare commit
         * has no slash to walk past, so this answers "master", and the
         * next `git commit` then writes refs/heads/master, moving a
         * branch the user was told, one command earlier, that they had
         * left. do_checkout prints "commits made here belong to no
         * branch", and this is the line that makes that untrue. the
         * caller needs the difference between a ref and a commit, not a
         * branch name either way.
         */
        const char *fallback = "master";
        while (*fallback != '\0') { out[at++] = *fallback++; }
    }
    out[at] = '\0';
}

static void branch_path(char out[256], const char *name)
{
    unsigned long at = put_path(out, 0, 256, HEADS_DIR);
    at = put_path(out, at, 256, "/");
    at = put_path(out, at, 256, name);
    out[at] = '\0';
}

static bool read_branch(const char *name, char out[GIT_HEX])
{
    char path[256];
    branch_path(path, name);
    char buf[256];
    long n = gio_read(path, buf, sizeof buf - 1);
    if (n < 40) {
        return false;
    }
    for (int i = 0; i < 40; i++) {
        out[i] = buf[i];
    }
    out[40] = '\0';
    return true;
}

static bool read_ref(char out[GIT_HEX])
{
    char branch[128];
    current_branch(branch);
    return read_branch(branch, out);
}

static bool write_branch(const char *name, const char *hex)
{
    char path[256];
    branch_path(path, name);
    char line[GIT_HEX + 1];
    int i = 0;
    while (hex[i] != '\0' && i < GIT_HEX - 1) {
        line[i] = hex[i];
        i++;
    }
    line[i++] = '\n';
    return gio_write(path, line, (unsigned long)i) == 0;
}

static bool write_ref(const char *hex)
{
    char branch[128];
    current_branch(branch);
    return write_branch(branch, hex);
}

static int do_init(void)
{
    gio_mkdir(GIT_DIR);
    gio_mkdir(OBJECTS_DIR);
    gio_mkdir(".git/refs");
    gio_mkdir(HEADS_DIR);
    /*
     * HEAD is a ref to a ref, which is what makes switching branches a
     * one-line write rather than a checkout of anything
     */
    const char *head = "ref: refs/heads/master\n";
    unsigned long n = 0;
    while (head[n] != '\0') {
        n++;
    }
    if (gio_write(HEAD_PATH, head, n) != 0) {
        say_line("git: cannot write .git/HEAD, is this a writable disk?");
        return 1;
    }
    say_line("an empty repository, in .git");
    return 0;
}

static int do_hash_object(const char *path, bool store)
{
    long n = gio_read(path, scratch, sizeof scratch);
    if (n < 0) {
        say("git: cannot read ");
        say_line(path);
        return 1;
    }
    char hex[GIT_HEX];
    if (store) {
        if (git_write(OBJECTS_DIR, GIT_BLOB, scratch, (unsigned long)n,
                      hex) != 0) {
            say_line("git: cannot write the object");
            return 1;
        }
    } else {
        git_name(GIT_BLOB, scratch, (unsigned long)n, hex);
    }
    say_line(hex);
    return 0;
}

static int do_cat_file(const char *sha)
{
    enum git_type type;
    long n = git_read(OBJECTS_DIR, sha, &type, scratch, sizeof scratch);
    if (n < 0) {
        say_line("git: no such object");
        return 1;
    }
    if (type == GIT_TREE) {
        /*
         * a tree is binary, so it gets printed rather than dumped,
         * the same shape `git cat-file -p` prints, so the two can be
         * put side by side
         */
        struct git_entry entry[128];
        int count = git_tree_decode(scratch, (unsigned long)n, entry, 128);
        for (int i = 0; i < count; i++) {
            char hex[GIT_HEX];
            static const char digit[] = "0123456789abcdef";
            for (int k = 0; k < 20; k++) {
                hex[k * 2] = digit[entry[i].sha[k] >> 4];
                hex[k * 2 + 1] = digit[entry[i].sha[k] & 15];
            }
            hex[40] = '\0';
            say(entry[i].mode == GIT_MODE_TREE ? "tree " : "blob ");
            say(hex);
            say("  ");
            say_line(entry[i].name);
        }
        return 0;
    }
    write_fd(STDOUT, scratch, n);
    return 0;
}

static int do_commit(const char *message)
{
    char tree[GIT_HEX];
    if (!git_write_tree(OBJECTS_DIR, ".", tree, scratch, sizeof scratch)) {
        say_line("git: cannot write the tree");
        return 1;
    }

    struct git_commit c;
    for (unsigned long i = 0; i < sizeof c; i++) {
        ((char *)&c)[i] = 0;
    }
    for (int i = 0; i < GIT_HEX; i++) {
        c.tree[i] = tree[i];
    }
    read_ref(c.parent);         /* empty on the first one, which is fine */

    c.who = "velvet <velvet@velvetos>";
    /*
     * when. there is no clock syscall, and there is a real clock: the
     * file the project just wrote has a timestamp on it, so the date comes back
     * out of the filesystem and through the same epoch conversion the
     * kernel's own is checked against
     */
    c.when = 0;
    struct stat st;
    if (stat(HEAD_PATH, &st) == 0 && st.year > 1970) {
        struct rtc_time t;
        t.year = st.year;
        t.month = st.month;
        t.day = st.day;
        t.hour = st.hour;
        t.minute = st.minute;
        t.second = st.second;
        c.when = (long)epoch_from_date(&t);
    }
    c.offset = "+0000";
    c.message = message;

    long n = git_commit_encode(&c, scratch, sizeof scratch);
    char hex[GIT_HEX];
    if (n < 0 || git_write(OBJECTS_DIR, GIT_COMMIT, scratch,
                           (unsigned long)n, hex) != 0) {
        say_line("git: cannot write the commit");
        return 1;
    }
    if (!write_ref(hex)) {
        say_line("git: cannot move the branch");
        return 1;
    }
    say(c.parent[0] == '\0' ? "the first commit: " : "committed: ");
    say_line(hex);
    say("tree ");
    say_line(tree);
    return 0;
}

static int do_log(void)
{
    char at[GIT_HEX];
    if (!read_ref(at)) {
        say_line("git: no commits yet");
        return 1;
    }

    /*
     * walk the parents. a commit names the one before it, so history is
     * a chain that is read by following it, there is no list of
     * commits anywhere, and nothing had to be updated when this one was
     * made except the single file the branch is
     */
    for (int guard = 0; guard < 1000; guard++) {
        enum git_type type;
        long n = git_read(OBJECTS_DIR, at, &type, scratch, sizeof scratch);
        if (n < 0 || type != GIT_COMMIT) {
            say_line("git: the history stops at an object that is not there");
            return 1;
        }
        char tree[GIT_HEX], parent[GIT_HEX];
        const char *message = 0;
        if (!git_commit_decode(scratch, (unsigned long)n, tree, parent,
                               &message)) {
            say_line("git: a commit i cannot read");
            return 1;
        }
        say("commit ");
        say_line(at);
        say("    ");
        if (message != 0) {
            const char *p = message;
            while (*p != '\0' && *p != '\n') {
                char one[2] = { *p++, '\0' };
                say(one);
            }
        }
        say("\n\n");
        if (parent[0] == '\0') {
            return 0;
        }
        for (int i = 0; i < GIT_HEX; i++) {
            at[i] = parent[i];
        }
    }
    return 0;
}


/*
 * a commit's tree covers everything underneath it, so "what changed"
 * is answered by walking the tree and comparing each blob against the
 * file of the same name, and by noticing anything on disk the tree
 * does not mention.
 *
 * no index, so there is nothing in between: the comparison is always
 * the commit against what is actually there. that is the whole benefit
 * of not having one, and the whole cost is that `git status` cannot be
 * a cheap answer, it has to read every file.
 */

static char blob_buf[1 << 18];
static char file_buf[1 << 18];

/*
 * the lines of a buffer, in place, as difflib wants them: pointers and
 * lengths, with nothing copied and no terminator written
 */
static size_t line_up(char *text, long len, const char **out, size_t *lens,
                      size_t max)
{
    size_t count = 0;
    long at = 0;
    while (at < len && count < max) {
        long start = at;
        while (at < len && text[at] != '\n') {
            at++;
        }
        out[count] = text + start;
        lens[count] = (size_t)(at - start);
        count++;
        if (at < len) {
            at++;               /* the newline itself belongs to neither */
        }
    }
    return count;
}

static void print_line(char lead, const char *text, size_t len)
{
    char one[2] = { lead, '\0' };
    say(one);
    write_fd(STDOUT, text, (long)len);
    say("\n");
}

/* one file against one blob. */
static void diff_one(const char *path, const char *a, long alen,
                     char *b, long blen)
{
    static const char *atext[DIFF_MAX_LINES], *btext[DIFF_MAX_LINES];
    static size_t alens[DIFF_MAX_LINES], blens[DIFF_MAX_LINES];
    static struct diff_edit edits[DIFF_MAX_LINES * 2];

    struct diff_lines A, B;
    A.text = atext; A.len = alens;
    A.count = line_up((char *)a, alen, atext, alens, DIFF_MAX_LINES);
    B.text = btext; B.len = blens;
    B.count = line_up(b, blen, btext, blens, DIFF_MAX_LINES);

    if (diff_same(&A, &B)) {
        return;
    }

    say("--- a/");
    say_line(path);
    say("+++ b/");
    say_line(path);

    size_t n = 0;
    if (!diff_compare(&A, &B, edits, sizeof edits / sizeof edits[0], &n)) {
        say_line("    (too large to compare)");
        return;
    }
    for (size_t i = 0; i < n; i++) {
        if (edits[i].op == DIFF_REMOVED) {
            print_line('-', atext[edits[i].line], alens[edits[i].line]);
        } else if (edits[i].op == DIFF_ADDED) {
            print_line('+', btext[edits[i].line], blens[edits[i].line]);
        }
    }
}

/* walk a tree against a directory. */
static void walk(const char *tree_hex, const char *dir, bool print,
                 int *changed)
{
    enum git_type type;
    static char tree_data[1 << 16];
    long n = git_read(OBJECTS_DIR, tree_hex, &type, tree_data,
                      sizeof tree_data);
    if (n < 0 || type != GIT_TREE) {
        return;
    }

    struct git_entry entry[128];
    int count = git_tree_decode(tree_data, (unsigned long)n, entry, 128);
    static const char digit[] = "0123456789abcdef";

    for (int i = 0; i < count; i++) {
        char path[512];
        unsigned long at = 0;
        if (dir[0] != '\0') {
            at = put_path(path, 0, sizeof path, dir);
            at = put_path(path, at, sizeof path, "/");
        }
        at = put_path(path, at, sizeof path, entry[i].name);
        path[at] = '\0';

        char hex[GIT_HEX];
        for (int k = 0; k < 20; k++) {
            hex[k * 2] = digit[entry[i].sha[k] >> 4];
            hex[k * 2 + 1] = digit[entry[i].sha[k] & 15];
        }
        hex[40] = '\0';

        if (entry[i].mode == GIT_MODE_TREE) {
            walk(hex, path, print, changed);
            continue;
        }

        long blen = gio_read(path, file_buf, sizeof file_buf);
        if (blen < 0) {
            (*changed)++;
            if (print) {
                say("deleted: ");
                say_line(path);
            }
            continue;
        }
        long alen = git_read(OBJECTS_DIR, hex, &type, blob_buf,
                             sizeof blob_buf);
        if (alen < 0) {
            continue;
        }
        if (alen == blen) {
            bool same = true;
            for (long k = 0; k < alen && same; k++) {
                same = blob_buf[k] == file_buf[k];
            }
            if (same) {
                continue;
            }
        }
        (*changed)++;
        if (print) {
            diff_one(path, blob_buf, alen, file_buf, blen);
        }
    }
}

static int do_diff(void)
{
    char head[GIT_HEX];
    if (!read_ref(head)) {
        say_line("git: no commits yet, so there is nothing to differ from");
        return 1;
    }
    enum git_type type;
    long n = git_read(OBJECTS_DIR, head, &type, scratch, sizeof scratch);
    char tree[GIT_HEX], parent[GIT_HEX];
    const char *message = 0;
    if (n < 0 || !git_commit_decode(scratch, (unsigned long)n, tree, parent,
                                    &message)) {
        say_line("git: cannot read the commit HEAD names");
        return 1;
    }
    int changed = 0;
    walk(tree, "", true, &changed);
    if (changed == 0) {
        say_line("nothing changed");
    }
    return 0;
}

/*
 * the one command here that writes to the working tree, which makes it
 * the first thing in this project that can destroy something a person
 * typed. so it refuses when there is anything to destroy, and says
 * what: a checkout that quietly overwrites an unrecorded change is a
 * tool nobody can trust twice.
 */

static bool restore(const char *tree_hex, const char *dir)
{
    enum git_type type;
    static char tree_data[1 << 16];
    long n = git_read(OBJECTS_DIR, tree_hex, &type, tree_data,
                      sizeof tree_data);
    if (n < 0 || type != GIT_TREE) {
        return false;
    }

    struct git_entry entry[128];
    int count = git_tree_decode(tree_data, (unsigned long)n, entry, 128);
    static const char digit[] = "0123456789abcdef";

    for (int i = 0; i < count; i++) {
        char path[512];
        unsigned long at = 0;
        if (dir[0] != '\0') {
            at = put_path(path, 0, sizeof path, dir);
            at = put_path(path, at, sizeof path, "/");
        }
        at = put_path(path, at, sizeof path, entry[i].name);
        path[at] = '\0';

        char hex[GIT_HEX];
        for (int k = 0; k < 20; k++) {
            hex[k * 2] = digit[entry[i].sha[k] >> 4];
            hex[k * 2 + 1] = digit[entry[i].sha[k] & 15];
        }
        hex[40] = '\0';

        if (entry[i].mode == GIT_MODE_TREE) {
            gio_mkdir(path);
            if (!restore(hex, path)) {
                return false;
            }
            continue;
        }
        long len = git_read(OBJECTS_DIR, hex, &type, blob_buf,
                            sizeof blob_buf);
        if (len < 0 || gio_write(path, blob_buf, (unsigned long)len) != 0) {
            say("git: cannot write ");
            say_line(path);
            return false;
        }
    }
    return true;
}

static int do_checkout(const char *what)
{
    /*
     * a name or a commit. a branch is tried first, because a forty
     * character branch name is not a thing anybody has
     */
    char target[GIT_HEX];
    bool is_branch = read_branch(what, target);
    if (!is_branch) {
        int len = 0;
        while (what[len] != '\0') {
            len++;
        }
        if (len != 40) {
            say("git: no branch or commit called ");
            say_line(what);
            return 1;
        }
        for (int i = 0; i < 41; i++) {
            target[i] = what[i];
        }
    }

    /*
     * what would be lost. this is the diff walk with its printing
     * turned off, which is why the two can never disagree about what
     * counts as a change
     */
    char head[GIT_HEX];
    if (read_ref(head)) {
        enum git_type type;
        long n = git_read(OBJECTS_DIR, head, &type, scratch, sizeof scratch);
        char tree[GIT_HEX], parent[GIT_HEX];
        const char *message = 0;
        if (n >= 0 && git_commit_decode(scratch, (unsigned long)n, tree,
                                        parent, &message)) {
            int changed = 0;
            walk(tree, "", false, &changed);
            if (changed > 0) {
                say_line("git: the working tree has changes that are not "
                         "committed.");
                say_line("     `git diff` will show them. commit them, or "
                         "undo them, first.");
                return 1;
            }
        }
    }

    enum git_type type;
    long n = git_read(OBJECTS_DIR, target, &type, scratch, sizeof scratch);
    char tree[GIT_HEX], parent[GIT_HEX];
    const char *message = 0;
    if (n < 0 || type != GIT_COMMIT
        || !git_commit_decode(scratch, (unsigned long)n, tree, parent,
                              &message)) {
        say_line("git: that is not a commit");
        return 1;
    }
    if (!restore(tree, "")) {
        return 1;
    }

    /*
     * and HEAD follows. onto the branch if it was one, so the next
     * commit continues it, and onto the commit itself if it was not,
     * which every git calls a detached head and this one does too
     */
    if (is_branch) {
        char line[256];
        unsigned long at = put_path(line, 0, sizeof line, "ref: refs/heads/");
        at = put_path(line, at, sizeof line, what);
        line[at++] = '\n';
        gio_write(HEAD_PATH, line, at);
        say("now on ");
        say_line(what);
    } else {
        say("now at ");
        say_line(target);
        say_line("(a detached head: commits made here belong to no branch)");
    }
    return 0;
}

static int do_branch(const char *name)
{
    char here[128];
    current_branch(here);

    if (name == 0) {
        for (long i = 0; ; i++) {
            char found[256];
            bool is_dir = false, is_exec = false;
            if (!gio_entry(HEADS_DIR, i, found, sizeof found, &is_dir,
                           &is_exec)) {
                break;
            }
            say(ustrcmp(found, here) == 0 ? "* " : "  ");
            say_line(found);
        }
        return 0;
    }

    char at[GIT_HEX];
    if (!read_ref(at)) {
        say_line("git: no commits yet, so there is nothing to branch from");
        return 1;
    }
    /* a branch is one file with one name in it. */
    if (!write_branch(name, at)) {
        say_line("git: cannot write the branch");
        return 1;
    }
    say("branch ");
    say(name);
    say(" at ");
    say_line(at);
    return 0;
}

void _start(int argc, char **argv)
{
    struct args a;
    const char *error;
    if (!args_parse(&git_prog, argc, argv, &a, &error)) {
        say("git: ");
        say_line(error);
        exit(1);
    }
    if (a.wants_help || a.count == 0) {
        args_usage(&git_prog);
        exit(a.count == 0 ? 1 : 0);
    }

    const char *what = a.rest[0];
    int bad = 0;

    if (ustrcmp(what, "init") == 0) {
        bad = do_init();
    } else if (!gio_exists(GIT_DIR)) {
        say_line("git: not a repository, run `git init` first");
        bad = 1;
    } else if (ustrcmp(what, "hash-object") == 0 && a.count >= 2) {
        bad = do_hash_object(a.rest[1], true);
    } else if (ustrcmp(what, "cat-file") == 0 && a.count >= 2) {
        bad = do_cat_file(a.rest[1]);
    } else if (ustrcmp(what, "write-tree") == 0) {
        char hex[GIT_HEX];
        if (!git_write_tree(OBJECTS_DIR, ".", hex, scratch, sizeof scratch)) {
            say_line("git: cannot write the tree");
            bad = 1;
        } else {
            say_line(hex);
        }
    } else if (ustrcmp(what, "commit") == 0 && a.count >= 2) {
        bad = do_commit(a.rest[1]);
    } else if (ustrcmp(what, "log") == 0) {
        bad = do_log();
    } else if (ustrcmp(what, "diff") == 0) {
        bad = do_diff();
    } else if (ustrcmp(what, "checkout") == 0 && a.count >= 2) {
        bad = do_checkout(a.rest[1]);
    } else if (ustrcmp(what, "branch") == 0) {
        bad = do_branch(a.count >= 2 ? a.rest[1] : 0);
    } else {
        args_usage(&git_prog);
        bad = 1;
    }
    exit(bad);
}
