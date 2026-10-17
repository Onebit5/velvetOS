// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/sh.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the shell, as a program. what the kernel's shell did inside the kernel,
 * done with the calls any program has: a pipeline is forks joined by
 * pipes, a job is a process group, ctrl+z is a stop that wait reports,
 * and the terminal goes to whichever job is in front of it.
 *
 *   sh            read commands from the terminal
 *   sh <file>     run the commands in a file, and leave
 */

#include "syscall.h"
#include "shparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define PATH_DEFAULT  "/bin:/boot/bin"
#define NAME_MAX      256
#define HISTORY_MAX   64
#define JOBS_MAX      8
#define JOB_TEXT_MAX  64
#define SCRIPT_MAX    (16 * 1024)
#define LINES_MAX     512
#define DEPTH_MAX     8
#define STEPS_MAX     100000
#define SAVED_FD      11        /* where stdout waits while a builtin has it */

/* the status of the last command, which is what `$?` and `if` read */
static int s_i_status;
static bool s_b_interactive;
static bool s_b_leaving;
static long s_i_pgid;

static char s_arr_history[HISTORY_MAX][SH_LINE_MAX];
static int s_i_history;

/* a job: a pipeline's processes, one group, and what was typed to start it */
struct job {
    int  i_number;              /* 0 while the slot is free */
    long i_pgid;
    long arr_pids[SH_STAGES_MAX];   /* 0 once that one has ended */
    int  i_count;
    int  i_running;
    int  i_status;              /* the last stage's, which is the job's */
    bool b_stopped;
    char arr_text[JOB_TEXT_MAX];
};

static struct job s_arr_jobs[JOBS_MAX];
static int s_i_current;         /* the job `fg` and `bg` mean with no number */

static int run_line(const char *str_line);
static int run_file(const char *str_path);

/* $NAME from the environment, and $? from the shell itself */
static bool lookup(const char *str_name, char *str_out, size_t sz_size,
                   void *p_ctx)
{
    (void)p_ctx;
    if (strcmp(str_name, "?") == 0) {
        snprintf(str_out, sz_size, "%d", s_i_status);
        return true;
    }
    return getenv(str_name, str_out, (long)sz_size) >= 0;
}

static void var(const char *str_name, const char *str_default, char *str_out,
                size_t sz_size)
{
    if (getenv(str_name, str_out, (long)sz_size) < 0) {
        snprintf(str_out, sz_size, "%s", str_default);
    }
}

/*
 * where a program by this name is: a path with a `/` is itself, and a
 * bare name is looked for along PATH
 */
static bool find_program(const char *str_name, char *str_out)
{
    char arr_path[NAME_MAX];
    struct stat st;

    if (strchr(str_name, '/') != NULL) {
        snprintf(str_out, NAME_MAX, "%s", str_name);
        return stat(str_out, &st) == 0 && !st.is_dir;
    }
    var("PATH", PATH_DEFAULT, arr_path, sizeof arr_path);
    for (char *str_dir = arr_path; *str_dir != '\0';) {
        char *str_colon = strchr(str_dir, ':');
        if (str_colon != NULL) {
            *str_colon = '\0';
        }
        snprintf(str_out, NAME_MAX, "%s/%s", str_dir, str_name);
        if (stat(str_out, &st) == 0 && !st.is_dir) {
            return true;
        }
        if (str_colon == NULL) {
            break;
        }
        str_dir = str_colon + 1;
    }
    return false;
}

/* a file that starts with #! is commands for this shell, not a program */
static bool is_script(const char *str_path)
{
    char arr_head[2] = { 0, 0 };
    long i_fd = open(str_path);

    if (i_fd < 0) {
        return false;
    }
    read_fd(i_fd, arr_head, 2);
    close(i_fd);
    return arr_head[0] == '#' && arr_head[1] == '!';
}

/*
 * a file opened for a redirection: a descriptor, or -1 after saying
 * why. `>` starts the file again and `>>` adds to its end
 */
static long open_for(const char *str_path, bool b_write, bool b_append)
{
    struct stat st;
    bool b_there = stat(str_path, &st) == 0;

    if (!b_write) {
        long i_fd = open(str_path);
        if (i_fd < 0) {
            printf("%s: no such file to read from\n", str_path);
        }
        return i_fd;
    }
    if (b_there && st.is_dir) {
        printf("%s: that is a directory\n", str_path);
        return -1;
    }
    if (b_there && !b_append) {
        unlink(str_path);
    }
    long i_fd = create(str_path);
    if (i_fd < 0) {
        if (getuid() != 0) {
            printf("only the master may write files\n");
        } else {
            printf("%s: cannot write there, only the disk takes new "
                   "files\n", str_path);
        }
        return -1;
    }
    if (b_append) {
        lseek(i_fd, 0, SEEK_END);
    }
    return i_fd;
}

