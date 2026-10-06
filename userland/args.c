// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/args.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * arguments, and the ones that are only half parsed.
 */

#include "args.h"
#include "syscall.h"

static bool same(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return false;
    }
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

/*
 * the same, but only as far as `n`, for `--out=name`, where the name
 * being matched stops at the equals
 */
static bool same_n(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (a[i] == '\0' || a[i] != b[i]) {
            return false;
        }
    }
    return a[n] == '\0';
}

static int find_brief(const struct program *p, char c)
{
    for (size_t i = 0; i < p->opt_count; i++) {
        if (p->opts[i].brief != 0 && p->opts[i].brief == c) {
            return (int)i;
        }
    }
    return -1;
}

static int find_long(const struct program *p, const char *name, size_t n)
{
    for (size_t i = 0; i < p->opt_count; i++) {
        if (p->opts[i].name == NULL) {
            continue;
        }
        if (n == 0 ? same(p->opts[i].name, name)
                   : same_n(p->opts[i].name, name, n)) {
            return (int)i;
        }
    }
    return -1;
}

static bool keep(struct args *out, char *word, const char **error)
{
    if (out->count >= ARGS_MAX_REST) {
        *error = "too many arguments for the program to hold";
        return false;
    }
    out->rest[out->count++] = word;
    return true;
}

bool args_parse(const struct program *p, int argc, char **argv,
                struct args *out, const char **error)
{
    out->given = 0;
    out->count = 0;
    out->wants_help = false;
    for (size_t i = 0; i < ARGS_MAX_OPTS; i++) {
        out->values[i] = NULL;
    }
    *error = NULL;

    if (p->opt_count > ARGS_MAX_OPTS) {
        *error = "this program declares more options than the program can hold";
        return false;
    }

    bool options_over = false;

    for (int i = 1; i < argc; i++) {
        char *word = argv[i];

        if (options_over || word[0] != '-' || word[1] == '\0') {
            /*
             * a bare dash is a filename by long convention, and so is
             * anything at all once `--` has been seen
             */
            if (!keep(out, word, error)) {
                return false;
            }
            continue;
        }


        if (word[1] == '-') {
            if (word[2] == '\0') {
                options_over = true;    /* just `--`: nothing after is one */
                continue;
            }

            const char *name = word + 2;
            size_t n = 0;
            while (name[n] != '\0' && name[n] != '=') {
                n++;
            }
            bool has_equals = (name[n] == '=');

            if (same_n("help", name, n)) {
                out->wants_help = true;
                continue;
            }

            int at = find_long(p, name, has_equals ? n : 0);
            if (at < 0) {
                *error = "no such option";
                return false;
            }

            out->given |= 1u << at;
            if (p->opts[at].takes_value) {
                if (has_equals) {
                    out->values[at] = name + n + 1;
                } else if (i + 1 < argc) {
                    out->values[at] = argv[++i];
                } else {
                    *error = "that option wants a value after it";
                    return false;
                }
            } else if (has_equals) {
                *error = "that option takes no value";
                return false;
            }
            continue;
        }


        for (int k = 1; word[k] != '\0'; k++) {
            if (word[k] == 'h') {
                out->wants_help = true;
                continue;
            }

            int at = find_brief(p, word[k]);
            if (at < 0) {
                *error = "no such option";
                return false;
            }

            out->given |= 1u << at;
            if (!p->opts[at].takes_value) {
                continue;
            }

            /*
             * a value comes from the rest of this word if there is any
             *, `-oname`, and otherwise from the next one
             */
            if (word[k + 1] != '\0') {
                out->values[at] = &word[k + 1];
            } else if (i + 1 < argc) {
                out->values[at] = argv[++i];
            } else {
                *error = "that option wants a value after it";
                return false;
            }
            break;      /* the rest of the word was the value */
        }
    }

    return true;
}

bool args_has(const struct args *a, const struct program *p, char brief)
{
    int at = find_brief(p, brief);
    return at >= 0 && (a->given & (1u << at)) != 0;
}

bool args_has_long(const struct args *a, const struct program *p,
                   const char *name)
{
    int at = find_long(p, name, 0);
    return at >= 0 && (a->given & (1u << at)) != 0;
}

const char *args_value(const struct args *a, const struct program *p,
                       char brief)
{
    int at = find_brief(p, brief);
    return at >= 0 ? a->values[at] : NULL;
}




static void pad_to(size_t from, size_t to)
{
    for (size_t i = from; i < to; i++) {
        write(" ");
    }
}

static size_t len_of(const char *s)
{
    size_t n = 0;
    while (s != NULL && s[n] != '\0') {
        n++;
    }
    return n;
}

void args_usage(const struct program *p)
{
    write(p->usage);
    write("\n");
    if (p->summary != NULL) {
        write("  ");
        write(p->summary);
        write("\n");
    }

    if (p->opt_count == 0) {
        return;
    }

    write("\n");
    for (size_t i = 0; i < p->opt_count; i++) {
        const struct opt *o = &p->opts[i];
        size_t w = 0;

        write("  ");
        if (o->brief != 0) {
            char two[3] = { '-', o->brief, '\0' };
            write(two);
            w += 2;
            if (o->name != NULL) {
                write(", ");
                w += 2;
            }
        } else {
            write("    ");
            w += 4;
        }
        if (o->name != NULL) {
            write("--");
            write(o->name);
            w += 2 + len_of(o->name);
        }
        if (o->takes_value) {
            write(" <value>");
            w += 8;
        }

        pad_to(w, 22);
        write("  ");
        write(o->help);
        write("\n");
    }

    write("  -h, --help            this\n");
}
