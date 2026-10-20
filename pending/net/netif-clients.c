// SPDX-License-Identifier: GPL-2.0-only
/*
 * pending/net/netif-clients.c
 * Jose A. Perez de Azpillaga <azpijr@gmail.com>, 2026
 * where the kernel drove dhcp and the resolver, cut out of its netif.c.
 */

/*
 * not a file that compiles. the protocols in pending/net/ are pure: told
 * the time and handed bytes, they say what to send. this is the half of
 * velvet's net/netif.c, at 92087d5, that put them on the wire from the
 * kernel's poll thread: the resolver with its cache, dhcp's broadcast from
 * a machine with no address, and the two places the receive path handed
 * a datagram to them instead of to a socket:
 *
 *     static ipv4 my_router, my_dns;
 *     static struct dhcp dhcp;
 *
 *     in the receive path, before the sockets were asked:
 *
 *         if (uh.from_port == DNS_PORT) {
 *             uint64_t f = spin_lock_irq(&net_lock);
 *             got_dns(payload + uh.payload_at, uh.payload_len);
 *             spin_unlock_irq(&net_lock, f);
 *             return;
 *         }
 *
 *         if (uh.to_port == DHCP_CLIENT_PORT) {
 *             dhcp_input(&dhcp, payload + uh.payload_at, uh.payload_len,
 *                        pit_uptime_ms());
 *             return;
 *         }
 *
 *     and on every pass of the poll thread, after the frames:
 *
 *         dhcp_run();
 *         if (query.waiting && !query.answered) {
 *             waitq_wake_all(&dns_waiters);
 *         }
 *
 * in ring 3 the poll thread is the program's own loop, the intercepts are
 * a socket bound to port 68 and one that talks to port 53, and net_up is
 * whatever call velvet gives a program to set the address.
 */

/*
 * the resolver. dns.c is the pure half, build a question, read an
 * answer, believe as little of it as possible, and this is the part
 * that puts it on the wire and waits.
 *
 * it is a *blocking* call made from whatever thread wanted the name,
 * rather than a state machine driven by the poll thread like dhcp. the
 * difference is that dhcp runs whether or not anybody asked, and this
 * only ever happens because a caller is sitting there wanting an answer.
 * so the caller does the waiting, and the poll thread's only job is to
 * hand over the reply when it turns up.
 *
 * the server comes from dhcp, which recorded option 6 long before
 * anything used it.
 */
#define DNS_CACHE_MAX   8
#define DNS_RETRIES     3
#define DNS_WAIT_MS     1000

struct dns_entry {
    char     name[DNS_NAME_MAX + 1];
    ipv4     address;
    uint64_t expires_ms;
};
static struct dns_entry dns_cache[DNS_CACHE_MAX];
static size_t dns_cache_at;

/* the one question outstanding at a time. */
static struct {
    bool     waiting;
    uint64_t started_ms;        /* so a jammed one can be taken over */
    uint16_t id;
    char     name[DNS_NAME_MAX + 1];
    bool     answered;
    ipv4     address;
    uint32_t ttl;
    enum dns_result result;
} query;

static struct waitq dns_waiters;
static uint16_t dns_next_id = 1;
static uint64_t dns_queries, dns_replies, dns_hits;

/* a reply arrived on the poll thread. called with the lock held */
static void got_dns(const uint8_t *p, size_t len)
{
    if (!query.waiting || query.answered) {
        return;
    }
    ipv4 address = 0;
    uint32_t ttl = 0;
    enum dns_result r = dns_parse_response(p, len, query.id, query.name,
                                           &address, &ttl);
    if (r == DNS_MALFORMED) {
        /* not an answer to this question, a stray reply, or somebody guessing at the id. */
        return;
    }
    dns_replies++;
    query.address = address;
    query.ttl     = ttl;
    query.result  = r;
    query.answered = true;
    waitq_wake_all(&dns_waiters);
}

