.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: avb (command)

avb command
===========

Synopsis
--------

::

    avb init <dev>
    avb verify [<slot_suffix>]
    avb is_unlocked
    avb read_rb <num>
    avb write_rb <num> <rb>
    avb get_uuid <partname>
    avb read_part <partname> <offset> <num> <addr>
    avb read_part_hex <partname> <offset> <num>
    avb write_part <partname> <offset> <num> <addr>
    avb read_pvalue <name> <bytes>
    avb write_pvalue <name> <value>

Description
-----------

The avb command runs Android Verified Boot 2.0, which checks the boot image
and the dm-verity metadata of an Android installation against the signed
vbmeta structure before the kernel is started.
:doc:`../../android/avb2` describes what the chain of trust covers and how a
boot script uses the command.

Every sub-command works through the libavb operations in
``common/avb_verify.c``, which read and write partitions of one MMC device.
That device is chosen by 'avb init' and remembered until the next 'avb init',
so each of the other sub-commands fails until an init has run.

The rollback indexes, the device lock state and the persistent values live in
replay-protected memory which OP-TEE manages, so the sub-commands which use
them need CONFIG_OPTEE_TA_AVB. Without it the device always reports itself
unlocked, a rollback index always reads as 0 and writing one does nothing, as
:doc:`../../android/avb2` explains.

avb init
~~~~~~~~

This creates the libavb operations for an MMC device, given by its device
number, and drops any created by an earlier init. It does not touch the
device, so it succeeds even where no such device exists; the sub-command which
uses it reports the failure.

avb verify
~~~~~~~~~~

This runs the verification chain on the 'boot' partition, using the hash
descriptors in the vbmeta partition. A slot suffix such as '_a' or '_b' may be
given where the partitions are slotted, in which case it is appended to each
partition name.

On success the kernel command line which libavb produces is stored in the
environment variable *avb_bootargs*, along with the boot state the device has
reached: 'green' where the device is locked and verification passed,
'orange' where it is unlocked, so the user is told that the device may have
been modified.

avb is_unlocked
~~~~~~~~~~~~~~~

This reports whether the device is unlocked, as 1, or locked, as 0. An
unlocked device may be freely modified, so verification failures do not stop
it booting.

avb read_rb and avb write_rb
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These read and write a rollback index, which records the lowest image version
the device accepts, so that an older signed image cannot be flashed back on to
it. Both take the location of the index as a hex number. An index may only be
raised, and a location which the secure storage does not have is refused.

avb get_uuid
~~~~~~~~~~~~

This shows the partition UUID of a named partition, which a kernel command
line uses to name its root device.

avb read_part, avb read_part_hex and avb write_part
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These copy bytes between a partition and memory, through the same operations
libavb itself uses, which is what makes them useful for checking a
verification problem by hand. The offset into the partition and the number of
bytes are hex numbers and need not be a multiple of the block size.
'read_part' and 'write_part' take the memory address to use, while
'read_part_hex' prints the bytes it has read as hex digits.

avb read_pvalue and avb write_pvalue
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These read and write a named persistent value in secure storage, which libavb
uses for such things as the persistent digest of a partition. 'write_pvalue'
stores a string, including its terminator, and 'read_pvalue' reads back the
number of bytes given. Both need CONFIG_OPTEE_TA_AVB.

Example
-------

This shows the sub-commands which work on the state of the device, on sandbox,
where OP-TEE is emulated::

    => avb init 0
    => avb is_unlocked
    Unlocked = 1
    => avb read_rb 1
    Rollback index: 0
    => avb write_rb 1 5
    => avb read_rb 1
    Rollback index: 5
    => avb write_pvalue bootstate green
    Wrote 6 bytes
    => avb read_pvalue bootstate 6
    Read 6 bytes, value = green

The sub-commands which work on partitions need a partition table on the
device. Sandbox has a 1MB card as mmc 0 with nothing on it, so this writes one
partition to it first::

    => gpt write mmc 0 name=boot,start=0x8000,size=0x10000,uuid=4ab5f3c8-0000-4000-8000-000000000001
    Writing GPT: success!
    => avb init 0
    => avb get_uuid boot
    'boot' UUID: 4ab5f3c8-0000-4000-8000-000000000001
    => mw 1000000 deadbeef 2
    => avb write_part boot 0 8 1000000
    Wrote 8 bytes
    => avb read_part boot 0 8 2000000
    Read 8 bytes
    => avb read_part_hex boot 0 8
    Requested 8, read 8 bytes
    Data: EFBEADDEEFBEADDE

Verification itself needs a signed vbmeta partition, which sandbox does not
have. On a board holding an Android installation on mmc 1 it looks like this::

    => avb init 1
    => avb verify
    ## Android Verified Boot 2.0 version 1.3.0
    Verification passed successfully

Return value
------------

The return value $? is 0 (true) if the sub-command does what it is asked.

It is 1 (false) if no 'avb init' has run, if the device or the partition
cannot be read, if the rollback index location or the persistent value does
not exist, or if verification fails, in which case the reason libavb gives is
shown. The command shows its usage message where the sub-command is missing,
is not one it has, or has the wrong number of arguments.

See also
--------

* :doc:`../../android/avb2` for the chain of trust, the root key and the
  options a board needs
* :doc:`bcb<bcb>` for the control block which asks for a recovery boot
* :doc:`abootimg<abootimg>` for reading the boot image which verification
  covers
* :doc:`gpt<gpt>` for writing the partition table the sub-commands read
* :doc:`part<part>` for listing the partitions of a device
