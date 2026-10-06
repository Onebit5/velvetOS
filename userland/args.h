// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/args.h
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * arguments, parsed once and in one place.
 */

#ifndef USER_ARGS_H
#define USER_ARGS_H

#include <stddef.h>
#include <stdbool.h>

/* arguments, parsed once and in one place. */

#define ARGS_MAX_OPTS 8
#define ARGS_MAX_REST 32

struct opt {
    char        brief;      /* the -x form, or 0 if there is none */
    const char *name;       /* the --long form, or NULL */
    bool        takes_value;
    const char *help;       /* one line, for whoever has to explain it */
};

struct program {
    const char *name;
    const char *usage;      /* the shape of a command line */
    const char *summary;    /* one line about what it is for */
    const struct opt *opts;
    size_t      opt_count;
};

struct args {
    /* one bit per declared option, by its position in the list */
    unsigned    given;
    const char *values[ARGS_MAX_OPTS];

    /* everything that was not an option, in order */
    int         count;
    char       *rest[ARGS_MAX_REST];

    /* --help was asked for. */
    bool        wants_help;
};

/* print what this program is and what it takes, built entirely out of the declaration above. */
void args_usage(const struct program *p);

/* returns false and sets `error` to something worth printing */
bool args_parse(const struct program *p, int argc, char **argv,
                struct args *out, const char **error);

/* was this option given? */
bool args_has(const struct args *a, const struct program *p, char brief);
bool args_has_long(const struct args *a, const struct program *p,
                   const char *name);

/* the value it was given, or NULL */
const char *args_value(const struct args *a, const struct program *p,
                       char brief);

#endif
