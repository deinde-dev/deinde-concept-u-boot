.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: mii (command)

mii command
===========

Synopsis
--------

::

    mii device
    mii device <devname>
    mii info [<addr>]
    mii read <addr> <reg>
    mii write <addr> <reg> <data>
    mii modify <addr> <reg> <data> <mask>
    mii dump <addr> <reg>

Description
-----------

The mii command reads and writes the registers of an Ethernet PHY over the
MII management interface, the two-wire serial interface described by clause 22
of IEEE 802.3. Up to 32 PHYs share a bus, each with up to 32 registers of 16
bits, and the first few of those registers are the same on every PHY, which is
what lets this command report a link or set a speed without knowing which PHY
it is talking to.

All numbers are given in hex, without a leading 0x. Both <addr> and <reg> may
be a range such as 2-7, in which case the sub-command is repeated over every
value in it.

A bus is called a device here, and the command works on one of them at a time,
the current device. Where a board has a single bus there is nothing to choose
and the sub-commands below can be used straight away.

device
    With no argument, list the buses and say which one is current. With a name,
    make that bus current; the name is the one the list shows.

info
    Report the PHY found at each address, reading the two PHY identifier
    registers to do so. An address, or a range, limits the search; without one
    every address from 0 to 31 is tried. An address with no PHY on it is passed
    over silently, so a board with one PHY prints a single line.

read
    Show the contents of a register, as four hex digits. Where more than one
    register or address is asked for, each line names the address and register
    it belongs to.

write
    Write a value to a register. Nothing is printed if the write succeeds.

modify
    Change only the bits of a register which are set in <mask>, leaving the
    rest as they are. This reads the register, merges in <data>, and writes it
    back.

dump
    Show a register field by field, naming each field and giving its value.
    Only the registers which clause 22 defines are understood, which is 0 to 5
    and 9 to a; a higher register number is refused.

The sub-command is recognised by its first letter, except for device and dump,
which both begin with a d and so need two. The full words are accepted as
well, and anything else shows the usage message.

Pressing Enter repeats the last sub-command with the same arguments, which is
a quick way to watch a register change as a link comes up.

Buses which the driver model provides appear here only once they have been
probed, which the :doc:`mdio<mdio>` command does for every MDIO bus in the
system. Where the mii command reports no device at all, run 'mdio list'
first.

Example
-------

This is on sandbox, whose emulated MDIO bus has one PHY at address 5 with two
registers. The buses come from the driver model, so the mdio command is used
first to probe them::

    => mdio list
    mdio-test:
    mdio-ch-test@0:
    mdio-ch-test@1:
    => mii device
    MII devices: 'mdio-test' 'mdio-ch-test@0' 'mdio-ch-test@1'
    Current device: 'mdio-test'

Writing a register and reading it back::

    => mii write 5 0 1140
    => mii read 5 0
    1140

Clearing the two speed-selection bits, leaving the rest of the register
alone::

    => mii modify 5 0 0000 2040
    => mii read 5 0
    1100

A range names each value it shows::

    => mii read 5 0-1
    addr=05 reg=00 data=1100
    addr=05 reg=01 data=0000

Showing the PHY control register field by field, with the speed set to 1000
Mbps and full duplex::

    => mii write 5 0 1140
    => mii dump 5 0
    0.     (1140)                 -- PHY control register --
      (8000:0000) 0.15    =     0     reset
      (4000:0000) 0.14    =     0     loopback
      (2040:0040) 0. 6,13 =   b10    speed selection = 1000 Mbps
      (1000:1000) 0.12    =     1     A/N enable
      (0800:0000) 0.11    =     0     power-down
      (0400:0000) 0.10    =     0     isolate
      (0200:0000) 0. 9    =     0     restart A/N
      (0100:0100) 0. 8    =     1     duplex = full
      (0080:0000) 0. 7    =     0     collision test enable
      (003f:0000) 0. 5- 0 =     0     (reserved)

An address with no PHY on it is reported, one line for each register asked
for::

    => mii read 6 0
    Error reading from the PHY addr=06 reg=00

On a board with a real PHY, 'mii info' identifies it. This is a BeagleBone
Black, whose LAN8710A sits at address 0 with the link up at 100 Mbps::

    => mii info
    PHY 0x00: OUI = 0x01F0, Model = 0x0F, Rev = 0x01, 100baseT, FDX

The OUI, model and revision come from the two identifier registers, so they
name the make and type of the PHY; the speed and duplex are what the link has
settled on.

Configuration
-------------

The command is only available if CONFIG_CMD_MII=y. CONFIG_MII_INIT=y makes it
call the board's mii_init() before each operation, which a few older boards
need to set their bus up.

Return value
------------

The return value $? is 0 (true) when every read, write or modify in the ranges
given succeeds. It is 1 (false) when one of them fails, in which case the
address and register are reported, and when a dump is asked for beyond the
registers clause 22 defines.

The usage message is shown, which is also false, when no sub-command is given,
when the word given is not one of those above, or when an address above 31 is
asked for.

Naming a device which does not exist reports that, but is not itself a
failure: the current device is left as it was and the return value is 0.

See also
--------

* :doc:`mdio<mdio>` for the same job through the driver-model MDIO uclass,
  which also reaches clause 45 registers
* :doc:`ethsw<ethsw>` for the switch which the PHYs of a board with one sit
  behind
* :doc:`dm<dm>` for checking which MDIO buses are bound and which have probed
