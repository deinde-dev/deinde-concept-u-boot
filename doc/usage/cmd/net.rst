.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: net (command)

net command
===========

Synopsis
--------

::

    net list
    net stats <device>

Description
-----------

The net command reports on the Ethernet devices U-Boot knows about. It sends
nothing itself: the commands which do use the network, such as ping, dhcp and
tftpboot, work on the current device, the one named by the *ethact* environment
variable, and 'net list' is how to find out what that variable may be set to.

net list
    Lists every Ethernet device which is bound, one per line::

        eth<seq> : <name> <mac> [active]

    *seq* is the device's sequence number, which comes from the 'ethernet<n>'
    aliases in the devicetree, and *name* is its name in the driver model. The
    *ethact* variable accepts either form, so both 'eth3' and the device's own
    name select the same device.

    *mac* is the MAC address in the environment, from *ethaddr* for the device
    with sequence number 0 and 'eth<n>addr' for the rest. It is not necessarily
    the address the hardware is using, and a device with no variable of its own
    shows as 00:00:00:00:00:00.

    The word 'active' marks the current device. Devices are listed in the
    order they were bound, which is rarely the order of their sequence
    numbers, and none of them is probed in the process.

net stats <device>
    Dumps the hardware counters of one device, one per line, indented by two
    spaces and in the form 'name: value'. The names and their number are
    whatever the driver reports, so two devices on the same board may dump
    different counters.

    *device* is the name from the middle column of 'net list'. The 'eth<n>'
    alias is not accepted here, so 'net stats eth0' reports that the device
    cannot be found. Unlike 'net list' this probes the device.

    Few drivers keep counters. One which does not reports that it does not
    implement a stats dump, and the command fails.

Example
-------

This lists the eight emulated devices of the sandbox board, the first of them
current::

    => net list
    eth0 : eth@10002000 02:00:11:22:33:44 active
    eth5 : eth@10003000 02:00:11:22:33:46
    eth3 : sbe5 02:00:11:22:33:45
    eth6 : eth@10004000 02:00:11:22:33:47
    eth8 : phy-test-eth 02:00:11:22:33:49
    eth4 : dsa-test-eth 02:00:11:22:33:48
    eth2 : lan0 02:00:11:22:33:48
    eth7 : lan1 02:00:11:22:33:48

The sandbox driver keeps no counters, so a dump of one of those devices fails::

    => net stats eth@10002000
    Driver does not implement stats dump!

A dump on an ls2088ardb, whose driver does keep them, begins::

    => net stats DPMAC1@10gbase-r
      [dpni ] rx frames: 7
      [dpni ] rx bytes: 1046
      [dpni ] rx mcast frames: 0
      ...

Return value
------------

The return value $? is 0 (true) if the sub-command succeeds. 'net stats'
returns 1 (false) if the device cannot be found, if its driver keeps no
counters, or if memory for the dump cannot be allocated. A missing or unknown
sub-command, or one given an argument too many, shows the usage message and
also returns 1.

Configuration
-------------

The command is only available if CONFIG_CMD_NET=y, which it is by default. Both
network stacks provide it, CONFIG_NET_LEGACY=y and CONFIG_NET_LWIP=y alike,
since it only asks the driver model and the drivers what they have.

See also
--------

* :doc:`dhcp<dhcp>` for obtaining the network settings over the current device
* :doc:`ping<ping>` for checking that the current device reaches a host
* :doc:`tftpboot<tftpboot>` for loading a file over the current device
* :doc:`mii<mii>` for the state of the PHY attached to a device
* :doc:`mdio<mdio>` for reading the registers of a PHY by address
* :doc:`dm<dm>` for the driver-model tree the devices appear in
