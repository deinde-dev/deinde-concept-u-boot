.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: pstore (command)

pstore command
==============

Synopsis
--------

::

    pstore set <addr> <len> [record-size] [console-size] [ftrace-size] [pmsg-size] [ecc-size]
    pstore display [record-type] [nb]
    pstore save <interface> <dev[:part]> <directory-path>

Description
-----------

Linux PStore and Ramoops modules (Linux config options PSTORE and PSTORE_RAM)
allow to use memory to pass data from the dying breath of a crashing kernel to
its successor. This command allows to read those records from U-Boot command
line.

Ramoops is an oops/panic logger that writes its logs to RAM before the system
crashes. It works by logging oopses and panics in a circular buffer. Ramoops
needs a system with persistent RAM so that the content of that area can survive
after a restart.

Ramoops uses a predefined memory area to store the dump.

Ramoops parameters can be passed as kernel parameters or through Device Tree,
i.e.::

    ramoops.mem_address=0x30000000 ramoops.mem_size=0x100000 ramoops.record_size=0x2000 ramoops.console_size=0x2000 memmap=0x100000$0x30000000

The same values should be set in U-Boot to be able to retrieve the records.
This values can be set at build time in U-Boot configuration file, or at runtime.
U-Boot automatically patches the Device Tree to pass the Ramoops parameters to
the kernel.

The PStore configuration parameters are:

======================= ==========
 Name                   Default
======================= ==========
CMD_PSTORE_MEM_ADDR
CMD_PSTORE_MEM_SIZE     0x10000
CMD_PSTORE_RECORD_SIZE  0x1000
CMD_PSTORE_CONSOLE_SIZE 0x1000
CMD_PSTORE_FTRACE_SIZE  0x1000
CMD_PSTORE_PMSG_SIZE    0x1000
CMD_PSTORE_ECC_SIZE     0
======================= ==========

Records sizes should be a power of 2.
The memory size and the record/console size must be non-zero.

Multiple 'dump' records can be stored in the memory reserved for PStore.
The memory size has to be larger than the sum of the record sizes, i.e.::

    MEM_SIZE >= RECORD_SIZE * n + CONSOLE_SIZE + FTRACE_SIZE + PMSG_SIZE

The command lays the zones out as Linux lays them out, so that it finds the
records where the kernel left them. Each size is rounded down to a power of
two, the memory left over goes to as many dump zones as there is room for
records, and the console, user space and FTrace zones follow them in that
order. Linux 6.1 and older put the FTrace zone before the user space one,
which the command recognises from the signature the kernel writes there.

The 'pstore set' command sets the memory holding the records, and the size of
each zone. Only the address and the length are needed; a size which is left
out keeps the value it has, which is the one the board was configured with.

    addr
        address of the memory reserved for PStore
    len
        size of that memory
    record-size
        size of one panic or oops record, of the 'dump' type
    console-size
        size of the kernel log record
    ftrace-size
        size of the FTrace record
    pmsg-size
        size of the user space log record
    ecc-size
        0 to disable ECC, else the size of the ECC buffer in bytes, where 1
        means 16 bytes

The 'pstore display' command displays the records. With no 'record-type' every
record is shown, else only the records of that kind.

    record-type
        one of 'dump', 'console', 'ftrace' or 'user'
    nb
        number of the dump record to display, where the first is 0. It is
        ignored for the other record types, each of which has a single zone

A compressed dump record is reported rather than displayed, since the command
cannot uncompress one.

The 'pstore save' command writes each record it finds to its own file, under a
directory which should already exist. The filenames are the ones Linux uses in
/sys/fs/pstore, so the dump records become dmesg-ramoops-0 upwards, the
console record console-ramoops-0, the FTrace record ftrace-ramoops-0 and the
user space record pmsg-ramoops-0. A compressed dump record is saved with
'.enc.z' on the end of its name.

    interface
        interface of the device to write to, such as mmc
    dev[:part]
        device number, and the partition to use
    directory-path
        directory to write the files to

Example
-------

Generate a kernel crash
~~~~~~~~~~~~~~~~~~~~~~~

For test purpose, you can generate a kernel crash by setting reboot timeout to
10 seconds and trigger a panic

.. code-block:: console

    $ sudo sh -c "echo 1 > /proc/sys/kernel/sysrq"
    $ sudo sh -c "echo 10 > /proc/sys/kernel/panic"
    $ sudo sh -c "echo c > /proc/sysrq-trigger"

Retrieve the records in U-Boot
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Read the records which the crashing kernel left at 0x3000000, with the zone
sizes the board was configured with:

.. code-block:: console

    => pstore set 0x3000000 0x100000
    => pstore display dump 0
    **** Dump
    Oops#1 Part1
    ...
    <4>[  177.629101] PC is at sysrq_handle_crash+0x30/0x38
    ...
    <4>[  177.935761] ---[ end trace a8b83aca7b90dce0 ]---

Write all of them to the root directory of the first partition of the second
MMC device, which holds two dump records and a console record:

.. code-block:: console

    => pstore save mmc 1:1 /
    3798 bytes written in 0 ms
    4035 bytes written in 0 ms
    4084 bytes written in 0 ms

Return value
------------

The return value $? is 0 (true) on success, 1 (false) on failure.

The usage message is shown, and the command fails, when no sub-command is
given, when the sub-command is not one of the three, when 'pstore set' is
given neither an address nor a length, when 'pstore display' is given a
'record-type' which does not exist, and when either of the other two
sub-commands runs with no configuration set.

A 'len' too small for the zones is reported by 'pstore set', which then holds
no configuration, but the sub-command itself succeeds.

Both 'pstore display' and 'pstore save' fail where the buffer for a record
cannot be allocated. 'pstore save' succeeds even where a file cannot be
written, since it does not check the result of writing one.

See also
--------

* :doc:`save<save>` for writing a region of memory to a file, which is how
  each record is written
* :doc:`md<md>` for looking at the PStore memory by hand
* :doc:`fdt<fdt>` for inspecting the reserved-memory node which this command
  adds to the devicetree it passes to Linux
