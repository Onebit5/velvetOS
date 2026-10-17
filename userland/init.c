// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/init.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the machine's init, pid 1: a login on each console, started again
 * when its session ends, and every orphan collected. the kernel starts
 * this and nothing else of the system's.
 */

#include "syscall.h"
#include <stdio.h>

#define CONSOLES     4
#define LOGIN        "/bin/login"
#define RESPAWN_MAX  5          /* this many quick deaths, and it stays down */
#define RESPAWN_MS   10000

struct session {
    long i_pid;
    long i_started;             /* uptime when it was last started */
    int  i_deaths;              /* in quick succession */
};

static struct session s_arr_sessions[CONSOLES];

/* a login on console `i_console`, in a group of its own, owning the console */
static long start(int i_console)
{
    long i_pid = fork();

    if (i_pid == 0) {
        char arr_tty[4] = { (char)('1' + i_console), '\0' };
        char *arr_args[] = { "login", NULL };

        setpgid(0, 0);
        ttyattach(i_console);
        setenv("TTY", arr_tty);
        signal_default(SIGINT);
        signal_default(SIGTSTP);
        execv(LOGIN, arr_args);
        printf("init: there is no " LOGIN " to start\n");
        exit(1);
    }
    return i_pid;
}

/* a session ended: start it again, unless it keeps ending at once */
static void ended(struct session *p_s, int i_console)
{
    long i_now = uptime();

    p_s->i_deaths = (i_now - p_s->i_started < RESPAWN_MS)
                    ? p_s->i_deaths + 1 : 0;
    if (p_s->i_deaths >= RESPAWN_MAX) {
        printf("init: the login on console %d keeps ending as soon as it "
               "starts. it stays down\n", i_console + 1);
        p_s->i_pid = 0;
        return;
    }
    p_s->i_started = i_now;
    p_s->i_pid = start(i_console);
}

int main(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;

    /*
     * pid 1 ending is the end of the machine, so the signals that end
     * things are not for it
     */
    signal_ignore(SIGINT);
    signal_ignore(SIGTSTP);
    signal_ignore(SIGTERM);
    signal_ignore(SIGHUP);

    for (int i_c = 0; i_c < CONSOLES; i_c++) {
        s_arr_sessions[i_c].i_started = uptime();
        s_arr_sessions[i_c].i_pid = start(i_c);
    }

    /* every child ends here, the sessions and every orphan handed over */
    for (;;) {
        int i_status = 0;
        long i_pid = waitpid(-1, &i_status, 0);
        if (i_pid <= 0) {
            sleep(1000);
            continue;
        }
        for (int i_c = 0; i_c < CONSOLES; i_c++) {
            if (s_arr_sessions[i_c].i_pid == i_pid) {
                ended(&s_arr_sessions[i_c], i_c);
            }
        }
    }
}
