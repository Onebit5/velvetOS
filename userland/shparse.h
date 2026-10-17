// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/shparse.h
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * a line of the shell, turned into the programs it runs and what they
 * are joined by. nothing here runs anything, so all of it can be
 * checked on the host.
 */

#ifndef USER_SHPARSE_H
#define USER_SHPARSE_H

#include <stddef.h>
#include <stdbool.h>

#define SH_LINE_MAX   512
#define SH_WORDS_MAX  32
#define SH_STAGES_MAX 8

/* one program in a pipeline: its arguments, and where its i/o goes */
struct sh_stage {
    int         i_argc;
    char       *arr_argv[SH_WORDS_MAX + 1];  /* ended by NULL, for exec */
    const char *str_in;                      /* `<`, or NULL */
    const char *str_out;                     /* `>` or `>>`, or NULL */
    bool        b_append;
};

/* a line: the stages joined by `|`, and whether it ends in `&` */
struct sh_command {
    int             i_stages;
    struct sh_stage arr_stages[SH_STAGES_MAX];
    bool            b_background;
};

/*
 * a variable's value, for `$NAME` and `$?`: true with it in `str_out`,
 * or false when it is not set. handed in, so the parser has no idea of
 * an environment of its own
 */
typedef bool (*sh_lookup)(const char *str_name, char *str_out,
                          size_t sz_size, void *p_ctx);

/*
 * split `str_line` into `p_out`. words are separated by spaces and tabs,
 * a `#` that starts a word starts a comment, single quotes keep what is
 * inside them exactly, double quotes keep it but for `$`, and `|`, `<`,
 * `>`, `>>` and a last `&` mean what they mean outside quotes. the words
 * are written into `str_store`, which the stages point into. NULL when
 * the line made sense, which includes an empty one, or what was wrong
 * with it
 */
const char *sh_parse(const char *str_line, struct sh_command *p_out,
                     char *str_store, size_t sz_store, sh_lookup p_lookup,
                     void *p_ctx);

#endif