/* a stage's redirections, put in place in the process that is about to be it */
static bool redirect(const struct sh_stage *p_stage)
{
    if (p_stage->str_in != NULL) {
        long i_fd = open_for(p_stage->str_in, false, false);
        if (i_fd < 0) {
            return false;
        }
        dup2(i_fd, STDIN);
        close(i_fd);
    }
    if (p_stage->str_out != NULL) {
        long i_fd = open_for(p_stage->str_out, true, p_stage->b_append);
        if (i_fd < 0) {
            return false;
        }
        dup2(i_fd, STDOUT);
        close(i_fd);
    }
    return true;
}

/* jobs */

static void job_print(const struct job *p_job, const char *str_what)
{
    printf("[%d]%c %-8s %s\n", p_job->i_number,
           p_job->i_number == s_i_current ? '+' : ' ', str_what,
           p_job->arr_text);
}

static struct job *job_new(void)
{
    int i_number = 1;

    for (bool b_taken = true; b_taken; ) {
        b_taken = false;
        for (int i_at = 0; i_at < JOBS_MAX; i_at++) {
            if (s_arr_jobs[i_at].i_number == i_number) {
                i_number++;
                b_taken = true;
            }
        }
    }
    for (int i_at = 0; i_at < JOBS_MAX; i_at++) {
        if (s_arr_jobs[i_at].i_number == 0) {
            memset(&s_arr_jobs[i_at], 0, sizeof s_arr_jobs[i_at]);
            s_arr_jobs[i_at].i_number = i_number;
            return &s_arr_jobs[i_at];
        }
    }
    return NULL;
}

static void job_forget(struct job *p_job)
{
    int i_was = p_job->i_number;

    p_job->i_number = 0;
    if (s_i_current == i_was) {
        s_i_current = 0;
        for (int i_at = 0; i_at < JOBS_MAX; i_at++) {
            if (s_arr_jobs[i_at].i_number > s_i_current) {
                s_i_current = s_arr_jobs[i_at].i_number;
            }
        }
    }
}

static struct job *job_with(long i_pid, int *p_stage)
{
    for (int i_at = 0; i_at < JOBS_MAX; i_at++) {
        struct job *p_job = &s_arr_jobs[i_at];
        for (int i_s = 0; p_job->i_number != 0 && i_s < p_job->i_count;
             i_s++) {
            if (p_job->arr_pids[i_s] == i_pid) {
                *p_stage = i_s;
                return p_job;
            }
        }
    }
    return NULL;
}

/* what wait said about one of a job's processes */
static void job_heard(struct job *p_job, int i_stage, int i_status)
{
    if (i_status >= WAIT_STOPPED) {
        p_job->b_stopped = true;
        return;
    }
    p_job->arr_pids[i_stage] = 0;
    p_job->i_running--;
    if (i_stage == p_job->i_count - 1) {
        p_job->i_status = i_status;
    }
}

/*
 * wait for the job in front until every process in it has ended or it
 * has been stopped, which ctrl+z does to all of them at once
 */
static void job_wait(struct job *p_job)
{
    for (int i_s = 0; i_s < p_job->i_count && !p_job->b_stopped; i_s++) {
        int i_status = 0;
        if (p_job->arr_pids[i_s] == 0) {
            continue;
        }
        if (waitpid(p_job->arr_pids[i_s], &i_status, WUNTRACED) < 0) {
            p_job->arr_pids[i_s] = 0;
            p_job->i_running--;
            continue;
        }
        job_heard(p_job, i_s, i_status);
    }
    tcsetpgrp(s_i_pgid);

    if (p_job->b_stopped) {
        s_i_current = p_job->i_number;
        printf("\n");
        job_print(p_job, "stopped");
        return;
    }
    s_i_status = p_job->i_status;
    job_forget(p_job);
}

/* anything in the background that ended or stopped while nobody was looking */
static void jobs_reap(void)
{
    for (;;) {
        int i_status = 0;
        int i_stage = 0;
        long i_pid = waitpid(-1, &i_status, WNOHANG | WUNTRACED);
        if (i_pid <= 0) {
            return;
        }
        struct job *p_job = job_with(i_pid, &i_stage);
        if (p_job == NULL) {
            continue;
        }
        bool b_was = p_job->b_stopped;
        job_heard(p_job, i_stage, i_status);
        if (p_job->i_running == 0) {
            job_print(p_job, "done");
            job_forget(p_job);
        } else if (p_job->b_stopped && !b_was) {
            job_print(p_job, "stopped");
        }
    }
}

/*
 * the child's half of a stage: its group, its descriptors, and the
 * program. a job in front waits on `arr_go` until the shell has given it
 * the terminal, which only the shell may do, so that the program's first
 * read of the keyboard is never made from behind
 */
