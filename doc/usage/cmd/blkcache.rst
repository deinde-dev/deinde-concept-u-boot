.. SPDX-License-Identifier: GPL-2.0+
.. Copyright 2023, Heinrich Schuchardt <heinrich.schuchardt@canonical.com>

.. index::
   single: blkcache (command)

blkcache command
================

Synopsis
--------

::

    blkcache show
    blkcache configure <blocks> <entries>

Description
-----------

The *blkcache* command is used to control the size of the block cache and to
display statistics.

The block cache buffers data read from block devices. This speeds up the access
to file-systems.

show
    show the statistics and reset the hit and miss counts

configure
    set the maximum number of blocks per cache entry and the maximum number of
    cache entries

blocks
    maximum number of blocks per cache entry. The block size is device specific.
    The initial value is 8. A read of more blocks than this is not cached.

entries
    maximum number of entries in the cache. The initial value is 32. Setting
    this to 0 turns the cache off, which can help where it hurts performance,
    for instance with the *ums* command.

Changing either limit empties the cache. Writing to a device, erasing it,
probing it or selecting an mmc device drops the entries for that device alone.

Example
-------

.. code-block::

    => blkcache show
    hits: 296
    misses: 149
    entries: 7
    max blocks/entry: 8
    max cache entries: 32
    => blkcache show
    hits: 0
    misses: 0
    entries: 7
    max blocks/entry: 8
    max cache entries: 32
    => blkcache configure 16 64
    changed to max of 64 entries of 16 blocks each
    => blkcache show
    hits: 0
    misses: 0
    entries: 0
    max blocks/entry: 16
    max cache entries: 64
    =>

Return value
------------

The return value $? is set to 0 (true) if the command succeeds. It is set to 1
(false) if no sub-command is given, if the sub-command is not one the command
knows, or if *configure* is given anything other than two arguments; the usage
message is shown in each case.

Configuration
-------------

The blkcache command is only available if CONFIG_CMD_BLOCK_CACHE=y.

See also
--------

* :doc:`read<read>` for reading blocks from a device, which is what fills the
  cache
* :doc:`write<write>` for writing blocks, which drops the cached blocks of the
  device written to
* :doc:`part<part>` for the partition table of a device, which is read through
  the cache as well
* :doc:`blkmap<blkmap>` for making a block device out of memory, whose reads
  are cached like any other
