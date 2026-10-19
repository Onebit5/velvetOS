# the kernel this system is built on: a release of velvet, by its version.
#
# `make` clones the tag for it, vX.Y.Z, into obj/velvet/, which git here
# ignores, and clones again only when this file names another release.
# a released tag is never moved (velvet's docs/versioning.rst), so the
# version is all there is to write down.
# `make kernel-update` moves this file to the newest stable release and
# fetches it, and committing the file is how the system moves to a new
# kernel: one line in the log that says which kernel every build after it
# ships.
KERNEL_VERSION := 0.4.2
KERNEL_URL     ?= https://github.com/Onebit5/velvet.git
