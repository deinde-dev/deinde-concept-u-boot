.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: tftpboot (command)

tftpboot command
================

Synopsis
--------

::

    tftpboot [loadAddress] [[hostIPaddr:]bootfilename] [-ipv6]

Description
-----------

The tftpboot command downloads a file from a TFTP server into memory. It is
the plain download command of the TFTP family: unlike *bootp* and
:doc:`dhcp<dhcp>` it asks for no network settings, so the address of the board
and of the server must already be known.

loadAddress
    memory address the file is loaded to. If not given, the address comes from
    the *loadaddr* environment variable

hostIPaddr
    address of the TFTP server, in place of the *serverip* environment
    variable. It applies to this transfer alone and *serverip* is left as it
    is

bootfilename
    path of the file to download, in place of the *bootfile* environment
    variable. With neither, the address of the board is used to make up a
    name, so 192.0.2.1 asks for C0000201.img, and the made-up name is
    reported

-ipv6
    use IPv6, taking the server from *serverip6*. This parameter must come
    last

A single argument is taken as the load address where it reads as a hexadecimal
number and as a file name where it does not, so a file whose name looks like a
number needs the address given as well.

The file is requested a block at a time, 1468 bytes by default, and each block
is acknowledged before the next is sent. A '#' is shown for each block which
arrives, and the transfer rate is reported at the end where the transfer takes
a millisecond or more. A request which goes unanswered for five seconds is
sent again, showing a 'T', up to twice CONFIG_NET_RETRY_COUNT times — ten with
the default of five. The command then reports 'Retry count exceeded' and
fails, unless the *netretry* environment variable asks for the whole transfer
to be tried again.

Once the file is loaded, *filesize* and *fileaddr* are set to describe it, so
that a command which uses it needs no numbers typing in. The command then
boots the file, as the :doc:`bootm<bootm>` command would, if the *autostart*
environment variable is set to 'yes'.

Example
-------

This downloads a file from the server named by *serverip*::

    => setenv serverip 192.0.2.2
    => tftpboot 1000000 image.bin
    Using eth@10002000 device
    TFTP from server 192.0.2.2; our IP address is 192.0.2.1
    Filename 'image.bin'.
    Load address: 0x1000000
    Loading: #
             25.4 KiB/s
    done
    Bytes transferred = 26 (1a hex)
    => printenv filesize fileaddr
    filesize=1a
    fileaddr=1000000

With no name on the command line and no *bootfile* variable, the command makes
a name up from the address of the board::

    => tftpboot 1000000
    *** Warning: no boot file name; using 'C0000201.img'
    Using eth@10002000 device
    TFTP from server 192.0.2.2; our IP address is 192.0.2.1
    Filename 'C0000201.img'.
    Load address: 0x1000000
    Loading: #
    done
    Bytes transferred = 26 (1a hex)

A file the server does not have ends the transfer with the error the server
reports::

    => tftpboot 1000000 missing.bin
    Using eth@10002000 device
    TFTP from server 192.0.2.2; our IP address is 192.0.2.1
    Filename 'missing.bin'.
    Load address: 0x1000000
    Loading: *
    TFTP error: 'File not found' (1)
    Not retrying...

Return value
------------

The return value $? is set to 0 (true) if the file was downloaded, and to 1
(false) if the server refused the request, if no server answered or if the
arguments cannot be parsed. Where *autostart* is set to 'yes' the value is
that of the boot, which does not return if it succeeds.

A command line with more arguments than the command takes, or with an option
other than *-ipv6*, shows the usage message instead.

Configuration
-------------

The command is only available if CONFIG_CMD_TFTPBOOT=y.

CONFIG_TFTP_BLOCKSIZE sets the block size asked for, which defaults to 1468 to
match an Ethernet MTU of 1500. The server may offer a smaller one, and a
server which negotiates no options at all sends the 512 bytes of the original
protocol.

CONFIG_TFTP_WINDOWSIZE sets how many blocks the server may send before it
waits for an ACK, which defaults to 1.

If CONFIG_TFTP_TSIZE=y the command asks the server for the size of the file
and limits the progress bar to 50 '#' characters, which keeps it on one line.

With CONFIG_NET_TFTP_VARS=y the *tftpblocksize*, *tftpwindowsize*,
*tftptimeout* and *tftptimeoutcountmax* environment variables override the
options above for a single board. The ports are controlled by *tftpsrcp* and
*tftpdstp*, which default to a pseudo-random source port and the well-known
destination port 69.

The *-ipv6* parameter needs CONFIG_IPV6=y. An IPv6 address may also be given
in place of *hostIPaddr*, framed with [] brackets, which selects IPv6 without
the parameter.

The lwIP network stack (CONFIG_NET_LWIP=y) provides the command as well. It
takes the server from *tftpserverip* where that is set, in preference to
*serverip*, and accepts a port as hostIPaddr:port:bootfilename. It has no
window size, no IPv6 parameter and no made-up file name: a transfer with no
file name, or none with a load address, is an error.

See also
--------

* :doc:`bootp<bootp>` for obtaining the network settings before a download
* :doc:`dhcp<dhcp>` for obtaining the settings and the file in one command
* :doc:`tftpput<tftpput>` for sending a file to a server
* :doc:`tftpsrv<tftpsrv>` for receiving a file from a host
* :doc:`wget<wget>` for downloading a file over HTTP
* :doc:`pxe<pxe>` for downloading a PXELINUX configuration file over TFTP and
  booting what it names
* :doc:`net<net>` for listing the Ethernet devices a download may go over
* :doc:`bootm<bootm>` for booting the image once it is in memory
