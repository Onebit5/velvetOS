the network clients, as the kernel had them
===========================================

cut out of velvet's ``docs/subsystems/net.rst`` at 92087d5, the commit before
0.4.3 took dhcp, the resolver and http out of the kernel. it describes the
files beside it in ``pending/``, and ``../README.md`` says what is to become
of them.

the layers
----------

``dhcp.c``
   an address somebody else decides, and the first *conversation* on the
   wire: discover, offer, request, ack. the request is broadcast on
   purpose, so any other server that also offered can hear it was not
   chosen and take its offer back.

   the chicken-and-egg: a machine doing dhcp has no address yet, which
   every layer underneath assumes it has. so discover goes out from
   0.0.0.0 to 255.255.255.255, the ethernet frame goes to the broadcast
   address directly rather than through arp (arp cannot help. asking who
   has an address requires having one), and the client sets the broadcast
   flag so the server can answer without arp either. that is why
   ``netif.c`` grew a send path that does not check whether the interface
   is up. the state machine is *pure*: told the time, given packets, it
   returns what should be sent.

``dns.c``
   turning a word into an address, and the first time this machine asks a
   question of a protocol whose other end it does not control. a name is a
   sequence of length-prefixed labels ending in a zero byte; no dots on
   the wire. a name may be cut short by a **pointer** whose fourteen bits
   are an offset into the message.

   that is the whole of the danger: a pointer may point backwards,
   forwards, or at itself, and nothing in the format forbids a cycle, so
   a decoder that simply follows them is one a fourteen-byte reply can
   hang forever. every jump is counted and bounded, every offset checked
   against the message length, and a name that has not ended by the time
   the budget runs out is refused rather than truncated. a truncated name
   is worse than no name: it resolves to *something*, and something is
   what gets connected to.

``http.c``
   enough http to pull a file down. it is a **state machine and not a
   parser**, because a response is not handed over whole: it arrives as
   whatever pieces tcp felt like delivering. the same response is legally
   the same whether it arrives as one buffer of nine hundred bytes or nine
   hundred buffers of one. the test drives the same response through it
   split at every single byte offset and checks the answer never changes.

   two ways a body ends: ``Content-Length``, and ``Transfer-Encoding:
   chunked`` (each piece preceded by its length in hexadecimal, ending
   with a piece of length zero, which a server generating a page as it
   sends it cannot avoid). when neither is present, the body ends when the
   connection does. **no https**, which is a real limit: on today's
   internet most sites answer with a redirect to a scheme this cannot
   speak, and ``fetch`` reports that plainly rather than pretending.

the address
-----------

until something sets an address. dhcp, or somebody typing one. the
machine can answer arp and nothing else, which is honest: it has no address
to put in a packet. dhcp runs on the poll thread and configures the
interface when a lease is granted, so the machine boots with no address and
acquires one a moment later, and every command that needs one already knew
how to say "the wire is not up". setting an address by hand stops it: two
things deciding the address is worse than either alone.

the dhcp interface
--------------------

an address somebody else decides, and the only conversation here: four
messages, discover, offer, request and ack, with the third going out by
broadcast so any server that also offered can hear it was not chosen.

the state machine is told the time and given packets, and returns what should
be sent, which is what lets a test walk it through an eight-second backoff
and a twelve-hour lease in microseconds.

``#define DHCP_SERVER_PORT 67``
    dhcp: an address somebody else decides.

``#define DHCP_DISCOVER 1``
    the message types, which travel in option 53

``#define DHCP_FIXED_LEN 236``
    a bootp message is 236 bytes before the options begin, and the whole thing is padded out to 300, older bootp relays drop anything shorter, and a padded message costs nothing

