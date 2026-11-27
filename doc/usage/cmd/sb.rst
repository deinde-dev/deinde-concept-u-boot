.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: sb (command)

sb command
==========

Synopsis
--------

::

    sb devoff <node>
    sb devon <node>
    sb grid <0|1> [<size>]
    sb handoff
    sb map
    sb state

Description
-----------

The *sb* command is used to display information about sandbox's internal
operation and to manage devices. See :doc:`/arch/sandbox/index` for more
information.

sb devoff
~~~~~~~~~

This disables a device, whether it was bound at startup or by *sb devon*. The
device is removed (deactivated) and unbound from the driver model. The
parameter is the name of a root devicetree node.

sb devon
~~~~~~~~

This enables a device from the device tree. The device tree node is located,
bound to the driver model, and probed (activated). This is useful for testing
devices that are not automatically bound at startup, i.e. those marked as
status = "disabled" in the device tree. The parameter is the name of a root
devicetree node.

sb grid
~~~~~~~

This enables or disables a grid overlay on the video display. When enabled,
a grid is drawn over the display, which is useful for debugging UI layout and
alignment. Use ``sb grid 1`` to enable and ``sb grid 0`` to disable. An
optional second parameter specifies the grid size in pixels, 0x20 by default.
Both parameters are read as hexadecimal, so ``sb grid 1 14`` asks for a grid
of 20 pixels.

The overlay is drawn when the display is next updated, so it appears only
where sandbox has one, i.e. when it is run with the -l flag.

sb handoff
~~~~~~~~~~

This shows the handoff information received from SPL. If U-Boot is started
from an SPL build, it shows the magic number from that information. If it is
started directly, it reports that no handoff arrived.

sb map
~~~~~~

This shows the mappings which map_sysmem() has set up between U-Boot
addresses and the underlying host address-space. A host pointer inside
sandbox's emulated RAM needs no mapping, since its U-Boot address is just its
offset into that RAM, so only pointers outside it are listed.

Fields shown are:

Addr
    Address which U-Boot uses for this mapping. These are allocated from
    ``0xff000000`` upwards, above emulated RAM, so that an ordinary memory
    command can be pointed at a host pointer.

Mapping
    Host pointer which the address maps to.

Refcnt
    Number of references to this mapping, one for each map_sysmem() which
    unmap_sysmem() has yet to undo.

sb state
~~~~~~~~

This shows basic information about the sandbox state, currently just the
command-line with which sandbox was started.

Example
-------

This shows enabling a device from the ``test.dts`` devicetree. Note that
sandbox must be run with the -T flag to use the test devicetree::

    => sb devon mmc11
    Device 'mmc11' enabled
    => ls mmc b
                extlinux/
            7   initrd.img-6.8.0-53-generic
         1616   vmlinuz-6.8.0-53-generic

    2 file(s), 1 dir(s)

    => sb devoff mmc11
    Device 'mmc11' disabled
    => ls mmc b
    ** Bad device specification mmc b **
    Couldn't find partition mmc b
    =>

This shows checking for the presence of SPL-handoff information. For this to
work, ``u-boot-spl`` must be run, from a build which enables ``CONFIG_SPL``,
such as ``sandbox_spl``::

    => sb handoff
    SPL handoff magic 14f93c7b

Where the build supports the handoff but none arrived, since U-Boot was
started directly rather than from SPL::

    => sb handoff
    SPL handoff info not received

This shows output from the *sb map* sub-command, with a single mapping::

    => sb map
    Sandbox memory-mapping
        Addr           Mapping  Refcnt
    ff000000  000056185b46d6d0       2

This shows output from the *sb state* sub-command::

    => sb state
    Arguments:
    /tmp/b/sandbox/u-boot -D

Configuration
-------------

The command itself is available on sandbox only, since it is built with
CONFIG_SANDBOX rather than having a CONFIG_CMD option of its own.

The *sb handoff* sub-command needs CONFIG_HANDOFF. Without it the
sub-command reports that it is not supported and shows the usage message.

Return value
------------

The return value $? is 0 (true) if the sub-command does its work. It is 1
(false) if no sub-command is given or it is not one of those above, if a
sub-command is given the wrong number of arguments, if *sb devon* or
*sb devoff* cannot find the node or cannot bind, probe, remove or unbind the
device, or if *sb handoff* is used on a build without CONFIG_HANDOFF.

See also
--------

* :doc:`bind<bind>` for attaching a driver to a device by name, on any board
* :doc:`unbind<unbind>` for detaching a driver again
* :doc:`dm<dm>` for the devices and uclasses driver model has bound
* :doc:`meminfo<meminfo>` for the regions of memory U-Boot itself uses, as
  opposed to the host mappings *sb map* lists
