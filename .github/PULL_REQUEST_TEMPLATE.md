<!-- thanks for sending a patch. the guide is docs/contributing.rst; this is
     the checklist it asks for, so a reviewer does not have to ask. -->

## what this changes

<!-- a paragraph on what it does and why. the diff already shows how. -->

## subsystem

<!-- arch, boot, mm, sched, fs, drivers, net, lib, shell, user, toolchain,
     tools, docs -->

## checklist

- [ ] branched from the latest `main`
- [ ] one change, and one subsystem, per commit
- [ ] `make lint` passes
- [ ] `make test` passes
- [ ] `make boottest` passes, if it changes what the machine does
- [ ] `make selfboot` passes, if it touches the kernel, userland or tools
- [ ] a new feature brings its documentation in `docs/`
- [ ] signed off with `git commit -s`
- [ ] any model assistance disclosed with an `Assisted-by:` trailer

## notes for review

<!-- what a reviewer should look at first, or a question you want answered.
     if you are fixing an issue, quote the number: `fixes #12`. -->