static void become(const struct sh_stage *p_stage, const char *str_path,
                   long i_pgid, int32_t arr_go[2], long i_in,
                   int32_t arr_out[2])
{
    char c_go;

    setpgid(0, i_pgid);
    if (arr_go[0] >= 0) {
        close(arr_go[1]);
        read_fd(arr_go[0], &c_go, 1);   /* 0 bytes, once the shell lets go */
        close(arr_go[0]);
    }
    signal_default(SIGINT);
    signal_default(SIGTSTP);

    if (i_in >= 0) {
        dup2(i_in, STDIN);
        close(i_in);
    }
    if (arr_out[1] >= 0) {
        dup2(arr_out[1], STDOUT);
        close(arr_out[1]);
        close(arr_out[0]);
    }
    if (!redirect(p_stage)) {
        exit(1);
    }
    if (is_script(str_path)) {
        exit(run_file(str_path));
    }
    execv(str_path, p_stage->arr_argv);
    printf("%s: cannot run it\n", str_path);
    exit(126);
}

/* a pipeline as a job: one process per stage, all in one group */
static int run_pipeline(const struct sh_command *p_cmd, const char *str_text)
{
    char arr_paths[SH_STAGES_MAX][NAME_MAX];

    for (int i_s = 0; i_s < p_cmd->i_stages; i_s++) {
        if (!find_program(p_cmd->arr_stages[i_s].arr_argv[0],
                          arr_paths[i_s])) {
            printf("%s: nothing by that name, `help` lists what there is\n",
                   p_cmd->arr_stages[i_s].arr_argv[0]);
            return 127;
        }
    }

    struct job *p_job = job_new();
    if (p_job == NULL) {
        printf("too many jobs already. `jobs` lists them\n");
        return 1;
    }
    snprintf(p_job->arr_text, JOB_TEXT_MAX, "%s", str_text);
    bool b_front = s_b_interactive && !p_cmd->b_background;
    int32_t arr_go[2] = { -1, -1 };
    long i_in = -1;

    if (b_front && pipe(arr_go) != 0) {
        arr_go[0] = arr_go[1] = -1;
    }

    for (int i_s = 0; i_s < p_cmd->i_stages; i_s++) {
        int32_t arr_out[2] = { -1, -1 };
        if (i_s + 1 < p_cmd->i_stages && pipe(arr_out) != 0) {
            printf("no pipe for that\n");
            break;
        }
        long i_pid = fork();
        if (i_pid == 0) {
            become(&p_cmd->arr_stages[i_s], arr_paths[i_s], p_job->i_pgid,
                   arr_go, i_in, arr_out);
        }
        if (i_pid < 0) {
            printf("no room for another process\n");
            break;
        }
        if (p_job->i_pgid == 0) {
            p_job->i_pgid = i_pid;
        }
        setpgid(i_pid, p_job->i_pgid);
        p_job->arr_pids[p_job->i_count++] = i_pid;
        p_job->i_running++;
        if (i_in >= 0) {
            close(i_in);
        }
        if (arr_out[1] >= 0) {
            close(arr_out[1]);
        }
        i_in = arr_out[0];
    }
    if (i_in >= 0) {
        close(i_in);
    }
    if (b_front && p_job->i_count > 0) {
        tcsetpgrp(p_job->i_pgid);
    }
    if (arr_go[0] >= 0) {
        close(arr_go[0]);
        close(arr_go[1]);           /* and the job goes */
    }

    if (p_job->i_count == 0) {
        job_forget(p_job);
        return 1;
    }
    if (p_cmd->b_background) {
        s_i_current = p_job->i_number;
        job_print(p_job, "running");
        return 0;
    }
    job_wait(p_job);
    return s_i_status;
}

/* builtins */

static struct job *job_named(int i_argc, char **pp_argv, const char *str_who)
{
    int i_number = s_i_current;

    if (i_argc > 1) {
        const char *str_n = pp_argv[1] + (pp_argv[1][0] == '%');
        i_number = atoi(str_n);
        if (i_number <= 0) {
            printf("%s <number>, `jobs` lists them\n", str_who);
            return NULL;
        }
    }
    if (i_number == 0) {
        printf("nothing is waiting\n");
        return NULL;
    }
    for (int i_at = 0; i_at < JOBS_MAX; i_at++) {
        if (s_arr_jobs[i_at].i_number == i_number) {
            return &s_arr_jobs[i_at];
        }
    }
    printf("there is no job %d\n", i_number);
    return NULL;
}

static int cmd_jobs(int i_argc, char **pp_argv)
{
    int i_shown = 0;

    (void)i_argc;
    (void)pp_argv;
    for (int i_at = 0; i_at < JOBS_MAX; i_at++) {
        if (s_arr_jobs[i_at].i_number != 0) {
            job_print(&s_arr_jobs[i_at],
                      s_arr_jobs[i_at].b_stopped ? "stopped" : "running");
            i_shown++;
        }
    }
    if (i_shown == 0) {
        printf("nothing is waiting\n");
    }
    return 0;
}

