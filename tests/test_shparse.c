// SPDX-License-Identifier: GPL-2.0-only
/*
 * tests/test_shparse.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * host-side test for the shell's line parser.
 */

#include <stdio.h>
#include <string.h>

#include "../userland/shparse.h"

static int s_i_failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); s_i_failures++; } \
} while (0)

static char s_arr_store[SH_LINE_MAX * 2];
static struct sh_command s_cmd;

/* HOME is /home/igor, ? is 3, EMPTY is empty, and nothing else is set */
static bool lookup(const char *str_name, char *str_out, size_t sz_size,
                   void *p_ctx)
{
    (void)p_ctx;
    const char *str_value = strcmp(str_name, "HOME") == 0 ? "/home/igor"
                          : strcmp(str_name, "?") == 0    ? "3"
                          : strcmp(str_name, "EMPTY") == 0 ? ""
                          : NULL;
    if (str_value == NULL) {
        return false;
    }
    snprintf(str_out, sz_size, "%s", str_value);
    return true;
}

static const char *parse(const char *str_line)
{
    return sh_parse(str_line, &s_cmd, s_arr_store, sizeof s_arr_store,
                    lookup, NULL);
}

/* the words of stage `i_stage`, joined by single spaces */
static const char *words(int i_stage)
{
    static char s_arr_joined[SH_LINE_MAX];
    const struct sh_stage *p_stage = &s_cmd.arr_stages[i_stage];

    s_arr_joined[0] = '\0';
    for (int i_at = 0; i_at < p_stage->i_argc; i_at++) {
        if (i_at > 0) {
            strcat(s_arr_joined, " ");
        }
        strcat(s_arr_joined, p_stage->arr_argv[i_at]);
    }
    return s_arr_joined;
}

int main(void)
{
    CHECK(parse("echo the bond endures") == NULL && s_cmd.i_stages == 1
          && strcmp(words(0), "echo the bond endures") == 0
          && s_cmd.arr_stages[0].arr_argv[4] == NULL,
          "words split on spaces, and argv ends with NULL");
    CHECK(parse("   \t ") == NULL && s_cmd.i_stages == 0,
          "an empty line is nothing to run, and not a mistake");
    CHECK(parse("# a comment") == NULL && s_cmd.i_stages == 0
          && parse("ls # and the rest") == NULL
          && strcmp(words(0), "ls") == 0,
          "a # that starts a word starts a comment");
    CHECK(parse("echo a#b") == NULL && strcmp(words(0), "echo a#b") == 0,
          "and one inside a word is only a #");

    CHECK(parse("echo 'two  words' \"$HOME is home\"") == NULL
          && s_cmd.arr_stages[0].i_argc == 3
          && strcmp(s_cmd.arr_stages[0].arr_argv[1], "two  words") == 0
          && strcmp(s_cmd.arr_stages[0].arr_argv[2],
                    "/home/igor is home") == 0,
          "quotes keep spaces, and double quotes still expand");
    CHECK(parse("echo '$HOME'") == NULL
          && strcmp(words(0), "echo $HOME") == 0,
          "single quotes keep a $ as it is");
    CHECK(parse("echo $? ${HOME}/x $NOTSET $EMPTY end") == NULL
          && strcmp(words(0), "echo 3 /home/igor/x end") == 0,
          "$?, ${NAME}, and an unset or empty one is no word at all");
    CHECK(parse("echo \"\" end") == NULL && s_cmd.arr_stages[0].i_argc == 3,
          "but \"\" is a word, an empty one");
    CHECK(parse("echo costs $5") == NULL
          && strcmp(words(0), "echo costs $5") == 0,
          "a $ before something that is not a name stays a $");

    CHECK(parse("cat x | grep y | wc -l") == NULL && s_cmd.i_stages == 3
          && strcmp(words(0), "cat x") == 0
          && strcmp(words(1), "grep y") == 0
          && strcmp(words(2), "wc -l") == 0,
          "| joins the stages of a pipeline");
    CHECK(parse("sort < in > out") == NULL
          && strcmp(s_cmd.arr_stages[0].str_in, "in") == 0
          && strcmp(s_cmd.arr_stages[0].str_out, "out") == 0
          && !s_cmd.arr_stages[0].b_append,
          "< and > take the word after them");
    CHECK(parse("echo hi >>log") == NULL
          && strcmp(s_cmd.arr_stages[0].str_out, "log") == 0
          && s_cmd.arr_stages[0].b_append
          && strcmp(words(0), "echo hi") == 0,
          ">> appends, and needs no space before its word");
    CHECK(parse("bin/counter > /c1.out &") == NULL
          && s_cmd.b_background
          && strcmp(s_cmd.arr_stages[0].str_out, "/c1.out") == 0,
          "a last & runs it in the background");
    CHECK(parse("a|b") == NULL && s_cmd.i_stages == 2,
          "| needs no spaces round it");

    CHECK(parse("| wc") != NULL, "nothing before a | is a mistake");
    CHECK(parse("ls |") != NULL, "and so is nothing after one");
    CHECK(parse("ls >") != NULL, "and a > with nothing to write to");
    CHECK(parse("> out") != NULL, "and a redirection with nothing to run");
    CHECK(parse("a & b") != NULL, "& only goes at the end");
    CHECK(parse("echo 'open") != NULL && parse("echo \"open") != NULL,
          "a quote that never ends is a mistake");
    CHECK(parse("a|b|c|d|e|f|g|h|i") != NULL, "too many stages");

    if (s_i_failures == 0) {
        printf("all good\n");
    }
    return s_i_failures;
}
