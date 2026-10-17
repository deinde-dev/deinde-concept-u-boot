.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: tftpput (command)

tftpput command
===============

Synopsis
--------

::

    tftpput address size [[hostIPaddr:]filename]

Description
-----------

The tftpput command is used to transfer a file to a TFTP server.

By default the destination port is 69 and the source port is pseudo-random.
The environment variable *tftpsrcp* can be used to set the source port and the
environment variable *tftpdstp* can be used to set the destination port.

address
    memory address where the data starts

size
    number of bytes to be transferred

hostIPaddr
    IP address of the TFTP server, defaults to the value of environment
    variable *serverip*

filename
    path of the file to be written, defaults to the value of environment
    variable *bootfile*. With neither, the client's IP address is used to
    construct a default file name, e.g. C0A80028.img for IP address
    192.168.0.40, and the name is reported as it is made up.

Example
-------

In the example the following steps are executed:

* setup client network address
* load a file from the SD-card
* send the file via TFTP to a server

::

    => env set autoload no
    => dhcp
    BOOTP broadcast 1
    DHCP client bound to address 192.168.1.40 (7 ms)
    => load mmc 0:1 $loadaddr test.txt
    260096 bytes read in 13 ms (19.1 MiB/s)
    => tftpput $loadaddr $filesize 192.168.1.3:upload/test.txt
    Using ethernet@1c30000 device
    TFTP to server 192.168.1.3; our IP address is 192.168.1.40
    Filename 'upload/test.txt'.
    Save address: 0x42000000
    Save size:    0x3f800
    Saving: #################
         4.4 MiB/s
    done
    Bytes transferred = 260096 (3f800 hex)
    =>

Configuration
-------------

The command is only available if CONFIG_CMD_TFTPPUT=y.

CONFIG_TFTP_BLOCKSIZE defines the size of the TFTP blocks sent. It defaults
to 1468 matching an ethernet MTU of 1500.

CONFIG_TFTP_WINDOWSIZE can be used to set the TFTP window size of transmits
after which an ACK response is required. The window size defaults to 1. It has
no effect on an upload, since the option is offered to the server only when
reading a file.

If CONFIG_TFTP_TSIZE=y, the progress bar is limited to 50 '#' characters.
Otherwise an '#' is written per UDP package which may decrease performance.

Return value
------------

The return value $? is set to 0 (true) if the whole file reached the server
and it acknowledged the last block.

If an error occurs, the return value $? is set to 1 (false). A server which
does not answer, one which refuses the transfer, and Ctrl-C during the
transfer are all errors.

A command line without both an address and a size, or with an address or size
which is not a hexadecimal number, shows the usage message instead; the latter
reports 'Invalid address/size' first.

See also
--------

* :doc:`bootp<bootp>` for obtaining the network settings before an upload
* :doc:`tftpsrv<tftpsrv>` for receiving a file from a host
* :doc:`wget<wget>` for downloading a file over HTTP
* :doc:`pcap<pcap>` for producing a packet capture in memory to upload
* *tftpboot* for downloading a file over TFTP