static int cmd_fg(int i_argc, char **pp_argv)
{
    struct job *p_job = job_named(i_argc, pp_argv, "fg");

    if (p_job == NULL) {
        return 1;
    }
    printf("%s\n", p_job->arr_text);
    s_i_current = p_job->i_number;
    p_job->b_stopped = false;
    tcsetpgrp(p_job->i_pgid);
    sigsend(-p_job->i_pgid, SIGCONT);
    job_wait(p_job);
    return s_i_status;
}

static int cmd_bg(int i_argc, char **pp_argv)
{
    struct job *p_job = job_named(i_argc, pp_argv, "bg");

    if (p_job == NULL) {
        return 1;
    }
    if (!p_job->b_stopped) {
        printf("[%d] is already running\n", p_job->i_number);
        return 1;
    }
    s_i_current = p_job->i_number;
    p_job->b_stopped = false;
    sigsend(-p_job->i_pgid, SIGCONT);
    job_print(p_job, "running");
    return 0;
}

static int cmd_cd(int i_argc, char **pp_argv)
{
    const char *str_to = (i_argc > 1) ? pp_argv[1] : "/";
    struct stat st;

    if (stat(str_to, &st) != 0) {
        printf("%s: no such place\n", str_to);
        return 1;
    }
    if (!st.is_dir) {
        printf("%s is a file, not somewhere to stand\n", str_to);
        return 1;
    }
    return chdir(str_to) == 0 ? 0 : 1;
}

static int cmd_pwd(int i_argc, char **pp_argv)
{
    char arr_cwd[NAME_MAX];

    (void)i_argc;
    (void)pp_argv;
    getcwd(arr_cwd, sizeof arr_cwd);
    printf("%s\n", arr_cwd);
    return 0;
}

/* every variable this process holds, as /proc/environ lists them */
static int list_variables(void)
{
    static char s_arr_env[4096];
    long i_fd = open("/proc/environ");
    long i_len = (i_fd >= 0) ? read_fd(i_fd, s_arr_env, sizeof s_arr_env)
                             : -1;
    int i_shown = 0;

    if (i_fd >= 0) {
        close(i_fd);
    }
    for (long i_at = 0; i_at < i_len; i_at += (long)strlen(s_arr_env + i_at)
                                               + 1) {
        printf("%s\n", s_arr_env + i_at);
        i_shown++;
    }
    if (i_shown == 0) {
        printf("nothing is set. `set NAME value`\n");
    }
    return 0;
}

static int cmd_set(int i_argc, char **pp_argv)
{
    char arr_value[NAME_MAX];

    if (i_argc == 1) {
        return list_variables();
    }
    if (i_argc == 2) {
        if (getenv(pp_argv[1], arr_value, sizeof arr_value) < 0) {
            printf("%s is not set\n", pp_argv[1]);
            return 1;
        }
        printf("%s\n", arr_value);
        return 0;
    }
    /* everything after the name is the value, one space between words */
    size_t sz_len = 0;
    arr_value[0] = '\0';
    for (int i_at = 2; i_at < i_argc && sz_len < sizeof arr_value; i_at++) {
        sz_len += (size_t)snprintf(arr_value + sz_len,
                                   sizeof arr_value - sz_len, "%s%s",
                                   i_at > 2 ? " " : "", pp_argv[i_at]);
    }
    return setenv(pp_argv[1], arr_value) == 0 ? 0 : 1;
}

static int cmd_unset(int i_argc, char **pp_argv)
{
    for (int i_at = 1; i_at < i_argc; i_at++) {
        unsetenv(pp_argv[i_at]);
    }
    return 0;
}

static int cmd_env(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;
    return list_variables();
}

static long number_of(const char *str_word)
{
    long i_n = 0;

    for (; *str_word >= '0' && *str_word <= '9'; str_word++) {
        i_n = i_n * 10 + (*str_word - '0');
    }
    return i_n;
}

