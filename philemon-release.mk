# the loader this system boots with: a release of philemon, by its version.
#
# `make` clones the tag for it, vX.Y.Z, into obj/philemon/, the way
# kernel-release.mk names the kernel, and clones again only when this file
# names another release. a released tag is never moved, so the version is
# all there is to write down. `make philemon-update` moves this file to the
# newest stable release and fetches it.
PHILEMON_VERSION := 1.0.1
PHILEMON_URL     ?= https://github.com/Onebit5/philemon.git
