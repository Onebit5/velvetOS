# pending

velvet's network clients and installer, waiting to become programs.

velvet 0.4.3 took dhcp, the resolver, http and the installer out of the
kernel. they are a system's work, and the kernel keeps what is under them:
the stack, the sockets and the drives. this directory holds them as they
were at velvet `92087d5`, the commit before they went, so that making them
velvetOS programs starts from code that has been tested and run rather than
from nothing. that is [ROADMAP.md](../ROADMAP.md)'s 0.4.2.

nothing here is built, and nothing here is on the machine yet. each file is
velvet's, at the path velvet had it under `pending/`, changed only where the
header has to name where the file now is.

## what is here

- `net/dhcp.c`, `net/dns.c` and `net/http.c`, with their headers in
  `include/velvet/`: the protocols. they are pure already, told the time
  and handed bytes, they say what to send, so they port as they are, against
  the library's headers instead of the kernel's.
- `net/netif-clients.c`: the half of the kernel's `netif.c` that drove them
  from its poll thread. in ring 3 the poll thread is the program's own loop,
  and what the kernel intercepted on ports 53 and 68 is a socket.
- `fs/install.c` and `fs/mkfs.c`: the installer and the formatter it calls.
  `install_system` already works on sectors through function pointers; the
  tree copy at its end used the kernel's vfs, which a program does with files.
  velvet keeps its own `mkfs.c` in its `tools/`, for the images it builds on
  a host; this one is the copy the machine formats with.
- `shell/commands.c`: the monitor's `install`, `dhcp`, `host` and `fetch`,
  the half that becomes each program's `main`.
- `tests/`: their suites, and the captured http response one of them runs
  against. `tools/fuzz_parsers.c`, whose dns and http halves left velvet with
  them.
- `docs/`: the pages that described them.

## what the kernel has to give first

dns and http need nothing new: udp and tcp are calls already. the other two
need velvet 0.4.4:

- a dhcp client needs a call that sets the address, the netmask and the
  router, and a datagram to 255.255.255.255 that leaves before there is an
  address to send it from.
- the installer needs to write a drive that holds no filesystem yet, and to
  read the medium it booted from as sectors.

## how a file leaves

when a file becomes part of a program it moves out of here into `userland/`
or `tests/`, its header names its new path, it is built, and its suite runs
in `make test`. when the last one has gone, so does this directory, and the
pin moves to the velvet release that no longer has them.
