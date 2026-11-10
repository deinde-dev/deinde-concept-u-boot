.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: bcb (command)

bcb command
===========

Synopsis
--------

::

    bcb load <interface> <dev> <part>
    bcb load <dev> <part>
    bcb set <field> <val>
    bcb clear [<field>]
    bcb test <field> <op> <val>
    bcb dump <field>
    bcb store
    bcb ab_select <slot_var> <interface> <dev[:part|#part_name]> [--no-dec]
    bcb ab_dump <interface> <dev[:part|#part_name]>

Description
-----------

The bcb command reads, changes and writes back the Android Bootloader Control
Block, the 2KB structure which Android keeps at the start of a raw partition,
usually called 'misc', to pass messages between the Android user space and the
bootloader. :doc:`../../android/bcb` describes what the block is for and how a
boot script uses it.

The command holds one copy of the block in memory. 'bcb load' reads it from a
partition, 'set', 'clear', 'test' and 'dump' work on that copy, and 'store'
writes it back, so a change is only on the device once 'store' has run. Nothing
but 'load' needs a device, and every other sub-command fails until a load has
succeeded.

The block has five fields, each a string padded with NUL bytes:

command
    what to do on the next boot, such as 'boot-recovery' or
    'bootonce-bootloader' (32 bytes)

status
    what the last attempt did, which recovery writes (32 bytes)

recovery
    the command line to pass to recovery, one argument per line (768 bytes)

stage
    progress through a multi-stage package, such as '1/2' (32 bytes)

reserved
    the rest of the block, which Android does not define (1184 bytes)

bcb load
~~~~~~~~

This reads the block from a partition. The interface may be left out, in which
case 'mmc' is used, and the partition may be named either by its index or by
its label. The whole block is read, so the partition must be at least 2KB.

bcb set
~~~~~~~

This writes a string into a field. Any ':' in the value becomes a line feed,
which is how a list of recovery arguments is passed in one word. A value which
does not fit in the field is refused.

bcb clear
~~~~~~~~~

This fills a field with NUL bytes, or the whole block where no field is named.

bcb test
~~~~~~~~

This compares a field with a string and reports the result as the return value,
for use in an 'if' statement. The operator '=' is true when the field holds
exactly that string, and '~' is true when the string appears anywhere in the
field.

bcb dump
~~~~~~~~

This shows a field as hex and ASCII. The offsets shown are those of the field
within the block, not within the partition.

bcb store
~~~~~~~~~

This writes the block back to the partition it came from.

bcb ab_select
~~~~~~~~~~~~~

This chooses which of the A/B slots to boot, registers the attempt in the A/B
metadata which shares the same partition, and puts the name of the slot - 'a'
or 'b' - in the named environment variable. Where the metadata is missing or
its CRC is wrong, fresh metadata is written first. With --no-dec the number of
tries left for the chosen slot is not reduced, so the choice can be made
without spending an attempt. The partition may be named as '<dev>:<part>' by
index or as '<dev>#<part_name>' by label. See :doc:`../../android/ab` for how
the slots are used.

This sub-command needs CONFIG_ANDROID_AB=y.

bcb ab_dump
~~~~~~~~~~~

This shows the A/B metadata: the active slot, the magic number and version,
the number of slots, how many tries recovery has left, the CRC and its
validity, then the priority, tries left, successful-boot flag and
verity-corrupted flag of each slot. It needs CONFIG_ANDROID_AB=y as well.

Example
-------

This uses sandbox, where the first MMC device is emulated and has no partition
table to start with, so one is written with a 'misc' partition in it::

    => gpt write mmc 0 "name=misc,start=0x8000,size=0x10000,uuid=4ab5f3c8-0000-4000-8000-000000000001"
    Writing GPT: success!
    => bcb load mmc 0 misc
    => bcb set command boot-recovery
    => bcb dump command
    00000000: 62 6f 6f 74 2d 72 65 63 6f 76 65 72 79 00 00 00  boot-recovery...
    00000010: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
    => bcb store
    => bcb test command = boot-recovery
    => echo $?
    0
    => bcb test command = bootonce-bootloader
    => echo $?
    1
    => bcb clear
    => bcb dump command
    00000000: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
    00000010: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................

The A/B sub-commands work on the same partition. The first use of a partition
whose A/B metadata is missing reports an invalid CRC and writes fresh metadata;
this shows the call after that one, by which time each slot has had an attempt
registered against it::

    => bcb ab_select slot_name mmc 0#misc
          ab_select_slot() ANDROID: Attempting slot b, tries remaining 7
    ANDROID: Booting slot: b
    => printenv slot_name
    slot_name=b
    => bcb ab_dump mmc 0#misc
    Bootloader Control:       [misc]
    Active Slot:              _b
    Magic Number:             0x42414342
    Version:                  1
    Number of Slots:          2
    Recovery Tries Remaining: 0
    CRC:                      0x011ec016 (Valid)

    Slot[0] Metadata:
            - Priority:         15
            - Tries Remaining:  6
            - Successful Boot:  0
            - Verity Corrupted: 0

    Slot[1] Metadata:
            - Priority:         15
            - Tries Remaining:  6
            - Successful Boot:  0
            - Verity Corrupted: 0

The log line from 'ab_select' carries the name of the function which emits it
because sandbox enables CONFIG_LOGF_FUNC; a board without it shows the message
on its own.

Return value
------------

The return value $? is 0 (true) if the sub-command does what it is asked. For
'bcb test' it is 0 when the comparison holds and 1 when it does not, which is
the point of that sub-command.

It is 1 (false) if no BCB is loaded, if the device or partition cannot be read,
if the field named does not exist, if a value is too long for its field, if the
operator is not '=' or '~', or if the block cannot be written back. The
command shows its usage message where a sub-command is missing or has the wrong
number of arguments.

See also
--------

* :doc:`../../android/bcb` for what the block is for and a script which acts on
  it
* :doc:`../../android/ab` for the A/B slot metadata which 'ab_select' and
  'ab_dump' work on
* :doc:`abootimg<abootimg>` for reading the boot image which the chosen slot
  names
* :doc:`avb<avb>` for verifying the images the control block asks for
* :doc:`gpt<gpt>` for creating the 'misc' partition the block lives in
* :doc:`part<part>` for listing the partitions of a device