/* the rules the kernel's shell had, which scripts already rely on */
static bool test_says_yes(int i_argc, char **pp_argv)
{
    struct stat st;

    if (i_argc == 2) {
        return pp_argv[1][0] != '\0';
    }
    if (i_argc == 3) {
        const char *str_op = pp_argv[1];
        bool b_there = stat(pp_argv[2], &st) == 0;
        return strcmp(str_op, "-n") == 0   ? pp_argv[2][0] != '\0'
             : strcmp(str_op, "-z") == 0   ? pp_argv[2][0] == '\0'
             : strcmp(str_op, "-e") == 0   ? b_there
             : strcmp(str_op, "-f") == 0   ? b_there && !st.is_dir
             : strcmp(str_op, "-d") == 0   ? b_there && st.is_dir
             : false;
    }
    if (i_argc == 4) {
        const char *str_op = pp_argv[2];
        long i_a = number_of(pp_argv[1]);
        long i_b = number_of(pp_argv[3]);
        return strcmp(str_op, "=") == 0   ? strcmp(pp_argv[1], pp_argv[3]) == 0
             : strcmp(str_op, "!=") == 0  ? strcmp(pp_argv[1], pp_argv[3]) != 0
             : strcmp(str_op, "-eq") == 0 ? i_a == i_b
             : strcmp(str_op, "-ne") == 0 ? i_a != i_b
             : strcmp(str_op, "-lt") == 0 ? i_a < i_b
             : strcmp(str_op, "-gt") == 0 ? i_a > i_b
             : strcmp(str_op, "-le") == 0 ? i_a <= i_b
             : strcmp(str_op, "-ge") == 0 ? i_a >= i_b
             : false;
    }
    return false;
}

static int cmd_test(int i_argc, char **pp_argv)
{
    if (i_argc < 2) {
        printf("test <word>            true if it is not empty\n"
               "test -n/-z <word>      not empty / empty\n"
               "test -e/-f/-d <path>   exists / is a file / is a "
               "directory\n"
               "test <a> = / != <b>    the same text, or not\n"
               "test <a> -eq -ne -lt -gt -le -ge <b>   as numbers\n"
               "\nit says nothing and sets $?, which is what `if` reads\n");
        return 2;
    }
    return test_says_yes(i_argc, pp_argv) ? 0 : 1;
}

static int cmd_source(int i_argc, char **pp_argv)
{
    if (i_argc < 2) {
        printf("source <file>\n");
        return 2;
    }
    return run_file(pp_argv[1]);
}

static int cmd_history(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;
    for (int i_at = 0; i_at < s_i_history; i_at++) {
        printf("  %2d  %s\n", i_at + 1, s_arr_history[i_at]);
    }
    return 0;
}

static int cmd_clear(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;
    return clear_screen() == 0 ? 0 : 1;
}

static int cmd_exit(int i_argc, char **pp_argv)
{
    s_b_leaving = true;
    return (i_argc > 1) ? atoi(pp_argv[1]) : s_i_status;
}

static int cmd_logout(int i_argc, char **pp_argv)
{
    char arr_user[NAME_MAX];

    (void)i_argc;
    (void)pp_argv;
    var("USER", "thee", arr_user, sizeof arr_user);
    printf("fare thee well, %s\n", arr_user);
    s_b_leaving = true;
    return 0;
}

static int cmd_help(int i_argc, char **pp_argv);
static int cmd_run(int i_argc, char **pp_argv);

struct builtin {
    const char *str_name;
    const char *str_help;
    int (*p_fn)(int i_argc, char **pp_argv);
};

static const struct builtin s_arr_builtins[] = {
    { "help",    "list what thou may command",              cmd_help },
    { "cd",      "go somewhere; no argument means the root", cmd_cd },
    { "pwd",     "where the shell is standing",              cmd_pwd },
    { "run",     "run a program by its path; & for background", cmd_run },
    { "jobs",    "what thou hast set aside",                 cmd_jobs },
    { "fg",      "bring one back to the front",              cmd_fg },
    { "bg",      "let one carry on without the keyboard",    cmd_bg },
    { "set",     "a variable, or all of them",               cmd_set },
    { "unset",   "forget one",                               cmd_unset },
    { "env",     "what is set, and inherited by what runs",  cmd_env },
    { "test",    "answer a question in $?, for `if`",        cmd_test },
    { "source",  "run a file of commands in this shell",     cmd_source },
    { "history", "what thou hast said before",               cmd_history },
    { "clear",   "wipe the screen clean",                    cmd_clear },
    { "exit",    "leave this shell",                         cmd_exit },
    { "logout",  "leave, and let somebody else in",          cmd_logout },
};

#define BUILTINS (sizeof s_arr_builtins / sizeof s_arr_builtins[0])

static const struct builtin *builtin_named(const char *str_name)
{
    for (size_t sz_at = 0; sz_at < BUILTINS; sz_at++) {
        if (strcmp(s_arr_builtins[sz_at].str_name, str_name) == 0) {
            return &s_arr_builtins[sz_at];
        }
    }
    return NULL;
}

static void column(const char *str_name, int *p_n, int i_per_row)
{
    printf(" %-11s", str_name);
    if (++*p_n % i_per_row == 0) {
        printf("\n ");
    }
}

