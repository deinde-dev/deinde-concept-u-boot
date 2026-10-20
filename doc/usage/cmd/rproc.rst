.. SPDX-License-Identifier: GPL-2.0+
   Copyright 2025 NXP
   Written by Peng Fan <peng.fan@nxp.com>

.. index::
   single: rproc (command)

rproc command
=============

Synopsis
--------

::

    rproc init [<id>]
    rproc list
    rproc load <id> <addr> <size>
    rproc start <id>
    rproc stop <id>
    rproc reset <id>
    rproc is_running <id>
    rproc ping <id>

Description
-----------

The rproc command controls the remote processors of an SoC, meaning the cores
which sit beside the one U-Boot runs on, such as a microcontroller or a DSP.
Each is a device of the UCLASS_REMOTEPROC uclass and is known by its ID, which
is the sequence number of that device.

The *rproc init* command initialises the remote processors, which probes their
devices and calls the init operation of each driver. Given no ID it does this
for every processor and says so if any of them fails; given one it does it for
that processor alone. Every other sub-command needs its processor initialised
first.

Once every processor is initialised, *rproc init* refuses to run again, since
there is nothing left for it to do.

The *rproc list* command shows the processors which are initialised, one per
line, giving the ID, the name, how the processor's memory is reached and the
operations its driver provides. A processor which is not initialised is left
out, so the list is empty until *rproc init* has run.

The *rproc load* command loads an image which is already in memory into a
processor. The driver copies it to wherever that processor runs from, which is
usually not somewhere U-Boot can address directly.

The *rproc start* command starts a processor which has an image, and *rproc
stop* stops it again. *rproc reset* holds it in reset, from where it can be
loaded afresh.

The *rproc is_running* command reports whether a processor is running, and
*rproc ping* asks it to answer, which only a running processor does.

What a sub-command does is up to the driver, and a driver need not provide all
of them. *rproc list* shows the operations each processor supports, and one
which is missing gives an error.

id
    ID of the remote processor, as shown by *rproc list*

addr
    address in memory of the image to load

size
    size of the image in bytes, which cannot be zero

Example
-------

This shows the three remote processors of the sandbox devicetree. Nothing is
listed until they are initialised::

    => rproc list
    => rproc init
    => rproc list
    2 - Name:'proc_3_legacy' type:'internal memory mapped' supports: load start stop reset is_running ping
    0 - Name:'remoteproc-test-dev1' type:'internal memory mapped' supports: load start stop reset is_running ping
    1 - Name:'remoteproc-test-dev2' type:'internal memory mapped' supports: load start stop reset is_running ping

A processor does nothing until it has an image and is started. The sandbox
driver only tracks the state it is asked for, so any address and size will do::

    => rproc is_running 0
    Remote processor is NOT Running
    => rproc load 0 1000 100
    Load Remote Processor 0 with data@addr=0x00001000 256 bytes: Success!
    => rproc start 0
    => rproc is_running 0
    Remote processor is Running
    => rproc ping 0
    Remote processor responds 'Pong'
    => rproc stop 0

A second *rproc init* has nothing to do::

    => rproc init
        Remote Processors are already initialized

Configuration
-------------

The command is only available if CONFIG_CMD_REMOTEPROC=y.

A driver for the processors is needed as well. Each is selected by its own
CONFIG_REMOTEPROC_* option, which selects CONFIG_REMOTEPROC in turn.

Return value
------------

The return value $? is set to 0 (true) if the command succeeds. It is set to 1
(false) if a processor fails to initialise, if every processor is initialised
already, if an ID names no processor, or if the driver cannot do what is asked
of it, which includes an operation it does not provide at all. It is also set
to 1 when the command shows its usage message, which it does without a
sub-command, for one it does not know, for *rproc load* given fewer than three
arguments, and for a size of zero.

Note that *rproc is_running* gives its answer in the message and returns 0
either way. *rproc ping* does the same where the driver reports a silent
processor as such; where it treats that as an error, as the sandbox driver
does, the command returns 1.

See also
--------

* :doc:`cpu<cpu>` for the processors U-Boot itself runs on
* :doc:`fatload<fatload>` for reading an image into memory, which *rproc load*
  then hands to a processor
* :doc:`../../board/nxp/rproc` for the remote processors of the i.MX boards
