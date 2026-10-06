// SPDX-License-Identifier: GPL-2.0-only
/*
 * toolchain/asmpre.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the macro pass, in front of the assembler.
 */

#include "asmpre.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_VARS   64
#define MAX_MACROS 32
#define MAX_BODY   128
#define MAX_DEPTH  16

struct var {
    char    name[32];
    int64_t value;
};

struct macro {
    char   name[32];
    int    args;
    /* fixed rows rather than pointers, so the body can be handed to `run` as-is. */
    char   body[MAX_BODY][ASMPRE_LINE];
    int    lines;
};

struct pre {
    struct var   var[MAX_VARS];
    int          vars;
    struct macro macro[MAX_MACROS];
    int          macros;

    char (*out)[ASMPRE_LINE];
    size_t out_max;
    size_t out_len;

    char error[160];
    int  error_line;
};

static bool pre_fail(struct pre *p, int line, const char *what)
{
    if (p->error[0] == '\0') {
        snprintf(p->error, sizeof p->error, "%s", what);
        p->error_line = line;
    }
    return false;
}



static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t') { s++; }
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'
                  || s[n - 1] == ' ' || s[n - 1] == '\t')) {
        s[--n] = '\0';
    }
    return s;
}

static bool starts(const char *s, const char *word)
{
    size_t n = strlen(word);
    if (strncmp(s, word, n) != 0) {
        return false;
    }
    return s[n] == '\0' || s[n] == ' ' || s[n] == '\t';
}

static int64_t *var_of(struct pre *p, const char *name)
{
    for (int i = 0; i < p->vars; i++) {
        if (strcmp(p->var[i].name, name) == 0) {
            return &p->var[i].value;
        }
    }
    return NULL;
}

/*
 * enough for `%1 == 8 || %1 == 14` and `v+1`, which between them are
 * every expression in the four files. one precedence step for `*`, then
 * `+ -`, then the comparisons, then the logical pair, the ordinary
 * shape, small enough to read in one sitting
 */
struct scan { const char *at; struct pre *p; bool ok; };

static int64_t expr_or(struct scan *s);

static void skip_blank(struct scan *s)
{
    while (*s->at == ' ' || *s->at == '\t') { s->at++; }
}

static int64_t expr_atom(struct scan *s)
{
    skip_blank(s);

    if (*s->at == '(') {
        s->at++;
        int64_t v = expr_or(s);
        skip_blank(s);
        if (*s->at == ')') { s->at++; }
        return v;
    }
    if (*s->at == '-') {
        s->at++;
        return -expr_atom(s);
    }
    if ((*s->at >= '0' && *s->at <= '9')) {
        char *end = NULL;
        int64_t v = (int64_t)strtoll(s->at, &end, 0);
        s->at = end;
        return v;
    }

    char name[64];
    size_t n = 0;
    while ((*s->at >= 'a' && *s->at <= 'z')
        || (*s->at >= 'A' && *s->at <= 'Z')
        || (*s->at >= '0' && *s->at <= '9')
        || *s->at == '_') {
        if (n + 1 < sizeof name) { name[n++] = *s->at; }
        s->at++;
    }
    name[n] = '\0';
    if (n == 0) {
        s->ok = false;
        return 0;
    }

    int64_t *v = var_of(s->p, name);
    if (v == NULL) {
        /*
         * a name this pass does not know is not an error: it may be a
         * label the assembler will resolve. it counts as zero here,
         * which only matters inside %if, and an %if on a label
         * address is not something these files do
         */
        return 0;
    }
    return *v;
}

static int64_t expr_mul(struct scan *s)
{
    int64_t v = expr_atom(s);
    for (;;) {
        skip_blank(s);
        if (*s->at == '*') { s->at++; v *= expr_atom(s); }
        else if (*s->at == '/') { s->at++; int64_t d = expr_atom(s);
                                  v = (d != 0) ? v / d : 0; }
        else { return v; }
    }
}