static void cache_put(const char *name, ipv4 address, uint32_t ttl_s)
{
    /*
     * a ttl of an hour is plenty for a machine that reboots often, and
     * caps whatever a server claims, some hand out days, and this
     * table has eight entries
     */
    if (ttl_s > 3600) {
        ttl_s = 3600;
    }
    if (ttl_s == 0) {
        return;         /* it asked not to be remembered */
    }
    struct dns_entry *e = &dns_cache[dns_cache_at % DNS_CACHE_MAX];
    dns_cache_at++;

    size_t n = 0;
    while (name[n] != '\0' && n < DNS_NAME_MAX) {
        e->name[n] = name[n];
        n++;
    }
    e->name[n] = '\0';
    e->address = address;
    e->expires_ms = pit_uptime_ms() + (uint64_t)ttl_s * 1000;
}

static bool cache_get(const char *name, ipv4 *out)
{
    uint64_t now = pit_uptime_ms();
    for (int i = 0; i < DNS_CACHE_MAX; i++) {
        struct dns_entry *e = &dns_cache[i];
        if (e->address == 0 || now >= e->expires_ms) {
            continue;
        }
        if (strcasecmp(e->name, name) == 0) {
            *out = e->address;
            return true;
        }
    }
    return false;
}

enum dns_result net_resolve(const char *name, ipv4 *out)
{
    if (name == NULL || name[0] == '\0') {
        return DNS_MALFORMED;
    }

    /* a name that is already an address is not a question */
    if (ipv4_parse(name, out)) {
        return DNS_OK;
    }
    if (!up) {
        return DNS_REFUSED;
    }
    if (cache_get(name, out)) {
        dns_hits++;
        return DNS_OK;
    }

    ipv4 server = my_dns;
    if (server == 0) {
        /* dhcp offered none, and there is nothing sensible to guess. */
        return DNS_REFUSED;
    }

    uint8_t message[DNS_MESSAGE_MAX];
    size_t n;

    /* the longest one question can take: every try, plus its wait */
    const uint64_t whole_thing = (uint64_t)DNS_RETRIES * DNS_WAIT_MS;

    uint64_t flags = spin_lock_irq(&net_lock);
    if (query.waiting
     && pit_uptime_ms() - query.started_ms < whole_thing + DNS_WAIT_MS) {
        spin_unlock_irq(&net_lock, flags);
        return DNS_REFUSED;     /* somebody else is asking, just now */
    }
    /* an older one than that is abandoned rather than respected. */
    query.started_ms = pit_uptime_ms();
    query.waiting  = true;
    query.answered = false;
    query.id = dns_next_id++;
    if (dns_next_id == 0) {
        dns_next_id = 1;
    }
    size_t at = 0;
    while (name[at] != '\0' && at < DNS_NAME_MAX) {
        query.name[at] = name[at];
        at++;
    }
    query.name[at] = '\0';
    uint16_t id = query.id;
    spin_unlock_irq(&net_lock, flags);

    n = dns_build_query(message, sizeof message, name, id);
    if (n == 0) {
        query.waiting = false;
        return DNS_MALFORMED;   /* not a name this could ever ask about */
    }

    enum dns_result result = DNS_REFUSED;

    /* how many times the send itself may fail before those stop being free. */
    unsigned stalled = 0;

    for (int try = 0; try < DNS_RETRIES; try++) {
        uint8_t datagram[UDP_HEADER_LEN + DNS_MESSAGE_MAX];
        size_t dn = udp_build(datagram, my_ip, server, DNS_PORT + 10000 + try,
                              DNS_PORT, message, n);
        if (!net_send_ip(server, IP_PROTO_UDP, datagram, dn)) {
            /* it did not go: the hardware address is not known yet and an arp request has gone off instead. */
            sleep_ms(50);
            if (++stalled < 20) {
                try--;      /* this try has not happened yet */
            }
            continue;
        }
        dns_queries++;

        uint64_t deadline = pit_uptime_ms() + DNS_WAIT_MS;
        for (;;) {
            flags = spin_lock_irq(&net_lock);
            if (query.answered) {
                result = query.result;
                *out   = query.address;
                spin_unlock_irq(&net_lock, flags);
                goto done;
            }
            if (pit_uptime_ms() >= deadline) {
                spin_unlock_irq(&net_lock, flags);
                break;          /* ask again */
            }
            /* the poll thread wakes this queue every time it runs, whether or not anything arrived. */
            waitq_enqueue(&dns_waiters);
            spin_unlock_irq(&net_lock, flags);
            waitq_sleep();
        }
    }

done:
    flags = spin_lock_irq(&net_lock);
    query.waiting = false;
    spin_unlock_irq(&net_lock, flags);

