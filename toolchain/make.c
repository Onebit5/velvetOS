// SPDX-License-Identifier: GPL-2.0-only
/*
 * toolchain/make.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * make.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>

/* the sizes, and where they came from. */
#define MAX_VARS   512
#define MAX_RULES  2048
#define MAX_LIST   256
#define MAX_RECIPE 64
#define MAX_NODES  4096
#define MAX_LINE   8192
#define MAX_DEPTH  64

/* the name the project call ourselves in messages. */
static char program[64] = "make";
static const char *makefile_name = "Makefile";
static bool dry_run;    /* -n: say what would happen, do none of it */
static bool silent;     /* -s: run it all without the echo */

static void oom(void)
{
    fprintf(stderr, "%s: out of memory\n", program);
    exit(2);
}

static char *copy(const char *s)
{
    char *p = strdup(s);
    if (p == NULL) {
        oom();
    }
    return p;
}



struct buf { char *p; size_t len, cap; };

static void put(struct buf *b, const char *s, size_t n)
{
    if (b->len + n + 1 > b->cap) {
        b->cap = (b->len + n + 1) * 2;
        b->p = realloc(b->p, b->cap);
        if (b->p == NULL) {
            oom();
        }
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static void puts_(struct buf *b, const char *s)
{
    put(b, s, strlen(s));
}
static void putc_(struct buf *b, char c)
{
    put(b, &c, 1);
}

static char *taken(struct buf *b)
{
    return b->p != NULL ? b->p : copy("");
}



static bool space(char c)
{
    return c == ' ' || c == '\t' || c == '\n';
}

/* split on whitespace, in place, into a caller's array. */
static int words(char *s, char **out, int max)
{
    int n = 0;
    while (*s != '\0' && n < max) {
        while (space(*s)) {
            s++;
        }
        if (*s == '\0') {
            break;
        }
        out[n++] = s;
        while (*s != '\0' && !space(*s)) {
            s++;
        }
        if (*s != '\0') {
            *s++ = '\0';
        }
    }
    return n;
}



/*
 * a variable holds text, and *when* that text is expanded is the whole
 * difference between the two kinds of assignment:
 *
 *   V  = $(W)    recursive: the text is kept, and expanded every time V
 *                is used, so V sees whatever W means by then, even if
 *                W is defined further down the file
 *   V := $(W)    simple: the text is expanded now, once, and V is that
 *                answer forever
 *
 * which is not a detail. a makefile that sets CFLAGS with = and adds to
 * it later works; one that used := gets whatever CFLAGS was on that
 * line and nothing after it.
 */
struct var {
    char  name[128];
    char *value;
    bool  simple;
};
static struct var var[MAX_VARS];
static int vars;

static struct var *find_var(const char *name)
{
    for (int i = 0; i < vars; i++) {
        if (strcmp(var[i].name, name) == 0) {
            return &var[i];
        }
    }
    return NULL;
}

static void set_var(const char *name, char *value, bool simple)
{
    struct var *v = find_var(name);
    if (v == NULL) {
        if (vars >= MAX_VARS) {
            fprintf(stderr, "%s: too many variables\n", program);
            exit(2);
        }
        v = &var[vars++];
        snprintf(v->name, sizeof v->name, "%s", name);
    }
    v->value = value;
    v->simple = simple;
}



/* what the automatic variables mean at this moment. */
struct context {
    const char *target;
    char      **prereq;
    int         prereqs;
    const char *stem;
};

static char *expand(const char *s, const struct context *ctx);

static bool pattern_match(const char *pattern, const char *name, char *stem,
                          size_t stem_size);

/* `*.c`, which is a different thing from `%.c` and it is worth being clear about why. */
static bool glob_match(const char *pat, const char *s)
{
    if (*pat == '\0') {
        return *s == '\0';
    }
    if (*pat == '*') {
        for (const char *t = s; ; t++) {
            if (glob_match(pat + 1, t)) {
                return true;
            }
            if (*t == '\0') {
                return false;
            }
        }
    }
    if (*s == '\0') {
        return false;
    }
    if (*pat == '?' || *pat == *s) {
        return glob_match(pat + 1, s + 1);
    }
    return false;
}

/* $(subst from,to,text) and friends. */
static char *call(char *name, char **arg, int args, const struct context *ctx)
{
    struct buf b = { 0 };
    (void)ctx;

    if (strcmp(name, "subst") == 0 && args == 3) {
        const char *from = arg[0], *to = arg[1], *s = arg[2];
        size_t n = strlen(from);
        if (n == 0) {
            return copy(s);
        }
        while (*s != '\0') {
            if (strncmp(s, from, n) == 0) {
                puts_(&b, to);
                s += n;
            } else {
                putc_(&b, *s++);
            }
        }
        return taken(&b);
    }

    if (strcmp(name, "patsubst") == 0 && args == 3) {
        char *list[MAX_LIST];
        char *text = copy(arg[2]);
        int n = words(text, list, MAX_LIST);
        for (int i = 0; i < n; i++) {
            char stem[512];
            if (i > 0) {
                putc_(&b, ' ');
            }
            if (pattern_match(arg[0], list[i], stem, sizeof stem)) {
                for (const char *p = arg[1]; *p != '\0'; p++) {
                    if (*p == '%') {
                        puts_(&b, stem);
                    } else {
                        putc_(&b, *p);
                    }
                }
            } else {
                puts_(&b, list[i]);
            }
        }
        return taken(&b);
    }

    if ((strcmp(name, "filter") == 0 || strcmp(name, "filter-out") == 0) &&
        args == 2) {
        bool keep = strcmp(name, "filter") == 0;
        char *pats[MAX_LIST], *list[MAX_LIST];
        char *a = copy(arg[0]), *c = copy(arg[1]);
        int np = words(a, pats, MAX_LIST), n = words(c, list, MAX_LIST);
        int put_any = 0;
        for (int i = 0; i < n; i++) {
            bool hit = false;
            for (int j = 0; j < np && !hit; j++) {
                char stem[512];
                hit = pattern_match(pats[j], list[i], stem, sizeof stem);
            }
            if (hit == keep) {
                if (put_any++) {
                    putc_(&b, ' ');
                }
                puts_(&b, list[i]);
            }
        }
        return taken(&b);
    }

    if (strcmp(name, "strip") == 0 && args == 1) {
        char *list[MAX_LIST];
        char *text = copy(arg[0]);
        int n = words(text, list, MAX_LIST);
        for (int i = 0; i < n; i++) {
            if (i > 0) {
                putc_(&b, ' ');
            }
            puts_(&b, list[i]);
        }
        return taken(&b);
    }

    if ((strcmp(name, "notdir") == 0 || strcmp(name, "dir") == 0 ||
         strcmp(name, "basename") == 0 || strcmp(name, "suffix") == 0) &&
        args == 1) {
        char *list[MAX_LIST];
        char *text = copy(arg[0]);
        int n = words(text, list, MAX_LIST);
        for (int i = 0; i < n; i++) {
            char *slash = strrchr(list[i], '/');
            if (i > 0) {
                putc_(&b, ' ');
            }
            if (strcmp(name, "notdir") == 0) {
                puts_(&b, slash != NULL ? slash + 1 : list[i]);
            } else if (strcmp(name, "dir") == 0) {
                if (slash != NULL) {
                    put(&b, list[i], (size_t)(slash - list[i]) + 1);
                } else {
                    puts_(&b, "./");
                }
            } else {
                char *dot = strrchr(slash != NULL ? slash : list[i], '.');
                if (strcmp(name, "basename") == 0) {
                    put(&b, list[i], dot != NULL ? (size_t)(dot - list[i])
                                                 : strlen(list[i]));
                } else if (dot != NULL) {
                    puts_(&b, dot);
                }
            }
        }
        return taken(&b);
    }

    if ((strcmp(name, "addprefix") == 0 || strcmp(name, "addsuffix") == 0) &&
        args == 2) {
        char *list[MAX_LIST];
        char *text = copy(arg[1]);
        int n = words(text, list, MAX_LIST);
        for (int i = 0; i < n; i++) {
            if (i > 0) {
                putc_(&b, ' ');
            }
            if (strcmp(name, "addprefix") == 0) {
                puts_(&b, arg[0]);
                puts_(&b, list[i]);
            } else {
                puts_(&b, list[i]);
                puts_(&b, arg[0]);
            }
        }
        return taken(&b);
    }

    if (strcmp(name, "firstword") == 0 && args == 1) {
        char *list[MAX_LIST];
        char *text = copy(arg[0]);
        int n = words(text, list, MAX_LIST);
        return copy(n > 0 ? list[0] : "");
    }

    if (strcmp(name, "words") == 0 && args == 1) {
        char *list[MAX_LIST];
        char *text = copy(arg[0]);
        char n[16];
        snprintf(n, sizeof n, "%d", words(text, list, MAX_LIST));
        return copy(n);
    }

    /* $(wildcard *.c), the one function that asks the disk a question. */
    if (strcmp(name, "wildcard") == 0 && args == 1) {
        char *pats[MAX_LIST];
        char *text = copy(arg[0]);
        int np = words(text, pats, MAX_LIST);
        char *hit[MAX_LIST];
        int hits = 0;

        for (int i = 0; i < np; i++) {
            char *slash = strrchr(pats[i], '/');
            char dir[512] = ".";
            const char *leaf = pats[i];
            if (slash != NULL) {
                snprintf(dir, sizeof dir, "%.*s",
                         (int)(slash - pats[i]), pats[i]);
                leaf = slash + 1;
            }
            DIR *d = opendir(dir);
            if (d == NULL) {
                continue;
            }
            struct dirent *e;
            while ((e = readdir(d)) != NULL && hits < MAX_LIST) {
                if (e->d_name[0] == '.' && leaf[0] != '.') {
                    continue;
                }
                if (!glob_match(leaf, e->d_name)) {
                    continue;
                }
                char full[1024];
                if (slash != NULL) {
                    snprintf(full, sizeof full, "%s/%s", dir, e->d_name);
                } else {
                    snprintf(full, sizeof full, "%s", e->d_name);
                }
                hit[hits++] = copy(full);
            }
            closedir(d);
        }
        for (int i = 1; i < hits; i++) {
            char *k = hit[i];
            int j = i - 1;
            while (j >= 0 && strcmp(hit[j], k) > 0) {
                hit[j + 1] = hit[j];
                j--;
            }
            hit[j + 1] = k;
        }
        for (int i = 0; i < hits; i++) {
            if (i > 0) {
                putc_(&b, ' ');
            }
            puts_(&b, hit[i]);
        }
        return taken(&b);
    }

    /* the one function that is not about text at all. */
    if (strcmp(name, "shell") == 0 && args >= 1) {
        struct buf line = { 0 };
        for (int i = 0; i < args; i++) {
            if (i > 0) { putc_(&line, ','); }
            puts_(&line, arg[i]);
        }
        char *command = taken(&line);

        FILE *p = popen(command, "r");
        if (p == NULL) {
            free(command);
            return taken(&b);
        }
        int c;
        while ((c = fgetc(p)) != EOF) {
            putc_(&b, (char)c);
        }
        pclose(p);
        free(command);

        while (b.len > 0 && b.p[b.len - 1] == '\n') {
            b.p[--b.len] = '\0';
        }
        for (size_t i = 0; i < b.len; i++) {
            if (b.p[i] == '\n') {
                b.p[i] = ' ';
            }
        }
        return taken(&b);
    }

    fprintf(stderr, "%s: %s: i do not know the function `%s'\n",
            program, makefile_name, name);
    exit(2);
}

/*
 * the inside of a `$(...)`: a variable name, an automatic variable, or a
 * function and its arguments
 */
static char *reference(char *text, const struct context *ctx)
{
    /* a function is a name, a space, and the arguments */
    char *sp = strpbrk(text, " \t");
    if (sp != NULL) {
        char saved = *sp;
        *sp = '\0';
        static const char *known[] = {
            "subst", "patsubst", "strip", "filter", "filter-out",
            "notdir", "dir", "basename", "suffix", "addprefix",
            "addsuffix", "firstword", "words", "wildcard", "shell", NULL
        };
        bool is_function = false;
        for (int i = 0; known[i] != NULL; i++) {
            if (strcmp(text, known[i]) == 0) {
                is_function = true;
            }
        }
        if (is_function) {
            char *args[8];
            int n = 0;
            char *p = sp + 1;
            while (space(*p)) {
                p++;
            }
            args[n++] = p;
            int depth = 0;
            for (; *p != '\0'; p++) {
                if (*p == '(') {
                    depth++;
                } else if (*p == ')')
{
                    depth--;
                } else if (*p == ',' && depth == 0 && n < 8)
{
                    *p = '\0';
                    args[n++] = p + 1;
                }
            }
            return call(text, args, n, ctx);
        }
        *sp = saved;
    }

    /*
     * `$(@D)` and `$(<F)`: the directory or the file part of an
     * automatic variable, which is how a rule makes the directory its
     * own output goes in
     */
    size_t n = strlen(text);
    if (n == 2 && (text[1] == 'D' || text[1] == 'F') &&
        (text[0] == '@' || text[0] == '<')) {
        const char *whole = text[0] == '@' ? ctx->target
                          : (ctx->prereqs > 0 ? ctx->prereq[0] : "");
        if (whole == NULL) {
            whole = "";
        }
        const char *slash = strrchr(whole, '/');
        if (text[1] == 'F') {
            return copy(slash != NULL ? slash + 1 : whole);
        }
        if (slash == NULL) {
            return copy(".");
        }
        char dir[1024];
        snprintf(dir, sizeof dir, "%.*s", (int)(slash - whole), whole);
        return copy(dir);
    }

    if (n == 1 && ctx != NULL) {
        struct buf b = { 0 };
        switch (text[0]) {
        case '@':
            return copy(ctx->target != NULL ? ctx->target : "");
        case '*':
            return copy(ctx->stem != NULL ? ctx->stem : "");
        case '<':
            return copy(ctx->prereqs > 0 ? ctx->prereq[0] : "");
        case '^':
        case '+':
            /* `$^` drops repeats and `$+` keeps them. */
            for (int i = 0; i < ctx->prereqs; i++) {
                bool seen = false;
                for (int j = 0; text[0] == '^' && j < i; j++) {
                    seen = seen || strcmp(ctx->prereq[i], ctx->prereq[j]) == 0;
                }
                if (seen) {
                    continue;
                }
                if (b.len > 0) {
                    putc_(&b, ' ');
                }
                puts_(&b, ctx->prereq[i]);
            }
            return taken(&b);
        default:
            break;
        }
    }

    struct var *v = find_var(text);
    if (v == NULL) {
        return copy("");
    }
    if (v->simple) {
        return copy(v->value);
    }
    return expand(v->value, ctx);   /* recursive: expanded at every use */
}

static char *expand(const char *s, const struct context *ctx)
{
    static int depth;
    struct buf b = { 0 };

    if (++depth > MAX_DEPTH) {
        fprintf(stderr, "%s: %s: a variable refers to itself\n",
                program, makefile_name);
        exit(2);
    }

    while (*s != '\0') {
        if (*s != '$') {
            putc_(&b, *s++);
            continue;
        }
        s++;
        if (*s == '$') {                /* `$$` is one dollar, for the shell */
            putc_(&b, '$');
            s++;
            continue;
        }
        if (*s == '(' || *s == '{') {
            char open = *s, close = open == '(' ? ')' : '}';
            int level = 1;
            const char *start = ++s;
            while (*s != '\0' && level > 0) {
                if (*s == open) {
                    level++;
                } else if (*s == close) {
                    level--;
                }
                if (level > 0) {
                    s++;
                }
            }
            char inner[MAX_LINE];
            snprintf(inner, sizeof inner, "%.*s", (int)(s - start), start);
            if (*s == close) {
                s++;
            }
            /*
             * the inside is expanded first, so `$(subst a,b,$(V))` and
             * `$($(WHICH))` both work
             */
            char *done = expand(inner, ctx);
            char *value = reference(done, ctx);
            puts_(&b, value);
            free(done);
            free(value);
            continue;
        }
        if (*s != '\0') {               /* `$@`, and one-letter variables */
            char one[2] = { *s++, '\0' };
            char *value = reference(one, ctx);
            puts_(&b, value);
            free(value);
        }
    }
    depth--;
    return taken(&b);
}



struct rule {
    char *target[MAX_LIST];
    int   targets;
    char *prereq[MAX_LIST];
    int   prereqs;
    char *recipe[MAX_RECIPE];
    int   recipe_at[MAX_RECIPE];    /* the line it was written on */
    int   lines;
    bool  pattern;                  /* the target has a % in it */

    /* written with `::` rather than `:`. */
    bool  doubled;
};
static struct rule rule[MAX_RULES];
static int rules;

/* what a target is known to be. */
struct node {
    char   name[512];
    bool   phony;
    bool   visiting;
    bool   done;
    bool   updated;     /* a recipe of its own actually ran */
    bool   exists;
    struct timespec when;
};
static struct node node[MAX_NODES];
static int nodes;

static struct node *node_for(const char *name)
{
    for (int i = 0; i < nodes; i++) {
        if (strcmp(node[i].name, name) == 0) {
            return &node[i];
        }
    }
    if (nodes >= MAX_NODES) {
        fprintf(stderr, "%s: too many targets\n", program);
        exit(2);
    }
    struct node *n = &node[nodes++];
    snprintf(n->name, sizeof n->name, "%s", name);
    return n;
}

/* `%.o` against `main.o`: the stem is what the % stood for. */
static bool pattern_match(const char *pattern, const char *name, char *stem,
                          size_t stem_size)
{
    const char *pct = strchr(pattern, '%');
    if (pct == NULL) {
        stem[0] = '\0';
        return strcmp(pattern, name) == 0;
    }
    size_t head = (size_t)(pct - pattern);
    size_t tail = strlen(pct + 1);
    size_t n = strlen(name);
    if (n < head + tail || strncmp(name, pattern, head) != 0 ||
        strcmp(name + n - tail, pct + 1) != 0) {
        return false;
    }
    snprintf(stem, stem_size, "%.*s", (int)(n - head - tail), name + head);
    return true;
}

static bool has_target(struct rule *r, const char *name)
{
    for (int i = 0; i < r->targets; i++) {
        if (strcmp(r->target[i], name) == 0) {
            return true;
        }
    }
    return false;
}



static void add_to(char **list, int *count, char *text)
{
    char *w[MAX_LIST];
    int n = words(text, w, MAX_LIST);
    for (int i = 0; i < n && *count < MAX_LIST; i++) {
        list[(*count)++] = copy(w[i]);
    }
}

/* `ifdef`, `ifndef`, `ifeq`, `ifneq`, `else` and `endif`, kept as a stack because they nest. */
#define MAX_IFS 32
static struct { bool taking, taken; } ifs[MAX_IFS];
static int if_depth;

static bool reading_now(void)
{
    for (int i = 0; i < if_depth; i++) {
        if (!ifs[i].taking) {
            return false;
        }
    }
    return true;
}

/*
 * the two halves of `ifeq (a,b)` or `ifeq "a" "b"`, compared after both
 * have been expanded, which is the whole point of the thing: what is
 * being asked is what the variables say, not what the file says
 */
static bool same_thing(char *text)
{
    char *expanded = expand(text, NULL);
    char *p = expanded;
    char first[MAX_LINE] = "", second[MAX_LINE] = "";

    while (space(*p)) {
        p++;
    }
    if (*p == '(') {
        char *comma = NULL;
        int depth = 0;
        for (char *q = p + 1; *q != '\0'; q++) {
            if (*q == '(') { depth++; }
            else if (*q == ')') { if (depth == 0) { *q = '\0'; break; } depth--; }
            else if (*q == ',' && depth == 0 && comma == NULL) { comma = q; }
        }
        if (comma != NULL) {
            *comma = '\0';
            snprintf(first, sizeof first, "%s", p + 1);
            snprintf(second, sizeof second, "%s", comma + 1);
        }
    } else {
        /*
         * the quoted form. either kind of quote, and they need not be
         * the same kind on both halves
         */
        for (int which = 0; which < 2; which++) {
            while (space(*p)) {
                p++;
            }
            char quote = *p;
            if (quote != '"' && quote != '\'') {
                break;
            }
            char *end = strchr(p + 1, quote);
            if (end == NULL) {
                break;
            }
            *end = '\0';
            snprintf(which == 0 ? first : second,
                     which == 0 ? sizeof first : sizeof second, "%s", p + 1);
            p = end + 1;
        }
    }

    /*
     * the halves are compared stripped: gnu make treats `ifeq (a, b)`
     * as being about a and b rather than about b with a space in front
     */
    char *a = first, *z;
    while (space(*a)) { a++; }
    for (z = a + strlen(a); z > a && space(z[-1]); z--) { z[-1] = '\0'; }
    char *c = second;
    while (space(*c)) { c++; }
    for (z = c + strlen(c); z > c && space(z[-1]); z--) { z[-1] = '\0'; }

    bool answer = strcmp(a, c) == 0;
    free(expanded);
    return answer;
}

/* a directive line, if this one is: true when it was dealt with here */
static bool conditional(char *p)
{
    static const char *words_[] = { "ifdef", "ifndef", "ifeq", "ifneq",
                                    "else", "endif", NULL };
    const char *which = NULL;
    size_t n = 0;
    for (int i = 0; words_[i] != NULL; i++) {
        n = strlen(words_[i]);
        if (strncmp(p, words_[i], n) == 0
         && (p[n] == '\0' || space(p[n]))) {
            which = words_[i];
            break;
        }
    }
    if (which == NULL) {
        return false;
    }
    char *rest = p + n;
    while (space(*rest)) {
        rest++;
    }

    if (strcmp(which, "endif") == 0) {
        if (if_depth > 0) {
            if_depth--;
        }
        return true;
    }
    if (strcmp(which, "else") == 0) {
        if (if_depth > 0) {
            bool outer = true;
            for (int i = 0; i + 1 < if_depth; i++) {
                outer = outer && ifs[i].taking;
            }
            /*
             * `else ifdef X` is an else and an if in one line, and the
             * if half is only asked if no branch has been taken yet
             */
            if (rest[0] != '\0' && strncmp(rest, "if", 2) == 0) {
                if (ifs[if_depth - 1].taken || !outer) {
                    ifs[if_depth - 1].taking = false;
                } else {
                    if_depth--;
                    conditional(rest);
                    return true;
                }
            } else {
                ifs[if_depth - 1].taking =
                    !ifs[if_depth - 1].taken && outer;
            }
        }
        return true;
    }

    if (if_depth >= MAX_IFS) {
        fprintf(stderr, "%s: conditionals nested too deep\n", program);
        exit(2);
    }
    bool yes;
    if (strcmp(which, "ifdef") == 0 || strcmp(which, "ifndef") == 0) {
        char name[128];
        char *expanded = expand(rest, NULL);
        snprintf(name, sizeof name, "%s", expanded);
        free(expanded);
        for (char *q = name; *q != '\0'; q++) {
            if (space(*q)) { *q = '\0'; break; }
        }
        struct var *v = find_var(name);
        /*
         * defined *and not empty*, which is what gnu make means by it
         * and is the difference between `ifdef` and "was it written
         * down": `X =` with nothing after it is not defined here
         */
        yes = v != NULL && v->value != NULL && v->value[0] != '\0';
        if (strcmp(which, "ifndef") == 0) {
            yes = !yes;
        }
    } else {
        yes = same_thing(rest);
        if (strcmp(which, "ifneq") == 0) {
            yes = !yes;
        }
    }
    if (!reading_now()) {
        yes = false;                /* inside a branch nobody is reading */
    }
    ifs[if_depth].taking = yes;
    ifs[if_depth].taken  = yes;
    if_depth++;
    return true;
}

static void read_makefile(const char *path);

/* `include a.mk b.mk`, and the forgiving kind. */
static void read_included(char *list, bool required, const char *from,
                          int at)
{
    char *expanded = expand(list, NULL);
    char *w[MAX_LIST];
    int n = words(expanded, w, MAX_LIST);

    for (int i = 0; i < n; i++) {
        FILE *probe = fopen(w[i], "r");
        if (probe == NULL) {
            if (required) {
                fprintf(stderr, "%s:%d: %s: No such file or directory\n",
                        from, at, w[i]);
                fprintf(stderr, "%s: *** No rule to make target '%s'.  "
                        "Stop.\n", program, w[i]);
                exit(2);
            }
            continue;
        }
        fclose(probe);
        read_makefile(w[i]);
    }
    free(expanded);
}

static void read_makefile(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "%s: *** No targets specified and no makefile found."
                "  Stop.\n", program);
        exit(2);
    }

    char line[MAX_LINE];
    int lineno = 0;
    struct rule *current = NULL;

    while (fgets(line, sizeof line, f) != NULL) {
        int started_at = ++lineno;
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        bool is_recipe = line[0] == '\t' && current != NULL
                      && reading_now();

        /* a line ending in a backslash continues on the next one. */
        while (len > 0 && line[len - 1] == '\\') {
            char more[MAX_LINE];
            if (fgets(more, sizeof more, f) == NULL) {
                break;
            }
            lineno++;
            size_t mlen = strlen(more);
            while (mlen > 0 && (more[mlen - 1] == '\n' || more[mlen - 1] == '\r')) {
                more[--mlen] = '\0';
            }
            if (is_recipe) {
                snprintf(line + len, sizeof line - len, "\n%s", more);
            } else {
                char *p = more;
                while (space(*p)) {
                    p++;
                }
                line[--len] = '\0';
                snprintf(line + len, sizeof line - len, " %s", p);
            }
            len = strlen(line);
        }

        if (is_recipe) {
            if (current->lines < MAX_RECIPE) {
                current->recipe_at[current->lines] = started_at;
                current->recipe[current->lines++] = copy(line + 1);
            }
            continue;
        }

        /*
         * a comment runs to the end of the line, but only outside a
         * recipe, where a `#` belongs to the shell and means something
         * else entirely
         */
        char *hash = strchr(line, '#');
        if (hash != NULL) {
            *hash = '\0';
        }

        char *p = line;
        while (space(*p)) {
            p++;
        }
        if (*p == '\0') {
            continue;
        }

        /*
         * the conditionals are read whether or not this branch is being
         * taken, because an `endif` inside a branch nobody is reading is
         * still the end of it
         */
        if (conditional(p)) {
            continue;
        }
        if (!reading_now()) {
            continue;
        }

        if (strncmp(p, "include", 7) == 0 && space(p[7])) {
            read_included(p + 8, true, path, started_at);
            current = NULL;
            continue;
        }
        if ((strncmp(p, "-include", 8) == 0 && space(p[8]))
         || (strncmp(p, "sinclude", 8) == 0 && space(p[8]))) {
            read_included(p + 9, false, path, started_at);
            current = NULL;
            continue;
        }

        /* an assignment, if there is an `=` before any `:` */
        char *eq = NULL;
        int kind = '=';
        for (char *q = p; *q != '\0'; q++) {
            if (*q == '=') {
                eq = q;
                if (q > p && (q[-1] == '+' || q[-1] == '?')) {
                    kind = q[-1];
                    eq = q - 1;
                }
                break;
            }
            if (*q == ':') {
                /* `:=` is an assignment and `:` is a rule, and the two are told apart by one character. */
                if (q[1] == '=') {
                    kind = ':';
                    eq = q;
                }
                break;
            }
            if (*q == ';') {
                break;
            }
        }

        if (eq != NULL) {
            char name[128];
            char *end = eq;
            while (end > p && space(end[-1])) {
                end--;
            }
            snprintf(name, sizeof name, "%.*s", (int)(end - p), p);

            char *value = eq + (kind == '=' ? 1 : 2);
            while (space(*value)) {
                value++;
            }

            struct var *old = find_var(name);
            if (kind == '?' && old != NULL) {
                continue;               /* already said: leave it alone */
            }
            if (kind == '+' && old != NULL) {
                /*
                 * appending keeps the kind it already had, which is the
                 * only way `CFLAGS = $(WARN)` then `CFLAGS += -g` can
                 * still see a WARN defined further down
                 */
                struct buf b = { 0 };
                puts_(&b, old->value);
                putc_(&b, ' ');
                puts_(&b, old->simple ? (char *)(value = expand(value, NULL))
                                      : value);
                set_var(name, taken(&b), old->simple);
                continue;
            }
            set_var(name, kind == ':' ? expand(value, NULL) : copy(value),
                    kind == ':');
            current = NULL;
            continue;
        }

        char *colon = strchr(p, ':');
        if (colon == NULL) {
            fprintf(stderr, "%s:%d: *** missing separator.  Stop.\n",
                    path, started_at);
            exit(2);
        }

        /*
         * the target and prerequisite halves are expanded *now*, unlike
         * a recipe, which is expanded when it runs. that is why a rule
         * cannot use `$@`, there is no target yet at the moment the
         * line is read
         */
        char targets[MAX_LINE];
        snprintf(targets, sizeof targets, "%.*s", (int)(colon - p), p);
        bool doubled = (colon[1] == ':');
        char *rest = colon + (doubled ? 2 : 1);
        char *semi = strchr(rest, ';');
        char one_line[MAX_LINE] = "";
        if (semi != NULL) {
            snprintf(one_line, sizeof one_line, "%s", semi + 1);
            *semi = '\0';
        }

        char *left = expand(targets, NULL);
        char *right = expand(rest, NULL);

        if (rules >= MAX_RULES) {
            fprintf(stderr, "%s: too many rules\n", program);
            exit(2);
        }
        struct rule *r = &rule[rules++];
        memset(r, 0, sizeof *r);
        r->doubled = doubled;
        add_to(r->target, &r->targets, left);
        add_to(r->prereq, &r->prereqs, right);
        for (int i = 0; i < r->targets; i++) {
            if (strchr(r->target[i], '%') != NULL) {
                r->pattern = true;
            }
        }
        if (one_line[0] != '\0') {
            char *at = one_line;
            while (space(*at)) {
                at++;
            }
            r->recipe_at[r->lines] = started_at;
            r->recipe[r->lines++] = copy(at);
        }
        current = r;

        /* `.PHONY` names targets that are not files. */
        if (has_target(r, ".PHONY")) {
            for (int i = 0; i < r->prereqs; i++) {
                node_for(r->prereq[i])->phony = true;
            }
        }
    }
    fclose(f);
}



