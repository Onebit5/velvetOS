// SPDX-License-Identifier: GPL-2.0-only
/*
 * pending/shell/commands.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * the monitor's install, dhcp, host and fetch, as the kernel had them.
 */

/*
 * not a file that compiles. these are the functions velvet's shell/shell.c
 * held at 92087d5, before 0.4.3 took them out: what a person typed, and what
 * they were told. the protocols are beside this, in pending/net/ and
 * pending/fs/; this is the half that becomes a program's main().
 */

/* the installer talks to two drives by number. */
struct install_context {
    unsigned src, dst;
};

static bool install_src_read(void *ctx, uint64_t lba, uint32_t count,
                             void *buf)
{
    const struct install_context *c = ctx;
    return disk_raw_read(c->src, lba, count, buf);
}

static bool install_dst_read(void *ctx, uint64_t lba, uint32_t count,
                             void *buf)
{
    const struct install_context *c = ctx;
    return disk_raw_read(c->dst, lba, count, buf);
}

static bool install_dst_write(void *ctx, uint64_t lba, uint32_t count,
                              const void *buf)
{
    const struct install_context *c = ctx;
    return disk_raw_write(c->dst, lba, count, buf);
}

static void install_say(void *ctx, const char *what)
{
    (void)ctx;
    kprintf("  %s\n", what);
}

/* every file, which on a ramdisk this size is a wall of text. */
static void install_report(const char *what)
{
    kprintf("  %s\n", what);
}

/*
 * the ramdisk has been kept on the argument that it is what
 * makes the machine work when the disk does not. that makes this machine
 * a live medium by default, and there was never a way to say so.
 *
 * a drive is carrying the system if it has philemon's table at sector
 * 32. the machine is *installed* if that drive is also the one the root
 * filesystem is mounted from, one disk that both starts the machine
 * and holds it. anything else is live: running from something that
 * carries a system but is not the system's home
 */

/*
 * the ramdisk has been kept on the argument that it is what
 * makes the machine work when the disk does not. that makes this machine
 * a live medium by default, and there was never a way to say so.
 *
 * a drive is carrying the system if it has philemon's table at sector
 * 32. the machine is *installed* if that drive is also the one the root
 * filesystem is mounted from, one disk that both starts the machine
 * and holds it. anything else is live: running from something that
 * carries a system but is not the system's home
 */