``struct dhcp_message { uint8_t  type;              /* option 53 */``
    the fields worth having, out of a format that carries a great many that no longer mean anything.

``#define DHCP_TRIES      4``
    how many times a message is repeated before giving up, and how long the first wait is.

``ipv4 address, server, mask, router, dns;``
    what has been offered or granted

``uint64_t sent_ms;``
    when the current message was sent, and how many times.

``uint64_t bound_ms;``
    when the lease was granted, so renewal can be worked out from it.

``unsigned discovers, offers, requests, acks, naks;``
    how the conversation went, for `ifconfig`.

``void dhcp_start(struct dhcp *d, const struct mac *me, uint32_t xid, uint64_t now_ms);``
    begin. `xid` identifies this conversation and must differ between runs, a reply carrying somebody else's number is somebody else's reply, which is the only thing distinguishing two machines shouting on the same wire at the same moment

``void dhcp_input(struct dhcp *d, const uint8_t *p, size_t len, uint64_t now_ms);``
    something arrived on port 68.

``size_t dhcp_tick(struct dhcp *d, uint64_t now_ms, uint8_t *out, size_t max);``
    time passed. returns the number of bytes to send, or 0 for "nothing to do just now", which is the answer almost every time it is asked. the caller sends whatever comes back to 255.255.255.255, except in DHCP_RENEWING where it goes to `d->server` directly: a machine that still holds a lease has an address to speak from and no reason to shout at everybody

``bool dhcp_take_address(struct dhcp *d);``
    whether the caller should now configure the interface.

``uint32_t dhcp_lease_left(const struct dhcp *d, uint64_t now_ms);``
    seconds until the lease runs out, 0 if there is none.

the dns interface
-------------------

turning a word into an address, and the first place here that asks a
question of software somebody else wrote years ago. the only sensible posture
is to believe none of it: a name on the wire is length-prefixed labels, a
pointer in the middle of one may point at itself, so every jump is counted
and a name that has not ended when the budget runs out is refused rather than
truncated.

``#define DNS_NAME_MAX  255``
    the largest a name may be, and the largest a label may be.

``#define DNS_MESSAGE_MAX 512``
    a udp reply is capped at 512 bytes unless both ends agreed otherwise, and they have not

``#define DNS_TYPE_A     1``
    the record types worth naming

``enum dns_result { DNS_OK, DNS_NO_SUCH_NAME,   /* rcode 3: it does not exist */ DNS_NO_ADDRESS,     /* it exists and has no A record */ DNS_REFUSED,        /* the server would not answer */ DNS_MALFORMED       /* and this one is not the server's fault */ };``
    what a server said about the question.

``size_t dns_build_query(uint8_t *p, size_t max, const char *name, uint16_t id);``
    write a query for `name`.

``enum dns_result dns_parse_response(const uint8_t *p, size_t len, uint16_t id, const char *name, ipv4 *out, uint32_t *ttl);``
    read a reply. `out` gets the first A record found.

``bool dns_read_name(const uint8_t *p, size_t len, size_t at, char *out, size_t max, size_t *after);``
    decode a name at `at`, following compression pointers safely.

the http interface
--------------------

enough http to pull a file down, and a state machine rather than a parser,
because a response arrives as whatever pieces tcp felt like delivering: the
status line may come in two segments and a header may be split inside its
name.

what it reaches is plain http, and a redirect is reported rather than
followed, since following one silently is how a fetch ends up with something
other than what was asked for.

``#define HTTP_PORT 80``
    enough http to pull a file down.

``#define HTTP_LINE_MAX 512``
    the longest a status line or a single header may be.

``#define HTTP_VALUE_MAX 256``
    how much of the interesting headers is kept

``char   line[HTTP_LINE_MAX];``
    a line being assembled across however many feeds it took

``char content_type[HTTP_VALUE_MAX];``
    the two headers worth keeping.

``size_t http_feed(struct http *h, const uint8_t *in, size_t len, uint8_t *out, size_t max, size_t *out_len);``
    feed it whatever arrived.

``void http_closed(struct http *h);``
    the connection closed.

``bool http_parse_url(const char *url, char *host, size_t host_max, uint16_t *port, char *path, size_t path_max, bool *unsupported_scheme);``
    enough of one to fetch with: a scheme this can speak, a host, an optional port, and a path.
