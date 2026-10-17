// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/shparse.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * a line of the shell, turned into the programs it runs.
 */

#include "shparse.h"
#include <string.h>

#define NAME_MAX  64
#define VALUE_MAX 256

/* where the parse has got to, in the line and in the store */
struct parser {
    const char *str_at;
    char       *str_out;
    char       *str_end;
    sh_lookup   p_lookup;
    void       *p_ctx;
    const char *str_error;
};

static void put(struct parser *p, char c_ch)
{
    if (p->str_out < p->str_end) {
        *p->str_out++ = c_ch;
    } else {
        p->str_error = "that line is too long";
    }
}

static bool is_name(char c_ch)
{
    return (c_ch >= 'a' && c_ch <= 'z') || (c_ch >= 'A' && c_ch <= 'Z')
           || (c_ch >= '0' && c_ch <= '9') || c_ch == '_';
}

/* a word ends at a space, at the end of the line, or at an operator */
static bool ends_word(char c_ch)
{
    return c_ch == '\0' || c_ch == ' ' || c_ch == '\t' || c_ch == '\n'
           || c_ch == '\r' || c_ch == '|' || c_ch == '<' || c_ch == '>'
           || c_ch == '&';
}

/*
 * just past a `$`: `$?`, `$NAME` or `${NAME}`, replaced by its value, or
 * by nothing when it is not set. a `$` that names nothing is a `$`
 */
static void expand(struct parser *p)
{
    char arr_name[NAME_MAX];
    char arr_value[VALUE_MAX];
    size_t sz_len = 0;
    bool b_braced = *p->str_at == '{';
    char c_first = p->str_at[b_braced];

    if (*p->str_at == '?') {
        arr_name[sz_len++] = *p->str_at++;
    } else if (is_name(c_first) && !(c_first >= '0' && c_first <= '9')) {
        /* a name starts with a letter or `_`, so `$5` is just that */
        p->str_at += b_braced;
        while (is_name(*p->str_at) && sz_len + 1 < NAME_MAX) {
            arr_name[sz_len++] = *p->str_at++;
        }
        if (b_braced) {
            if (*p->str_at != '}') {
                p->str_error = "a ${ that never ends";
                return;
            }
            p->str_at++;
        }
    }
    arr_name[sz_len] = '\0';

    if (sz_len == 0) {
        put(p, '$');
        return;
    }
    if (p->p_lookup != NULL
        && p->p_lookup(arr_name, arr_value, sizeof arr_value, p->p_ctx)) {
        for (const char *str_v = arr_value; *str_v != '\0'; str_v++) {
            put(p, *str_v);
        }
    }
}

/* the rest of a quoted stretch, up to its closing quote */
static void quoted(struct parser *p, char c_quote)
{
    p->str_at++;
    while (*p->str_at != '\0' && *p->str_at != c_quote) {
        if (c_quote == '"' && *p->str_at == '$') {
            p->str_at++;
            expand(p);
        } else {
            put(p, *p->str_at++);
        }
    }
    if (*p->str_at != c_quote) {
        p->str_error = "a quote that never ends";
        return;
    }
    p->str_at++;
}

/*
 * one word into the store: where it starts, or NULL when it came to
 * nothing, which an unquoted variable that is not set does. `""` is a
 * word, and an empty one
 */
static const char *word(struct parser *p)
{
    char *str_start = p->str_out;
    bool b_quoted = false;

    while (!ends_word(*p->str_at) && p->str_error == NULL) {
        char c_ch = *p->str_at;
        if (c_ch == '\'' || c_ch == '"') {
            b_quoted = true;
            quoted(p, c_ch);
        } else if (c_ch == '$') {
            p->str_at++;
            expand(p);
        } else {
            put(p, *p->str_at++);
        }
    }
    put(p, '\0');
    return (str_start[0] == '\0' && !b_quoted) ? NULL : str_start;
}

static void skip_space(struct parser *p)
{
    while (*p->str_at == ' ' || *p->str_at == '\t' || *p->str_at == '\n'
           || *p->str_at == '\r') {
        p->str_at++;
    }
}

/* whether a stage has anything in it yet */
static bool begun(const struct sh_stage *p_stage)
{
    return p_stage->i_argc > 0 || p_stage->str_in != NULL
           || p_stage->str_out != NULL;
}

const char *sh_parse(const char *str_line, struct sh_command *p_out,
                     char *str_store, size_t sz_store, sh_lookup p_lookup,
                     void *p_ctx)
{
    struct parser p = { str_line, str_store, str_store + sz_store,
                        p_lookup, p_ctx, NULL };
    struct sh_stage *p_stage = &p_out->arr_stages[0];
    bool b_piped = false;

    memset(p_out, 0, sizeof *p_out);
    for (;;) {
        skip_space(&p);
        char c_ch = *p.str_at;
        if (c_ch == '\0' || c_ch == '#') {
            break;
        }
        if (p_out->b_background) {
            return "& only goes at the end of a line";
        }

        if (c_ch == '|') {
            if (p_stage->i_argc == 0) {
                return "nothing to run before a |";
            }
            if (p_stage == &p_out->arr_stages[SH_STAGES_MAX - 1]) {
                return "too many programs joined by |";
            }
            p_stage++;
            p.str_at++;
            b_piped = true;
            continue;
        }
        if (c_ch == '&') {
            p.str_at++;
            p_out->b_background = true;
            continue;
        }

        if (c_ch == '<' || c_ch == '>') {
            bool b_append = c_ch == '>' && p.str_at[1] == '>';
            p.str_at += b_append ? 2 : 1;
            skip_space(&p);
            const char *str_target = ends_word(*p.str_at) ? NULL : word(&p);
            if (p.str_error != NULL) {
                return p.str_error;
            }
            if (str_target == NULL) {
                return "nothing to redirect to";
            }
            if (c_ch == '<') {
                p_stage->str_in = str_target;
            } else {
                p_stage->str_out = str_target;
                p_stage->b_append = b_append;
            }
            continue;
        }

        const char *str_word = word(&p);
        if (p.str_error != NULL) {
            return p.str_error;
        }
        if (str_word == NULL) {
            continue;
        }
        if (p_stage->i_argc == SH_WORDS_MAX) {
            return "too many words";
        }
        p_stage->arr_argv[p_stage->i_argc++] = (char *)str_word;
    }

    if (!begun(p_stage)) {
        if (b_piped) {
            return "nothing to run after a |";
        }
        if (p_out->b_background) {
            return "nothing to run before a &";
        }
        return NULL;            /* an empty line, or only a comment */
    }
    if (p_stage->i_argc == 0) {
        return "a redirection with nothing to run";
    }
    p_out->i_stages = (int)(p_stage - p_out->arr_stages) + 1;
    return NULL;
}
