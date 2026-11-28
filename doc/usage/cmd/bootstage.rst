.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: bootstage (command)

bootstage command
=================

Synopsis
--------

::

    bootstage report
    bootstage save
    bootstage restore
    bootstage stash [<start> [<size>]]
    bootstage unstash [<start> [<size>]]

Description
-----------

The *bootstage* command provides access to U-Boot's boot-timing records.

bootstage report
    Print a report of all bootstage records, showing the timestamp and elapsed
    time for each stage. Stages which are recorded as an accumulated time,
    rather than as a point in the boot, are listed separately with the number
    of calls.

bootstage save
    Save the current record count to the *bootstage_count* environment
    variable. This can be used to snapshot the bootstage state before an
    operation that adds records.

bootstage restore
    Restore the record count from *bootstage_count*, discarding any records
    added since the last save. This is used by the Python test framework to
    prevent records from accumulating across tests. The count is left alone
    if *bootstage_count* is not set.

bootstage stash
    Stash bootstage data into memory, so that it survives into the next phase
    or into the OS. Only available when ``CONFIG_BOOTSTAGE_STASH`` is enabled.

bootstage unstash
    Read back previously stashed bootstage data from memory, adding the
    records it holds to those already in the table. Only available when
    ``CONFIG_BOOTSTAGE_STASH`` is enabled.

start
    Address to stash the data at, or read it from. This defaults to
    ``CONFIG_BOOTSTAGE_STASH_ADDR``, which is 0 unless the board sets it.

size
    Size of the stash area in bytes. This defaults to
    ``CONFIG_BOOTSTAGE_STASH_SIZE``.

Configuration
-------------

The *bootstage* command is available when ``CONFIG_BOOTSTAGE`` is enabled.

CONFIG_BOOTSTAGE_REPORT
    Enable output of a boot-time report before booting the OS.

CONFIG_BOOTSTAGE_RECORD_COUNT
    Number of bootstage records to store (default 50).

CONFIG_BOOTSTAGE_SAVE
    Enable the *save* and *restore* subcommands. Default y when
    ``CONFIG_UNIT_TEST`` is set.

CONFIG_BOOTSTAGE_STASH
    Enable the *stash* and *unstash* subcommands for passing bootstage
    data to the OS via memory.

Example
-------

Print the records collected so far::

    => bootstage report
    Timer summary in microseconds (8 records):
           Mark    Elapsed  Stage
              0          0  reset
              0          0  board_init_f
        100,321    100,321  board_init_r
        140,602     40,281  eth_common_init
        140,701         99  eth_initialize

    Accumulated time:
                     1,108  dm_f (1 call, 1108 us each)
                       873  of_live (1 call, 873 us each)
                     9,804  dm_r (1 call, 9804 us each)

Note the record count covers the accumulated stages as well as the marks.

Save the count, then put it back to drop whatever the commands in between
record::

    => bootstage save
    => printenv bootstage_count
    bootstage_count=8
    => bootstage restore

Write the records to memory and read them back. Both sub-commands are silent
when they work, and unstashing adds to the table rather than replacing it, so
the report then shows each stage twice::

    => bootstage stash 1000000 1000
    => bootstage unstash 1000000 1000
    => bootstage report
    Timer summary in microseconds (16 records):
           Mark    Elapsed  Stage
              0          0  reset
              0          0  board_init_f
         89,752     89,752  board_init_r
        118,615     28,863  eth_common_init
        118,715        100  eth_initialize
              0          0  reset
              0          0  board_init_f
         89,752     89,752  board_init_r
        118,615     28,863  eth_common_init
        118,715        100  eth_initialize

    Accumulated time:
                     1,126  dm_f (1 call, 1126 us each)
                       883  of_live (1 call, 883 us each)
                     8,440  dm_r (1 call, 8440 us each)
                     1,126  dm_f (1 call, 1126 us each)
                       883  of_live (1 call, 883 us each)
                     8,440  dm_r (1 call, 8440 us each)

An address of all ones means that no stash area is defined::

    => bootstage stash ffffffffffffffff
    No bootstage stash area defined

Return value
------------

The return value $? is 0 (true) on success and 1 (false) on failure. A
sub-command which is missing or unknown, or a *start* or *size* which is not
a hexadecimal number, shows the usage message. The stash sub-commands fail if
no stash area is defined, if the area is too small for the records, or if
*unstash* finds no stashed data at the address.

See also
--------

* :doc:`log<log>` for the messages U-Boot produces as it runs, which are
  collected and reported separately from timing records
* :doc:`trace<trace>` for the timing of individual functions, rather than of
  the boot as a whole
