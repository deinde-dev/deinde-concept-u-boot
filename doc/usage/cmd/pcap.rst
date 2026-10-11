.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: pcap (command)

pcap command
============

Synopsis
--------

::

    pcap init <addr> <max_size>
    pcap start
    pcap stop
    pcap status
    pcap clear

Description
-----------

The pcap command records the Ethernet packets U-Boot sends and receives into a
buffer in memory, in the PCAP 2.4 file format. The buffer can then be copied
off the board and opened with a tool such as Wireshark, so a network problem on
a board with no other way of watching the wire can be looked at with the usual
tools.

Every packet the Ethernet uclass sends is recorded, and so is every packet the
network stack receives, whether or not it is one U-Boot was waiting for. A
packet the driver drops before handing it up is not seen.

Recording does not start on its own. 'pcap init' sets up the buffer and writes
the 24-byte file header into it, and 'pcap start' begins recording. Each packet
then costs a 16-byte record header plus its own length. When the buffer fills,
a warning is printed and later packets are dropped silently until the capture
is cleared or set up again.

The timestamp in each record comes from timer_get_us(), which counts from when
U-Boot started rather than from the epoch, so a reader shows the capture as
having been taken in January 1970. Only the intervals between packets are
meaningful.

pcap init <addr> <max_size>
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Points the capture at a buffer and writes the file header into it, discarding
anything recorded before. Recording is left stopped, so the buffer can be set
up before the operation of interest begins.

addr
    address of the buffer, hexadecimal

max_size
    size of the buffer in bytes, decimal rather than hexadecimal. It must be at
    least 24 bytes, enough for the file header on its own.

pcap start
~~~~~~~~~~

Starts recording, or continues after a stop.

pcap stop
~~~~~~~~~

Stops recording, leaving the buffer and its contents alone, so that a capture
can be collected around one operation and copied out afterwards.

pcap status
~~~~~~~~~~~

Shows the state of the capture:

Initialized addr, max length
    the buffer given to 'pcap init'

Status
    Active while recording, Idle when stopped

file size
    the number of bytes used so far, which is the length of the PCAP file to
    copy out. It starts at 24, the file header alone.

Incoming packets, Outgoing packets
    how many packets have been recorded in each direction

A network command which finishes while recording is active prints this report
of its own accord, so there is no need to ask for it afterwards.

pcap clear
~~~~~~~~~~

Throws away what has been captured, keeping the same buffer: the file header is
left in place, the packet counts are reset and a buffer which had filled up
accepts packets again.

Each packet recorded also sets the *pcapsize* environment variable to the
number of bytes used, in hexadecimal, which is the length to give a command
copying the buffer out.

Example
-------

This captures a ping on sandbox. The buffer holds the file header and the four
packets of the exchange, and the report at the end of the ping comes from the
network stack rather than from a 'pcap status' of its own::

    => setenv ethact eth@10002000
    => pcap init 1000000 4096
    PCAP capture initialized: addr: 0x1000000 max length: 4096
    => pcap start
    => ping 1.1.2.2
    Using eth@10002000 device
    PCAP status:
    	Initialized addr: 0x1000000	max length: 4096
    	Status: Active.	 file size: 256
    	Incoming packets: 2 Outgoing packets: 2
    host 1.1.2.2 is alive
    => pcap stop
    => printenv pcapsize
    pcapsize=100

The buffer now starts with the PCAP file header: the magic number, version 2.4,
no time-zone offset, a snapshot length of 65535 and link type 1 for Ethernet::

    => md.l 1000000 6
    01000000: a1b2c3d4 00040002 00000000 00000000  ................
    01000010: 0000ffff 00000001                    ........

The whole of it can then be sent to a server with
``tftpput 1000000 $pcapsize 1.1.2.2:ping.pcap``, or written to a filesystem
with 'save'.

A buffer too small for the traffic fills up part-way through, which is reported
once::

    => pcap init 1000000 100
    PCAP capture initialized: addr: 0x1000000 max length: 100
    => pcap start
    => ping 1.1.2.2
    Using eth@10002000 device

    !!! Buffer is full, consider increasing buffer size !!!
    PCAP status:
    	Initialized addr: 0x1000000	max length: 100
    	Status: Active.	 file size: 82
    	Incoming packets: 0 Outgoing packets: 1
    host 1.1.2.2 is alive

Clearing it starts again in the same buffer::

    => pcap clear
    pcap capture cleared
    => pcap status
    PCAP status:
    	Initialized addr: 0x1000000	max length: 100
    	Status: Active.	 file size: 24
    	Incoming packets: 0 Outgoing packets: 0

Every sub-command but 'pcap init' needs a buffer to work on::

    => pcap status
    pcap was not initialized
    => pcap start
    error: pcap was not initialized

Return value
------------

The return value $? is 0 (true) if the sub-command runs. It is 1 (false) if no
sub-command is given or the name matches none of them, if 'pcap init' is given
anything other than two arguments or a size too small for the file header, and
if any other sub-command is used before 'pcap init' has set a buffer up.

Configuration
-------------

The command is only available if CONFIG_CMD_PCAP=y, which needs the legacy
network stack.

See also
--------

* :doc:`tftpput<tftpput>` for copying the finished capture to a server
* :doc:`save<save>` for writing it to a file on a filesystem instead
* :doc:`md<md>` for examining the capture buffer by hand
* :doc:`iotrace<iotrace>` for recording I/O accesses to a buffer in the same
  way
