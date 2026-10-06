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
 * XXX: these numbers are the whole interface, and they are written out
 * twice, once here and once in the kernel's dispatch, with nothing
 * holding the two lists together. inserting a call in the middle of
 * either one renumbers every call after it, and a program then asks the
 * kernel for something else entirely rather than failing, which turns
 * one forgotten edit into a whole image of subtly wrong programs.
 * mkboot.py already checks the boot constants agree across the same kind
 * of boundary; the syscall numbers want the same treatment.
 */
#define SYS_EXIT   0
#define SYS_WRITE  1
#define SYS_READ   2
#define SYS_UPTIME 3
#define SYS_YIELD  4
#define SYS_SLEEP  5
#define SYS_OPEN   6
#define SYS_CLOSE  7
#define SYS_GETPID 8
#define SYS_SPAWN  9
#define SYS_WAIT   10
#define SYS_READDIR 11
#define SYS_GETUID 12
#define SYS_CREATE 13
#define SYS_CHDIR  14
#define SYS_GETCWD 15
#define SYS_MKDIR  16
#define SYS_RMDIR  17
#define SYS_UNLINK 18
#define SYS_RENAME 19
#define SYS_STAT   20
#define SYS_GETKEY 21
#define SYS_SCREEN 22
#define SYS_CURSOR 23
#define SYS_CLEAR  24
#define SYS_FORK   25
#define SYS_MMAP   26
#define SYS_MUNMAP 27
#define SYS_CHMOD  28
#define SYS_CHOWN  29
#define SYS_SYMLINK 30
#define SYS_READLINK 31
#define SYS_GETENV 32
#define SYS_SETENV 33
#define SYS_SOCKET   34
#define SYS_SENDTO   35
#define SYS_RECVFROM 36
#define SYS_RECVWAIT 37
#define SYS_SELECT   38
#define SYS_CONNECT  39
#define SYS_LISTEN   40
#define SYS_ACCEPT   41
#define SYS_SEND     42
#define SYS_RECV     43
#define SYS_SHUTDOWN 44
#define SYS_SIGNAL    45
#define SYS_SIGSEND   46
#define SYS_SIGRETURN 47
#define SYS_SIGMASK   48
#define SYS_TTYMODE   49
#define SYS_WINSIZE   50
#define SYS_NOW       51
#define SYS_ALARM     52

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

#define KEY_UP     0x100
#define KEY_DOWN   0x101
#define KEY_LEFT   0x102
#define KEY_RIGHT  0x103
#define KEY_DELETE 0x104
#define KEY_HOME   0x105
#define KEY_END    0x106
#define KEY_PGUP   0x107
#define KEY_PGDN   0x108

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

#define SIG_DEFAULT 0
#define SIG_IGNORE  1

#define SIGHUP   1
#define SIGINT   2
#define SIGQUIT  3
#define SIGABRT  6
#define SIGKILL  9
#define SIGSEGV 11
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20

__attribute__((naked, unused))
static void __sig_trampoline(void)
{
    __asm__ volatile (
        "mov %%rsp, %%rdi\n\t"
        "mov $47, %%eax\n\t"
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

#define TTY_RAW      1
#define TTY_ECHO     2
#define TTY_SIGNALS  4

#define TTY_COOKED  (TTY_ECHO | TTY_SIGNALS)

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