static void cmd_install(int argc, char **argv)
{
    if (disk_drive_count() == 0) {
        kprintf("there are no drives here to install onto\n");
        return;
    }

    /* which drive carries this system. */
    int source = disk_system_drive();
    if (source < 0) {
        kprintf("this machine is not running from a boot medium, so "
                "there is nothing to copy\n");
        return;
    }

    if (argc < 2) {
        kprintf("install <drive>, copy this system onto a drive and make "
                "it bootable\n\n");
        kprintf("%-6s %-22s %10s   %s\n", "drive", "model", "MiB", "");
        for (size_t i = 0; i < disk_drive_count(); i++) {
            uint64_t sectors = disk_drive_sectors((unsigned)i);
            const char *note = "";
            if ((int)i == source) {
                note = "carries this system, the one you booted";
            } else if ((int)i == disk_mounted_drive()) {
                note = "currently mounted at /";
            }
            kprintf("%-6zu %-22s %10lu   %s\n", i, disk_drive_model((unsigned)i),
                    sectors / 2048, note);
        }
        kprintf("\neverything on the drive you name is destroyed.\n");
        return;
    }

    /*
     * two partitions unless told otherwise: the system on one and
     * somewhere to work on the other, so that filling the second does
     * not stop the machine. `install <n> whole` gives the older
     * arrangement, one filesystem over the whole drive
     */
    bool split = true;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "whole") == 0) {
            split = false;
        }
    }

    size_t target = 0;
    for (const char *p = argv[1]; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            kprintf("'%s' is not a drive number. `install` lists them\n",
                    argv[1]);
            return;
        }
        target = target * 10 + (size_t)(*p - '0');
    }
    if (target >= disk_drive_count()) {
        kprintf("there is no drive %zu\n", target);
        return;
    }
    if ((int)target == source) {
        kprintf("that is the drive this system is running from. installing "
                "onto it would overwrite what is being read\n");
        return;
    }

    /* the cache is bound to the mounted drive and the installer talks to another one. */
    if (disk_dirty() && !disk_sync()) {
        kprintf("could not flush what is still in memory. not starting\n");
        return;
    }

    console_set_colors(COLOR_WARN, 0x101018);
    kprintf("everything on drive %zu is about to be destroyed.\n", target);
    console_set_colors(COLOR_TEXT, 0x101018);
    kprintf("type the drive number again to go ahead: ");

    char confirm[8];
    read_line(confirm, sizeof confirm);
    if (strcmp(confirm, argv[1]) != 0) {
        kprintf("nothing was written\n");
        return;
    }

    struct install_context ctx = { (unsigned)source, (unsigned)target };

    struct install_io io = {
        .src_read    = install_src_read,
        .dst_read    = install_dst_read,
        .dst_write   = install_dst_write,
        .ctx         = &ctx,
        .dst_sectors = disk_drive_sectors((unsigned)target),
        .say         = install_say,
    };

    struct install_result r;
    const char *error = NULL;
    if (!install_system(&io, (uint32_t)(pit_uptime_ms() / 1000), split,
                        &r, &error)) {
        console_set_colors(COLOR_WARN, 0x101018);
        kprintf("install failed: %s\n", error);
        console_set_colors(COLOR_TEXT, 0x101018);
        return;
    }

    kprintf("  system %lu sectors, partition at %lu, %lu MiB of ext4\n",
            r.system_sectors, r.part_first_lba, r.part_sectors / 2048);
    if (r.work_sectors > 0) {
        kprintf("  and %lu MiB more at %lu, which will be %s, so that "
                "filling it\n", r.work_sectors / 2048, r.work_first_lba,
                DISK_WORK_AT);
        kprintf("  does not leave the system with nowhere to write\n");
    } else if (split) {
        kprintf("  one partition: the drive is not big enough for a second "
                "one worth\n  having, and two filesystems too small to "
                "hold anything is worse\n  than one that is merely "
                "small\n");
    }

    /*
     * and now the files. the new filesystem has to be mounted to be
     * written to, which means letting go of the one at /, so the
     * ramdisk at /boot is the source, since it is memory and does not
     * care which disk is selected
     */
    kprintf("mounting the new filesystem\n");
    if (!disk_mount()) {
        kprintf("the new disk did not come back as mountable. "
                "the system is on it but there is nothing in the filesystem\n");
        return;
    }

    size_t files = install_copy_tree(VFS_BOOT, "/", install_report, &error);
    if (error != NULL) {
        console_set_colors(COLOR_WARN, 0x101018);
        kprintf("copying stopped after %zu files: %s\n", files, error);
        console_set_colors(COLOR_TEXT, 0x101018);
        return;
    }

    /* and the source, expanded into /src. */
    if (source_present()) {
        kprintf("writing the source out to /src\n");
        size_t src_files = install_copy_tree(VFS_SRC, "/src", install_report,
                                             &error);
        if (error != NULL) {
            console_set_colors(COLOR_WARN, 0x101018);
            kprintf("the system is installed, but the source stopped after "
                    "%zu files: %s\n", src_files, error);
            console_set_colors(COLOR_TEXT, 0x101018);
            return;
        }
        files += src_files;
    }

    if (!disk_sync()) {
        kprintf("the drive would not flush. what was written may not be "
                "all of it\n");
        return;
    }

    console_set_colors(COLOR_PROMPT, 0x101018);
    kprintf("\n%zu files copied. drive %zu will boot on its own now.\n",
            files, target);
    console_set_colors(COLOR_TEXT, 0x101018);
}

/* read a line for the kernel's own prompts. */
static void read_line(char *buf, size_t max)
{
    size_t len = 0;
    for (;;) {
        int c = input_getchar_blocking();

        if (c == '\n') {
            kprintf("\n");
            break;
        }
        if (c == '\b') {
            if (len > 0) {
                len--;
                kprintf("\b \b");
            }
            continue;
        }
        if (c < ' ' || c > '~') {
            continue;
        }
        if (len + 1 < max) {
            buf[len++] = (char)c;
            kprintf("%c", (char)c);
        }
    }
    buf[len] = '\0';
}

