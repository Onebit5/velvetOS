// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/syscall.h
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the user side of the door.
 */

#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include <stdint.h>
#include <stddef.h>

/*
 * the numbers, and the structs and flags the calls take, are the
 * kernel's, in its include/uapi/velvet/, and are included rather than
 * copied. written out twice, a call inserted in the middle of either
 * list renumbered every call after it, and a program then asked the
 * kernel for something else entirely rather than failing
 */
#include <uapi/velvet/syscall.h>
#include <uapi/velvet/signal.h>
#include <uapi/velvet/input.h>
#include <uapi/velvet/tty.h>
#include <uapi/velvet/fsck.h>

#define STDIN   0
#define STDOUT  1
#define STDERR  2

static inline long syscall3(long nr, long a0, long a1, long a2)
{
    long ret;
    __asm__ volatile ("syscall"
                      : "=a"(ret)
                      : "a"(nr), "D"(a0), "S"(a1), "d"(a2)
                      : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall5(long nr, long a0, long a1, long a2,
                            long a3, long a4)
{
    long ret;
    register long r10 __asm__("r10") = a3;
    register long r8  __asm__("r8")  = a4;
    __asm__ volatile ("syscall"
                      : "=a"(ret)
                      : "a"(nr), "D"(a0), "S"(a1), "d"(a2), "r"(r10), "r"(r8)
                      : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall4(long nr, long a0, long a1, long a2, long a3)
{
    return syscall5(nr, a0, a1, a2, a3, 0);
}
static inline long syscall2(long nr, long a0, long a1)
{
    return syscall3(nr, a0, a1, 0);
}
static inline long syscall1(long nr, long a0)
{
    return syscall3(nr, a0, 0, 0);
}
static inline long syscall0(long nr)
{
    return syscall3(nr, 0, 0, 0);
}

static inline size_t ustrlen(const char *s)
{
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static inline long write_fd(long fd, const void *buf, long len)
{
    return syscall3(SYS_WRITE, fd, (long)buf, len);
}

static inline void write(const char *s)
{
    write_fd(STDOUT, s, (long)ustrlen(s));
}

static inline long read_fd(long fd, void *buf, long len)
{
    return syscall3(SYS_READ, fd, (long)buf, len);
}

static inline long open(const char *path)
{
    return syscall2(SYS_OPEN, (long)path, (long)ustrlen(path));
}
static inline long close(long fd)
{
    return syscall1(SYS_CLOSE, fd);
}

static inline long readdir(long n, char *buf, long len)
{
    return syscall3(SYS_READDIR, n, (long)buf, len);
}

static inline long readdir_at(long n, char *buf, long len, const char *path)
{
    return syscall5(SYS_READDIR, n, (long)buf, len,
                    (long)path, (long)ustrlen(path));
}

static inline long create(const char *path)
{
    return syscall2(SYS_CREATE, (long)path, (long)ustrlen(path));
}

static inline long chdir(const char *path)
{
    return syscall2(SYS_CHDIR, (long)path, (long)ustrlen(path));
}
static inline long getcwd(char *buf, long len)
{
    return syscall2(SYS_GETCWD, (long)buf, len);
}
static inline long mkdir(const char *path)
{
    return syscall2(SYS_MKDIR, (long)path, (long)ustrlen(path));
}
static inline long rmdir(const char *path)
{
    return syscall2(SYS_RMDIR, (long)path, (long)ustrlen(path));
}

static inline long unlink(const char *path)
{
    return syscall2(SYS_UNLINK, (long)path, (long)ustrlen(path));
}

static inline long rename(const char *from, const char *to)
{
    return syscall5(SYS_RENAME, (long)from, (long)ustrlen(from),
                    (long)to, (long)ustrlen(to), 0);
}

struct stat {
    uint64_t size;
    uint32_t mode;
    uint32_t is_dir;
    uint32_t uid, gid;
    uint32_t is_symlink;
    uint16_t year;
    uint8_t  month, day, hour, minute, second;
    uint8_t  pad;
};

/*
 * struct user_stat, under the name a program expects. a struct of its
 * own so that `struct stat` means here what it means everywhere else,
 * and held to the kernel's layout field by field, so that the two
 * cannot drift apart without the build saying so
 */
#define SAME_AS_KERNEL(ours, theirs, f) \
    _Static_assert(offsetof(struct ours, f) == offsetof(struct theirs, f) \
                   && sizeof(((struct ours *)0)->f) \
                      == sizeof(((struct theirs *)0)->f), \
                   "struct " #ours " and struct " #theirs " disagree on " #f)
_Static_assert(sizeof(struct stat) == sizeof(struct user_stat),
               "struct stat and struct user_stat are not the same size");
SAME_AS_KERNEL(stat, user_stat, size);
SAME_AS_KERNEL(stat, user_stat, mode);
SAME_AS_KERNEL(stat, user_stat, is_dir);
SAME_AS_KERNEL(stat, user_stat, uid);
SAME_AS_KERNEL(stat, user_stat, gid);
SAME_AS_KERNEL(stat, user_stat, is_symlink);
SAME_AS_KERNEL(stat, user_stat, year);
SAME_AS_KERNEL(stat, user_stat, month);
SAME_AS_KERNEL(stat, user_stat, day);
SAME_AS_KERNEL(stat, user_stat, hour);
SAME_AS_KERNEL(stat, user_stat, minute);
SAME_AS_KERNEL(stat, user_stat, second);

static inline long stat(const char *path, struct stat *out)
{
    return syscall3(SYS_STAT, (long)path, (long)ustrlen(path), (long)out);
}

static inline long chmod(const char *path, long mode)
{
    return syscall3(SYS_CHMOD, (long)path, (long)ustrlen(path), mode);
}
static inline long chown(const char *path, long uid, long gid)
{
    return syscall5(SYS_CHOWN, (long)path, (long)ustrlen(path), uid, gid, 0);
}

static inline long symlink(const char *path, const char *target)
{
    return syscall5(SYS_SYMLINK, (long)path, (long)ustrlen(path),
                    (long)target, (long)ustrlen(target), 0);
}
static inline long readlink(const char *path, char *out, long size)
{
    return syscall5(SYS_READLINK, (long)path, (long)ustrlen(path),
                    (long)out, size, 0);
}

static inline long getenv(const char *name, char *out, long size)
{
    return syscall5(SYS_GETENV, (long)name, (long)ustrlen(name),
                    (long)out, size, 0);
}

static inline long setenv(const char *name, const char *value)
{
    return syscall5(SYS_SETENV, (long)name, (long)ustrlen(name),
                    (long)value, (long)ustrlen(value), 0);
}

static inline long unsetenv(const char *name)
{
    return syscall5(SYS_SETENV, (long)name, (long)ustrlen(name), 0, 0, 0);
}

static inline long getpid(void)
{
    return syscall0(SYS_GETPID);
}
static inline long getuid(void)
{
    return syscall0(SYS_GETUID);
}

static inline long fork(void)
{
    return syscall0(SYS_FORK);
}

static inline void *mmap(long len)
{
    return (void *)syscall1(SYS_MMAP, len);
}

static inline long munmap(void *at)
{
    return syscall1(SYS_MUNMAP, (long)at);
}

static inline long spawn(const char *path)
{
    return syscall2(SYS_SPAWN, (long)path, (long)ustrlen(path));
}

static inline long wait(long pid, int *code)
{
    return syscall2(SYS_WAIT, pid, (long)code);
}

static inline long getkey(void)
{
    return syscall0(SYS_GETKEY);
}

static inline long screen_size(uint32_t *cols, uint32_t *rows)
{
    return syscall2(SYS_SCREEN, (long)cols, (long)rows);
}
static inline long cursor_to(long col, long row)
{
    return syscall2(SYS_CURSOR, col, row);
}
static inline long clear_screen(void)
{
    return syscall0(SYS_CLEAR);
}

static inline long uptime(void)
{
    return syscall0(SYS_UPTIME);
}
static inline void yield(void)
{
    syscall0(SYS_YIELD);
}

static inline long sleep(long ms)
{
    return syscall1(SYS_SLEEP, ms);
}
static inline void exit(long code)
{
    syscall1(SYS_EXIT, code); __builtin_unreachable();
}

struct from {
    unsigned int   address;
    unsigned short port;
    unsigned short reserved;
};

/* and struct user_from, the same way */
_Static_assert(sizeof(struct from) == sizeof(struct user_from),
               "struct from and struct user_from are not the same size");
SAME_AS_KERNEL(from, user_from, address);
SAME_AS_KERNEL(from, user_from, port);
SAME_AS_KERNEL(from, user_from, reserved);
#undef SAME_AS_KERNEL

static inline long socket(unsigned short port)
{
    return syscall1(SYS_SOCKET, port);
}

static inline long sendto(long handle, unsigned int to, unsigned short port,
                          const void *buf, long len)
{
    return syscall5(SYS_SENDTO, handle, to, port, (long)buf, len);
}

static inline long recvfrom(long handle, struct from *who,
                            void *buf, long len)
{
    return syscall4(SYS_RECVFROM, handle, (long)who, (long)buf, len);
}

static inline long recvwait(long handle, struct from *who, void *buf,
                            unsigned long len, long timeout_ms)
{
    return syscall5(SYS_RECVWAIT, handle, (long)who, (long)buf, (long)len,
                    timeout_ms);
}

static inline long select(const int *handles, unsigned long count,
                          long timeout_ms, unsigned char *ready)
{
    return syscall4(SYS_SELECT, (long)handles, (long)count, timeout_ms,
                    (long)ready);
}

static inline long connect(unsigned long ip, long port)
{
    return syscall2(SYS_CONNECT, (long)ip, port);
}
static inline long listen(long port)
{
    return syscall1(SYS_LISTEN, port);
}
static inline long accept(long handle, long timeout_ms)
{
    return syscall2(SYS_ACCEPT, handle, timeout_ms);
}
static inline long send(long handle, const void *buf, unsigned long len)
{
    return syscall3(SYS_SEND, handle, (long)buf, (long)len);
}
static inline long recv(long handle, void *buf, unsigned long len,
                        long timeout_ms)
{
    return syscall4(SYS_RECV, handle, (long)buf, (long)len, timeout_ms);
}
static inline long shutdown(long handle)
{
    return syscall1(SYS_SHUTDOWN, handle);
}

/*
 * where a handler returns to. naked, so the number cannot be handed in as
 * an operand, and it is spelled out of SYS_SIGRETURN instead of written
 * as a literal that could fall out of step with it
 */
#define SIGRETURN_STR_(n) #n
#define SIGRETURN_STR(n)  SIGRETURN_STR_(n)

__attribute__((naked, unused))
static void __sig_trampoline(void)
{
    __asm__ volatile (
        "mov %%rsp, %%rdi\n\t"
        "mov $" SIGRETURN_STR(SYS_SIGRETURN) ", %%eax\n\t"
        "syscall\n\t"
        "ud2\n\t"
        : : : "memory");
}

static inline long signal(long sig, void (*handler)(long))
{
    return syscall3(SYS_SIGNAL, sig, (long)handler,
                    (long)(void *)__sig_trampoline);
}

static inline long sigsend(long pid, long sig)
{
    return syscall2(SYS_SIGSEND, pid, sig);
}

static inline long sigmask(long set, unsigned long mask)
{
    return syscall2(SYS_SIGMASK, set, (long)mask);
}

static inline long tty_mode(unsigned long flags)
{
    return syscall2(SYS_TTYMODE, 1, (long)flags);
}
static inline long tty_mode_get(void)
{
    return syscall2(SYS_TTYMODE, 0, 0);
}

static inline long winsize(unsigned *cols, unsigned *rows)
{
    return syscall2(SYS_WINSIZE, (long)cols, (long)rows);
}

static inline long now_seconds(void)
{
    return syscall0(SYS_NOW);
}

static inline long alarm(long seconds)
{
    return syscall1(SYS_ALARM, seconds);
}

/*
 * the calls velvet 0.4.1 added, which are what a program needs to do
 * everything the kernel's shell used to do for it: become another
 * program, build a pipeline, run jobs on a console of its own, become a
 * user, stop the machine, and ask for the reports only the shell could
 * print
 */
static inline long waitpid(long i_pid, int *p_status, long i_flags)
{
    return syscall3(SYS_WAIT, i_pid, (long)p_status, i_flags);
}

/* the environment is the process's and goes with it, so there is no envp */
static inline long execv(const char *str_path, char *const pp_argv[])
{
    return syscall3(SYS_EXECVE, (long)str_path, (long)ustrlen(str_path),
                    (long)pp_argv);
}

static inline long pipe(int32_t arr_fds[2])
{
    return syscall1(SYS_PIPE, (long)arr_fds);
}

static inline long dup2(long i_from, long i_to)
{
    return syscall2(SYS_DUP2, i_from, i_to);
}

static inline long lseek(long i_fd, long i_offset, long i_whence)
{
    return syscall3(SYS_LSEEK, i_fd, i_offset, i_whence);
}

static inline long setpgid(long i_pid, long i_pgid)
{
    return syscall2(SYS_SETPGID, i_pid, i_pgid);
}

static inline long tcsetpgrp(long i_pgid)
{
    return syscall1(SYS_TCSETPGRP, i_pgid);
}

static inline long tcgetpgrp(void)
{
    return syscall0(SYS_TCGETPGRP);
}

static inline long ttyattach(long i_console)
{
    return syscall1(SYS_TTYATTACH, i_console);
}

static inline long setuid(long i_uid)
{
    return syscall1(SYS_SETUID, i_uid);
}

static inline long reboot(long i_how)
{
    return syscall1(SYS_REBOOT, i_how);
}

static inline long sync(void)
{
    return syscall0(SYS_SYNC);
}

static inline long fsck(const char *str_at, long i_flags,
                        struct user_fsck *p_out)
{
    return syscall4(SYS_FSCK, (long)str_at, (long)ustrlen(str_at), i_flags,
                    (long)p_out);
}

static inline long resize(const char *str_at, long i_blocks,
                          struct user_resize *p_out)
{
    return syscall4(SYS_RESIZE, (long)str_at, (long)ustrlen(str_at),
                    i_blocks, (long)p_out);
}

static inline long source_tree(long i_flags, struct user_source *p_out)
{
    return syscall2(SYS_SOURCE, i_flags, (long)p_out);
}

static inline long klogctl(long i_type, char *str_buf, long i_len)
{
    return syscall3(SYS_SYSLOG, i_type, (long)str_buf, i_len);
}

/*
 * a signal ignored, or put back to what it does by default, which needs
 * no handler and so no trampoline
 */
static inline long signal_ignore(long i_sig)
{
    return syscall3(SYS_SIGNAL, i_sig, SIG_IGNORE, 0);
}

static inline long signal_default(long i_sig)
{
    return syscall3(SYS_SIGNAL, i_sig, SIG_DEFAULT, 0);
}

static inline unsigned int parse_ip(const char *s)
{
    unsigned int addr = 0;
    for (int part = 0; part < 4; part++) {
        if (part > 0) {
            if (*s != '.') return 0;
            s++;
        }
        if (*s < '0' || *s > '9') return 0;
        unsigned int v = 0;
        while (*s >= '0' && *s <= '9') {
            v = v * 10 + (unsigned int)(*s - '0');
            if (v > 255) return 0;
            s++;
        }
        addr = (addr << 8) | v;
    }
    return (*s == '\0') ? addr : 0;
}

static inline int ustrcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static inline void write_num(long v)
{
    char buf[24];
    int i = 23;
    buf[i] = '\0';
    if (v == 0) buf[--i] = '0';
    while (v > 0) { buf[--i] = (char)('0' + v % 10); v /= 10; }
    write(&buf[i]);
}

#endif
