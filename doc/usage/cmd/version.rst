.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: version (command)

version command
===============

Synopsis
--------

::

    version

Description
-----------

The version command shows which U-Boot this is and which tools built it. It
takes no arguments and prints:

* the U-Boot version string, which is the banner U-Boot shows as it starts:
  the release, the commit the tree was at, and the date, time and time zone of
  the build, followed by CONFIG_IDENT_STRING where a board sets one. A blank
  line follows it.
* the version of the C compiler which compiled U-Boot
* the version of the linker which linked it

The compiler and linker lines are the first line of ``$(CC) --version`` and
``$(LD) --version``, recorded when U-Boot was built, so they name the
toolchain which produced this binary rather than whichever one is installed
now.

Where the build sets a BUILD_TAG, it is appended to the version string as
``, Build: <tag>``. On a board which starts from coreboot
(CONFIG_SYS_COREBOOT) a fourth line gives the coreboot version and build
string, read from the sysinfo table coreboot leaves behind.

Example
-------

This is a sandbox build::

    => version
    Deinde Concept U-Boot 2026.10-rc4-g65b55eff7f5a (Oct 27 2026 - 05:36:20 -0600)

    gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
    GNU ld (GNU Binutils for Ubuntu) 2.42
    =>

Return value
------------

The return value $? is always 0 (true), since there is nothing which can
fail. An argument, or an option, shows the usage message instead and sets $?
to 1 (false).

Configuration
-------------

The command is available on any board with a command line: cmd/version.c is
built whenever CONFIG_CMDLINE=y, with no symbol of its own.

With CONFIG_VERSION_VARIABLE=y the environment variable *ver* holds the
version string, so a script can read it without running the command. Note
that it is the version string alone, without the compiler and linker lines.

See also
--------

* :doc:`ufetch<ufetch>` for the version alongside a summary of the board
* :doc:`bdinfo<bdinfo>` for what this build is running on, rather than how it
  was made
* :doc:`cbsysinfo<cbsysinfo>` for the rest of the coreboot sysinfo table which
  the coreboot line comes from
