# velvetOS

![ci](https://github.com/Onebit5/velvetOS/actions/workflows/ci.yml/badge.svg)

a tiny 64-bit system for x86_64: a kernel of its own, a libc and programs to run
on it, and the assembler, linker and make that build it.

built to make what happens between "power button" and "shell prompt" legible, rather
than to be the next linux. small enough to fit in one head.

the kernel is [velvet](https://github.com/Onebit5/velvet), a project of
its own since 0.4.0, carried here as the `kernel/` submodule and pinned to the commit
this system is built and booted with. this tree is everything on top of it: the
programs in ring 3 and the library they link, git, what goes on the disk, and the
toolchain. the toolchain is ours except for the C compiler, which is gcc: our make,
our assembler and our linker build the kernel out of gcc's objects, and it boots.

where it goes next is [ROADMAP.md](ROADMAP.md).

still a non-goal: being useful in any practical sense. networking stopped
being one in 0.3.0.

## building

you need `gcc`, `nasm`, `make`, `python3`, `git` and `qemu` (any recentish versions). on fedora:

```
dnf install gcc nasm make python3 git qemu-system-x86
```

the kernel is a submodule, so clone with it:

```
git clone --recurse-submodules https://github.com/Onebit5/velvetOS.git
# or, in a clone made without it:
git submodule update --init
```

then:

```
make lint         # the style guide, checked (whole tree, or FILES=userland/ls.c)
make              # the bootable disk image, velvetos.img
make kernel       # just the kernel, built out of kernel/ into bin/kernel/
make run          # boot it in qemu (CPUS=4, MEM=2G)
make test         # the host test suites and the toolchain's checks
make kernel-test  # the kernel's own suites, run in kernel/
make boottest     # boot it and drive the shell over serial
make selfboot     # and boot the kernel our own tools built
```

`make run` gives you a qemu window *and* a serial console in your terminal, and since com1 is wired into the input queue, you can type at either one. the shell cannot tell the difference.

## documentation

the full documentation is in [`docs/`](docs/index.rst), split into pages
about the project and pages about the code:

- [**code style**](docs/coding-style.rst): how to write the code, name things, comment, and handle errors; the TODO/FIXME convention, and `make lint`
- [**contributing**](docs/contributing.rst): which repository a change goes to, the workflow, commit messages, sign-off, review, patches by email, and the rule that **ci is the gate**
- [**building**](docs/building.rst): the toolchain, the submodule, every make target, running it, and the self-hosting build
- [**testing**](docs/testing.rst): the host suites, the checks against other implementations, and the boot test
- [**layout**](docs/layout.rst): the source tree, and the line between it and the kernel
- [**releases**](docs/versioning.rst): how versions are cut, how the kernel is pinned, and why neither is in the log

and one page per part of the system under [`docs/subsystems/`](docs/subsystems/index.rst):
[userland](docs/subsystems/userland.rst), [toolchain](docs/subsystems/toolchain.rst),
[tools](docs/subsystems/tools.rst). the kernel's subsystems, and the system calls,
are documented in the kernel's own [`docs/`](https://github.com/Onebit5/velvet/blob/master/docs/index.rst),
which is `kernel/docs/` in a checkout of this tree.

the project's own history is [CHANGELOG.md](CHANGELOG.md): every version,
newest first, one machine that booted each.

## naming

things that need a name take one from the Persona series, when one fits:
the velvet room, igor, margaret, theodore, lavenza, and the arcana. it is a
flavour rather than a rule, and it is expected to fade after 1.0.0.

## license

GPL-2.0-only, see [LICENSE](LICENSE). Copyright (c) 2026 Jose A. Perez de Azpillaga.
that includes the console font, which is in the kernel: the vga 8x16 from linux's
`lib/fonts/`, GPL-2.0 like the rest.