static int run_line(const char *text, const char *target, int at)
{
    bool quiet = silent, ignore = false;
    const char *cmd = text;

    while (*cmd == '@' || *cmd == '-' || *cmd == '+' || space(*cmd)) {
        if (*cmd == '@') {
            quiet = true;
        } else if (*cmd == '-') {
            ignore = true;
        }
        cmd++;
    }
    if (*cmd == '\0') {
        return 0;
    }

    /* -n prints even the quiet ones. */
    if (!quiet || dry_run) {
        printf("%s\n", cmd);
    }
    fflush(stdout);     /* before the child writes anything of its own */
    if (dry_run) {
        return 0;
    }

    /* the one line that is not portable to velvetOS. */
    int status = system(cmd);
    int code = 0;
    if (status == -1) {
        code = 127;
    } else if (WIFEXITED(status)) {
        code = WEXITSTATUS(status);
    } else {
        code = 128 + WTERMSIG(status);
    }
    if (code != 0 && ignore) {
        /* a `-` in front means carry on, and it still says so. */
        fprintf(stderr, "%s: [%s:%d: %s] Error %d (ignored)\n",
                program, makefile_name, at, target, code);
        return 0;
    }
    if (code != 0) {
        fprintf(stderr, "%s: *** [%s:%d: %s] Error %d\n",
                program, makefile_name, at, target, code);
        return code;
    }
    return 0;
}



