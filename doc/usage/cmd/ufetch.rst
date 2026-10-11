.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: ufetch (command)

ufetch command
==============

Synopsis
--------

::

    ufetch [-n]

Description
-----------

The ufetch command prints a summary of U-Boot and the board it is running on,
in the style of the neofetch utility: a block of fields beside an ASCII-art
U-Boot logo. Everything it shows is already available from another command,
so it is a convenient overview rather than a source of new information.

The report starts with the first *compatible* string of the root node of the
devicetree, underlined, and then gives:

Kernel
    the U-Boot version, as the *version* command reports it. The field is
    named for the kernel which neofetch would be showing on an operating
    system

Config
    the defconfig the build came from, shown only where
    CONFIG_SYS_CONFIG_NAME is defined

Host
    the *model* property of the root node, omitted if there is none

Uptime
    the time since U-Boot started, in seconds

IP Address
    the *ipaddr* environment variable, or 'none' if it is unset, followed by
    *ipv6addr* where that is set

Commands
    the number of commands this build carries

Consoles
    the *stdout* environment variable and the baud rate, where one is set

Features
    which of networking, the EFI loader and the cat command are built in. On
    arm64 the exception level U-Boot is running at is added, since running at
    EL2 or EL3 decides what it is able to do

Relocated
    the address U-Boot has relocated itself to, or 'no' where it has not

CPU
    the architecture and the number of *cpu* nodes under /cpus

Memory
    the total size of the DRAM banks

The remaining lines are the block devices, up to ten of them, each shown as
its uclass, its device number and its size, or 'No media' where the device
holds none.

The -n option prints the fields alone, without the logo.

Note that the logo and the field names are drawn with ANSI escape sequences
for colour, so the output is best read on a terminal which handles them.

Example
-------

This is a sandbox build, which has no media in any of its three mmc devices::

    => ufetch -n
    sandbox
    -------
    Kernel: Deinde Concept U-Boot 2026.10-rc4-gfc78dd80ff41
    Config: sandbox_defconfig
    Host: sandbox
    Uptime: 3048243 seconds
    IP Address: 192.0.2.1
    Commands: 264 (help)
    Consoles: serial,vidconsole (115200 baud)
    Features: Net, EFI, cat :3
    Relocated: to 0x00e75f000
    CPU: sandbox (3 cores, 1 in use)
    Memory: 256 MiB
     mmc 2: No media
     mmc 1: No media
     mmc 0: No media

The uptime is long because sandbox reads the host clock rather than counting
from when U-Boot started.

Dropping the -n draws the logo to the left of the same fields, in blue and
yellow, which takes 25 lines whether or not there are fields left to show.

Return value
------------

The return value $? is always 0 (true), since every field is optional and a
missing one is simply left out. An option other than -n, or more than one
argument, prints the usage message and sets $? to 1 (false).

Configuration
-------------

The command is only available if CONFIG_CMD_UFETCH=y.

See also
--------

* :doc:`bdinfo<bdinfo>` for the board information in full, rather than a
  summary of it
* :doc:`coninfo<coninfo>` for the console devices behind the Consoles line
* :doc:`cpu<cpu>` for what the CPU line leaves out
* :doc:`meminfo<meminfo>` for where the memory ufetch counts has gone
* :doc:`mmc<mmc>` for the block devices the last lines list
