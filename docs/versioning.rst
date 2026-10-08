releases
========

velvetOS does not put version numbers in commits. the log is a record of
changes, not a sequence of numbered drops: a commit says what it changed and
why, and nothing about which release it will end up in. the numbers live in
**github releases**, and a release is cut from ``main`` when a milestone is
done.

how a release happens
---------------------

#. the work for a roadmap entry is merged, with its tests, and CI is green
   on ``main``.
#. ``base/ramdisk/etc/os-release`` is bumped to the new number.
#. the ``CHANGELOG.md`` entry is added, in the roadmap's words, with the
   parts that turned out to matter written in.
#. the release is published on github, tagged ``vX.Y.Z``, with that changelog
   entry as its notes.

the tag and the release are the version. no commit in the history names it.

the kernel, pinned
------------------

the kernel is velvet, released on its own with numbers of its own, and
``kernel/`` is a submodule pinned to one of its commits. until 0.4.0 the two
were one tree with one version; from 0.4.0 the system's number is the
system's, so velvetOS 0.4.3 may well ship velvet 0.4.1, and the
changelog entry says which.

a kernel release reaches this tree only when the pin is moved, which is an
ordinary change with the same gate as any other:

.. code-block:: sh

   git -C kernel fetch --tags
   git -C kernel checkout v0.4.1      # the kernel release to ship
   make test && make boottest && make selfboot
   git add kernel
   git commit -s                      # kernel: what the new one brings

the commit says what the new kernel brings and why this system wants it, the
same as any commit, and CI holds it to the same bar. it does not name the
version: the diff already says which commit the pin moved to, and the log
stays a record of changes rather than of numbers. a pin is moved to a
release tag rather than to whatever velvet's ``master`` is that day,
because a release is a kernel its own CI has booted, and a pin is a claim
that this system has booted it too.

the 0.x milestones
------------------

until 1.0.0 the rule that makes a number worth anything is simple:

    every version is a machine that boots.

a version that only builds, or only passes its tests, is not a version. the
number is a claim about a disk, and the claim is checked the same way
everything else is, by the boot test in CI. that is why there are eighty-odd
of them instead of five, and why a release is cut only when the machine it
names has been watched to start.

it has a second edge worth saying plainly: because each version boots, a
regression can be bisected to the release that owns it. the `testing`_ page
describes the checks; the changelog says which version introduced the thing
that broke.

after 1.0.0
-----------

once the project reaches 1.0.0 the milestone rule stops. from there the
numbers are ordinary versions, bumped when they grow unwieldy rather than
because a machine learned something, and a number no longer carries the
sentence a 0.x number carried. the releases keep happening; the ceremony
does not.

where a version lives
---------------------

``ROADMAP.md <../ROADMAP.md>``
   the plan. one entry per version, each entry the sentence that version is
   about. everything after 0.4.x is written there too, so the shape of the
   project is public before the work is done. the roadmap also names the
   non-goals, and those are as much a part of the plan as the goals.

``CHANGELOG.md <../CHANGELOG.md>``
   the record, newest first, one entry per release, saying what changed and
   why, not how, which is what the commit log is for. the release notes are
   this entry, copied.

``base/ramdisk/etc/os-release``
   the one place the system's version is written down, on the medium, for
   anything that asks what it is running on. the kernel's version is the
   kernel's, in ``kernel/version.h``, and is what the boot banner,
   ``arcana`` and ``persona`` print.

all three have to agree at the moment a release is cut. a version in one and
not the others is the kind of inconsistency that looks fine until a machine
is asked what it is.

a release that turns out to be wrong is not edited and re-tagged. the next
one fixes it, and the changelog says so, because a changelog that gets
quietly corrected is a changelog nobody can trust about the past.

.. _testing: testing.rst
