# velvetOS

![ci](https://github.com/Onebit5/velvetOS/actions/workflows/ci.yml/badge.svg)

a tiny 64-bit kernel for x86_64, written in C, booted by a bootloader of its own.

built to make what happens between "power button" and "shell prompt" legible, rather
than to be the next linux. small enough to fit in one head.

where it goes next is [ROADMAP.md](ROADMAP.md). the toolchain is ours
except for the C compiler, which is gcc: our make, our assembler and our
linker build the kernel out of gcc's objects, and it boots.

still a non-goal: being useful in any practical sense. networking stopped
being one in 0.3.0.

## building

you need `gcc`, `nasm`, `make`, `xorriso` and `qemu` (any recentish versions). on fedora:

```
dnf install gcc nasm make xorriso qemu-system-x86
```

then:

```
make lint       # the style guide, checked (whole tree, or FILES=userland/ls.c)
make            # the bootable disk image, velvetos.img
make run        # boot it in qemu (CPUS=4, MEM=2G)
make test       # the host test suites (no qemu needed, takes a second)
make boottest   # boot it and drive the shell over serial
make selfboot   # and boot the kernel our own tools built
```

`make run` gives you a qemu window *and* a serial console in your terminal, and since com1 is wired into the input queue, you can type at either one. the shell cannot tell the difference.

## documentation

the full documentation is in [`docs/`](docs/index.rst), split into pages
about the project and pages about the code:

- [**code style**](docs/coding-style.rst): how to write the code, name things, comment, and handle errors; the TODO/FIXME convention, and `make lint`
- [**contributing**](docs/contributing.rst): the workflow, commit messages, sign-off, review, patches by email, and the rule that **ci is the gate**
- [**building**](docs/building.rst): the toolchain, every make target, running it, and the self-hosting build
- [**testing**](docs/testing.rst): the host suites, the checks against other implementations, and the boot test
- [**layout**](docs/layout.rst): the source tree and the subsystem map
- [**releases**](docs/versioning.rst): how versions are cut, and why they are not in the log

and one page per subsystem under [`docs/subsystems/`](docs/subsystems/index.rst):
[arch](docs/subsystems/arch.rst), [boot](docs/subsystems/boot.rst),
[mm](docs/subsystems/mm.rst), [sched](docs/subsystems/sched.rst),
[fs](docs/subsystems/fs.rst), [drivers](docs/subsystems/drivers.rst),
[net](docs/subsystems/net.rst), [lib](docs/subsystems/lib.rst),
[shell](docs/subsystems/shell.rst), [userland](docs/subsystems/userland.rst),
[toolchain](docs/subsystems/toolchain.rst), [tools](docs/subsystems/tools.rst).

the project's own history is [CHANGELOG.md](CHANGELOG.md): every version,
newest first, one machine that booted each.

## naming

things that need a name take one from the Persona series, when one fits:
the velvet room, igor, margaret, theodore, lavenza, and the arcana. it is a
flavour rather than a rule, and it is expected to fade after 1.0.0.

## license

GPL-2.0-only, see [LICENSE](LICENSE). Copyright (c) 2026 Jose A. Perez de Azpillaga. the console font is [spleen 8x16](https://github.com/fcambus/spleen) by frederic cambus, bsd 2-clause, see [FONT-LICENSE](FONT-LICENSE).
