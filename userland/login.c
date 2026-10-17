// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/login.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the door, which used to be in the kernel's shell. it does not open
 * until somebody names themselves, and then it becomes that person's
 * shell rather than starting one beside itself.
 */

#include "syscall.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PASSWD    "/boot/passwd"
#define RELEASE   "/boot/etc/os-release"
#define WELCOME   "/boot/welcome.txt"
#define SHELL     "/bin/sh"
#define WORD_MAX  64
#define FILE_MAX  4096

static char s_arr_file[FILE_MAX];

/* a whole file into s_arr_file, ended by a NUL: its length, or -1 */
static long slurp(const char *str_path)
{
    long i_fd = open(str_path);
    long i_len = 0;

    if (i_fd < 0) {
        return -1;
    }
    for (long i_got; (i_got = read_fd(i_fd, s_arr_file + i_len,
                                      FILE_MAX - 1 - i_len)) > 0;) {
        i_len += i_got;
    }
    close(i_fd);
    s_arr_file[i_len] = '\0';
    return i_len;
}

/*
 * what the machine says about itself before anybody has: the system's
 * name, and /boot/welcome.txt, named absolutely so that whatever is on
 * the data disk cannot change it
 */
static void greet(void)
{
    char arr_name[WORD_MAX] = "velvetOS";

    clear_screen();
    if (slurp(RELEASE) > 0) {
        const char *str_at = strstr(s_arr_file, "PRETTY_NAME=\"");
        if (str_at != NULL) {
            str_at += strlen("PRETTY_NAME=\"");
            size_t sz_len = 0;
            while (str_at[sz_len] != '"' && str_at[sz_len] != '\0'
                   && sz_len + 1 < sizeof arr_name) {
                arr_name[sz_len] = str_at[sz_len];
                sz_len++;
            }
            arr_name[sz_len] = '\0';
        }
    }
    printf("%s\n\n", arr_name);
    printf("Thou art I... And I am thou...\n"
           "Thou hast established a new bond...\n\n"
           "Thou shalt be blessed when creating\n"
           "Personas of the Computer's Arcana...\n\n");
    long i_len = slurp(WELCOME);
    if (i_len > 0) {
        write_fd(STDOUT, s_arr_file, i_len);
        printf("\n");
    }
}

/* a line from the keyboard, without its newline: false when there is none */
static bool ask(const char *str_question, char *str_out, bool b_echo)
{
    long i_mode = tty_mode_get();
    long i_got;

    printf("%s", str_question);
    if (!b_echo) {
        tty_mode((unsigned long)i_mode & ~(unsigned long)TTY_ECHO);
    }
    i_got = read_fd(STDIN, str_out, WORD_MAX - 1);
    if (!b_echo) {
        tty_mode((unsigned long)i_mode);
        printf("\n");           /* the one the hidden word did not echo */
    }
    if (i_got <= 0) {
        str_out[0] = '\0';
        return false;
    }
    str_out[i_got] = '\0';
    char *str_nl = strchr(str_out, '\n');
    if (str_nl != NULL) {
        *str_nl = '\0';
    }
    return true;
}

/*
 * the uid of `str_name` if `str_word` is its password, or -1. one answer
 * for a wrong name and a wrong word, because saying which was wrong
 * hands over half of it. a line is name:password:uid:description
 */
static long check(const char *str_name, const char *str_word)
{
    if (slurp(PASSWD) < 0) {
        return -1;
    }
    for (char *str_line = s_arr_file; *str_line != '\0';) {
        char *str_next = strchr(str_line, '\n');
        if (str_next != NULL) {
            *str_next++ = '\0';
        }
        char *str_pass = strchr(str_line, ':');
        char *str_uid = (str_pass != NULL) ? strchr(str_pass + 1, ':') : NULL;
        if (str_line[0] != '#' && str_uid != NULL) {
            *str_pass++ = '\0';
            *str_uid++ = '\0';
            if (strcmp(str_line, str_name) == 0
                && strcmp(str_pass, str_word) == 0) {
                return atoi(str_uid);
            }
        }
        if (str_next == NULL) {
            break;
        }
        str_line = str_next;
    }
    return -1;
}

int main(int i_argc, char **pp_argv)
{
    char arr_name[WORD_MAX];
    char arr_word[WORD_MAX];
    long i_uid;

    (void)i_argc;
    (void)pp_argv;
    greet();
    for (;;) {
        if (!ask("\nname the guest: ", arr_name, true)
            || arr_name[0] == '\0') {
            continue;
        }
        ask("and the word: ", arr_word, false);
        i_uid = check(arr_name, arr_word);
        memset(arr_word, 0, sizeof arr_word);
        if (i_uid >= 0) {
            break;
        }
        printf("that is not a name and a word known here.\n");
    }

    printf("\nwelcome, %s.\n", arr_name);
    if (i_uid != 0) {
        printf("thou art a guest here, and the room knows it.\n");
    }
    setenv("USER", arr_name);
    setenv("HOME", "/");
    chdir("/");
    if (setuid(i_uid) != 0) {
        printf("the kernel would not let this become %s\n", arr_name);
        return 1;
    }

    char *arr_args[] = { "sh", NULL };
    execv(SHELL, arr_args);
    printf("there is no shell at " SHELL " to give thee\n");
    return 1;
}