static int64_t expr_add(struct scan *s)
{
    int64_t v = expr_mul(s);
    for (;;) {
        skip_blank(s);
        if (*s->at == '+') { s->at++; v += expr_mul(s); }
        else if (*s->at == '-' && s->at[1] != '-') { s->at++; v -= expr_mul(s); }
        else { return v; }
    }
}

static int64_t expr_cmp(struct scan *s)
{
    int64_t v = expr_add(s);
    for (;;) {
        skip_blank(s);
        if (s->at[0] == '=' && s->at[1] == '=') { s->at += 2; v = (v == expr_add(s)); }
        else if (s->at[0] == '!' && s->at[1] == '=') { s->at += 2; v = (v != expr_add(s)); }
        else if (s->at[0] == '<' && s->at[1] == '=') { s->at += 2; v = (v <= expr_add(s)); }
        else if (s->at[0] == '>' && s->at[1] == '=') { s->at += 2; v = (v >= expr_add(s)); }
        else if (s->at[0] == '<') { s->at++; v = (v < expr_add(s)); }
        else if (s->at[0] == '>') { s->at++; v = (v > expr_add(s)); }
        else { return v; }
    }
}

static int64_t expr_or(struct scan *s)
{
    int64_t v = expr_cmp(s);
    for (;;) {
        skip_blank(s);
        if (s->at[0] == '|' && s->at[1] == '|') {
            s->at += 2;
            int64_t r = expr_cmp(s);
            v = (v || r);
        } else if (s->at[0] == '&' && s->at[1] == '&')
{
            s->at += 2;
            int64_t r = expr_cmp(s);
            v = (v && r);
        } else {
            return v;
        }
    }
}

static int64_t evaluate(struct pre *p, const char *text, bool *ok)
{
    struct scan s = { text, p, true };
    int64_t v = expr_or(&s);
    *ok = s.ok;
    return v;
}



/* replace %1..%9 with the arguments given at the point of use. */
static void substitute(const char *in, char (*arg)[64], int args,
                       char *out, size_t max)
{
    size_t at = 0;
    for (const char *s = in; *s && at + 1 < max; ) {
        if (s[0] == '%' && s[1] >= '1' && s[1] <= '9') {
            int which = s[1] - '1';
            if (which < args) {
                for (const char *a = arg[which]; *a && at + 1 < max; a++) {
                    out[at++] = *a;
                }
            }
            s += 2;
            continue;
        }
        out[at++] = *s++;
    }
    out[at] = '\0';
}

/*
 * nasm's token paste: `isr_stub_%+v` is `isr_stub_` and the value of
 * `v`, run together into one name.
 *
 * it exists because there is no other way to build a name from a
 * counter, `isr_stub_v` would be a label called that, and the table
 * of 256 entries needs `isr_stub_0` through `isr_stub_255`. so the
 * operator says "the thing after this is a number to spell out, not a
 * word to keep"
 */
