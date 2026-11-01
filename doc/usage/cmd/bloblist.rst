.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: bloblist (command)

bloblist command
================

Synopsis
--------

::

    bloblist info
    bloblist list

Description
-----------

The bloblist command shows the contents of the bloblist, a collection of
binary blobs held in memory which survives from one phase of the boot to the
next. Each blob has a tag which says what it holds, so that the code owning it
can find it again in a later phase. See :doc:`../../develop/bloblist` for the
design and for the list of tags.

The command only reads the bloblist, so it is safe to use at any point. It is
not needed for the bloblist to work; it is a way of seeing what the earlier
phases of the boot have left behind.

bloblist info
~~~~~~~~~~~~~

This shows how much of the bloblist is in use, as four lines:

base
    address of the bloblist header

total size
    size of the whole region the bloblist may grow into

used size
    size of the header and the blobs in it

free
    space left for more blobs

Each size is shown twice, first as a hex byte count and then in the units
which suit it best.

bloblist list
~~~~~~~~~~~~~

This lists the blobs, one per line:

Address
    address of the blob's data, which is where the header ends, not the
    address of the record itself

Size
    size of the data, in hex bytes

Tag
    the blob's tag, in hex

Name
    what the tag means, or 'invalid' for a tag the build does not know

Example
-------

This shows the bloblist on sandbox, which holds the log of EFI calls::

    => bloblist info
    base:       66a6000
    total size: 40000  256 KiB
    used size:  10020  64 KiB
    free:       2ffe0  192 KiB
    => bloblist list
    Address       Size   Tag Name
    066a6020     10000  fff003 EFI-call log

The address a blob is listed at can be fed straight to :doc:`md<md>`::

    => md.b 066a6020 10
    066a6020: c4 05 00 00 00 00 01 00 00 00 00 00 0a 00 00 00  ................

Giving no sub-command, or one which is not recognised, shows the usage
message::

    => bloblist
    bloblist - Bloblists

    Usage:
    bloblist info   - show information about the bloblist
    bloblist list   - list blobs in the bloblist

Return value
------------

The return value $? is 0 (true) for both sub-commands, neither of which can
fail, and 1 (false) where no sub-command is given or the one given is not
recognised.

Configuration
-------------

The command is only available if CONFIG_CMD_BLOBLIST=y, which needs
CONFIG_BLOBLIST=y and is enabled by default where that is set.

See also
--------

* :doc:`meminfo<meminfo>` for where the bloblist sits among the other regions
  of memory
* :doc:`md<md>` for dumping the contents of a blob at the address this command
  shows
