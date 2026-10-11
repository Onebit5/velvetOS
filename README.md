# velvetOS

![ci](https://github.com/Onebit5/velvetOS/actions/workflows/ci.yml/badge.svg)

a tiny 64-bit system for x86_64: a kernel of its own underneath, a libc and programs
to run on it, and the assembler, linker and make that build it.

built to make what happens between "power button" and "shell prompt" legible, rather
than to be the next linux. small enough to fit in one head.

the kernel is [velvet](https://github.com/Onebit5/velvet), a project of its own since
0.4.0. this tree does not carry it: [kernel-release.mk](kernel-release.mk) names the
velvet release the system is built on, by its version, and the build clones that
release's tag into `obj/velvet/`, with the rest of what it makes. velvet is heading
towards linux's interface to programs, and the programs here follow it through the
kernel's uapi headers rather than copies of them, so a kernel that speaks linux's
numbers is a rebuild away rather than a port.

this tree is everything on top of the kernel: the programs in ring 3 and the library
they link, git, what goes on the disk, and the toolchain. the toolchain is ours
except for the C compiler, which is gcc: our make, our assembler and our linker build
the kernel out of gcc's objects, and it boots.

where it goes next is [ROADMAP.md](ROADMAP.md), with the non-goals at the top.

still a non-goal: being useful in any practical sense. networking stopped
being one in 0.3.0.

## building

you need `gcc`, `nasm`, `make`, `python3`, `git` and `qemu` (any recentish versions). on fedora:

```
dnf install gcc nasm make python3 git qemu-system-x86
```

then a plain clone, since the first build fetches the kernel for itself:

```
git clone https://github.com/Onebit5/velvetOS.git
cd velvetOS
make lint           # the style guide, checked (whole tree, or FILES=userland/ls.c)
make                # the bootable disk image, velvetos.img
make kernel         # just the kernel, built out of obj/velvet/ into bin/kernel/
make run            # boot it in qemu (CPUS=4, MEM=2G)
make test           # the host test suites and the toolchain's checks
make kernel-test    # the kernel's own suites, run in its checkout
make boottest       # boot it and drive the shell over serial
make selfboot       # and boot the kernel our own tools built
make kernel-update  # move kernel-release.mk to the newest stable velvet release
```

`make run` gives you a qemu window *and* a serial console in your terminal, and since com1 is wired into the input queue, you can type at either one. the shell cannot tell the difference.

to build against a velvet of your own rather than the release on github,
`make KERNEL_URL=../velvet` fetches the same release out of a local copy, and
`make KERNEL_SRC=../velvet` builds that checkout as it is, for working on the kernel
and the system at once. [docs/building.rst](docs/building.rst) has the rest.

## documentation

the full documentation is in [`docs/`](docs/index.rst), split into pages
about the project and pages about the code:

- [**code style**](docs/coding-style.rst): how to write the code, name things, comment, and handle errors; the TODO/FIXME convention, and `make lint`
- [**contributing**](docs/contributing.rst): which repository a change goes to, the workflow, commit messages, sign-off, review, patches by email, and the rule that **ci is the gate**
- [**building**](docs/building.rst): the toolchain, the kernel release, every make target, running it, and the self-hosting build
- [**testing**](docs/testing.rst): the host suites, the checks against other implementations, and the boot test
- [**layout**](docs/layout.rst): the source tree, and the line between it and the kernel
- [**releases**](docs/versioning.rst): how versions are cut, how the system moves to a new kernel, and why neither is in the log

and one page per part of the system under [`docs/subsystems/`](docs/subsystems/index.rst):
[userland](docs/subsystems/userland.rst), [toolchain](docs/subsystems/toolchain.rst),
[tools](docs/subsystems/tools.rst). the kernel's subsystems, and the system calls,
are documented in the kernel's own [`docs/`](https://github.com/Onebit5/velvet/blob/master/docs/index.rst),
which is `obj/velvet/docs/` once a build has fetched it.

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