    if (result == DNS_OK && *out != 0) {
        cache_put(name, *out, query.ttl);
    }
    return result;
}

void net_dns_stats(uint64_t *asked, uint64_t *answered, uint64_t *cached)
{
    *asked    = dns_queries;
    *answered = dns_replies;
    *cached   = dns_hits;
}

/*
 * this is the one thing that has to be sent by a machine with no
 * address, so it goes round net_send_ip entirely rather than through it.
 *
 * net_send_ip refuses when the interface is down, resolves the
 * destination through arp, and drops anything not on this wire. every
 * one of those is right, and every one of them is fatal here: there is
 * no address to send from, arp cannot ask on behalf of a machine that
 * has none, and 255.255.255.255 is on no wire in particular. so the
 * frame is built and handed to the card directly, addressed to the
 * ethernet broadcast.
 *
 * a *renewal* is the exception and goes the ordinary way, by then
 * there is an address, and the server it is talking to is a machine on
 * this wire like any other
 */
static uint8_t dhcp_msg[DHCP_MAX_LEN];
static uint8_t dhcp_packet[IP_HEADER_MIN + UDP_HEADER_LEN + DHCP_MAX_LEN];

static bool dhcp_broadcast(const uint8_t *msg, size_t len)
{
    uint8_t datagram[UDP_HEADER_LEN + DHCP_MAX_LEN];
    size_t n = udp_build(datagram, 0, 0xffffffff,
                         DHCP_CLIENT_PORT, DHCP_SERVER_PORT, msg, len);

    size_t at = ip_build(dhcp_packet, 0, 0xffffffff, IP_PROTO_UDP,
                         (uint16_t)n, (uint16_t)(pit_uptime_ms() & 0xffff));
    memcpy(dhcp_packet + at, datagram, n);

    return send_frame(&MAC_BROADCAST, ETHERTYPE_IPV4, dhcp_packet, at + n);
}

/*
 * time passed. called from the poll thread, which is the only thread
 * that touches the client
 */
static void dhcp_run(void)
{
    uint64_t now = pit_uptime_ms();

    size_t n = dhcp_tick(&dhcp, now, dhcp_msg, sizeof dhcp_msg);
    if (n > 0) {
        if (dhcp.state == DHCP_RENEWING && up) {
            /* it has an address now, so this one is an ordinary datagram to an ordinary machine. */
            uint8_t datagram[UDP_HEADER_LEN + DHCP_MAX_LEN];
            size_t dn = udp_build(datagram, my_ip, dhcp.server,
                                  DHCP_CLIENT_PORT, DHCP_SERVER_PORT,
                                  dhcp_msg, n);
            net_send_ip(dhcp.server, IP_PROTO_UDP, datagram, dn);
        } else {
            dhcp_broadcast(dhcp_msg, n);
        }
    }

    if (dhcp_take_address(&dhcp)) {
        ipv4 mask = dhcp.mask != 0 ? dhcp.mask : IPV4(255, 255, 255, 0);
        net_up(dhcp.address, mask);
        my_router = dhcp.router;
        my_dns    = dhcp.dns;

        char a[32], m[32];
        ipv4_format(dhcp.address, a, sizeof a);
        ipv4_format(mask, m, sizeof m);
        /* to the log rather than the screen. */
        klog_printf("net        : %s netmask %s, leased for %u seconds\n",
                    a, m, dhcp.lease_s);
    }
}

void net_dhcp_start(void)
{
    if (!e1000_present()) {
        return;
    }
    /*
     * the conversation's number has to differ between runs, or a reply
     * left over from the last one is indistinguishable from an answer
     * to this one. there is no random source in this kernel, so it is
     * the clock and the hardware address, which differ between two
     * machines and between two boots of one
     */
    const struct mac *me = e1000_mac();
    uint32_t xid = (uint32_t)pit_uptime_ms();
    for (int i = 0; i < MAC_LEN; i++) {
        xid = xid * 31 + me->b[i];
    }
    dhcp_start(&dhcp, me, xid, pit_uptime_ms());
}

void net_dhcp_stop(void)
{
    dhcp_stop(&dhcp);
}

const struct dhcp *net_dhcp(void)
{
    return &dhcp;
}
