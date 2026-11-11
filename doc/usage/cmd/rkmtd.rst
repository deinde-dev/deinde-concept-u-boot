.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: rkmtd (command)

rkmtd command
=============

Synopsis
--------

::

    rkmtd bind <label>
    rkmtd unbind <label|seq>
    rkmtd info [<label|seq>]
    rkmtd dev [<label|seq>]

Description
-----------

The rkmtd command creates a virtual block device which holds the Rockchip boot
blocks of a raw NAND chip. The boot blocks live in a layout which only the
Rockchip boot ROM understands, so they cannot be reached with the ordinary
:doc:`nand<nand>` or :doc:`mtd<mtd>` commands. Binding an rkmtd device scans
the chip for them and presents them as a GPT-formatted disk of 609 sectors
with a single partition, 'loader1', starting at sector 64. Writing an
'idbloader.img' to that partition erases up to five NAND erase blocks and
writes the boot blocks back in the pattern the NAND ID calls for.

The point of the exercise is that a block device can be handed to a host over
USB with :doc:`ums<ums>` or *rockusb*, so that a tool on the host can read and
write the boot blocks with no knowledge of the layout. See
:doc:`../../board/rockchip/rkmtd` for the host-side commands which work with
it, and for the limitations of the driver.

Common arguments:

label|seq
    This selects an rkmtd device, either by the label given when it is bound
    or by its sequence number in hex. A value matching neither causes the
    command to fail.

rkmtd bind
~~~~~~~~~~

This creates a new rkmtd device, attaches the Rockchip MTD driver to it and
scans the NAND chip for boot blocks.

Arguments:

label
    Label to identify this device by. This can be any string. Binding a
    second device with a label already in use replaces the first one.

rkmtd unbind
~~~~~~~~~~~~

This detaches and removes a device which was bound earlier. The sequence
numbers of the other devices stay as they are.

rkmtd info
~~~~~~~~~~

This shows the sequence number, size in blocks and label of one device, or of
all of them when no device is named.

rkmtd dev
~~~~~~~~~

With an argument this makes that device current; with none it shows which
device is current.

Example
-------

Initially there are no devices::

    => rkmtd info
    dev       blocks label

Bind two devices and list them::

    => rkmtd bind test1
    => rkmtd bind test2
    => rkmtd info
    dev       blocks label
      0          609 test1
      1          609 test2

Show a single device::

    => rkmtd info test2
    dev       blocks label
      1          609 test2

The devices appear as block devices, so the boot blocks can be read and
written with any command which takes one. The other drivers which 'lsblk'
lists are left out here::

    => lsblk
    Block Driver          Devices
    -----------------------------
    rkmtd_blk           : rkmtd 0, rkmtd 1

A 'part list rkmtd 0' then shows the single 'loader1' partition, running from
sector 0x40 to sector 0x23f.

Select a device by label or by sequence number::

    => rkmtd dev
    No current rkmtd device
    => rkmtd dev test2
    => rkmtd dev
    Current rkmtd device: 1: test2
    => rkmtd dev 0
    => rkmtd dev
    Current rkmtd device: 0: test1

Unbind a device::

    => rkmtd unbind test1
    => rkmtd info
    dev       blocks label
      1          609 test2

Return value
------------

The return value $? is 0 (true) if the command succeeds. It is 1 (false) if a
named device does not exist, if 'rkmtd bind' cannot create the device or
attach the driver, if 'rkmtd unbind' cannot detach or unbind it, or if
'rkmtd dev' is asked for a current device when none is selected.

See also
--------

* :doc:`../../board/rockchip/rkmtd` for the Linux-side tools which read and
  write the boot blocks this command exposes
* :doc:`ums<ums>` for handing one of these devices to a host over USB
* :doc:`nand<nand>` for the rest of the NAND chip, which this command does
  not touch
* :doc:`lsblk<lsblk>` for the devices this command creates listed with the
  block devices of every other driver
* :doc:`host<host>` for the sandbox command which binds and unbinds block
  devices the same way