static void cmd_ifconfig(int argc, char **argv)
{
    if (!e1000_present()) {
        kprintf("no network card. `lspci` says what is on the bus\n");
        return;
    }

    char text[32];

    if (argc >= 2) {
        ipv4 addr;
        if (!ipv4_parse(argv[1], &addr)) {
            kprintf("'%s' is not an address\n", argv[1]);
            return;
        }
        ipv4 mask = IPV4(255, 255, 255, 0);
        if (argc >= 3 && !ipv4_parse(argv[2], &mask)) {
            kprintf("'%s' is not a netmask\n", argv[2]);
            return;
        }
        /* an address set by hand ends the lease. */
        net_dhcp_stop();
        if (!net_up(addr, mask)) {
            kprintf("the card would not come up\n");
            return;
        }
        ipv4_format(addr, text, sizeof text);
        kprintf("address %s", text);
        ipv4_format(mask, text, sizeof text);
        kprintf(" netmask %s\n", text);
        return;
    }

    mac_format(e1000_mac(), text, sizeof text);
    kprintf("card    : %s, hardware address %s\n", e1000_model(), text);

    struct e1000_stats es;
    e1000_get_stats(&es);

    /*
     * which way the card is being noticed, and above the address rather
     * than below it: the interrupt is arranged when the card is found
     * and has nothing to do with whether anything has been addressed
     * yet. it lived under the statistics for exactly one version, where
     * a machine with no address returned before reaching it, and
     * `ifconfig` with no argument is the first thing anybody types,
     * which is where a diagnostic has to be if it is going to be read.
     *
     * the whole point of it: a machine on the fallback timer works, and
     * looks exactly like one that does not, until it is asked
     */
    if (e1000_irq_line() == 0) {
        console_set_colors(COLOR_WARN, 0x101018);
        kprintf("woken   : on a timer. the firmware gave the card no "
                "interrupt line, this works, half a second behind\n");
        console_set_colors(COLOR_TEXT, 0x101018);
    } else if (!e1000_interrupts_working()) {
        kprintf("woken   : armed on irq %u, nothing has arrived yet. "
                "send something and look again\n", e1000_irq_line());
    } else {
        kprintf("woken   : by the card on irq %u, %lu times",
                e1000_irq_line(), es.interrupts);
        if (es.not_ours > 0) {
            kprintf(" (%lu on the line were somebody else's)", es.not_ours);
        }
        kprintf("\n");
    }

    if (!net_is_up()) {
        const struct dhcp *pending = net_dhcp();
        if (pending->state == DHCP_OFF || pending->state == DHCP_FAILED) {
            kprintf("address : none, and none coming, %s. "
                    "`ifconfig <address> [netmask]` sets one\n",
                    dhcp_state_name(pending));
        } else {
            kprintf("address : none yet, %s\n", dhcp_state_name(pending));
        }
        return;
    }
    ipv4_format(net_address(), text, sizeof text);
    kprintf("address : %s", text);
    ipv4_format(net_netmask(), text, sizeof text);
    kprintf(" netmask %s", text);

    const struct dhcp *d = net_dhcp();
    if (d->state == DHCP_OFF) {
        kprintf(" (set by hand)\n");
    } else {
        kprintf(" (leased, %u seconds left)\n",
                dhcp_lease_left(d, pit_uptime_ms()));
    }
    if (net_router() != 0) {
        ipv4_format(net_router(), text, sizeof text);
        kprintf("gateway : %s, anything not on this wire goes to "
                "it\n", text);
    }
    if (net_dns() != 0) {
        ipv4_format(net_dns(), text, sizeof text);
        kprintf("names   : %s\n", text);
    } else {
        kprintf("names   : none offered, `host` cannot work without "
                "one\n");
    }

    struct net_stats ns;
    net_get_stats(&ns);

    kprintf("frames  : %lu in, %lu out\n", ns.frames_in, ns.frames_out);
    kprintf("of those: %lu arp, %lu ip (%lu icmp, %lu udp)\n",
            ns.arp_in, ns.ip_in, ns.icmp_in, ns.udp_in);

    /* the two numbers worth watching. */
    kprintf("dropped : %lu not for this machine, %lu malformed\n",
            ns.dropped_not_mine, ns.dropped_bad);

    if (es.send_dropped > 0 || es.receive_dropped > 0) {
        console_set_colors(COLOR_WARN, 0x101018);
        kprintf("card    : %lu could not be sent, %lu arrived with nowhere "
                "to go, the rings are too small for this much traffic\n",
                es.send_dropped, es.receive_dropped);
        console_set_colors(COLOR_TEXT, 0x101018);
    }
}

