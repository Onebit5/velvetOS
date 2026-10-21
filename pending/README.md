# pending

what velvet took out of the kernel, waiting to become programs.

velvet 0.4.3 took dhcp, the resolver, http and the installer out of the
kernel, and 0.4.4 took the filesystem checker and the source reader. they
are a system's work, and the kernel keeps what is under them: the stack,
the sockets and the drives. this directory holds them as they were before
they went, the first four at velvet `92087d5` and the other two at
`a52a64d`, velvet 0.4.3, so that making them velvetOS programs starts from
code that has been tested and run rather than from nothing. that is
[ROADMAP.md](../ROADMAP.md)'s 0.4.2.

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
- `fs/fsck.c`, with `include/velvet/fsck.h` and `include/uapi/velvet/fsck.h`:
  the ext4 checker the kernel ran as the fsck call. it already reads and
  writes through two function pointers and is handed its workspace, so it
  ports as it is; what was the kernel's half, the call and the walk under
  the disk lock, becomes a program reading the partition by sector.
  `tools/fsckcheck.py` breaks images to see it notice. velvet keeps a copy
  in its `tools/`, for its own suites to judge its ext4 with.
- `fs/source.c` and `include/velvet/source.h`: the reader for the source
  archive on the boot medium, which the kernel mounted at `/boot/src` and
  hashed for the source call, with its suite. `tools/srcstamp.py` wrote the
  archive's hash into the kernel and `tools/srccheck.py` checks the archive
  against the tree; this tree calls velvet's copies of both until the pin
  moves, and these after.
- `tools/testing/selftests/fs/`: the two calls' selftests, which say from
  ring 3 what the programs have to go on answering.
- `docs/`: the pages that described them.

## what the kernel has to give first

dns and http need nothing new: udp and tcp are calls already. the rest
need velvet 0.4.5:

- a dhcp client needs a call that sets the address, the netmask and the
  router, and a datagram to 255.255.255.255 that leaves before there is an
  address to send it from.
- the installer needs to write a drive that holds no filesystem yet, and to
  read the medium it booted from as sectors.
- fsck needs to read, and to mend, the partition a filesystem is mounted
  from, as sectors, and src to read the medium the same way.

## how a file leaves

when a file becomes part of a program it moves out of here into `userland/`
or `tests/`, its header names its new path, it is built, and its suite runs
in `make test`. when the last one has gone, so does this directory, and the
pin moves to the velvet release that no longer has them.
