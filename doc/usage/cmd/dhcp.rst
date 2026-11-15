.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: dhcp (command)

dhcp command
============

Synopsis
--------

::

    dhcp [loadAddress] [[hostIPaddr:]bootfilename]

Description
-----------

The dhcp command obtains a lease from a DHCP server, which provides the
network settings of the board, and then, unless this is turned off, downloads
the boot file the server names over TFTP.

loadAddress
    memory address the file is loaded to. If not given, the address comes from
    the *loadaddr* environment variable

hostIPaddr
    address of the TFTP server to download from, in place of the *serverip*
    environment variable

bootfilename
    name of the file to download, in place of the name the server offers

The command broadcasts a DHCPDISCOVER message on the current Ethernet device,
takes the address out of the DHCPOFFER which comes back, asks for it with a
DHCPREQUEST and treats the lease as held once the DHCPACK arrives. The
discover is retried, with a growing interval, until a server answers or the
time limit is reached. That limit is three seconds plus five for each retry
CONFIG_NET_RETRY_COUNT allows, so 28 seconds with the default of five, and the
*bootpretryperiod* environment variable overrides it.

The reply provides the address of the board and may carry the subnet mask, the
gateway, the name servers, the host name, the NIS domain and the root path as
well; each of those which arrives is written to the matching environment
variable (*ipaddr*, *netmask*, *gatewayip*, *dnsip*, *hostname*, *domain* and
*rootpath*). The lease time is noted but not acted on: the command obtains a
lease and never renews it.

The file is then downloaded over TFTP, as the :doc:`tftpboot<tftpboot>`
command would do, and *filesize* and *fileaddr* are set to describe it.
Setting the *autoload* environment variable to 'no' stops this, so that the
command obtains the network settings alone. Where it is set to 'NFS' the file
is fetched over NFS instead.

Once the file is loaded the command boots it, as the *bootm* command would,
if the *autostart* environment variable is set to 'yes'.

Example
-------

This obtains a lease and downloads the file the server offers::

    => dhcp 1000000
    BOOTP broadcast 1
    DHCP client bound to address 192.0.2.10 (1 ms)
    Using eth@10002000 device
    TFTP from server 192.0.2.2; our IP address is 192.0.2.10
    Filename 'boot.img'.
    Load address: 0x1000000
    Loading: #
             21.5 KiB/s
    done
    Bytes transferred = 22 (16 hex)

With autoload turned off only the settings are obtained::

    => setenv autoload no
    => dhcp
    BOOTP broadcast 1
    DHCP client bound to address 192.0.2.10 (0 ms)

Without a server to answer, the discover is retried until the command gives
up::

    => dhcp
    BOOTP broadcast 1
    BOOTP broadcast 2
    BOOTP broadcast 3
    BOOTP broadcast 4
    BOOTP broadcast 5
    BOOTP broadcast 6
    BOOTP broadcast 7

    Retry time exceeded; starting again

Return value
------------

The return value $? is set to 0 (true) if a lease is obtained, and to 1
(false) if no server answers, if the download fails or if the arguments cannot
be parsed.

Configuration
-------------

The command is only available if CONFIG_CMD_DHCP=y.

With the legacy network stack (CONFIG_NET_LEGACY=y) the discover is sent by
the same code as the *bootp* command uses, which is why the broadcast is
reported as a BOOTP one above.

With CONFIG_BOOTP_SERVERIP=y the TFTP server is taken from the *serverip*
environment variable rather than from the reply. The server address in the
header of the reply is then ignored, along with the boot file name there,
although a name in the DHCP bootfile option is still used.

The lwIP network stack (CONFIG_NET_LWIP=y) provides the command as well, with
a fixed ten-second limit of its own and no *autoload* handling: a file is
downloaded only when the command is given one to download. It also sets
*serverip* and *bootfile* from the lease.

See also
--------

* :doc:`bootp<bootp>` for obtaining the settings from a BOOTP server instead
* :doc:`rarpboot<rarpboot>` for obtaining the address of the board over RARP
* :doc:`linklocal<linklocal>` for setting up the network with no server at all
* :doc:`cdp<cdp>` for obtaining the VLAN settings of the port before asking for
  an address
* :doc:`dns<dns>` for looking up a host once the lease is held
* :doc:`ping<ping>` for checking that a host answers
* :doc:`sntp<sntp>` for setting the clock from the time server the lease names
* :doc:`tftpboot<tftpboot>` for downloading a file over TFTP by hand
* :doc:`tftpput<tftpput>` for sending a file to a server
* :doc:`wget<wget>` for downloading a file over HTTP
* :doc:`pxe<pxe>` for downloading a PXELINUX configuration file over TFTP and
  booting what it names