static int cmd_help(int i_argc, char **pp_argv)
{
    char arr_path[NAME_MAX];
    char arr_name[NAME_MAX];
    unsigned u_cols = 80;
    unsigned u_rows = 0;
    int i_n = 0;

    if (i_argc > 1) {
        const struct builtin *p_b = builtin_named(pp_argv[1]);
        if (p_b != NULL) {
            printf("%s: %s\n", p_b->str_name, p_b->str_help);
            return 0;
        }
        /* a program's own answer, which cannot be out of date */
        snprintf(arr_path, sizeof arr_path, "%s --help", pp_argv[1]);
        return run_line(arr_path);
    }

    winsize(&u_cols, &u_rows);
    int i_per_row = (u_cols > 20) ? (int)(u_cols - 4) / 12 : 4;
    printf("built in\n ");
    for (size_t sz_at = 0; sz_at < BUILTINS; sz_at++) {
        column(s_arr_builtins[sz_at].str_name, &i_n, i_per_row);
    }
    printf("\n\nprograms, in ring 3 with memory of their own\n ");
    i_n = 0;
    var("PATH", PATH_DEFAULT, arr_path, sizeof arr_path);
    for (char *str_dir = arr_path; str_dir != NULL;) {
        char *str_colon = strchr(str_dir, ':');
        if (str_colon != NULL) {
            *str_colon = '\0';
        }
        for (long i_at = 0;
             readdir_at(i_at, arr_name, sizeof arr_name, str_dir) >= 0;
             i_at++) {
            if (arr_name[strlen(arr_name) - 1] != '/') {
                column(arr_name, &i_n, i_per_row);
            }
        }
        str_dir = (str_colon != NULL) ? str_colon + 1 : NULL;
    }
    printf("\n\n`help <name>` for what one takes, and for a program that "
           "answer\ncomes from the program itself, so it cannot be out of "
           "date.\n");
    var("PATH", PATH_DEFAULT, arr_path, sizeof arr_path);
    printf("looked for in %s\n", arr_path);
    return 0;
}

/* a builtin, with its output wherever the line sent it */
static int run_builtin(const struct builtin *p_b, struct sh_stage *p_stage)
{
    bool b_moved = p_stage->str_in != NULL || p_stage->str_out != NULL;
    int i_status;

    if (b_moved) {
        dup2(STDIN, SAVED_FD - 1);
        dup2(STDOUT, SAVED_FD);
        if (!redirect(p_stage)) {
            dup2(SAVED_FD - 1, STDIN);
            dup2(SAVED_FD, STDOUT);
            close(SAVED_FD - 1);
            close(SAVED_FD);
            return 1;
        }
    }
    i_status = p_b->p_fn(p_stage->i_argc, p_stage->arr_argv);
    if (b_moved) {
        dup2(SAVED_FD - 1, STDIN);
        dup2(SAVED_FD, STDOUT);
        close(SAVED_FD - 1);
        close(SAVED_FD);
    }
    return i_status;
}

/*
 * `run` on its own. with something after it, the word is dropped before
 * anything is run, see run_line(), so this only ever says how it is used
 */
static int cmd_run(int i_argc, char **pp_argv)
{
    (void)i_argc;
    (void)pp_argv;
    printf("run <program> [arguments] [&]\n");
    return 2;
}

/* one line: a builtin, run here, or a pipeline of programs */
static int run_line(const char *str_line)
{
    static char s_arr_store[SH_LINE_MAX * 2];
    struct sh_command cmd;
    char arr_text[SH_LINE_MAX];

    snprintf(arr_text, sizeof arr_text, "%s", str_line);
    char *str_nl = strchr(arr_text, '\n');
    if (str_nl != NULL) {
        *str_nl = '\0';
    }
    const char *str_error = sh_parse(arr_text, &cmd, s_arr_store,
                                     sizeof s_arr_store, lookup, NULL);
    if (str_error != NULL) {
        printf("%s\n", str_error);
        s_i_status = 2;
        return s_i_status;
    }
    if (cmd.i_stages == 0) {
        return s_i_status;
    }

    /*
     * `run prog args &` is the kernel shell's way of saying what a name
     * says now: the word goes, and what is left is the command, & and
     * all, which a builtin taking the line could not have run behind
     */
    struct sh_stage *p_first = &cmd.arr_stages[0];
    if (strcmp(p_first->arr_argv[0], "run") == 0 && p_first->i_argc > 1) {
        memmove(p_first->arr_argv, p_first->arr_argv + 1,
                sizeof p_first->arr_argv[0] * (size_t)p_first->i_argc);
        p_first->i_argc--;
    }

    const struct builtin *p_b = builtin_named(cmd.arr_stages[0].arr_argv[0]);
    if (p_b != NULL && cmd.i_stages == 1 && !cmd.b_background) {
        s_i_status = run_builtin(p_b, &cmd.arr_stages[0]);
        return s_i_status;
    }
    for (int i_s = 0; i_s < cmd.i_stages; i_s++) {
        if (builtin_named(cmd.arr_stages[i_s].arr_argv[0]) != NULL) {
            printf("%s is built in, and cannot be part of a pipeline or "
                   "run in the background\n",
                   cmd.arr_stages[i_s].arr_argv[0]);
            s_i_status = 2;
            return s_i_status;
        }
    }
    s_i_status = run_pipeline(&cmd, arr_text);
    return s_i_status;
}