static void paste(struct pre *p, char *line, size_t max)
{
    for (;;) {
        char *at = strstr(line, "%+");
        if (at == NULL) {
            return;
        }

        char name[64];
        size_t n = 0;
        char *s = at + 2;
        while (*s == ' ' || *s == '\t') { s++; }
        while (((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z')
             || (*s >= '0' && *s <= '9') || *s == '_')
               && n + 1 < sizeof name) {
            name[n++] = *s++;
        }
        name[n] = '\0';

        char value[32];
        int64_t *v = var_of(p, name);
        if (v != NULL) {
            snprintf(value, sizeof value, "%lld", (long long)*v);
        } else {
            snprintf(value, sizeof value, "%s", name);
        }

        char rest[ASMPRE_LINE];
        snprintf(rest, sizeof rest, "%s", s);

        size_t head = (size_t)(at - line);
        snprintf(at, max - head, "%s%s", value, rest);
    }
}



static bool emit(struct pre *p, const char *line, int at_line)
{
    if (p->out_len >= p->out_max) {
        return pre_fail(p, at_line, "the expansion is too large");
    }
    snprintf(p->out[p->out_len], ASMPRE_LINE, "%s", line);
    p->out_len++;
    return true;
}

static bool run(struct pre *p, char (*lines)[ASMPRE_LINE], size_t count,
                int depth, char (*arg)[64], int args);

/*
 * gather the body of a %rep or %macro up to its matching end, allowing
 * for nesting, a %rep inside a %macro is exactly what isr.asm does
 */
static size_t gather(char (*lines)[ASMPRE_LINE], size_t from, size_t count,
                     const char *opener, const char *closer,
                     char (*body)[ASMPRE_LINE], size_t max, size_t *taken)
{
    size_t n = 0;
    int depth = 1;
    size_t i = from;

    for (; i < count; i++) {
        char copy[ASMPRE_LINE];
        snprintf(copy, sizeof copy, "%s", lines[i]);
        char *t = trim(copy);

        if (starts(t, opener)) { depth++; }
        if (starts(t, closer)) {
            depth--;
            if (depth == 0) { break; }
        }
        if (n < max) {
            snprintf(body[n], ASMPRE_LINE, "%s", lines[i]);
            n++;
        }
    }
    *taken = i - from;
    return n;
}

static bool run(struct pre *p, char (*lines)[ASMPRE_LINE], size_t count,
                int depth, char (*arg)[64], int args)
{
    if (depth > MAX_DEPTH) {
        return pre_fail(p, 0, "macros nested too deeply");
    }

    for (size_t i = 0; i < count; i++) {
        char work[ASMPRE_LINE];
        substitute(lines[i], arg, args, work, sizeof work);
        paste(p, work, sizeof work);

        char copy[ASMPRE_LINE];
        snprintf(copy, sizeof copy, "%s", work);
        char *t = trim(copy);

        if (t[0] != '%') {
            /* an ordinary line, or a use of a macro */
            char first[64] = { 0 };
            sscanf(t, "%63s", first);

            bool used = false;
            for (int m = 0; m < p->macros; m++) {
                if (strcmp(p->macro[m].name, first) != 0) {
                    continue;
                }
                /*
                 * the arguments, evaluated *now*, `ISR_STUB v` has to
                 * become the number v currently holds, not the letter
                 */
                char mine[9][64];
                memset(mine, 0, sizeof mine);
                const char *rest = t + strlen(first);
                for (int k = 0; k < p->macro[m].args && k < 9; k++) {
                    while (*rest == ' ' || *rest == '\t' || *rest == ',') {
                        rest++;
                    }
                    char piece[64];
                    size_t n = 0;
                    while (*rest && *rest != ',' && n + 1 < sizeof piece) {
                        piece[n++] = *rest++;
                    }
                    piece[n] = '\0';
                    char *q = trim(piece);

                    bool ok = true;
                    int64_t v = evaluate(p, q, &ok);
                    if (ok && (var_of(p, q) != NULL
                               || (q[0] >= '0' && q[0] <= '9'))) {
                        snprintf(mine[k], sizeof mine[k], "%lld",
                                 (long long)v);
                    } else {
                        snprintf(mine[k], sizeof mine[k], "%s", q);
                    }
                }
                if (!run(p, p->macro[m].body, (size_t)p->macro[m].lines,
                         depth + 1, mine, p->macro[m].args)) {
                    return false;
                }
                used = true;
                break;
            }
            if (!used && !emit(p, work, (int)i)) {
                return false;
            }
            continue;
        }


        if (starts(t, "%assign")) {
            char name[32] = { 0 };
            const char *rest = t + 7;
            while (*rest == ' ' || *rest == '\t') { rest++; }
            size_t n = 0;
            while (*rest && *rest != ' ' && *rest != '\t'
                   && n + 1 < sizeof name) {
                name[n++] = *rest++;
            }
            name[n] = '\0';

            bool ok = true;
            int64_t v = evaluate(p, rest, &ok);

            int64_t *slot = var_of(p, name);
            if (slot != NULL) {
                *slot = v;
            } else if (p->vars < MAX_VARS) {
                snprintf(p->var[p->vars].name, 32, "%s", name);
                p->var[p->vars].value = v;
                p->vars++;
            }
            continue;
        }

        if (starts(t, "%rep")) {
            bool ok = true;
            int64_t times = evaluate(p, t + 4, &ok);

            static char body[MAX_BODY * 4][ASMPRE_LINE];
            size_t taken = 0;
            size_t n = gather(lines, i + 1, count, "%rep", "%endrep",
                              body, MAX_BODY * 4, &taken);

            for (int64_t k = 0; k < times; k++) {
                /*
                 * the body is run again each time rather than copied,
                 * so that an %assign inside it counts, which is how
                 * `ISR_STUB v` gets a different v on every pass
                 */
                if (!run(p, body, n, depth + 1, arg, args)) {
                    return false;
                }
            }
            i += taken;
            continue;
        }

        if (starts(t, "%macro")) {
            char name[32] = { 0 };
            int argc = 0;
            sscanf(t + 6, "%31s %d", name, &argc);

            if (p->macros >= MAX_MACROS) {
                return pre_fail(p, (int)i, "too many macros");
            }
            struct macro *m = &p->macro[p->macros++];
            snprintf(m->name, sizeof m->name, "%s", name);
            m->args = argc;

            static char body[MAX_BODY][ASMPRE_LINE];
            size_t taken = 0;
            size_t n = gather(lines, i + 1, count, "%macro", "%endmacro",
                              body, MAX_BODY, &taken);

            m->lines = (int)n;
            for (size_t k = 0; k < n; k++) {
                snprintf(m->body[k], ASMPRE_LINE, "%s", body[k]);
            }
            i += taken;
            continue;
        }

        if (starts(t, "%if")) {
            bool ok = true;
            int64_t cond = evaluate(p, t + 3, &ok);

            static char taken_body[MAX_BODY][ASMPRE_LINE];
            static char other_body[MAX_BODY][ASMPRE_LINE];
            size_t tn = 0, on = 0;
            int nest = 1;
            bool in_else = false;
            size_t j = i + 1;

            for (; j < count; j++) {
                char c2[ASMPRE_LINE];
                snprintf(c2, sizeof c2, "%s", lines[j]);
                char *u = trim(c2);

                if (starts(u, "%if")) { nest++; }
                if (starts(u, "%endif")) {
                    nest--;
                    if (nest == 0) { break; }
                }
                if (nest == 1 && starts(u, "%else")) { in_else = true; continue; }

                if (!in_else) {
                    if (tn < MAX_BODY) { snprintf(taken_body[tn++], ASMPRE_LINE, "%s", lines[j]); }
                } else {
                    if (on < MAX_BODY) { snprintf(other_body[on++], ASMPRE_LINE, "%s", lines[j]); }
                }
            }

            if (!run(p, cond ? taken_body : other_body,
                     cond ? tn : on, depth + 1, arg, args)) {
                return false;
            }
            i = j;
            continue;
        }

        if (starts(t, "%endrep") || starts(t, "%endmacro")
         || starts(t, "%endif") || starts(t, "%else")) {
            continue;   /* consumed by whatever opened them */
        }

        /*
         * anything else beginning with % is not understood, and saying
         * so beats emitting it for the assembler to choke on with a
         * worse message
         */
        return pre_fail(p, (int)i, t);
    }
    return true;
}

bool asmpre_run(char (*in)[ASMPRE_LINE], size_t count,
                char (*out)[ASMPRE_LINE], size_t out_max, size_t *out_len,
                char *error, size_t error_max, int *error_line)
{
    static struct pre p;
    memset(&p, 0, sizeof p);
    p.out = out;
    p.out_max = out_max;

    char none[9][64];
    memset(none, 0, sizeof none);

    bool ok = run(&p, in, count, 0, none, 0);
    *out_len = p.out_len;

    if (!ok) {
        snprintf(error, error_max, "%s", p.error);
        *error_line = p.error_line;
    }
    return ok;
}