static bool newer(struct timespec a, struct timespec b)
{
    if (a.tv_sec != b.tv_sec) {
        return a.tv_sec > b.tv_sec;
    }
    return a.tv_nsec > b.tv_nsec;
}

static int build(const char *name, const char *needed_by)
{
    struct node *n = node_for(name);
    if (n->done) {
        return 0;
    }
    n->visiting = true;

    /*
     * each rule stands alone: its own prerequisites, its own answer to
     * "is this out of date", its own recipe run or not run. so the walk
     * for one of these is a loop over the rules rather than a single
     * decision, and the ordinary path below never sees them.
     *
     * the case that makes them worth having is the one at the end: a
     * rule with no prerequisites at all runs every time, which for the
     * single-colon kind would be a rule that never runs once its target
     * exists
     */
    bool doubled = false;
    for (int i = 0; i < rules; i++) {
        if (!rule[i].pattern && rule[i].doubled && has_target(&rule[i], name)) {
            doubled = true;
        }
    }
    if (doubled) {
        struct stat st;
        n->exists = stat(name, &st) == 0;
        if (n->exists) {
            n->when = st.st_mtim;
        }

        /*
         * the state the target was in *before any of these ran*, which
         * is what all of them are judged against. asking again between
         * rules would let the first recipe's own output make the second
         * one look up to date, so a target with two rules and neither
         * of its files there would build half of itself
         */
        bool existed = n->exists;
        struct timespec when = n->when;

        for (int i = 0; i < rules; i++) {
            if (rule[i].pattern || !rule[i].doubled
             || !has_target(&rule[i], name)) {
                continue;
            }
            bool stale = !existed || n->phony || rule[i].prereqs == 0;

            for (int j = 0; j < rule[i].prereqs; j++) {
                struct node *p = node_for(rule[i].prereq[j]);
                if (p->visiting) {
                    fprintf(stderr,
                            "%s: Circular %s <- %s dependency dropped.\n",
                            program, name, rule[i].prereq[j]);
                    continue;
                }
                if (build(rule[i].prereq[j], name) != 0) {
                    return 2;
                }
                if (p->updated || (p->exists && (!existed
                                || newer(p->when, when)))) {
                    stale = true;
                }
            }

            if (!stale || rule[i].lines == 0) {
                continue;
            }
            struct context ctx = { name, rule[i].prereq, rule[i].prereqs, "" };
            for (int k = 0; k < rule[i].lines; k++) {
                char *text = expand(rule[i].recipe[k], &ctx);
                int bad = run_line(text, name, rule[i].recipe_at[k]);
                free(text);
                if (bad != 0) {
                    return 2;
                }
            }
            n->updated = true;
        }
        if (stat(name, &st) == 0) {
            n->exists = true;
            n->when = st.st_mtim;
        }

        n->visiting = false;
        n->done = true;
        return 0;
    }

    /* everything the explicit rules say about this target. */
    char *prereq[MAX_LIST];
    int prereqs = 0;
    struct rule *recipe = NULL;
    bool named = false;
    char stem[512] = "";

    for (int i = 0; i < rules; i++) {
        if (rule[i].pattern || !has_target(&rule[i], name)) {
            continue;
        }
        named = true;
        if (rule[i].lines > 0) {
            recipe = &rule[i];
        }
    }

    /*
     * the rule that brought the recipe goes first, and the rest follow
     * in the order the file wrote them. that is gnu make's order rather
     * than an obvious one, i had them all in file order and the
     * comparison said otherwise, and it matters, because the order of
     * `$^` is the order the arguments reach the command
     */
    if (recipe != NULL) {
        for (int j = 0; j < recipe->prereqs && prereqs < MAX_LIST; j++) {
            prereq[prereqs++] = recipe->prereq[j];
        }
    }
    for (int i = 0; i < rules; i++) {
        if (rule[i].pattern || &rule[i] == recipe ||
            !has_target(&rule[i], name)) {
            continue;
        }
        for (int j = 0; j < rule[i].prereqs && prereqs < MAX_LIST; j++) {
            prereq[prereqs++] = rule[i].prereq[j];
        }
    }

    struct stat st;
    n->exists = stat(name, &st) == 0;
    if (n->exists) {
        n->when = st.st_mtim;
    }

    /* no explicit recipe: try the pattern rules. */
    static char *made[MAX_LIST];
    if (recipe == NULL) {
        for (int i = 0; i < rules && recipe == NULL; i++) {
            if (!rule[i].pattern || rule[i].lines == 0) {
                continue;
            }
            char try[512];
            if (!pattern_match(rule[i].target[0], name, try, sizeof try)) {
                continue;
            }
            int candidates = 0;
            bool usable = true;
            for (int j = 0; j < rule[i].prereqs; j++) {
                struct buf b = { 0 };
                for (const char *p = rule[i].prereq[j]; *p != '\0'; p++) {
                    if (*p == '%') {
                        puts_(&b, try);
                    } else {
                        putc_(&b, *p);
                    }
                }
                char *want = taken(&b);
                made[candidates++] = want;

                struct stat unused;
                if (stat(want, &unused) == 0) {
                    continue;
                }
                bool buildable = false;
                for (int k = 0; k < rules && !buildable; k++) {
                    buildable = !rule[k].pattern && has_target(&rule[k], want);
                }
                if (!buildable) {
                    usable = false;
                }
            }
            if (!usable) {
                continue;
            }
            for (int j = 0; j < candidates && prereqs < MAX_LIST; j++) {
                prereq[prereqs++] = made[j];
            }
            recipe = &rule[i];
            snprintf(stem, sizeof stem, "%s", try);
            named = true;
        }
    }

    if (!named && !n->exists) {
        if (needed_by != NULL) {
            fprintf(stderr, "%s: *** No rule to make target '%s', "
                    "needed by '%s'.  Stop.\n", program, name, needed_by);
        } else {
            fprintf(stderr, "%s: *** No rule to make target '%s'.  Stop.\n",
                    program, name);
        }
        return 2;
    }

    /* the prerequisites, in the order they were written. */
    bool stale = false;
    for (int i = 0; i < prereqs; i++) {
        struct node *p = node_for(prereq[i]);
        if (p->visiting) {
            fprintf(stderr, "%s: Circular %s <- %s dependency dropped.\n",
                    program, name, prereq[i]);
            continue;
        }
        if (build(prereq[i], name) != 0) {
            return 2;
        }
        if (p->updated) {
            stale = true;
            /*
             * something further down was remade, so this counts as
             * having been dealt with even if it has no recipe of its
             * own, which is what keeps `make` quiet about a target
             * that had nothing to do but whose prerequisites did
             */
            n->updated = true;
        } else if (p->exists && (!n->exists || newer(p->when, n->when))) {
            /* the one question make asks. */
            stale = true;
        }
    }

    if (n->phony || !n->exists) {
        stale = true;
    }

    if (stale && recipe != NULL) {
        struct context ctx = { name, prereq, prereqs, stem };
        for (int i = 0; i < recipe->lines; i++) {
            char *text = expand(recipe->recipe[i], &ctx);
            int bad = run_line(text, name, recipe->recipe_at[i]);
            free(text);
            if (bad != 0) {
                return 2;
            }
        }
        n->updated = true;
        if (stat(name, &st) == 0) {
            n->exists = true;
            n->when = st.st_mtim;
        }
    }

    n->visiting = false;
    n->done = true;
    return 0;
}

