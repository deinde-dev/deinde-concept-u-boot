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
    time for each stage.

bootstage save
    Save the current record count to the *bootstage_count* environment
    variable. This can be used to snapshot the bootstage state before an
    operation that adds records.

bootstage restore
    Restore the record count from *bootstage_count*, discarding any records
    added since the last save. This is used by the Python test framework to
    prevent records from accumulating across tests.

bootstage stash
    Stash bootstage data into memory at the given address and size.
    Only available when ``CONFIG_BOOTSTAGE_STASH`` is enabled.

bootstage unstash
    Read back previously stashed bootstage data from memory.
    Only available when ``CONFIG_BOOTSTAGE_STASH`` is enabled.

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

::

    => bootstage report
    Timer summary in microseconds (8 records):
           Mark    Elapsed  Stage
              0          0  reset
              0          0  board_init_f
         29,743     29,743  board_init_r
         52,918     23,175  eth_common_init
         53,007         89  eth_initialize

    Accumulated time:
                     1,235  dm_f
                       670  of_live
                     5,621  dm_r

    => bootstage save
    => bootstage restore

See also
--------

* :doc:`log<log>` for the messages U-Boot produces as it runs, which are
  collected and reported separately from timing records
* :doc:`trace<trace>` for the timing of individual functions, rather than of
  the boot as a whole
