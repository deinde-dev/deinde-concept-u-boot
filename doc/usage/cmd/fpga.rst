.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: fpga (command)

fpga command
============

Synopsis
--------

::

    fpga info [<dev>]
    fpga dump <dev> <address> <size>
    fpga load <dev> <address> <size>
    fpga loadb <dev> <address> <size>
    fpga loadp <dev> <address> <size>
    fpga loadbp <dev> <address> <size>
    fpga loadfs <dev> <address> <size> <blocksize> <interface> <dev[:part]> <filename>
    fpga loadmk <dev> <address>
    fpga loads <dev> <address> <size> <authflag> <encflag> [<userkey>]

Description
-----------

The fpga command configures an FPGA from a bitstream held in memory or in a
file, and reports what the board knows about the devices it can configure.

The devices come from a table which board code fills in before the command
runs, so only a board with FPGA support of its own has any: a board with none
reports 'No FPGA devices available.' and the command fails. Each device is
identified by its index in that table, counting from zero, and the index is
read in hex.

Only *fpga info*, *fpga dump* and *fpga load* are always present. The rest are
each gated by a config option, so a board may have any subset of them.

Arguments:

dev
    Device to configure, as an index into the board's table of FPGA devices,
    in hex. If the environment variable *fpga* is set it gives the device
    instead and this argument is ignored.

address
    Address of the bitstream in memory, in hex.

size
    Size of the bitstream in bytes, in hex. It is an error for this to be
    zero.

Two environment variables affect the command:

fpga
    Device to use, in hex, in place of the <dev> argument of every
    sub-command.

fpgadata
    Address of the image for *fpga loadmk*, used when the command is given no
    address of its own.

fpga info
~~~~~~~~~

This shows what the driver knows about a device: its family, the interface
used to configure it, its size in bytes, its cookie and its name. With no
device named it reports on every device in the table.

fpga dump
~~~~~~~~~

This reads a device's configuration back into memory. Few drivers implement
it; one which does not reports a missing dump operation and the command
fails.

fpga load
~~~~~~~~~

This configures a device from a full bitstream in memory, in the raw form
which Vivado writes to a '.bin' file.

fpga loadb
~~~~~~~~~~

This configures a device from a '.bit' file, which carries a header giving
the design name, the part number, the date and time of the build and the
length of the bitstream which follows. The command prints those fields as it
parses them, and refuses a bitstream whose part number does not match the
device. Setting the environment variable *fpga_skip_idcheck* to 'y' loads it
anyway.

fpga loadp
~~~~~~~~~~

As *fpga load*, but the bitstream is a partial one, which reconfigures part
of a device that is already running.

fpga loadbp
~~~~~~~~~~~

As *fpga loadb*, but the '.bit' file holds a partial bitstream.

fpga loadfs
~~~~~~~~~~~

This configures a device from a file on a filesystem, reading it in chunks of
<blocksize> bytes, so that a bitstream larger than the memory available can
be loaded. Every argument is required, including the device and partition.

Arguments:

blocksize
    Size of each chunk read from the file, in hex.

interface
    Interface the file is on, such as 'mmc'.

dev[:part]
    Device number on that interface, and the partition to read from.

filename
    File to read the bitstream from.

fpga loadmk
~~~~~~~~~~~

This configures a device from a bitstream wrapped by mkimage, either in the
legacy image format or in a FIT. A legacy image may be gzip-compressed, in
which case it is decompressed to the load address its header gives. For a FIT
the address must name the subimage to use, in the form
``<address>:<subimage>``.

fpga loads
~~~~~~~~~~

This configures a device from a bitstream which is authenticated, encrypted
or both, as the Xilinx bootgen tool produces.

Arguments:

authflag
    Where to authenticate the bitstream: 0 for OCM, 1 for DDR, 2 for no
    authentication.

encflag
    Which key decrypts the bitstream: 0 for the device key, 1 for a user key,
    2 for no encryption.

userkey
    Address of the user key, needed when <encflag> is 1.

A bitstream which is neither authenticated nor encrypted is a job for
*fpga load*, and the command refuses it.

Example
-------

This is a Zynq-7000 board carrying a 7z020 part, with the bitstream already
read into memory at 0x100000. There is one FPGA device, so it is device 0::

    => fpga info
    Family:         Zynq PL
    Interface type: Device configuration interface (Zynq)
    Device Size:    4045564 bytes
    Cookie:         0x0 (0)
    Device name:    7z020
    No Device Function Table.

Load a raw bitstream, as written to a '.bin' file::

    => fpga load 0 0x100000 0x3dbafc
    INFO:post config was not run, please run manually if needed

The same design as a '.bit' file is 126 bytes longer, since it carries a
header, and the command shows what that header says::

    => fpga loadb 0 0x100000 0x3dbb7a
      design filename = "system.bit;UserID=0XFFFFFFFF;Version=2019.2"
      part number = "7z020clg400"
      date = "2019/12/04"
      time = "12:03:19"
      bytes in bitstream = 4045564
    INFO:post config was not run, please run manually if needed

The Zynq driver cannot read a configuration back, so a dump fails::

    => fpga dump 0 0x100000 0x3dbafc
    xilinx_dump: Missing dump operation
    Command 'dump' failed: Error 1

Return value
------------

The return value $? is 0 (true) if the command succeeds. It is 1 (false) if
the sub-command is not one of those listed, if the number of arguments is
wrong, if the address or the size is zero, if <dev> does not name a device in
the board's table, or if the driver reports a failure configuring the device
or reading it back.

See also
--------

* :doc:`fatload<fatload>` for reading a bitstream into memory from a FAT
  filesystem before loading it
* :doc:`load<load>` for doing that from any filesystem U-Boot can read
* :doc:`iminfo<iminfo>` for checking the mkimage wrapper which
  'fpga loadmk' takes