int main(int argc, char **argv)
{
    const char *slash = strrchr(argv[0], '/');
    snprintf(program, sizeof program, "%s", slash != NULL ? slash + 1 : argv[0]);

    char *goal[MAX_LIST];
    int goals = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            makefile_name = argv[++i];
        } else if (strcmp(argv[i], "-n") == 0) {
            dry_run = true;
        } else if (strcmp(argv[i], "-s") == 0) {
            silent = true;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "%s: i do not understand %s\n", program, argv[i]);
            return 2;
        } else if (goals < MAX_LIST) {
            goal[goals++] = argv[i];
        }
    }

    read_makefile(makefile_name);

    /*
     * with nothing asked for, the first target in the file, and not a
     * pattern rule or a `.PHONY`, which are about other targets rather
     * than targets themselves
     */
    if (goals == 0) {
        for (int i = 0; i < rules && goals == 0; i++) {
            if (rule[i].pattern) {
                continue;
            }
            for (int j = 0; j < rule[i].targets && goals == 0; j++) {
                if (rule[i].target[j][0] != '.') {
                    goal[goals++] = rule[i].target[j];
                }
            }
        }
    }
    if (goals == 0) {
        fprintf(stderr, "%s: *** No targets.  Stop.\n", program);
        return 2;
    }

    for (int i = 0; i < goals; i++) {
        if (build(goal[i], NULL) != 0) {
            return 2;
        }
        struct node *n = node_for(goal[i]);
        if (!n->updated) {
            /*
             * nothing ran, and there are two different reasons for that
             * worth telling apart: the file is there and current, or
             * there was never anything to do in the first place
             */
            if (n->exists) {
                printf("%s: '%s' is up to date.\n", program, goal[i]);
            } else {
                printf("%s: Nothing to be done for '%s'.\n", program, goal[i]);
            }
        }
    }
    return 0;
}