/* scripts: lines, with if, else, while and end around some of them */

static void first_word(const char *str_line, char *str_out, size_t sz_size)
{
    size_t sz_n = 0;

    while (*str_line == ' ' || *str_line == '\t') {
        str_line++;
    }
    while (*str_line != '\0' && *str_line != ' ' && *str_line != '\t'
           && *str_line != '\n' && sz_n + 1 < sz_size) {
        str_out[sz_n++] = *str_line++;
    }
    str_out[sz_n] = '\0';
}

/* what follows the first word: for `if` and `while`, what they ask */
static const char *after_first(const char *str_line)
{
    while (*str_line == ' ' || *str_line == '\t') {
        str_line++;
    }
    while (*str_line != '\0' && *str_line != ' ' && *str_line != '\t') {
        str_line++;
    }
    return str_line;
}

/* how a line changes the depth of blocks: 1 opens one, -1 closes one */
static int depth_change(const char *str_line)
{
    char arr_word[16];

    first_word(str_line, arr_word, sizeof arr_word);
    if (strcmp(arr_word, "if") == 0 || strcmp(arr_word, "while") == 0) {
        return 1;
    }
    return strcmp(arr_word, "end") == 0 ? -1 : 0;
}

/*
 * the line after the end of the block that line `i_from` is inside, or
 * the `else` that belongs to it when `b_else` asks for one
 */
static int skip_block(char **pp_lines, int i_count, int i_from, bool b_else)
{
    int i_depth = 0;
    char arr_word[16];

    for (int i_at = i_from + 1; i_at < i_count; i_at++) {
        first_word(pp_lines[i_at], arr_word, sizeof arr_word);
        if (i_depth == 0 && strcmp(arr_word, "end") == 0) {
            return i_at;
        }
        if (i_depth == 0 && b_else && strcmp(arr_word, "else") == 0) {
            return i_at;
        }
        i_depth += depth_change(pp_lines[i_at]);
    }
    return i_count;
}

struct block {
    bool b_while;
    int  i_at;          /* the line the block began on */
};

static int run_lines(char **pp_lines, int i_count, const char *str_name)
{
    struct block arr_blocks[DEPTH_MAX];
    int i_depth = 0;
    char arr_word[16];
    long i_steps = 0;

    for (int i_at = 0; i_at < i_count && !s_b_leaving; i_at++) {
        if (++i_steps > STEPS_MAX) {
            printf("%s: stopped after %d steps. if that was on purpose, "
                   "it needs to be shorter\n", str_name, STEPS_MAX);
            return 1;
        }
        first_word(pp_lines[i_at], arr_word, sizeof arr_word);

        if (strcmp(arr_word, "if") == 0 || strcmp(arr_word, "while") == 0) {
            if (i_depth == DEPTH_MAX) {
                printf("%s: blocks nested deeper than %d\n", str_name,
                       DEPTH_MAX);
                return 1;
            }
            bool b_yes = run_line(after_first(pp_lines[i_at])) == 0;
            if (!b_yes) {
                i_at = skip_block(pp_lines, i_count, i_at, arr_word[0] == 'i');
                first_word(i_at < i_count ? pp_lines[i_at] : "", arr_word,
                           sizeof arr_word);
                if (strcmp(arr_word, "else") == 0) {
                    arr_blocks[i_depth++] = (struct block){ false, i_at };
                }
                continue;
            }
            arr_blocks[i_depth++] = (struct block){ arr_word[0] == 'w', i_at };
        } else if (strcmp(arr_word, "else") == 0) {
            /* reached from a branch that ran, so the else does not */
            i_at = skip_block(pp_lines, i_count, i_at, false);
            i_depth -= (i_depth > 0);
        } else if (strcmp(arr_word, "end") == 0) {
            if (i_depth == 0) {
                printf("%s: an end with nothing to end\n", str_name);
                return 1;
            }
            struct block *p_b = &arr_blocks[--i_depth];
            if (p_b->b_while) {
                i_at = p_b->i_at - 1;   /* and ask again */
            }
        } else {
            run_line(pp_lines[i_at]);
        }
    }
    return s_i_status;
}