/* the conversation, in the detail `ifconfig` has no room for. */
static void cmd_dhcp(int argc, char **argv)
{
    if (!e1000_present()) {
        kprintf("no network card\n");
        return;
    }

    if (argc >= 2) {
        if (strcmp(argv[1], "renew") == 0 || strcmp(argv[1], "start") == 0) {
            net_dhcp_start();
            kprintf("asking. `ifconfig` says how it went\n");
            return;
        }
        if (strcmp(argv[1], "stop") == 0) {
            net_dhcp_stop();
            kprintf("stopped. the address stays until somebody changes "
                    "it, the lease simply stops being renewed\n");
            return;
        }
        kprintf("`dhcp renew` or `dhcp stop`\n");
        return;
    }

    const struct dhcp *d = net_dhcp();
    char text[32];

    kprintf("state   : %s\n", dhcp_state_name(d));

    if (d->address != 0 && d->state == DHCP_OFF) {
        /* a lease that was abandoned rather than one that is held. */
        ipv4_format(d->address, text, sizeof text);
        kprintf("was     : %s, and given up. the address in use is "
                "whatever `ifconfig` shows, this lease is not being "
                "renewed and will simply expire\n", text);
    } else if (d->address != 0) {
        ipv4_format(d->address, text, sizeof text);
        kprintf("address : %s", text);
        ipv4_format(d->server, text, sizeof text);
        kprintf(", from the server at %s\n", text);
        kprintf("lease   : %u seconds, %u left, renewed at half\n",
                d->lease_s, dhcp_lease_left(d, pit_uptime_ms()));
    }

    /* what was said, in both directions. */
    kprintf("sent    : %u discover, %u request\n", d->discovers, d->requests);
    kprintf("heard   : %u offer, %u ack, %u nak\n", d->offers, d->acks,
            d->naks);

    if (d->state == DHCP_FAILED) {
        console_set_colors(COLOR_WARN, 0x101018);
        kprintf("nobody answered after %u tries. there may be no server on "
                "this wire, `ifconfig <address>` sets one by hand\n",
                DHCP_TRIES);
        console_set_colors(COLOR_TEXT, 0x101018);
    }
}

