.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: mdio (command)

mdio command
============

Synopsis
--------

::

    mdio list
    mdio read <phydev> [<devad>.]<reg>
    mdio write <phydev> [<devad>.]<reg> <data>
    mdio rx <phydev> [<devad>.]<reg>
    mdio wx <phydev> [<devad>.]<reg> <data>

Description
-----------

The mdio command reads and writes the registers of an Ethernet PHY over an
MDIO bus. It covers clause 45 of IEEE 802.3, which extends the two-wire
management interface of clause 22 by addressing a device inside the PHY as
well as a register within it, so that a single PHY can hold far more than the
32 registers clause 22 allows.

All numbers are given in hex. Where the buses come from the driver model
(CONFIG_DM_MDIO=y), every one of them is probed before each operation, so
there is nothing to set up first.

list
    Show each bus by name, and under it each PHY which has been attached to
    it, by driver name and by the Ethernet device it serves.

read
    Show the contents of a register. The bus name, the PHY address and the
    register are each printed, so the output says what was read as well as
    what was in it.

write
    Write a value to a register. Nothing is printed if the write succeeds.

rx, wx
    Read or write an extended register, through the readext() and writeext()
    methods of the PHY's driver. These reach registers which are specific to
    one make of PHY, so they work only where the PHY is known to U-Boot and
    its driver provides the method; the PHY must be named by its Ethernet
    device for the driver to be found.

The PHY to work on is given as <phydev>, which may be:

<busname> <addr>
    A bus by name, as the list shows it, and an address on that bus.

<addr>
    An address on the bus which the last mdio command used. The list of
    Ethernet devices is searched for this name first, which reports that the
    address is 'not a known ethernet' before the address itself is tried.

<eth name>
    The name of an Ethernet device, as the 'Net:' line of the boot output
    gives it. This finds the PHY which that device uses, wherever it is.

The address, the device address and the register may each be a range, such as
1-5.4-0x1f, in which case the operation is repeated over every value in it.
A register given without a '<devad>.' prefix is read with no device address,
which is what a clause 22 PHY expects.

Example
-------

This is on sandbox, whose emulated MDIO bus has one PHY at address 5 with two
registers, and a two-channel mux over that bus. No PHY driver is attached to
any of them, so the listing shows the buses alone::

    => mdio list
    mdio-test:
    mdio-ch-test@0:
    mdio-ch-test@1:

Writing a register and reading it back::

    => mdio write mdio-test 5 0 1140
    => mdio read mdio-test 5 0
    Reading from bus mdio-test
    PHY at address 5:
    0 - 0x1140

A range covers both registers of the PHY::

    => mdio write mdio-test 5 1 abcd
    => mdio read mdio-test 5 0-1
    Reading from bus mdio-test
    PHY at address 5:
    0 - 0x1140
    1 - 0xabcd

Giving only the address uses the bus of the previous command, after reporting
that the address is not the name of an Ethernet device::

    => mdio read 5 0
    5 is not a known ethernet
    Reading from bus mdio-test
    PHY at address 5:
    0 - 0x1140

A device address which the PHY does not answer for is reported as an error::

    => mdio read mdio-test 5 1.0
    Reading from bus mdio-test
    PHY at address 5:
    Error

Configuration
-------------

The command is only available if CONFIG_CMD_MDIO=y, which requires
CONFIG_PHYLIB=y. CONFIG_CMD_MII=y implies it.

Return value
------------

The return value $? is 0 (true) when a bus is found for the operation, and 1
(false) when it is not: a bus name which does not exist, an Ethernet device
which has no PHY, or a PHY whose driver does not provide the extended access
asked for. The usage message is shown, which is also false, when no
sub-command is given.

A read or write which the bus itself refuses is reported as an error but does
not change the return value, so a failed access still gives 0. A sub-command
which is not one of those above does nothing and also gives 0.

See also
--------

* :doc:`mii<mii>` for the same registers through the clause 22 interface,
  which also decodes the standard registers field by field
* :doc:`ethsw<ethsw>` for the switch which the PHYs of a board with one sit
  behind
* :doc:`dm<dm>` for checking which MDIO buses are bound and which have probed