/* a file of commands, run in this shell */
static int run_file(const char *str_path)
{
    long i_fd = open(str_path);

    if (i_fd < 0) {
        printf("%s: no such file\n", str_path);
        return 127;
    }

    /* buffers of its own, since a script may source another */
    char *str_text = malloc(SCRIPT_MAX);
    char **pp_lines = malloc(sizeof *pp_lines * LINES_MAX);
    if (str_text == NULL || pp_lines == NULL) {
        close(i_fd);
        free(str_text);
        free(pp_lines);
        printf("%s: no memory to read it into\n", str_path);
        return 1;
    }
    long i_len = read_fd(i_fd, str_text, SCRIPT_MAX - 1);
    close(i_fd);
    str_text[i_len > 0 ? i_len : 0] = '\0';

    int i_count = 0;
    for (char *str_line = str_text; *str_line != '\0' && i_count < LINES_MAX;) {
        pp_lines[i_count++] = str_line;
        char *str_nl = strchr(str_line, '\n');
        if (str_nl == NULL) {
            break;
        }
        *str_nl = '\0';
        str_line = str_nl + 1;
    }
    int i_status = run_lines(pp_lines, i_count, str_path);
    free(pp_lines);
    free(str_text);
    return i_status;
}

/* the terminal */

static void prompt(void)
{
    char arr_user[NAME_MAX];
    char arr_tty[8];
    char arr_cwd[NAME_MAX];

    var("USER", "someone", arr_user, sizeof arr_user);
    var("TTY", "?", arr_tty, sizeof arr_tty);
    getcwd(arr_cwd, sizeof arr_cwd);
    printf("%s@velvet[%s]:%s%s ", arr_user, arr_tty, arr_cwd,
           getuid() == 0 ? "#" : "$");
}

static void remember(const char *str_line)
{
    if (str_line[0] == '\0' || str_line[0] == '\n') {
        return;
    }
    if (s_i_history == HISTORY_MAX) {
        memmove(s_arr_history[0], s_arr_history[1],
                sizeof s_arr_history[0] * (HISTORY_MAX - 1));
        s_i_history--;
    }
    snprintf(s_arr_history[s_i_history], SH_LINE_MAX, "%s", str_line);
    char *str_nl = strchr(s_arr_history[s_i_history], '\n');
    if (str_nl != NULL) {
        *str_nl = '\0';
    }
    s_i_history++;
}

/* a line typed at the terminal, or -1 at the end of input */
static long read_line(char *str_line, size_t sz_size)
{
    for (;;) {
        long i_got = read_fd(STDIN, str_line, (long)sz_size - 1);
        if (i_got == 0) {
            return -1;
        }
        if (i_got > 0) {
            str_line[i_got] = '\0';
            return i_got;
        }
        /*
         * interrupted, which is ctrl+c at the prompt: a fresh one. a
         * moment's pause first, so a read that keeps failing for some
         * other reason does not print prompts as fast as it can
         */
        sleep(50);
        str_line[0] = '\0';
        return 0;
    }
}

/*
 * a block typed at the prompt is read to its end before any of it runs,
 * since a while has to be able to go back to its start
 */
static void run_typed_block(char *str_first)
{
    static char s_arr_block[LINES_MAX][SH_LINE_MAX];
    static char *s_arr_ptrs[LINES_MAX];
    int i_count = 0;
    int i_depth = 0;

    for (char *str_line = str_first; ; ) {
        snprintf(s_arr_block[i_count], SH_LINE_MAX, "%s", str_line);
        s_arr_ptrs[i_count] = s_arr_block[i_count];
        i_depth += depth_change(str_line);
        i_count++;
        if (i_depth <= 0 || i_count == LINES_MAX) {
            break;
        }
        printf("> ");
        if (read_line(str_line, SH_LINE_MAX) < 0) {
            break;
        }
    }
    run_lines(s_arr_ptrs, i_count, "the block");
}

static void read_profile(void)
{
    static const char *const s_arr_places[] = {
        "/etc/profile",         /* on the disk, if somebody has put one there */
        "/boot/etc/profile",    /* and the one that ships with the machine */
    };
    struct stat st;

    for (size_t sz_at = 0; sz_at < 2; sz_at++) {
        if (stat(s_arr_places[sz_at], &st) == 0 && !st.is_dir) {
            run_file(s_arr_places[sz_at]);
            return;
        }
    }
}

int main(int i_argc, char **pp_argv)
{
    char arr_line[SH_LINE_MAX];

    if (i_argc > 1) {
        return run_file(pp_argv[1]);
    }

    s_b_interactive = true;
    setpgid(0, 0);
    s_i_pgid = getpid();
    tcsetpgrp(s_i_pgid);
    signal_ignore(SIGINT);
    signal_ignore(SIGTSTP);
    read_profile();

    while (!s_b_leaving) {
        jobs_reap();
        prompt();
        if (read_line(arr_line, sizeof arr_line) < 0) {
            printf("\n");
            break;
        }
        remember(arr_line);
        if (depth_change(arr_line) > 0) {
            run_typed_block(arr_line);
        } else {
            run_line(arr_line);
        }
    }
    return s_i_status;
}