/* fetch something. the point is not the web. */
static void cmd_fetch(int argc, char **argv)
{
    if (argc < 2) {
        kprintf("fetch <url> [file]\n");
        return;
    }
    if (!net_is_up()) {
        kprintf("the wire is not up\n");
        return;
    }

    char host[128], path[256];
    uint16_t port;
    bool unsupported = false;

    if (!http_parse_url(argv[1], host, sizeof host, &port, path,
                        sizeof path, &unsupported)) {
        if (unsupported) {
            kprintf("this machine speaks http and not https, there is "
                    "no tls here, and a client that tried plain http on "
                    "the wrong port would simply hang\n");
        } else {
            kprintf("'%s' is not a url the shell can use\n", argv[1]);
        }
        return;
    }

    ipv4 to;
    if (!address_of(host, &to)) {
        return;
    }

    int h = net_tcp_open(-1, to, port);
    if (h < 0) {
        kprintf("no room for another connection, `tcp` shows what is "
                "still open\n");
        return;
    }

    /* wait for the handshake. */
    if (net_tcp_accept(-1, h, 5000) < 0) {
        struct tcp_conn *c = net_tcp_at(h - TCP_HANDLE_BASE);
        kprintf("could not connect: %s\n",
                (c != NULL && c->error != NULL) ? c->error
                                                : "it never answered");
        net_tcp_shut(-1, h);
        return;
    }

    char request[512];
    size_t n = http_get(request, sizeof request, host, path);
    if (n == 0 || net_tcp_send(-1, h, request, n) < 0) {
        kprintf("could not send the request\n");
        net_tcp_shut(-1, h);
        return;
    }

    struct http parser;
    http_start(&parser);

    /* the file is not created until there is a body byte to put in it. */
    struct vfs_file file;
    bool to_file = (argc >= 3);
    bool opened = false;
    uint64_t written = 0;

    for (;;) {
        uint8_t in[1024];
        int64_t got = net_tcp_recv(-1, h, in, sizeof in, 5000);

        if (got < 0) {
            kprintf("it stopped answering\n");
            break;
        }
        if (got == 0) {
            /*
             * the stream ended, which for a body with no declared
             * length is how it is *supposed* to end
             */
            http_closed(&parser);
            break;
        }

        size_t used = 0;
        while (used < (size_t)got) {
            uint8_t body[512];
            size_t produced = 0;
            size_t took = http_feed(&parser, in + used, (size_t)got - used,
                                    body, sizeof body, &produced);

            if (produced > 0) {
                if (to_file && !opened) {
                    if (!vfs_create(argv[2], &file)) {
                        kprintf("cannot write to '%s', the directory it "
                                "is in has to exist already\n", argv[2]);
                        net_tcp_shut(-1, h);
                        return;
                    }
                    opened = true;
                }
                if (to_file) {
                    vfs_write(&file, written, body, produced);
                }
                written += produced;
            }
            if (took == 0) {
                break;
            }
            used += took;
        }
        if (parser.state == HTTP_DONE || parser.state == HTTP_BROKEN) {
            break;
        }
    }

    net_tcp_shut(-1, h);

    if (parser.status == 0) {
        kprintf("nothing that looked like a reply came back\n");
        return;
    }

    kprintf("%d", parser.status);
    if (parser.content_type[0] != '\0') {
        kprintf(", %s", parser.content_type);
    }
    kprintf(", %lu bytes\n", written);

    if (parser.location[0] != '\0') {
        kprintf("it points somewhere else: %s\n", parser.location);
        kprintf("redirects are reported rather than followed, a fetch "
                "that quietly went elsewhere would hand you something "
                "other than what you asked for\n");
    }

    if (parser.state == HTTP_BROKEN) {
        console_set_colors(COLOR_WARN, 0x101018);
        kprintf("the reply was cut short. what arrived is a fragment, and "
                "a fragment written over a whole file is worse than "
                "nothing\n");
        console_set_colors(COLOR_TEXT, 0x101018);
        return;
    }
    if (to_file && opened) {
        kprintf("written to %s\n", argv[2]);
    } else if (to_file) {
        kprintf("nothing came back to write, so %s was left alone\n",
                argv[2]);
    }
}

/* a word or an address, for the commands that take either. */
static bool address_of(const char *what, ipv4 *out)
{
    /* an address is not a question. */
    if (ipv4_parse(what, out)) {
        return true;
    }

    enum dns_result r = net_resolve(what, out);

    switch (r) {
    case DNS_OK:
        return true;
    case DNS_NO_SUCH_NAME:
        kprintf("there is no such name as '%s'\n", what);
        return false;
    case DNS_NO_ADDRESS:
        kprintf("'%s' exists and has no address\n", what);
        return false;
    case DNS_REFUSED:
        if (net_dns() == 0) {
            kprintf("'%s' is a name, and nobody offered this machine a "
                    "name server, `dhcp` says what was handed out\n",
                    what);
        } else {
            kprintf("the name server would not answer about '%s'\n", what);
        }
        return false;
    case DNS_MALFORMED:
        kprintf("'%s' is neither an address nor a name\n", what);
        return false;
    }
    return false;
}

/* what a name resolves to, and nothing else. */
static void cmd_host(int argc, char **argv)
{
    if (argc < 2) {
        kprintf("host <name>\n");
        return;
    }
    if (!net_is_up()) {
        kprintf("the wire is not up\n");
        return;
    }

    ipv4 got = 0;
    uint64_t before = pit_uptime_ms();
    if (!address_of(argv[1], &got)) {
        return;
    }
    uint64_t took = pit_uptime_ms() - before;

    char text[32];
    ipv4_format(got, text, sizeof text);
    kprintf("%s is %s", argv[1], text);
    if (took > 0) {
        kprintf(" (%lu ms)", took);
    } else {
        kprintf(" (remembered)");
    }
    kprintf("\n");

    uint64_t asked, answered, cached;
    net_dns_stats(&asked, &answered, &cached);
    kprintf("asked %lu, answered %lu, and %lu were already known\n",
            asked, answered, cached);
}
