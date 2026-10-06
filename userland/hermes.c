// SPDX-License-Identifier: GPL-2.0-only
/*
 * userland/hermes.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * hermes: the messenger. a program that speaks udp.
 */

#include "syscall.h"
#include "args.h"

/* hermes: the messenger, a program that speaks udp. */

static const struct opt opts[] = {
    { 'q', "quiet", false, "do not answer what arrives" },
};

static const struct program hermes = {
    .name = "hermes",
    .usage = "hermes listen <port>  |  hermes send <address> <port> <words...>",
    .summary = "the messenger: send and receive datagrams",
    .opts = opts,
    .opt_count = sizeof opts / sizeof opts[0],
};

static void say_from(struct from *who)
{
    unsigned int a = who->address;
    write("from ");
    write_num((a >> 24) & 0xff); write(".");
    write_num((a >> 16) & 0xff); write(".");
    write_num((a >> 8) & 0xff);  write(".");
    write_num(a & 0xff);
    write(" port ");
    write_num(who->port);
    write(": ");
}

static void listen_on(unsigned short port, int answer)
{
    long h = socket(port);
    if (h < 0) {
        write("hermes: that port is taken, or the wire is down\n");
        exit(1);
    }

    write("hermes: listening on port ");
    write_num(port);
    write(". ctrl+c to stop\n");

    char buf[512];
    struct from who;

    for (;;) {
        long n = recvfrom(h, &who, buf, sizeof buf - 1);
        if (n < 0) {
            /*
             * nothing yet. recvfrom does not block, so this is a poll,
             * and sleeping between asks is what stops it being a spin
             */
            sleep(50);
            continue;
        }
        buf[n] = '\0';
        say_from(&who);
        write(buf);
        write("\n");

        if (answer) {
            /*
             * say it back. a listener that answers is what lets two of
             * these be pointed at each other, and what lets one be
             * pointed at itself
             */
            sendto(h, who.address, who.port, buf, n);
        }
    }
}

/*
 * every program here is entered at _start rather than at main: there is
 * no libc and therefore no crt to run one and call the other. writing
 * `main` gets you a program whose entry point is whatever the linker put
 * first, which for this one was write_num, entered with argc in the
 * register it expected a number in. it faulted on an instruction fetch
 * somewhere in its own argv, which is a long way from the mistake
 */
void _start(int argc, char **argv)
{
    struct args a;
    const char *error = NULL;
    if (!args_parse(&hermes, argc, argv, &a, &error)) {
        write("hermes: ");
        write(error);
        write("\n");
        exit(1);
    }
    if (a.wants_help || a.count < 1) {
        args_usage(&hermes);
        exit(a.wants_help ? 0 : 1);
    }

    if (ustrcmp(a.rest[0], "listen") == 0) {
        if (a.count < 2) {
            write("hermes listen <port>\n");
            exit(1);
        }
        long port = 0;
        for (const char *p = a.rest[1]; *p; p++) {
            if (*p < '0' || *p > '9') { write("that is not a port\n"); exit(1); }
            port = port * 10 + (*p - '0');
        }
        listen_on((unsigned short)port, !args_has(&a, &hermes, 'q'));
    }

    if (ustrcmp(a.rest[0], "send") == 0) {
        if (a.count < 4) {
            write("hermes send <address> <port> <words...>\n");
            exit(1);
        }
        unsigned int to = parse_ip(a.rest[1]);
        if (to == 0) {
            write("that is not an address\n");
            exit(1);
        }
        long port = 0;
        for (const char *p = a.rest[2]; *p; p++) {
            if (*p < '0' || *p > '9') { write("that is not a port\n"); exit(1); }
            port = port * 10 + (*p - '0');
        }

        /* a port of this program's own, so the far end has somewhere to answer to. */
        long h = socket(40000);
        if (h < 0) {
            write("hermes: could not claim a port to send from\n");
            exit(1);
        }

        char msg[512];
        long at = 0;
        for (int i = 3; i < a.count && at < (long)sizeof msg - 1; i++) {
            if (i > 3) msg[at++] = ' ';
            for (const char *p = a.rest[i]; *p && at < (long)sizeof msg - 1; p++) {
                msg[at++] = *p;
            }
        }
        msg[at] = '\0';

        if (sendto(h, to, (unsigned short)port, msg, at) < 0) {
            write("hermes: it would not go. `arp` may not know that "
                  "address yet, try again\n");
            exit(1);
        }
        write("sent ");
        write_num(at);
        write(" bytes\n");

        /*
         * and wait a moment for an answer, since the thing on the other
         * end may well be another hermes
         */
        struct from who;
        char reply[512];
        for (int wait = 0; wait < 20; wait++) {
            long n = recvfrom(h, &who, reply, sizeof reply - 1);
            if (n >= 0) {
                reply[n] = '\0';
                say_from(&who);
                write(reply);
                write("\n");
                exit(0);
            }
            sleep(50);
        }
        exit(0);
    }

    args_usage(&hermes);
    exit(1);
}
