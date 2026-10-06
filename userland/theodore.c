// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/theodore.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * theodore, an attendant who waits.
 */

#include "syscall.h"

static void say(const char *s)
{
    write_fd(STDOUT, s, (long)ustrlen(s));
}

static void say_num(long n)
{
    char b[24];
    int at = 0;
    if (n == 0) {
        b[at++] = '0';
    }
    char tmp[24];
    int t = 0;
    while (n > 0) {
        tmp[t++] = (char)('0' + (n % 10));
        n /= 10;
    }
    while (t > 0) {
        b[at++] = tmp[--t];
    }
    b[at] = '\0';
    say(b);
}

void _start(int argc, char **argv)
{
    long tcp_port = 5555;

    if (argc >= 2) {
        long v = 0;
        for (const char *p = argv[1]; *p; p++) {
            if (*p < '0' || *p > '9') {
                say("theodore: '");
                say(argv[1]);
                say("' is not a port\n");
                exit(1);
            }
            v = v * 10 + (*p - '0');
        }
        if (v <= 0 || v > 65535) {
            say("theodore: that is not a port\n");
            exit(1);
        }
        tcp_port = v;
    }
    long udp_port = tcp_port + 1;

    long tcp = listen(tcp_port);
    if (tcp < 0) {
        say("theodore: nothing is listening, is the wire up?\n");
        exit(1);
    }
    long udp = socket(udp_port);
    if (udp < 0) {
        say("theodore: could not take the udp port\n");
        exit(1);
    }

    say("theodore attends. tcp ");
    say_num(tcp_port);
    say(", udp ");
    say_num(udp_port);
    say("\n");

    /*
     * before a connection exists, the tcp handle is a *listener* and
     * `select` has nothing to say about it, so accept is what waits,
     * with a short timeout, and the udp socket is looked at in between.
     *
     * that is not as neat as one wait covering everything, and it is
     * honest about what this kernel has: a listening entry becomes the
     * connection when a syn arrives, so there is no separate thing for
     * select to watch until somebody has already connected
     */
    long conn = -1;

    for (;;) {
        if (conn < 0) {
            /* look for a caller, briefly, then go and look at the udp port. */
            long got = accept(tcp, 200);
            if (got >= 0) {
                conn = got;
                say("theodore: somebody has connected\n");
                send(conn, "welcome to the velvet room.\n", 28);
            }
        }

        /* everything that is a socket rather than a listener, waited on together. */
        int handles[2];
        unsigned char ready[2];
        unsigned long count = 0;

        handles[count++] = (int)udp;

        long n = select(handles, count, conn < 0 ? 200 : 50, ready);

        if (n > 0 && ready[0]) {
            struct from who;
            char buf[512];
            long got = recvwait(udp, &who, buf, sizeof buf - 1, 0);
            if (got > 0) {
                buf[got] = '\0';
                say("udp: ");
                say(buf);
                if (buf[got - 1] != '\n') {
                    say("\n");
                }
                /*
                 * answered on the connection if there is one, so a
                 * datagram arriving becomes something the tcp caller
                 * sees, which is what makes this a server rather than
                 * two programs sharing a process
                 */
                if (conn >= 0) {
                    send(conn, "udp: ", 5);
                    send(conn, buf, (unsigned long)got);
                }
            }
        }

        if (conn >= 0) {
            char buf[512];
            long got = recv(conn, buf, sizeof buf - 1, 50);

            if (got == 0) {
                /*
                 * the stream ended. zero is not an error and this is the
                 * distinction the whole api rests on, it is how a
                 * reader knows to stop rather than to try again
                 */
                say("theodore: they have gone\n");
                shutdown(conn);
                conn = -1;
                continue;
            }
            if (got > 0) {
                buf[got] = '\0';
                say("tcp: ");
                say(buf);
                if (buf[got - 1] != '\n') {
                    say("\n");
                }

                if (buf[0] == 'q') {
                    send(conn, "farewell.\n", 10);
                    shutdown(conn);
                    conn = -1;
                    continue;
                }
                /*
                 * echoed back, which is enough to prove a stream works
                 * in both directions at once
                 */
                send(conn, "you said: ", 10);
                send(conn, buf, (unsigned long)got);
            }
        }
    }
}
